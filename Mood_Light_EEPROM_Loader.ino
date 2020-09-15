#include <Wire.h>
#define EEPROM_ADDR 0x57

int idx = 1;
uint8_t test[16] = {
  0, 255, 255, 255,
  63, 0, 255, 0,
  128, 0, 0, 255,
  255, 255, 255, 255
};


void setup() {
  Serial.begin(9600);

  Wire.begin();
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write(idx>>8);
  Wire.write(idx&0xff);
  Wire.write(4);
  delay(10); // chuck in a tWR
  Wire.endTransmission();
  delay(10);

  idx = 32*(idx+1);
  //do a partial/full page write
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write(idx>>8);
  Wire.write(idx&0xff);
  for(int i = 0; i < 16; i++){
    Wire.write(test[i]);
    delay(10);
    
  }
  Wire.endTransmission();
  delay(10);
  idx = 0;
}

void loop() {
  
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write(idx>>8);
  Wire.write(idx&0xff);
  Wire.endTransmission();

  Wire.requestFrom(EEPROM_ADDR,1);
  //delay(10);
  int data = Wire.read();
  Serial.print(data);
  Serial.print(" ");
  Serial.println(idx);
  //Wire.endTransmission();
  idx++;

  delay(500);
  

}
