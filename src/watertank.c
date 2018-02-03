/* watertank filler firmware 
 *
 * most all is my code now
 * Time-stamp: "2018-02-03 16:56:16 john";
 * John Sheahan December 2017
 *
 */
 
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay_basic.h>
#include <util/delay.h>
#include "usb_debug_only.h"
#include "print.h"
#include "watertank.h"
#include "i2c_master.h"
#include "printd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avr/interrupt.h>

// uint8_t poll_keypad(void);
void setup_io (void);
void enable_t3_int(void) ;


/* water tank filler controller
 * This is the firmware for a controller to fill a header tank from the farm 
 * main rainwater tanks.
 * The header tank has a couple of reasons for its existance.
 * 1. more pressure. Wanted sufficient to fill a barn roof mounted 
 *  solar hot water
 * 2. provide a measure of, and some protection against bad, water leaks. 
 *  By some combination of counting the number of header tank fill cycles, and 
 *  timing the fill time, I can get a measure of the amount of water consumed in a day.   Or some convenient period.    If the consumption is too high (like a significant leak) then the controller refuses to fill more without operator intervention. 
 * 
 * So I want to time the pump runtime (it seems to be about 3 minutes to get 
 * to the initial level, then total 10 minutes to fill the header tank.
 * The header tank has 2 level sensors, one at ~20%, one at ~80% full.    
 * 
 * Also want to allocate  credits, say ~ 250l per day. Which hopefully is 
 * sufficient for a heavy useage day.  Credits are allocated daily, but don't accumulate past ~~ 1 days worth.       Time resolution of 1s would be heaps.
 * 
 * Ability to manually add credits for some special occasion. ~~ days worth.
 * ability to disable pump.
 * 
 */

/* Implementation
 * Some sort of interrupt accumulating time, with roughly 1s resolution. 
 * counts for at least 24h or 86,400 seconds.
 * A foreground loop, checking UI, limit buttons,
 * a little app next to the loop setting state.
 * when the ebay stuff arrives, state to be displayed on a 3v3 lcd
 * in the interim, have a red led with a blink code.   something like 50l per half second 
 * or perhaps one short blink, then up to 5 half second blinks showing amout. Then 1.5 seconds off.    
 * Such a blink code would be a pain on a 1 second update loop. So I either have to run the main loop at few hundred ms (which seems practical) or maybe run the different subloops off a timer ISR. 
 */




// delay needs a compile time constant, so make it a macro
// #define BLINK_LED_OK    LED_ON; _delay_ms(BLINK_OK);   LED_OFF;
// #define BLINK_LED_BAD  LED_ON;  _delay_ms(BLINK_BAD);  LED_OFF;


// counts 'seconds' for at least a day
uint32_t     timer;

// counts 'seconds' for the motor on... to get to a motor credit. 
uint8_t     on_timer;
uint8_t     pump_state;

int main(void)
{
  int i;
  uint8_t     blink_timer;
  uint8_t     blink_state;
  uint8_t     blink_seq_on_time;
  uint8_t     last_credit_pb;  // debounces pushbutton
  int16_t     credits;
  int16_t     blink_seq_count;
  uint8_t     debug_count;
  uint16_t    time_temp;  
  uint8_t    time_temp8;  
  uint8_t    limits_were_broken;  

  // set for 8 MHz clock, and make sure the LED is off
  CPU_PRESCALE(1);

  setup_io();
  i2c_init();
  LED_ON;
  _delay_ms(300);
  LED_OFF;
  _delay_ms(300);
  LED_ON;
  debug_count=0;

  // INITIALIZE the USB, but don't want for the host to
  // configure.  The first several messages sent will be
  // lost because the PC hasn't configured the USB yet,
  // need hid_listen running
  usb_init();
  on_timer = 0;
  blink_timer = 0;
  timer = 0;
  enable_t3_int();
  credits=0;
  pump_state = PUMP_STATE_OFF;
  blink_state = BLINK_FRAME_GAP;
  limits_were_broken = 0;
  
  for (i =0; i<2; i++) {
    LED_ON;
    _delay_ms(500);
    LED_OFF;
    _delay_ms(500);
    print("hello from watertank\n");
  }
  LED_ON;

  while (1) {
    _delay_ms(MAINLOOP_PERIOD);

    // pump state machine
    switch  (pump_state)
      {
      case PUMP_STATE_LIMITS_BROKEN:
	{
	  if (!((above_header_tank_top) && (below_header_tank_bottom)))
	    {
	      pump_state = PUMP_STATE_OFF;
	    } 
	  break;
	}
	
      case PUMP_STATE_ON:
	{
	  if (above_header_tank_top)
	    {
	      if (below_header_tank_bottom)
		{
		  pump_state = PUMP_STATE_LIMITS_BROKEN;
		  PUMP_OFF;
		  limits_were_broken = 1;
		  credits = 0;
		  break;
		}
	      pump_state = PUMP_STATE_OFF;
	      PUMP_OFF;
	    }
	  if (on_timer >= ON_CREDIT_TIME) {
	    if (credits > 0)
	      {
		credits--;
		on_timer = 0;
	      }
	    else 
	    { pump_state = PUMP_STATE_OFF;
	      PUMP_OFF;
	    }
	  }	      
	  break;
	}
      case PUMP_STATE_OFF:
	{
	  if (below_header_tank_bottom)
	    {
	      if (credits > 0)
		{
		  pump_state = PUMP_STATE_ON;
		  on_timer = 0;
		  PUMP_ON;
		}
	    }  
	  break;
	}
      }
    
    
    // UI state machine
    // today, UI inputs are the credit pushbutton.

    if ((last_credit_pb != credit_pb_state) &&  (credit_pb_pressed))
      {
	credits += CREDIT_PER_PB;
      }
    last_credit_pb = credit_pb_state;

    // blinker state machine
    // blink_timer is a single byte, so don't worry about ISRs / SEI / CLI
    blink_timer++;
    switch (blink_state)
      {
      case BLINK_FRAME_GAP:
	{ if (blink_timer == blink_frame_gap_time)
	    { blink_state = BLINK_FRAME_ON;
	      blink_timer=0;
	      LED_ON;
	    }
	  break;
	}
      case BLINK_FRAME_ON:
	{ if (blink_timer == blink_frame_on_time)
	    {
	      if (limits_were_broken)
		{
		  blink_state = BLINK_LIMITS_OFF;
		} else
		{
		  blink_state = BLINK_SEQ_OFF;
		}
	      blink_timer=0;
	      LED_OFF;
	      blink_seq_count=credits / credit_blink_scale;
	      
	    }
	  break;
	}
      case BLINK_LIMITS_OFF:
	{ if (blink_timer == blink_frame_gap_time)
	    { blink_state = BLINK_LIMITS_ON;
	      blink_timer=0;
	      LED_ON;
	    }
	  break;
	}
      case BLINK_LIMITS_ON:
	{ if (blink_timer == blink_frame_on_time)
	    { blink_state = BLINK_SEQ_OFF;
	      blink_timer=0;
	      LED_OFF;
	      blink_seq_count=credits / credit_blink_scale;

	    }
	  break;
	}
      case BLINK_SEQ_OFF:
	{ if (blink_seq_count > 0)
	    {
	      if (blink_timer == blink_seq_off_time)
		{ blink_state = BLINK_SEQ_ON_CALC;
		  blink_timer=0;
		  LED_ON;
		}
	    }
	  else
	    {
	      blink_state = BLINK_FRAME_GAP;
	      LED_OFF;
	    }
	  break;
	}

      case BLINK_SEQ_ON_CALC:
	{
	  if (blink_seq_count >= max_blink_seq)
	    {
	      blink_seq_on_time = max_blink_seq;
	      blink_seq_count = blink_seq_count - max_blink_seq;
	    }
	  else
	    {
	      blink_seq_on_time = blink_seq_count;
	      blink_seq_count = 0;
	    }
	  blink_state = BLINK_SEQ_ON;
	}
	// note fall through is intentional
	
      case BLINK_SEQ_ON:
	
	  if (blink_timer >= blink_seq_on_time)
	    { blink_state = BLINK_SEQ_OFF;
	      blink_timer=0;
	      LED_OFF;
	    }
	  break;
	}

	
	

    // issue credit
    cli();
    if (timer >= TICKS_PER_DAY)
      {
	timer = 0;
	if (credits <= DAILY_CREDIT)
	  {
	    credits = DAILY_CREDIT;
	  }
      }
    sei();
      

    // debug

    //    time_temp8 = timer;
    // phex1(time_temp8);
    // print("\n");
	  

    debug_count++;
    if (debug_count == 50)
      {
	print("credit=");
	printd3(credits);
	print(" on_timer=");
	phex16(on_timer);
	time_temp = timer;
	print(" timer=");
	phex16(time_temp);
	print(" pump_state=");

	if (pump_state == PUMP_STATE_LIMITS_BROKEN)
	  { print("LIMITS_BROKEN");
	  }
	else  
	  if (pump_state == PUMP_STATE_ON)
	    { print("ON");
	    }
	  else
	    if (pump_state == PUMP_STATE_OFF)
	      { print("OFF");
	      }
	
	print(" blink_state=");
	if (blink_state == BLINK_FRAME_GAP)
	  { print("FRAME_GAP");
	  }
	else  
	if (blink_state == BLINK_FRAME_ON)
	  { print("FRAME_ON");
	  }
	else  
	if (blink_state == BLINK_SEQ_OFF)
	  { print("SEQ_OFF");
	  }
	else  
	if (blink_state == BLINK_SEQ_ON)
	  { print("SEQ_ON");
	  }
	else  
	if (blink_state == BLINK_SEQ_ON_CALC)
	  { print("SEQ_ON_CALC");
	  }

	print( " top_level=");
	if (above_header_tank_top)
	  {
	    print( "above");
	  }
	if (below_header_tank_top)
	  {
	    print( "below");
	  }

	print( " bottom_level=");
	if (above_header_tank_bottom)
	  {
	    print( "above");
	  }
	if (below_header_tank_bottom)
	  {
	    print( "below");
	  }


	print("\n");
	debug_count = 0;
      }
	
    
  }
}





void setup_io (void) {
//  //   // set outputs, ddr=1
//  trig_PORT_DDR |= (1 << trig_PORT_BIT);
//  activitylight_PORT_DDR |= (1 << activitylight_PORT_BIT);
//
//  led_PORT_DDR |= (1 << led_PORT_BIT);
  pump_PORT_DDR |= (1 << pump_PORT_BIT);
		   
//   // direction=input ddr=0
   credit_btn_PORT_DDR &= ~(1 << credit_btn_PORT_BIT);
//  remote_n_PORT_DDR &= ~(1 << remote_n_PORT_BIT);
//  bar_locked_PORT_DDR &= ~(1 << bar_locked_PORT_BIT);
//  bar_unlocked_PORT_DDR &= ~(1 << bar_unlocked_PORT_BIT);
//
//   // input no pullup, out=0
//   rain_int_PORT_OUT &= ~(1 << rain_int_PORT_BIT);
//
//  // define output bit levels
//  trig_OFF;
//  ACTIVITYLIGHT_OFF;
   PUMP_OFF;
   LED_CONFIG;
   LED_OFF;
}


// if I consider T3
// its a 16 bit timer
// can prescale by 1024
// I've made clk_cpu 8MHz, so seems like clkI/0 is 8mhz
// divided by 2^16 and divided by 8 thats a timer overflow of 152.58Hz
// postscaling by 153, get one tick every 1s +/- 1% 
uint8_t     t3_postscaler;
uint8_t     t3_postscaler4;


// PRR1 to 0 to enable t3
void enable_t3_int (void)
{
  //  PRR1 &= ~(0x10);        // power control
                          // clock source
                          // enable
                          // OF int
  TCCR3A = 0;             // no output compare bits, normal operation
  TCCR3B = 2;             // input prescaled 8
  TCCR3C = 0;             // no force output compare
  TIFR3 =  0;             // clear overflow flag
  TIMSK3 = 1;             // timer overflow inten
}



ISR(TIMER3_OVF_vect)
{ /* timer3 overflow */
  /* dividing t3 by 15.25 to get close to 1hz */
  t3_postscaler++;
  if (((t3_postscaler == (T3_POST-1)) && (t3_postscaler4 !=(T3_POST4-1))) ||
      (t3_postscaler == (T3_POST)))
    {
      t3_postscaler4++;
      if (t3_postscaler4 == T3_POST4)
	{
	  t3_postscaler4 = 0;
	}
      t3_postscaler = 0;
      timer = timer + 1;
      if (pump_state == PUMP_STATE_ON)
	{
	  on_timer = on_timer + 1;
	}
    }
  
}

