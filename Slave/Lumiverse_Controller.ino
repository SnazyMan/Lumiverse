/* 
* NOTES:
*  *bpmTuner must be tuned if code is changed, as described in bpmToMicros if the bpm
*    functionality isn't timed correctly.
*
* Versions
* V3: Support for constant dim and bpm mode. Supports low BPM using delayMicrosecond hack.
* V4: raw -> smooth response.
* V5 (4/29/16): Cleaned up code.
*/

/*
Connections to an Arduino:
1. Connect the C terminal of the ZeroCross Tail to digital pin 2 with a 10K ohm pull up to Arduino 5V.
2. Connect the E terminal of the ZeroCross Tail to Arduino Gnd.
3. Connect the PowerSSR Tail +in terminal to digital pin 4 and the -in terminal to Gnd.
*/

#include <TimerOne.h> 
#include <stdio.h>                   
#include <ctype.h>

#define SSR 4                   //SSR output on digital pin 4
#define RX_BUFF_SIZE 16         //Max size of input RX buffer from signal processor
#define BPM_TUNER 484

//***********************GLOBALS***********************\\

/* DEBUG VALUES - REMOVE FOR PRODUCTION */
unsigned int debug_time; //Used to get time taken in bpm loop

/* MODE VARIABLES */
int mode = 0;             //Light function mode (TODO: Raw, Glow, Pulse, Strobe, etc...)
int lastMode = 0;         //Stores last mode used for proper transitions
int value;                //Mode Value (Light intensity for Raw mode, BPS for pulse mode, etc...)
float bps;                //BPS Mode: BPS to blink lights at
unsigned int us;          //microseconds for delaying

/* RX VARIABLES */
char buff[RX_BUFF_SIZE];     //Buffer for data recieved from the signal processor

/* INTERRUPT VARIABLES */
volatile int dimCount = 0;      // Variable to use as a counter for dimming
volatile boolean zc_flag = 0;   // Boolean to store a "switch" to tell us if we have crossed zero
int dim = 0;                    // Dimming level (0-128)  0 = on, 128 = off
int freqStep = 60;              // 60 Hz = 166667us. 166667/2=8333.5 (because zero crossing halfway through)
                // 8333.5/128 (# of steps) = ~65
//*********************END GLOBALS*********************\\

//************************SETUP************************\\

void setup()
{
  Serial.begin(1000000);
  pinMode(SSR, OUTPUT);                    // Set SSR1 pin as output
  attachInterrupt(0, zc_detect, RISING);   // Attach an Interupt to digital pin 2 (interupt 0),
  Timer1.initialize(freqStep);
  Timer1.attachInterrupt(set_dim);
}
//***********************END SETUP**********************\\

//***********************FUNCTIONS**********************\\

/*
* Convert bpm to microseconds
*/
unsigned int bpmToMicros(int value){
  /*us=((us in a sec)*bps/steps per second) - (delay from interrupts)
  Delay from interrupts can be found by setting bps to 1 and subtracting the result 
  from 1000. */
  bps = 1/(value/60.0);                     //value is bpm, so divide by 60 for bps.
  us = (1000000*bps/252)-(BPM_TUNER*bps);   
  return us;
}


/*
* Delay Microseconds hack. Only supports values up to 16383.
*/
void delayHack(unsigned int microseconds){
  int i;
  int loopCount = microseconds/16383;
  int remainder = microseconds%16383;

  for(i=0; i<loopCount; i++) 
  delayMicroseconds(16383);
  delayMicroseconds(remainder);

  return;
}


/*
* Pulse the lights on and off as defined by microSeconds, with a period of 256*microseconds.
*/
void bpmPulse(unsigned int microSeconds){
  int j;
  for(j=2; j<128; j++){
    dim = j;
    delayHack(us);

    //If new mode given, immediateley use it. Otherwise, just change the bpm period.
    if(Serial.available()) {
      mode = getCommand(&value);
      if(mode == 2) us = bpmToMicros(value);
      else {
        lastMode = mode;
        return;
      }   
    }
  }

  for(j=128; j>2; j--){
    dim = j;
    delayHack(us);

    //If new mode given, immediateley use it. Otherwise, just change the bpm period.
    if(Serial.available()) {
      mode = getCommand(&value);
      if(mode == 2) us = bpmToMicros(value);
      else {
        lastMode = mode;
        return;
      }
    }
  }
}


/* 
*  Checks to see if string 'a' starts with string 'b'.
*/
bool StartsWith(const char *a, const char *b)
{
  if(strncmp(a, b, strlen(b)) == 0) return 1;
  return 0;
}

/* 
*  Returns the operating mode, or -1 if none given. Value for mode stored in value pointer arg.
*/
int getCommand(int *value){
  int result = -1;
  if(Serial.available() > 0){
    int i = 0;
    char incomingByte;

    //Get input into buffer
    while(1){
      incomingByte = Serial.read();
      if(incomingByte == '\r' || i == RX_BUFF_SIZE) {
        buff[i] = 0; //Null terminate buff for printing
        break;   // exit the while(1), we're done receiving
      }
      if(incomingByte == -1) continue;  // if no characters are in the buffer read() returns -1
      buff[i] = incomingByte;
      i++;
    }

    //Parse buffer. 
    //NOTE: Poor buffer sanitization; User should never have control.
    if(isdigit(buff[0])){   //Constant Light Mode
      result = (int)strtol(buff, (char **)NULL, 10);
      if(result != 0){
        //Serial.print("C: ");
        Serial.println(result);
        *value = result;
        return 1;
      }
    } else if(StartsWith(buff, "bpm:")){  //BPM Mode
      result = (int)strtol(buff+4, (char **)NULL, 10); //Get values after "bpm:"
      if(result != 0){
        Serial.print("BPM Mode: ");
        Serial.println(result);
        *value = result;
        return 2;
      }
    } else if(StartsWith(buff, "off")){
      Serial.println("Off");
      return 0;
    } else {
      return -1;
    }
  }
  return -1; // No change or invalid input.
}
//*********************END FUNCTIONS*********************\\

//***********************MAIN LOOP***********************\\

void loop()
{

  mode = getCommand(&value); // Get mode and value

  // Handle no change/invalid input. Ensures off stays off.
  if(mode == -1){
    if(lastMode == 0){
      mode = 0;
    } else if(lastMode == 1){
      mode = 1;
    } else if(lastMode == 2){
      mode = 2;
    }
  }

  lastMode = mode; //Save last mode

  //Perform given mode
  switch(mode){
    case 0: //Off.
      dim = 128;
      break;
    case 1: //Constant Light Mode
      // Check for valid input.
      if(value <= 128 && value >= 0){
        dim = value;  //Set dim value if valid
      }
      break;
    case 2: //BPM Pulse Mode
      
      us = bpmToMicros(value);
      Serial.print("us:"); //debug
      Serial.println(us); //debug
      debug_time = millis();
      bpmPulse(us); //Pulse at given microseconds.
      Serial.print("Loop Time: "); //debug
      Serial.println(millis()-debug_time); //debug PRINT TIME TAKE TO COMPLETE LOOP
      break;
    default:
      break;
  }
}
//*********************END MAIN LOOP*********************\\

//**********************INTERRUPTS***********************\\

// Interrupt at every step of the phase, as defined in freqStep
void set_dim() {                     // This function will fire the triac at the proper time
  if(zc_flag == 1) {                 // First check to make sure the zero-cross has happened else do nothing
    if(dimCount>=dim+1) {            // +1 creates a buffer to keep it away from the first/last step to prevent flickering
      delayMicroseconds(100);
      digitalWrite(SSR, HIGH);       // SENDS PULSE. Triac latches on until next zero.
      delayMicroseconds(50);         // Let the pulse latch
      digitalWrite(SSR, LOW);        // set pulse low
      dimCount = 0;                  // Reset the accumulator
      zc_flag = 0;                   // Reset the zero_cross so it may be turned on again at the next zero_cross_detect    
    } else {
      dimCount++;                    // If the dimming value has not been reached, increment the counter
    }                                
  }                                  
}

// Set flag on zero cross interrupt.
void zc_detect() 
{
  zc_flag = 1;
} 
