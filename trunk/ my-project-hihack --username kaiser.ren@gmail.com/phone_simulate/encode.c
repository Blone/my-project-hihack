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
#include "encode.h"

/*----------------------------------------------------------------------------
 *        Variable
 *----------------------------------------------------------------------------*/
modulate_t mod;
static uint8_t odd;	//odd parity

void findParam(uint8_t bit_msk);

/*----------------------------------------------------------------------------
 *        ISR Handler
 *----------------------------------------------------------------------------*/
/**
 *  \brief Interrupt handler for DACC.
 *
 * Server routine when DACC complete the convertion.
 */
void DAC_IrqHandler(void)
{
	uint32_t status ;

    status = DACC_GetStatus( DACC ) ;

    /* if conversion is done*/
    if ( (status & DACC_IER_EOC) == DACC_IER_EOC ){
		if ( index_sample >= SAMPLES ){
		  	if(0 == mod.reverse)
				encode_switch();
			index_sample = 0;
			ticker = 0;
		}
		else if( ( 50 == index_sample ) && (Div2 == mod.factor ) ){
			encode_switch();
			ticker = 0;
		}
		else if( ( 50 == index_sample ) && ( 1 == mod.reverse ) ){
		  	mod.reverse = 0;
			encode_switch();
			ticker = 0;
		}
		DACC->DACC_IDR = DACC_IER_EOC;
	}

}

/**
 *  \brief Interrupt handler for USART.
 *
 * Increments the number of bytes received in the
 * current second and starts another transfer if the desired bps has not been
 * met yet.
 */
void USART1_IrqHandler(void)
{
    uint32_t status;

    /* Read USART status*/
    status = BOARD_USART_BASE->US_CSR;

    /* Receive byte is stored in buffer. */
    if ((status & US_CSR_RXRDY) == US_CSR_RXRDY) {
		if(us1.count < 20){
	    	us1.buff[us1.head++] = USART_GetChar(BOARD_USART_BASE);
			us1.count++;
			if(us1.head >= 20)
				us1.head = 0;
	  	}
		else{
			us1.buff[us1.head] = USART_GetChar(BOARD_USART_BASE);
		}
    }
}

/**
 *  \brief Interrupt handler for SysTick.
 *
 */
void SysTick_Handler( void )
{
    //uint32_t status ;
	uint16_t value;

    //status = DACC_GetStatus( DACC ) ;

    /* if conversion is done*/
    //if ( (status & DACC_ISR_EOC) == DACC_ISR_EOC )
    {
		if ( 0 == ( ticker%mod.factor) ){
		  	//if( Waiting != mod.state){
		  	 	value = sine_data[index_sample++] * amplitude / (MAX_DIGITAL/2) + MAX_DIGITAL/2;
        		DACC_SetConversionData(DACC, value ) ;
				DACC->DACC_IER = DACC_IER_EOC;
			//}
			//else{
			//	index_sample++;
			//	DACC_SetConversionData( DACC,sine_data[90]*amplitude/(MAX_DIGITAL/2)+MAX_DIGITAL/2);
			//	DACC->DACC_IER = DACC_IER_EOC;
			//}
		}
		ticker++;
    }
}

/*----------------------------------------------------------------------------
 *        Exported functions
 *----------------------------------------------------------------------------*/
/**
 *  \brief USART hardware handshaking configuration
 *
 * Configures USART in hardware handshaking mode, asynchronous, 8 bits, 1 stop
 * bit, no parity, 115200 bauds and enables its transmitter and receiver.
 */
void _ConfigureUsart( void )
{
    uint32_t mode = US_MR_USART_MODE_NORMAL
                        | US_MR_USCLKS_MCK
                        | US_MR_CHRL_8_BIT
                        | US_MR_PAR_NO
                        | US_MR_NBSTOP_1_BIT
                        | US_MR_CHMODE_NORMAL ;

    /* Enable the peripheral clock in the PMC*/
    PMC->PMC_PCER0 = 1 << BOARD_ID_USART ;

    /* Configure the USART in the desired mode @115200 bauds*/
    USART_Configure( BOARD_USART_BASE, mode, 115200, BOARD_MCK ) ;

    /* Configure the RXBUFF interrupt*/
    NVIC_EnableIRQ( USART1_IRQn ) ;

    /* Enable receiver & transmitter*/
    USART_SetTransmitterEnabled( BOARD_USART_BASE, 1 ) ;
    USART_SetReceiverEnabled( BOARD_USART_BASE, 1 ) ;
	
	 BOARD_USART_BASE->US_IER = US_IER_RXRDY ;
}
/*
 * Get us1 buffer amount.
*/
uint8_t us1_get_count(void)
{
  	return (us1.count);
}

/*
 * Get a charactors.
*/
uint8_t us1_get_char(void)
{
  	uint8_t temp;
	
	if(us1.count){
    	temp = us1.buff[us1.tail++];
		us1.count--;
		if(us1.tail >= 20){
			us1.tail = 0;
		}
  	}
    return temp;
}
/*
 * Find proper factor depending on state and bit mask.
*/

void findParam(uint8_t bit_msk)
{
  	if( ( mod.data & ( 1 << bit_msk) ) && (SET == mod.cur) ){
	  	odd++;	//bit is one, odd cnt increacement
  		mod.factor = Div1;
		if( 50 == index_sample )
		  	mod.reverse = 1;
	}
	else if( ( mod.data & ( 1 << bit_msk) ) && (CLR == mod.cur) ){
	  	odd++;	//bit is one, odd cnt increacement
		mod.factor = Div2;
		mod.cur = SET;
	}
	else if( !( mod.data & ( 1 << bit_msk) ) && (SET == mod.cur) ){
		mod.factor = Div2;
		mod.cur = CLR;
	}
	else if( !( mod.data & ( 1 << bit_msk) ) && (CLR == mod.cur) ){
		mod.factor = Div1;
		if( 50 == index_sample )
		  	mod.reverse = 1;
	}
}

/*
 * State machine for machester encoding.
 * Achieve wave phase.
*/

void encode_switch(void)
{
  	state_t sta = mod.state;
	
	switch(sta){
		case Waiting:
			if(us1_get_count() ){ 	//prepare for sta0
				mod.state = Sta0; 	//next state is start bit
				mod.factor = Div2;  //start bit is zero
				mod.cur = CLR;
				mod.data = us1_get_char();
				index_sample = 0;
			}
			break;
			//
		case Sta0: 	//prepare for bit0
		  	odd = 0;	//clear odd cnt
		  	findParam(BIT0);
	  		mod.state = Bit0;
			break;
			//
		case Sta1:	//prepare for sta2
		  	mod.state = Sta2;
			break;
			//
	  	case Sta2:	//prepare for sta3
		  	mod.state = Sta3;
			break;
			//
		case Sta3:	//prepare for bit7, sta3 is SET
		  	if( mod.data & (1 << BIT7) ){
		  		mod.factor = Div1;
				mod.cur = SET;
				if( 50 == index_sample )
		  			mod.reverse = 1;
		  	}
			else{
				mod.factor = Div2;
				mod.cur = CLR;	
			}
		  	mod.state = Bit7;
			break;
			//	
		case Bit0: 	//prepare for Bit1
		  	findParam(BIT1);
			mod.state = Bit1;
	    	break;
			//
		case Bit1: 	//prepare for Bit2
		  	findParam(BIT2);
			mod.state = Bit2;
	    	break;
			//
		case Bit2: 	//prepare for Bit3
		  	findParam(BIT3);
			mod.state = Bit3;
	    	break;
			//
		case Bit3: 	//prepare for Bit4
		  	findParam(BIT4);
			mod.state = Bit4;
	    	break;
			//
		case Bit4: 	//prepare for Bit5
		  	findParam(BIT5);
			mod.state = Bit5;
	    	break;
			//
		case Bit5: 	//prepare for Bit6
		  	findParam(BIT6);
			mod.state = Bit6;
	    	break;
			//
		case Bit6: 	//prepare for Bit7
		  	findParam(BIT7);
			mod.state = Bit7;
	    	break;
			//
		case Bit7: 	//prepare for Parity
		  	if( ( 0 == ( odd % 2 ) ) && (SET == mod.cur) ){//there is even 1(s), output 1, cur is 1 	
				mod.factor = Div1;
				mod.cur = SET;
				if( 50 == index_sample )
					mod.reverse = 1;
			}
			else if( ( 0 == ( odd % 2 ) ) && (CLR == mod.cur) ){//there is even 1(s), output 1, cur is 0 	
				mod.factor = Div2;
				mod.cur = SET;
			}
			else if( ( 1 == ( odd % 2 ) ) && (CLR == mod.cur) ){//there is odd 1(s), output 0, cur is 0 	
				mod.factor = Div1;
				mod.cur = CLR;
				if( 50 == index_sample )
					mod.reverse = 1;
			}
			else if( ( 1 == ( odd % 2 ) ) && (SET == mod.cur) ){//there is odd 1(s), output 0, cur is 1
				mod.factor = Div2;
				mod.cur = CLR;
			}
			mod.state = Parity;
	    	break;
			//
		case Parity:	//prepare for stop bit
		  	if(SET == mod.cur){
		  		mod.factor = Div1;
				mod.cur = SET;
				if( 50 == index_sample )
					mod.reverse = 1;	
			}
			else{
				mod.factor = Div2;
				mod.cur = CLR;
			}
			mod.state = Sto0;
		  	break;
            //
		case Sto0:     //prepare for Waiting
			mod.state = Waiting;
			mod.factor = Div1;
			mod.cur = SET;
			break;
			//
		default:
	  		break;
	  		//
	}
}


//end of file