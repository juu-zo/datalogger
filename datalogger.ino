
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

const int chipSelect = 10;

#define DS3231_I2C_ADDRESS 0x68

unsigned long PulseStartTime1;   // Saves Start of pulse in us
unsigned long PulseStartTime2;
unsigned long PulseEndTime1;     // Saves End of pulse in us
unsigned long PulseEndTime2;
unsigned long PulseTime1;        // Stores dif between start and stop of pulse
unsigned long PulseTime2;
unsigned long RPM = 0;          // RPM to output (30*1000/PulseTime1)
unsigned long PULSES = 0;          // PULSES to output (30*1000/PulseTime2)
boolean Cycle1 = false;
boolean Cycle2 = false;
String datetimeString;
String dataString;

File myFile;

void setup()
{
Wire.begin();
Serial.begin(115200);  // OPENS SERIAL PORT SETS DATA TO 115200 bps
delay(500);
attachInterrupt(0, RPMPulse, RISING); // Attaches interrupt to Digital Pin 2 = int0
attachInterrupt(1, PulseCounter, RISING); // Attaches interrupt to Digital Pin 3 = int1
  // set the initial time here:
  // DS3231 seconds, minutes, hours, day, date, month, year
  // setDS3231time(30,42,21,4,26,10,16);
  
  while (!Serial) 
  {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  
  Serial.print("Initializing SD card...");

  if (!SD.begin(chipSelect)) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");

  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  myFile = SD.open("log.csv", FILE_WRITE);

  if (myFile)  // if the file opened okay, write to it:
  {
    Serial.print("Writing to log.csv...");
    myFile.println("Datalog file");
    myFile.close();  // close the file:
    Serial.println("done.");
  } else Serial.println("error opening log.csv");  // if the file didn't open, print an error:
}

byte decToBcd(byte val)
{
  return( (val/10*16) + (val%10) );  
}
byte bcdToDec(byte val)
{
  return( (val/16*10) + (val%16) );
}

void setDS3231time(byte second, byte minute, byte hour, byte dayOfWeek, byte
dayOfMonth, byte month, byte year)
{
  // sets time and date data to DS3231
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set next input to start at the seconds register
  Wire.write(decToBcd(second)); // set seconds
  Wire.write(decToBcd(minute)); // set minutes
  Wire.write(decToBcd(hour)); // set hours
  Wire.write(decToBcd(dayOfWeek)); // set day of week (1=Sunday, 7=Saturday)
  Wire.write(decToBcd(dayOfMonth)); // set date (1 to 31)
  Wire.write(decToBcd(month)); // set month
  Wire.write(decToBcd(year)); // set year (0 to 99)
  Wire.endTransmission();
}

void readDS3231time(byte *second, byte *minute, byte *hour, byte *dayOfWeek, byte *dayOfMonth, byte *month, byte *year)
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

void readTime()
{
  datetimeString = "";
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  // retrieve data from DS3231
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month,
  &year);
  datetimeString += String(hour, DEC);
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

void displayTime()
{ 
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  // retrieve data from DS3231
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month,
  &year);
  // send it to the serial monitor
  Serial.print(hour, DEC);
  // convert the byte variable to a decimal number when displayed
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

void RPMPulse()
{
 if (Cycle1 == false)           // Check to see if start pulse
 {
   PulseStartTime1 = micros();  // stores start time
   Cycle1 = !Cycle1;             // toggle Cycle boolean
   return;                     // a return so it doesnt run the next if
 }
 if (Cycle1 == true)            // Check to see if end pulse
 {          
   PulseEndTime1 = micros();    // stores end time
   Cycle1 = !Cycle1;             // toggle Cycle boolean
   calcRPM();                  // call to calculate pulse time
 }
}

void calcRPM()
{
 detachInterrupt(0);            // Turns off inturrupt for calculations
 PulseTime1 = PulseEndTime1 - PulseStartTime1; // Gets pulse duration
 RPM = 30*1000000/PulseTime1*2;              // Calculates RPM
 attachInterrupt(0, RPMPulse, RISING);      // re-attaches interrupt to Digi Pin 2
}

void PulseCounter ()
{
 if (Cycle2 == false)           // Check to see if start pulse
 {
   PulseStartTime2 = micros();  // stores start time
   Cycle2 = !Cycle2;             // toggle Cycle boolean
   return;                     // a return so it doesnt run the next if
 }
 if (Cycle2 == true)            // Check to see if end pulse
 {          
   PulseEndTime2 = micros();    // stores end time
   Cycle2 = !Cycle2;             // toggle Cycle boolean
   calcPulses();                  // call to calculate pulse time
 }
}

void calcPulses()
{
 detachInterrupt(1);            // Turns off inturrupt for calculations
 PulseTime2 = PulseEndTime2 - PulseStartTime2; // Gets pulse duration
 PULSES = 30*1000000/PulseTime2*2;              // Calculates RPM
 attachInterrupt(1, PulseCounter, RISING);      // re-attaches interrupt to Digi Pin 2
}

void readAnalog()
{
  dataString = "";
  for (int analogPin = 0; analogPin < 4; analogPin++) 
  {
    int sensor = analogRead(analogPin);
    dataString += String(sensor);
    if (analogPin < 5) 
    {
      dataString += ", ";
    }
  } 
  for (int analogPin = 6; analogPin < 8; analogPin++) 
  {
    int sensor = analogRead(analogPin);
    dataString += String(sensor);
    if (analogPin < 7) 
    {
      dataString += ", ";
    }
  }
}
void loop()
{
  myFile = SD.open("log.csv", FILE_WRITE);

  readAnalog(); // read four sensors and append to the string
  readTime(); // read date and time and append to the string
  
  myFile.print(datetimeString);
  myFile.print(", ");
  myFile.print(RPM);
  myFile.print(", ");
  myFile.print(PULSES);
  myFile.print(", ");
  myFile.print(dataString);
  myFile.print(", ");
  myFile.println();
  myFile.close();

  displayTime(); // display the real-time clock data on the Serial Monitor
  Serial.print("RPM = ");      // Output RPM for debug
  Serial.println(RPM);
  Serial.print("PULSES = ");      // Output RPM for debug
  Serial.println(PULSES);
  Serial.print("Analogvalues = ");      
  Serial.println(dataString); 
  delay(100);                   // delay for debug output
}
