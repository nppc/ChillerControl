# CoolerBox firmware

This is the PlatformIO project for the Wemos D1 mini (ESP8266).

In VS Code, the recommended PlatformIO IDE extension (suggested by this
workspace) provides Build, Upload, and Serial Monitor actions. On Windows, if
you are using the terminal, run these PowerShell commands from this directory:

```powershell
$pio = Join-Path $env:USERPROFILE ".platformio\penv\Scripts\platformio.exe"
& $pio run
& $pio run -t upload
& $pio device monitor
```

The regular PlatformIO Build action also regenerates the LittleFS filesystem
image from `data`, as well as building the firmware.

The PlatformIO IDE installs its Core in this virtual environment. A bare
`python -m platformio` uses whichever Python happens to be on `PATH` and can
fail with `No module named platformio`; use the executable above instead.
If it is missing, install/enable the PlatformIO IDE extension or install
PlatformIO Core for the Python environment you intend to use.

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

The firmware serves its main pages from LittleFS templates. Settings are
stored in emulated EEPROM, separate from the LittleFS partition, so uploading a
LittleFS image does not replace them. The ESP8266 LittleFS mount may format the
filesystem if it cannot mount it, erasing files managed through the File
Browser; back those files up before migrating from SPIFFS or before uploading
a filesystem image. PlatformIO is configured to build LittleFS images for
`uploadfs`.
The Settings page reloads its saved values from EEPROM and scans the OneWire
bus directly, validating DS18B20 family codes and address CRCs before rendering
the temperature sensor selectors. This avoids relying on DallasTemperature's
cached device count for discovery.
