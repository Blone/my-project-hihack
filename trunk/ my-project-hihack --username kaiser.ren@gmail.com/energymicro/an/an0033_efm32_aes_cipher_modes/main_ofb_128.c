/***************************************************************************//**
 * @file
 * @brief AES OFB 128-bit example for EFM32
 * @author Energy Micro AS
 * @version 1.10
 *******************************************************************************
 * @section License
 * <b>(C) Copyright 2013 Energy Micro AS, http://www.energymicro.com</b>
 *******************************************************************************
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 * 4. The source and compiled code may only be used on Energy Micro "EFM32"
 *    microcontrollers and "EFR4" radios.
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
#include <stdint.h>
#include <stdbool.h>
#include "em_device.h"
#include "em_chip.h"
#include "em_cmu.h"
#include "aes_ofb_128.h"

const uint8_t exampleData[] = { 0x6B, 0xC1, 0xBE, 0xE2, 0x2E, 0x40, 0x9F, 0x96, 
                                0xE9, 0x3D, 0x7E, 0x11, 0x73, 0x93, 0x17, 0x2A,
                                0xAE, 0x2D, 0x8A, 0x57, 0x1E, 0x03, 0xAC, 0x9C, 
                                0x9E, 0xB7, 0x6F, 0xAC, 0x45, 0xAF, 0x8E, 0x51,
                                0x30, 0xC8, 0x1C, 0x46, 0xA3, 0x5C, 0xE4, 0x11, 
                                0xE5, 0xFB, 0xC1, 0x19, 0x1A, 0x0A, 0x52, 0xEF,
                                0xF6, 0x9F, 0x24, 0x45, 0xDF, 0x4F, 0x9B, 0x17, 
                                0xAD, 0x2B, 0x41, 0x7B, 0xE6, 0x6C, 0x37, 0x10};

const uint8_t expectedEncryptedData[] = { 0x3B, 0x3F, 0xD9, 0x2E, 0xB7, 0x2D, 0xAD, 0x20, 
                                          0x33, 0x34, 0x49, 0xF8, 0xE8, 0x3C, 0xFB, 0x4A,
                                          0x77, 0x89, 0x50, 0x8D, 0x16, 0x91, 0x8F, 0x03, 
                                          0xF5, 0x3C, 0x52, 0xDA, 0xC5, 0x4E, 0xD8, 0x25,
                                          0x97, 0x40, 0x05, 0x1E, 0x9C, 0x5F, 0xEC, 0xF6, 
                                          0x43, 0x44, 0xF7, 0xA8, 0x22, 0x60, 0xED, 0xCC,
                                          0x30, 0x4C, 0x65, 0x28, 0xF6, 0x59, 0xC7, 0x78, 
                                          0x66, 0xA5, 0x10, 0xD9, 0xC1, 0xD6, 0xAE, 0x5E};

const uint8_t exampleKey[] = { 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 
                               0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C};

uint8_t dataBuffer[sizeof(exampleData) / sizeof(exampleData[0])];

/* Initialization vector used during Cipher Feedback.*/
const uint8_t    initVector[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F };

/**************************************************************************//**
 * @brief  Main function
 *         The example data is first encrypted and encrypted data is checked.
 *         The encrypted data is then decrypted and checked against original data.
 *         Program ends at last while loop if all is OK.
 *****************************************************************************/
int main(void)
{
  uint32_t i;
  
  /* Chip errata */
  CHIP_Init();

  /* Initialize error indicator */
  bool error = false;

  /* Enable AES clock */
  CMU_ClockEnable(cmuClock_AES, true);
  
  /* Copy plaintext to dataBuffer */
  for (i=0; i<(sizeof(exampleData) / sizeof(exampleData[0])); i++)
  {
    dataBuffer[i] = exampleData[i];
  }

  /* Encrypt data in AES-128 OFB */
  AesOfb128(exampleKey,
            dataBuffer,
            dataBuffer,
            sizeof(dataBuffer) / (sizeof(dataBuffer[0]) * 16),
            initVector);

  /* Wait for AES to finish */
  while (!AesFinished());

  /* Check whether encrypted results are correct */
  for (i = 0; i < (sizeof(dataBuffer) / sizeof(dataBuffer[0])); i++)
  {
    if (dataBuffer[i] != expectedEncryptedData[i])
    {
      error = true;
    }
  }

  /* Decrypt data in AES-128 OFB. Note that this is the same operation as encrypt */
  AesOfb128(exampleKey,
            dataBuffer,
            dataBuffer,
            sizeof(dataBuffer) / (sizeof(dataBuffer[0]) * 16),
            initVector);

  /* Wait for AES to finish */
  while (!AesFinished());

  /* Check whether decrypted result is identical to the plaintext */
  for (i = 0; i < (sizeof(dataBuffer) / sizeof(dataBuffer[0])); i++)
  {
    if (dataBuffer[i] != exampleData[i])
    {
      error = true;
    }
  }

  /* Check for success */
  if (error)
  {
    while (1) ; /* Ends here if there has been an error */
  }
  else
  {
    while (1) ; /* Ends here if all OK */
  }
}
