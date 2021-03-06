#include <FS.h>
#include <ArduinoJson.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include "thermalcam.pb.h"

#include <MLX90640_API.h>
#include <MLX90640_I2C_Driver.h>
#include <SparkFun_GridEYE_Arduino_Library.h>
#include <Wire.h>

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <MQTT.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

#define DATA_INTERVAL 5 * 1000 //in millis
#define TEMP_INTERVAL 60 * 1000
#define TOPIC_PREFIX "SONY/"
#define TOPIC_DEVICE "/sensor/thermal"
#define MQTT_RETAINED true
#define MQTT_QOS 1

char mqtt_server[40] = "192.168.1.1";
char room_uuid[40] = "xxxxxxxx-xxxx-4xxx-xxxx-xxxxxxxxxxxx";

GridEYE grideye;

uint8_t pb_buffer[1024];
bool pb_status;
float pixels[64];
const size_t capacity = JSON_ARRAY_SIZE(64) + 350;

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
    if (SPIFFS.exists("/config")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t configfile_size = configFile.size();

        /* allocate space for decode config. */
        sony_MQTTConfig config = sony_MQTTConfig_init_default;

        /* read config into buffer. */
        configFile.read(pb_buffer, configfile_size);
        pb_istream_t stream = pb_istream_from_buffer(pb_buffer, configfile_size);

        pb_status = pb_decode(&stream, sony_MQTTConfig_fields, &config);

        /* check for error. */
        if (!pb_status) {
          Serial.print("failed to decode config file!");
        } else {                /* print the config content. */
          Serial.print("ip: ");
          Serial.println(config.ip);
          strcpy(mqtt_server, config.ip);

          Serial.print("port: ");
          Serial.println(config.port);
          Serial.print("uuid: ");
          Serial.println(config.uuid);
          strcpy(room_uuid, config.uuid);
        }

        configFile.close();
      }
    } else {
      Serial.println("config file does not exist.");
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

#ifdef SETUP_SSID
  wifiManager.autoConnect(SETUP_SSID, SETUP_PASSWORD);
#else
  wifiManager.autoConnect();
#endif

  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(room_uuid, custom_room_uuid.getValue());

  //save the custom parameters to FS
  Serial.println("saving config");
  sony_MQTTConfig config = sony_MQTTConfig_init_default;
  pb_ostream_t stream = pb_ostream_from_buffer(pb_buffer, sizeof(pb_buffer));
  strcpy(config.ip, mqtt_server);
  strcpy(config.uuid, room_uuid);

  /* encode config setting */
  pb_status = pb_encode(&stream, sony_MQTTConfig_fields, &config);

  if (!pb_status) {
    Serial.println("Encoding failed\n");
  } else {                      /* write to file when encode success */
    File configFile = SPIFFS.open("/config", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }
    configFile.write(pb_buffer, stream.bytes_written);
    Serial.println("config saved.\n");
    configFile.close();
    //end save
  }



  /* hostname will be grideye.local */
  MDNS.begin("grideye");

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  client.begin(mqtt_server, net);

  Wire.begin();
  grideye.begin();

  delay(100); // let sensor boot up
}


void loop() {
  ArduinoOTA.handle();

  client.loop();
  delay(10);

  if (!client.connected()) {
    connect();
  }

  if (millis() - lastRawMillis > DATA_INTERVAL) {
    StaticJsonBuffer<capacity> jsonBuffer;
    JsonArray& root = jsonBuffer.createArray();

    for(unsigned char i = 0; i < 64; i++) {
      pixels[i] = grideye.getPixelTemperature(i);
    }
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
                  String(grideye.getDeviceTemperature()), MQTT_RETAINED, MQTT_QOS);
  }
}
