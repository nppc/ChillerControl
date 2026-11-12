# Temperature cascade control

This document describes how the firmware converts the desired fermenter
temperature (`setTemp`) into a box-air target, how that target is followed, and
which constants control the behavior. The cascade is the slow master loop:
fermenter temperature is its process value, and the box-air PID is the faster
slave loop that follows `effectiveBoxTarget`.

The main implementation is `CoolerBox/TemperatureControl.cpp`. iSpindel
measurements and their reported interval arrive through
`CoolerBox/CoolerBox.cpp`; cascade status is exposed by
`CoolerBox/status.cpp`.

## Control flow

1. **Advance the fermenter setpoint ramp.** `setTemp` is the current ramp
   setpoint; `rampTargetTemp` is its eventual destination. Ramp increments
   happen every configured interval. On the first active iSpindel report after
   boot, a running ramp resumes from one increment beyond the measured liquid
   temperature, clamped between the current setpoint and ramp destination.
2. **Update the liquid trend.** Each new active iSpindel sample updates a
   filtered rate and a half-hour temperature forecast. A stale/invalid sensor
   does not provide a trend.
3. **Reset or adapt control state.** A changed base gain or a setpoint change
   reinitializes the directional gains and baseline. Otherwise, new iSpindel
   samples may adapt the gain for the current heating/cooling direction.
4. **Compute the raw box target.** With active iSpindel data, the predicted
   liquid error is multiplied by the applicable gain and added to the learned
   box thermal baseline. Without active data, the target either holds the box
   baseline while waiting for/recovering from iSpindel data, or directly
   follows `setTemp` when iSpindel control is disabled.
5. **Limit movement of the box target.** `effectiveBoxTarget` slews toward the
   raw target at separate up/down rates. It is then passed to the box-air
   heating/cooling control in `processControl()`.
6. **Apply operating-mode and safety gates.** Auto mode selects heating or
   cooling using the measured control temperature. Cooling demand and heater
   operation also have separate gates; see [Mode switching and interlocks](#mode-switching-and-interlocks).

`updateCascadeTarget()` is called from `processControl()` and also after
temperature changes or iSpindel data arrives. The ramp, prediction, gain
adaptation, baseline, and slew therefore share one update path.

## When iSpindel data is active

`isISpindelActive()` requires all of the following:

- iSpindel temperature control is enabled (`useISpindelTemp`).
- At least one report has arrived (`lastISpindelMillis != 0`).
- The last report is younger than `max(45 minutes, 2.5 × iSpindelIntervalSec)`.
- Temperature is strictly between -50 °C and 100 °C and is not exactly 0 °C.

The iSpindel report interval defaults to 900 seconds (15 minutes). A valid
reported interval from 30 to 86,400 seconds replaces it.

When active, the iSpindel temperature is the fermenter measurement. When
iSpindel control is enabled but no first report has arrived, `getFermenterTemp()`
returns `setTemp` to avoid reacting to the box sensor as if it were the wort.
After a report becomes stale or invalid, it falls back to `boxTemp`. The cascade
target in either inactive case is separately held at the valid box baseline
(or `setTemp` if no valid baseline exists). When iSpindel control is disabled,
the cascade is bypassed and the box target directly equals `setTemp`.

Box-temperature validity uses the same range and zero exclusion. This test is
for selecting/learning the baseline; it does not itself disable the box PID.

## Forecast and target calculation

For each new active iSpindel report, the rate sample is the liquid temperature
change divided by elapsed hours. The sample is limited to ±0.75 °C/hour and
low-pass filtered. The forecast is the current temperature plus filtered rate
over 0.5 hour, with the forecast delta limited to ±0.75 °C. If the gap between
trend samples exceeds the active-data timeout, the filtered rate is reset to
zero. The current liquid temperature—not the forecast—is still used for gain
adaptation and mode/safety decisions.

The predicted error is:

`predictedError = setTemp - predictedFermenterTemp()`

Heating and cooling have independent gains. Heating gain is selected whenever
the controller is in a heating mode, or predicted error is non-negative;
otherwise cooling gain is used. The initial offset is the predicted error
multiplied by that gain, clamped to ±`cascadeMaxOffset`.

There is an extra heating floor: while the actual liquid is more than
`CASCADE_TARGET_RELEASE_DEADBAND` below `setTemp`, the offset cannot be less
than `(actual liquid error - release deadband) × heating gain`, also capped by
`cascadeMaxOffset`. This preserves some heating boost even when a warming trend
reduces the forecast error. It does not impose a corresponding cooling floor.

The offset is applied to `cascadeBoxBaseline`, not directly to `setTemp`. The
final raw target is clamped to `[setTemp - cascadeMaxOffset, setTemp +
cascadeMaxOffset]`.

## Thermal baseline

The baseline represents the box-air temperature that has been observed while
the fermenter is near its target:

- Reset initializes it from valid `boxTemp`; without a valid box reading it is
  set to `setTemp` and marked uninitialized.
- The first later valid box reading initializes an uninitialized baseline.
- On each new iSpindel report, if actual liquid error is within
  `CASCADE_ADAPT_DEADBAND`, the baseline moves 15% toward current box
  temperature (85% old baseline + 15% current reading).
- The baseline is constrained to within `cascadeMaxOffset` of `setTemp` when a
  new report is processed.
- With iSpindel disabled, the baseline is set to `setTemp`. While enabled but
  inactive, it is retained as the hold target so the box is not driven
  aggressively while telemetry is absent.

The baseline can be queried as `cascadeBoxBaseline` and is included in the
status response.

## Adaptive directional gains

On reset, heating gain is `cascadeGain` clamped to the allowed range; cooling
gain is `cascadeGain × CASCADE_COOLING_GAIN_MULTIPLIER`, also clamped. These
are separate in-memory values.

Adaptation runs at most once per new active iSpindel report:

- Liquid error is `setTemp - iSpindelTemperature`. Positive means heating is
  needed; negative means cooling.
- Errors strictly smaller than `CASCADE_ADAPT_DEADBAND` clear the
  non-improving count and do not adapt.
- Gain is adapted only if that direction is available: the cooler must not be
  stopped; in Auto mode the matching heating/cooling submode must be active;
  in fixed modes the matching heating-only/cooling-only mode must be active.
- If liquid error crosses the setpoint (changes direction), the gain for the
  previous direction is reduced by one step, and the new direction begins a
  fresh count.
- Otherwise, consecutive reports whose absolute error has not decreased count
  toward `CASCADE_ADAPT_READINGS`. At that threshold the gain is increased by
  one step, unless `abs(liquidError × gain)` already reaches
  `cascadeMaxOffset`. The count is then cleared.
- A long report gap (at least the active-data timeout) or a setpoint change
  resets the adaptation sequence. A direction that is unavailable also clears
  it.
- Every gain adjustment is clamped to the configured minimum/maximum. The
  current gains are reported in status, but are not persisted across restart.

## Target slew

The first target update initializes `effectiveBoxTarget` directly to the raw
target. Later updates move it toward that target by at most the direction's
per-second limit multiplied by elapsed time. Elapsed time used for this
calculation is capped at 10 seconds, so a long pause does not cause a large
catch-up jump. The upward and downward limits are intentionally different.

## Mode switching and interlocks

- In Auto mode (`boxMode == 1`), heating switches to cooling only when the
  measured control temperature is strictly greater than `setTemp` plus
  `FERMENTER_EMERGENCY_OVERTEMP`. Cooling switches back to heating as soon as
  the measured control temperature is at or below `setTemp`. The transition
  uses `getFermenterTemp()` (iSpindel when active, otherwise its documented
  fallback), not the trend forecast.
- The fermenter-temperature gate allows cooling unconditionally when iSpindel
  is inactive; with active iSpindel it allows cooling only while measured
  liquid temperature is above `setTemp`. Operating mode and stopped state
  still determine whether the cooling loop runs.
- The emergency heat lockout latches when active iSpindel temperature is
  strictly above `setTemp + FERMENTER_EMERGENCY_OVERTEMP` and releases when it
  returns to or below `setTemp`. It gates heating in Auto mode. In heating-only
  mode the cascade target instead lowers naturally as the liquid approaches
  target; this hard lockout is intentionally not applied there.
- Heating is also gated by stopped state and the Peltier voltage interlock,
  which prevents simultaneous heater operation when the Peltier is commanded
  on and measured voltage is at/above its safety threshold. The threshold is
  `max(2.5 V, minVoltage + 0.5 V)`.
- In Auto cooling submode and cooling-only mode, the cooling zero-voltage
  condition is set when `boxTemp < effectiveBoxTarget - DEFAULT_HYSTERESIS`.
  This uses the effective box target, not the fermenter setpoint.
- Heating autotune bypasses normal cascade output/mode handling and controls
  against its autotune target.

## Constant reference

Values below are defined in `TemperatureControl.cpp` unless noted. Temperatures
are in °C.

| Constant | Default | Role |
| --- | ---: | --- |
| `CASCADE_MAX_OFFSET` (`TemperatureControl.cpp`, externally declared in `TemperatureControl.h`) | 4.0 °C | Maximum permitted cascade offset from `setTemp`; initializes `cascadeMaxOffset`. Also clamps the predicted offset, heating floor, baseline, and final raw target. |
| `CASCADE_GAIN_MIN` | 0.5 | Lower bound for each learned directional gain. |
| `CASCADE_GAIN_MAX` | 5.0 | Upper bound for each learned directional gain. |
| `CASCADE_GAIN_STEP` | 0.1 | Gain increase/decrease per adaptation event. |
| `CASCADE_COOLING_GAIN_MULTIPLIER` | 1.5 | Scales configured base gain to initialize cooling gain; then cooling gain adapts independently. |
| `CASCADE_ADAPT_DEADBAND` | 0.15 °C | Inside this liquid-error band gain adaptation is suppressed; also marks the near-target condition for baseline learning. |
| `CASCADE_TARGET_RELEASE_DEADBAND` | 0.1 °C | Distance below target that is excluded when calculating the minimum heating offset. |
| `CASCADE_ADAPT_READINGS` | 3 reports | Number of consecutive non-improving liquid-error reports before trying to increase the active-direction gain. |
| `DEFAULT_CASCADE_GAIN` | 2.0 | Initial configured base gain; cooling starts at base × cooling multiplier. |
| `FERMENTER_PREDICTION_HORIZON_HOURS` | 0.5 h | Forecast horizon used for the liquid trend. |
| `FERMENTER_MAX_RATE_C_PER_HOUR` | 0.75 °C/h | Clamp on each measured rate sample before filtering. |
| `FERMENTER_MAX_PREDICTION_C` | 0.75 °C | Maximum forecast delta from the current iSpindel temperature. |
| `FERMENTER_RATE_FILTER_ALPHA` | 0.15 | Weight of a new rate sample in the filtered rate (15% new, 85% prior). |
| `FERMENTER_EMERGENCY_OVERTEMP` | 0.1 °C | Auto-mode heat-to-cool threshold and emergency heater-lockout threshold above `setTemp`. |
| `BOX_TARGET_MAX_SLEW_UP_PER_SEC` | 0.08 °C/min | Maximum upward movement of `effectiveBoxTarget`. |
| `BOX_TARGET_MAX_SLEW_DOWN_PER_SEC` | 0.12 °C/min | Maximum downward movement of `effectiveBoxTarget`. |
| `DEFAULT_HYSTERESIS` (`CoolerBox/Globals.h`) | 0.05 °C | Half-width used by cooling control; cascade uses it to set the cooling zero-voltage condition below the effective box target. |

Other numeric conditions that affect cascade operation:

| Condition/value | Role |
| --- | --- |
| `max(45 minutes, 2.5 × iSpindel interval)` | iSpindel freshness timeout; also the maximum gap accepted for retaining the trend rate and the gap that resets gain-adaptation progress. |
| iSpindel interval 30–86,400 seconds | Accepted range for a reported sampling interval. |
| Box/iSpindel temperature `-50 < temperature < 100`, and temperature != 0 | Validity gate for sensor readings used by cascade/fallback logic. |
| Gain configuration change >= 0.001 | Detects a changed base gain and resets adapted gains/baseline. |
| Setpoint change >= 0.01 °C | Resets adapted gains/baseline and gain-adaptation progress. |
| Maximum elapsed time 10 seconds per target update | Caps the time multiplier used by the target slew limiter. |
| Peltier safe threshold `max(2.5 V, minVoltage + 0.5 V)` | Additional guard for heater operation when the Peltier is commanded on. |

Related non-constant state/inputs:

| Name | Default / range | Role |
| --- | --- | --- |
| `cascadeGain` | 2.0; directional reset clamps to 0.5–5.0 | Configured base from which heating and cooling gains initialize. |
| `cascadeMaxOffset` | 4.0 °C | Runtime offset/baseline limit initialized from `CASCADE_MAX_OFFSET`. |
| `iSpindelIntervalSec` | 900 seconds; accepted reports 30–86,400 | Sets the report freshness timeout together with the 45-minute minimum. |
| `heatingCascadeGain`, `coolingCascadeGain` | Reset from base gain; each 0.5–5.0 | Learned in-memory gains used to calculate the target. |
| `cascadeBoxBaseline` | Initially 18.0 °C, then reset/learned | Learned thermal-equilibrium anchor for box target. |
| `setTemp`, `rampTargetTemp`, `rampStepC`, `rampIntervalMinutes` | Setpoint target 0–45 °C; step 0.01–5 °C; interval 1–10,080 min | The ramp changes `setTemp`, which is the cascade's liquid target. |

## Configuration and persistence note

The settings restore/default code currently assigns `cascadeMaxOffset` to
`CASCADE_MAX_OFFSET` and `cascadeGain` to `2.0`, even though `saveSettings()`
also writes JSON fields with those names. Thus the saved values are not applied
on restore by the current implementation. The settings page exposes the
iSpindel-vs-box source choice, but no gain/offset tuning controls. To change
these settings reliably, update the initialization/restore path as well as any
desired UI/API; changing only the saved JSON fields is not sufficient.

The trend rate, filtered rate, learned gains, and baseline are runtime state,
not saved calibration. Startup initializes a new baseline from the box sensor
when available and resets the gains from the base setting.

## Where to look when tuning

| Observed behavior | First constants/state to inspect |
| --- | --- |
| Box target too far from fermenter setpoint | `CASCADE_MAX_OFFSET` / `cascadeMaxOffset`, directional gains |
| Box target reacts too weakly or strongly | `DEFAULT_CASCADE_GAIN`, `CASCADE_COOLING_GAIN_MULTIPLIER`, gain adaptation thresholds |
| Gain changes too often or not at all | `CASCADE_ADAPT_DEADBAND`, `CASCADE_ADAPT_READINGS`, `CASCADE_GAIN_STEP`, `cascadeMaxOffset` saturation |
| Forecast too reactive or too sluggish | Prediction horizon, maximum rate/delta, filter alpha |
| Box temperature settles at a biased target | Baseline learning deadband and 15% blend in `updateCascadeTarget()` |
| Box target changes too abruptly or slowly | Up/down slew limits and the 10-second elapsed-time cap |
| Heater/cooler mode flips at an unexpected point | `FERMENTER_EMERGENCY_OVERTEMP`, measured-temperature mode conditions, and `DEFAULT_HYSTERESIS` |
| Heat remains active unexpectedly | Auto-only emergency lockout, active iSpindel status, stopped/Peltier interlocks, and the effective box target |
