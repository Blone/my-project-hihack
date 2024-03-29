/**
 * @file timer.h
 *
 * @brief hal board specific functionality
 *
 * This file implements hal board specific functionality.
 *
 * @author    kren
 * @data		Nov 21, 2012
 */


/* === Includes ============================================================ */

/* === MACROS ============================================================== */
//MCK:4MHz, Pre Scale: 62.5KHz
#define	PWM_TMR0_CLK_SRC_PRE_SCALE  (64ul)
#define	PWM_TMR0_CLK_SRC_BIT_DEF ( _BV(CS01) | _BV(CS00) )
#define 	PWM_TMR0_CMP_OUT_FREQ (500ul)
#define 	PWM_TMR0_CMP_OUT_OCRA(freq, cpu, prescale) (cpu/freq/prescale/2)
#define 	PWM_TMR0_CMP_OUT_PULSE_CNT 16

/* === Types =============================================================== */
typedef struct ac_cap_tag
{
  	uint8_t occur;
	uint16_t interval;
	uint16_t cur_stamp;
}ac_cap_t;

typedef struct pwm_out_tag
{
  	uint8_t toggle_cnt;
	uint8_t data;
}pwm_out_t;

/* === EXTERNALS =========================================================== */
extern ac_cap_t ac_cap_para;
extern pwm_out_t pwm_para;

/* === PROTOTYPES ========================================================== */
void pwm_init(void);
void pwm_uninit(void);
void pwm_set_freq(uint16_t freq);
//eof

