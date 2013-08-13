/***************************************************************************//**
 * @file
 * @brief AES CBC 256-bit interrupt driven functions for EFM32
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
 
#include <stdbool.h>
#include "em_device.h"
#include "aes_cbc_256.h"

static uint16_t          numberOfBlocks;
static uint16_t          blockIndex;
static uint32_t*         outputDataG;
static const uint32_t*   inputDataG;
static const uint32_t*   keyG;
static volatile uint32_t prev[4];
static volatile bool     aesFinished;
static volatile bool     decryptG;

/**************************************************************************//**
 * @brief  AES IRQ Handler
 *****************************************************************************/
void AES_IRQHandler(void)
{
  int i; 
  
  /* Clear interrupt flag */
  AES->IFC = AES_IFC_DONE;

  /* Decryption */
  if (decryptG)
  {
  /* Writing to the XOR register here does NOT trigger decryption */                           
    for (i = 3; i >= 0; i--)
    {
      AES->XORDATA = __REV(prev[i]);
      prev[i]      = inputDataG[i];
    }                        
    inputDataG += 4;
      
    /* Store decrypted data */                         
    for (i = 3; i >= 0; i--)
    {
      outputDataG[i] = __REV(AES->DATA);
    }  
    outputDataG += 4;
    
    if (blockIndex < (numberOfBlocks-1))
    {
      /* Write only the KEYH/LA 4 times to transfer the key to all 4
       * high and low key registers  */        
      for (i = 3; i >= 0; i--)
      {
        AES->KEYLA = __REV(keyG[i+4]);
        AES->KEYHA = __REV(keyG[i]);
      }    

      /* Load data and trigger a decryption */      
      for (i = 3; i >= 0; i--)
      {
        AES->DATA = __REV(inputDataG[i]);
      }
    }

    /* Indicate last block */
    else
    {
      aesFinished = true;
    }

    /* Increase block index */
    blockIndex++;
  }

  /* Encryption */
  else
  {
    /* Store encrypted data */      
    for (i = 3; i >= 0; i--)
    {
      outputDataG[i] = __REV(AES->DATA);
    }  
    outputDataG += 4;

    if (blockIndex < (numberOfBlocks-1))
    {
      /* Load low and high key */
      /* Write only the KEYH/LA 4 times to transfer the key to all 4
       * high and low key registers */     
      for (i = 3; i >= 0; i--)
      {
        AES->KEYLA = __REV(keyG[i+4]);
        AES->KEYHA = __REV(keyG[i]);
      }
      
      /* Load data and trigger encryption using XORDATA */
      inputDataG += 4;        
      for (i = 3; i >= 0; i--)
      {
        AES->XORDATA = __REV(inputDataG[i]);
      }
    }

    /* Indicate last block */
    else
    {
      aesFinished = true;
    }

    /* Increase block index */
    blockIndex++;
  }
}
/**************************************************************************//**
 * @brief  Encrypt/decrypt with 256-bit key in CBC mode. Function returns after
 * initializing encryption/decryption. Use AesFinished() to check for 
 * completion. Output data only valid after completion. 
 *
 * @param[in] key
 *   When doing encryption, this is the 256-bit encryption key. When doing
 *   decryption, this is the 256-bit decryption key. The decryption key may
 *   be generated from the encryption key with AES_DecryptKey256().
 *
 * @param[in] inputData
 *   Buffer holding data to encrypt/decrypt.
 *
 * @param[out] outputData
 *   Buffer to put output data, must be of size blockNumber*16 bytes. This can
 *   be the same location as inputData.
 *
 * @param[in] decrypt
 *   True to decrypt, false to encrypt
 *
 * @param[in] blockNumber
 *   Number of 128-bit blocks to decrypt/encrypt.
 *
 * @param[in] iv
 *   128-bit initialization vector to use
 *****************************************************************************/
void AesCBC256(const uint8_t* key,
               const uint8_t* inputData,
               uint8_t*       outputData,
               bool           decrypt,
               const uint32_t blockNumber,
               const uint8_t* iv)
{
  int i;
  int j;
  
  uint32_t* iv32 = (uint32_t*) iv;
  
  /* Copy to global variables */
  inputDataG     = (uint32_t*) inputData;
  outputDataG    = (uint32_t*) outputData;
  keyG           = (uint32_t*) key;
  numberOfBlocks = blockNumber;

  /* Global flag for selection encryption or decryption */
  decryptG = decrypt;

  /* Reset block index */
  blockIndex = 0;

  /* Initialize finished flag */
  aesFinished = false;

  /* Clear and enable AES interrupt */
  AES->IFC = AES_IFC_DONE;
  AES->IEN = AES_IEN_DONE;
  NVIC_EnableIRQ(AES_IRQn);

  /* Load low and high key */
  /* Write only the KEYH/LA 4 times to transfer the key to all 4
   * high and low key registers */  
  for (i = 3, j = 7; i >= 0; i--, j--)
  {
    AES->KEYLA = __REV(keyG[j]);
    AES->KEYHA = __REV(keyG[i]);
  }

  /* Decryption */
  if (decryptG)
  {
    /* Configure AES module */
    AES->CTRL = AES_CTRL_AES256 |
                AES_CTRL_DECRYPT |    /* Set decryption */
                AES_CTRL_DATASTART;   /* Start decryption on data write */

    /* Make a copy of the initialization vector */
    for (i = 0; i <= 3; i++)
    {
      prev[i] = iv32[i];
    }
    /* Load data and trigger a decryption */   
    for (i = 3; i >= 0; i--)
    {
      AES->DATA = __REV(inputDataG[i]);
    }
  }

  /* Encryption */
  else
  {
    AES->CTRL = AES_CTRL_AES256 |
                AES_CTRL_XORSTART; /* Start encryption on data write */

    /* Load data but will NOT trrigger encryption */      
    for (i = 3; i >= 0; i--)
    {
      AES->DATA = __REV(iv32[i]);
    }

    /* Load data and trigger encryption using XORDATA*/     
    for (i = 3; i >= 0; i--)
    {
      AES->XORDATA = __REV(inputDataG[i]);
    }
  }
}

/**************************************************************************//**
 * @brief  Function to check if AES process has finished
 * @return true if AES operation has finished
 *****************************************************************************/
bool AesFinished(void)
{
  return aesFinished;
}


