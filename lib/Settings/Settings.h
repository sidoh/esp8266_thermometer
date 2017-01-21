#ifndef _SETTINGS_H_INCLUDED
#define _SETTINGS_H_INCLUDED

#include <StringStream.h>
#include <Time.h>
#include <Timezone.h>

#define SETTINGS_SIZE  1024
#define SETTINGS_FILE  "/config.json"
#define SETTINGS_TERMINATOR '\0'

#define TEMP_SENSOR_PIN 2

class Settings {
public:
  Settings() :
    gatewayServer(""),
    gatewaySensorPath(""),
    hmacSecret(""),
    flagServer(""),
    adminUsername(""),
    adminPassword(""),
    flagServerPort(31415),
    updateInterval(600000000L)
  { }

  static void deserialize(Settings& settings, String json);
  static void load(Settings& settings);
  
  void save();
  String toJson(const bool prettyPrint = true);
  void serialize(Stream& stream, const bool prettyPrint = false);
  
  bool requiredSettingsDefined();
  
  String adminUsername;
  String adminPassword;
  
  String gatewayServer;
  String gatewaySensorPath;
  String hmacSecret;
  
  String flagServer;
  uint16 flagServerPort;
  
  unsigned long updateInterval;
};

#endif 