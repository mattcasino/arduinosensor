#include <ScioSense_ENS160.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_AHTX0.h>
const int trigPin = 9;  
const int echoPin = 10; 
float duration, distance;  

const float distanceThresh = 2.0;

ScioSense_ENS160 ens160(ENS160_I2CADDR_0);
Adafruit_AHTX0 aht;
int tempC;
int humidity;

int val = 0;
bool motion = false;
void setup() {
	pinMode(trigPin, OUTPUT);  
	pinMode(echoPin, INPUT);  
  Serial.begin(9600);   // USB serial
  while( !Serial )
    delay(100);
  Serial.println("starting");
  if( !ens160.begin() ) {
    Serial.println("ENS failed\n");
    while(1);
  }
  ens160.setMode(ENS160_OPMODE_STD);
  delay(1000);
  ens160.measure(true);
  delay(1000);

  if( !aht.begin() ) {
    Serial.println("AHT failed\n");
    while(1);
  }
  Serial.println("Sensors initialized");
}

void loop() {

  //we want this to run faster than the other sensors
	digitalWrite(trigPin, LOW);  
	delayMicroseconds(2);  
	digitalWrite(trigPin, HIGH);  
	delayMicroseconds(10);  
	digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  int newDist = (duration*.0343)/2;  
  if( abs(newDist - distance) > distanceThresh )
    Serial.println("Motion Detected");
  distance = newDist;

  Serial.print("Distance: ");  
	Serial.println(distance);  
  // Read AHT
  sensors_event_t humidity_event, temp_event;
  aht.getEvent(&humidity_event, &temp_event);

  tempC = temp_event.temperature;
  humidity = humidity_event.relative_humidity;
  Serial.print("Humidity: ");
  Serial.println(humidity);
  Serial.print("Temperature: ");
  Serial.println(tempC);
  // Feed ENS160


  // Only read when ready
  if (ens160.available()) {
    ens160.set_envdata(tempC, humidity);
     ens160.measure(true);
    ens160.measureRaw(true);
    Serial.print("AQI: ");
    Serial.print(ens160.getAQI());
    Serial.print('\t');

    Serial.print("  TVOC: ");
    Serial.print(ens160.getTVOC());
    Serial.print("ppb\t");

    Serial.print("  eCO2: ");
    Serial.print(ens160.geteCO2());
    Serial.print("ppm\n");
  }
  delay(1000); // Wait 2 seconds before next reading
}
