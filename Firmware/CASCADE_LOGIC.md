# Temperature control: mode guide

This document describes how the controller selects a process mode, generates the
box-air target, and drives the heater and Peltier. It is organized by the modes
shown in the UI so the behavior of each mode can be found directly.

## Quick navigation

- [Control layers and temperature sources](#control-layers-and-temperature-sources)
- [Heating only](#1-heating-only)
- [Cooling only](#2-cooling-only)
- [Automatic (mixed heating/cooling)](#3-automatic-mixed-heatingcooling)
- [Shared outer cascade](#shared-outer-cascade)
- [Inner heater PID](#inner-heater-pid)
- [Autotune, stop state, and safety](#autotune-stop-state-and-safety)
- [PID settings and diagnostics](#pid-settings-and-diagnostics)
- [Preserving PID state across OTA](#preserving-pid-state-across-ota)

The mode values are defined in `CoolerBox/CoolerBox.cpp`:

| UI mode | `boxMode` | Behavior |
| --- | ---: | --- |
| Automatic | 1 | Selects a heating or cooling submode; it does not heat and cool simultaneously. |
| Cooling only | 2 | Runs the Peltier cooling controller; heater is disabled. |
| Heating only | 3 | Runs the heater PID; Peltier cooling is disabled. |

The main mode selection and actuator gates are in
[`CoolerBox/TemperatureControl.cpp`](CoolerBox/TemperatureControl.cpp).
The heater PID is in [`CoolerBox/HeatingControl.cpp`](CoolerBox/HeatingControl.cpp),
and the Peltier command/hysteresis logic is in
[`CoolerBox/CoolingControl.cpp`](CoolerBox/CoolingControl.cpp).

## Control layers and temperature sources

The controller has two levels:

1. The **outer cascade** translates the liquid target `setTemp` into the box-air
   target `effectiveBoxTarget`.
2. The **inner actuator controller** moves `boxTemp` toward
   `effectiveBoxTarget`: QuickPID drives the heater, while cooling uses
   hysteresis to command the Peltier.

When iSpindel control is enabled and its data is current and valid,
`iSpindelTemperature` is the liquid/process temperature. Otherwise,
`getFermenterTemp()` falls back to the box sensor (or temporarily to `setTemp`
while waiting for the first iSpindel reading after boot).

When iSpindel control is disabled, `effectiveBoxTarget` follows `setTemp`
directly. When iSpindel control is enabled and active, the outer cascade adjusts
the box target based on liquid error and trend. If iSpindel is enabled but
inactive/timed out, the target uses the stored outer integral as a fallback;
the outer P and D terms are not updated until a new active iSpindel report is
processed.

## 1. Heating only

**Selection:** `boxMode == 3`.

**Control path:**

1. `updateCascadeTarget()` computes/slews `effectiveBoxTarget` using the shared
   cascade behavior described below.
2. Cooling is disabled, so `processCooling()` commands Peltier voltage zero.
3. The heater PID compares filtered `boxTemp` with `effectiveBoxTarget` and
   requests heater power.

Heating is allowed when the controller is not stopped and the active mode is
heating. The fermenter emergency-temperature lockout is specific to Automatic
mode; it does not disable heating in Heating only. The Peltier interlock also
does not normally block heating here because cooling is disabled and the
commanded Peltier voltage is zero.

If `boxTemp` is above the target, reducing or cutting heater power is expected.
The heater output is set to zero immediately when the PID requests zero; thermal
inertia can make box temperature continue to fall before heat takes effect.

## 2. Cooling only

**Selection:** `boxMode == 2`.

**Control path:**

1. The shared cascade sets/slews `effectiveBoxTarget`.
2. The heater is disabled.
3. `processCooling()` evaluates box temperature against the target using
   `DEFAULT_HYSTERESIS` (0.05 °C):
   - Cooling demand turns on at or above `target + 0.05 °C`.
   - Cooling demand turns off at or below `target - 0.05 °C`.
   - Between those thresholds, the previous demand state is retained.
4. With demand, the Peltier is commanded at its configured maximum voltage,
   subject to the radiator-temperature protection cap.
5. Without demand, the command is normally `minVoltage`; it is commanded to
   zero when `boxTemp < effectiveBoxTarget - DEFAULT_HYSTERESIS`.

The Peltier control is hysteresis/on-off voltage selection, not the inner
QuickPID used for heating. A stopped controller commands zero voltage.

## 3. Automatic (mixed heating/cooling)

**Selection:** `boxMode == 1`. The active submode is `boxSubMode`:

| `boxSubMode` | Active operation |
| ---: | --- |
| 3 | Heating |
| 2 | Cooling |

Only one actuator direction is active at a time:

- In Heating submode, the heater PID runs and cooling is disabled.
- In Cooling submode, Peltier control runs and the heater is disabled.

The mode switch uses `getFermenterTemp()` (iSpindel when active, otherwise the
box temperature):

- When transitioning from Cooling-only (`boxMode == 2`) into Automatic (`boxMode == 1`),
  if `controlTemp <= setTemp`, `boxSubMode` is immediately initialized to Heating (`3`)
  so that heating can begin without delay.
- Heating switches to Cooling when the control temperature rises above
  `setTemp + 0.1 °C`.
- Cooling switches back to Heating when the control temperature is at or below
  `setTemp`.

The 0.1 °C return band avoids rapid mode toggling. In Cooling submode, Peltier
voltage is cut to zero when box temperature falls below
`effectiveBoxTarget - DEFAULT_HYSTERESIS`.

The emergency heater lockout also applies in Automatic mode when iSpindel is
active: it engages above `setTemp + 0.1 °C` and releases when liquid temperature
returns to or below `setTemp`. A stopped controller disables both actuators.

## Shared outer cascade

The outer cascade runs independently of the selected actuator mode. Its job is
to choose a safe, useful air target for maintaining the liquid temperature; the
mode logic above decides whether heating or cooling can act on that target.

### Outer PID update timing and equation

`updateCascadeTarget()` updates the setpoint ramp and liquid-temperature trend.
When iSpindel is active, it updates the outer controller only once for each new
iSpindel report. Repeated control-loop calls for the same report do not
re-integrate the error.

Let:

- `T = iSpindelTemperature`;
- `e = setTemp - T`;
- `rate` be the filtered liquid-temperature rate in °C/hour;
- `I = outerPID.GetOutputSum()`.

For each new report:

```text
u_pi = outerPID_kP * e + I
u_d  = -outerPID_kP * clamp(outerPID_tdHours * rate,
                            -outerPID_forecastLimitC,
                            +outerPID_forecastLimitC)
u    = clamp(u_pi + u_d, -cascadeMaxOffset, +cascadeMaxOffset)
targetRaw = setTemp + u
```

QuickPID supplies P and I (`pOnError`, `iAwClamp`, direct action); its built-in
D is disabled. The D forecast is calculated separately from the filtered
iSpindel trend. Before each outer `Compute()`, Ki is scaled by the elapsed time
between reports (`outerPID_kIPerHour * dtHours`). `dtHours` is bounded below by
30 seconds and above by 1.5 times the configured iSpindel interval.

Finally, `targetRaw` is rate-limited into `effectiveBoxTarget`:

- Rise: at most 0.08 °C/min.
- Fall: at most 0.04 °C/min.
- The elapsed time used for a single slew update is capped at 10 seconds.

### Trend filter and integral freeze

For each new active iSpindel report, the rate filter:

1. Computes the sample rate from elapsed report time.
2. Limits the unfiltered rate to ±0.75 °C/hour.
3. Updates an exponential average with alpha 0.15.
4. Resets the rate to zero when the report gap exceeds the allowed timeout.

The outer integral is frozen if the demanded direction is unavailable, heater
autotune is running, or the liquid is moving toward its target faster than
`outerPID_freezeRateCPerHour`. P and D continue to be calculated during an
integral freeze; the previous integral sum is restored after `Compute()`.

Direction availability reflects the actuator mode and its current state. For
example, Heating only cannot increase heat by changing the box target while the
heater is already off, and Cooling only cannot increase cooling unless a
cooling demand is active.

### Outer PID initialization and fallback

- The initial integral offset is `boxTemp - setTemp`, bounded by
  `±cascadeMaxOffset`. If box temperature is not yet valid, initialization waits
  for a valid measurement.
- Enabling iSpindel control initializes the outer loop from the current box
  temperature.
- When transitioning from Cooling-only mode (`boxMode == 2`) into Automatic
  (`boxMode == 1`) or Heating-only (`boxMode == 3`) while active, any negative
  outer integral (`outerPID.GetOutputSum() < 0`) accumulated during Peltier cooling
  is purged: `initializeOuterPIDFromBox()` re-seeds the outer loop to
  `constrain(boxTemp - setTemp, -cascadeMaxOffset, cascadeMaxOffset)` (or resets
  it to `0.0` if box temperature is invalid), and `effectiveBoxTarget` and
  `lastSlewMillis` are immediately aligned to prevent delay from slew-rate recovery.
- A setpoint change or ramp step does not itself reset the outer integral.
- If iSpindel becomes inactive while still configured, the target falls back to
  `setTemp + outerPID.GetOutputSum()`; no new P/D calculation or integration
  occurs until a fresh report is processed.
- If iSpindel control is disabled, the target is `setTemp`; the stored outer
  integral remains available if iSpindel control is enabled again.

### Why the equilibrium box target can differ from the liquid target

The heater is near the bottom of the chamber and transfers much of its heat
directly to the fermenter. The box sensor is near a wall that can be cooled by
the surrounding environment. Consequently, stable liquid temperature may
require box air to remain below `setTemp`; the outer integral `I` learns this
offset. For a cool environment, a negative equilibrium offset is plausible and
does not by itself indicate an error.

## Inner heater PID

This section applies whenever the heater is enabled: Heating only, Automatic
Heating submode, and the heating-autotune override.

- Input: an EMA-filtered box temperature. The filter alpha is 0.25 to smooth
  DS18B20 quantization.
- Setpoint: `effectiveBoxTarget` in normal operation.
- Compute interval: 2 seconds (`PIDCOMPUTE_INTERVAL`).
- QuickPID modes: proportional on error, derivative on measurement, integral
  anti-windup clamp, direct action.
- Output limits: 0–100 percent. The requested hardware PWM is
  `pidOutput / 100 * maxHotPWM`.
- Positive PWM changes are ramped by at most 100 hardware PWM units per control
  call. A zero/nonpositive request bypasses the ramp and turns the heater off
  immediately.
- When heating is disabled, the filtered input is reset, the heater is switched
  off immediately, and the exposed `pidOutput` is cleared. The debug capture
  preserves the last actual PID terms/output separately so a forced-off zero is
  distinguishable from a PID-computed zero.

The inner heater PID does not control the Peltier. Cooling-only and Automatic
Cooling submode do not run the heater PID.

## Autotune, stop state, and safety

Heating autotune overrides normal mode control: it disables cooling, keeps the
fan off, and uses its own target and relay-style heater sequence. Heating is
permitted only when the Peltier voltage safety check passes. The outer integral
is frozen during autotune.

When the controller is stopped, normal heating and cooling are disabled and the
Peltier command is zero. On entering heating after a stop or major mode change,
the heater PID is reset and initialized for the new operation.

During cooling, radiator overtemperature protection progressively caps the
maximum Peltier voltage while `HotTemp` is at or above
`RADIATOR_EXTREME_TEMP` (45 °C), then restores the cap as the radiator cools.

## PID settings and diagnostics

### Outer cascade settings

The PID settings page configures the outer cascade and maximum box-target
offset. Settings are persisted in EEPROM and range-checked.

| Setting | Default | Allowed range | Meaning |
| --- | ---: | ---: | --- |
| `outerPID_kP` | 3.0 | 0.1–10.0 | Proportional gain |
| `outerPID_kIPerHour` | 1.0 /h | 0.001–2.0 /h | Integral rate |
| `outerPID_tdHours` | 0.5 h | 0–6.0 h | D forecast horizon; zero disables the forecast |
| `outerPID_forecastLimitC` | 0.25 °C | 0–2.0 °C | Limit on forecast temperature change |
| `outerPID_freezeRateCPerHour` | 0.1 °C/h | 0–2.0 °C/h | Rate threshold for freezing the integral |
| `cascadeMaxOffset` | 4.0 °C | 1–10 °C | Limit on outer-loop box-target offset |

The source constants also include a ±0.75 °C/hour input-rate clamp, filter
alpha 0.15, and box-target slew limits described above.

### Inner heater settings

The inner PID has separate `hotPID_kP`, `hotPID_kI`, and `hotPID_kD` settings.
They tune heater response to box-air error, not the liquid-temperature
cascade. Changing these does not change the outer cascade. The firmware’s
current defaults are defined in `CoolerBox/Globals.h`; the heating autotune can
provide an initial estimate, which should be verified under normal operating
conditions.

### Reading the outer PID debug line

For each processed iSpindel report, the PID log contains a line similar to:

```text
Outer PID: e=0.163 rate=0.011 P=0.975 I=-2.618 D=-0.033 u=-1.676 freeze=none
```

- `e`: `setTemp - liquid temperature`, in °C. Positive means the liquid is
  below target.
- `rate`: filtered liquid-temperature rate in °C/hour. Positive means warming.
- `P`, `I`, `D`, `u`: outer-loop terms/offset in °C.
- `freeze`: why outer integration is paused, or `none`.

The D term is negative while the liquid is warming and positive while it is
cooling. The resulting `u` is constrained by `cascadeMaxOffset`, then the
resulting target is subject to its separate slew limits.

### Diagnosing a temperature trace

For a heater-power drop, compare `boxTemp` with `effectiveBoxTarget` and
determine which path caused the drop:

1. If heating is not allowed, check the stop state, active mode/submode, and
   Peltier interlock.
2. If heating is allowed but PID output is zero, inspect the inner P/I/D terms
   and the sign/magnitude of `effectiveBoxTarget - boxTemp`.
3. If PID request is positive but applied PWM is lower, account for the
   per-call PWM ramp; a forced-off gate cuts output immediately.
4. Check whether `effectiveBoxTarget` is moving. The inner loop may be
   accurately following a changing cascade target rather than holding a fixed
   target.
5. Expect a delay between PWM changes and box-temperature response because of
   sensor filtering and thermal inertia.

The optional fast PID diagnostic capture records the inner PID terms/request,
applied PWM, heating permission, and cutoff reason. Its flags and sample format
are defined in `CoolerBox/PidFastDebug.h`; compile-time enablement is controlled
by `ENABLE_PID_FAST_DEBUG` in `CoolerBox/Globals.h`.

## Preserving PID state across OTA

The `/update` page has a **Preserve PID state across update** option (off by
default). When enabled for a firmware or LittleFS update, the controller saves:

1. `outerPID.GetOutputSum()` — the outer-loop integral offset;
2. `effectiveBoxTarget` — the current box target;
3. `hotPID.GetOutputSum()` — the heater PID integral;
4. `setTemp` and a CRC.

The data is duplicated in RTC user memory and EEPROM. After reboot, it is
restored only if valid and the target has not changed by more than 2.0 °C. The
saved copy is invalidated after the one-time handoff.
