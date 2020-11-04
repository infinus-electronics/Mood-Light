#include <Wire.h>
#define EEPROM_ADDR 0x57
#define WAIT_FOR_SERIAL(x) while(Serial.available()<x)

void writeEEPROM(uint8_t _data, int address) {
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write(address >> 8);
  Wire.write(address & 0xff);
  Wire.write(_data);
  delay(10); //chuck in a tWR
  Wire.endTransmission();
  return;
}

uint8_t readEEPROM(int address) {
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write(address >> 8);
  Wire.write(address & 0xff);
  Wire.endTransmission();

  Wire.requestFrom(EEPROM_ADDR, 1);
  return (Wire.read());
}



void setup() {
  Serial.begin(9600);

  Wire.begin();
}

void loop() {
  
  while(Serial.available() > 0){
    int addr = Serial.read();
    Serial.println("Insert Data");
    WAIT_FOR_SERIAL(1); //wait for payload
    uint8_t payload = Serial.read();
    Serial.println(payload);
    writeEEPROM(payload, addr);
    delay(100);
    uint8_t readback = readEEPROM(addr);
    Serial.println(readback);
    //wait for input
  }
  

}
