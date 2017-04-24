#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <inttypes.h>
#include <TimeLib.h>
#include <NtpClientLib.h>
#include <WiFiManager.h>
#include <HmacHelpers.h>
#include <Settings.h>

ESP8266WebServer server(80);
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature sensors(&oneWire);
WiFiManager wifiManager;
Settings settings;

enum OperatingMode { UNCHECKED, SETTINGS_MODE, NORMAL_MODE };
OperatingMode operatingMode = UNCHECKED;

ADC_MODE(ADC_TOUT);

float readTemperature() {
  sensors.requestTemperatures();
  return sensors.getTempFByIndex(0);
}

time_t timestamp() {
  TimeChangeRule dstOn = {"DT", Second, dowSunday, Mar, 2, 60};
  TimeChangeRule dstOff = {"ST", First, dowSunday, Nov, 2, 0};
  Timezone timezone(dstOn, dstOff);
  
  return timezone.toLocal(NTP.getTime());
}

bool isAuthenticated() {
  String username = settings.adminUsername;
  String password = settings.adminPassword;
  
  if (username && password && username.length() > 0 && password.length() > 0) {
    if (!server.authenticate(username.c_str(), password.c_str())) {
      server.requestAuthentication();
      return false;
    }
  }
  
  return true;
}

void startSettingsServer() {
  server.on("/", HTTP_GET, []() {
    if (isAuthenticated()) {
      String body = "<html>";
      body += "<body>";
      body += "<h1>ESP8266 - ";
      body += String(ESP.getChipId(), HEX);
      body += "</h1>";
      body += "<h3>Settings</h3>";
      
      body += "<form method='post' action='/settings'>";
      body += "<div>";
      body += "<textarea name='settings' rows='20' cols='80' style='font-family: courier new, courier, mono;'>";
      body += settings.toJson();
      body += "</textarea>";
      body += "</div>";
      body += "<p><input type='submit'/></p></form>";
      
      body += "<h3>Update Firmware</h3>";
      body += "<form method='post' action='/update' enctype='multipart/form-data'>";
      body += "<input type='file' name='update'><input type='submit' value='Update'></form>";
      
      body += "</body></html>";
      
      server.send(
        200,
        "text/html",
        body
      );
    }
  });
  
  server.on("/temperature", HTTP_GET, []() {
    server.send(
      200,
      "text/plain",
      String(readTemperature())
    );
  });
  
  server.on("/settings", HTTP_POST, []() {
    if (isAuthenticated()) {
      Settings::deserialize(settings, server.arg("settings"));
      settings.save();
      
      server.sendHeader("Location", "/");
      server.send(302, "text/plain");
      
      ESP.restart();
    }
  });
  
  server.on("/signal_strength", HTTP_GET, []() {
    server.send(
      200,
      "text/plain",
      String(WiFi.RSSI())
    );
  });
  
  server.on("/update", HTTP_POST, [](){
    if (isAuthenticated()) {
      server.sendHeader("Connection", "close");
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(200, "text/plain", (Update.hasError())?"FAIL":"OK");
      ESP.restart();
    }
  },[](){
    if (isAuthenticated()) {
      HTTPUpload& upload = server.upload();
      if(upload.status == UPLOAD_FILE_START){
        Serial.setDebugOutput(true);
        WiFiUDP::stopAll();
        Serial.printf("Update: %s\n", upload.filename.c_str());
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if(!Update.begin(maxSketchSpace)){//start with max available size
          Update.printError(Serial);
        }
      } else if(upload.status == UPLOAD_FILE_WRITE){
        if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){
          Update.printError(Serial);
        }
      } else if(upload.status == UPLOAD_FILE_END){
        if(Update.end(true)){ //true to set the size to the current progress
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
        Serial.setDebugOutput(false);
      }
    }
    yield();
  });
  
  server.on("/voltage", HTTP_GET, []() {
    server.send(
      200,
      "text/plain",
      String(analogRead(A0))
    );
  });
  
  server.on("/pin", HTTP_PUT, []() {
    DynamicJsonBuffer buffer;
    const JsonObject& req = buffer.parse(server.arg("plain"));
    
    const uint8_t pin = req["pin"];
    const uint8_t val = req["value"];
    
    pinMode(pin, OUTPUT);
    digitalWrite(pin, val);
    
    server.send(200);
  });
  
  server.begin();
}

bool isDefined(String setting) {
  return setting && setting.length() > 0;
}

bool isSettingsMode() {
  if (operatingMode != UNCHECKED) {
    return operatingMode == SETTINGS_MODE;
  }
  
  if (settings.requiredSettingsDefined()) {
    WiFiClient client;
    
    if (client.connect(settings.flagServer.c_str(), settings.flagServerPort)) {
      Serial.println("Connected to flag server");
      String response = client.readString();
      
      if (response == "update") {
        operatingMode = SETTINGS_MODE;
      }
    } else {
      Serial.println("Failed to connect to flag server");
      operatingMode = NORMAL_MODE;
    }
  } else {
    operatingMode = SETTINGS_MODE;
  } 
  
  return operatingMode == SETTINGS_MODE;
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
  
  sensors.begin();
  wifiManager.autoConnect();
  wifiManager.setConfigPortalTimeout(180);
  
  if (!WiFi.isConnected()) {
    Serial.println("Timed out trying to connect, going to reboot");
    ESP.restart();
  }
  
  NTP.begin();
  
  Settings::load(settings);
  
  if (isSettingsMode()) {
    Serial.println("Entering settings mode");
    startSettingsServer();
  }
}
 
void loop() {
  if (isSettingsMode()) {
    server.handleClient();
    yield();
  } else {
    while (!NTP.getFirstSync()) {
      yield();
    }
    
    HTTPClient http;
    
    StaticJsonBuffer<100> responseBuffer;
    JsonObject& response = responseBuffer.createObject();
    
    response["temperature"] = readTemperature();
    response["voltage"] = analogRead(A0);
        
    char bodyBuffer[responseBuffer.size()];
    response.printTo(bodyBuffer, sizeof(bodyBuffer));
    
    String body = String(bodyBuffer);
    time_t now = timestamp();
    String url = String(settings.gatewayServer) + settings.gatewaySensorPath;
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    if (settings.hmacSecret) {
      String signature = requestSignature(settings.hmacSecret, settings.gatewaySensorPath, body, now);
      http.addHeader("X-Signature-Timestamp", String(now));
      http.addHeader("X-Signature", signature);
    }
    
    http.sendRequest("PUT", body);
    http.end();
  
    Serial.println();
    Serial.println("closing connection. going to sleep...");
    
    delay(1000);
    
    ESP.deepSleep(settings.updateInterval, WAKE_RF_DEFAULT);
  }
} 
