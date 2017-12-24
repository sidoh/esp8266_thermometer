#include <TempIface.h>
#include <IntParsing.h>

TempIface::TempIface(DallasTemperature*& sensors, Settings& settings)
  : sensors(sensors),
    settings(settings),
    lastUpdatedAt(0)
{ }

TempIface::~TempIface() { }

void TempIface::begin() {

  char strAddr[50];

  for (uint8_t i = 0; i < sensors->getDeviceCount(); ++i) {
    uint8_t* addr = new uint8_t[8];
    sensors->getAddress(addr, i);
    IntParsing::bytesToHexStr(addr, 8, strAddr, sizeof(strAddr)-1);

    seenIds[strAddr] = addr;
  }

}

void TempIface::loop() {

  time_t n = now();

  if (n > (lastUpdatedAt + settings.sensorPollInterval)) {
    for (std::map<String, uint8_t*>::iterator itr = seenIds.begin(); itr != seenIds.end(); ++itr) {
      sensors->requestTemperaturesByAddress(itr->second);
      lastTemps[itr->first] = sensors->getTempF(itr->second);
    }
    lastUpdatedAt = n;
  }

}

const std::map<String, uint8_t*>& TempIface::thermometerIds() {

  return seenIds;

}

const float TempIface::lastSeenTemp(const String& id) {

  if (hasSeenId(id)) {
    return lastTemps[id];
  } else {
    return -187;
  }

}

const bool TempIface::hasSeenId(const String& id) {

  return lastTemps.count(id) > 0;

}