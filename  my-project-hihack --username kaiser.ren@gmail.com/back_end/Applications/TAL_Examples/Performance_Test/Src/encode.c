/* ----------------------------------------------------------------------------
 *         ENCODE SOURCE CODE
 * ----------------------------------------------------------------------------
 * Copyright (c) 2012, kren
 */

/**
 * \file encode.c
 *
 * Implements machester modulation.
 *
 */
/*----------------------------------------------------------------------------
 *        Headers
 *----------------------------------------------------------------------------*/
#include "hal.h"

encode_t enc;
uint8_t tmr0_occur = 0;
uint8_t ticker = 0 ;

/*----------------------------------------------------------------------------
 *        ISR Handler
 *----------------------------------------------------------------------------*/
/**
 *  \brief Interrupt handler for TMR0.
 *
 * Server routine when TMR0 Compare Int occur.
 */
ISR(TIMER0_COMPA_vect)
{
  	if(0x80 == enc.port)
		PORTF |= _BV(PORTF1)  ;  	
  	else
	  	PORTF &= ~(_BV(PORTF1))  ;
	ticker++;
	tmr0_occur = 1; 	
}

/*----------------------------------------------------------------------------
 *        Exported functions
 *----------------------------------------------------------------------------*/
/**
 *  \brief TMR0 initialization
 *
 * F_CPU: 4MHz
 * Prescaler: 8
 * Ticker: 4MHz/8 = 2us,
 * CNT: 250us/2us = 125.
 */
void tmr0_init(void)
{
  	/* variable initialization. */
  	memset(&enc, 0, sizeof(encode_t) );
	
    /* CTC mode, no compare unit */
	TCCR0A = _BV(WGM01);

	/* Clock source and prescaler, 8 */
	TCCR0B = _BV(CS01);

    /* load compare cnt. */
    OCR0A = 125;

    /* make PF1, ADC_INPUT1, OC0A, to output wave. */
	DDRF |= _BV(DDF1);

	/* clear int flag and enable OCR0A compare. */
	TIFR0 = 0xFF;
	TIMSK0 = _BV(OCIE0A);
}

/*
 * Get us1 buffer amount.
*/

/*
 * Find proper factor depending on state and bit mask.
*/

void findParam(uint8_t bit_msk, mod_state_t state)
{
	if( 0 == ticker % 2){
		if( enc.data & ( 1 << bit_msk) ){
			enc.edge = rising;
            enc.port = 0x00;	//next is 0x80, for rising
		}
		else{
			enc.edge = falling;
            enc.port = 0x80;	//next is 0x00, for falling
		}
	}
	else{
	  	if( rising == enc.edge ){
		  	enc.port = 0x80;	//rising	
	  	}
	  	else{
		  	enc.port = 0x00;	//falling
	  	}
		enc.state = state;	//state switch
	}
}

/*
 * State machine for machester encoding.
 * Achieve wave phase.
*/

void encode_machine(void)
{
  	uint8_t c;
  	mod_state_t sta = enc.state;

	switch(sta){
		case Waiting:
		  	if( 0 == ticker % 2){
			  	c = sio_getchar_nowait();
				if( c == 0xff){	//no byte	
					enc.port = 0x00;	//next is 0x80, rising	
					enc.byte_rev = 0;
				}
				else{	//new byte
					enc.data = c;
					enc.byte_rev = 1;
					enc.port = 0x80;	//next is 0x00, falling
				}
		  	}
			else{
			  	if(enc.byte_rev){//new byte
			  	 	enc.port = 0x00;	//falling
					enc.state = Sta0;	//state switch
				}
				else{	//no byte
				    enc.port = 0x80;	//rising, keep in waiting state
				  	//enc.port = 0x00;
				}
			}
			break;
			//
		case Sta0: 	//prepare for sta1, rising edge
		  	if( 0 == ticker % 2){
			 	enc.port = 0x00;	//next is 0x80, for rising
			}
			else{
				enc.port = 0x80;	//rising
				enc.state = Sta1;  	//state switch
			}
			break;
			//
		case Sta1:	//prepare for sta2, falling edge
		  	if( 0 == ticker % 2){
			 	enc.port = 0x80;	//next is 0x00, for falling
			}
			else{
				enc.port = 0x00;	//falling
				enc.state = Sta2; 	//state switch
			}
			break;
			//
	  	case Sta2:	//prepare for sta3, rising edge
		  	if( 0 == ticker % 2){
			 	enc.port = 0x00;	//next is 0x80, for rising
			}
			else{
				enc.port = 0x80;	//rising
				enc.state = Sta3;	//state switch
			}
			break;
			//
		case Sta3:	//prepare for bit7
		  	findParam(BIT7, Bit7);
			break;
			//
		case Bit7: 	//prepare for Bit6
		  	findParam(BIT6, Bit6);
	    	break;
			//
		case Bit6: 	//prepare for Bit5
		  	findParam(BIT5, Bit5);
	    	break;
			//
		case Bit5: 	//prepare for Bit4
		  	findParam(BIT4, Bit4);
	    	break;
			//
		case Bit4: 	//prepare for Bit3
		  	findParam(BIT3, Bit3);
	    	break;
			//
		case Bit3: 	//prepare for Bit2
		  	findParam(BIT2, Bit2);
	    	break;
			//
		case Bit2: 	//prepare for Bit1
		  	findParam(BIT1, Bit1);
	    	break;
			//
		case Bit1: 	//prepare for Bit0
		  	findParam(BIT0, Bit0);
	    	break;
			//
		case Bit0: 	//new byte is exist?
		  	if( 0 == ticker % 2){
				enc.port = 0x00;	//next is 0x80, for rising
			}
			else{
			  	enc.port = 0x80;	
				enc.state = Sto0;	//state switch
			}
	    	break;
			//
		case Sto0:     //prepare for sto1
			if( 0 == ticker % 2){
				enc.port = 0x00;	//next is 0x80, for rising
			}
			else{
				enc.port = 0x80;	//rising	
				enc.byte_rev = 0;
				enc.state = Waiting;	//state switch
			}
			break;
			//
#if 0
		case Sto1:		//go to waiting mode
			enc.state = Waiting;
			break;
			//
#endif
		default:
	  		break;
	  		//
	}
}

//end of file
