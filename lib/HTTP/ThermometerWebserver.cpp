#include <ThermometerWebserver.h>
#include <Updater.h>
#include <IndexPage.h>
#include <Javascript.h>
#include <Stylesheet.h>
#include <IntParsing.h>
#include <map>

#if defined(ESP8266)
#include <Updater.h>
#elif defined(ESP32)
#include <Update.h>
#endif

static const char TEXT_HTML[] = "text/html";
static const char TEXT_PLAIN[] = "text/plain";
static const char APPLICATION_JSON[] = "application/json";

static const char CONTENT_TYPE_HEADER[] = "Content-Type";

using namespace std::placeholders;

ThermometerWebserver::ThermometerWebserver(TempIface& sensors, Settings& settings)
  : sensors(sensors),
    settings(settings),
    port(settings.webPort),
    server(RichHttpServer<RichHttpConfig>(settings.webPort))
{
  applySettings();
}

ThermometerWebserver::~ThermometerWebserver() {
  server.reset();
}

uint16_t ThermometerWebserver::getPort() const {
  return port;
}

void ThermometerWebserver::begin() {
  server
    .buildHandler("/settings")
    .on(HTTP_GET, std::bind(&ThermometerWebserver::handleListSettings, this, _1))
    .on(HTTP_PUT, std::bind(&ThermometerWebserver::handleUpdateSettings, this, _1));

  server
    .buildHandler("/about")
    .on(HTTP_GET, std::bind(&ThermometerWebserver::handleAbout, this, _1));

  server
    .buildHandler("/firmware")
    .handleOTA();

  server
    .buildHandler("/thermometers/:thermometer")
    .on(HTTP_GET, std::bind(&ThermometerWebserver::handleGetThermometer, this, _1));
  server
    .buildHandler("/thermometers")
    .on(HTTP_GET, std::bind(&ThermometerWebserver::handleListThermometers, this, _1));

  server
    .buildHandler("/commands")
    .on(HTTP_POST, std::bind(&ThermometerWebserver::handleCreateCommand, this, _1));

  server
    .buildHandler("/")
    .on(HTTP_GET, std::bind(&ThermometerWebserver::serveProgmemStr, this, INDEX_PAGE_HTML, TEXT_HTML, _1));
  server
    .buildHandler("/style.css")
    .on(HTTP_GET, std::bind(&ThermometerWebserver::serveProgmemStr, this, STYLESHEET, "text/css", _1));
  server
    .buildHandler("/")
    .on(HTTP_GET, std::bind(&ThermometerWebserver::serveProgmemStr, this, JAVASCRIPT, "application/javascript", _1));

  server.clearBuilders();
  server.begin();
}

void ThermometerWebserver::handleCreateCommand(RequestContext& request) {
  JsonObject req = request.getJsonBody().as<JsonObject>();

  if (req.isNull()) {
    request.response.json["error"] = F("Invalid JSON - must be object");
    request.response.setCode(400);
    return;
  }

  if (! req.containsKey("command")) {
    request.response.json["error"] = F("JSON did not contain `command' key");
    request.response.setCode(400);
    return;
  }

  const String& command = req["command"];
  if (command.equalsIgnoreCase("reboot")) {
    request.rawRequest->send_P(200, TEXT_PLAIN, PSTR("OK"));

    ESP.restart();
  } else {
    request.response.json["error"] = F("Unhandled command");
    request.response.setCode(400);
  }
}

void ThermometerWebserver::handleListThermometers(RequestContext& request) {
  JsonArray thermometers = request.response.json.to<JsonArray>();
  const std::map<String, uint8_t*>& sensorIds = sensors.thermometerIds();

  for (std::map<String, uint8_t*>::const_iterator itr = sensorIds.begin(); itr != sensorIds.end(); ++itr) {
    JsonObject therm = thermometers.createNestedObject();

    if (settings.deviceAliases.count(itr->first) > 0) {
      therm["name"] = settings.deviceAliases[itr->first];
    }

    therm["temperature"] = sensors.lastSeenTemp(itr->first);
    therm["id"] = itr->first;
  }
}

void ThermometerWebserver::handleGetThermometer(RequestContext& request) {
  if (request.pathVariables.hasBinding("thermometer")) {
    const char* thermometer = request.pathVariables.get("thermometer");
    uint8_t addr[8];
    String name;

    // If the provided token is an ID we have an alias for
    if (settings.deviceAliases.count(thermometer) > 0) {
      name = settings.deviceAliases[thermometer];
      hexStrToBytes(thermometer, strlen(thermometer), addr, 8);
    // Otherwise, if it's an alias, try to find it
    } else {
      bool found = false;

      for (std::map<String, String>::iterator itr = settings.deviceAliases.begin(); itr != settings.deviceAliases.end(); ++itr) {
        if (itr->second == thermometer) {
          hexStrToBytes(itr->first.c_str(), itr->first.length(), addr, 8);
          name = thermometer;
          found = true;
          break;
        }
      }

      // Try to treat it as an address
      if (!found) {
        hexStrToBytes(thermometer, strlen(thermometer), addr, 8);
      }
    }

    char addrStr[50];
    IntParsing::bytesToHexStr(addr, 8, addrStr, sizeof(addrStr)-1);

    if (sensors.hasSeenId(addrStr)) {
      JsonObject json = request.response.json.to<JsonObject>();

      float temp = sensors.lastSeenTemp(addrStr);

      json["id"] = addrStr;
      json["name"] = name;
      json["current_temperature"] = temp;
    } else {
      request.response.json["error"] = F("Could not find the provided thermometer");
      request.response.setCode(404);
    }
  } else {
    request.response.json["error"] = PSTR("You must provide a thermometer name");
    request.response.setCode(400);
  }
}

void ThermometerWebserver::serveProgmemStr(const char* pgmStr, const char* contentType, RequestContext& request) {
  request.rawRequest->send_P(200, contentType, pgmStr);
}

void ThermometerWebserver::handleAbout(RequestContext& request) {
  // Measure before allocating buffers
  uint32_t freeHeap = ESP.getFreeHeap();

  JsonObject res = request.response.json.to<JsonObject>();

  res["version"] = QUOTE(ESP8266_THERMOMETER_VERSION);
  res["variant"] = QUOTE(FIRMWARE_VARIANT);
  res["voltage"] = analogRead(A0);
  res["signal_strength"] = WiFi.RSSI();
  res["free_heap"] = freeHeap;
  res["sdk_version"] = ESP.getSdkVersion();
}

void ThermometerWebserver::handleUpdateSettings(RequestContext& request) {
  JsonObject req = request.getJsonBody().as<JsonObject>();

  if (req.isNull()) {
    request.response.json["error"] = F("Invalid JSON");
    request.response.setCode(400);
    return;
  }

  settings.patch(req);
  settings.save();

  applySettings();
  request.rawRequest->send(SPIFFS, SETTINGS_FILE, APPLICATION_JSON);
}

void ThermometerWebserver::handleListSettings(RequestContext& request) {
  request.rawRequest->send(SPIFFS, SETTINGS_FILE, APPLICATION_JSON);
}

void ThermometerWebserver::applySettings() {
  if (settings.hasAuthSettings()) {
    Serial.println("requiring auth");
    server.requireAuthentication(settings.adminUsername, settings.adminPassword);
  } else {
    server.disableAuthentication();
  }
}