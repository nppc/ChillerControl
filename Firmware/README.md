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

The firmware serves its main pages from compiled-in HTML; no filesystem image
is needed to build or upload the firmware.
