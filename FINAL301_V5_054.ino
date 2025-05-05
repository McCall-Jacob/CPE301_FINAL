// Jacob McCall && Andrew Shelton

// timer 1
volatile unsigned char *myTCCR1A = (unsigned char *) 0x80;
volatile unsigned char *myTCCR1B = (unsigned char *) 0x81;
volatile unsigned char *myTCCR1C = (unsigned char *) 0x82;
volatile unsigned char *myTIMSK1 = (unsigned char *) 0x6F;
volatile unsigned int  *myTCNT1  = (unsigned int *) 0x84;
volatile unsigned char *myTIFR1 =  (unsigned char *) 0x36;

//serial print
volatile unsigned char* myUCSR0A = (unsigned char*) 0x00C0;
volatile unsigned char* myUCSR0B = (unsigned char*) 0x00C1;
volatile unsigned char* myUCSR0C = (unsigned char*) 0x00C2;
volatile unsigned int*  myUBRR0  = (unsigned int*)  0x00C4;
volatile unsigned char* myUDR0   = (unsigned char*) 0x00C6;

//pin 8-13, 50-52
volatile unsigned char *portDDRB = (unsigned char *) 0x24;
volatile unsigned char *portB    = (unsigned char *) 0x25;

// pins A0 -A5
volatile unsigned char *portDDRC = (unsigned char *) 0x27;
volatile unsigned char *portC    = (unsigned char *) 0x28;

// pins 0 -7
volatile unsigned char *portDDRD = (unsigned char *) 0x2A;
volatile unsigned char *portD    = (unsigned char *) 0x2B;

// save values
#include <EEPROM.h>

// start interrupt
int interrupt__start_pin = 2;
bool calibrate = false;
bool on = false;
void Start();

// photoresisitor
int inRoomLight[5];
int currentRoomLight;
int lightTriggerValue;
#define LIGHT_AC 0
int light_value;
#define INTENSITY 0.95

// potenitometer
int dial_value;
int AC_dial = 0;
#define DIAL_AC 1

// stepper motor 
#include <Stepper.h>
int stepsPerRevolution = 2456;
Stepper myStepper = Stepper(stepsPerRevolution, 10, 12, 11, 13);
int motorSpeed = 10;
void DispenseRoll();

// led lights red/green
int green_light_pin = 52;
int red_light_pin = 50;
bool replace_tp = false;

// lcd display
#include <LiquidCrystal.h>
const int RS = 9, EN = 8, D4 = 7, D5 = 6, D6 = 5, D7 = 4;
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);
#define USED 0
#define ALL_USED 1
int tpUsed;
int amount = 100;
#define MARKER 2456
#define TALLY 9
#define TALLY_IN 9
int tally;
float inUse;
void Lcd_Display();

// internal clock (independent + battery)
#include <Wire.h>
#include "RTClib.h"
#define TIME 2
RTC_DS1307 rtc;
DateTime last;
void timeStamp();
unsigned long lastLcdUpdate = 0;
const unsigned long lcdUpdateInterval = 60000; // 1 min

// reset after replace tp roll
int interrupt_reset_pin = 3;
#define ONE_ROLL 614
bool out = false;
void tpReset();
bool reset_flag = false;


// setup
void setup() {
  U0init(9600);
  *portDDRB |= (1 << 1) | (1 << 3);
  *portDDRD &= ~((1 << 2) | (1 << 3));
  *portD |= (1 << 2) | (1 << 3);

  attachInterrupt(0, Start, FALLING);
  attachInterrupt(1, tpReset, FALLING);

  myStepper.setSpeed(motorSpeed);
  lcd.begin(16, 2);
  Wire.begin();
  rtc.begin();
  int sec   = EEPROM.read(TIME + 0);
  int min   = EEPROM.read(TIME + 1);
  int hour  = EEPROM.read(TIME + 2);
  int day   = EEPROM.read(TIME + 3);
  int month = EEPROM.read(TIME + 4);
  int year  = EEPROM.read(TIME + 5);
  last = DateTime(year, month, day, hour, min, sec);
  inUse = EEPROM.read(USED);
  EEPROM.get(ALL_USED, tpUsed);
  EEPROM.get(TALLY_IN, tally);
  U0putchar('\n');
  U0print("Last Used On:  "); 
  printTime(last);
}

// loop
void loop() {
  if(on && !replace_tp){
    if(calibrate){
      calibrate = false;
      currentRoomLight = 0;
      for(int i = 0; i < 5; i++){
        light_value = readADC(LIGHT_AC);
        inRoomLight[i] = light_value;
        currentRoomLight += light_value;
        timer1Delay_rep(250);
      }
      currentRoomLight /= 5;
      lightTriggerValue = currentRoomLight * INTENSITY;
    }
    light_value = readADC(LIGHT_AC);
    dial_value = readADC(DIAL_AC);
    timer1Delay_rep(250);
    if(light_value <= lightTriggerValue){
      DispenseRoll();
    }
    Lcd_Display();
  }
  if(tally > 5 && on){
    *portB |= (1 << 1);
    *portB &= ~(1 << 3);
    if(reset_flag){
      U0print("RESET TP ROLL at: ");
      timeStamp();
      reset_flag = false;
    }
  }
  else if(on){
    *portB |= (1 << 3);
    *portB &= ~(1 << 1);
    replace_tp = true;
    U0print("OUT OF TP at: ");
    timeStamp();
  }
  else if(!on){
    lcd.clear();
  }
}

// start interrupt
void Start(){
  U0print("STATE CHANGE at: ");
  if(!on){
    on = true;
    calibrate = true;
    printTime(last);
    U0print("STATE: ON");
    U0putchar('\n');
    return;
  }
  if(on){
    on = false;
    lcd.clear();
    *portB &= ~(1 << 1);
    EEPROM.put(USED, inUse);
    EEPROM.put(ALL_USED, tpUsed);
    printTime(last);
    lcd.clear();
    U0print("STATE: OFF"); 
    U0putchar('\n');
  }
}

// select analog pin for reading input
int readADC(byte chl) {
  ADMUX = (1 << REFS0) | (chl & 0x0F);
  ADCSRA = (1 << ADEN) | (1 << ADSC) | (1 << ADPS2) | (1 << ADPS1);
  while (ADCSRA & (1 << ADSC));
  return ADC;
}

// reset interrupt
void tpReset(){
  inUse = MARKER;
  tally = TALLY;
  EEPROM.put(USED, inUse);
  EEPROM.put(TALLY_IN, tally);
  replace_tp = false;
  reset_flag = true;
}

// lcd display
void Lcd_Display(){
  amount = (tally * 100) / TALLY;
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.write("Paper left: ");
  lcd.setCursor(12,0);
  char nums[4];
  itoa(amount, nums, 10);
  lcd.write(nums);
  lcd.setCursor(15, 0);
  lcd.write("%");
  lcd.setCursor(0,1);
  lcd.write("Total (ft): ");
  lcd.setCursor(12,1);
  int ftUsed = tpUsed / 1842;
  itoa(ftUsed, nums, 10);
  lcd.write(nums); //nums
}

// timer to mimic a delay using the 16Mh crystal
void timer1Delay_rep(unsigned int ms) {
  unsigned long ticks = ((unsigned long) ms * 16000000) / 64 / 1;  // prescaler
  if (ticks > 65535) ticks = 65535; // limit to timer size
  *myTCCR1A = 0x00;
  *myTCCR1B = 0x00;
  *myTCNT1 = 65536 - ticks;
  *myTIFR1 |= (1 << 0);
  *myTCCR1B = 0x03;
  while (!(*myTIFR1 & (1 << 0)));
  *myTCCR1B = 0x00;
}

// calculate baud rate
void U0init(unsigned long baud) {
  unsigned long FCPU = 16000000;
  unsigned int tbaud = (FCPU / 16 / baud - 1);
  *myUCSR0A = 0x20;
  *myUCSR0B = 0x18; 
  *myUCSR0C = 0x06;     
  *myUBRR0  = tbaud;
}

// send to monitor a char
void U0putchar(unsigned char U0pdata)
{
  while(!(*myUCSR0A & (1 << UDRE0)));
  *myUDR0 = U0pdata;
}

// loop to make string into one char
void U0print(const char* str) {
  while (*str) {
    U0putchar(*str++);
  }
}

// make into into string
void U0printInt(int num) {
  char nums[8];
  itoa(num, nums, 10);
  U0print(nums);
}

// get the current time from clock
void timeStamp() {
  DateTime now = rtc.now();
  EEPROM.write(TIME + 0, now.second());  
  EEPROM.write(TIME + 1, now.minute());
  EEPROM.write(TIME + 2, now.hour());
  EEPROM.write(TIME + 3, now.day());
  EEPROM.write(TIME + 4, now.month());
  EEPROM.write(TIME + 5, now.year() - 2000); // - 2000
  last = now;
  printTime(now);
}

// print to serial monitor the time
void printTime(DateTime now){
  U0printInt(now.year());
  U0print("/");
  U0printInt(now.month());
  U0print("/");
  U0printInt(now.day());
  U0print(" ");
  U0printInt(now.hour());
  U0print(":");
  U0printInt(now.minute());
  U0print(":");
  U0printInt(now.second());
  U0putchar('\n');
}

// move motor to make tp roll out
void DispenseRoll(){
  U0print("Stepper Moved at: "); 
  timeStamp();
  stepsPerRevolution = ONE_ROLL + (dial_value * 1.806);
  inUse -= stepsPerRevolution;
  if(inUse < 0){
    tally--;
    inUse = MARKER + inUse;
  }
  tpUsed += stepsPerRevolution;
  myStepper.setSpeed(motorSpeed);
  myStepper.step(stepsPerRevolution);
  *portB &= ~((1 << 4) | (1 << 5) | (1 << 6) | (1 << 7));
  EEPROM.put(USED, inUse);
  EEPROM.put(ALL_USED, tpUsed);
}
