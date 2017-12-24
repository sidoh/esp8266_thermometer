# esp8266_thermometer
ESP8266-based thermometer. Pushes temperature data to a URL at a configurable interval. Suitable for battery power.  Works with multiple probes.

## Parts

* ESP8266. I'm using a NodeMCUs.
* DS18B20 temperature probe(s). Probably works with other Dallas Instruments temperature probes.
* (optional) batteries and battery holder. I've had good luck with a Li-Ion 18650 cell.

## Circuit

Not even worth drawing out. Data line from the DS18B20 is connected to GPIO 2 by default (D4 on Wemos D1), and has a 4.7 KΩ pullup resistor.  You might want to user a lower resistor value if you have a long wire run or many sensors.

## Configuring

#### WiFi

If WiFi isn't configured, it'll start a setup WiFi network named something like Thermometer_XXXX. **The password is `fireitup`**. Connect to this network to configure WiFi.

#### Other settings

The other settings are stored as JSON in SPIFFS. When the chip is unconfigured, it starts a web server on port 80. Navigate here to edit the settings.

<img src="https://imgur.com/ZyHefLa.png" width="400" />

#### Multiple sensors

Sensors connected to the OneWire bus will be auto-detected.  Data from all sensors will be pushed.  You can configure aliases for detected device IDs in the UI or via the REST API.

#### Operating mode

There are two operating modes: Always On, and Deep Sleep.  In Always On mode, the device will stay powered and connected to WiFi.  The UI will stay running.  This is good when connected to a persistent power source.  Deep Sleep will push sensor readings to MQTT/HTTP and enter deep sleep.  This is better when using a battery.

**Breaking out of deep sleep loop**

Each time the device wakes from deep sleep, it checks if it can connect to the "flag server" (configured in the JSON blob), and if the flag server sends the string **`update`**.  If it does, it'll boot into settings mode. 

Example command:

```
$ echo -ne 'update' | nc -vvl 31415
```

#### OTA updates

You can push firmware updates to `POST /firmware` when in settings mode.  This can also be done through the UI.

## Integrations

#### MQTT

To push updates to MQTT, add an MQTT server and a topic prefix.  You can optionally configure a username and password.  Updates will be sent to the topic `<topic_prefix>/<sensor_name>` for each detected sensor.  `sensor_name` will be the device ID if an alias hasn't been added.

#### HTTP

To push updates to HTTP, configure a gateway server and a path for each sensor you want to push data for.  Example:

<img src="https://imgur.com/VDvCJmk.png" width="300" />

If you configure an HMAC secret, an HMAC of the path, body, and current timestamp will be included in the request.  This allows you to verify the authenticity of the request.  HMAC is computed for the concatenation of:

* The path being requested on the gateway server
* The body of the request
* Current timestamp

The signature and the timestamp are included respectively as the HTTP headers `X-Signature` and `X-Signature-Timestamp`.

## REST Routes

The following routes are available when the settings server is active:

* `GET /` - the settings index page
* `GET /thermometers` - gets list of thermometers 
* `GET /thermometers/:thermometer` - `:thermometer` can either be address or alias
* `GET /settings` - return settings as JSON
* `PUT /settings` - patch settings.  Body should be JSON
* `GET /about` - bunch of environment info
* `POST /update`
