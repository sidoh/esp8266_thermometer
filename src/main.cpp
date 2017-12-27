#include <inttypes.h>

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ThermometerWebserver.h>
#include <FS.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <TimeLib.h>
#include <NtpClientLib.h>
#include <WiFiManager.h>
#include <HmacHelpers.h>
#include <Settings.h>
#include <IntParsing.h>
#include <TempIface.h>
#include <MqttClient.h>
#include <ESP8266HTTPClient.h>

MqttClient* mqttClient = NULL;
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
  String strDeviceId = settings.deviceName(deviceId, false);

  StaticJsonBuffer<100> responseBuffer;
  JsonObject& response = responseBuffer.createObject();
  String body;

  response["temperature"] = temp;
  response["voltage"] = analogRead(A0);

  response.printTo(body);

  if (settings.sensorPaths.count(strDeviceId) > 0) {
    HTTPClient http;

    String sensorPath = settings.sensorPaths[strDeviceId];
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

    mqttClient->sendUpdate(deviceName, body.c_str());
  }
}

void startSettingsServer() {
  server = new ThermometerWebserver(tempIface, settings);
  server->begin();
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

  if (settings._mqttServer.length() > 0) {
    mqttClient = new MqttClient(settings);
    mqttClient->begin();
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

  if (mqttClient) {
    mqttClient->handleClient();
  }
}
