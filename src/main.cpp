#include <SPIFFS.h>
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager       version = 2.0.3-alpha
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>

WiFiManager wifiManager;

void saveConfigCallback();
void loadParameters();
void saveParameters();

char devID[10];
char PortalName[20];

char mqtt_Server[20];
char sendPort[6];

uint32_t chipId = 0;
bool shouldSaveConfig = true; //flag for saving data / erase data

void setup()
{
  Serial.begin(115200);
  pinMode(5, OUTPUT);

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
  WiFiManagerParameter custom_sendPort("Port", "mqtt_port 443", sendPort, 6);

  //add all your parameters here
  wifiManager.addParameter(&custom_devID);
  wifiManager.addParameter(&custom_mqtt_Server);
  wifiManager.addParameter(&custom_sendPort);

  wifiManager.setShowInfoErase(false);
  wifiManager.setConfigPortalBlocking(false);

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
}

void loop()
{
  // put your main code here, to run repeatedly:
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