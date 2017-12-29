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

ThermometerWebserver::ThermometerWebserver(TempIface& sensors, Settings& settings)
  : sensors(sensors),
    settings(settings),
    port(settings.webPort),
    server(AsyncWebServer(settings.webPort))
{ }

ThermometerWebserver::~ThermometerWebserver() {
  server.reset();
}

uint16_t ThermometerWebserver::getPort() const {
  return port;
}

void ThermometerWebserver::begin() {
  on("/settings", HTTP_GET, handleListSettings());
  on("/settings", HTTP_PUT, handleUpdateSettings());

  on("/about", HTTP_GET, handleAbout());
  onUpload("/firmware", HTTP_POST, handleOtaSuccess(), handleOtaUpdate());

  onPattern("/thermometers/:thermometer", HTTP_GET, handleGetThermometer());
  on("/thermometers", HTTP_GET, handleListThermometers());

  on("/commands", HTTP_POST, handleCreateCommand());

  on("/", HTTP_GET, serveProgmemStr(INDEX_PAGE_HTML, TEXT_HTML));
  on("/style.css", HTTP_GET, serveProgmemStr(STYLESHEET, "text/css"));
  on("/script.js", HTTP_GET, serveProgmemStr(JAVASCRIPT, "application/javascript"));

  server.begin();
}

ArBodyHandlerFunction ThermometerWebserver::handleCreateCommand() {
  return [this](
    AsyncWebServerRequest* request,
    uint8_t* data,
    size_t len,
    size_t index,
    size_t total
  ) {
    DynamicJsonBuffer buffer;
    JsonObject& req = buffer.parseObject(data);

    if (! req.success()) {
      request->send_P(400, TEXT_PLAIN, PSTR("Invalid JSON"));
      return;
    }

    if (! req.containsKey("command")) {
      request->send_P(400, TEXT_PLAIN, PSTR("JSON did not contain `command' key"));
      return;
    }

    const String& command = req["command"];
    if (command.equalsIgnoreCase("reboot")) {
      request->send_P(200, TEXT_PLAIN, PSTR("OK"));

      ESP.restart();
    } else {
      request->send_P(400, TEXT_PLAIN, PSTR("Unhandled command"));
    }
  };
}

ArRequestHandlerFunction ThermometerWebserver::handleListThermometers() {
  return [this](AsyncWebServerRequest* request) {
    DynamicJsonBuffer buffer;
    JsonArray& thermometers = buffer.createArray();
    const std::map<String, uint8_t*>& sensorIds = sensors.thermometerIds();

    for (std::map<String, uint8_t*>::const_iterator itr = sensorIds.begin(); itr != sensorIds.end(); ++itr) {
      JsonObject& therm = thermometers.createNestedObject();

      if (settings.deviceAliases.count(itr->first) > 0) {
        therm["name"] = settings.deviceAliases[itr->first];
      }

      therm["temperature"] = sensors.lastSeenTemp(itr->first);
      therm["id"] = itr->first;
    }

    String body;
    thermometers.printTo(body);

    request->send(200, APPLICATION_JSON, body);
  };
}

PatternHandler::TPatternHandlerFn ThermometerWebserver::handleGetThermometer() {
  return [this](const UrlTokenBindings* bindings, AsyncWebServerRequest* request) {
    if (bindings->hasBinding("thermometer")) {
      const char* thermometer = bindings->get("thermometer");
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
        DynamicJsonBuffer buffer;
        JsonObject& json = buffer.createObject();

        float temp = sensors.lastSeenTemp(addrStr);

        json["id"] = addrStr;
        json["name"] = name;
        json["current_temperature"] = temp;

        String body;
        json.printTo(body);

        request->send(200, APPLICATION_JSON, body);
      } else {
        request->send_P(404, TEXT_PLAIN, PSTR("Could not find the provided thermometer"));
      }
    } else {
      request->send_P(400, TEXT_PLAIN, PSTR("You must provide a thermometer name"));
    }
  };
}

ArRequestHandlerFunction ThermometerWebserver::serveProgmemStr(const char* pgmStr, const char* contentType) {
  return [this, pgmStr, contentType](AsyncWebServerRequest* request) {
    request->send_P(200, contentType, pgmStr);
  };
}

ArRequestHandlerFunction ThermometerWebserver::handleOtaSuccess() {
  return [this](AsyncWebServerRequest* request) {
    request->send_P(200, TEXT_PLAIN, PSTR("Update successful.  Device will now reboot.\n\n"));

    ESP.restart();
  };
}

ArUploadHandlerFunction ThermometerWebserver::handleOtaUpdate() {
  return [this](
    AsyncWebServerRequest *request,
    const String& filename,
    size_t index,
    uint8_t *data,
    size_t len,
    bool isFinal
  ) {
    if (index == 0) {
      if (request->contentLength() > 0) {
#if defined(ESP8266)
        Update.runAsync(true);
#endif
        Update.begin(request->contentLength());
      } else {
        Serial.println(F("OTA Update: ERROR - Content-Length header required, but not present."));
      }
    }

    if (Update.size() > 0) {
      if (Update.write(data, len) != len) {
        Update.printError(Serial);

#if defined(ESP32)
        Update.abort();
#endif
      }

      if (isFinal) {
        if (!Update.end(true)) {
          Update.printError(Serial);
#if defined(ESP32)
          Update.abort();
#endif
        }
      }
    }
  };
}

ArRequestHandlerFunction ThermometerWebserver::handleAbout() {
  return [this](AsyncWebServerRequest* request) {
    // Measure before allocating buffers
    uint32_t freeHeap = ESP.getFreeHeap();

    StaticJsonBuffer<150> buffer;
    JsonObject& res = buffer.createObject();

    res["version"] = QUOTE(ESP8266_THERMOMETER_VERSION);
    res["variant"] = QUOTE(FIRMWARE_VARIANT);
    res["voltage"] = analogRead(A0);
    res["signal_strength"] = WiFi.RSSI();
    res["free_heap"] = freeHeap;
    res["sdk_version"] = ESP.getSdkVersion();

    String body;
    res.printTo(body);

    request->send(200, APPLICATION_JSON, body);
  };
}

ArRequestHandlerFunction ThermometerWebserver::sendSuccess() {
  return [this](AsyncWebServerRequest* request) {
    request->send(200, APPLICATION_JSON, "true");
  };
}

ArBodyHandlerFunction ThermometerWebserver::handleUpdateSettings() {
  return [this](
    AsyncWebServerRequest* request,
    uint8_t* data,
    size_t len,
    size_t index,
    size_t total
  ) {
    DynamicJsonBuffer buffer;
    JsonObject& req = buffer.parse(data);

    if (! req.success()) {
      request->send_P(400, TEXT_PLAIN, PSTR("Invalid JSON"));
      return;
    }

    settings.patch(req);
    settings.save();

    request->send(200);
  };
}

ArRequestHandlerFunction ThermometerWebserver::handleListSettings() {
  return [this](AsyncWebServerRequest* request) {
    request->send(200, APPLICATION_JSON, settings.toJson());
  };
}

bool ThermometerWebserver::isAuthenticated(AsyncWebServerRequest* request) {
  if (settings.hasAuthSettings()) {
    if (request->authenticate(settings.adminUsername.c_str(), settings.adminPassword.c_str())) {
      return true;
    } else {
      request->requestAuthentication();
      return false;
    }
  } else {
    return true;
  }
}

void ThermometerWebserver::onPattern(const String& pattern, const WebRequestMethod method, PatternHandler::TPatternHandlerFn fn) {
  PatternHandler::TPatternHandlerFn authedFn = [this, fn](const UrlTokenBindings* b, AsyncWebServerRequest* request) {
    if (isAuthenticated(request)) {
      fn(b, request);
    }
  };

  server.addHandler(new PatternHandler(pattern.c_str(), method, authedFn, NULL));
}

void ThermometerWebserver::onPattern(const String& pattern, const WebRequestMethod method, PatternHandler::TPatternHandlerBodyFn fn) {
  PatternHandler::TPatternHandlerBodyFn authedFn = [this, fn](
    const UrlTokenBindings* bindings,
    AsyncWebServerRequest* request,
    uint8_t* data,
    size_t len,
    size_t index,
    size_t total
  ) {
    if (isAuthenticated(request)) {
      fn(bindings, request, data, len, index, total);
    }
  };

  server.addHandler(new PatternHandler(pattern.c_str(), method, NULL, authedFn));
}

void ThermometerWebserver::on(const String& path, const WebRequestMethod method, ArRequestHandlerFunction fn) {
  ArRequestHandlerFunction authedFn = [this, fn](AsyncWebServerRequest* request) {
    if (isAuthenticated(request)) {
      fn(request);
    }
  };

  server.on(path.c_str(), method, authedFn);
}

void ThermometerWebserver::on(const String& path, const WebRequestMethod method, ArBodyHandlerFunction fn) {
  ArBodyHandlerFunction authedFn = [this, fn](
    AsyncWebServerRequest* request,
    uint8_t* data,
    size_t len,
    size_t index,
    size_t total
  ) {
    if (isAuthenticated(request)) {
      fn(request, data, len, index, total);
    }
  };

  server.addHandler(new ThermometerWebserver::BodyHandler(path.c_str(), method, authedFn));
}

void ThermometerWebserver::onUpload(const String& path, const WebRequestMethod method, ArUploadHandlerFunction fn) {
  ArUploadHandlerFunction authedFn = [this, fn](
    AsyncWebServerRequest *request,
    const String& filename,
    size_t index,
    uint8_t *data,
    size_t len,
    bool isFinal
  ) {
    if (isAuthenticated(request)) {
      fn(request, filename, index, data, len, isFinal);
    }
  };

  server.addHandler(new ThermometerWebserver::UploadHandler(path.c_str(), method, authedFn));
}

void ThermometerWebserver::onUpload(const String& path, const WebRequestMethod method, ArRequestHandlerFunction onCompleteFn, ArUploadHandlerFunction fn) {
  ArUploadHandlerFunction authedFn = [this, fn](
    AsyncWebServerRequest *request,
    const String& filename,
    size_t index,
    uint8_t *data,
    size_t len,
    bool isFinal
  ) {
    if (isAuthenticated(request)) {
      fn(request, filename, index, data, len, isFinal);
    }
  };

  ArRequestHandlerFunction authedOnCompleteFn = [this, onCompleteFn](AsyncWebServerRequest* request) {
    if (isAuthenticated(request)) {
      onCompleteFn(request);
    }
  };

  server.addHandler(new ThermometerWebserver::UploadHandler(path.c_str(), method, authedOnCompleteFn, authedFn));
}

ThermometerWebserver::UploadHandler::UploadHandler(
  const char* uri,
  const WebRequestMethod method,
  ArRequestHandlerFunction onCompleteFn,
  ArUploadHandlerFunction handler
) : uri(new char[strlen(uri) + 1]),
    method(method),
    handler(handler),
    onCompleteFn(onCompleteFn)
{
  strcpy(this->uri, uri);
}

ThermometerWebserver::UploadHandler::UploadHandler(
  const char* uri,
  const WebRequestMethod method,
  ArUploadHandlerFunction handler
) : UploadHandler(uri, method, NULL, handler)
{ }

ThermometerWebserver::UploadHandler::~UploadHandler() {
  delete uri;
}

bool ThermometerWebserver::UploadHandler::canHandle(AsyncWebServerRequest *request) {
  if (this->method != HTTP_ANY && this->method != request->method()) {
    return false;
  }

  return request->url() == this->uri;
}

void ThermometerWebserver::UploadHandler::handleUpload(
  AsyncWebServerRequest *request,
  const String &filename,
  size_t index,
  uint8_t *data,
  size_t len,
  bool isFinal
) {
  handler(request, filename, index, data, len, isFinal);
}

void ThermometerWebserver::UploadHandler::handleRequest(AsyncWebServerRequest* request) {
  if (onCompleteFn == NULL) {
    request->send(200);
  } else {
    onCompleteFn(request);
  }
}

ThermometerWebserver::BodyHandler::BodyHandler(
  const char* uri,
  const WebRequestMethod method,
  ArBodyHandlerFunction handler
) : uri(new char[strlen(uri) + 1]),
    method(method),
    handler(handler)
{
  strcpy(this->uri, uri);
}

ThermometerWebserver::BodyHandler::~BodyHandler() {
  delete uri;
}

bool ThermometerWebserver::BodyHandler::canHandle(AsyncWebServerRequest *request) {
  if (this->method != HTTP_ANY && this->method != request->method()) {
    return false;
  }

  return request->url() == this->uri;
}

void ThermometerWebserver::BodyHandler::handleRequest(AsyncWebServerRequest* request) {
  request->send(200);
}

void ThermometerWebserver::BodyHandler::handleBody(
  AsyncWebServerRequest *request,
  uint8_t *data,
  size_t len,
  size_t index,
  size_t total
) {
  handler(request, data, len, index, total);
}
