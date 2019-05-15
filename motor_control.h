#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <Arduino.h>
#include <SPI.h>
#include <Preferences.h>
#include "pins.h"

#ifndef DEBUG_STREAM
#define DEBUG_STREAM Serial
#endif

#define DIR_OPEN 1
#define DIR_CLOSE 2
#define TRACK_OPEN 1
#define TRACK_CLOSE 2
#define SHAFT_OPEN 3
#define SHAFT_CLOSE 4
#define COOLCONF_DEFAULT 0

//#define TOTAL_STEPS 1500000

extern Preferences preferences;

unsigned long sendData(unsigned long address, unsigned long datagram);

void stopTrackMotor(); // track motor is motor two
void stopShaftMotor(); // shaft motor is motor one


void setup_motors(){
  // put your setup code here, to run once:
  pinMode(chipCS,OUTPUT);
  pinMode(CLOCKOUT,OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(chipCS,HIGH);
  //digitalWrite(ENABLE_PIN,HIGH);
  digitalWrite(ENABLE_PIN,LOW);
  
  /*  Dylan Brophy from Upwork, code to create 16 MHz clock output
  *  
  *  This code will print the frequency output when the program starts
  *  so that it is easy to verify the output frequency is correct.
  *  
  *  This code finds the board clock speed using compiler macros; no need
  *  to specify 80 or 40 MHz.
  */

  SPI.setBitOrder(MSBFIRST);
  SPI.setClockDivider(SPI_CLOCK_DIV16);
  SPI.setDataMode(SPI_MODE3);
  SPI.begin(22,23,19,21); // SCLK, MISO, MOSI, SS
  sendData(0xB4, 0x400);          // make stallguard stop the motor
  sendData(0xD4, 0x400);          // make stallguard stop the motor
  
  sendData(0x80, 0x00000300);     // GCONF
  
  //2-phase configuration Motor 1
  sendData(0xEC, 0x01010134);     // CHOPCONF_M1
  sendData(0xB0, 0x00001600);     // IHOLD_IRUN_M1
  
  //2-phase configuration Motor 2
  
  sendData(0xFC, 0x01010134);     // CHOPCONF_M2 // My configuration
  sendData(0xD0, 0x00001600);     // IHOLD_IRUN_M2
  
  //Standard values for speed and acceleration
  int q=preferences.getInt("stallguard_1", -9);
  DEBUG_STREAM.print("Stall1 value: ");
  DEBUG_STREAM.println(q);
  q&=0x7F;
  q=q<<16;
  sendData(0xED, COOLCONF_DEFAULT|q);     // STALLGUARD_M1
  q=preferences.getInt("stallguard_2", -9);
  DEBUG_STREAM.print("Stall2 value: ");
  DEBUG_STREAM.println(q);
  q&=0x7F;
  q=q<<16;
  sendData(0xFD, COOLCONF_DEFAULT|q);     // STALLGUARD_M2
  
  //DEBUG_STREAM.print("Acceleration value: ");
  //DEBUG_STREAM.println(q);
  sendData(0xA6, preferences.getLong("accel_1",0x96));     // AMAX_M1
  sendData(0xC6, preferences.getLong("accel_2",0x96));     // AMAX_M2
  
  //DEBUG_STREAM.print("Velocity value: ");
  //DEBUG_STREAM.println(q);
  
  sendData(0xA7, preferences.getLong("velocity_1",100000));          // VMAX_M1
  sendData(0xC7, preferences.getLong("velocity_2",100000));          // VMAX_M2

  sendData(0xA3, 0x00000180);     // writing value 0x00000000 = 0 = 0.0 to address 15 = 0x23(VSTART)
  sendData(0xA4, 0x00001000);     // writing value 0x0000012C = 300 = 0.0 to address 16 = 0x24(A1)
  sendData(0xA5, 0x00020000);     // writing value 0x0000C350 = 50000 = 0.0 to address 17 = 0x25(V1)
  sendData(0xA8, 0x00001000);     // writing value 0x000007D0 = 2000 = 0.0 to address 20 = 0x28(DMAX)
  sendData(0xAA, 0x00001000);     // writing value 0x00000BB8 = 3000 = 0.0 to address 21 = 0x2A(D1)
  sendData(0xAB, 0x0000000A);     // writing value 0x0000000A = 10 = 0.0 to address 22 = 0x2B(VSTOP)
  
  sendData(0xC3, 0x00000180);     // writing value 0x00000000 = 0 = 0.0 to address 15 = 0x23(VSTART)
  sendData(0xC4, 0x00001000);     // writing value 0x0000012C = 300 = 0.0 to address 16 = 0x24(A1)
  sendData(0xC5, 0x00020000);     // writing value 0x0000C350 = 50000 = 0.0 to address 17 = 0x25(V1)
  sendData(0xC8, 0x00001000);     // writing value 0x000007D0 = 2000 = 0.0 to address 20 = 0x28(DMAX)
  sendData(0xCA, 0x00001000);     // writing value 0x00000BB8 = 3000 = 0.0 to address 21 = 0x2A(D1)
  sendData(0xCB, 0x0000000A);     // writing value 0x0000000A = 10 = 0.0 to address 22 = 0x2B(VSTOP)
  stopM1();
  stopM2();
}

unsigned long sendData(unsigned long address, unsigned long datagram){
  //TMC5130 takes 40 bit data: 8 address and 32 data
  
  delay(10);
  uint8_t stat;
  unsigned long i_datagram=0;
  
  digitalWrite(chipCS,LOW);
  delayMicroseconds(10);
  
  stat = SPI.transfer(address);
  
  i_datagram |= SPI.transfer((datagram >> 24) & 0xff);
  i_datagram <<= 8;
  i_datagram |= SPI.transfer((datagram >> 16) & 0xff);
  i_datagram <<= 8;
  i_datagram |= SPI.transfer((datagram >> 8) & 0xff);
  i_datagram <<= 8;
  i_datagram |= SPI.transfer((datagram) & 0xff);
  digitalWrite(chipCS,HIGH);
  return i_datagram;
}


// these variables keep track of which motors are running
bool shaft_motor_running = false;
bool track_motor_running = false;

// this function disables the TMC5072 if both motors are off.
//  Under no circumstance does it enable the driver, this is done elsewhere!
void opt_motors(){
  if(!(track_motor_running||shaft_motor_running))digitalWrite(ENABLE_PIN,HIGH);
}

void setM1dir(int dir){
  Serial.print("Setting motor 1 motion direction to ");
  Serial.println(dir);
  digitalWrite(ENABLE_PIN,LOW);
  sendData(0xB4, 0x000);              // reset stallguard
  sendData(0xA0, dir);                // RAMPMODE_M1
  delay(70);
  sendData(0xB4, 0x400);              // make stallguard stop the motor
  stall_timer=millis()+6000;
  one_stall=false;
  motor1_running = true;
}
void setM2dir(int dir){
  Serial.print("Setting motor 2 motion direction to ");
  Serial.println(dir);
  digitalWrite(ENABLE_PIN,LOW);
  sendData(0xD4, 0x000);              // reset stallguard
  sendData(0xC0, dir);                // RAMPMODE_M1
  delay(70);
  sendData(0xD4, 0x400);              // make stallguard stop the motor
  stall_timer=millis()+6000;
  two_stall=false;
  motor2_running = true;
}

void stopShaftMotor(){
  sendData(0xA3, 0);
  sendData(0xA7, 0);
  sendData(0xA0, 0);
  while(sendData(0x22, 0)!=0)delayMicroseconds(10);
  sendData(0xA1, 0); // target=xactual=0
  sendData(0xAD, 0);
  
  sendData(0xA3, 0x180);
  sendData(0xA7, preferences.getLong("velocity_1",100000));
  motor1_running = false;
  opt_motors();
}

void stopTrackMotor(){
  sendData(0xC3, 0);
  sendData(0xC7, 0);
  sendData(0xC0, 0);
  while(sendData(0x42, 0)!=0)delayMicroseconds(10);
  sendData(0xC1, 0); // target=xactual=0
  sendData(0xCD, 0);
  
  sendData(0xC3, 0x180);
  sendData(0xC7, preferences.getLong("velocity_2",100000));
  motor2_running = false;
  opt_motors();
}
void turnStepsM1(int steps){
  digitalWrite(ENABLE_PIN,LOW);
  motor1_running = true;
  sendData(0xB4, 0x000);              // reset stallguard
  sendData(0xA1, 0);
  sendData(0xAD, -steps);
  delay(1);
  sendData(0xA0, 0);
  delay(100);
  sendData(0xB4, 0x400);              // reset stallguard
  while(sendData(0x35, 0)&0x200==0)delayMicroseconds(10);
  stopM1();
}
void turnStepsM2(int steps){
  digitalWrite(ENABLE_PIN,LOW);
  motor2_running = true;
  sendData(0xD4, 0x000);              // reset stallguard
  sendData(0xC1, 0);
  sendData(0xCD, -steps);
  delay(1);
  sendData(0xC0, 0);
  delay(100);
  sendData(0xD4, 0x400);              // reset stallguard
  while(sendData(0x55, 0)&0x200==0)delayMicroseconds(10);
  stopM2();
}

void stall_turn_steps(int dir, int steps){
  setM1dir(dir);
  
  waitStallM1(8000);
  turnStepsM1(steps*(-dir+2));
}
void waitStallM1(long timeout){
  timeout+=millis();
  do{
    unsigned long m1_raw=sendData(0x6F,0);
    bool stall = (((m1_raw>>24)&1)==1);
    
    if(stall){
      break;
    }
  }while(millis()<timeout);
  stopM1();
}
void waitStallM2(long timeout){
  timeout+=millis();
  do{
    unsigned long m2_raw=sendData(0x7F,0);
    bool stall = (((m2_raw>>24)&1)==1);
    
    if(stall){
      break;
    }
  }while(millis()<timeout);
  stopM2();
}
void delayStallM1(long timeout){
  timeout+=millis();
  do{
    unsigned long m1_raw=sendData(0x6F,0);
    bool stall = (((m1_raw>>24)&1)==1);
    
    if(stall){
      stopM1();
    }
  }while(millis()<timeout);
}
void delayStallM2(long timeout){
  timeout+=millis();
  do{
    unsigned long m2_raw=sendData(0x7F,0);
    bool stall = (((m2_raw>>24)&1)==1);
    
    if(stall){
      stopM2();
    }
  }while(millis()<timeout);
}

#endif
