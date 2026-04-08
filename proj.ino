#include <ScioSense_ENS160.h>
#include <Wire.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
const int trigPin = 9;  
const int echoPin = 10; 
float duration, distance;  

const int distanceThresh = 2.0;
bool motion = false;

//used for json parsing
#define JSON_BUF_SIZE 128 //this is the hard limit of information we are allowed to send
StaticJsonDocument<JSON_BUF_SIZE> doc;

char jsonBuffer[JSON_BUF_SIZE];

//the ens160 and aht sensors are located on the same chip using the magic of I2C
ScioSense_ENS160 ens160(ENS160_I2CADDR_0);

const int DHTPIN = 4;

DHT dht(DHTPIN, DHT22);

float tempC;
float humidity;


//these intervals tell us when we should send data. When the 
const unsigned long sonicInterval = 100; //how often the ultrasonic Sensor polls
const unsigned long lcdInterval = 1000; //how often the lcd screen updates
const unsigned long sensorInterval = 5000; //how often the sensors update
const unsigned long sendInterval = 10000; //how often the esp-01s sends data

//these store the last time the part updated in ms. When the value is greater than the interval, it means that we should update
unsigned long lastSonic = 0;
unsigned long lastSens = 0;
unsigned long lastSend = 0;
unsigned long lastLCD = 0;


LiquidCrystal_I2C lcd(0x27, 16, 2); //onboard display



SoftwareSerial esp(2, 3); // TX, RX. We directly interface with the esp-01s through a command interface


//cheap little hack to save on ram, much help from https://forum.arduino.cc/t/string-or-not-to-string-or-how-the-hell-do-i-not-use-string/629554/4
#define SENDCMD(x, y, ...) do {\
  char _buf[64];\
  snprintf(_buf, sizeof(_buf), x, ##__VA_ARGS__)  ;\
  sendCommand(_buf, y);\
} while(0);
//sends a command, blocking for a certain amount of time before continuing
void sendCommand(const char* cmd, int delayTime) {
  esp.println(cmd);
  delay(delayTime);
  while (esp.available()) {
    Serial.write(esp.read());
  }
}
void setup() {
	pinMode(trigPin, OUTPUT);  
	pinMode(echoPin, INPUT);
  Serial.begin(9600);   // USB serial
  esp.begin(9600);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Starting");

  while( !Serial )
    delay(100);
  Serial.println("starting");
  if( !ens160.begin() ) {
    lcd.setCursor(0,0);
    lcd.print(F("ENS FAILED"));

    Serial.println(F("ENS failed\n"));
    while(1);
  }
  ens160.setMode(ENS160_OPMODE_STD);
  delay(1000);
  ens160.measure(true);
  delay(1000);

  dht.begin();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(F("SENSORS GOOD"));
  lcd.setCursor(0,1);
  lcd.print(F("STARTING WIFI"));
  Serial.println(F("Sensors initialized"));
  SENDCMD("AT", 1000); // station mode
  SENDCMD("AT+CWMODE=1", 1000); // station mode
  SENDCMD("AT+CWLAP", 5000);
  SENDCMD("AT+CWJAP=\"M CASINO\",\"urmom123\"", 15000); //log into the internet (my hotspot)
  SENDCMD("AT+CIFSR", 3000); // print ESP IP
  SENDCMD("AT+CIPSTART=\"TCP\",\"172.20.10.3\",5000", 3000);

  SENDCMD("AT+CIPSEND=7", 1000);
  esp.print("hella\r\n");
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(F("READY TO SEND"));
}

void loop() {
  unsigned long now = millis();
  //while (esp.available()) {
  //  Serial.write(esp.read());
  //}
  //we want this to run faster than the other sensors
  //The sonic sensor runs every 100 ms, and 
  if(now - lastSonic >= sonicInterval) {
    digitalWrite(trigPin, LOW);  
	  delayMicroseconds(2);  
	  digitalWrite(trigPin, HIGH);  
	  delayMicroseconds(10);  
	  digitalWrite(trigPin, LOW);
    duration = pulseIn(echoPin, HIGH, 30000);
    if(duration != 0) { //sometimes, pulsein can take too long.
      long newDist = (duration*.0343)/2; //get the value of the sensor in centimeters
      if( abs(newDist - distance) >= distanceThresh) //if the new distance and the old distance have a difference of over the threshold, then some motion must have occurred
        motion = true;
      distance = newDist;

    }
    lastSonic = now; //update the timer
  }
  if(now - lastLCD >= lcdInterval) {
    lcd.clear();
    lcd.print(F("M "));
    lcd.print(F("T  "));
    lcd.print(F("H  "));
    lcd.setCursor(0, 1);
    lcd.print(motion);
    lcd.setCursor(2, 1);
    lcd.print(int(tempC));
    lcd.setCursor(5, 1);
    lcd.print(int(humidity));
    lastLCD = now;
  }
  if(now - lastSens >= sensorInterval) {
    humidity = dht.readHumidity();
    tempC = dht.readTemperature();
    if (ens160.available()) { //the ens sensor is finicky and isn't always ready to send data for some reason
      ens160.set_envdata(tempC, humidity); //set some variables so that the ens is a bit more accurate. This setup is technically not optimal
                                          //and i should be using the internal AHT sensor to set these values, but i need save on ram.
      ens160.measure(true);
      ens160.measureRaw(true);
    }
    lastSens = now;
  }
  // Read AHT
  if(now - lastSend >= sendInterval) {
    doc["humidity"] = humidity;
    doc["temperatureC"] = tempC;
    // Only read when ready
    doc["motion"] = motion;
    motion = false; //data has been sent, reset motion
    doc["AQI"] = ens160.getAQI();

    doc["TVOC"] = ens160.getTVOC();

    doc["eCO2"] = ens160.geteCO2();

    size_t n = serializeJson(doc, jsonBuffer, sizeof(jsonBuffer) - 1); //create the JSON element from the information within doc
    jsonBuffer[n] = '\n';

    SENDCMD("AT+CIPSEND=%i", 2000, n+1); //tell ESP-01S what we are about to send
    esp.write((uint8_t*)jsonBuffer, n+1); //write in the whole buffer of info
    Serial.println(jsonBuffer);
    lastSend = now;
  }
}
