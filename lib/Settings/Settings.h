#ifndef _SETTINGS_H_INCLUDED
#define _SETTINGS_H_INCLUDED

#include <Time.h>
#include <Timezone.h>
#include <ArduinoJson.h>
#include <map>

#define SETTINGS_FILE  "/config.json"
#define SETTINGS_TERMINATOR '\0'

#define XQUOTE(x) #x
#define QUOTE(x) XQUOTE(x)

#ifndef FIRMWARE_VARIANT
#define FIRMWARE_VARIANT unknown
#endif

#ifndef ESP8266_THERMOMETER_VERSION
#define ESP8266_THERMOMETER_VERSION unknown
#endif

#define DEFAULT_MQTT_PORT 1883

enum class OperatingMode {
  DEEP_SLEEP = 0,
  ALWAYS_ON = 1
};

class Settings {
public:
  Settings()
    : flagServerPort(31415)
    , updateInterval(600)
    , sensorPollInterval(5)
    , webPort(80)
    , opMode(OperatingMode::DEEP_SLEEP)
    , sensorBusPin(2)
  { }

  static void deserialize(Settings& settings, String json);
  static void load(Settings& settings);

  void save();
  String toJson(const bool prettyPrint = true);
  void serialize(Stream& stream, const bool prettyPrint = false);
  void patch(JsonObject json);

  bool requiredSettingsDefined();

  bool isAuthenticationEnabled() const;
  const String& getUsername() const;
  const String& getPassword() const;

  String mqttServer();
  uint16_t mqttPort();

  String deviceName(uint8_t* addr, bool resolveDeviceName = true);

  String adminUsername;
  String adminPassword;
  uint16_t webPort;

  String gatewayServer;
  String hmacSecret;

  String flagServer;
  uint16 flagServerPort;

  unsigned long updateInterval;
  time_t sensorPollInterval;
  OperatingMode opMode;

  String _mqttServer;
  String mqttTopic;
  String mqttUsername;
  String mqttPassword;

  uint8_t sensorBusPin;

  std::map<String, String> deviceAliases;
  std::map<String, String> sensorPaths;

  template <typename T>
  void setIfPresent(JsonObject obj, const char* key, T& var) {
    if (obj.containsKey(key)) {
      JsonVariant val = obj[key];
      var = val.as<T>();
    }
  }
};

#endif
