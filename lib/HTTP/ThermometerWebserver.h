#include <FS.h>
#include <Settings.h>
#include <TempIface.h>

#include <RichHttpServer.h>

#ifndef _WEB_SERVER_H
#define _WEB_SERVER_H

using RichHttpConfig = RichHttp::Generics::Configs::AsyncWebServer;
using RequestContext = RichHttpConfig::RequestContextType;

class ThermometerWebserver {
public:
  ThermometerWebserver(TempIface& sensors, Settings& settings);
  ~ThermometerWebserver();

  void begin();
  uint16_t getPort() const;

private:
  RichHttpServer<RichHttpConfig> server;
  TempIface& sensors;
  Settings& settings;
  uint16_t port;

  // Special routes
  void handleAbout(RequestContext& request);
  void handleOtaUpdate(RequestContext& request);
  void handleOtaSuccess(RequestContext& request);
  void handleCreateCommand(RequestContext& request);

  void handleUpdateSettings(RequestContext& request);
  void handleListSettings(RequestContext& request);

  void handleListThermometers(RequestContext& request);
  void handleGetThermometer(RequestContext& request);

  void serveProgmemStr(const char* pgmStr, const char* contentType, RequestContext& request);

  void applySettings();
};

#endif
