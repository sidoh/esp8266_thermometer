#include <Settings.h>
#include <IntParsing.h>

#include <ArduinoJson.h>
#include <FS.h>

static const char* OP_MODE_NAMES[2] = {
  "deep_sleep",
  "always_on"
};

OperatingMode opModeFromString(const String& s) {
  for (size_t i = 0; i < sizeof(OP_MODE_NAMES); i++) {
    if (s == OP_MODE_NAMES[i]) {
      return static_cast<OperatingMode>(i);
    }
  }
  return OperatingMode::DEEP_SLEEP;
}

template <typename T>
bool isDefined(const T& setting) {
  return setting != NULL && setting.length() > 0;
}

bool Settings::requiredSettingsDefined() {
  return isDefined(flagServer) && flagServerPort > 0;
}

String Settings::resolveDeviceName(uint8_t* addr) {
  char deviceIdHex[50];
  IntParsing::bytesToHexStr(addr, 8, deviceIdHex, sizeof(deviceIdHex) - 1);

  if (deviceAliases.count(deviceIdHex) > 0) {
    return deviceAliases[deviceIdHex];
  } else {
    return deviceIdHex;
  }
}

void Settings::deserialize(Settings& settings, String json) {
  StaticJsonBuffer<SETTINGS_SIZE> jsonBuffer;
  JsonObject& parsedSettings = jsonBuffer.parseObject(json);

  settings.gatewayServer = parsedSettings.get<String>("gateway_server");
  settings.hmacSecret = parsedSettings.get<String>("hmac_secret");
  settings.flagServer = parsedSettings.get<String>("flag_server");
  settings.flagServerPort = parsedSettings["flag_server_port"];
  settings.updateInterval = parsedSettings["update_interval"];
  settings.adminUsername = parsedSettings.get<String>("admin_username");
  settings.adminPassword = parsedSettings.get<String>("admin_password");
  settings.mqttServer = parsedSettings.get<String>("mqtt_server");
  settings.mqttPort = parsedSettings["mqtt_port"];
  settings.mqttTopic = parsedSettings.get<String>("mqtt_topic");
  settings.mqttUsername = parsedSettings.get<String>("mqtt_username");
  settings.mqttPassword = parsedSettings.get<String>("mqtt_password");
  settings.opMode = opModeFromString(parsedSettings.get<String>("operating_mode"));
  settings.sensorBusPin = parsedSettings["sensor_bus_pin"];

  if (parsedSettings.containsKey("device_aliases")) {
    JsonObject& aliases = parsedSettings["device_aliases"];
    settings.deviceAliases.clear();

    for (JsonObject::iterator itr = aliases.begin(); itr != aliases.end(); ++itr) {
      settings.deviceAliases[itr->key] = itr->value.as<const char*>();
    }
  }

  if (parsedSettings.containsKey("sensor_paths")) {
    JsonObject& sensorPaths = parsedSettings["sensor_paths"];
    settings.sensorPaths.clear();

    for (JsonObject::iterator itr = sensorPaths.begin(); itr != sensorPaths.end(); ++itr) {
      settings.sensorPaths[itr->key] = itr->value.as<const char*>();
    }
  }
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
  root["hmac_secret"] = this->hmacSecret;
  root["flag_server"] = this->flagServer;
  root["flag_server_port"] = this->flagServerPort;
  root["update_interval"] = this->updateInterval;
  root["admin_username"] = this->adminUsername;
  root["admin_password"] = this->adminPassword;
  root["mqtt_server"] = this->mqttServer;
  root["mqtt_port"] = this->mqttPort;
  root["mqtt_topic"] = this->mqttTopic;
  root["mqtt_username"] = this->mqttUsername;
  root["mqtt_password"] = this->mqttPassword;
  root["operating_mode"] = OP_MODE_NAMES[static_cast<uint8_t>(this->opMode)];
  root["sensor_bus_pin"] = this->sensorBusPin;

  JsonObject& aliases = root.createNestedObject("device_aliases");
  for (std::map<String, String>::iterator itr = this->deviceAliases.begin(); itr != this->deviceAliases.end(); ++itr) {
    aliases[itr->first] = itr->second;
  }

  JsonObject& sensorPaths = root.createNestedObject("sensor_paths");
  for (std::map<String, String>::iterator itr = this->sensorPaths.begin(); itr != this->sensorPaths.end(); ++itr) {
    sensorPaths[itr->first] = itr->second;
  }

  if (prettyPrint) {
    root.prettyPrintTo(stream);
  } else {
    root.printTo(stream);
  }
}
