
#include <Wire.h> // included libraries
#include <SPI.h>
#include <SD.h>
#include <max6675.h>

const int sdcardCS = 10;      // sdcard chipselect pin number
const int tempCS = 5;        // k-type thermocouple 1 chipselect pin number
const int tempDO = 4;        // k-type thermocouple 2 chipselect pin number
const int buttonPin = 9;      // log start pin number
const int microsddetect = 8;  // sdcard card detect pin number
const int statusled1 = 7;     // statusled pin number
const int statusled2 = 6;     // statusled pin number

#define DS3231_I2C_ADDRESS 0x68 //real time clock i2c address
MAX6675 thermocouple(13, tempCS, tempDO); // thermocouple pins

unsigned long PulseStartTime1;  // Saves Start of pulse1 in us, RPM
unsigned long PulseStartTime2;  // Saves Start of pulse2 in us
unsigned long PulseEndTime1;    // Saves End of pulse1 in us, RPM
unsigned long PulseEndTime2;    // Saves End of pulse2 in us
unsigned long PulseTime1;       // Stores dif between start and stop of pulse, RPM
unsigned long PulseTime2;       // Stores dif between start and stop of pulse, RPM
unsigned long RPM = 0;          // RPM to output (30*1000/PulseTime1)
unsigned long PULSES = 0;       // PULSES to output (30*1000/PulseTime2)
boolean Cycle1 = false;         // toggle for pulse1 start and stop
boolean Cycle2 = false;         // toggle for pulse2 start and stop
boolean statusled1state = false; // toggle for blinking statusled1
String datetimeString;          // String to save date and time
String timeString;              // String to save time
String dataString;              // String to save analog values
String filenameString;          // String to save filename

float temperature;         // thermocouple temperature variable, celsius
int currbuttonState = 1;        // variable initializations
int prevbuttonState = 1;
int logMode = 0;
int reboot = 0;

File myFile;                    // file calling name

void setup()
{
pinMode(buttonPin, INPUT);          // Set digital pins as inputs and outputs
digitalWrite(buttonPin, HIGH);      // Set digital pins as high or low state
pinMode(microsddetect, INPUT);
digitalWrite(microsddetect, HIGH);
pinMode(statusled1, OUTPUT);
digitalWrite(statusled1, HIGH);
pinMode(statusled2, OUTPUT);
digitalWrite(statusled2, HIGH);
Wire.begin();                       // opens i2c communication
Serial.begin(115200);               // OPENS SERIAL PORT SETS DATA TO 115200 bps
delay(500);                         // wait a bit 
attachInterrupt(0, RPMPulse, RISING); // Attaches interrupt to Digital Pin 2 = int0
attachInterrupt(1, PulseCounter, RISING); // Attaches interrupt to Digital Pin 3 = int1
  
/* SET INITIAL TIME HERE: DS3231 seconds, minutes, hours, day, date, month, year  */
  //setDS3231time(30,42,21,4,26,10,16); //uncomment at first time to set rtc time
  filename(); //read rtc for filename
  
  while (!Serial) 
  {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  if(digitalRead(microsddetect) == 0)    // check if sdcard is inserted
  {
  Serial.print("Initializing SD card..."); 

  if (!SD.begin(sdcardCS))            // Initialize sdcard
  {        
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");
  }
 else                                  // If sdcard not inserted, blink error 
 {
 statusled1state = !statusled1state;
 digitalWrite(statusled1, statusled1state);
 digitalWrite(statusled2, LOW);
 Serial.println("Insert microsd card and reset");
 delay(200);
 return;
 }
 Serial.println(filenameString);
  myFile = SD.open(filenameString, FILE_WRITE); // open the file. note that only one file can be open at a time, so you have to close this one before opening another.

  if (myFile)  // if the file opened okay, write to it:
  {
    Serial.print("Writing to ");
    Serial.print(filenameString);
    Serial.print("...");
    myFile.println(filenameString);   // print filename to first row
    myFile.close();  // close the file:
    Serial.println("done.");
  } 
  else 
  {
  Serial.println("error opening log file!");  // if the file didn't open, print an error:
  reboot = 1;
  }
  digitalWrite(statusled1, LOW);  // Shut down both leds after setup
  digitalWrite(statusled2, LOW);
}
  
byte decToBcd(byte val) // decimal to binary coded decimal conversion for rtc
{
  return( (val/10*16) + (val%10) );  
}
byte bcdToDec(byte val) // binary coded decimal to decimal conversion for rtc
{
  return( (val/16*10) + (val%16) );
}

void setDS3231time(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year)  // sets time and date data to DS3231
{
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set next input to start at the seconds register
  Wire.write(decToBcd(second)); // set seconds
  Wire.write(decToBcd(minute)); // set minutes
  Wire.write(decToBcd(hour)); // set hours
  Wire.write(decToBcd(dayOfWeek)); // set day of week (1=Monday, 7=Sunday)
  Wire.write(decToBcd(dayOfMonth)); // set date (1 to 31)
  Wire.write(decToBcd(month)); // set month
  Wire.write(decToBcd(year)); // set year (0 to 99)
  Wire.endTransmission();
}

void readDS3231time(byte *second, byte *minute, byte *hour, byte *dayOfWeek, byte *dayOfMonth, byte *month, byte *year) // read time from real time clock
{
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set DS3231 register pointer to 00h
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 7);
  // request seven bytes of data from DS3231 starting from register 00h
  *second = bcdToDec(Wire.read() & 0x7f);
  *minute = bcdToDec(Wire.read());
  *hour = bcdToDec(Wire.read() & 0x3f);
  *dayOfWeek = bcdToDec(Wire.read());
  *dayOfMonth = bcdToDec(Wire.read());
  *month = bcdToDec(Wire.read());
  *year = bcdToDec(Wire.read());
}

void filename()
{
  filenameString = ""; // empty filename string
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year; 
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year); // retrieve data from DS3231
  
 /* filenameString += String(dayOfMonth, DEC);
  filenameString += String(month, DEC);
  filenameString += String(year, DEC);*/
  filenameString += "011016.csv";
}

void readDateTime()
{
  datetimeString = ""; // empty date and time string
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year; 
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year); // retrieve data from DS3231
 
  datetimeString += String(hour, DEC); // write to the datetimeString, convert the byte variable to a decimal number when writed
  datetimeString += ":";
  if (minute<10)
  {
    datetimeString += "0";
  }
  datetimeString += String(minute, DEC);
  datetimeString += ":";
  if (second<10)
  {
    datetimeString += "0";
  }
  datetimeString += String(second, DEC);
  datetimeString += " ";
  datetimeString += String(dayOfMonth, DEC);
  datetimeString += "/";
  datetimeString += String(month, DEC);
  datetimeString += "/";
  datetimeString += String(year, DEC);
}

void readTime()
{
  timeString = ""; // empty date and time string
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year; 
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year); // retrieve data from DS3231
 
  timeString += String(hour, DEC); // write to the datetimeString, convert the byte variable to a decimal number when writed
  timeString += ":";
  if (minute<10)
  {
    timeString += "0";
  }
  timeString += String(minute, DEC);
  timeString += ":";
  if (second<10)
  {
    timeString += "0";
  }
  timeString += String(second, DEC);
}

void displayTime()
{ 
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);  // retrieve data from DS3231
  
  Serial.print(hour, DEC);   // send it to the serial monitor, convert the byte variable to a decimal number when displayed
  Serial.print(":"); 
  if (minute<10) 
  {
    Serial.print("0");
  }
  Serial.print(minute, DEC);
  Serial.print(":");
  if (second<10)
  {
    Serial.print("0");
  }
  Serial.print(second, DEC);
  Serial.print(" ");
  Serial.print(dayOfMonth, DEC);
  Serial.print("/");
  Serial.print(month, DEC);
  Serial.print("/");
  Serial.print(year, DEC);
  Serial.print(" Day of week: ");
  switch(dayOfWeek){
  case 1:
    Serial.println("Monday");
    break;
  case 2:
    Serial.println("Tuesday");
    break;
  case 3:
    Serial.println("Wednesday");
    break;
  case 4:
    Serial.println("Thursday");
    break;
  case 5:
    Serial.println("Friday");
    break;
  case 6:
    Serial.println("Saturday");
    break;
  case 7:
    Serial.println("Sunday");
    break;
  }
  Serial.println("");
}

void RPMPulse()   // Function to check RPM pulsetime
{
 if (Cycle1 == false)             // Check to see if start pulse
 {
   PulseStartTime1 = micros();    // stores start time
   Cycle1 = !Cycle1;              // toggle Cycle boolean
   return;                        // a return so it doesnt run the next if
 }
 if (Cycle1 == true)              // Check to see if end pulse
 {          
   PulseEndTime1 = micros();      // stores end time
   Cycle1 = !Cycle1;              // toggle Cycle boolean
   calcRPM();                     // call to calculate pulse time
 }
}

void calcRPM()      // Function to calculate RPM 
{
 detachInterrupt(0);                            // Turns off interrupt for calculations
 PulseTime1 = PulseEndTime1 - PulseStartTime1;  // Gets pulse duration
 RPM = 30*1000000/PulseTime1*2;                 // Calculates RPM
 attachInterrupt(0, RPMPulse, RISING);          // re-attaches interrupt to Digi Pin 2
}

void PulseCounter () // Function to check pulsetime
{
 if (Cycle2 == false)             // Check to see if start pulse
 {
   PulseStartTime2 = micros();    // stores start time
   Cycle2 = !Cycle2;              // toggle Cycle boolean
   return;                        // a return so it doesnt run the next if
 }
 if (Cycle2 == true)              // Check to see if end pulse
 {          
   PulseEndTime2 = micros();      // stores end time
   Cycle2 = !Cycle2;              // toggle Cycle boolean
   calcPulses();                  // call to calculate pulse time
 }
}

void calcPulses()      // Function to calculate pulses 
{
 detachInterrupt(1);                            // Turns off inturrupt for calculations
 PulseTime2 = PulseEndTime2 - PulseStartTime2;  // Gets pulse duration
 PULSES = 30*1000000/PulseTime2*2;              // Calculates RPM
 attachInterrupt(1, PulseCounter, RISING);      // re-attaches interrupt to Digi Pin 2
}

void readAnalog()  // Function to read analog values
{
  dataString = ""; // empty dataString
  for (int analogPin = 0; analogPin < 4; analogPin++) // read analog pins 0-3
  {
    int sensor = analogRead(analogPin);   // read input to sensor
    dataString += String(sensor);   // write sensor value to dataString
    if (analogPin < 5) 
    {
      dataString += ", ";   // add comma for easy parse between values
    }
  } 
  for (int analogPin = 6; analogPin < 8; analogPin++) // read analog pins 6-7
  {
    int sensor = analogRead(analogPin);   // read input to sensor
    dataString += String(sensor);   // write sensor value to dataString
    if (analogPin < 7) 
    {
      dataString += ", ";   // add comma for easy parse between values
    }
  }
}

void readThermocouple()
{
  temperature = thermocouple.readCelsius();
}

void loop()
{
  
  if (reboot == 1) // if device reset needed, blink error led
  {
    statusled1state = !statusled1state;
    digitalWrite(statusled1, statusled1state);
    digitalWrite(statusled2, LOW); 
    Serial.println("Reset datalogger!");
    delay(200);
    return;  
  }
  
  currbuttonState = digitalRead(buttonPin);   // read log start button 
  
  if(currbuttonState != prevbuttonState && digitalRead(microsddetect) == 0) // check if button pressed and sdcard inserted
  {
    if (currbuttonState == HIGH)
     {
      if (logMode == 0) {            // if logging off...
          logMode = 1;               // turn logging on!
          myFile = SD.open(filenameString, FILE_WRITE); // open datalog file
          readDateTime();                           // read date and time and append to the string
          myFile.print("Loggin started at ");   // write info
          myFile.println(datetimeString);       // write date and time to file
          myFile.close();                       // close datalog file
        } else {
          logMode = 0;              // if logging on, turn it off!
        }          
     }
  }
  
  prevbuttonState = currbuttonState; // save last button press

  if (logMode == 1)     // if datalogging started
  {  
      digitalWrite(statusled1, LOW); // turn statusled1 off
      digitalWrite(statusled2, HIGH); // turn statusled2 on
      myFile = SD.open(filenameString, FILE_WRITE); // open datalog file

      readAnalog(); // read analog sensors and append to the string
      readTime(); // read date and time and append to the string
      readThermocouple();  // read thermocouple temperature
  
      myFile.print(timeString);       // write date and time to file
      myFile.print(", ");             // add comma for easy parse between values
      myFile.print(micros());         // write microseconds to file to ease plotting
      myFile.print(", "); 
      myFile.print(RPM);              // write rpm value to file
      myFile.print(", ");
      myFile.print(PULSES);           // write pulse value to file
      myFile.print(", ");
      myFile.print(dataString);       // write analog values to file
      myFile.print(", ");
      myFile.print(temperature);       // write thermocouple temperature to file
      myFile.println();               // move to next row
      myFile.close();                 // close datalog file

      displayTime(); // display the real-time clock data on the Serial Monitor
      Serial.print("RPM = ");      // Output RPM for debug
      Serial.println(RPM);
      Serial.print("PULSES = ");      // Output pulses for debug
      Serial.println(PULSES);
      Serial.print("Analogvalues = ");      // Output analog values for debug
      Serial.println(dataString); 
      
      delay(50);       // delay in ms between loops. Increase for lower sample rate, decreas for higher sample rate
  }
  
  if(logMode == 0 && digitalRead(microsddetect) == 0) // check if sdcard inserted and device ready for logging
  {
    Serial.println("Ready to log, press button");
    digitalWrite(statusled1, HIGH);
    digitalWrite(statusled2, HIGH);
    delay(100);
  }
    
  if (digitalRead(microsddetect) == 1) // check if sdcard removed and blink error
  {
    reboot = 1;
    statusled1state = !statusled1state;
    digitalWrite(statusled1, statusled1state);
    digitalWrite(statusled2, LOW); 
    Serial.println("Insert microsd card and reset");
    delay(200);
  }
}

