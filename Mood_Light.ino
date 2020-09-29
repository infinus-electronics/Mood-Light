//#define FASTLED_INTERRUPT_RETRY_COUNT 0

#include <Wire.h>
#include <FastLED.h>


FASTLED_USING_NAMESPACE
#define DATA_PIN    3
//#define CLK_PIN   4
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS    24
CRGB leds[NUM_LEDS];

uint8_t brightness;

uint8_t aIdx = 0; //animation index from 0 to 255

CRGBPalette16 currentPalette;

uint8_t currMode = 0;
uint8_t currR = 255;
uint8_t currG = 255;
uint8_t currB = 255;
bool currSolidHasChanged = false; //its time to update the LEDs
/*
   Mode Definition:
   0: Solid Color
   1: Gradient Animation
   2: Rainbow
   3: *Temporary* Night Light Mode
*/


#define EEPROM_ADDR 0x57
int eepromAddrPtr = 0;

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

*/
void loadPalette(CRGBPalette16& palette, int index) { // pass in the target palette and also which palette to load (from EEPROM)
  int len = constrain(readEEPROM(index), 0 , 8);
  uint8_t *data = new uint8_t[len * 4];

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
  delete[] data;
}


void setup() {
  Serial.begin(9600);



  Wire.begin();
  Wire.setClock(400000); //fast mode 400kHz

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  brightness = readEEPROM(16); //load last configured brightness
  FastLED.setBrightness(brightness);
  currMode = readEEPROM(17);
  currSolidHasChanged = true; //update LEDS at least once on wake


  loadPalette(currentPalette, 1);
  //currentPalette.loadDynamicGradientPalette(test);


  /*
    Wire.beginTransmission(EEPROM_ADDR);
    Wire.write(eepromAddrPtr>>8);
    Wire.write(eepromAddrPtr&0xff);
    //Wire.write(0x00);
    Wire.write(0x44);
    delay(10);
    Wire.endTransmission();
    delay(10);
    eepromAddrPtr = 0;
  */

}

void loop() {
  if (Serial.available() != 0) { // maybe try to read in all available bytes until /n before processing
    //Serial.println("1");
    while (Serial.available() > 0) { //[INCOMPLETE] Serial comms command list. To do: clear buffer as needed
      uint8_t cmd = Serial.read(); // read in command byte;
      if (cmd == 0xdf) { // swap palette
        uint8_t paletteNumber = Serial.read();
        loadPalette(currentPalette, paletteNumber);
        Serial.println(paletteNumber);
        aIdx = 0; //reset animation index
      }
      else if (cmd == 0xbb) { // change master brightness
        brightness = Serial.read(); // brightness has already been delcared
        writeEEPROM(brightness, 16);
        FastLED.setBrightness(brightness);
        Serial.println(brightness);

      }
      else if (cmd == 0xc7) { //change mode
        currMode = Serial.read();
        writeEEPROM(currMode, 17);
        currSolidHasChanged = true;
        Serial.println(currMode);
        aIdx = 0; //reset animation index
      }
      else if (cmd == 0xcc) { //set color [CRITICAL] THIS ROUTINE DOES NOT WORK FOR WHATEVER REASON
        currR = Serial.read();
        currG = Serial.read();
        currB = Serial.read();
        currSolidHasChanged = true;
        Serial.println(currR);
        Serial.println(currG);
        Serial.println(currB);
      }
      else {
        Serial.read(); //clear the buffer if command is unknown
      }

    }
  }

  FastLED.show();


  if (currMode == 0 && currSolidHasChanged == true) { //solid color mode
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = (currR<<16)|(currG<<8)|(currB);
    }
    currSolidHasChanged = false;
  }
  else if (currMode == 1) { //animation mode
    //currSolidHasChanged = false;
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = ColorFromPalette(currentPalette, aIdx);
    }
    //FastLED.show();
    EVERY_N_MILLISECONDS(100) {
      aIdx++;
    }
  }
  else if (currMode == 2) { //rgb rainbow mode
    //currSolidHasChanged = false;
    EVERY_N_MILLISECONDS(20) {
      aIdx++;
    }
    fill_rainbow(leds, NUM_LEDS, aIdx, 8);
  }
  else if (currMode == 3 && currSolidHasChanged == true){
    //fill_solid(leds, NUM_LEDS, CRGB(0xF6, 0xCD, 0x8B)); //generic warm white
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = 0xFF9329;
    }
    currSolidHasChanged = false;
  }


  //FastLED.delay(2);
}
