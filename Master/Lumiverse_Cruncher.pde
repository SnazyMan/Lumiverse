//v3: Removed button/mode code, cleaned code up, added max brightness
#define maxStates 4
#define buffSize 10
#define analogInPin 15
#define responsePot 20
#define decayPot 19
#define brightnessPot 18
#define buffSize 10

int sensorVal = 0;        
int mappedVal = 0;    

int responsePotVal;
int brightnessPotVal;

int absVal;
int i = 0;
int j = 0;
int avgBuff[buffSize] = {0};
int avg = 0;

int decayCount = 0;
int decayVal = 0;
int decayMapped = 0;
int decayPotVal = 0;

void setup() {
    Serial1.begin(1000000);
    pinMode(responsePot, INPUT_ANALOG);
    pinMode(decayPot, INPUT_ANALOG);
    pinMode(brightnessPot, INPUT_ANALOG);
    pinMode(analogInPin, INPUT_ANALOG);
}

//Loops at 0.000382 seconds
void loop() {
  
    brightnessPotVal = analogRead(brightnessPot);  
    brightnessPotVal = map(brightnessPotVal, 0, 4096, 50, 128); //map pot to a max brightness val from 0 to 128
    responsePotVal = analogRead(responsePot);  //Get input sensitivity response value
    responsePotVal = map(responsePotVal, 0, 4096, 115, 2048); //map the full voltage range from 115 to 2048. 115 is the map output range.
    sensorVal = analogRead(analogInPin);  //read in guitar signal
    absVal = abs(sensorVal-2048);         //take abs value (signal centered at 2048)
    mappedVal = map(absVal, 0, responsePotVal, 120, 5); //Map input signal to response sensitivity range
    
    
    //Smooth out signal jitter (moving average of size buffSize)
    avg = 0;
    avgBuff[i] = mappedVal;
    for(j=0; j<buffSize; j++){
      avg += avgBuff[j];
    }
    avg = avg/buffSize;
    i++;
    if(i==buffSize) i = 0;
    
    
    //INPUT DECAY RESPONSE
    //Loops at 0.000382 seconds. 0.5/0.000382=1308.46 (1 sec = 2616). Input val of 120 will take 1 second to decay. 
    //Input val of 60 will take 0.5 seconds to decay.
    decayPotVal = analogRead(decayPot);
    decayPotVal = map(decayPotVal, 0, 4096, 20, 5232); //map input from 0 to 2 seconds. Lights dont respond well when Val < 20 - why?
    // if input is brighter than mapped value, remap (lower=brighter)
    if(avg < decayMapped){
        decayVal = map(avg, 120, 5, 0, decayPotVal);
    }
    if(decayVal > 0) decayVal--; //decay 
    decayMapped = map(decayVal, 0, decayPotVal, 120, 5); //Map if back after subtracting time ticks
    //END INPUT DECAY RESPONSE
    
    
    //max brightness check - lower == brighter
    if(decayMapped < brightnessPotVal) decayMapped = brightnessPotVal;
    
    
    Serial1.print(decayMapped);
    Serial1.write('\r');
    delayMicroseconds(300);
    
    
    /*3.33 kHz - starts getting corrupted on arduino end if you go lower, 
       but this has something to do with printing it back over usb. Max used to be
       2 ms with arduino printing "Constant Mode:xxx". Changed it to "C:" and brought
       it down to 500 us. Then just the number got us 300 us. Could go lower without
       debug statements.
     */
    /*
    //Test max speed using different baud rates
    for(i=0; i < 128; i++){
       //SerialUSB.println(i);
       Serial1.print(i); 
       Serial1.write('\r');
       delayMicroseconds(300); 
              
       //delay(1);
    }
    */
}
