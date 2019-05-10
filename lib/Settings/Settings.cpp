#include <Settings.h>
#include <IntParsing.h>

#include <ArduinoJson.h>
#include <FS.h>

#define PORT_POSITION(s) ( s.indexOf(':') )

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

String Settings::mqttServer() {
  int pos = PORT_POSITION(_mqttServer);

  if (pos == -1) {
    return _mqttServer;
  } else {
    return _mqttServer.substring(0, pos);
  }
}

uint16_t Settings::mqttPort() {
  int pos = PORT_POSITION(_mqttServer);

  if (pos == -1) {
    return DEFAULT_MQTT_PORT;
  } else {
    return atoi(_mqttServer.c_str() + pos + 1);
  }
}

bool Settings::requiredSettingsDefined() {
  return isDefined(flagServer) && flagServerPort > 0;
}

bool Settings::isAuthenticationEnabled() const {
  return adminUsername.length() > 0 && adminPassword.length() > 0;
}

const String& Settings::getUsername() const {
  return adminUsername;
}

const String& Settings::getPassword() const {
  return adminPassword;
}

String Settings::deviceName(uint8_t* addr, bool resolveAlias) {
  char deviceIdHex[50];
  IntParsing::bytesToHexStr(addr, 8, deviceIdHex, sizeof(deviceIdHex) - 1);

  if (resolveAlias && deviceAliases.count(deviceIdHex) > 0) {
    return deviceAliases[deviceIdHex];
  } else {
    return deviceIdHex;
  }
}

void Settings::deserialize(Settings& settings, String json) {
  DynamicJsonDocument jsonBuffer(2048);
  deserializeJson(jsonBuffer, json);
  JsonObject parsedSettings = jsonBuffer.as<JsonObject>();

  if (parsedSettings.isNull()) {
    Serial.println(F("ERROR: could not parse settings file on flash"));
  }

  settings.patch(parsedSettings);
}

void Settings::patch(JsonObject json) {
  setIfPresent(json, "mqtt.server", _mqttServer);
  setIfPresent(json, "mqtt.topic_prefix", mqttTopic);
  setIfPresent(json, "mqtt.username", mqttUsername);
  setIfPresent(json, "mqtt.password", mqttPassword);

  setIfPresent(json, "http.gateway_server", gatewayServer);
  setIfPresent(json, "http.hmac_secret", hmacSecret);

  setIfPresent(json, "admin.web_ui_port", webPort);
  setIfPresent(json, "admin.flag_server", flagServer);
  setIfPresent(json, "admin.flag_server_port", flagServerPort);
  setIfPresent(json, "admin.username", adminUsername);
  setIfPresent(json, "admin.password", adminPassword);
  setIfPresent(json, "thermometers.update_interval", updateInterval);
  setIfPresent(json, "thermometers.poll_interval", sensorPollInterval);
  setIfPresent(json, "thermometers.sensor_bus_pin", sensorBusPin);

  if (json.containsKey("admin.operating_mode")) {
    opMode = opModeFromString(json["admin.operating_mode"]);
  }

  if (json.containsKey("thermometers.aliases")) {
    JsonObject aliases = json["thermometers.aliases"];
    deviceAliases.clear();

    for (JsonObject::iterator itr = aliases.begin(); itr != aliases.end(); ++itr) {
      const char* value = itr->value().as<const char*>();

      if (strlen(value) > 0) {
        deviceAliases[itr->key().c_str()] = value;
      } else {
        deviceAliases.erase(itr->key().c_str());
      }
    }
  }

  if (json.containsKey("http.sensor_paths")) {
    JsonObject sensorPaths = json["http.sensor_paths"].as<JsonObject>();
    this->sensorPaths.clear();

    for (JsonObject::iterator itr = sensorPaths.begin(); itr != sensorPaths.end(); ++itr) {
      const char* value = itr->value().as<const char*>();

      if (strlen(value) > 0) {
        this->sensorPaths[itr->key().c_str()] = value;
      } else {
        this->sensorPaths.erase(itr->key().c_str());
      }
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
  DynamicJsonDocument jsonBuffer(2048);
  JsonObject root = jsonBuffer.to<JsonObject>();

  root["mqtt.server"] = this->_mqttServer;
  root["mqtt.topic_prefix"] = this->mqttTopic;
  root["mqtt.username"] = this->mqttUsername;
  root["mqtt.password"] = this->mqttPassword;

  root["http.gateway_server"] = this->gatewayServer;
  root["http.hmac_secret"] = this->hmacSecret;

  root["admin.web_ui_port"] = this->webPort;
  root["admin.flag_server"] = this->flagServer;
  root["admin.flag_server_port"] = this->flagServerPort;
  root["admin.username"] = this->adminUsername;
  root["admin.password"] = this->adminPassword;
  root["admin.operating_mode"] = OP_MODE_NAMES[static_cast<uint8_t>(this->opMode)];
  root["thermometers.sensor_bus_pin"] = this->sensorBusPin;
  root["thermometers.update_interval"] = this->updateInterval;
  root["thermometers.poll_interval"] = this->sensorPollInterval;

  JsonObject aliases = root.createNestedObject("thermometers.aliases");
  for (std::map<String, String>::iterator itr = this->deviceAliases.begin(); itr != this->deviceAliases.end(); ++itr) {
    aliases[itr->first] = itr->second;
  }

  JsonObject sensorPaths = root.createNestedObject("http.sensor_paths");
  for (std::map<String, String>::iterator itr = this->sensorPaths.begin(); itr != this->sensorPaths.end(); ++itr) {
    sensorPaths[itr->first] = itr->second;
  }

  if (prettyPrint) {
    serializeJsonPretty(root, stream);
  } else {
    serializeJson(root, stream);
  }
}
