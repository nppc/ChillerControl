# CoolerBox firmware

This is the PlatformIO project for the Wemos D1 mini (ESP8266).

From this directory, run:

```sh
python -m platformio run
python -m platformio run -t upload
python -m platformio device monitor
```

PlatformIO builds the C++ sources in `CoolerBox` and installs their library
dependencies from `platformio.ini`. ArduinoJson uses the latest release
available in the PlatformIO registry; the firmware uses its v7 `JsonDocument`
API. Because the dependency is unpinned, future builds may resolve to a newer
release. Set `upload_port` in `platformio.ini` if PlatformIO does not detect
the board's serial port.

Telemetry is sent as a JSON `POST` to the configured HTTPS endpoint at the
configured interval. The Network page builds the URL from the endpoint address
(for example, `https://aa.ddnss.eu/api/v1`), the Access Token path segment, and
the endpoint path (for example, `/telemetry`). The payload includes temperature,
set temperature, hot-side temperature, voltage, current, heater PWM, and the
device's current LAN IP address. Server certificate validation is disabled;
configure this only on a network where accepting an unverified certificate is
acceptable. iSpindel-based dynamic temperature adjustment is disabled with
`ENABLE_ISPINDEL_DYNAMIC_TEMP` until an input source is implemented.
Cooling is on/off: it requests the configured maximum voltage when the box
temperature is at or above the target, and requests zero below target. The
buck converter's configured voltage-change rate continues to smooth power
transitions. Heating remains under PID control.

The firmware serves its main pages from compiled-in HTML; no filesystem image
is needed to build or upload the firmware. Settings are stored in emulated
EEPROM, separate from the LittleFS partition, so uploading a LittleFS image
does not replace them. On first boot after this firmware update, existing
`/settings.json` values in LittleFS are migrated to EEPROM automatically.
Subsequent settings changes are saved only to EEPROM. The ESP8266 LittleFS
mount may format the filesystem if it cannot mount it, erasing files managed
through the File Browser; back those files up before migrating from SPIFFS or
before uploading a filesystem image. PlatformIO is configured to build
LittleFS images for `uploadfs`.
The Settings page reloads its saved values from EEPROM and scans the OneWire
bus directly, validating DS18B20 family codes and address CRCs before rendering
the temperature sensor selectors. This avoids relying on DallasTemperature's
cached device count for discovery.
