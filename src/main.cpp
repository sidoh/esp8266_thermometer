#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ThermometerWebserver.h>
#include <FS.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <inttypes.h>
#include <TimeLib.h>
#include <NtpClientLib.h>
#include <WiFiManager.h>
#include <HmacHelpers.h>
#include <Settings.h>
#include <IntParsing.h>
#include <IndexPage.h>
#include <Stylesheet.h>
#include <Javascript.h>
#include <PubSubClient.h>
#include <TempIface.h>

WiFiClient client;
PubSubClient* mqttClient = NULL;
ThermometerWebserver* server = NULL;
OneWire* oneWire = NULL;
DallasTemperature* sensors = NULL;
Settings settings;
TempIface tempIface(sensors, settings);
time_t lastUpdate = 0;

enum class OperatingState { UNCHECKED, SETTINGS, NORMAL };
OperatingState operatingState = OperatingState::UNCHECKED;

ADC_MODE(ADC_TOUT);

time_t timestamp() {
  TimeChangeRule dstOn = {"DT", Second, dowSunday, Mar, 2, 60};
  TimeChangeRule dstOff = {"ST", First, dowSunday, Nov, 2, 0};
  Timezone timezone(dstOn, dstOff);

  return timezone.toLocal(NTP.getTime());
}

void updateTemperature(uint8_t* deviceId, float temp) {
  String deviceName = settings.deviceName(deviceId);

  StaticJsonBuffer<100> responseBuffer;
  JsonObject& response = responseBuffer.createObject();
  String body;

  response["temperature"] = temp;
  response["voltage"] = analogRead(A0);

  response.printTo(body);

  if (settings.sensorPaths.count(deviceName) > 0) {
    HTTPClient http;

    String sensorPath = settings.sensorPaths[deviceName];
    time_t now = timestamp();
    String url = String(settings.gatewayServer) + sensorPath;

    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    if (settings.hmacSecret) {
      String signature = requestSignature(settings.hmacSecret, sensorPath, body, now);
      http.addHeader("X-Signature-Timestamp", String(now));
      http.addHeader("X-Signature", signature);
    }

    http.sendRequest("PUT", body);
    http.end();
  }

  if (mqttClient != NULL) {
    String topic = settings.mqttTopic;
    topic += "/";
    topic += deviceName;
    topic.replace(" ", "_");

    mqttClient->publish(topic.c_str(), body.c_str());
  }
}

void startSettingsServer() {
  server = new ThermometerWebserver(tempIface, settings);
  server->begin();

  // server.on("/thermometers", HTTP_GET, []() {
  //   DynamicJsonBuffer buffer;
  //   JsonArray& thermometers = buffer.createArray();
  //   uint8_t addr[8];

  //   for (uint8_t i = 0; i < sensors->getDeviceCount(); ++i) {
  //     sensors->getAddress(addr, i);
  //     String deviceName = settings.deviceName(addr, false);
  //     thermometers.add(deviceName);
  //   }

  //   String body;
  //   thermometers.printTo(body);

  //   server.send(
  //     200,
  //     "application/json",
  //     body
  //   );
  // });

  // server.on("/settings", HTTP_GET, []() {
  //   server.send(200, "application/json", settings.toJson());
  // });

  // server.on("/settings", HTTP_POST, []() {
  //   if (isAuthenticated()) {
  //     Settings::deserialize(settings, server.arg("plain"));
  //     settings.save();

  //     server.sendHeader("Location", "/");
  //     server.send(302, "text/plain");
  //   }
  // });

  // server.on("/settings", HTTP_PUT, []() {
  //   if (isAuthenticated()) {
  //     DynamicJsonBuffer buffer;
  //     JsonObject& json = buffer.parseObject(server.arg("plain"));

  //     settings.patch(json);
  //     settings.save();

  //     server.send(200, "text/plain", "success");
  //   }
  // });

  // server.on("/signal_strength", HTTP_GET, []() {
  //   server.send(
  //     200,
  //     "text/plain",
  //     String(WiFi.RSSI())
  //   );
  // });

  // server.on("/update", HTTP_POST, [](){
  //   if (isAuthenticated()) {
  //     server.sendHeader("Connection", "close");
  //     server.sendHeader("Access-Control-Allow-Origin", "*");
  //     server.send(200, "text/plain", (Update.hasError())?"FAIL":"OK");
  //     ESP.restart();
  //   }
  // },[](){
  //   if (isAuthenticated()) {
  //     HTTPUpload& upload = server.upload();
  //     if(upload.status == UPLOAD_FILE_START){
  //       Serial.setDebugOutput(true);
  //       WiFiUDP::stopAll();
  //       uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
  //       if(!Update.begin(maxSketchSpace)){//start with max available size
  //         Update.printError(Serial);
  //       }
  //     } else if(upload.status == UPLOAD_FILE_WRITE){
  //       if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){
  //         Update.printError(Serial);
  //       }
  //     } else if(upload.status == UPLOAD_FILE_END){
  //       if(Update.end(true)){ //true to set the size to the current progress
  //         Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
  //       } else {
  //         Update.printError(Serial);
  //       }
  //       Serial.setDebugOutput(false);
  //     }
  //   }
  //   yield();
  // });

  // server.on("/voltage", HTTP_GET, []() {
  //   server.send(
  //     200,
  //     "text/plain",
  //     String(analogRead(A0))
  //   );
  // });

  // server.on("/pin", HTTP_PUT, []() {
  //   DynamicJsonBuffer buffer;
  //   const JsonObject& req = buffer.parse(server.arg("plain"));

  //   const uint8_t pin = req["pin"];
  //   const uint8_t val = req["value"];

  //   pinMode(pin, OUTPUT);
  //   digitalWrite(pin, val);

  //   server.send(200);
  // });
}

bool isDefined(String setting) {
  return setting && setting.length() > 0;
}

bool isSettingsMode() {
  if (operatingState != OperatingState::UNCHECKED) {
    return operatingState == OperatingState::SETTINGS;
  }

  if (settings.requiredSettingsDefined()) {
    WiFiClient client;

    if (client.connect(settings.flagServer.c_str(), settings.flagServerPort)) {
      Serial.println("Connected to flag server");
      String response = client.readString();

      if (response == "update") {
        operatingState = OperatingState::SETTINGS;
      }
    } else {
      Serial.println("Failed to connect to flag server");
      operatingState = OperatingState::NORMAL;
    }
  } else {
    operatingState = OperatingState::SETTINGS;
  }

  return operatingState == OperatingState::SETTINGS;
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  pinMode(A0, INPUT);

  Serial.println();
  Serial.println("Booting Sketch...");

  if (! SPIFFS.begin()) {
    Serial.println("Failed to initialize SPFFS");
  }

  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(180);

  char apName[50];
  sprintf(apName, "Thermometer_%d", ESP.getChipId());
  wifiManager.autoConnect(apName, "fireitup");

  if (!WiFi.isConnected()) {
    Serial.println("Timed out trying to connect, going to reboot");
    ESP.restart();
  }

  NTP.begin();

  Settings::load(settings);

  oneWire = new OneWire(settings.sensorBusPin);
  sensors = new DallasTemperature(oneWire);
  sensors->begin();

  tempIface.begin();

  if (settings.mqttServer.length() > 0) {
    char clientId[40];
    sprintf(clientId, "esp8266-thermometer-%u", ESP.getChipId());

    mqttClient = new PubSubClient(client);
    mqttClient->setServer(settings.mqttServer.c_str(), settings.mqttPort);
    mqttClient->connect(clientId, settings.mqttUsername.c_str(), settings.mqttPassword.c_str());
  }

  if (isSettingsMode()) {
    Serial.println("Entering settings mode");
    startSettingsServer();
  }
}

void sendUpdates() {
  uint8_t addr[8];
  for (uint8_t i = 0; i < sensors->getDeviceCount(); ++i) {
    sensors->getAddress(addr, i);
    sensors->requestTemperaturesByAddress(addr);

    updateTemperature(addr, sensors->getTempF(addr));
  }
}

void loop() {
  while (!NTP.getFirstSync()) {
    yield();
  }

  tempIface.loop();

  if (isSettingsMode() || settings.opMode == OperatingMode::ALWAYS_ON) {
    time_t n = now();

    if (n > (lastUpdate + settings.updateInterval)) {
      sendUpdates();
      lastUpdate = n;
    }
  } else {
    sendUpdates();

    Serial.println();
    Serial.println("closing connection. going to sleep...");

    delay(1000);

    ESP.deepSleep(settings.updateInterval * 1000000L, WAKE_RF_DEFAULT);
  }
}
