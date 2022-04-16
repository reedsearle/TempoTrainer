#include <Arduino.h>
/*
 * Project      TempoTrainer
 * Description: Visual tempo training device for musicians
 * Author:      Reed Searle
 * Date:        31 March 2022
 */

#include "Adafruit_neopixel.h"
#include "colors.h"
#include "Encoder.h"
#include "TM1637.h"
#include <math.h>

const int   ENCRED        = 8;      // Encoder Red LED pin
const int   ENCGREEN      = 9;      // Encoder Green LED pin
const int   ENCBUTTONPIN  = 10;      // Button pin
const int   ENCAPIN       = 11;      //  Encoder B pin
const int   ENCBPIN       = 12;       //  Encoder A pin
const int   CLKPIN        = 13;       //  7-Segment Display clock out
const int   DIOPIN        = 14;       //  7-Segment Display data out
const int   VOXPIN        = 15;       //  VOX input
const int   PIXELPIN      = 16;       //  Neo Pixel output on digital pin D6
const int   SPEAKERPIN    = 23;       //  Speaker on D1
const int   NOTE_A4       = 440;      //  A4 is 440 Hz
const int   PIXELNUM      = 60;       //  60 pixels in neopixel ring
const int   BEATPERIOD    = 1000000;  //  1Hz metronome
const float BEATCONVERT   = 1000000 * 60.0;//  Convert BPM to beatPeriod
const int   LIGHTSTAYON   = 15;       //  number of pixels that button light will stay on
const int   TOCKLEN       = 5000;     //  Time in uS that TOCK will sound

int   i;
int   pixelFlashAddr;  //  address of the pixel to be lit on a rotating basis
int   buttonFlashAddr; //  address of the location of the button press
int   buttonCancel;    //  Address of the pixel to stop showing where the button was pressed
int   BPM;             //  Metronome cadence in beats per minute
int  *BPMPointer;      //  Pointer to BPM for function call
int   beatPeriod;      //  Metronome cadence in uS per beat
float duration;        //  length of time spent on individual pixel
bool  tockFlag;        //  indication that TOCK needs to sound
bool  voxIncoming;     //  Flag for incoming signal
bool  buttonPress;     // variable  for the value of the button pin.  This is ACTIVE LOW
bool  buttonPressed;   // variable to determine if button still pressed after action taken
bool  LED_ON;          // Indicates red/green encoder LED is lit.  HIGH means green LED is lit, Red LED is off
float toneOut;

//  Timing Variables
int intStartTime;
int intDeltaTime;
int LEDTimer;
int      beatTime;    //  Beat timer
int      tockTime;    //  TOCK timer for length of time to sound tone
uint32_t intEndTime;  //  Interrupt timeout variable
uint32_t intTimeOut;  //  Interrupt time out to remove multiple hits on one cycle


// CONSTRUCTORS
Adafruit_NeoPixel pixelRing(PIXELNUM, PIXELPIN, NEO_GRB + NEO_KHZ800);
Encoder           encOne(ENCAPIN, ENCBPIN);
TM1637            LEDSegOne(CLKPIN, DIOPIN);

void voxInt();
bool encoderTick(int *encPass);

void setup() {
  // Set up 7-Segment display
  LEDSegOne.clearDisplay();
  LEDSegOne.set(7);

  //  Set up NeoPixel Ring
  pixelRing.begin();
  pixelRing.show();

  // Set up encoder buttn and LEDs
  pinMode(ENCBUTTONPIN, INPUT_PULLUP); // setting button pin to input with a pull up
  pinMode(ENCRED, OUTPUT);          // setting encoder Red LED pin as output
  pinMode(ENCGREEN, OUTPUT);        // setting encoder Green LED pin as output
  

  // Set up VOX input
  pinMode(VOXPIN, INPUT);
  attachInterrupt(VOXPIN, voxInt, FALLING);

  // Set up tock speaker
  pinMode(SPEAKERPIN, OUTPUT);

//  Serial.begin(9600);
  
  beatTime        = micros();  // Initialize the beat timer
  intEndTime      = micros();  // Initialize the interrupt timeout
  pixelFlashAddr  = 0;         //  Start the rotating pixels on address 0
  buttonFlashAddr = PIXELNUM;  //  Set to value that wont display
  tockFlag        = false;     //  Start off with tockFlag off
  BPM             = 60;        //  Initial metronome cadence
  encOne.write(BPM);           //  Initialize the encoder
  LEDSegOne.displayNum(BPM);   //  First print to display
  LED_ON          = true;      //  Board starts with encoder green LED on
}



void loop() {
  buttonPress = digitalRead(ENCBUTTONPIN);  //  get initial value of the button (better be high);
  digitalWrite(ENCRED, !LED_ON);            // display Red LED
  digitalWrite(ENCGREEN, LED_ON);           // display Green LED
  if (!buttonPress && !buttonPressed) {
    LED_ON   = !LED_ON;                      // swap status of  LEDs
    buttonPressed = 1;                       // Indicate that button has been pressed already 
  }
  else if (buttonPress) {
        buttonPressed = 0;                   // Indicate that button has not been pressed  
  }

   if(encoderTick(&BPM)) {     //  Check if encoder has moved
    LEDSegOne.displayNum(BPM); //  Display the BPM on the 7-Seg display
   }
  beatPeriod = BEATCONVERT / BPM;                //  Create the beat period for moving the pixel
  duration   = beatPeriod / PIXELNUM;            //  Create the duration for individual pixels
  intTimeOut = beatPeriod/3;                     //  Create the timeoput before another sound may be sensed
 // intTimeOut = duration   * LIGHTSTAYON;

  if(LED_ON) {                                   //  LED is green so run the ring
  if (micros() - beatTime > duration) {
    if (buttonFlashAddr < PIXELNUM - LIGHTSTAYON) {       //  Button was in section with more that LIGHTSTAYON left in pixels
      buttonCancel = buttonFlashAddr + LIGHTSTAYON;
    } else if (buttonFlashAddr >= PIXELNUM - LIGHTSTAYON) {    //  Button was in last section of PIXELNUM less that LIGHTSTAYON
      buttonCancel = LIGHTSTAYON - (PIXELNUM - buttonFlashAddr);
    }

    if(pixelFlashAddr == buttonCancel) {
      buttonFlashAddr = PIXELNUM;    //  Reset button address
    }

    for(i=0; i<PIXELNUM; i++){
       if(i == pixelFlashAddr && pixelFlashAddr == 0 && buttonFlashAddr == 0) {                   //  We are at first pixel on ring AND button is pressed
         pixelRing.setPixelColor(i, green);                                                       //  display bright green
         tockFlag = true;                                                                         // Indicate that the TOCK needs to be sounded
         tockTime = micros();                                                                    //  Start the 5mS TOCK timer
       } else if(i == pixelFlashAddr && pixelFlashAddr == 0 && buttonFlashAddr != 0) {            //  We are at first pixel on ring sand button NOT pressed
         pixelRing.setPixelColor(i, cyan);                                                        //  display bright cyan
         tockFlag = true;                                                                         // Indicate that the TOCK needs to be sounded
         tockTime = micros();                                                                    //  Start the 5mS TOCK timer
       } else if(i == pixelFlashAddr && pixelFlashAddr != 0) {                                    //  we are at any other pixel so flash dim cyan
         pixelRing.setPixelColor(i, cyan & 0x18);                                                 //  Display dim cyan
       } else if(i == buttonFlashAddr && buttonFlashAddr < PIXELNUM/2 && buttonFlashAddr == 0) {  //  we are where the button was pressed
         pixelRing.setPixelColor(i, green);                                                       //  Display dim red
       } else if(i == buttonFlashAddr && buttonFlashAddr < PIXELNUM/2 && buttonFlashAddr != 0) {  //  we are where the button was pressed
         pixelRing.setPixelColor(i, red);                                                         //  Display dim red
       } else if(i == buttonFlashAddr && buttonFlashAddr > PIXELNUM/2) {                          //  we are where the button was pressed
         pixelRing.setPixelColor(i, blue);                                                        //  Display dim blue
       } else {                                                                                   //  We are not at a beat pixel 
         pixelRing.setPixelColor(i, 0, 0, 0);                                                     //  display black
       } 
    }

    pixelRing.show();                                   //  Display all pixels
    pixelFlashAddr = (pixelFlashAddr + 1) % PIXELNUM;   //  Increment the Pixel to be displayed 
    beatTime = micros();                                //  Reset the beat timer
  } //  end if timer
  
  }


  if(tockFlag) {                           //  Need a TOCK to signal beat
    if(micros() - tockTime < TOCKLEN) {    //  Check for TOCK length
      tone(SPEAKERPIN, 1000);              //  1KHz tone for 5000 uS
    } else {                               //  TOCK length exceeds 5000uS
      noTone(SPEAKERPIN);                  //  Stop tone
      tockFlag = false;                    //  Indicate that the TOCK is completed
    }
  } else {                                 //  No TOCK required
      noTone(SPEAKERPIN);                  //  Stop tone
  }

}

void voxInt() {
  if(micros() - intEndTime > intTimeOut){
    buttonFlashAddr = pixelFlashAddr;  // Set button addres with current address
    intEndTime = micros();
  }
}

bool encoderTick(int *encPass) {
  const int  ENCMIN    = 0;
  const int  ENCMAX    = 300;
  static int encPosOld = 60;
  int encPos;

      encPos = encOne.read();
      if(encPos != encPosOld) {
        if (encPos < ENCMIN) {                    // check if encoder has gone below minimum value
          encPos = ENCMIN;                        // Set position to minimum value
          encOne.write(ENCMIN);                   // force encoder to minimum value 
        }
       else if (encPos >= ENCMAX) {
          encPos = ENCMAX;                         // Set position to minimum value
          encOne.write(ENCMAX);                    // force encoder to minimum value 
        }
        *encPass = encPos;                        //  Pass encoder value to encoder pointer
        encPosOld = encPos;                         //  update encoder check value
        return true;
      }

    return false;
}
