#include <Settings.h>

#include <ArduinoJson.h>
#include <FS.h>

template <typename T>

bool isDefined(const T& setting) {
  return setting != NULL && setting.length() > 0;
}

bool Settings::requiredSettingsDefined() {
  return isDefined(gatewayServer) &&
    isDefined(gatewaySensorPath) &&
    isDefined(flagServer) &&
    flagServerPort > 0;
}

void Settings::deserialize(Settings& settings, String json) {
  StaticJsonBuffer<SETTINGS_SIZE> jsonBuffer;
  JsonObject& parsedSettings = jsonBuffer.parseObject(json);
    
  settings.gatewayServer = parsedSettings.get<String>("gateway_server");
  settings.gatewaySensorPath = parsedSettings.get<String>("gateway_sensor_path");
  settings.hmacSecret = parsedSettings.get<String>("hmac_secret");
  settings.flagServer = parsedSettings.get<String>("flag_server");
  settings.flagServerPort = parsedSettings["flag_server_port"];
  settings.updateInterval = parsedSettings["update_interval"];
  settings.adminUsername = parsedSettings.get<String>("admin_username");
  settings.adminPassword = parsedSettings.get<String>("admin_password");
}

void Settings::load(Settings& settings) {
  if (SPIFFS.exists(SETTINGS_FILE)) {
    File f = SPIFFS.open(SETTINGS_FILE, "r");
    String settingsContents = f.readStringUntil(SETTINGS_TERMINATOR);
    f.close();
    
    deserialize(settings, settingsContents);
  } else {
    settings.save();
  }
}

String Settings::toJson(const bool prettyPrint) {
  String buffer = "";
  StringStream s(buffer);
  serialize(s, prettyPrint);
  return buffer;
}

void Settings::save() {
  File f = SPIFFS.open(SETTINGS_FILE, "w");
  
  if (!f) {
    Serial.println("Opening settings file failed");
  } else {
    serialize(f);
    f.close();
  }
}

void Settings::serialize(Stream& stream, const bool prettyPrint) {
  StaticJsonBuffer<SETTINGS_SIZE> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  
  root["gateway_server"] = this->gatewayServer;
  root["gateway_sensor_path"] = this->gatewaySensorPath;
  root["hmac_secret"] = this->hmacSecret;
  root["flag_server"] = this->flagServer;
  root["flag_server_port"] = this->flagServerPort;
  root["update_interval"] = this->updateInterval;
  root["admin_username"] = this->adminUsername;
  root["admin_password"] = this->adminPassword;
  
  if (prettyPrint) {
    root.prettyPrintTo(stream);
  } else {
    root.printTo(stream);
  }
}