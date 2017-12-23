#ifndef _SETTINGS_H_INCLUDED
#define _SETTINGS_H_INCLUDED

#include <StringStream.h>
#include <Time.h>
#include <Timezone.h>
#include <map>

#define SETTINGS_SIZE  1024
#define SETTINGS_FILE  "/config.json"
#define SETTINGS_TERMINATOR '\0'

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
    mqttServer(""),
    mqttPort(1883),
    mqttTopic(""),
    mqttUsername(""),
    mqttPassword(""),
    sensorBusPin(2),
    opMode(OperatingMode::DEEP_SLEEP)
  { }

  static void deserialize(Settings& settings, String json);
  static void load(Settings& settings);

  void save();
  String toJson(const bool prettyPrint = true);
  void serialize(Stream& stream, const bool prettyPrint = false);

  bool requiredSettingsDefined();

  String resolveDeviceName(uint8_t* addr);

  String adminUsername;
  String adminPassword;

  String gatewayServer;
  String hmacSecret;

  String flagServer;
  uint16 flagServerPort;

  unsigned long updateInterval;
  OperatingMode opMode;

  String mqttServer;
  uint16_t mqttPort;
  String mqttTopic;
  String mqttUsername;
  String mqttPassword;

  uint8_t sensorBusPin;

  std::map<String, String> deviceAliases;
  std::map<String, String> sensorPaths;
};

#endif
