# Especificación Técnica: Sistema de Medición, Parametrización y Modo Clásico (Stage 3)

**Documento:** `docs/SPECS_STAGE3_MEASUREMENT_SYSTEM.md`  
**Fecha:** 2026-10-15  
**Autor:** Antigravity (Lead Architect) en tándem con el Usuario  
**Estado:** Aprobado / Referencia canónica para migración WebUI

---

## 1. Diagnóstico y Visión Arquitectónica

### 1.1 El Problema del Enfoque Guiado Anterior
* **Opacidad:** El modo guiado original actúa como una "caja negra" que oculta qué señal se inyecta, a qué nivel de dBFS, cuántos segundos dura la toma y qué magnitudes físicas se están midiendo.
* **Sesgo de Catálogo:** Estaba hiperfocalizado en 2 o 3 dispositivos fijos (BOSS DS-1, Korg MS-2000), cuando la realidad de un laboratorio de audio es que el usuario necesita medir **cualquier equipo analógico, modular, pedal boutique o plugin VST3** sin restricciones ni recetas cerradas.

### 1.2 La Solución Unificada
Se desacopla el proceso de medición en dos fases complementarias sin duplicar lógica ni crear silos:
1. **Paso 3A: Parametrización y Diseño de Pruebas (Test Suite Setup):**
   * Dónde se define **QUÉ** se va a medir y se calcula el tiempo estimado.
   * El modo "guiado" se integra aquí de forma natural como un selector de **plantillas preconfiguradas** (más de 25 disponibles) que rellenan automáticamente la matriz, pero dejando todos los parámetros 100% visibles y editables.
   * Permite crear pruebas desde cero (`+ Add Custom Test`) para cualquier equipo no listado.
2. **Paso 3B: Ejecución, Monitoreo y Gráficas en Tiempo Real (Live Execution & Plotter):**
   * Dónde se ejecuta y visualiza **CÓMO** responde el equipo en tiempo real.
   * Desglose punto a punto con re-medición selectiva (`Re-Measure`), pausa, abortar y toma libre al vuelo (`Capture Free Take`).
   * Visualizadores espectrales avanzados (FFT en vivo con armónicos $H_2, H_3$, cascada 3D, mapa de calor, fase y matriz de modulación).

---

## 2. Inventario C++ de Parametrización (`TestConfiguration` y `TestEditorPanel`)

Archivos de referencia en C++:
* `src/gui/TestConfiguration.h`
* `src/gui/TestEditorPanel.h` / `TestEditorPanel.cpp`
* `src/gui/TestConfigModal.h` / `TestConfigModal.cpp`
* `src/gui/MatrixResolutionTableComponent.h` / `MatrixResolutionTableComponent.cpp`
* `src/gui/TestPlanEstimationCardComponent.h` / `TestPlanEstimationCardComponent.cpp`
* `src/core/AutoTestPresetEngine.h` / `AutoTestPresetEngine.cpp`

### 2.1 Sección 1: Plantillas, Título y Tipo de Estímulo

#### Catálogo de Presets Maestros (`AutoTestPresetEngine::getAllPresets()`)
El motor nativo contiene más de 25 plantillas probadas en laboratorio:

| Categoría | Typology ID | Badge | Nombre Mostrado | Estímulo y Notas |
| :--- | :--- | :--- | :--- | :--- |
| **Filtros** | `vcf_ladder_ota_svf` | `VCF` | VCF Filter (Ladder, OTA, Sallen-Key, SVF, Diode) | Farina Log-Sine Sweep + Volterra $H_2..H_5$ + Q Resonance |
| | `filter_tpt_state_variable` | `SVF` | TPT / ZDF Multi-Mode State Variable Filter (LP/BP/HP/Notch) | TPT Pole-Zero Decomposition + Q Tuning |
| | `filter_formant_vocal` | `FRM` | Formant / Vocal Resonator Filter | Farina Sweep + LPC orden 20 para $F_1..F_3$ |
| | `eq_parametric_graphic` | `EQ` | Parametric / Graphic Equalizer | Fast Sweep (1.0s) + Bell / Shelf Fit |
| | `filter_comb` | `CMB` | Comb Filter (Positive / Negative Feedback) | Cepstrum Peak + Notch Depth |
| **Roland AIRA** | `aira_01_ladder_lpf` | `AIRA` | 01. Ladder Low-Pass Filter (-24dB/Oct) | Sweep + Volterra THD |
| | `aira_02_resonant_filter`| `AIRA` | 02. Resonant Filter (-18dB/Oct) | Resonant Peak Sweep |
| | `aira_03_high_pass` | `AIRA` | 03. High-Pass Filter (-12dB/Oct) | HPF Cutoff Curve |
| | `aira_04_band_pass` | `AIRA` | 04. Band-Pass Filter (BPF) | Center Freq & Bandwidth |
| | `aira_05_formant_filter` | `AIRA` | 05. Formant Vocal Filter | Double Peak Vocal Tracking |
| | `aira_06_svf_multimode` | `AIRA` | 06. State Variable Filter (SVF Multi-Mode) | Multi-tap simultaneous extraction |
| | `aira_12_adsr_envelope` | `AIRA` | 12. ADSR Envelope Generator | Sync Pulses 3-stage Gate |
| | `aira_13_ar_envelope` | `AIRA` | 13. AR Attack-Release Envelope | Fast Trigger Pulses |
| | `aira_14_multiwave_lfo` | `AIRA` | 14. Multi-Wave LFO Modulator | Ultra-low frequency extraction |
| | `aira_15_dual_lfo` | `AIRA` | 15. Dual Cross-Modulated LFO | Modulation Matrix & FM depth |
| | `aira_24_sample_hold` | `AIRA` | 24. Sample & Hold (S&H) Circuit | Stepped clock correlation |
| | `aira_25_slew_limiter` | `AIRA` | 25. Slew Limiter / Portamento Glide | Rising/Falling edge ramp response |
| **Envolventes/LFO** | `env_generator_adsr` | `ENV` | Envelope Generator (ADSR / AR / Function Gen) | Pulse Stimulus (3 amplitudes) |
| | `lfo_sample_hold` | `LFO` | LFO & Sample / Hold Circuit | Multi-waveform sub-audio extraction |
| **Dinámica / Saturación** | `vca_attenuverter` | `VCA` | VCA & Attenuverter (Lin/Exp Response & Bleed) | Linear/Exp Gain curve + Bleed-through |
| | `saturator_fuzz_tube` | `SAT` | Saturator / Fuzz / Tube Preamp / Wavefolder | Amplitude Ramp + 1kHz THD |
| | `pedal_overdrive_distortion` | `PEDAL` | Guitar Overdrive / Distortion Pedal (Drive & Tone) | Farina Sweep + Dynamic Staircase |
| | `analog_tape_saturation` | `TAPE` | Analog Tape Saturation & Magnetic Hysteresis | 1kHz + 10kHz Intermodulation (IMD) |
| | `compressor_limiter_gate` | `CMP` | Compressor / Limiter / Gate Dynamics | Threshold, Ratio, Knee & Attack/Release |
| **Casio CZ (PD)** | `cz_dcw_waveshaping` | `CZ-DCW` | Casio CZ: DCW Waveform Transformation | Phase Distortion Resonance extraction |
| | `cz_env_8stage` | `CZ-ENV` | Casio CZ: 8-Stage Pitch/Wave/Amp Envelope | Rate/Level contour tracking |
| | `cz_dco_phase` | `CZ-DCO` | Casio CZ: DCO Dual Waveform Generator | Digital carrier comparison |

#### Tipos de Estímulo (`audio::StimulusType`)
1. **`Exponential Log-Farina Sweep (Standard for Filters/EQ)`**:
   - `audio::StimulusType::LogFarinaSweep`
   - *Descripción UI:* "Log Sweep: Best SNR, precise 20Hz-20kHz response, extracts THD".
2. **`Amplitude Ramp (Saturation & Non-Linear Knees)`**:
   - `audio::StimulusType::AmplitudeRamp`
   - *Descripción UI:* "Amplitude Ramp: Non-linear saturation and compression knees".
3. **`Sync Pulses (ADSR Gate & Timing)`**:
   - `audio::StimulusType::SyncPulses3`
   - *Descripción UI:* "Sync Pulses: Gate step pulses for ADSR envelope timings".
4. **`Step Impulse / Dirac Delta (Transient & Phase Calibration)`**:
   - `audio::StimulusType::DiracDelta`
   - *Descripción UI:* "Step Impulse (Dirac Delta): Single 0 dBfs sample for transient/phase IR".
5. **`1kHz Sine Wave (Saturator & Harmonic Distortion THD)`**:
   - `audio::StimulusType::SineWave1kHz`
   - *Descripción UI:* "1kHz Sine Wave: Pure tone for harmonic distortion (THD) and saturation".
6. **`White Noise Burst (Statistical Noise Floor & Broad Band)`**:
   - `audio::StimulusType::WhiteNoise`
   - *Descripción UI:* "White Noise: Uniform energy density across all frequencies".
7. **`NAM / RTNeural Calibration (Sync, Chirp, Noise, Multitone)`**:
   - `audio::StimulusType::NamCalibration`
   - *Descripción UI:* "NAM Calibration: Multi-stage neural stimulus with sample-accurate sync pulses".

---

### 2.2 Sección 2: Duración de Ráfaga y Modo de Captura

* **Opciones de Duración (`comboDurationPreset`):**
  - `Ultra-Fast (0.25 sec per point)`
  - `Fast Standard (0.50 sec per point)`
  - `High Precision (1.00 sec per point)`
  - `Ultra High Precision (2.00 sec per point)`
  - `Long Tail (4.00 sec per point)`
  - `Custom Duration (Manual)...` (habilita campo numérico `txtManualDuration` con sufijo `sec`).
* **Corte Adaptativo de Silencio (`btnAdaptiveTail`):**
  - Etiqueta: *"Adaptive Auto-Tail Silence Cutoff (for ADSR / Reverb)"*.
  - Conmuta entre `captureMode = "FIXED_TIME"` y `"ADAPTIVE_ENVELOPE"` (umbral `-60.0 dBFS`).

---

### 2.3 Sección 3: Matriz Dimensional (`MatrixResolutionTableComponent`)

Representa la combinación factorial de parámetros que se van a muestrear:

#### Resoluciones Disponibles por Control (`ResolutionPreset`):
- `Fixed (1 point)` (1 valor estático / bypass de barrido)
- `Coarse (3 points)` (0%, 50%, 100%)
- `Standard (5 points)` (0%, 25%, 50%, 75%, 100%)
- `Detailed (8 points)`
- `Fine (16 points)`
- `Ultra-Fine (32 points)`
- `Extreme (64 points)`
- `Custom...` (permite ingresar un número entero arbitrario de puntos)

#### Ajustes Avanzados por Control (`AdvancedSettingsPanel`):
- **Rango:** `Range: min% — max%` (por defecto `0.0 ... 100.0 %`).
- **Orden de ejecución:** Botones `[Move Earlier]` y `[Move Later]`.
- **Eliminación:** Botón `[Remove Control]`.
- **Añadir Parámetro:** Botón `[+ Add Parameter]` para plugins VST3 o hardware libre.

---

### 2.4 Sección 4: Estimación del Plan (`TestPlanEstimationCardComponent`)

* **Badge:** `PLAN ESTIMATE` (fondo verde cian con tipografía destacada).
* **Fórmula Dimensional:**
  $$\text{Estados} = \prod_{i=1}^{N} \text{steps}_i \quad \text{Ejemplo: } 8 \text{ Cutoff} \times 4 \text{ Resonance} \times 1 \text{ Slope} = 32 \text{ measurement states}$$
* **Texto de Modo:**
  - Automatizado: `"✓ Fully automated profiling. No manual adjustments required."`
  - Manual: `"✓ Manual profiling • ~N operator adjustments"`
* **Tiempo Total Estimado:**
  $$\text{Tiempo} = \text{Estados} \times (\text{burstDurationSec} + \text{overhead}) \quad \longrightarrow \text{"Estimated Time: ~33s"}$$

---

## 3. Inventario C++ de Ejecución y Cola (`SoundIdSuiteList`)

Archivos de referencia en C++:
* `src/gui/SoundIdSuiteList.h` / `SoundIdSuiteList.cpp`
* `src/gui/suite/SuiteDataModels.h`
* `src/gui/suite/SuiteQueueModelManager.h` / `SuiteQueueModelManager.cpp`
* `src/gui/suite/SuiteListEventHandler.h` / `SuiteListEventHandler.cpp`
* `src/gui/suite/SuiteRowRenderer.h` / `SuiteRowRenderer.cpp`

### 3.1 Barra Superior de la Cola
* `[▲ / ▼]`: Maximizar cola de pruebas / Restaurar vista balanceada con la gráfica.
* `[+ Add Standard Test]`: Añade una prueba basada en el contrato del equipo activo.
* `[+ Add Custom Test]`: Abre el modal de configuración de parámetros físicos.
* `[Compact View / Detailed Points]`: Alterna entre ver solo la barra resumen de cada test o desplegar la lista de todos sus puntos hijos.
* `[Re-Measure (N)]`: Botón reactivo con contador numérico y el icono vectorial `drawReset`, habilitado cuando hay puntos marcados para repetir.
* `[Clear All]`: Limpia la cola pero **preserva siempre el Test 0 fijado (Noise Floor)**.

### 3.2 Filas de Prueba (`QueueItem`)
Estructura de datos en C++:
```cpp
struct QueueItem {
    juce::String id;
    juce::String badgeText;   // "NOI", "FLT", "VCF", "ENV", "SAT", "THD", etc.
    juce::Colour badgeColor;
    juce::String title;
    juce::String description;
    audio::StimulusType stimulusType;
    float burstDurationSec;
    std::vector<ControlStepConfig> controls;
    int totalPoints;
    QueueItemStatus status;   // Queued, Running, Incomplete, Completed, Invalidated
    int currentRunningPoint;
    bool isPinned;            // Pinned Test 0 (Noise Floor) no se puede borrar ni mover
    bool isSkipped;           // Flag de bypass
    bool isExpanded;          // Despliegue de puntos hijos
    std::vector<PointStatus> pointStatuses;
    std::vector<bool> pointSelections;
};
```

* **Elementos visuales en cada fila:**
  - Badge funcional (`NOI`, `FLT`, `VCF`...).
  - Icono de expansión: `▶` (colapsado) / `▼` (expandido con sus puntos).
  - Título y descripción.
  - Badges de estado: `[ACTIVE]`, `[QUEUED]`, `[COMPLETED]`, `[INVALIDATED]`.
  - Iconos de gestión: Editar (`✏️`), Duplicar (`📋`), Eliminar (`🗑️`), Reordenar (`▲`/`▼`).

### 3.3 Desglose de Puntos Hijos (`Point #N / Total`)
Cuando un test está expandido:
* **Barra de progreso del test:** `Progress: X / Y Points Completed (Z%)`.
* **Fila por cada punto individual:**
  - Checkbox de selección manual (para re-medición selectiva).
  - Nombre: `Point #1 / 32 (0.0% Pos)`, `Point #2 / 32 (3.2% Pos)`, etc.
  - Estado: `QUEUED`, `RUNNING`, `COMPLETED`, `INVALIDATED`.
  - Iconos de acción:
    1. **Ver / Audicionar (icono ojo):** Carga la respuesta de este punto en el visualizador superior y en el Inspector.
    2. **Re-medir (icono refresh):** Marca el punto como invalidado y lo coloca en cola de ejecución inmediata.
    3. **Descartar (icono papelera):** Anula el punto.

---

## 4. Inspector de Parámetros (`OperatorStepModalDialog`)

Archivos de referencia en C++:
* `src/gui/OperatorStepModalDialog.h` / `OperatorStepModalDialog.cpp`
* `src/gui/RhythmicMetronomeComponent.h` / `RhythmicMetronomeComponent.cpp`

* **Cabecera:** `INSPECTOR: [Nombre del Test]` | `POINT X / Y` | Flecha de colapso `▼`.
* **Dial / Knob Analógico:**
  - Renderizado vectorial con aguja indicadora de posición física (`0.0% Pos` a `100.0% Pos`).
  - Etiqueta con valor normalizado: `Target: 0% (0.000 norm) | Range: 0% - 100%`.
* **Especificación del Parámetro:**
  - `PARAMETER 1 [Knob]` (o `[Slider]`).
  - `Current State: Point #1/32 (0.0% Position) -- [Nombre del Test]`.
* **Guiado Manual (si el equipo no es automatizable por MIDI/VST3):**
  - `[Confirm Step & Continue Take ->]`
  - `[Repeat Step]`
  - `[Step Back]`
  - `[Abort]`

---

## 5. Visualizador Espectral Multitesta (`SoundIdCurvePlotter` y `LiveSpectrumAnalyzer`)

Archivos de referencia en C++:
* `src/gui/SoundIdCurvePlotter.h` / `SoundIdCurvePlotter.cpp`
* `src/gui/LiveSpectrumAnalyzer.h`

* **Botonera de 8 pestañas:**
  1. `Curve (μ ± σ)`: Curva media de respuesta en frecuencia con franja de desviación típica.
  2. `2D Heatmap`: Mapa de calor de respuesta vs nivel de excitación.
  3. `Spectrum FFT`: Analizador FFT en vivo a 60 Hz con **anotaciones automáticas de picos armónicos en naranja (`H2`, `H3`)** y líneas discontinuas hacia el piso de ruido.
  4. `Phase / GD`: Ángulo de fase y retardo de grupo (*Group Delay*).
  5. `3D Mountains`: Cascada espectral isométrica 3D (*waterfall*) con selector de paletas de color (`Retro Emerald`, `Thermal Fire`, etc.).
  6. `Mod Matrix`: Matriz de dispersión e inspectora de nodos de modulación ($K, c, R^2$).
  7. `±σ Band`: Banda de incertidumbre metrológica y tolerancia.
  8. `THD %`: Curva de distorsión armónica total sobre frecuencia.
* **Ejes cartesianos calibrados:**
  - Frecuencias logarítmicas: `20 Hz`, `50`, `100`, `200`, `500`, `1 kHz`, `2 kHz`, `5 kHz`, `10 kHz`, `20 kHz`.
  - Amplitud dB: `0, -12, -24, -36, -48, -60, -72, -84, -96 dB` (o `-12 dB` a `+12 dB`).

---

## 6. Barra de Gobernanza Superior (`MainContentComponentUiGovernance`)

Archivo de referencia: `src/gui/MainContentComponentUiGovernance.cpp`

* **Toggle de Modo:** Botón `[Classic: more control, advanced setup]` (conmuta entre Modo Guiado y Modo Clásico).
* **Badge de Estado:** `Classic: more control, advanced setup | Ready | Recorded takes: N`.
* **Botones de Acción:**
  - `[Capture Free Take]`: Disparo ad-hoc de una toma de 1 segundo para inspección inmediata.
  - `[Stop]`: Detiene la ejecución activa.
  - `[Promote to Recipe]`: Consolida las tomas libres capturadas en una receta permanente del modelo.
* **Health Panel (Banner en tiempo real):**
  - `● SNR: -XX.X dB` con indicador semafórico y badge de tolerancia (`[LOW]`, `[NOMINAL]`, `[GOOD]`).
  - `Noise Floor: -XX.X dBFS`.
  - `Points: X / Y`.

---

## 7. Plan de Implementación para WebUI

Para trasladar este sistema fielmente sin perder ninguna de estas funcionalidades:

1. **Submódulo `stage3_params.js` (Paso 3A - Parametrización):**
   - Renderiza el `TestEditorPanel` con el dropdown de los 25 presets reales, los 7 estímulos, los presets de duración y la tabla de resolución matricial con el cálculo de tiempo `PLAN ESTIMATE`.
   - Permite componer o editar las pruebas que alimentarán la cola.
2. **Submódulo `stage3_classic.js` (Paso 3B - Ejecución Clásica):**
   - Renderiza la barra de gobernanza superior (`Capture Free Take`, `Stop`, `Promote to Recipe`, `Health Panel`).
   - Renderiza el `SoundIdCurvePlotter` con sus 8 pestañas y los marcadores armónicos $H_2, H_3$.
   - Renderiza la `SoundIdSuiteList` con el desglose de puntos (`0.0% Pos` a `100% Pos`), checkboxes de selección y botón `Re-Measure (N)`.
   - Renderiza el `Inspector` inferior con su potenciómetro/knob rotatorio vectorial y telemetría de posición.
3. **Persistencia e IPC:**
   - Comunica los eventos nativos `startAcquisition`, `captureFreeTake`, `reMeasurePoints`, `pauseAcquisition`, `resumeAcquisition`, `abortAcquisition` y `promoteToRecipe` directamente hacia `AudioLabWebComponent.cpp`.
