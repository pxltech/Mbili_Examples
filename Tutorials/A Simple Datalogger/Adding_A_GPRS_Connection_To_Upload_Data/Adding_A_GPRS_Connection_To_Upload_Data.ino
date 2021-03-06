#include <Wire.h>
#include <SPI.h>
#include <SD.h>

//SODAQ Mbili libraries
#include <RTCTimer.h>
#include <Sodaq_BMP085.h>
#include <Sodaq_SHT2x.h>
#include <Sodaq_DS3231.h>
#include <GPRSbee.h>

//The delay between the sensor readings 
#define READ_DELAY 60000

//Digital pin 11 is the MicroSD slave select pin on the Mbili
#define SD_SS_PIN 11

//The data log file
#define FILE_NAME "DataLog.txt"

//Data header
#define DATA_HEADER "TimeDate, TempSHT21, TempBMP, PressureBMP, HumiditySHT21"

//Network constants
#define APN "internet"
#define APN_USERNAME ""
#define APN_PASSWORD ""

//SpeakThings constants
#define URL "api.thingspeak.com/update"
#define WRITE_API_KEY "XXXXXXXXXXXXXXXX" //Change to your channel's key

//Seperators
#define FIRST_SEP "?"
#define OTHER_SEP "&"
#define LABEL_DATA_SEP "="

//Data labels, cannot change for ThingSpeak
#define LABEL1 "field1"
#define LABEL2 "field2"
#define LABEL3 "field3"
#define LABEL4 "field4"

//TPH BMP sensor
Sodaq_BMP085 bmp;

//RTC Timer
RTCTimer timer;

void setup() 
{
  //Initialise the serial connection
  Serial.begin(9600);
  
  //Initialise sensors
  setupSensors();
    
  //Initialise log file
  setupLogFile();
  
  //Setup timer events
  setupTimer();
  
  //Setup GPRSbee
  setupComms();
  
  //Echo the data header to the serial connection
  Serial.println(DATA_HEADER);
  
  //Take first reading immediately
  takeReading(getNow());
}

void loop() 
{
  //Update the timer 
  timer.update();  
}

void takeReading(uint32_t ts)
{
  //Create the data record
  String dataRec = createDataRecord();
  
  //Save the data record to the log file
  logData(dataRec);
  
  //Echo the data to the serial connection
  Serial.println(dataRec);
  
  //Get the data record as a URL
  String url = createDataURL();
  
  //Send it over the GPRS connection
  sendURLData(url);
}

void setupSensors()
{
  //Initialise the wire protocol for the TPH sensors
  Wire.begin();
  
  //Initialise the TPH BMP sensor
  bmp.begin();

  //Initialise the DS3231 RTC
  rtc.begin();
}

void setupLogFile()
{
  //Initialise the SD card
  if (!SD.begin(SD_SS_PIN))
  {
    Serial.println("Error: SD card failed to initialise or is missing.");
    //Hang
    while (true); 
  }
  
  //Check if the file already exists
  bool oldFile = SD.exists(FILE_NAME);  
  
  //Open the file in write mode
  File logFile = SD.open(FILE_NAME, FILE_WRITE);
  
  //Add header information if the file did not already exist
  if (!oldFile)
  {
    logFile.println(DATA_HEADER);
  }
  
  //Close the file to save it
  logFile.close();  
}

void setupTimer()
{
  //Instruct the RTCTimer how to get the current time reading
  timer.setNowCallback(getNow);

  //Schedule the reading every second
  timer.every(READ_DELAY, takeReading);
}

void setupComms()
{
  //Start Serial1 the Bee port
  Serial1.begin(9600);
  
  //Intialise the GPRSbee
  gprsbee.init(Serial1, BEECTS, BEEDTR);

  //uncomment this line to debug the GPRSbee with the serial monitor
  //gprsbee.setDiag(Serial);
  
  //This is required for the Switched Power method
  gprsbee.setPowerSwitchedOnOff(true); 
}

void logData(String rec)
{
  //Re-open the file
  File logFile = SD.open(FILE_NAME, FILE_WRITE);
  
  //Write the CSV data
  logFile.println(rec);
  
  //Close the file to save it
  logFile.close();  
}

String createDataRecord()
{
  //Create a String type data record in csv format
  //TimeDate, TempSHT21, TempBMP, PressureBMP, HumiditySHT21
  String data = getDateTime() + ", ";
  data += String(SHT2x.GetTemperature())  + ", ";
  data += String(bmp.readTemperature()) + ", ";
  data += String(bmp.readPressure() / 100)  + ", ";
  data += String(SHT2x.GetHumidity());
  
  return data;
}

String createDataURL()
{
  //Construct data URL
  String url = URL;
 
  //Add key followed by each field
  url += String(FIRST_SEP) + String("key");
  url += String(LABEL_DATA_SEP) + String(WRITE_API_KEY);
  
  url += String(OTHER_SEP) + String(LABEL1);
  url += String(LABEL_DATA_SEP) + String(SHT2x.GetTemperature());
  
  url += String(OTHER_SEP) + String(LABEL2);
  url += String(LABEL_DATA_SEP) + String(bmp.readTemperature());
  
  url += String(OTHER_SEP) + String(LABEL3);
  url += String(LABEL_DATA_SEP) + String(bmp.readPressure() / 100);
  
  url += String(OTHER_SEP) + String(LABEL4);
  url += String(LABEL_DATA_SEP) + String(SHT2x.GetHumidity());

  return url;  
}

void sendURLData(String url)
{
  char result[20] = "";
  gprsbee.doHTTPGET(APN, APN_USERNAME, APN_PASSWORD, url.c_str(), result, sizeof(result));
  
  Serial.println("Received: " + String(result));
}

String getDateTime()
{
  String dateTimeStr;
  
  //Create a DateTime object from the current time
  DateTime dt(rtc.makeDateTime(rtc.now().getEpoch()));

  //Convert it to a String
  dt.addToString(dateTimeStr);
  
  return dateTimeStr;  
}

uint32_t getNow()
{
  return millis();
}
