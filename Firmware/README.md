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
the endpoint path (for example, `/telemetry`). Box snapshots are stored in a
48-record RAM ring (24 bytes per record); iSpindel readings are stored separately
in a 10-record RAM ring. When a ring fills, it keeps every second record,
doubling its sampling interval until its maximum stride (16 for box records,
8 for iSpindel readings); at that maximum, new records replace the oldest.
Successful uploads reset each ring's stride to one. Assuming the configured
box snapshot interval is 60 seconds, the box ring's retained history grows as
it thins:

| Stride | Box record interval | Coverage in 48 slots |
| --- | --- | --- |
| 1 | 1 minute | 48 minutes |
| 2 | 2 minutes | 1 hour 36 minutes |
| 4 | 4 minutes | 3 hours 12 minutes |
| 8 | 8 minutes | 6 hours 24 minutes |
| 16 | 16 minutes | 12 hours 48 minutes (12.8 hours) |

Each POST sends
iSpindel readings first, followed by up to 12 box snapshots, each with its own
timestamp. Box values include temperatures, voltage, current, PWM, state, RSSI,
and the device's current LAN IP address on the last box record in the chunk.
iSpindel values include temperature, gravity, battery, RSSI, angle, and interval.
Values are sent at their stored precision (temperatures/electrical values and
angle/battery at two decimals; gravity at three decimals). Missing measurements
are omitted. Chunks rejected with HTTP 400, 413, or 422 are dropped rather than
retried; other failed uploads remain queued and use progressive backoff.
Server certificate validation is disabled; configure this only on a network
where accepting an unverified certificate is acceptable.

## Wi-Fi behavior

In normal operation, the Wemos uses both Wi-Fi interfaces at once:

- **Device access point (AP):** The device starts its configured network name,
  password, and hidden/show setting. It allows one client. The AP is initially
  configured for channel 1; because the AP and router connection share one radio,
  the active radio channel is not independently pinned for the AP.
- **Router connection (STA):** At boot, the firmware explicitly starts a
  connection using the saved router name and password. It waits up to about
  10 seconds for the first connection. Automatic connection on boot is disabled,
  but the ESP8266 Wi-Fi library's automatic reconnect is enabled. While the
  router connection is down, the firmware also calls `WiFi.reconnect()` on a
  60-second timer. That timer starts at boot; it is not restarted when a drop is
  detected. Reconnection uses the saved credentials and does not pin a BSSID or
  channel.
- **Radio settings:** Wi-Fi sleep is disabled and the transmitter is set to
  20.5 dBm (maximum configured output).
- **AP address:** Without a router IP, the AP uses its existing address or
  defaults to `192.168.4.1`. When a router IP is available, the firmware chooses
  a private AP subnet that does not overlap the router subnet and configures the
  AP with that address. It checks this at startup, after the router receives an
  IP address, and during its regular Wi-Fi status checks. When connected, it
  also configures the AP's DHCP server with the router's DNS server when one is
  available.

If the boot connection does not succeed within the initial wait, the AP and
web server still start; automatic reconnect and the periodic reconnect call
continue to try the saved router credentials.

The firmware checks router connectivity every 3 seconds. It considers the
connection available only when Wi-Fi reports connected and a non-zero local IP
is present. A detected loss is recorded for the Debug page; a later connection
is also logged there. The Debug page shows the current router SSID, RSSI, and IP
when connected, and the most recent detected drop.

The `/ispindel` and `/api/ispindel` POST endpoints accept iSpindel data through
the device AP. These endpoints are not subject to the ordinary web-page access
restriction. Other web requests from the AP are blocked by default unless
**Allow web pages on the local access point** is enabled in Network settings.
Connections through the router are not blocked by that AP-only restriction.
Detailed Wi-Fi/AP and telemetry transport diagnostics, including iSpindel
client MAC/RSSI, are compiled only when `WIFI_DEBUG` is defined in
`CoolerBox/Globals.h`. Comment out that definition to disable the detailed
diagnostics. Generic status and error messages remain available either way.
The stall-detection ticker and loop-stage diagnostics are disabled by default;
define `STALL_DEBUG` in `CoolerBox/Globals.h` to enable them.

In emergency mode (double-reset recovery), the router connection is not started.
The device provides an open AP named `Cooler` at `192.168.4.1`, with up to four
clients, for recovery access.

NTP time synchronization starts asynchronously in normal mode using
`pool.ntp.org` and `time.google.com`; the configured timezone controls displayed
log times. Emergency mode does not start NTP. Telemetry uploads require a valid
clock and a connected router. When uploads cannot proceed, telemetry snapshots
are retained in the queue and sent later in chunks; failed uploads use
progressive backoff.

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

$$\text{effectiveBoxTarget} = T_{\text{baseline}} + K_{\text{direction}} \times (\text{setTemp} - T_{\text{liquid}})$$

- **Thermal Equilibrium Baseline ($T_{\text{baseline}}$)**: In real-world environments (such as a cooler
  box on a cold balcony), the natural passive air temperature settles between the fermenter and ambient
  (for example, ~16.7°C when ambient is 14°C and wort is 18.0°C). Rather than hardcoding the air target baseline
  to `setTemp` (which would demand 18°C air and trigger violent heating bursts when wort drops to 17.9°C), the master
  loop smoothly adapts $T_{\text{baseline}}$ toward the steady-state box temperature when holding near target.
  The cascade offset then adjusts gently relative to this true thermal balance point.
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
- **Cooling**: Modulates the Peltier thermoelectric cooler via I2C DC/DC buck converter voltage.
  Aggressive cooling action with cut to 0.0V when `boxTemp` is at or below `effectiveBoxTarget`.
- **Heating**: Driven by QuickPID modulating PWM to the heating wire (up to `MAXIMUM_PWM_HOT_WIRE`).
  Gentle action, strictly locked out while the fermenter liquid is at or above target temperature
  ($T_{\text{liquid}} \ge \text{setTemp}$), preventing the powerful heater from firing while the beer is hot.
- **Asymmetric Mode Switcher**: In Automatic mode, the primary control temperature governs the active
  regime with hysteresis ($\pm 0.05^\circ\text{C}$). Cooling is maintained while the liquid is above target;
  heating engages only when liquid drops below target.
- **Automatic Fan Control**: The hot-side radiator fan stays on during cooling and turns off
  only when the hot temperature drops to or below box temperature ($T_{\text{hot}} \le T_{\text{box}}$),
  preventing heat soak back into the chamber (runs continuously if only a single sensor is configured).

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
