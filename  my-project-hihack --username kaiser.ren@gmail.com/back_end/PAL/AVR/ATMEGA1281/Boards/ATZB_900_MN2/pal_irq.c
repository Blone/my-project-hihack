/**
 * @file pal_irq.c
 *
 * @brief PAL IRQ functionality
 *
 * This file contains functions to initialize, enable, disable and install
 * handler for the transceiver interrupts.
 *
 * $Id: pal_irq.c 22839 2010-08-10 09:35:20Z sschneid $
 *
 * @author    Atmel Corporation: http://www.atmel.com
 * @author    Support email: avr@atmel.com
 */
/*
 * Copyright (c) 2009, Atmel Corporation All rights reserved.
 *
 * Licensed under Atmel's Limited License Agreement --> EULA.txt
 */

/* === Includes ============================================================ */

#include <stdint.h>
#include "pal.h"
#include "pal_boardtypes.h"

#if (BOARD_TYPE == ATZB_900_MN2)

/* === Types ============================================================== */

/**
 * This is a typedef of the function which is called from the transceiver ISR
 */
typedef void (*irq_handler_t)(void);

/* === Globals ============================================================= */

/*
 * Function pointers to store the callback function of
 * the transceiver interrupt
 */
static irq_handler_t irq_hdl_trx;

/* === Prototypes ========================================================== */

/* === Implementation ====================================================== */

/**
 * @brief Initializes the main transceiver interrupt
 *
 * This function sets the microcontroller specific registers
 * responsible for handling the main transceiver interrupt
 *
 * @param trx_irq_cb Callback function for the main transceiver interrupt
 */
void pal_trx_irq_init(FUNC_PTR trx_irq_cb)
{
    /*
     * Set the handler function.
     * The handler is set before enabling the interrupt to prepare for spurious
     * interrupts, that can pop up the moment they are enabled
     */
    irq_hdl_trx = (irq_handler_t)trx_irq_cb;

    /* Init Main TRX interrupt */
    /* Select rising edge */
    EICRB |= _BV(ISC50) | _BV(ISC51);
    /* clear pending interrupt */
    EIFR = _BV(INTF5);
}


/**
 * @brief ISR for transceiver's main interrupt
 */
ISR(TRX_MAIN_ISR_VECTOR)
{
    irq_hdl_trx();
}

#endif /* ATZB_900_MN2 */

/* EOF */
