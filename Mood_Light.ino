//#define FASTLED_INTERRUPT_RETRY_COUNT 0

#define WAIT_FOR_SERIAL(x) while(Serial.available()<x)

#include <Wire.h>
#include <FastLED.h>
#include <RTClib.h>

////Timekeeping and alarm related declarations//////////////////////////////////////////
typedef struct alarm {
  uint8_t hour;
  uint8_t minute;
  uint8_t function;
  uint8_t parameter1;
  uint8_t parameter2;
  uint8_t parameter3;
} alarm;


RTC_DS3231 rtc;
uint8_t prevMinute;
alarm alarms[16];
/*
  Array that contains the alarm data,
  first layer has hour values,
  second has minute values,
  third has function to call,
  fourth, fifth and sixth have the specific parameters to that function

  Definitions:
  Mode 0: Solid Color
  First Layer: R value
  Second Layer: G value
  Third Layer: B value

  Mode 1: Gradient Animation
  First Layer: Palette number
  Second Layer: Time between frames
  Third Layer: In ms or s; 0 for ms, 1 for s

  Mode 2: Rainbow
  First Layer: Time between frames in ms

  Mode 3: NULL

  Mode 255: Turn off everything
*/


////LED related declarations/////////////////////////////////////////////////////////
FASTLED_USING_NAMESPACE
#define DATA_PIN    3
//#define CLK_PIN   4
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS    24
CRGB leds[NUM_LEDS];

uint8_t brightness;

uint8_t aIdx = 0; //animation index from 0 to 255
uint8_t timeBetweenFrames = 20; //by default just do a 20 ms or s animation
bool msOrS = false; //true = s, false = ms

unsigned long lastMillis; // to drive the animations

CRGBPalette16 currentPalette;

uint8_t currMode;
uint8_t currR;
uint8_t currG;
uint8_t currB;
bool currSolidHasChanged = false; //its time to update the LEDs
/*
   Mode Definition:
   0: Solid Color
   1: Gradient Animation
   2: Rainbow
   3: *Temporary* Night Light Mode
   255: Blackout
*/

////EEPROM related declarations/////////////////////////////////////////////////////////
#define EEPROM_ADDR 0x57


uint8_t readEEPROM(int address) {
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write(address >> 8);
  Wire.write(address & 0xff);
  Wire.endTransmission();

  Wire.requestFrom(EEPROM_ADDR, 1);
  return (Wire.read());
}

void writeEEPROM(uint8_t _data, int address) {
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write(address >> 8);
  Wire.write(address & 0xff);
  Wire.write(_data);
  delay(10); //chuck in a tWR
  Wire.endTransmission();
  return;
}


/*

  EEPROM MAPPING:

  Words 0 to 15 contain the lengths of palettes 0 to 15 respectively in terms of the number of "stops" in the gradient, with a maximum of 8 stops
  Hence, the number of bytes to be read will be x*4

  The AT24C32 has 32 byte pages

  Words 32 to 63 contain the stop data for gradient palette 0
  Words (n+1)32 to 32(n+1)+31 contain the stop data for gradient palette n

  Word 16 contains the last set brightness
  Word 17 contains the last set mode
  Words 18, 19, and 20 contain the last used RGB solid color value
  Words 20 and 21 contain timeBetweenFrames and msOrS

  Words 64 to 79 contain the hours value for alarms 0 to 15
  Words 80 to 95 contain the corresponding minute value
  Words 96 to 111 contain which function do they call
  Words 112 to 159 contain the parameters for each function (3 bytes to account for the RGB values in solid color mode)

*/
void loadPalette(CRGBPalette16& palette, int index) { // pass in the target palette and also which palette to load (from EEPROM)
  int len = constrain(readEEPROM(index), 0 , 8);
  uint8_t data[len * 4];

  int loc = 32 * (index + 1);
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write(loc >> 8);
  Wire.write(loc & 0xff);
  Wire.endTransmission();

  Wire.requestFrom(EEPROM_ADDR, len * 4);
  for (int j = 0; j < (len * 4); j++) {
    data[j] = (uint8_t)Wire.read();
  }
  palette.loadDynamicGradientPalette(data);

}

void loadAlarms() {
  for (int i = 0; i < 16; i++) { //load in all 16 alarms to RAM
    alarms[i].hour = readEEPROM(64 + i);
    alarms[i].minute = readEEPROM(80 + i);
    alarms[i].function = readEEPROM(96 + i);
    alarms[i].parameter1 = readEEPROM(112 + i);
    alarms[i].parameter2 = readEEPROM(128 + i);
    alarms[i].parameter3 = readEEPROM(144 + i);
  }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
  Serial.begin(9600);



  Wire.begin();
  Wire.setClock(400000); //fast mode 400kHz

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  //load in last configs
  brightness = readEEPROM(16); //load last configured brightness
  FastLED.setBrightness(brightness);
  currMode = readEEPROM(17); // load last configured mode
  currSolidHasChanged = true; //update LEDS at least once on wake
  currR = readEEPROM(18);
  currG = readEEPROM(19);
  currB = readEEPROM(20);
  timeBetweenFrames = readEEPROM(21);
  msOrS = readEEPROM(22);


  loadPalette(currentPalette, 1);


  DateTime now = rtc.now(); // request the current time
  prevMinute = now.minute(); // remember what second it is right now so we know if its time to trigger an alarm
  loadAlarms();


  lastMillis = millis();

}

void loop() {

  
  
  DateTime now = rtc.now();

  if (now.minute() != prevMinute) { //time has changed, check to see if there is an alarm to be triggered
    for (int i = 0; i < 16; i++) { //iterate through all alarms
      if (alarms[i].hour == now.hour() && alarms[i].minute == now.minute()) { //found an alarm that we have to trigger now

        currMode = alarms[i].function;

        if (alarms[i].function == 0) { //activate solid color mode
          currSolidHasChanged = true; //signal that LED color should be updated
          currR = alarms[i].parameter1;
          currG = alarms[i].parameter2;
          currB = alarms[i].parameter3;
        }
        else if (alarms[i].function == 1){ //activate gradient animation mode
          loadPalette(currentPalette, alarms[i].parameter1);
          aIdx = 0;
          timeBetweenFrames = alarms[i].parameter2;
          if (alarms[i].parameter3 == 0){
            msOrS = 0;
          }
          else{
            msOrS = 1;
          }
        }
        else if (alarms[i].function == 2){ //activate rainbow
          aIdx = 0;
          timeBetweenFrames = alarms[i].parameter1;
        }
        else if (alarms[i].function == 3){
          
        }
        else if (alarms[i].function == 255){ //trigger blackout
          currSolidHasChanged = true;
        }
        
        break; //exit the for loop, since there will be no further need to check anything
      }
    }
    prevMinute = now.minute();
  }


  if (Serial.available()) {
    FastLED.show();
    //Serial.println("1");
    while (Serial.available() > 0) { //[INCOMPLETE] Serial comms command list. To do: clear buffer as needed
      uint8_t cmd = Serial.read(); // read in command byte;

      if (cmd == 0xdf) { // swap palette
        WAIT_FOR_SERIAL(1); // wait for the slow 9600 baud crap
        uint8_t paletteNumber = Serial.read();
        loadPalette(currentPalette, paletteNumber);
        Serial.println(paletteNumber);
        aIdx = 0; //reset animation index
      }
      else if (cmd == 0xbb) { // change master brightness
        WAIT_FOR_SERIAL(1);
        brightness = Serial.read(); // brightness has already been delcared
        writeEEPROM(brightness, 16);
        FastLED.setBrightness(brightness);
        Serial.println(brightness);

      }
      else if (cmd == 0xc7) { //change mode
        WAIT_FOR_SERIAL(1);
        currMode = Serial.read();
        writeEEPROM(currMode, 17);
        currSolidHasChanged = true;
        Serial.println(currMode);
        aIdx = 0; //reset animation index
      }
      else if (cmd == 0xcc) { //set color
        WAIT_FOR_SERIAL(3); //Real trap for young players! absolutely came a gutsa here -> maybe blog about this
        currR = Serial.read();
        currG = Serial.read();
        currB = Serial.read();

        writeEEPROM(currR, 18); //commit changes to EEPROM
        writeEEPROM(currG, 19);
        writeEEPROM(currB, 20);

        currSolidHasChanged = true;
        Serial.println(currR);
        Serial.println(currG);
        Serial.println(currB);
      }
      else if (cmd == 0x05) { //print the current time for debug purposes
        Serial.print(now.hour(), DEC);
        Serial.print(':');
        Serial.print(now.minute(), DEC);
        Serial.print(':');
        Serial.print(now.second(), DEC);
        Serial.println();
      }
      else if (cmd == 0x41) { // make changes to time between frames
        WAIT_FOR_SERIAL(2);
        timeBetweenFrames = Serial.read();
        msOrS = Serial.read();
        writeEEPROM(timeBetweenFrames, 21);
        writeEEPROM(msOrS, 22);
        //aIdx = 0;
        Serial.println(timeBetweenFrames);
      }
      else {
        Serial.read(); //clear the buffer if command is unknown
      }

    }
  }

  FastLED.show();

////"State Machine" to run the LEDS/////////////////////////////////////////////////////////////////////////////////
  if (currMode == 0 && currSolidHasChanged == true) { //solid color mode
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i].r = currR;
      leds[i].g = currG;
      leds[i].b = currB;
    }
    currSolidHasChanged = false;
  }
  else if (currMode == 1) { //animation mode
    //currSolidHasChanged = false;
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = ColorFromPalette(currentPalette, aIdx);
    }
    //FastLED.show();
    if (millis() - lastMillis >= timeBetweenFrames*(msOrS*999+1)) { //millis rollover is not a problem since we're working modulo unsigned long https://www.gammon.com.au/millis
      aIdx++;
      lastMillis = millis();
    }
  }
  else if (currMode == 2) { //rgb rainbow mode
    //currSolidHasChanged = false;
    if (millis() - lastMillis >= timeBetweenFrames) { //had to write this code to drive the animations because for some reason EVERY_N_MILLISECONDS does not work with changing variables at runtime
      aIdx++;
      lastMillis = millis();
    }
    fill_rainbow(leds, NUM_LEDS, aIdx, 8);
  }
  else if (currMode == 3 && currSolidHasChanged == true) {
    //fill_solid(leds, NUM_LEDS, CRGB(0xF6, 0xCD, 0x8B)); //generic warm white
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = 0xFFAF29;
    }
    currSolidHasChanged = false;
  }
  else if (currMode == 255 && currSolidHasChanged == true){ // blackout
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i].r = 0;
      leds[i].g = 0;
      leds[i].b = 0;
    }
    currSolidHasChanged = false;
  }


  //FastLED.delay(2);
}
