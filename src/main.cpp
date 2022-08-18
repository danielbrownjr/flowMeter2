/*
RedShiftBio Flow Meter
Initial coding provided by SparkFun Electronics and Sensirion. Coding designed by Jasmine Battikha.

Maintained by Daniel Brown, Embeded Systems Engineer @ RedShiftBio 2022

The RedShiftBio Flow Meter is a sensor that measures the flow of water in from the fraction collector port on the back of the AQS3/Apollo instruments.

Note: This code is designed to work with a MicroMod Teensy board. As such, the standard arduino SdFat library is not used.
You must use the Teensy SdFat library - if you cloned this repo from github, platform IO should be able to handle the dependencies.
If you install the SdFat library from the Platform IO repository, you will run into compilation errors.
*/

#include <Arduino.h>
#include <SdFat.h>
#include <Wire.h>
#include <SerLCD.h>

SerLCD lcd;

byte heart[8] = {
  0b00000,
  0b01010,
  0b11111,
  0b11111,
  0b11111,
  0b01110,
  0b00100,
  0b00000
};

byte smiley[8] = {
  0b00000,
  0b00000,
  0b01010,
  0b00000,
  0b00000,
  0b10001,
  0b01110,
  0b00000
};

byte frownie[8] = {
  0b00000,
  0b00000,
  0b01010,
  0b00000,
  0b00000,
  0b00000,
  0b01110,
  0b10001
};

byte armsDown[8] = {
  0b00100,
  0b01010,
  0b00100,
  0b00100,
  0b01110,
  0b10101,
  0b00100,
  0b01010
};

byte armsUp[8] = {
  0b00100,
  0b01010,
  0b00100,
  0b10101,
  0b01110,
  0b00100,
  0b00100,
  0b01010
};

char buffer[16];

// Flow sensor setup
/*
Flow Sensor Pinout:
1: SDA (Brown Wire)
2: SCL (Red Wire)
3: VDD (Orange Wire)
4: GND (Yellow Wire)
5: Analog Out (Not Connected)
*/

const int ADDRESS = 0x40; // Standard address for Liquid Flow Sensors
const bool VERBOSE_OUTPUT = true; // set to false for less verbose output
// EEPROM Addresses for factor and unit of calibration fields 0,1,2,3,4.
const uint16_t SCALE_FACTOR_ADDRESSES[] = {0x2B6, 0x5B6, 0x8B6, 0xBB6, 0xEB6};
const uint16_t UNIT_ADDRESSES[] =         {0x2B7, 0x5B7, 0x8B7, 0xBB7, 0xEB6};
// Flow Units and their respective codes.
const char    *FLOW_UNIT[] = {"nl/min", "ul/min", "ml/min", "ul/sec", "ml/h"};
const uint16_t FLOW_UNIT_CODES[] = {2115, 2116, 2117, 2100, 2133};
uint16_t scale_factor;
const char *unit;

void setup() {
  int ret;

  uint16_t user_reg;
  uint16_t scale_factor_address;

  uint16_t unit_code;

  byte crc1;
  byte crc2;
  
  Serial.begin(9600);
  Serial.println("Hello World!");
  Wire.begin();
  lcd.begin(Wire);
  

  //Send custom characters to display
  //These are recorded to SerLCD and are remembered even after power is lost
  //There is a maximum of 8 custom characters that can be recorded
  lcd.createChar(0, heart);
  lcd.createChar(1, smiley);
  lcd.createChar(2, frownie);
  lcd.createChar(3, armsDown);
  lcd.createChar(4, armsUp);

  // set the cursor to the top left
  lcd.setBacklight(0xA020F0); //violet
  lcd.setCursor(0, 0);

    do {
    delay(1000); // Error handling for example: wait a second, then try again

    // Soft reset the sensor
    Wire.beginTransmission(ADDRESS);
    Wire.write(0xFE);
    ret = Wire.endTransmission();
    if (ret != 0) {
      Serial.println("Error while sending soft reset command, retrying...");
      continue;
    }
    delay(50); // wait long enough for reset

    // Read the user register to get the active configuration field
    Wire.beginTransmission(ADDRESS);
    Wire.write(0xE3);
    ret = Wire.endTransmission();
    if (ret != 0) {
      Serial.println("Error while setting register read mode");
      continue;
    }

    Wire.requestFrom(ADDRESS, 2);
    if (Wire.available() < 2) {
      Serial.println("Error while reading register settings");
      continue;
    }
    user_reg  = Wire.read() << 8;
    user_reg |= Wire.read();

    // The active configuration field is determined by bit <6:4>
    // of the User Register
    scale_factor_address = SCALE_FACTOR_ADDRESSES[((user_reg & 0x0070) >> 4)];

    // Read scale factor and measurement unit
    Wire.beginTransmission(ADDRESS);
    Wire.write(0xFA); // Set EEPROM read mode
    // Write left aligned 12 bit EEPROM address
    Wire.write(scale_factor_address >> 4);
    Wire.write((scale_factor_address << 12) >> 8);
    ret = Wire.endTransmission();
    if (ret != 0) {
      Serial.println("Error during write EEPROM address");
      continue;
    }

    // Read the scale factor and the adjacent unit
    Wire.requestFrom(ADDRESS, 6);
    if (Wire.available() < 6) {
      Serial.println("Error while reading EEPROM");
      continue;
    }
    scale_factor = Wire.read() << 8;
    scale_factor|= Wire.read();
    crc1         = Wire.read();
    unit_code    = Wire.read() << 8;
    unit_code   |= Wire.read();
    crc2         = Wire.read();

    switch (unit_code) {
     case 2115:
       { unit = FLOW_UNIT[0]; }
       break;
     case 2116:
       { unit = FLOW_UNIT[1]; }
       break;
     case 2117:
       { unit = FLOW_UNIT[2]; }
       break;
     case 2100:
       { unit = FLOW_UNIT[3]; }
       break;
     case 2133:
       { unit = FLOW_UNIT[4]; }
       break;
     default:
       Serial.println("Error: No matching unit code");
       break;
   }

    if (VERBOSE_OUTPUT) {
      Serial.println();
      Serial.println("-----------------------");
      Serial.print("Scale factor: ");
      Serial.println(scale_factor);
      Serial.print("Unit: ");
      Serial.print(unit);
      Serial.print(", code: ");
      Serial.println(unit_code);
      Serial.println("-----------------------");
      Serial.println();
    }

    // Switch to measurement mode
    Wire.beginTransmission(ADDRESS);
    Wire.write(0xF1);
    ret = Wire.endTransmission();
    if (ret != 0) {
      Serial.println("Error during write measurement mode command");
    }
  } while (ret != 0);

  // Print a message to the LCD.
  lcd.print("I ");
  lcd.writeChar(0); // Print the heart character. We have to use writeChar since it's a serial display.
  lcd.print(" SerLCD! ");
  lcd.writeChar(1); // Print smiley
  delay(1000);
}

void loop() {
  uint16_t raw_sensor_value;
  float sensor_reading;
  
  // Displaying flow on the LCD
  lcd.setCursor(2, 1);

  Wire.requestFrom(ADDRESS, 2); // reading 2 bytes ignores the CRC byte
  if (Wire.available() < 2) {
    Serial.println("Error while reading flow measurement");
    // Display error on the LCD
    // move the LCD cursor to the second row
    lcd.setCursor(0, 1);
    lcd.print("Error reading flow");

  } else {
    raw_sensor_value  = Wire.read() << 8; // read the MSB from the sensor
    raw_sensor_value |= Wire.read();      // read the LSB from the sensor
    sensor_reading = ((int16_t) raw_sensor_value) / ((float) scale_factor);

/*     Serial.print("Sensor reading: ");
    Serial.print(sensor_reading);
    Serial.print(" ");
    Serial.println(unit); */
    sprintf(buffer, "%3.2f uL/min\n", sensor_reading);
    
    Serial.print(buffer);

  }
  lcd.print(sensor_reading);
  lcd.print(buffer);

}