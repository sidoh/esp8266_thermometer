#ifndef _SETTINGS_H_INCLUDED
#define _SETTINGS_H_INCLUDED

#include <StringStream.h>
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

#ifndef THERMOMETER_VERSION
#define THERMOMETER_VERSION unknown
#endif

enum class OperatingMode {
  DEEP_SLEEP = 0,
  ALWAYS_ON = 1
};

class Settings {
public:
  Settings() :
    gatewayServer(""),
    hmacSecret(""),
    flagServer(""),
    adminUsername(""),
    adminPassword(""),
    flagServerPort(31415),
    updateInterval(600),
    sensorPollInterval(5),
    mqttServer(""),
    mqttPort(1883),
    mqttTopic(""),
    mqttUsername(""),
    mqttPassword(""),
    sensorBusPin(2),
    opMode(OperatingMode::DEEP_SLEEP),
    webPort(80)
  { }

  static void deserialize(Settings& settings, String json);
  static void load(Settings& settings);

  void save();
  String toJson(const bool prettyPrint = true);
  void serialize(Stream& stream, const bool prettyPrint = false);
  void patch(JsonObject& json);

  bool requiredSettingsDefined();
  bool hasAuthSettings();

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

  String mqttServer;
  uint16_t mqttPort;
  String mqttTopic;
  String mqttUsername;
  String mqttPassword;

  uint8_t sensorBusPin;

  std::map<String, String> deviceAliases;
  std::map<String, String> sensorPaths;

  template <typename T>
  void setIfPresent(JsonObject& obj, const char* key, T& var) {
    if (obj.containsKey(key)) {
      var = obj.get<T>(key);
    }
  }
};

#endif
