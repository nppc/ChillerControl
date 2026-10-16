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
the endpoint path (for example, `/telemetry`). The payload includes box temperature,
fermenter liquid temperature (when active), active control temperature, box target,
hot-side temperature, voltage, current, heater PWM, control source, and the
device's current LAN IP address. Server certificate validation is disabled;
configure this only on a network where accepting an unverified certificate is
acceptable.

Cooling requests the configured maximum voltage when the box temperature is at or
above the active target, and requests zero below target. The buck converter's
configured voltage-change rate continues to smooth power transitions. Heating
remains under QuickPID control.

## Cascade Temperature Control Architecture

CoolerBox uses a dual-loop **Cascade Control** architecture to accurately regulate
fermentation liquid temperature despite large thermal mass, thermal lag, and
infrequent telemetry updates from wireless hydrometers (iSpindel).

```
                      +---------------------------------------+
                      |          iSpindel Hydrometer          |
                      |     (Sends T_liquid every 5 min)      |
                      +-------------------+-------------------+
                                          |
                                          v  HTTP POST /ispindel
+-------------------+      +----------------------------------+
| User Setpoint     | ---> |           MASTER LOOP            |
| (e.g. 18.0 °C)    |      | Computes dynamic Box Air Target  |
+-------------------+      +-----------------+----------------+
                                             |
                                             v effectiveBoxTarget
                           +----------------------------------+
+-------------------+      |            SLAVE LOOP            |
| Box Air Sensor    | ---> |        (Runs every 2 sec)        |
| (DS18B20)         |      | QuickPID Heating + Peltier Cool  |
+-------------------+      +-----------------+----------------+
                                             |
                                             v
                           +----------------------------------+
                           |     Heating Wire / Peltier       |
                           +----------------------------------+
```

### 1. The Challenge
Fermenting beer (typically 20–30 L) has high heat capacity and reacts very slowly
to changes in surrounding ambient air temperature. Wireless hydrometers like the
iSpindel sleep between transmissions to conserve battery, reporting temperature
only once every 5 - 10 minutes. Feeding this slow, infrequent reading directly into a
2-second PID loop causes severe overshooting, oscillations, or integral windup.

### 2. Master Loop (Liquid Error $\rightarrow$ Dynamic Box Air Setpoint)
When an iSpindel packet arrives via `POST /ispindel` or `POST /api/ispindel`, or when
the user changes the target temperature, the master loop executes:

$$\text{effectiveBoxTarget} = \text{setTemp} + K_{\text{direction}} \times (\text{setTemp} - T_{\text{liquid}})$$

- **`cascadeGain`** (default `1.5`, adjustable in Settings `0.5`–`5.0`): Sets the
  base heating gain. Heating starts at this value; cooling starts at twice this value
  (capped at `5.0`) to account for the much lower cooling capacity. The two active
  gains adapt independently within `0.5`–`5.0`.
- **Online gain adaptation**: Uses only new, active iSpindel readings. If the liquid
  error is at least `0.15°C` and is not improving over three consecutive readings,
  the gain for the active direction increases by `0.1`. If the liquid crosses the
  setpoint, the previous direction's gain decreases by `0.1`. Adaptation is skipped
  when that direction is not available in the selected control mode or its requested
  box target has already reached the configured offset limit. Learned gains are
  runtime-only and restart from the configured base gain after reboot.
- Example: with an `18.0°C` target and liquid at `18.5°C`, cooling starts with gain
  `3.0`, giving a box target of `16.5°C` before the safety clamp is applied.
- **`cascadeMaxOffset`** (default $\pm 4.0^\circ\text{C}$, adjustable `1.0`–`10.0`):
  Enforces a strict safety envelope around `setTemp`:
  $$\text{effectiveBoxTarget} \in [\text{setTemp} - \text{cascadeMaxOffset},\, \text{setTemp} + \text{cascadeMaxOffset}]$$
  This prevents extreme air temperatures that could stress yeast, freeze liquid, or
  cause excessive overshoot.

### 3. Slave Loop (Box Air $\rightarrow$ Physical Actuators)
The slave loop runs on a fast, deterministic 2-second cycle (`PIDCOMPUTE_INTERVAL`):
- Regulates the physical box air temperature (`boxTemp`) toward `effectiveBoxTarget`.
- **Heating**: Driven by QuickPID modulating PWM to the heating wire (up to `MAXIMUM_PWM_HOT_WIRE`).
- **Cooling**: Modulates the Peltier thermoelectric cooler via I2C DC/DC buck converter voltage.
- **Dual-Mode Tendency Switcher**: Employs a 5-minute sliding observation window
  (`MODE_SWITCH_WINDOW_SEC`) and a deadband ($\pm 0.05^\circ\text{C}$) to prevent rapid
  toggling between heating and cooling near the setpoint.

### 4. Automatic Fallback & Safety
- **Dynamic Packet Timeout**: The firmware extracts the `interval` field (in seconds)
  from the iSpindel payload. If no packet is received for $2 \times \text{interval}$
  (or if iSpindel mode is disabled in Settings), the system smoothly reverts to direct
  box air control (`effectiveBoxTarget = setTemp`).
- **Status Indication**: When the iSpindel drops offline, the web UI automatically
  reverts the primary card to Box Sensor and indicates fallback mode. As soon as a new
  packet arrives, cascade control resumes immediately.
- **Plausibility Filters**: Incoming liquid temperature values are validated against
  physical bounds ($0.0^\circ\text{C}$ to $45.0^\circ\text{C}$) before acceptance.

### 5. Autocalibration (Relay / Åström–Hägglund)
The heating autotune feature (`/autotune`) operates strictly on the fast slave loop
(air $\rightarrow$ heating wire). It bypasses cascade setpoint calculation, allowing
heating PID constants ($K_p, K_i, K_d$) to calibrate within 5–10 minutes without
being distorted by liquid thermal inertia.
