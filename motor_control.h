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

// this turns on the shaft motor and sets the direction in mode 1 or 2.  Check for stalls!
void turnShaftMotor(int dir){
  Serial.print("Setting shaft motor direction to ");
  Serial.println(dir);
  digitalWrite(ENABLE_PIN,LOW);       // enable the TMC5072
  sendData(0xB4, 0x000);              // disable stallguard to prevent a premature stall
  sendData(0xA0, dir);                // set rampmode to the correct direction
  delay(70);                          // wait for stallguard to be safe
  sendData(0xB4, 0x400);              // make stallguard stop the motor
  shaft_motor_running = true;         // mark that the shaft motor is running
}

// this turns on the track motor and sets the direction in mode 1 or 2.  Check for stalls!
void turnTrackMotor(int dir){
  Serial.print("Setting track motor direction to ");
  Serial.println(dir);
  digitalWrite(ENABLE_PIN,LOW);       // enable the TMC5072
  sendData(0xD4, 0x000);              // disable stallguard to prevent a premature stall
  sendData(0xC0, dir);                // set rampmode to the correct direction
  delay(70);                          // wait for stallguard to be safe
  sendData(0xD4, 0x400);              // make stallguard stop the motor
  track_motor_running = true;         // mark that the track motor is running
}

void stopShaftMotor(){
  sendData(0xA3, 0);          // set VMAX and VSTART to zero, then enable positioning mode
  sendData(0xA7, 0);          //  > doing this stops the motor
  sendData(0xA0, 0);
  while(sendData(0x22, 0)!=0) // wait for the motor to stop (VACTUAL != 0 until stopped)
    delayMicroseconds(10);
  sendData(0xA1, 0);          // target=xactual=0 to keep motor stopped
  sendData(0xAD, 0);
  
  sendData(0xA3, 0x180);      // fix VMAX and VSTART to previous values so motor can run again
  sendData(0xA7, preferences.getLong("velocity_1",100000));
  shaft_motor_running = false;// mark that the shaft motor is stopped
  opt_motors();               // disable the motor driver if possible
}

void stopTrackMotor(){
  sendData(0xC3, 0);          // set VMAX and VSTART to zero, then enable positioning mode
  sendData(0xC7, 0);          //  > doing this stops the motor
  sendData(0xC0, 0);
  while(sendData(0x42, 0)!=0) // wait for the motor to stop (VACTUAL != 0 until stopped)
    delayMicroseconds(10);
  sendData(0xC1, 0);          // target=xactual=0 to keep motor stopped
  sendData(0xCD, 0);
  
  sendData(0xC3, 0x180);      // fix VMAX and VSTART to previous values so motor can run again
  sendData(0xC7, preferences.getLong("velocity_2",100000));
  track_motor_running = false;// mark that the shaft motor is stopped
  opt_motors();               // disable the motor driver if possible
}


// this function makes the shaft motor turn a certain number of steps.  It is a blocking function,
// so a function call will wait until the motor has finished moving.

void shaftMotorTurnSteps(int steps){
  digitalWrite(ENABLE_PIN,LOW);       // enable the TMC5072
  motor1_running = true;
  sendData(0xB4, 0x000);              // disable stallguard to prevent premature stall
  sendData(0xA1, 0);                  // set XACTUAL to zero and set XTARGET
  sendData(0xAD, steps);
  sendData(0xA0, 0);                  // Now we are ready to enable positioning mode
  delay(100);
  sendData(0xB4, 0x400);              // enable stallguard - it's safe now.
  while(sendData(0x35, 0)&0x200==0)   // wait for position_reached flag
    delayMicroseconds(10);            // waiting here gives time to other CPU processes while we wait
  stopShaftMotor();                   // position reached - make sure motor is properly stopped
}


// this function makes the track motor turn a certain number of steps.  It is a blocking function,
// so a function call will wait until the motor has finished moving.

void trackMotorTurnSteps(int steps){
  digitalWrite(ENABLE_PIN,LOW);       // enable the TMC5072
  motor2_running = true;
  sendData(0xD4, 0x000);              // disable stallguard to prevent premature stall
  sendData(0xC1, 0);                  // set XACTUAL to zero and set XTARGET
  sendData(0xCD, steps);
  sendData(0xC0, 0);                  // Now we are ready to enable positioning mode
  delay(100);
  sendData(0xD4, 0x400);              // enable stallguard - it's safe now.
  while(sendData(0x55, 0)&0x200==0)   // wait for position_reached flag
    delayMicroseconds(10);            // waiting here gives time to other CPU processes while we wait
  stopTrackMotor();                   // position reached - make sure motor is properly stopped
}

void trackClose{

}

void trackOpen{

}

void shaftClose{
  
}

void shaftOpen{
  shaftMotorTurnSteps(-5120);  // turn 5120 steps (- reverses direction)
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


//MOVEMENT FUNCTIONS//
//These provide the actual movements and are made up of motorControl functions

void move_close{
  shaftClose(); // We need to close it first in order to find a reference point
  shaftOpen(); // Need to open the shaft to make sure blinds are in exact position to open
  trackClose();
  shaftClose();
}

void move_open{
  shaftClose();
  shaftOpen();
  trackOpen();
}

void move_shaft_close{
  shaftClose();
}

void move_shaft_open{
  shaftClose(); // We need to close it first in order to find a reference point
  shaftOpen();
}

#endif
