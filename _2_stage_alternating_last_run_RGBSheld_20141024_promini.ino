/* This Arduino Sketch controls two AC units on/off with one wire temp sensors

includes rotary encoder for temp change
saves last setting to eeprom location 0
Adafruit RGB Buttons for temp up and down
add latching fans

*/
#include <OneWire.h>
#include <DallasTemperature.h>
//#include <SendOnlySoftwareSerial.h>
#include <Wire.h>
#include <Adafruit_MCP23017.h>
#include <Adafruit_RGBLCDShield.h>
#include <EEPROM.h>

Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

#define WHITE 0x7

int currentAC = 0; //placeholder for current AC unit
int lastAC = 0;    //placeholder for last prime AC unit
// int tensOfdays;    //count tens of days for switching systems
int stage1On = 0;  //indicate stage one on
int stage2On = 0;  //indicate stage two on
int setTempint = 0; //int value of setTemp for display
int insideTempint = 0; //int value of insideTemp for display
int mrUnit0 = 0; //compressor min run indicator
int mrUnit1 = 0; //compressor min run indicator
int hoUnit0 = 0; //compressor hold off indicator
int hoUnit1 = 0; //compressor hold off indicator
int eeTemp = 0;  //eeprom int temp
int fanLatch0 = 0; //latch unit 0 fan on
int fanLatch1 = 0; //latch unit 1 fan on
const int eeAddrtemp = 0; // eeprom address for temperature setting
const int eeFanlatch0addr = 1; //eeprom address for fan 0 latch
const int eeFanlatch1addr = 2; //eeprom address for fan 1 latch
const int ac0Fan = 9;  //AC 1 Fan Relay
const int ac0Cool = 10;  //AC 1 Cooling Demand
const int ac1Fan = 11;  //AC 2 Fan Relay
const int ac1Cool = 12;  //AC 2 Cooling Demand
const int upperLimit = 77; // set upper temperature setting limit
const int lowerLimit = 66; //set lower temperature setting limit


int encoderPin1 = 2;  // Added for rotary encoder
int encoderPin2 = 3;  // Added for rotary encoder

volatile int lastEncoded = 0;  // Added for rotary encoder
volatile long encoderValue = 0;  // Added for rotary encoder

long lastencoderValue = 0;  // Added for rotary encoder

int lastMSB = 0;  // Added for rotary encoder
int lastLSB = 0;  // Added for rotary encoder


float insideTemp;  //measured inside temperature
float supplyAir1temp;  //supply air temp of unit one
float supplyAir2temp;  //supply air temp of unit two
float setTemp = 68.00;  //Room Temp Set Point
const float hysTemp = 0.50;   //Toom Temp difference
float onTemp;  //AC On Temp
float offTemp;  //AC Off Temp
float stage2Tempon; //Secondary AC on temp
float stage2Tempoff; //Secondary AC off temp
const float supplyDiff = 7; //Temp diff for supply air

//long millisLast; //millis last time units switched
//const long millis10days = 180000; //number of millis in 10 days 864000000

unsigned long stage1Ontime = 0;    //time in millis last time unit started
unsigned long stage2Ontime = 0;    //time in millis last time unit started
unsigned long stage1Offtime = 0;  //time in millis last time unit shut off for compressor protection
unsigned long stage2Offtime = 0;  //time in millis last time unit shut off for compressor protection
const unsigned long compressorWait = 180000;  //set compressor wait time
const unsigned long compressorMinrun = 60000; //sets compressor minimum run time

//Initialize Serial Port for LCD

//SendOnlySoftwareSerial sLCD(13); //TX Only pin 13

//#define COMMAND 0xFE
//#define CLEAR   0x01
//#define LINE0   0x80
//#define LINE1   0xC0
//#define LINE2   0x94
//#define LINE3   0xD4

// Data wire is plugged into pin 2 on the Arduino
const int ONE_WIRE_BUS = 8;

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// Assign the addresses of your 1-Wire temp sensors.

DeviceAddress insideThermometer = { 0x28, 0xA2, 0x89, 0x60, 0x03, 0x00, 0x00, 0x60 };
DeviceAddress supplyAir1 = { 0x28, 0xA2, 0x89, 0x60, 0x03, 0x00, 0x00, 0x60 };
DeviceAddress supplyAir2 = { 0x28, 0xA2, 0x89, 0x60, 0x03, 0x00, 0x00, 0x60 };

void setup(void)
{
  // start serial ports
  Serial.begin(9600);
  //  sLCD.begin(9600);  Start the LCD Serial Port and clear the display
  //  delay(500);
  //  sLCD.write(COMMAND);
  //  sLCD.write(CLEAR);
  //  delay(500);

    // Start the LCD and set the columns and rows
  lcd.begin(16, 2);
  
  // Set the LCD Backlight
  lcd.setBacklight(WHITE);

  // Set the parameter labels
  lcd.setCursor(0, 0);
  lcd.print("S");
  lcd.setCursor(0, 1);
  lcd.print("T");
  lcd.setCursor(12, 0);
  lcd.print("Cu");
  lcd.setCursor(6, 0);
  lcd.print("On");
  lcd.setCursor(8, 0);
  lcd.print("Ho");
  lcd.setCursor(10, 0);
  lcd.print("Mr");
  lcd.setCursor(14, 0);
  lcd.print("Fl");
  
  
  // Start up the sensor library
  sensors.begin();

  // set the resolution to 10 bit (good enough?)
  sensors.setResolution(insideThermometer, 10);
  sensors.setResolution(supplyAir1, 10);
  sensors.setResolution(supplyAir2, 10);
  
  //set pin modes
  pinMode(ac0Fan, OUTPUT);
  pinMode(ac0Cool, OUTPUT);
  pinMode(ac1Fan, OUTPUT);
  pinMode(ac1Cool, OUTPUT);

  
  //set output pins low
  digitalWrite(ac0Fan, LOW);
  digitalWrite(ac0Cool, LOW);
  digitalWrite(ac1Fan, LOW);
  digitalWrite(ac1Cool, LOW);
  
  // Below lines added for enceoder
  pinMode(encoderPin1, INPUT); 
  pinMode(encoderPin2, INPUT);

  digitalWrite(encoderPin1, HIGH); //turn pullup resistor on
  digitalWrite(encoderPin2, HIGH); //turn pullup resistor on

  //call updateEncoder() when any high/low changed seen
  //on interrupt 0 (pin 2), or interrupt 1 (pin 3) 
  attachInterrupt(0, updateEncoder, CHANGE); 
  attachInterrupt(1, updateEncoder, CHANGE);
  // Above lines added for encoder
  
  //check eeprom and se to valid temp if out of range
  eeTemp = EEPROM.read(eeAddrtemp);
  if (eeTemp < 66 || eeTemp > 78)
  {
    EEPROM.write(eeAddrtemp, 68);
  }
  
  //set eeprom fan latches to 0 if not already set
  if (EEPROM.read(eeFanlatch0addr) > 1)
  {
    EEPROM.write(eeFanlatch0addr, 0);
  }
  if (EEPROM.read(eeFanlatch1addr) > 1)
  {
    EEPROM.write(eeFanlatch1addr, 0);
  }
  
  
  
}

void loop(void)
{ 
  delay(1000);  //wait .1 second for onewire buss to settle//  delay(100);  //wait .1 second for onewire buss to settle
  Serial.print("\n\r\n\rGetting temperatures...\n\r");
  sensors.requestTemperatures();  //tell sensors to read temperature
  
  //added to take avcantage of the Adafruit RGBLCD Buttons
  uint8_t buttons = lcd.readButtons();
  if (buttons != 0)
  {
    if (buttons & BUTTON_UP)
    {
      if (setTemp < upperLimit)
      {
        setTemp = setTemp + 1;
        eeTemp = (int) setTemp;
        EEPROM.write(eeAddrtemp, eeTemp);
      }
    }
    if (buttons & BUTTON_DOWN)
    {
      if (setTemp > lowerLimit)
      {
        setTemp = setTemp - 1;
        eeTemp = (int) setTemp;
        EEPROM.write(eeAddrtemp, eeTemp);
      }
    }
    if (buttons & BUTTON_LEFT)
    {
      if (EEPROM.read(eeFanlatch0addr) == 0)
      {
        EEPROM.write(eeFanlatch0addr, 1);
      }
      else // (EEPROM.read(eeFanlatch0addr) == 1)
      {
        EEPROM.write(eeFanlatch0addr, 0);
      }
    }
    if (buttons & BUTTON_RIGHT)
    {
      if (EEPROM.read(eeFanlatch1addr) == 0)
      {
        EEPROM.write(eeFanlatch1addr, 1);
      }
      else // (EEPROM.read(eeFanlatch1addr) == 1)
      {
        EEPROM.write(eeFanlatch1addr, 0);
      }
    }
  }

  eeTemp = EEPROM.read(eeAddrtemp);
  setTemp = (float) eeTemp;
  
    //moved to allow dynamic temp change
  onTemp = setTemp + 0.90;  //AC On Temp
  offTemp = setTemp - 0.10;  //AC Off Temp
  stage2Tempon = onTemp + 3; //Secondary AC on temp
  stage2Tempoff  = onTemp; //Secondary AC off temp
 
 /* 
       // section of code for debugging
  Serial.print("Inside temperature is: ");
  */
  
  insideTemp = sensors.getTempF(insideThermometer);  //get temperature from sensor for ambient air temp
  
  /*
  Serial.print(insideTemp);
  Serial.print("\n\r");
  Serial.print("Supply Air 1 Temp is: ");
  supplyAir1temp = sensors.getTempF(supplyAir1);  //get temp from sensor for unit 1 supply air
  Serial.print(supplyAir1temp);
  Serial.print("\n\r");
  Serial.print("Supply Air 2 Temp is: ");
  supplyAir2temp = sensors.getTempF(supplyAir2);  //get temp from sensor for unit 2 supply air
  Serial.print(supplyAir2temp);
  Serial.print("\n\r\n\rSet Temp ");
  Serial.print(setTemp);
  Serial.print("\n\rHysTemp ");
  Serial.print(hysTemp);
  Serial.print("\n\rOn Temp ");
  Serial.print(onTemp);
  Serial.print("\n\rOff Temp ");
  Serial.print(offTemp);
  Serial.print("\n\r\n\r");
//  Serial.print("\n\rTens of Days ");
//  Serial.print(tensOfdays);
  Serial.print("\n\rCurrent AC Unit ");
  Serial.print(currentAC);
  Serial.print("\n\rStage One On ");
  Serial.print(stage1On);
  Serial.print("\n\rStage Two On ");
  Serial.print(stage2On);
  Serial.print("\n\r");
  Serial.print("Stage 1 Off = ");
  Serial.print(stage1Offtime);
  Serial.print("\n\rStage 2 Off = ");
  Serial.print(stage2Offtime);
  Serial.print("\n\rMillis = ");
  Serial.print(millis());
  Serial.print("\n\n\r");
  Serial.print(EEPROM.read(eeAddrtemp));
  Serial.print("\n\n\r");
*/

//LCD Commands
  setTempint = (int) setTemp; //cast setTemp to int for display
  insideTempint = (int) insideTemp; //cast insideTemp to int for display

  // Print Setpoint
  lcd.setCursor(1, 0);
  lcd.print(setTempint);

  // Print Current Temp
  lcd.setCursor(1, 1);
  lcd.print(insideTemp);
  
  // Print Current Unit
  lcd.setCursor(13, 1);
  lcd.print(currentAC);
  
  // Indicate On Unit
  if (stage1On == 1)
  {
    lcd.setCursor(6, 1);
    lcd.print("0");
  }
  else
  {
    lcd.setCursor(6, 1);
    lcd.print(" ");
  }
  
  if (stage2On == 1)
  {
    lcd.setCursor(7, 1);
    lcd.print("1");
  }
  else
  {
    lcd.setCursor(7, 1);
    lcd.print(" ");
  }  
  
  // Indicate Hold-off
  if (hoUnit0 == 1)
  {
    lcd.setCursor(8, 1);
    lcd.print("0");
  }
  else
  {
    lcd.setCursor(8, 1);
    lcd.print(" ");
  }
  
  if (hoUnit1 == 1)
  {
    lcd.setCursor(9, 1);
    lcd.print("1");
  }
  else
  {
    lcd.setCursor(9, 1);
    lcd.print(" ");
  }
  
  // Indicate Min Run
  if (mrUnit0 == 1)
  {
    lcd.setCursor(10, 1);
    lcd.print("0");
  }
  else
  {
    lcd.setCursor(10, 1);
    lcd.print(" ");
  }
  
  if (mrUnit1 == 1)
  {
    lcd.setCursor(11, 1);
    lcd.print("1");
  }
  else
  {
    lcd.setCursor(11, 1);
    lcd.print(" ");
  }  
  
  if (EEPROM.read(eeFanlatch0addr) == 1)
  {
    lcd.setCursor(14, 1);
    lcd.print("0");
  }
  else
  {
    lcd.setCursor(14, 1);
    lcd.print(" ");
  }
  
  if (EEPROM.read(eeFanlatch1addr) == 1)
  {
    lcd.setCursor(15, 1);
    lcd.print("1");
  }
  else
  {
    lcd.setCursor(15, 1);
    lcd.print(" ");
  }
  
/*****************************
  sLCD.write(COMMAND);
  sLCD.write(LINE0);
  sLCD.print("SP=");
  sLCD.write(COMMAND);
  sLCD.write(LINE0 + 0x03);
  sLCD.print(setTempint);
  sLCD.write(COMMAND);
  sLCD.write(LINE0 + 6);
  sLCD.print("Tmp=");
  sLCD.write(COMMAND);
  sLCD.write(LINE0 + 0x0A);
  sLCD.print(insideTemp);
  sLCD.write(COMMAND);
  sLCD.write(LINE1);
  sLCD.print("Unit0 On=");
  sLCD.write(COMMAND);
  sLCD.write(LINE1 + 0x09);
  sLCD.print(stage1On);
  sLCD.write(COMMAND);
  sLCD.write(LINE1 + 0x0B);
  sLCD.print("HO=");
  sLCD.write(COMMAND);
  sLCD.write(LINE1 + 0x0E);
  sLCD.print(hoUnit0);
  sLCD.write(COMMAND);
  sLCD.write(LINE1 + 0x10);
  sLCD.print("MR=");
  sLCD.write(COMMAND);
  sLCD.write(LINE1 + 0x13);
  sLCD.print(mrUnit0);
  sLCD.write(COMMAND);
  sLCD.write(LINE2);
  sLCD.print("Unit1 On=");
  sLCD.write(COMMAND);
  sLCD.write(LINE2 + 0x09);
  sLCD.print(stage2On);
  sLCD.write(COMMAND);
  sLCD.write(LINE2 + 0x0B);
  sLCD.print("HO=");
  sLCD.write(COMMAND);
  sLCD.write(LINE2 + 0x0E);
  sLCD.print(hoUnit1);
  sLCD.write(COMMAND);
  sLCD.write(LINE2 + 0x10);
  sLCD.print("MR=");
  sLCD.write(COMMAND);
  sLCD.write(LINE2 + 0x13);
  sLCD.print(mrUnit1);
  sLCD.write(COMMAND);
  sLCD.write(LINE3);
  sLCD.print("CU=");
  sLCD.write(COMMAND);
  sLCD.write(LINE3 + 0x03);  
  sLCD.print(currentAC);
//  sLCD.write(COMMAND);
//  sLCD.write(LINE3 + 0x05);
//  sLCD.print("TOD=");
//  sLCD.write(COMMAND);
//  sLCD.write(LINE3 + 0x09);  
//  sLCD.print(tensOfdays);
  sLCD.write(COMMAND);
  sLCD.write(LINE3 + 0x0B);
  sLCD.print("MIN=");
  sLCD.write(COMMAND);
  sLCD.write(LINE3 + 0x0F);
  sLCD.print(millis()/60000);
****************************************/

  // Control Code

//tensOfdays = millis() / millis10days;  //check to see if it has been ten days
//currentAC = tensOfdays % 2;  //check for odd or even ten days to switch units

// the following four if statements check for millis() roll over and reset compressor timeouts to 0 if there is rollover
if (stage1Offtime > millis())
{
  stage1Offtime = 0;
}
if (stage2Offtime > millis())
{
  stage2Offtime = 0;
}
if (stage1Ontime > millis())
{
  stage1Ontime = 0;
}
if (stage2Ontime > millis())
{
  stage2Ontime = 0;
}

if (currentAC == 0 && lastAC == 0 && stage1On == 0 && hoUnit0 == 0) //if the last demand was AC0 then switch to AC1
{
  currentAC = 1;
}

if (currentAC == 1 && lastAC == 1 && stage2On == 0 && hoUnit1 == 0) //if the last demand was AC1 then switch to AC0
{
  currentAC = 0;
}

if (currentAC == 0)  //check if unit one is lead unit
{
    
//Stage One Prime
  
    if (insideTemp > onTemp && digitalRead(ac0Cool) == 0)  //check inside temp and if unit one is already on
     {       
       stageOneon();  //call routine to turn on unit 1
       lastAC = 0;
       if (digitalRead(ac1Cool) == 1 && onTemp < stage2Tempon)  //chec if unit two is on and temp is less than stage two cooling
       {
         stageTwooff();  //turns off unit two if only first stage cooling is needed
//         stage2Offtime = millis();
       }
     }
    
    if (insideTemp > stage2Tempon && digitalRead(ac0Cool) == 1 && digitalRead(ac1Cool) == 0)  //check inside temp and turn stage 2 on if called
       {
         stageTwoon();  //turn on stage two
       } 
}   

if (currentAC == 1)  //check if unit two is lead unit
{
 // Stage Two Prime
    
    if (insideTemp > onTemp && digitalRead(ac1Cool) == 0)  //check inside temp and if unit two is already on
     {
       stageTwoon();  //call routine to turn unit two on
       lastAC = 1;
       if (digitalRead(ac0Cool) == 1 && onTemp < stage2Tempon)  //check if unit one is on and temp is less than stage two cooling
       {
         stageOneoff();  //turn off unit one if only one stage cooling is needed
//         stage1Offtime = millis();
       }
     }
    
    if (insideTemp > stage2Tempon && digitalRead(ac1Cool) == 1 && digitalRead(ac0Cool) == 0)  //check inside temp and turn stage one on if called
       {
         stageOneon();  //turn stage one on
       }

}

if (insideTemp <= offTemp && digitalRead(ac0Cool) == 1)  //check inside temp and turn off if below set point
 {
   stageOneoff();  //routine to turn unit one off
 }

if (insideTemp <= offTemp && digitalRead(ac1Cool) == 1)  //check inside temp and turn off if below set point
 {
   stageTwooff();  //routine to turn unit one off
 } 
 
 
  if (EEPROM.read(eeFanlatch0addr) == 1 && digitalRead(ac0Fan == 0)) //checks to see if there is a manual fan 0 command
  {
    digitalWrite(ac0Fan, HIGH); //Turns Fan 0 on for manual operation
  }
  if (EEPROM.read(eeFanlatch0addr) == 0 && stage1On == 0) // Turns fan 0 off if there is no manual and no cooling demand
  {
    digitalWrite(ac0Fan, LOW); //Turns Fan 0 off for manual operation
  }
  
  if (EEPROM.read(eeFanlatch1addr) == 1 && digitalRead(ac1Fan == 0)) //checks to see if there is a manual fan 1 command
 {
  digitalWrite(ac1Fan, HIGH); //Turns fan on for manual operation. 
 }
  if (EEPROM.read(eeFanlatch1addr) == 0 && stage2On == 0) // Turns fan 1 off if there is no manual and no cooling demand
 {
  digitalWrite(ac1Fan, LOW); //Turns fan off for manual operation. 
 }
 
 
} // end of main loop


void stageOneon() //routine to turn unit one on
{
  if ((stage1Offtime + compressorWait) < millis())  //check last compressor shut off time and hold off if below five minutes
  {
    digitalWrite(ac0Fan, HIGH); //turns on ac 1 fan
    delay(5000);                // wait 5 seconds
    digitalWrite(ac0Cool, HIGH); //calls ac 1 compressor
    stage1Ontime = millis();
    stage1On = 1;
    hoUnit0 = 0;
  }
  else
  {
    hoUnit0 = 1;
  }
}

void stageOneoff() //routine to turn unit one off and set compressor off time
{
  if ((millis() - stage1Ontime) > compressorMinrun) //check last compressor start and hold off untill min comp run time expires
  {
    digitalWrite(ac0Cool, LOW);  //turns compressor off
    
    if (EEPROM.read(eeFanlatch0addr) == 0);
    {
      delay(5000);                 // pull cooling out of the coil
      digitalWrite(ac0Fan, LOW);  //turns fan off
    }
    stage1Offtime = millis();  //sets compressor off time
    stage1On = 0;
    mrUnit0 = 0;
  }
  else
  {
    mrUnit0 = 1;
  }

}

void stageTwoon()  //routine to turn unit two on
{
  if ((stage2Offtime + compressorWait) < millis())  //check last compressor shut off time and hold off if below five minutes
  {
    digitalWrite(ac1Fan, HIGH);  //turns on ac 2 fan
    delay(5000);                 // wait 5 seconds
    digitalWrite(ac1Cool, HIGH);  //calls ac 2 compressor
    stage2Ontime = millis();
    stage2On = 1;
    hoUnit1 = 0;
  }
  else
  {
    hoUnit1 = 1;
  }
}

void stageTwooff()
{
    if ((millis() - stage2Ontime) > compressorMinrun) //check last compressor start and hold off untill min comp run time expires
    {
      digitalWrite(ac1Cool, LOW);  //turn compressor off

      if (EEPROM.read(eeFanlatch1addr) == 0);
      {
        delay(5000);                 // pull cooling out of the coil
        digitalWrite(ac1Fan, LOW);  //turn fan off
      }
      stage2Offtime = millis();  //set compressor off time
      stage2On = 0;
      mrUnit1 = 0;
    }
    else
    {
      mrUnit1 = 1;
    }
}

void updateEncoder(){
  int MSB = digitalRead(encoderPin1); //MSB = most significant bit
  int LSB = digitalRead(encoderPin2); //LSB = least significant bit

  int encoded = (MSB << 1) |LSB; //converting the 2 pin value to single number
  int sum  = (lastEncoded << 2) | encoded; //adding it to the previous encoded value

  if(sum == 0b1101) setTemp = setTemp + 1;
  if(sum == 0b1110) setTemp = setTemp - 1;
  
  // set temp 
  if (setTemp < lowerLimit)
  {
    setTemp = lowerLimit;
  }
  if (setTemp > upperLimit)
  {
    setTemp = upperLimit;
  }
  
  eeTemp = (int) setTemp;
  EEPROM.write(eeAddrtemp, eeTemp);

  lastEncoded = encoded; //store this value for next time
}
