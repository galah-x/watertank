/* originally templated from LED Blink Example with USB Debug Channel for 
   Teensy USB Development Board   http://www.pjrc.com/teensy/
   should be no code remaining.

   Now code to drive a water tank header tank pump

 */


// how long the motor needs to run to claim a credit. //~5l/min
#define ON_CREDIT_TIME 12

// how many credits to allocate each PB press (about a tank) 
#define CREDIT_PER_PB  50

#define DAILY_CREDIT 250

// how many loop iterations (~100ms) between blink code frames
#define blink_frame_gap_time 15

// how many loop iterations (~100ms) for blink state start bit
#define blink_frame_on_time 1

// how many loop iterations (~100ms) between blink code bits
#define blink_seq_off_time 5

// how many loop iterations (~100ms) for blink code bits
#define max_blink_seq  5

// how to scale the credits to blinks... with max_blink_seq*credit_blink_sclae per blink
#define credit_blink_scale 10

#define T3_POST 15
/* speed up 10x for testing */
/*#define T3_POST 1 */
#define T3_POST4 4   
// 86400 seconds per day, but 8mhz / 2^^16 / 8 / 15.25 = 1.00057 hz

#define TICKS_PER_DAY 86350






// Teensy 2.0: LED is active high
// #if defined(__AVR_ATmega32U4__) || defined(__AVR_AT90USB1286__)
#define LED_ON		(PORTD |= (1<<6))
#define LED_OFF		(PORTD &= ~(1<<6))




#define LED_CONFIG	(DDRD |= (1<<6))
#define CPU_PRESCALE(n)	(CLKPR = 0x80, CLKPR = (n))
#define DIT 80		/* unit time for morse code */

//
// credit_btn B4
#define credit_btn_PORT_OUT PORTB
#define credit_btn_PORT_DDR DDRB
#define credit_btn_PORT_IN  PINB
#define credit_btn_PORT_BIT 4

// upper limit btn B5
#define header_tank_top_PORT_OUT PORTB
#define header_tank_top_PORT_DDR DDRB
#define header_tank_top_PORT_IN  PINB
#define header_tank_top_PORT_BIT 5

// lower limit button B6
#define header_tank_bot_PORT_OUT PORTB
#define header_tank_bot_PORT_DDR DDRB
#define header_tank_bot_PORT_IN  PINB
#define header_tank_bot_PORT_BIT 6

  
  
#define header_tank_top  (header_tank_top_PORT_IN & (1 << header_tank_PORT_BIT))
#define header_tank_bot  (header_tank_bot_PORT_IN & (1 << header_tank_PORT_BIT))
// switch closed when below. Switch closed == 0 input.
#define above_header_tank_top  ((header_tank_top_PORT_IN & (1 << header_tank_top_PORT_BIT)) != 0)
#define below_header_tank_top  ((header_tank_top_PORT_IN & (1 << header_tank_top_PORT_BIT)) == 0)
#define above_header_tank_bottom  ((header_tank_bot_PORT_IN & (1 << header_tank_bot_PORT_BIT)) != 0)
#define below_header_tank_bottom  ((header_tank_bot_PORT_IN & (1 << header_tank_bot_PORT_BIT)) == 0)
#define credit_pb_state    (credit_btn_PORT_IN & (1 << credit_btn_PORT_BIT))
#define credit_pb_pressed  ((credit_btn_PORT_IN & (1 << credit_btn_PORT_BIT)) == 0)
#define credit_pb_released ((credit_btn_PORT_IN & (1 << credit_btn_PORT_BIT)) != 0)


//// pump
#define pump_PORT_OUT PORTB
#define pump_PORT_DDR DDRB
#define pump_PORT_IN  PINB
#define pump_PORT_BIT 2

// pump output is inverted
#define PUMP_ON  	(pump_PORT_OUT &= ~(1<<pump_PORT_BIT))
#define PUMP_OFF 	(pump_PORT_OUT |= (1<<pump_PORT_BIT))



//// states
enum pump_state {
  PUMP_STATE_LIMITS_BROKEN,
  PUMP_STATE_ON,
  PUMP_STATE_OFF};

enum blink_state {
  BLINK_FRAME_GAP,
  BLINK_FRAME_ON,
  BLINK_SEQ_OFF,
  BLINK_SEQ_ON_CALC,
  BLINK_SEQ_ON,
  BLINK_LIMITS_OFF,
  BLINK_LIMITS_ON};


// time to wait for each mainloop iteration, in milliseconds.
// note it has to be a constant at compile time.
#define MAINLOOP_PERIOD 100

