/**************************************************************************//**
 * @file
 * @brief EFM32 Bootloader. Preinstalled on all new EFM32 devices
 * @author Energy Micro AS
 * @version 1.64
 ******************************************************************************
 * @section License
 * <b>(C) Copyright 2009 Energy Micro AS, http://www.energymicro.com</b>
 ******************************************************************************
 *
 * This source code is the property of Energy Micro AS. The source and compiled
 * code may only be used on Energy Micro "EFM32" microcontrollers.
 *
 * This copyright notice may not be removed from the source code nor changed.
 *
 * DISCLAIMER OF WARRANTY/LIMITATION OF REMEDIES: Energy Micro AS has no
 * obligation to support this Software. Energy Micro AS is providing the
 * Software "AS IS", with no express or implied warranties of any kind,
 * including, but not limited to, any implied warranties of merchantability
 * or fitness for any particular purpose or warranties against infringement
 * of any proprietary rights of a third party.
 *
 * Energy Micro AS will not be liable for any consequential, incidental, or
 * special damages, or any other relief, or for any claim by any third party,
 * arising from your use of this Software.
 *
 *****************************************************************************/
#include <stdbool.h>
#include "em_device.h"
#include "usart.h"
#include "xmodem.h"
#include "boot.h"
#include "debuglock.h"
#include "autobaud.h"
#include "crc.h"
#include "config.h"
#include "flash.h"
#include "em_cmu.h"
#include "em_emu.h"
#include "em_rmu.h"
#include "em_msc.h"
#include "em_chip.h"
#include "decode.h"
#include "encode.h"
#include "board.h"

#ifndef NDEBUG
#include "debug.h"
#include <stdio.h>
#endif

/** Version string, used when the user connects */
#define BOOTLOADER_VERSION_STRING "0.02 "
#define APP_IS_READY_TAG 0xa5a55a5a
/** Define USART baudrate. **/
#define BOOTLOADER_BAUD_RATE 115200

/* Vector table in RAM. We construct a new vector table to conserve space in
 * flash as it is sparsly populated. */
#if defined (__ICCARM__)
#pragma location=0x20000000
__no_init uint32_t vectorTable[47];
#elif defined (__CC_ARM)
uint32_t vectorTable[47] __attribute__((at(0x20000000)));
#elif defined (__GNUC__)
uint32_t vectorTable[47] __attribute__((aligned(512)));
#else
#error Undefined toolkit, need to define alignment
#endif

#pragma location=0x200000fc
__no_init uint32_t bootTagRam;

#pragma location=(BOOTLOADER_SIZE + 0xfc)
__root const uint32_t bootTagHead;

#pragma location=0x7ffc
__root const uint32_t bootTagTail;

/*
 * This variable holds the computed CRC-16 of the bootloader and is used during
 * production testing to ensure the correct programming of the bootloader.
 * This can safely be omitted if you are rolling your own bootloader.
 */
#if defined ( __ICCARM__ )
#pragma location=0x200000bc
__no_init uint16_t bootloaderCRC;
#elif defined (__CC_ARM)
uint16_t bootloaderCRC __attribute__((at(0x200000bc)));
#elif defined (__GNUC__)
uint16_t bootloaderCRC __attribute__((at(0x200000bc)));
#else
#error Undefined toolkit, need to define alignment
#endif

/* If this flag is set the bootloader will be reset when the RTC expires.
 * This is used when autobaud is started. If there has been no synchronization
 * until the RTC expires the entire bootloader is reset.
 *
 * Essentially, this makes the RTC work as a watchdog timer.
 */
bool resetEFM32onRTCTimeout = false;

/* Global variable . */
uint32_t sys_tick = 0;
uint8_t rxByteChannel = 0;
/**************************************************************************//**
 * Strings.
 *****************************************************************************/
uint8_t crcString[]     = "\r\nCRC: ";
uint8_t newLineString[] = "\r\n";
uint8_t readyString[]   = "\r\nReady\r\n";
uint8_t okString[]      = "\r\nOK\r\n";
uint8_t failString[]    = "\r\nFail\r\n";
uint8_t unknownString[] = "\r\n?\r\n";

#if SIZE_TAILOR==0
/**************************************************************************//**
 * @brief RTC IRQ Handler
 *   The RTC is used to keep the power consumption of the bootloader down while
 *   waiting for the pins to settle, or work as a watchdog in the autobaud
 *   sequence.
 *****************************************************************************/
void RTC_IRQHandler(void)
{
  /* Clear interrupt flag */
  RTC->IFC = RTC_IFC_COMP1 | RTC_IFC_COMP0 | RTC_IFC_OF;
  /* Check if EFM should be reset on timeout */
  if (resetEFM32onRTCTimeout)
  {
#ifndef NDEBUG
    printf("Autobaud Timeout. Resetting EFM32.\r\n");
#endif
    /* Write to the Application Interrupt/Reset Command Register to reset
     * the EFM32. See section 9.3.7 in the reference manual. */
    SCB->AIRCR = 0x05FA0004;
  }
}
#endif//#if SIZE_TAILOR==0

/**************************************************************************//**
 * @brief
 *   This function is to check whether application is ready for boot:
 *   1) RAM tag is 0xa5a55a5a, then retain in loader for uploading.
 *   OR:
 *   2) flash head and tail are 0xa5a55a5a, then jump to app.
 *****************************************************************************/
uint8_t IsBootReady(void)
{
  //pro 1
  if ( bootTagRam == APP_IS_READY_TAG ){
    return 0;
  }

  //pro 2
  if ( ( bootTagHead == APP_IS_READY_TAG ) && ( bootTagTail == APP_IS_READY_TAG ) ){
	 return 1;
  }

  return 0;
}

/**************************************************************************//**
 * @brief
 *   This function is an infinite loop. It actively waits for one of the
 *   following conditions to be true:
 *   1) The SWDClk Debug pins is not asserted and a valid application is
 *      loaded into flash.
 *      In this case the application is booted.
 *   OR:
 *   2) The SWD Clk pin is asserted and there is an incoming packet
 *      on the USART RX line
 *      In this case we start sensing to measure the baudrate of incoming packets.
 *
 *   If none of these conditions are met, the EFM32G is put to EM2 sleep for
 *   250 ms.
 *****************************************************************************/
void waitForBootOrUSART(void)
{
  uint32_t SWDpins;
  uint8_t isAppReady;
#ifndef NDEBUG
  uint32_t oldPins = 0xf;
#endif

#if defined SIZE_TAILOR==0
  /* Initialize RTC */
  /* Clear interrupt flags */
  RTC->IFC = RTC_IFC_COMP1 | RTC_IFC_COMP0 | RTC_IFC_OF;
  /* 250 ms wakeup time */
  RTC->COMP0 = (PIN_LOOP_INTERVAL * LFRCO_FREQ) / 1000;
  /* Enable Interrupts on COMP0 */
  RTC->IEN = RTC_IEN_COMP0;
  /* Enable RTC interrupts */
  NVIC_EnableIRQ(RTC_IRQn);
  /* Enable RTC */
  RTC->CTRL = RTC_CTRL_COMP0TOP | RTC_CTRL_DEBUGRUN | RTC_CTRL_EN;
#endif//SIZE_TAILOR==0

  while (1)
  {
    /* The SWDCLK signal is used to determine if the application
     * Should be booted or if the bootloader should be started
     * SWDCLK (F0) has an internal pull-down and should be pulled high
     *             to enter bootloader mode.
     */
    /* Check input pins */
    SWDpins = GPIO->P[5].DIN;
    SWDpins &= 0x01;
	
#ifndef NDEBUG
    //if (oldPins != SWDpins)
    {
      oldPins = SWDpins;
      printf("New pin: 0x%02X \r\n", SWDpins);
    }
#endif

    /* Check if pins are not asserted AND firmware is valid */
	isAppReady = IsBootReady();
    if ((SWDpins != 0x1) && (BOOT_checkFirmwareIsValid()) && isAppReady )
    {
      /* Boot application */
#ifndef NDEBUG
      printf("Booting application \r\n");
#endif
      BOOT_boot();
    }

    /* SWDCLK (F0) is pulled high and SWDIO (F1) is pulled low */
    /* Enter bootloader mode */
    if ( (SWDpins == 0x1) || !(isAppReady) )
    {
#if defined SIZE_TAILOR==0
      /* Increase timeout to 30 seconds */
      RTC->COMP0 = AUTOBAUD_TIMEOUT * LFRCO_FREQ;
      /* If this timeout occurs the EFM32 is rebooted. */
      /* This is done so that the bootloader cannot get stuck in autobaud sequence */
      resetEFM32onRTCTimeout = true;
#endif//SIZE_TAILOR==0
#ifndef NDEBUG
      printf("Starting autobaud sequence\r\n");
#endif
	  bootTagRam = 0;
      return;
    }
    /* Go to EM2 and wait for RTC wakeup. */
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    __WFI();
  }
}

/**************************************************************************//**
 * @brief
 *   Helper function to print flash write verification using CRC
 * @param start
 *
 * @param end
 *
 *****************************************************************************/
__ramfunc uint8_t rxByte(void)
{
  uint32_t timer = 1000000;
  while (!(BOOTLOADER_USART->STATUS & USART_STATUS_RXDATAV) && !(decBuf.pendingBytes) && --timer ) ;
  if (timer > 0)
  {
	if(decBuf.pendingBytes){
	  rxByteChannel = 0x01 << 1;
	  return (dec_rxByte());
	}
	else{
	  rxByteChannel = 0x01 << 2;	
	  return((uint8_t)(BOOTLOADER_USART->RXDATA & 0xFF));
	}
  }
  else
  {
    return 0;
  }
}


/**************************************************************************//**
 * @brief
 *   Helper function to print flash write verification using CRC
 * @param start
 *   The start of the block to calculate CRC of.
 * @param end
 *   The end of the block. This byte is not included in the checksum.
 *****************************************************************************/
__ramfunc void verify(uint32_t start, uint32_t end)
{
  USART_printString(crcString);
  USART_printHex(CRC_calc((void *) start, (void *) end));
  USART_printString(newLineString);
}

/**************************************************************************//**
 * @brief
 *   The main command line loop. Placed in Ram so that it can still run after
 *   a destructive write operation.
 *   NOTE: __ramfunc is a IAR specific instruction to put code into RAM.
 *   This allows the bootloader to survive a destructive upload.
 *****************************************************************************/
__ramfunc void commandlineLoop(void)
{
  uint32_t flashSize;
  uint8_t  c;
  uint8_t *returnString;

  /* Find the size of the flash. DEVINFO->MSIZE is the
   * size in KB so left shift by 10. */
  flashSize = ((DEVINFO->MSIZE & _DEVINFO_MSIZE_FLASH_MASK) >> _DEVINFO_MSIZE_FLASH_SHIFT)
              << 10;

  /* The main command loop */
  while (1)
  {
	
#if 0
    XMODEM_download(BOOTLOADER_SIZE, flashSize, rxByteChannel);	
#else
    /* Retrieve new character */
    c = rxByte();
    /* Echo */
    if (c != 0)
    {
      USART_txByte(c);
    }
    switch (c)
    {
    /* Upload command */
    case 'u':
      USART_printString(readyString);
      XMODEM_download(BOOTLOADER_SIZE, flashSize, rxByteChannel);
      break;
    /* Destructive upload command */
    case 'd':
      USART_printString(readyString);
      XMODEM_download(0, flashSize, rxByteChannel);
      break;
    /* Write to user page */
    case 't':
      USART_printString(readyString);
      XMODEM_download(XMODEM_USER_PAGE_START, XMODEM_USER_PAGE_END, rxByteChannel);
      break;
    /* Write to lock bits */
    case 'p':
      DEBUGLOCK_startDebugInterface();
      USART_printString(readyString);
#ifdef USART_OVERLAPS_WITH_BOOTLOADER
      /* Since the UART overlaps, the bit-banging in DEBUGLOCK_startDebugInterface()
       * Will generate some traffic. To avoid interpreting this as UART communication,
       * we need to flush the LEUART data buffers. */
      BOOTLOADER_USART->CMD = LEUART_CMD_CLEARRX;
#endif
      XMODEM_download(XMODEM_LOCK_PAGE_START, XMODEM_LOCK_PAGE_END, rxByteChannel);
      break;
    /* Boot into new program */
    case 'b':
      BOOT_boot();
      break;
    /* Debug lock */
#if defined SIZE_TAILOR==0
    case 'l':
#ifndef NDEBUG
      /* We check if there is a debug session active in DHCSR. If there is we
       * abort the locking. This is because we wish to make sure that the debug
       * lock functionality works without a debugger attatched. */
      if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0x0)
      {
        printf("\r\n\r\n **** WARNING: DEBUG SESSION ACTIVE. NOT LOCKING!  **** \r\n\r\n");
        USART_printString("Debug active.\r\n");
      }
      else
      {
        printf("Starting debug lock sequence.\r\n");
#endif
      if (DEBUGLOCK_lock())
      {
        returnString = okString;
      }
      else
      {
        returnString = failString;
      }
      USART_printString(returnString);
#ifndef NDEBUG
        printf("Debug lock word: 0x%x \r\n", *((uint32_t *) DEBUG_LOCK_WORD));
      }
#endif
      break;
    /* Verify content by calculating CRC of entire flash */
    case 'v':
      verify(0, flashSize);
      break;
    /* Verify content by calculating CRC of application area */
    case 'c':
      verify(BOOTLOADER_SIZE, flashSize);
      break;
    /* Verify content by calculating CRC of user page.*/
    case 'n':
      verify(XMODEM_USER_PAGE_START, XMODEM_USER_PAGE_END);
      break;
    /* Verify content by calculating CRC of lock page */
    case 'm':
      verify(XMODEM_LOCK_PAGE_START, XMODEM_LOCK_PAGE_END);
      break;
#endif//SIZE_TAILOR==0
    /* Reset command */
    case 'r':
      /* Write to the Application Interrupt/Reset Command Register to reset
       * the EFM32. See section 9.3.7 in the reference manual. */
      SCB->AIRCR = 0x05FA0004;
      break;
    /* Unknown command */
    case 0:
      /* Timeout waiting for RX - avoid printing the unknown string. */
      USART_printString("None.\r\n");
	  //enc_print("None.");
	  //HIJACKPutData("\r\n", &encBuf, 2);
      break;
    default:
      USART_printString(unknownString);
	  break;
    }
#endif
  }
}

/**************************************************************************//**
 * @brief  Create a new vector table in RAM.
 *         We generate it here to conserve space in flash.
 *****************************************************************************/
void generateVectorTable(void)
{
#if SIZE_TAILOR==0
  vectorTable[AUTOBAUD_TIMER_IRQn + 16] = (uint32_t) TIMER_IRQHandler;
  vectorTable[RTC_IRQn + 16]            = (uint32_t) RTC_IRQHandler;
#endif//SIZE_TAILOR==0
#ifdef USART_OVERLAPS_WITH_BOOTLOADER
  vectorTable[GPIO_EVEN_IRQn + 16]      = (uint32_t) GPIO_IRQHandler;
#endif

  vectorTable[TIMER0_IRQn + 16]         = (uint32_t) TIMER0_IRQHandler;
  vectorTable[TIMER1_IRQn + 16]         = (uint32_t) TIMER1_IRQHandler;
  SCB->VTOR                             = (uint32_t) vectorTable;
}

/**************************************************************************//**
 * @brief  Main function
 *****************************************************************************/
int main(void)
{
  uint32_t clkdiv;

  /* Handle potential chip errata */
  /* Uncomment the next line to enable chip erratas for engineering samples */
  CHIP_Init();

  CMU_HFRCOBandSet(cmuHFRCOBand_7MHz);

  /* Generate a new vector table and place it in RAM */
  generateVectorTable();

  /* Calculate CRC16 for the bootloader itself and the Device Information page. */
  /* This is used for production testing and can safely be omitted in */
  /* your own code. */
  bootloaderCRC  = CRC_calc((void *) 0x0, (void *) BOOTLOADER_SIZE);
  bootloaderCRC |= CRC_calc((void *) 0x0FE081B2, (void *) 0x0FE08200) << 16;
  /* End safe to omit. */

  /* Enable clocks for peripherals. */
  CMU->HFPERCLKDIV = CMU_HFPERCLKDIV_HFPERCLKEN;
  CMU->HFPERCLKEN0 = CMU_HFPERCLKEN0_GPIO | BOOTLOADER_USART_CLOCK |
                     AUTOBAUD_TIMER_CLOCK ;

  /* Enable LE and DMA interface */
  CMU->HFCORECLKEN0 = /*CMU_HFCORECLKEN0_LE |*/ CMU_HFCORECLKEN0_DMA;
#if SIZE_TAILOR==0
  /* Enable LFRCO for RTC */
  CMU->OSCENCMD = CMU_OSCENCMD_LFRCOEN;
  /* Setup LFA to use LFRCRO */
  CMU->LFCLKSEL = CMU_LFCLKSEL_LFA_LFRCO | CMU_LFCLKSEL_LFB_HFCORECLKLEDIV2;
  /* Enable RTC */
  CMU->LFACLKEN0 = CMU_LFACLKEN0_RTC;
#endif//#if SIZE_TAILOR==0

  /* Initial hijack part. */
  /* dec part initial. */
  dec_init();
  enc_init();
  NVIC_SetPriority(TIMER0_IRQn, 5);
  NVIC_SetPriority(TIMER1_IRQn, 10);

#ifndef NDEBUG
  DEBUG_init();
  printf("\r\n-- Debug output enabled --\r\n");
  printf("-- Compiled: %s %s --\r\n", __DATE__, __TIME__ ) ;
#endif
  /* Check if the clock division is too small, if it is, we change
   * to an oversampling rate of 4x and calculate a new clkdiv.
   */
  clkdiv = 7000000*16/BOOTLOADER_BAUD_RATE - 256;
  BOOTLOADER_USART->CTRL |= USART_CTRL_OVS_X16;

  /* Setup pins for USART */
  CONFIG_UsartGpioSetup();

  /* Initialize the UART */
  USART_init(clkdiv);

  USART_printString("\r\n\r\n\r\n-- welcome to bootloader --\r\n");
  USART_printString("-- Build on:"__DATE__" "__TIME__" --\r\n\r\n\r\n");

  /* Figure out correct flash page size */
  FLASH_CalcPageSize();

  /* Wait for a boot operation */
  waitForBootOrUSART();

#ifdef BOOTLOADER_LEUART_CLOCK
  /* Enable LEUART */
  CMU->LFBCLKEN0 = BOOTLOADER_LEUART_CLOCK;
#endif

  /* When autobaud has completed, we can be fairly certain that
   * the entry into the bootloader is intentional so we can disable the timeout.
   */
  NVIC_DisableIRQ(RTC_IRQn);

#ifdef BOOTLOADER_LEUART_CLOCK
  clkdiv = ((periodTime24_8 >> 1) - 256);
  GPIO->ROUTE = 0;
#else

#endif
#ifndef NDEBUG
  //printf("BOOTLOADER_USART clkdiv = %d\r\n", clkdiv);
#endif

  /* Print a message to show that we are in bootloader mode */
  USART_printString("\r\n\r\n" BOOTLOADER_VERSION_STRING  "ChipID: ");
  /* Print the chip ID. This is useful for production tracking */
  USART_printHex(DEVINFO->UNIQUEH);
  USART_printHex(DEVINFO->UNIQUEL);
  USART_printString("\r\n");

  /* Print reset cause in order to debug.  **/
  USART_printHex( RMU_ResetCauseGet() );
  USART_printString("\r\n");
  /* Clear reset cause. */
  RMU_ResetCauseClear();

  /* Initialize flash for writing */
  FLASH_init();

  /* Initialize bsp leds. **/
  BSP_LedsInit();
  GPIO_PinModeSet(gpioPortD, 5, gpioModePushPull, 0);

  /* Start executing command line */
  commandlineLoop();
}
