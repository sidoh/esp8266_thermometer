#include <stddef.h>
#include <MqttClient.h>
#include <WiFiClient.h>

MqttClient::MqttClient(Settings& settings)
  : settings(settings),
    lastConnectAttempt(0)
{
  String strDomain = settings.mqttServer();
  this->domain = new char[strDomain.length() + 1];
  strcpy(this->domain, strDomain.c_str());

  this->mqttClient = new PubSubClient(tcpClient);
}

MqttClient::~MqttClient() {
  mqttClient->disconnect();
  delete this->domain;
}

void MqttClient::begin() {
#ifdef MQTT_DEBUG
  printf_P(
    PSTR("MqttClient - Connecting to: %s\nparsed:%s:%u\n"),
    settings._mqttServer.c_str(),
    settings.mqttServer().c_str(),
    settings.mqttPort()
  );
#endif

  mqttClient->setServer(this->domain, settings.mqttPort());

  reconnect();
}

bool MqttClient::connect() {
  char nameBuffer[30];
  sprintf_P(nameBuffer, PSTR("esp8266-thermometer-%u"), ESP.getChipId());

#ifdef MQTT_DEBUG
    Serial.println(F("MqttClient - connecting"));
#endif

  if (settings.mqttUsername.length() > 0) {
    return mqttClient->connect(
      nameBuffer,
      settings.mqttUsername.c_str(),
      settings.mqttPassword.c_str()
    );
  } else {
    return mqttClient->connect(nameBuffer);
  }
}

void MqttClient::reconnect() {
  if (lastConnectAttempt > 0 && (millis() - lastConnectAttempt) < MQTT_CONNECTION_ATTEMPT_FREQUENCY) {
    return;
  }

  if (! mqttClient->connected()) {
    if (connect()) {
      subscribe();

#ifdef MQTT_DEBUG
      Serial.println(F("MqttClient - Successfully connected to MQTT server"));
#endif
    } else {
      Serial.println(F("ERROR: Failed to connect to MQTT server"));
    }
  }

  lastConnectAttempt = millis();
}

void MqttClient::handleClient() {
  reconnect();
  mqttClient->loop();
}

void MqttClient::sendUpdate(const String& deviceName, const char* update) {
  String topic = settings.mqttTopic;
  topic += "/";
  topic += deviceName;

  publish(topic, update, true);
}

void MqttClient::subscribe() {
  // This is necessary with pubsubclient because it assumes that a subscription is necessary in order
  // to maintain a connection.
  const char* phonyTopic = "esp8266-thermometer/empty-topic";

#ifdef MQTT_DEBUG
  printf_P(PSTR("MqttClient - subscribing phony topic: %s\n"), phonyTopic);
#endif

  mqttClient->subscribe(phonyTopic);
}

void MqttClient::publish(
  const String& topic,
  const char* message,
  const bool retain
) {
  if (topic.length() == 0) {
    return;
  }

#ifdef MQTT_DEBUG
  printf("MqttClient - publishing update to %s\n", topic.c_str());
#endif

  mqttClient->publish(topic.c_str(), message, retain);
}