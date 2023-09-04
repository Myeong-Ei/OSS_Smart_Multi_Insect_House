#if !defined(ESP8266)
#error This code is designed to run on ESP8266 and ESP8266-based boards! Please check your Tools->Board setting.
#endif

#include <Wire.h>
#include <ESP8266WiFi.h>
#include <AHTxx.h>
#include "config.h"
#include "ESP8266TimerInterrupt.h"

#define HUMIDIFIER 2
#define SOIL_SENSOR A0

AdafruitIO_Feed *temp_val = io.feed("temperature_value");
AdafruitIO_Feed *temp_send = io.feed("temperature_send");
AdafruitIO_Feed *humi_val = io.feed("humidity_value");
AdafruitIO_Feed *humi_send = io.feed("humidity_send");
AdafruitIO_Feed *soil_val = io.feed("soil_moisture_value");
AdafruitIO_Feed *soil_send = io.feed("soil_moisture_send");
AdafruitIO_Feed *humidifier = io.feed("humidifier");

AHTxx aht10(AHTXX_ADDRESS_X38, AHT1x_SENSOR); //AHT15 Initializing


float ahtValue = 0, ahtTemp = 0, ahtHumi = 0;
int soilValue;
int std_temp, std_humi, std_soil, humidifier_state;

void setup() {
  pinMode(HUMIDIFIER, OUTPUT);
  Serial.begin(115200);

  digitalWrite(HUMIDIFIER, LOW);

  while (! Serial && millis() < 5000); // Serial Enable Checking

  //-----------------------------------AHT15 code---------------------------------------------

  while (aht10.begin() != true) //for ESP-01 use aht10.begin(0, 2);
  {
    Serial.println(F("AHT1x not connected or fail to load calibration coefficient")); //(F()) save string to flash & keeps dynamic memory free

    delay(5000);
  }

  Serial.println(F("AHT10 OK"));

  Serial.print("Connecting to Adafruit IO");

  //--------------------------------Adafruit IO code------------------------------------------

  io.connect();

  temp_send->onMessage(temp_message);
  humi_send->onMessage(humi_message);
  soil_send->onMessage(soil_message);
  humidifier->onMessage(humidifier_message);

  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  //connected
  Serial.println();
  Serial.println(io.statusText());

  temp_send->get();
  humi_send->get();
  soil_send->get();
  humidifier->get();

  //-------------------------------------------------------------------------------------------
  digitalWrite(HUMIDIFIER, LOW);
}


void loop() {
  io.run();
  measureSoilMoisture();
  measureTempHumi();
  sendTempHumiSoil();
  operateFarm();
  delay(2000);
}

void measureTempHumi() {
  Serial.println();
  //Serial.println(F("DEMO 1: read 12-bytes"));

  ahtValue = aht10.readTemperature(); //read 6-bytes via I2C, takes 80 milliseconds

  Serial.print(F("Temperature...: "));

  if (ahtValue != AHTXX_ERROR) //AHTXX_ERROR = 255, library returns 255 if error occurs
  {
    ahtTemp = ahtValue;
    Serial.print(ahtValue);
    Serial.println(F(" +-0.3C"));
  }
  else
  {
    printStatus(); //print temperature command status

    if   (aht10.softReset() == true) Serial.println(F("reset success")); //as the last chance to make it alive
    else                             Serial.println(F("reset failed"));
  }


  //==================================================================================
    delay(2000); //measurement with high frequency leads to heating of the sensor, see NOTE
  //==================================================================================

  ahtValue = aht10.readHumidity(); //read another 6-bytes via I2C, takes 80 milliseconds

  Serial.print(F("Humidity......: "));

  if (ahtValue != AHTXX_ERROR) //AHTXX_ERROR = 255, library returns 255 if error occurs
  {
    ahtHumi = ahtValue;
    Serial.print(ahtValue);
    Serial.println(F(" +-2%"));
  }
  else
  {
    printStatus(); //print humidity command status
  }


  //==================================================================================
    delay(2000); //measurement with high frequency leads to heating of the sensor, see NOTE
  //==================================================================================

  /* DEMO - 2, temperature call will read 6-bytes via I2C, humidity will use same 6-bytes */

  Serial.println();
  //Serial.println(F("DEMO 2: read 6-byte"));

  ahtValue = aht10.readTemperature(); //read 6-bytes via I2C, takes 80 milliseconds

  Serial.print(F("Temperature: "));

  if (ahtValue != AHTXX_ERROR) //AHTXX_ERROR = 255, library returns 255 if error occurs
  {
    ahtTemp = ahtValue;
    Serial.print(ahtValue);
    Serial.println(F(" +-0.3C"));
  }
  else
  {
    printStatus(); //print temperature command status
  }

  ahtValue = aht10.readHumidity(AHTXX_USE_READ_DATA); //use 6-bytes from temperature reading, takes zero milliseconds!!!

  Serial.print(F("Humidity...: "));

  if (ahtValue != AHTXX_ERROR) //AHTXX_ERROR = 255, library returns 255 if error occurs
  {
    ahtHumi = ahtValue;
    Serial.print(ahtValue);
    Serial.println(F(" +-2%"));
  }
  else
  {
    printStatus(); //print temperature command status not humidity!!! RH measurement use same 6-bytes from T measurement
  }

  //==================================================================================
    delay(2000); //recomended polling frequency 8sec..30sec
  //==================================================================================
}

void printStatus()
{
  switch (aht10.getStatus())
  {
    case AHTXX_NO_ERROR:
      Serial.println(F("no error"));
      break;

    case AHTXX_BUSY_ERROR:
      Serial.println(F("sensor busy, increase polling time"));
      break;

    case AHTXX_ACK_ERROR:
      Serial.println(F("sensor didn't return ACK, not connected, broken, long wires (reduce speed), bus locked by slave (increase stretch limit)"));
      break;

    case AHTXX_DATA_ERROR:
      Serial.println(F("received data smaller than expected, not connected, broken, long wires (reduce speed), bus locked by slave (increase stretch limit)"));
      break;

    case AHTXX_CRC8_ERROR:
      Serial.println(F("computed CRC8 not match received CRC8, this feature supported only by AHT2x sensors"));
      break;

    default:
      Serial.println(F("unknown status"));
      break;
  }
}

void sendTempHumiSoil() {
  temp_val->save(ahtTemp);
  humi_val->save(ahtHumi);
  soil_val->save(soilValue);
}

void measureSoilMoisture() {
  soilValue = analogRead(SOIL_SENSOR);
  Serial.print("Soil Moisture Value : ");
  Serial.println(soilValue);
}



void temp_message(AdafruitIO_Data *data) {
  std_temp = data->toInt();
  Serial.print("Standard Temperature : ");
  Serial.println(std_temp);
}

void humi_message(AdafruitIO_Data *data) {
  std_humi = data->toInt();
  Serial.print("Standard Humidity : ");
  Serial.println(std_humi);
}

void soil_message(AdafruitIO_Data *data) {
  std_soil = data->toInt();
  Serial.print("Standard Soil Moisture : ");
  Serial.println(std_soil);
}

void humidifier_message(AdafruitIO_Data *data) {
  humidifier_state = data->toPinLevel();
  Serial.print("Humidifier Control Switch : ");
  Serial.println(humidifier_state);
  digitalWrite(HUMIDIFIER, humidifier_state);
}

void operateFarm(){
  if(ahtTemp > std_temp && ahtHumi < std_humi && soilValue < std_soil){
    digitalWrite(HUMIDIFIER, LOW);
  }
  //-------------------------------------------
  else if(ahtTemp < std_temp && ahtHumi > std_humi && soilValue > std_soil){
    digitalWrite(HUMIDIFIER, HIGH);
  }
}
