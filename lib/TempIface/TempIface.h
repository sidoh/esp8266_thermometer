#include <DallasTemperature.h>
#include <Settings.h>
#include <map>

#ifndef _TEMP_IFACE_H
#define _TEMP_IFACE_H

class TempIface {
public:

  TempIface(DallasTemperature*& sensors, Settings& settings);
  ~TempIface();

  void begin();
  void loop();
  const std::map<String, uint8_t*>& thermometerIds();
  const float lastSeenTemp(const String& id);
  const bool hasSeenId(const String& id);

private:

  std::map<String, uint8_t*> seenIds;
  std::map<String, float> lastTemps;
  time_t lastUpdatedAt;

  DallasTemperature*& sensors;
  Settings& settings;

};

#endif // _TEMP_IFACE_H