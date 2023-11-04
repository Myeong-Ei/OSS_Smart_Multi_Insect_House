#include <EEPROM.h>
#include <Ticker.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <AHTxx.h>

#define HUMIDIFIER 15
#define SOIL_SENSOR A0
#define BUFF_SIZE 50

unsigned long lastMsg = 0;

const char* ssid = "SK_WiFiGIGAE52A_2.4G";
const char* password = "BGWB7@6319";
const char* mqtt_server = "broker.mqtt-dashboard.com";

float ahtValue = 0, ahtTemp = 0, ahtHumi = 0;
int soilValue;
int std_temp, std_humi, std_soil;

uint8_t sensor_state = 1;                           // 초기값 : 1, 1~3 범위

char temp_buf[50];
char humi_buf[50];
char soil_buf[50];

AHTxx aht10(AHTXX_ADDRESS_X38, AHT1x_SENSOR); //AHT15 Initializing
Ticker timer;

WiFiClient espClient;
PubSubClient client(espClient);

void operate_humi() {
  
  
  if (std_temp > ahtTemp && std_humi < ahtHumi) {
    Serial.println(F("Humidifier can't work now!"));
    digitalWrite(HUMIDIFIER, LOW);
  }
  else {
    Serial.println(F("Tickle Timer Disable!"));
    timer.detach();  // Tickle Timer Disable
    
    digitalWrite(HUMIDIFIER, HIGH);
    Serial.print(F("Humidifier turnning ON\n"));
    delay(5000);
    digitalWrite(HUMIDIFIER, LOW);

    Serial.println(F("Tickle Timer Enable!"));
    sensor_state = 1;
    timer.attach_ms(2000, measureTempHumi);  // Tickle Timer Enable
  }

  
}

//----------------------------------MQTT Connection----------------------------
void setup_wifi() {

  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
 
  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  String myTopic = topic;
  String myPayload = (char *)payload;

  Serial.print("Message arrived [");
  Serial.print(myTopic);
  Serial.print("] :");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  //--------------------------Topic, Payload-------------------------------

  if (myTopic.equals("MyeongEi/OSS/Temp/in")) {
    EEPROM.write(0, (byte)myPayload.toInt());
    std_temp = myPayload.toInt();
    Serial.print("\nStandard Temp : ");
    Serial.println(EEPROM.read(0));
  }
  else if (myTopic.equals("MyeongEi/OSS/Humi/in")) {
    EEPROM.write(1, (byte)myPayload.toInt());
    std_humi = myPayload.toInt();
    Serial.print("\nStandard Humi : ");
    Serial.println(EEPROM.read(1));
  }
  else if (myTopic.equals("MyeongEi/OSS/Soil_Moisture/in")) {
    EEPROM.write(2, (byte)myPayload.toInt());
    std_soil = myPayload.toInt();
    Serial.print("\nStandard Soil Moisture : ");
    Serial.println(EEPROM.read(2));
  }
  else if (myTopic.equals("MyeongEi/OSS/Humidifier/in") && payload[0] == '1') {
    operate_humi();
  }
}

//-----------------------------------------------------------------------

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.publish("outTopic", "hello world");

      //------------Subscribe area----------------

      client.subscribe("MyeongEi/OSS/Temp/in");
      client.subscribe("MyeongEi/OSS/Humi/in");
      client.subscribe("MyeongEi/OSS/Soil_Moisture/in");
      client.subscribe("MyeongEi/OSS/Humidifier/in");

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}
//================================================================================

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

void measureTempHumi() {
  if (sensor_state == 1) {

    /* DEMO - 1, every temperature or humidity call will read 6-bytes over I2C, total 12-bytes */
    Serial.println();
    Serial.println(F("DEMO 1: read 12-bytes"));

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

    sensor_state++;
    timer.attach_ms(2000, measureTempHumi);
  }

  else if (sensor_state == 2) {

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

    sensor_state++;
    timer.attach_ms(2000, measureTempHumi);
  }

  else {
    timer.attach_ms(500, measureTempHumi);
    /* DEMO - 2, temperature call will read 6-bytes via I2C, humidity will use same 6-bytes */
    Serial.println();
    Serial.println(F("DEMO 2: read 6-byte"));

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

    sensor_state = 1;
    timer.attach_ms(8000, measureTempHumi);
  }
}

void measureSoilMoisture() {
  soilValue = map(analogRead(SOIL_SENSOR), 0, 1023, 0, 100);
  Serial.print("Soil Moisture Value : ");
  Serial.println(soilValue);
}

void operateFarm() {
  if (ahtTemp > std_temp && ahtHumi < std_humi) {
    digitalWrite(HUMIDIFIER, HIGH);
  }
  else if (ahtTemp < std_temp && ahtHumi > std_humi) {
    digitalWrite(HUMIDIFIER, LOW);
  }
}

void setup() {
  pinMode(HUMIDIFIER, OUTPUT);
  Serial.begin(115200);

  EEPROM.begin(4096);    // 0 : Temperature
                         // 1 : Humidity
                         // 2 : Soil Moisture

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  digitalWrite(HUMIDIFIER, LOW);

  while (! Serial && millis() < 5000);

  //-----------------------------------AHT15 code---------------------------------------------

  while (aht10.begin() != true)
  {
    Serial.println(F("AHT1x not connected or fail to load calibration coefficient")); //(F()) save string to flash & keeps dynamic memory free

    delay(5000);
  }

  Serial.println(F("AHT10 OK"));
  sensor_state = 1;
  timer.attach_ms(2000, measureTempHumi);
  

  //------------------------------------------------------------------------------------------

  std_temp = EEPROM.read(0);
  std_humi = EEPROM.read(1);
  std_soil = EEPROM.read(2);
}



void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  operateFarm();

  unsigned long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    
    
    measureSoilMoisture();

    
    //-------------------Publish area--------------------
    snprintf(temp_buf, BUFF_SIZE, "%.1f", ahtTemp);
    snprintf(humi_buf, BUFF_SIZE, "%.1f", ahtHumi);
    snprintf(soil_buf, BUFF_SIZE, "%d", soilValue);
    client.publish("MyeongEi/OSS/Temp/out", temp_buf);
    client.publish("MyeongEi/OSS/Humi/out", humi_buf);
    client.publish("MyeongEi/OSS/Soil_Moisture/out", soil_buf);
    //---------------------------------------------------

    Serial.print("\nStandard Temp : ");
    Serial.println(std_temp);
    Serial.print("Standard Humi : ");
    Serial.println(std_humi);
    Serial.print("Standard Soil Moisture : ");
    Serial.println(std_soil);
    Serial.print("\n");
  }
}
