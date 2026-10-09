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

The firmware serves its main pages from compiled-in HTML; no filesystem image
is needed to build or upload the firmware.
