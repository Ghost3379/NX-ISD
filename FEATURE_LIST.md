# NX-ISD — Feature & Mathematical Modeling Roadmap (TODO)

This document tracks planned, in-progress, and completed software features, algorithms, and mathematical models for the **NX-ISD** wearable watch running **ISD-Core**.

---

## 1. Mathematical Modeling & Sensor Fusion ("Coriolis" Engine)

Inspired by high-precision industrial metrology, we derive rich multi-variable insights by mathematically fusing the onboard sensors rather than adding extra physical hardware:

### [ ] Software Energy Accounting & Power Estimation
* **Sensors Involved:** FreeRTOS State Machine + MAX17048 Fuel Gauge + RV-3028 RTC.
* **Principle:** Microsecond-accurate tracking of hardware peripheral active time:
  $$\text{Energy}_{\text{total}} = \sum \left( I_{\text{subsystem}} \times \Delta t \right)$$
* **Subsystems Modeled:**
  * CPU: Active (240MHz) vs. Modem Sleep vs. Light/Deep Sleep ($<0.1\,\text{mA}$).
  * Display: Backlight PWM duty cycle ($0\,\text{mA}$ to $\approx 35\,\text{mA}$).
  * 4×4 NeoPixels: Active power domain (`PWR_NPM`) + LED count and brightness.
  * Sensors: MAX30102 active pulse measurement ($\approx 15\,\text{mA}$) vs. idle.
* **Auto-Calibration:** Closed-loop drift correction every 1% battery drop reported by the MAX17048 ($\approx 12\,\text{mAh}$ on a 1200mAh cell).
* **User Feature:** Apple Watch-style battery breakdown screen (*Display: 45%, Sensors: 25%, Glyphs: 18%, System: 12%*).

---

### [ ] Hypsometric Sub-Meter Altitude & Variometer
* **Sensors Involved:** BME690 (Barometer) + BNO085 (9-DOF Accelerometer).
* **Principle:** 
  * Barometric hypsometric equation gives absolute altitude:
    $$h = 44330 \times \left(1 - \left(\frac{P}{P_0}\right)^{\frac{1}{5.255}}\right)$$
  * Fuse with BNO085 vertical linear acceleration ($a_z$) via a 1D Kalman Filter.
* **User Feature:**
  * Zero-lag instantaneous vertical velocity (Variometer in $\text{m/s}$).
  * Real-time floor/stair counting and elevator detection without standard barometric delay.

---

### [ ] Weather Predictor, Dew Point & Absolute Humidity
* **Sensors Involved:** BME690 (Temperature, Humidity, Pressure).
* **Principle:**
  * **Dew Point ($T_{\text{dew}}$):** Magnus-Tetens formula:
    $$\gamma(T, RH) = \frac{17.27 \cdot T}{237.7 + T} + \ln\left(\frac{RH}{100}\right) \implies T_{\text{dew}} = \frac{237.7 \cdot \gamma}{17.27 - \gamma}$$
  * **Absolute Humidity:** Moisture density in grams per cubic meter ($g/\text{m}^3$).
  * **Heat Index / Humidex:** Thermal comfort index combining temperature and humidity.
  * **Storm Predictor:** Rolling $\Delta P / \Delta t$ pressure gradient over 3 hours. Warns if pressure drops $> 2.5\,\text{hPa} / 3\text{h}$ before rain hits.

---

### [ ] Autonomic Stress Index & Heart Rate Variability (HRV)
* **Sensors Involved:** MAX30102 (Optical PPG) + RV-3028 (1 ppm TCXO RTC).
* **Principle:** 
  * Microsecond timestamping of peak-to-peak Inter-Beat Intervals ($IBI$ / $R\text{-}R$ intervals).
  * Calculate RMSSD (Root Mean Square of Successive Differences):
    $$\text{RMSSD} = \sqrt{\frac{1}{N-1} \sum_{i=1}^{N-1} (IBI_{i+1} - IBI_i)^2}$$
* **User Feature:**
  * Real-time Autonomic Stress Score ($0\text{--}100$).
  * Sympathetic vs. Parasympathetic balance indicator.

---

### [ ] Motion-Artifact Cancellation for Biometrics
* **Sensors Involved:** MAX30102 (PPG) + BNO085 (3D Accelerometer).
* **Principle:** Adaptive frequency filter using 3D acceleration vectors to isolate and cancel out wrist tremor, finger movement, and walking shockwaves from the optical reflection channels.
* **User Feature:** Clean, stable pulse measurements even when moving.

---

### [ ] Multi-Factor Active Calorie Burn
* **Sensors Involved:** BNO085 (Kinetic Intensity) + MAX30102 (Heart Rate) + BME690 (Ambient Temp).
* **Principle:** Combines root-mean-square kinetic acceleration $J = \sqrt{a_x^2 + a_y^2 + a_z^2}$ with heart rate reserve elevation above baseline and ambient thermal stress.
* **User Feature:** Accurate Active Metabolic Rate (AMR $\text{kCal}$) beyond simple step counting.

---

### [ ] Circadian Daylight Dose & Night Shield
* **Sensors Involved:** OPT3001 (Human-Eye Photopic Lux) + RV-3028 (RTC).
* **Principle:** Integrates photopic lux across daytime hours: $\int \text{Lux}(t)\,dt$.
* **User Feature:**
  * Morning circadian light sufficiency check.
  * Late-night blue light / glare warning and automatic display/glyph dimming.

---

### [ ] Kinematic Wrist-Wake Gesture
* **Sensors Involved:** BNO085 (Rotation Vector & Angular Velocity).
* **Principle:** Detects inward wrist roll rotation ($\Delta \theta_{\text{roll}} > 45^\circ$, $\omega_{\text{roll}} > 1.2\,\text{rad/s}$) towards the eye vector.
* **User Feature:** Instantly turns on display only when looking at the watch; conserves battery when arm swings during walking.

---

## 2. NX-AIS — Advanced Information System (Contextual AI Engine)

**NX-AIS** is an autonomous, on-device contextual advisory engine running as an intelligent background supervisor. Rather than overwhelming the user with raw sensor numbers, NX-AIS continuously evaluates multi-sensor streams against physiological and environmental models to deliver proactive, human-centric advice and Glyph notifications.

> [!NOTE]
> **Toggleable System Architecture:** NX-AIS can be completely enabled, disabled, or configured to "Subtle Mode" (Glyphs only, no screen popups) in the `ISD-Core` System Settings to suit user preference.

### Contextual Advisory Modules:
1. **Hydration & Thermal Strain Guard:**
   * *Sensors:* BME690 ($T, RH$, Humidex) + BNO085 (Activity state) + RV-3028 (Timer).
   * *Trigger:* Ambient temperature $> 28^\circ\text{C}$ or high heat index sustained for $> 60\,\text{min}$, or elevated physical exertion under warmth.
   * *Action:* Soft buzzer chime + blue droplet ripple on 4×4 matrix + notification: *"Thermal load elevated. Remember to drink water!"*

2. **Alpine & High-Altitude Hiking Sentinel:**
   * *Sensors:* BME690 (Barometric Pressure / Hypsometric Altitude) + BNO085 (Ascent Rate) + MAX30102 ($SpO_2$ & Pulse) + RV-3028.
   * *Trigger:* Altitude $> 1800\,\text{m}$ combined with low dry humidity ($RH < 35\%$), or rapid vertical ascent rate $> 400\,\text{m/h}$.
   * *Action:* Mountain contour glyph on 4×4 matrix + alerts:
     * *Hydration:* *"Altitude 2,200m (Thin/Dry Air). Respiratory water loss doubled. Increase fluid intake."*
     * *Pacing:* *"Rapid ascent (+480m/h). Slow down pacing to prevent altitude sickness / fatigue."*

3. **Mountain Fog & Condensation Predictor:**
   * *Sensors:* BME690 ($T, RH, P \to T_{\text{dew}}$ Magnus-Tetens).
   * *Trigger:* Ambient temperature approaches dew point ($T - T_{\text{dew}} \le 1.0^\circ\text{C}$) with $RH > 90\%$ while hiking.
   * *Action:* White fog glyph on matrix + *"Dew point convergence. Dense fog or cloud immersion imminent. Check trail markers."*

4. **Indoor Air Quality & Ventilation Sentinel:**
   * *Sensors:* BME690 (Gas Resistance $R_{\text{gas}}$, VOCs, $eCO_2$).
   * *Trigger:* Air quality degradation detected indoors while user is sedentary for $> 30\,\text{min}$.
   * *Action:* Amber Glyph beacon + *"Indoor air quality declining. Consider opening a window."*

5. **Pre-Storm Meteorological Warning:**
   * *Sensors:* BME690 (Barometer).
   * *Trigger:* Barometric pressure drop $> 2.5\,\text{hPa}$ over a rolling 3-hour window.
   * *Action:* Tactical radar glyph flash + *"Rapid barometric drop (-X hPa). Rain or storm likely within 1-2h."*

6. **Alpine Solar & Snow Glare Guard:**
   * *Sensors:* OPT3001 (Photopic Human-Eye Lux) + BME690 (Altitude).
   * *Trigger:* Sustained extreme visible brightness $> 65{,}000\,\text{Lux}$ (common in high-altitude snowfields/glaciers due to high albedo reflection).
   * *Action:* Sunburst glyph + *"Extreme alpine glare detected (>65k Lux). Glare protection / sunglasses advised."*

7. **Circadian Daylight & Sleep Hygiene Coach:**
   * *Sensors:* OPT3001 (Photopic Lux) + RV-3028 (RTC Clock).
   * *Trigger:* Low daytime exposure ($< 200\,\text{Lux}$) before 1:00 PM, or harsh blue glare ($> 500\,\text{Lux}$) past 10:00 PM.
   * *Action:* Morning alert: *"Low daylight exposure today. Step outside for 10 min to support circadian rhythm."* / Night alert: *"Enabling Night Shield dimming."*

8. **Cold-Weather LiPo Battery Advisor:**
   * *Sensors:* BME690 ($T_{\text{ambient}}$) + MAX17048 ($V_{\text{cell}}$, Discharge Rate).
   * *Trigger:* Ambient temperature $< 4^\circ\text{C}$ causing electrolyte impedance rise and temporary voltage sag.
   * *Action:* Snowflake glyph + *"Low ambient temp (3°C). LiPo capacity temporarily reduced. Keep watch insulated under sleeve."*

9. **Autonomic Stress & Breathing Guide:**
   * *Sensors:* MAX30102 (PPG $R\text{-}R$ intervals) + BNO085 (Stationary confirmation).
   * *Trigger:* Heart Rate elevated above resting baseline accompanied by suppressed HRV (RMSSD stress spike) while completely stationary.
   * *Action:* 4×4 NeoPixel matrix automatically engages the `GLYPH BREATH` animation at a relaxing 4-second inhale / 4-second exhale pace + *"High physiological tension. Take a 60s breathing break."*

10. **Combustion Smoke & Pyrolysis Hazard Sentinel:**
    * *Sensors:* BME690 (MOX Gas Resistance $R_{\text{gas}}$ + Temperature $\Delta T / \Delta t$).
    * *Principle:* Combustion byproducts (wood smoke, melting wire insulation, flux fumes, carbon monoxide) flood the heated metal-oxide plate with reducing gases, causing an immediate, catastrophic drop in gas resistance ($R_{\text{gas}}$ drops $> 75\%$ from baseline within seconds) often paired with an ambient thermal surge.
    * *Trigger:* $\frac{R_{\text{gas}}}{R_{\text{baseline}}} < 0.25$ within 30 seconds, or concurrent $\Delta T > +3.0^\circ\text{C}$ spike.
    * *Action:* Urgent audible buzzer warble + pulsing crimson hazard glyph on 4×4 matrix + critical notification: *"CRITICAL: Smoke / Combustion gas spike detected! Inspect room/workbench safety."*

11. **Sedentary / Posture Nudge:**
    * *Sensors:* BNO085 (Stability / Inactivity classifier).
    * *Trigger:* Continuous zero-motion detected for $> 60\,\text{min}$ during daytime hours.
    * *Action:* Subtle haptic buzz + *"Time to stand up and stretch."*

---

## 3. Hardware Revision Tracking (v1.1 Rework)

* [x] **NeoPixel Matrix:** Upgraded from 3×3 (9 LEDs) to 4×4 serpentine matrix (16 LEDs: `G1..G16`).
* [x] **PMOS Polarity:** Fixed high-side PMOS circuit for 0µA sleep on matrix.
* [x] **Display Interface:** 10-pin 0.5mm TE FPC connector `J1` integrated with backlight driver.
* [x] **Storage:** ZDSD NAND flash integrated on SPI bus.
* [x] **BNO085 Pull-ups:** Added 10k pull-ups `R18` and `R19` to `+3V3`.
* [x] **MAX30102 Filtering:** Added 100nF decoupling capacitor `C18` on `+1V8` rail.
* [x] **BOOT Jumper:** Added `JP3` solder jumper to pull GPIO 0 low for manual flashing.
* [ ] **Power Architecture Rework:**
  * Replace tiny NCP167 LDO with **TLV62568** synchronous step-down buck converter (SOT-23 / SOT-563, $\approx 95\%$ efficiency).
  * Select compact shielded power inductor ($1.0\text{--}1.5\,\mu\text{H}$, $I_{sat} \ge 1.5\,\text{A}$).
  * Accommodate 1200mAh LiPo battery form factor on PCB layout.
  * Environmental sensor updated to **BME690**.

---

## 4. Firmware Implementation Checklist (ISD-Core)

* [x] FreeRTOS Multi-Tasking Core (`vUITask`, `vFastSensorTask`, `vSlowSensorTask`).
* [x] Thread-Safe Mutex-protected `SensorState` telemetry buffer.
* [x] 4×4 Serpentine Matrix Coordinate Engine (`NeoPixelTest::xy(x, y)`).
* [x] 6 Interactive Matrix Animations (`CYBER RADAR`, `GLYPH BREATH`, `QUANTUM RIPPLE`, `NEON TRACER`, `MATRIX RAIN`, `SPECTRUM PLASMA`).
* [x] Live 104×104 pixel TFT visualizer for NeoPixel matrix with Lever Left/Right cycling.
* [ ] Implement `NX-AIS` rule evaluation engine and settings toggle (`ENABLED / SUBTLE / OFF`).
* [ ] Implement `PowerEstimator` class tracking active states against MAX17048.
* [ ] Implement BME690 + BNO085 1D Kalman Filter for altitude/variometer.
* [ ] Implement Magnus-Tetens dew point and 3-hour barometric storm gradient.
* [ ] Implement MAX30102 $R\text{-}R$ peak detector and RMSSD stress score.
* [ ] Implement BNO085 wrist-flip wake interrupt routine.
