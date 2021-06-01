#include <SPIFFS.h>
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager       version = 2.0.3-alpha in /lib
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>

WiFiManager wifiManager;
WiFiClient espClient;
PubSubClient client(espClient);

void saveConfigCallback();
void loadParameters();
void saveParameters();
void reconnect();
void callback(char *topic, byte *payload, unsigned int length);

#define LED 25
#define MSG_BUFFER_SIZE (50)
#define NUMPIXELS 12

char devID[10];
char PortalName[20];

char mqtt_Server[20];
char sendPort[6];

char msg[MSG_BUFFER_SIZE];

uint32_t chipId = 0;
unsigned long lastMsg = 0;
unsigned long value = 0;
uint32_t color;
int r = 0;
int g = 0;
int b = 0;

bool shouldSaveConfig = true; //flag for saving data / erase data

Adafruit_NeoPixel ringLed(NUMPIXELS, LED, NEO_GRB + NEO_KHZ800);

void setup()
{
  Serial.begin(115200);
  pinMode(LED, OUTPUT);

  for (int i = 0; i < 17; i = i + 8)
  {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }

  //wifiManager.resetSettings();
  //SPIFFS.format();

  snprintf(PortalName, sizeof(PortalName), "IoTlights_%d", chipId);

  WiFi.setHostname(PortalName);
  Serial.println(PortalName);

  Serial.println("mounting FS...");
  SPIFFS.begin(true);

  loadParameters();

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_devID("devID", "Device ID", devID, 5);
  WiFiManagerParameter custom_mqtt_Server("mqtt_server", "mqtt_server", mqtt_Server, 20);
  WiFiManagerParameter custom_sendPort("sendPort", "mqtt_port 443", sendPort, 6);

  //add all your parameters here
  wifiManager.addParameter(&custom_devID);
  wifiManager.addParameter(&custom_mqtt_Server);
  wifiManager.addParameter(&custom_sendPort);

  wifiManager.setShowInfoErase(false);
  wifiManager.setConfigPortalTimeout(180);

  if (!wifiManager.autoConnect(PortalName, "EnterThis"))
  {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...");

  //read updated parameters
  strcpy(devID, custom_devID.getValue());
  strcpy(mqtt_Server, custom_mqtt_Server.getValue());
  strcpy(sendPort, custom_sendPort.getValue());

  if (shouldSaveConfig)
  {
    Serial.println("saving config");
    saveParameters();
  }

  Serial.println("local ip  ");
  Serial.print(WiFi.localIP());
  Serial.println();
  Serial.println("Done");

  client.setServer("mqtt.devlol.org", 1883);
  client.setCallback(callback);

  ringLed.begin();
}

void loop()
{
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  for (int i = 0; i < NUMPIXELS; i++)
  {
    ringLed.setPixelColor(i, color);
  }
  ringLed.show();
}

void saveConfigCallback()
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void loadParameters()
{
  if (SPIFFS.begin())
  {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json"))
    {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile)
      {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);

        StaticJsonDocument<140> doc;

        DeserializationError error = deserializeJson(doc, buf.get());

        if (error)
        {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          return;
        }

        const char *devIDjsn = doc["devID"];        // "MotXi1"
        const char *HostIPjsn = doc["mqtt_Server"]; // "192.168.0.8"
        const char *sendPortjsn = doc["sendPort"];  // "443"

        strcpy(devID, devIDjsn);
        strcpy(mqtt_Server, HostIPjsn);
        strcpy(sendPort, sendPortjsn);

        serializeJson(doc, Serial);

        Serial.println(devID);
        Serial.println(mqtt_Server);
        Serial.println(sendPort);

        Serial.println("\nparsed json");
        configFile.close();
      }
    }
    else
    {
      Serial.println("no Json Data!!");
    }
  }
  else
  {
    Serial.println("failed to mount FS");
    SPIFFS.format();
  }
}

void saveParameters()
{

  StaticJsonDocument<140> doc;

  doc["devID"] = devID;
  doc["mqtt_Server"] = mqtt_Server;
  doc["sendPort"] = sendPort;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile)
  {
    Serial.println("failed to open config file for writing");
  }
  serializeJson(doc, configFile);
  delay(10);
  configFile.close();
  Serial.println("Saved Data to FS");
  serializeJson(doc, Serial);
  shouldSaveConfig = false;
}

void callback(char *topic, byte *payload, unsigned int length)
{
  char msgIn[15];
  int count = 0;

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
    msgIn[i] = (char)payload[i];
    count++;
  }
  msgIn[count + 1] = '/0';
  Serial.println();

  color = strtoul(msgIn + 1, 0, 16);
}

void reconnect()
{
  // Loop until we're reconnected
  Serial.println("reconnect");

  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    // devID += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(PortalName))
    {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("devlol/IoTlights", PortalName);
      // ... and resubscribe
      client.subscribe("devlol/IoTlights/color");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
