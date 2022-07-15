/*
 power-monitor

 Sketch to test power monitor code on a breadboard. Arduino acts as Raspberry Pi

 Type 'mr=hi' or 'mr=lo' in serial monitor to set the MCU_RUNNING pin hi or lo

 Circuit:
 MOSI: pin 11
 MISO: pin 12
 SCK: pin 13
 Chip Sel: pin 10
 MCU_RUNNING: pin 9
 SHUTDOWN: pin 8
 ENABLE: pin 6
 */

#include <SPI.h>

const int chipSelectPin = 10;
const int mcuRunningPin = 9;
const int shutdownPin = 8;
const int enablePin = 6;

char input_buffer[20];
bool sd_signal;
unsigned long t;
bool shutdown;

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(2000);
  Serial.println("\nPower Monitor SPI tryout");

  // start the SPI library:
  SPI.begin();
  pinMode(chipSelectPin, OUTPUT);
  digitalWrite(chipSelectPin, HIGH);

  // setup board pins
  pinMode(mcuRunningPin, OUTPUT);
  digitalWrite(mcuRunningPin, LOW);
  pinMode(shutdownPin, INPUT);
  pinMode(enablePin, INPUT);

  // flags
  shutdown = true;
  sd_signal = false;
}

void loop() {

  // if enable pin is high, we can try spi
  if (digitalRead(enablePin)) {
    if (shutdown) {
      Serial.println("Pi power on..");
      shutdown = false;
      // get firmware version
      writeRegister(4, 0, 0);
      delay(10);
      // get can_hardware setting
      writeRegister(6, 0, 0);
      delay(10);
      // set channel 0, 1
      writeRegister(1, _BV(0)|_BV(1), 0x0);
      delay(10);
      sd_signal = false;
    }
    String str = Serial.readString();
    checkInput(str);
    if (!sd_signal) {
      writeRegister(0x2, 0x00, 0x00);
      delay(10);
      for (int i=0; i<2; i++)
      {
        // get channel(s)
        writeRegister(0x10+i, 0x00, 0x00);
        delay(10);
      }
      if (digitalRead(shutdownPin) == HIGH) {
        Serial.println("**** SHUTDOWN DETECTED ****");
        sd_signal = true;
        t = millis();
      }
    } else {
      if (digitalRead(shutdownPin) == HIGH) {
        unsigned long et = millis() - t;
        Serial.print("ET:"); Serial.println(et);
        if (et > 600) {
          Serial.println("**** SHUTDOWN INITIATED ****");
          delay(2000);
          digitalWrite(mcuRunningPin, LOW);
          sd_signal = false;
        }
      } else {
        Serial.println("**** SHUTDOWN PULSE too short ****");
        sd_signal = false;
      }
    }
    Serial.print("enable:");
    if (digitalRead(enablePin) == HIGH)
      Serial.println("hi");
    else
      Serial.println("lo");
    Serial.print("shutdown:");
    if (digitalRead(shutdownPin) == HIGH)
      Serial.println("hi");
    else
      Serial.println("lo");
    Serial.print("mcu_running:");
    if (digitalRead(mcuRunningPin) == HIGH)
      Serial.println("hi");
    else
      Serial.println("lo");
  } else {
    // enable pin not hi
    if (!shutdown) {
      Serial.println("Pi power off..");
    }
    shutdown = true;
  }
}

void checkInput(String s)
{
  if (s.length() == 0)
    return;
  if (s == "mr=hi\n") {
    digitalWrite(mcuRunningPin, HIGH);
    Serial.println("Setting MR Hi");
  }
  else if (s == "mr=lo\n") {
    digitalWrite(mcuRunningPin, LOW);
    Serial.println("Setting MR Lo");
  }
}

void writeRegister(int addr, int byte1, int byte2) {

  SPI.beginTransaction(SPISettings(32000, MSBFIRST, SPI_MODE0));

  // take the chip select low to select the device:
  digitalWrite(chipSelectPin, LOW);
  //for(int i=0; i<200; i++);
  _delay_us(50);

  SPI.transfer(addr); //Send register location
  _delay_us(40);
  unsigned int result = 0;
  result = SPI.transfer(byte1);  //Send value to record into register
  _delay_us(20);
  result += (SPI.transfer(byte2) << 8);  //Send value to record into register

  _delay_us(10);

  // take the chip select high to de-select:
  digitalWrite(chipSelectPin, HIGH);
  SPI.endTransaction();

  Serial.print("ADDR:[0x");
  Serial.print(addr, HEX);
  Serial.print("]:0x");
  Serial.println(result, HEX);
}
