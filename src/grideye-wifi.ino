#include <FS.h>
#include <ArduinoJson.h>

#include <Wire.h>
#include <Adafruit_AMG88xx.h>

#include <ESP8266WiFi.h>
#include <MQTT.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

#define DATA_INTERVAL 5 * 1000 //in millis
#define TEMP_INTERVAL 60 * 1000
#define TOPIC_PREFIX "SONY/"
#define TOPIC_DEVICE "/sensor/grideye"
#define MQTT_RETAINED true
#define MQTT_QOS 1

char mqtt_server[40] = "192.168.1.1";
char room_uuid[40] = "xxxxxxxx-xxxx-4xxx-xxxx-xxxxxxxxxxxx";

Adafruit_AMG88xx amg;

float pixels[AMG88xx_PIXEL_ARRAY_SIZE];
const size_t capacity = JSON_ARRAY_SIZE(AMG88xx_PIXEL_ARRAY_SIZE) + 350;

WiFiClient net;
MQTTClient client(512); //set enough buffer for the 8x8 payload.
char payload[2048];

unsigned long lastRawMillis = 0;
unsigned long lastTempMillis = 0;

void connect() {
  Serial.print("\nconnecting...");
  while (!client.connect("")) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nconnected!");
}

void setup() {
  Serial.begin(115200);

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(room_uuid, json["room_uuid"]);

        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
  //
  WiFiManagerParameter custom_mqtt_server("server", "MQTT Server", mqtt_server, 40);
  WiFiManagerParameter custom_room_uuid("uuid", "Room UUID", room_uuid, 40);
  WiFiManager wifiManager;

  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_room_uuid);

  wifiManager.autoConnect();
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(room_uuid, custom_room_uuid.getValue());

  //save the custom parameters to FS
  Serial.println("saving config");
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["mqtt_server"] = mqtt_server;
  json["room_uuid"] = room_uuid;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("failed to open config file for writing");
  }

  json.printTo(Serial);
  json.printTo(configFile);
  configFile.close();
  //end save

  client.begin(mqtt_server, net);

  // default settings
  if (!amg.begin()) {
    Serial.println("Could not find a valid AMG88xx sensor, check wiring!");
    while (1);
  }

  delay(100); // let sensor boot up
}


void loop() {
  client.loop();
  delay(10);

  if (!client.connected()) {
    connect();
  }

  if (millis() - lastRawMillis > DATA_INTERVAL) {
    StaticJsonBuffer<capacity> jsonBuffer;
    JsonArray& root = jsonBuffer.createArray();

    //read all the pixels
    amg.readPixels(pixels);

    root.copyFrom(pixels);

    root.printTo(payload);
    lastRawMillis = millis();
    client.publish(TOPIC_PREFIX + String(room_uuid) +
                  "/status" + TOPIC_DEVICE + "/raw", payload);
  }
  if (millis() - lastTempMillis > TEMP_INTERVAL) {
    lastTempMillis = millis();
    client.publish(TOPIC_PREFIX + String(room_uuid) +
                  "/status" + TOPIC_DEVICE + "/temperature",
                  String(amg.readThermistor()), MQTT_RETAINED, MQTT_QOS);
  }
}
