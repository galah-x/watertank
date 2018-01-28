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
#define blink_frame_on_time 3

// how many loop iterations (~100ms) between blink code bits
#define blink_seq_off_time 5

// how many loop iterations (~100ms) for blink code bits
#define max_blink_seq  5

// how to scale the credits to blinks... with max_blink_seq*credit_blink_sclae per blink
#define credit_blink_scale 10

#define T3_POST 122
// 86400 seconds per day, but 8mhz / 2^^16 / 8 / 122 = 1.00057 hz

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
#define header_tank_bot_PORT_BIT 5

  
  
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


//
//// lockadj motor
//#define lockadj_motor_en_PORT_OUT PORTB
//#define lockadj_motor_en_PORT_DDR DDRB
//#define lockadj_motor_en_PORT_IN  PINB
//#define lockadj_motor_en_PORT_BIT 3
//
//// motor h bridge in0
//#define hbridge_in0_PORT_OUT PORTB
//#define hbridge_in0_PORT_DDR DDRB
//#define hbridge_in0_PORT_IN  PINB
//#define hbridge_in0_PORT_BIT 0
//
////  motor h bridge in1 
//#define hbridge_in1_PORT_OUT PORTB
//#define hbridge_in1_PORT_DDR DDRB
//#define hbridge_in1_PORT_IN  PINB
//#define hbridge_in1_PORT_BIT 1
//

// 
// // row2 B6
// #define ROW2_PORT_OUT PORTB
// #define ROW2_PORT_DDR DDRB
// #define ROW2_PORT_IN  PINB
// #define ROW2_PORT_BIT 6
// 
// // row3 F7
// #define ROW3_PORT_OUT PORTF
// #define ROW3_PORT_DDR DDRF
// #define ROW3_PORT_IN  PINF
// #define ROW3_PORT_BIT 7
// 
// // echo F5
//#define echo_PORT_OUT PORTF
//#define echo_PORT_DDR DDRF
//#define echo_PORT_IN  PINF
//#define echo_PORT_BIT 5


// servo B5
// #define servo_PORT_OUT PORTB
// #define servo_PORT_DDR DDRB
// #define servo_PORT_IN  PINB
// #define servo_PORT_BIT 5
// servo B5
//#define activitylight_PORT_OUT PORTB
//#define activitylight_PORT_DDR DDRB
//#define activitylight_PORT_IN  PINB
//#define activitylight_PORT_BIT 5


// cols were inputs

// // bar_locked F4
//#define bar_locked_PORT_OUT PORTF
//#define bar_locked_PORT_DDR DDRF
//#define bar_locked_PORT_IN  PINF
//#define bar_locked_PORT_BIT 4
// 
// // remote_n input F0 from radio sensor 
//#define remote_n_PORT_OUT PORTF
//#define remote_n_PORT_DDR DDRF
//#define remote_n_PORT_IN  PINF
//#define remote_n_PORT_BIT 0
// 
// bar_unlocked F6
//#define bar_unlocked_PORT_OUT PORTF
//#define bar_unlocked_PORT_DDR DDRF
//#define bar_unlocked_PORT_IN  PINF
//#define bar_unlocked_PORT_BIT 6
// 
// 
//#define trig_ON  (trig_PORT_OUT |= (1 << trig_PORT_BIT))
//#define trig_OFF  (trig_PORT_OUT &= ~(1 << trig_PORT_BIT))


// 
// 













// no port allocation yet as no pulldowns fitted;
//#define READ_door_closed    0
 

//#define BLUELEDNUM   9
//#define GREENLEDNUM 10
//#define REDLEDNUM   12


// pc6 Output Compare and PWM output A for Timer/Counter3
// timer/counter3 is 16 bit.

//#define BLUELED_OUT PORTC
//#define BLUELED_DDR DDRC
//#define BLUELED_IN  PINC
//#define BLUELED_BIT 6

//#define TMAX 255

// row2 B6
//#define GREENLED_OUT PORTC
//#define GREENLED_DDR DDRC
//#define GREENLED_IN  PINC
//#define GREENLED_BIT 7

// row3 F7
//#define REDLED_OUT PORTD
//#define REDLED_DDR DDRD
//#define REDLED_IN  PIND
//#define REDLED_BIT 7






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
  BLINK_SEQ_ON};


// time to wait for each mainloop iteration, in milliseconds.
// note it has to be a constant at compile time.
#define MAINLOOP_PERIOD 100

