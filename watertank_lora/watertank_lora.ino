//    -*- Mode: c++     -*-
// emacs automagically updates the timestamp field on save
// watertank_lora for moteino Time-stamp: "2022-05-13 14:18:42 john";


/* this is a respin of watertank.c
 * it is ported to ardino env, for a 328p, and adds lora radio.
 * ie a moteino.
 *
 * after 4+ years, the watertank controller is playing up.
 * It runs out of credit occasionally. Generally during an evening shower.
 * the old SLA battery was EOL, but a different one didn't help.
 * The pump seems OK, it flows OK and fills in 5 minutes.
 * I guess its possible the motor brushes are suss, or I have a leak,
 * but I want to add start / stop logging to the homenet logger.
 * Which wasn't there when the original waterpump was made.
 * 
 * Knowing whether there is an occasional restart at night, or too frequent
 * fills, or long fill periods occasionally, should be helpfull.
 * Logging battery voltage is bound to help too.
 *
 * at this stage, no algorithm change is seen to be needed.
*/


/* ORIGINAL water tank filler controller comments, with minor edits to cover what I actually did!
 * This is the firmware for a controller to fill a header tank from the farm 
 * main rainwater tanks.
 * The header tank has a couple of reasons for its existance.
 * 1. more pressure. 
 * 2. provide a measure of, and some protection against bad, water leaks. 
 *  By some combination of counting the number of header tank fill cycles, and 
 *  timing the fill time, I can get a measure of the amount of water consumed in a day.
    Or some convenient period.    If the consumption is too high (like a significant leak)
    then the controller refuses to fill more without operator intervention.
    Right now, a credit is consumed every 10s of pump running.
 * So I want to time the pump runtime (it seems to be about 3 minutes to get 
 * to the initial level, then total 10 minutes to fill the header tank.
 * The header tank has 2 level sensors, one at ~20%, one at ~80% full.    
 * 
 * Also want to allocate  credits, say ~ 250l per day. Which hopefully is 
 * sufficient for a heavy useage day.  Credits are allocated daily, but don't accumulate past ~~ 1 days worth.       Time resolution of 1s would be heaps.
 * 1 Credit is consumed every 10 seconds of pump running
 * 
 * Also credits don't get allocated if zero.  You have to do the first one
 * manually, also manually ack it if credits gets to zero. 
 * This is to ensure an unattended serious fault is capped after a day. 
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
 * a red led with a blink code.  
 * or perhaps one short blink, then up to 5 half second blinks showing amount. Then 1.5 seconds off.    
 * Such a blink code would be a pain on a 1 second update loop. So I either have to run the main loop at few hundred ms (which seems practical) or maybe run the different subloops off a timer ISR. 
 */



#include <RHReliableDatagram.h>
#include <RH_RF95.h>

#include <avr/wdt.h> // watchdog
#include <LowPower.h>

#define DOG
#define SLEEP

// sleep this long (ms) after loging a line 
#define LOGSLEEP 100

//*********************************************************************************************
//************ IMPORTANT SETTINGS - YOU MUST CHANGE/CONFIGURE TO FIT YOUR HARDWARE *************
//*********************************************************************************************
const uint8_t NODEID = 0x86;   // watertank sender
const uint8_t RELAY  = 0x81;   // relay on the roof to homenet
const uint8_t LOGGER = 0x02;   // homenet logger

// Moteinos have LEDs on D9
#define LED              9

// digital input from the floatsw. shorted == 0 == empty. Open == 1 == full. 
#define TOP_LIMIT        4 
#define BOTTOM_LIMIT     3  
// the motor
#define PRESSURE_PUMP_N  5
// pushbutton UI input
#define PB               0
// analog IO for measuring battery
#define BATTERY_ADC_CH    7  


// trimmed empirically for correct result. depends on 3v3 reg.
#define BATTERY_GAIN 0.004606


// Singleton instance of the radio driver
RH_RF95 radio;

// Class to manage message delivery and receipt, using the radio declared above
RHReliableDatagram manager(radio, NODEID);


// the RH buffer is ~250, I don't need anything like that much. And I'm very short of ram with the RH code in.
const uint8_t SHORT_RH_RF95_MAX_MESSAGE_LEN=50; 
uint8_t buf[SHORT_RH_RF95_MAX_MESSAGE_LEN];
char bufnum[6];
char strbuf[30];


int16_t credit;
int16_t blink_seq_count;

uint8_t pump_state;

// pump state enum
const uint8_t  PUMP_STATE_LIMITS_BROKEN = 0;
const uint8_t  PUMP_STATE_ON = 1;
const uint8_t  PUMP_STATE_OFF = 2;

uint8_t blink_state;
// blink state enum
const uint8_t  BLINK_FRAME_GAP   = 0;
const uint8_t  BLINK_FRAME_ON    = 1;
const uint8_t  BLINK_SEQ_OFF     = 2;
const uint8_t  BLINK_SEQ_ON_CALC = 3;
const uint8_t  BLINK_SEQ_ON      = 4;
const uint8_t  BLINK_LIMITS_OFF  = 5;
const uint8_t  BLINK_LIMITS_ON   = 6;

uint32_t  ticks;  // timer. counts 120ms ticks of the mainloop.
                  // 65k is enough for about 2 hours of 120ms ticks. So need a uint32

uint8_t on_timer;  // counts up cyclically to ticks_per_credit then resets when on.  
uint8_t blink_timer;
uint8_t blink_seq_on_time;

// how long (in ticks of 120ms) the motor needs to run to consume a credit. //~10l/min
// so a credit is 60/10 or 6 seconds worth, or ~1 litre of water.
// 6 seconds is 48 ticks of 120ms
const uint8_t TICKS_PER_CREDIT = 48; 

// how many credits to allocate each PB press (about a tanks worth) 
const uint16_t CREDIT_PER_PB  = 50;

const int16_t  DAILY_EXTRA_CREDIT = 250 ;
const int16_t  MAX_CREDIT = 250;

// how many loop iterations (~120ms) between blink code frames. 16 is about 2 secs.
const uint8_t blink_frame_gap_time = 16;

// how many loop iterations (~120ms) for blink state start bit
const uint8_t blink_frame_on_time =1;

// how many loop iterations (~120ms) between blink code bits
const uint8_t blink_seq_off_time = 5;

// how many loop iterations (~120ms) for blink code bits
const uint8_t max_blink_seq = 5;

// how to scale the credits to blinks... with max_blink_seq*credit_blink_scale per blink
//  typical credit=250, scale=10; 25 blink periods in groups of 5 (ie max_blink_seq)
const int16_t CREDIT_BLINK_SCALE = 10;

// 86400 seconds per day, and 8 ticks per second
const uint32_t TICKS_PER_DAY = 691200;

bool limits_were_broken;
bool last_credit_pb;
bool this_pb_read;

void setup() {

  // define the used IOs. All named.
  pinMode(TOP_LIMIT, INPUT);
  pinMode(BOTTOM_LIMIT, INPUT);
  pinMode(PB, INPUT_PULLUP);
  pinMode(PRESSURE_PUMP_N, OUTPUT);
  pinMode(LED, OUTPUT);
  digitalWrite(PRESSURE_PUMP_N, HIGH);
  pinMode(BATTERY_ADC_CH, INPUT);




  // set spares to digital output low to minimize power and other funnies
  pinMode(1, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);
  pinMode(A1, OUTPUT);
  pinMode(A2, OUTPUT);
  pinMode(A3, OUTPUT);
  pinMode(A4, OUTPUT);
  pinMode(A5, OUTPUT);
  pinMode(A6, OUTPUT);
  digitalWrite(6,  LOW);
  digitalWrite(7,  LOW);
  digitalWrite(A0, LOW);
  digitalWrite(A1, LOW);
  digitalWrite(A2, LOW);
  digitalWrite(A3, LOW);
  digitalWrite(A4, LOW);
  digitalWrite(A5, LOW);
  digitalWrite(A6, LOW);

  credit=0;
  limits_were_broken = false;
  last_credit_pb = digitalRead(PB);
  on_timer = 0;
  blink_timer = 0;
  blink_state = BLINK_FRAME_GAP;
  pump_state = PUMP_STATE_OFF;
  
  if (!manager.init())
    {
      while (1) {  // radio broken, just blink.
	digitalWrite(LED,  LOW);
        delay (100);
	digitalWrite(LED,  HIGH);
        delay (100);
      }
    }
  radio.setFrequency(915);  //just to be certain.  
  radio.setModemConfig(RH_RF95::Bw31_25Cr48Sf512);  //set for pre-configured long range
  radio.setTxPower(17);  //set for 50mw , +17dbm
  
  Greet();

#ifdef SLEEP
  radio.sleep();
#endif
  

#ifdef DOG
  wdt_enable(WDTO_4S);
  wdt_reset(); // pat the dog;
#endif
}


void loop() {
  // was 100ms, now 120ms. affects a few details later
  LowPower.powerDown(SLEEP_120MS, ADC_OFF, BOD_OFF);
  ticks++;
  
  // pump state machine .. control the pump
  switch  (pump_state)
    {
    case PUMP_STATE_LIMITS_BROKEN:
      {
	//	  if (!((above_header_tank_top) && (below_header_tank_bottom)))
	//       switch is closed when below. Switch closed == 0 input == below. 1 == above.
	
	// recover if no longer broken.
	if (!(digitalRead(TOP_LIMIT)  && !(digitalRead(BOTTOM_LIMIT))))
	  {
	    pump_state = PUMP_STATE_OFF;
	    digitalWrite(PRESSURE_PUMP_N, HIGH);
	    sprintf((char*)strbuf, "limits now OK");
	    Log(strbuf);
	  } 
	break;
      }
      
    case PUMP_STATE_ON:
      {
	on_timer++;
	//	  if (above_header_tank_top)
	if (digitalRead(TOP_LIMIT))
	  {
	    pump_state = PUMP_STATE_OFF;
	    // PUMP_OFF;
	    digitalWrite(PRESSURE_PUMP_N, HIGH);
	    sprintf((char*)strbuf, "stop pump, now full");
	    Log(strbuf);
	    
	    //	      if (below_header_tank_bottom)
	    if (!digitalRead(BOTTOM_LIMIT))
	      {
		pump_state = PUMP_STATE_LIMITS_BROKEN;
		limits_were_broken = true;
		credit = 0;
		sprintf((char*)strbuf, "broken limits");
		Log(strbuf);
	      }
	  }
	else if (on_timer >= TICKS_PER_CREDIT) {
	  on_timer = 0;
	  if (credit > 0)
	    {
	      credit--;
	    }
	  else 
	    {
	      // run out of credit while pumping
	      pump_state = PUMP_STATE_OFF;
	      digitalWrite(PRESSURE_PUMP_N, HIGH);
	      sprintf((char*)strbuf, "stop pump, credit exhausted");
	      Log(strbuf);
	    }
	}	      
	break;
      }

    case PUMP_STATE_OFF:
      {
	//	if (below_header_tank_bottom)
	if (!digitalRead(BOTTOM_LIMIT))
	  {
	    if (credit > 0)
	      {
		pump_state = PUMP_STATE_ON;
		on_timer = 0;
		//	  PUMP_ON;
		digitalWrite(PRESSURE_PUMP_N, LOW);
		sprintf((char*)strbuf, "start pump, empty");
		Log(strbuf);
	      }
	  }
	else
	  { // to be sure to be sure
	    //     PUMP_OFF;
	    digitalWrite(PRESSURE_PUMP_N, HIGH);
	  }
	break;
      }

    default :
      pump_state = PUMP_STATE_OFF;
    }
  
  // UI STATE MACHINE
  // today, Only UI input is the manual credit pushbutton.
  
  // debounce the pb (but not well). Issue credit on posedge. No limit implemented on PB presses.
  this_pb_read = digitalRead(PB);
  if ((last_credit_pb != this_pb_read) &&  this_pb_read)
    {
      credit += CREDIT_PER_PB;
      sprintf((char*)strbuf, "manually add credit");
      Log(strbuf);
    }
  last_credit_pb = this_pb_read;

  
  // BLINKER STATE MACHINE
  // blink_timer is a single byte, so don't worry about ISRs / SEI / CLI
  // the blink code is:
  // a gap then a short frame marker
  // a second short frame marker if there has been a limits problem
  // a series of long blinks, where each long blink is about a header tank fill long.
  // the last long blink can be fractional... eg 4 1/2 long blinks.
  // each long blink represents 50 credits or 500s of runtime

  blink_timer++;
  switch (blink_state)
    {
    case BLINK_FRAME_GAP:
      {
	if (blink_timer >= blink_frame_gap_time)
	  {
	    blink_state = BLINK_FRAME_ON;
	    blink_timer=0;
	    digitalWrite(LED, HIGH);
	  }
	break;
      }

    case BLINK_FRAME_ON:
      {
	if (blink_timer >= blink_frame_on_time)
	  {
	    if (limits_were_broken)
	      {
		blink_state = BLINK_LIMITS_OFF;
	      }
	    else
	      {
		blink_state = BLINK_SEQ_OFF;
	      }
	    blink_timer=0;
	    digitalWrite(LED, LOW);
	    blink_seq_count=credit / CREDIT_BLINK_SCALE;
	  }
	break;
      }
      
    case BLINK_LIMITS_OFF:
      { if (blink_timer == blink_frame_gap_time)
	  { blink_state = BLINK_LIMITS_ON;
	    blink_timer=0;
	    digitalWrite(LED, HIGH);
	  }
	break;
      }
      
    case BLINK_LIMITS_ON:
      {
	if (blink_timer == blink_frame_on_time)
	  {
	    blink_state = BLINK_SEQ_OFF;
	    blink_timer=0;
	    digitalWrite(LED, LOW);
	    blink_seq_count=credit / CREDIT_BLINK_SCALE;
	  }
	break;
      }

    case BLINK_SEQ_OFF:
	{
	  if (blink_seq_count > 0)
	    {
	      if (blink_timer >= blink_seq_off_time)
		{
		  blink_state = BLINK_SEQ_ON_CALC;
		  blink_timer=0;
		  digitalWrite(LED, HIGH);
		}
	    }
	  else
	    {
	      blink_state = BLINK_FRAME_GAP;
	      digitalWrite(LED, LOW);
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
      // note fall through is intentional only here.
      
    case BLINK_SEQ_ON:
      {	
	if (blink_timer >= blink_seq_on_time)
	  {
	    blink_state = BLINK_SEQ_OFF;
	    blink_timer=0;
	    digitalWrite(LED, LOW);
	  }
	break;
      }
    default : blink_state = BLINK_FRAME_GAP;
    }
  
  // AUTO ISSUE CREDIT DAILY.
  // Issue credit up to a maximum of DAILY_CREDIT
  // Don't issue credit if its run down to zero. Require manual
  // intervention in that event.
  if (ticks >= TICKS_PER_DAY)
    {
      ticks = 0;
      if ((credit < MAX_CREDIT) && (credit > 0))
	{
	  credit += DAILY_EXTRA_CREDIT;
	  sprintf((char*)strbuf, "allocate daily credits");
	  Log(strbuf);
	}
    }

  // PAT FIDO IF NEEDED
#ifdef DOG
  wdt_reset(); 
#endif
}


void Greet (void)

{
  sprintf((char*)buf, "%c%cWaterPumpLora 20220512", LOGGER, NODEID);
  sendMsg(RELAY);
  delay(LOGSLEEP);
  sprintf((char*)strbuf, "startup");
  Log(strbuf);
}

// log given string to homenet, with battery voltage and credit appended too.
void Log(char *msgbuf)
{
  dtostrf((BATTERY_GAIN * analogRead(BATTERY_ADC_CH)), 5, 2, bufnum);
  sprintf((char*)buf, "%c%c%c Battery=%sV credit=%d", LOGGER,NODEID, msgbuf, bufnum, credit);  
  sendMsg(RELAY);
  delay(LOGSLEEP);
  radio.sleep();
#ifdef DOG
  wdt_reset(); // pat the dog;
#endif
 
}

 
void sendMsg(uint8_t to)
{
  uint8_t len = strlen((char *) buf);
  manager.sendtoWait(buf, len, to);
}
