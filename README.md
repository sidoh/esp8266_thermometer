# esp8266_thermometer
ESP8266-based thermometer. Pushes temperature data to a URL at a configurable interval. Suitable for battery power.  Works with multiple probes.

## Parts

* ESP8266. I'm using a NodeMCUs.
* DS18B20 temperature probe(s). Probably works with other Dallas Instruments temperature probes.
* (optional) batteries and battery holder. I've had good luck with a Li-Ion 18650 cell.

## Circuit

Not even worth drawing out. Data line from the DS18B20 is connected to GPIO 2, and has a 4.7 KΩ pullup resistor.  You can configure a different data pin later.

## Configuring

#### WiFi

I used `WifiManager`, so assuming your chip didn't already have WiFi configured, it'll start in AP mode (network will be named something like Thermometer_XXXX). **The password is `fireitup`**. Connect to this network to configure WiFi.

#### Other settings

The other settings are stored as JSON in SPIFFS. When the chip is unconfigured, it starts a web server on port 80. Navigate here to edit the settings. Saving will reboot the device with the new settings.

#### Breaking out of deep sleep cycle

When the device is configured, it'll push temperature data and enter deep sleep. When it's in deep sleep, it's pretty much off. Each time it wakes, though, it checks if it can connect to the "flag server" (configured in the JSON blob), and if the flag server sends the string "update", it'll boot into settings mode. I just use this on the flag server:

```
$ echo -ne 'update' | nc -vvl 31415
```

#### OTA updates

You can push firmware updates to `POST /update` when in settings mode.

## Routes

The following routes are available when the settings server is active:

* `GET /` - the settings index page
* `GET /thermometers` - gets list of thermometers 
* `GET /thermometers/:thermometer` - `:thermometer` can either be address or alias
* `GET /about` - bunch of environment info
* `POST /update`

## Security

If you want to verify authenticity of requests sent from this device, you can specify an HMAC secret. Temperature data sent to the gateway server will include an HMAC for the concatenation of:

* The path being requested on the gateway server
* The body of the request
* Current timestamp

The signature and the timestamp are included respectively as the HTTP headers `X-Signature` and `X-Signature-Timestamp`.
