# Plan de Migración de Seams de Telemetría Legacy — HITO-06

**Proyecto:** ABDAudioLab (Universal Black-Box Musical Hardware & Synth Profiler)  
**Documento:** PLAN_HITO_06_TELEMETRY_SEAMS.md  
**Hito:** HITO-06-TELEMETRY-SEAMS-MIGRATION  
**Versión:** 1.0.0  
**Fecha:** 2026-09-22  
**Responsable:** Antigravity (Lead Architect & Planner)  
**Estado:** Aprobado para Planificación  

---

## 1. Misión y Alcance del Hito 6

El **HITO-06** tiene como misión desacoplar y migrar los puntos de enganche (*seams*) de telemetría heredados del diseño preliminar, garantizando que el flujo de datos de instrumentación cumpla con la arquitectura canónica:

```
┌─────────────────────────┐        ┌─────────────────────────┐        ┌─────────────────────────┐
│  PRODUCTOR (Thread-Safe)│───────▶│ SNAPSHOT INMUTABLE      │───────▶│ CONSUMIDOR UI           │
│  (AudioEngine, Session) │        │ (TelemetrySnapshot)     │        │ (Vúmetros, FFT, Header) │
└─────────────────────────┘        └─────────────────────────┘        └─────────────────────────┘
```

### Alcance Específico
1. **Desacoplar `loopbackModal`**: Reemplazar la lectura de `loopbackModal.getCalibrationData()` en la telemetría por la fuente canónica de calibración (`NativeCalibrationPanel` / `ProfilingSessionController`).
2. **Desacoplar `stepperBar`**: Eliminar la consulta de estado de `stepperBar.getStepStatus(CalibrateLoopback)` en telemetría, enrutándolo hacia `sidebarStepper` / `WorkflowNavigationController`.
3. **Formalizar la Fachada de Telemetría (`ITelemetrySource`)**: Aplicar el patrón **Strangler Fig** asegurando paridad estricta entre el backend legacy y el nuevo backend unificado.
4. **Preservar la visualización en tiempo real**: Vúmetros (`SoundIdMeterStrip`), analizador espectral FFT (`curvePlotter`), barras de progreso y estado de sesión deben mantenerse 100% operativos y fluidos.
5. **Cero impacto en Audio y DSP**: Prohibición absoluta de llamadas bloqueantes, asignaciones dinámicas o sincronización cruzada desde el hilo de audio.

---

## 2. Guardas Técnicas e Invariantes No Negociables

1. **Cero asignaciones (`zero heap allocation`) en el hilo de audio**: Prohibido el uso de `new`, `std::vector::push_back`, o cualquier alocación en el callback de procesamiento de audio en tiempo real.
2. **Cero llamadas de UI desde hilos de audio o workers**: Ningún componente derivado de `juce::Component` puede ser invocado, medido o modificado fuera del `juce::MessageThread`.
3. **Cero Timers como relojes DSP**: Los `juce::Timer` pertenecen exclusivamente a la UI (Message Thread de Windows) para refresco visual (25–30 Hz); no deben usarse para cadencia de audio ni medición de tiempos de audio.
4. **Respeto estricto del ciclo de vida de `AudioProcessorValueTreeState` (APVTS)**: No desacoplar ni intercambiar estados de parámetros durante la ejecución activa de plugins VST3.
5. **Cero cambios en contratos metrológicos**: El formato `.abdlabtest`, los archivos del `ProductionPackage` y los hashes criptográficos SHA-256 no deben alterarse.
6. **Conservación del 100% de tests**: La suite completa (615 casos, 228.813 aserciones) debe mantenerse en verde tras cada cambio.

---

## 3. Fase 1: Inventario Exhaustivo de Seams de Telemetría

| Seam ID | Productor | Consumidor | Hilo de Ejecución | Frecuencia | Tipo de Dato | Propiedad de Memoria | Acción al Cerrar UI | Clasificación |
|---|---|---|---|---|---|---|---|---|
| **SEAM-01** (Audio Levels) | `audio::LabAudioEngine` | `SoundIdMeterStrip` | Message Thread (vía `DiagnosticsTelemetryPoller`) | 25 Hz | `TelemetryAudioLevels` (peaks/RMS L/R) | Lectura de atómicos / buffers preasignados | Se detiene timer | Audio $\rightarrow$ Telemetría |
| **SEAM-02** (Spectrum FFT) | `audio::LabAudioEngine` | `curvePlotter.getSpectrumAnalyzer()` | Message Thread (vía `DiagnosticsTelemetryPoller`) | 25 Hz | `std::array<float, 1024>` magnitudes | Copia prealocada en stack/buffer | Se detiene timer | Audio $\rightarrow$ Telemetría |
| **SEAM-03** (Device Metrics) | `juce::AudioDeviceManager` | `guidedWorkflowContainer` / Header | Message Thread | 25 Hz | Sample rate, buffer size, % CPU | Primitivas (`float`, `double`, `int`) | Se detiene timer | Driver $\rightarrow$ Telemetría |
| **SEAM-04** (Calibration Loopback) | `LoopbackCalibrationModal` (Legacy) | `MainContentTelemetrySource` $\rightarrow$ Header | Message Thread | Cada 15 ticks (~0.6 s) | `TelemetryCalibrationData` (`isCalibrated`, `sampleRate`) | Referencia directa a modal oculta | Riesgo TOCTOU si modal no existe | UI $\rightarrow$ Telemetría (Legacy) |
| **SEAM-05** (Calibration Skip) | `WorkflowStepperBar` (Legacy) | `MainContentTelemetrySource` $\rightarrow$ Header | Message Thread | Cada 15 ticks (~0.6 s) | `bool isSkipped` | Consulta a componente `stepperBar` inactivo | Referencia a widget huérfano | UI $\rightarrow$ Telemetría (Legacy) |
| **SEAM-06** (Session Progress) | `SessionExecutionCoordinator` | `SoundIdProfilingRunView` | Message Thread | 25 Hz | Puntos medidos, total, estado sesión | Atómicos / estados de coordinador | Se desacopla listener | Worker $\rightarrow$ Telemetría |

---

## 4. Fase 2: Contrato Canónico de Telemetría

Se adopta como contrato único el valor inmutable `TelemetrySnapshot` (ya definido en [DiagnosticsTelemetrySnapshot.h](file:///d:/desarrollos/ABDSynths/ABDAudioLab/src/gui/controllers/DiagnosticsTelemetrySnapshot.h)):

```cpp
struct TelemetrySnapshot
{
    // 1. Niveles de Audio (In/Out Peak y RMS)
    float inputPeakL { 0.0f }, inputPeakR { 0.0f };
    float inputRmsL { 0.0f },  inputRmsR { 0.0f };
    float outputPeakL { 0.0f }, outputPeakR { 0.0f };
    float outputRmsL { 0.0f },  outputRmsR { 0.0f };

    // 2. Magnitudes Espectrales (Array fijo, zero-heap)
    std::array<float, kMaxTelemetryFftBins> fftMagnitudes {};
    std::size_t fftBinCount { 0 };
    bool spectrumReady { false };

    // 3. Métricas de Driver y CPU
    float cpuUsagePercent { 0.0f };
    double sampleRate { 0.0 };
    int bufferSizeSamples { 0 };

    // 4. Estado MIDI
    int activeMidiNoteNumber { -1 };
    juce::String activeMidiNoteName { "No MIDI note" };

    // 5. Progreso de Sesión
    int currentTrial { 0 };
    int totalTrials { 0 };
    float progressPercent { 0.0f };
    float lastPluginOutputRmsDb { -120.0f };
    std::string stimulusDescription;
    int sessionStateCode { 0 };

    // 6. Calibración (Fuente canónica desacoplada)
    bool isCalibrated { false };
    double calibrationSampleRate { 0.0 };
    bool isCalibrationSkipped { false };
    bool calibrationTickDue { false };
};
```

---

## 5. Fase 3: Fachada Compatible (Strangler Fig)

La interfaz [IDiagnosticsTelemetrySource.h](file:///d:/desarrollos/ABDSynths/ABDAudioLab/src/gui/controllers/IDiagnosticsTelemetrySource.h) constituye la frontera aislada:

```
                  ┌───────────────────────────────┐
                  │  IDiagnosticsTelemetrySource  │
                  └───────────────┬───────────────┘
                                  │
                 ┌────────────────┴────────────────┐
                 │                                 │
                 ▼                                 ▼
   ┌───────────────────────────┐     ┌───────────────────────────┐
   │ MainContentTelemetrySource│     │ CanonicalTelemetrySource  │
   │ (Legacy: loopbackModal,   │     │ (Desacoplado de widgets,  │
   │  stepperBar)              │     │  solo controladores)      │
   └───────────────────────────┘     └───────────────────────────┘
```

---

## 6. Fase 4: Plan de Migración Secuencial

### Paso 1: Migración de Seam `loopbackModal`
- **Problema actual:** `MainContentTelemetrySource` mantiene una referencia `LoopbackCalibrationModal&` solo para invocar `getCalibrationData()`.
- **Nuevo enfoque:** Obtener los datos de calibración desde el `NativeCalibrationPanel` o directamente desde el contrato de sesión / `audioEngine`.
- **Validación:** Comprobación de que `isCalibrated` y `calibrationSampleRate` se reflejan idénticamente en la cabecera.

### Paso 2: Migración de Seam `stepperBar`
- **Problema actual:** `MainContentTelemetrySource` mantiene una referencia `WorkflowStepperBar&` inactiva solo para consultar si el paso `CalibrateLoopback` está en estado `Skipped`.
- **Nuevo enfoque:** Consultar el estado del paso en `workflowNavController` / `sidebarStepper`.
- **Validación:** Comprobación de que al omitir la calibración, el snapshot refleja `isCalibrationSkipped = true`.

### Paso 3: Retirada de Parámetros Legacy en Constructor
- Eliminar `LoopbackCalibrationModal&` y `WorkflowStepperBar&` del constructor de `MainContentTelemetrySource`.
- Actualizar instanciación en [MainContentComponent.h](file:///d:/desarrollos/ABDSynths/ABDAudioLab/src/gui/MainContentComponent.h) y tests unitarios.

### Paso 4: Validación de No Regresión
- Ejecutar:
  - `ABDAudioLab_Tests.exe "[diagnostics]"`
  - `ABDAudioLab_Tests.exe "[perf]"`
  - `ABDAudioLab_Tests.exe "[e2e]"`
- Smoke test interactivo de vúmetros y FFT.

---

## 7. Criterios de Aceptación y Certificación de HITO-06

1. `MainContentTelemetrySource` **no posee referencias** a `loopbackModal` ni a `stepperBar`.
2. Los vúmetros (`SoundIdMeterStrip`) y el analizador FFT se actualizan fluidamente a 25–30 Hz sin anomalías.
3. El estado de calibración en la cabecera (`mainHeader`) refleja fielmente el estado del sistema.
4. Cero asignaciones en el callback de audio.
5. Suite completa de tests en verde (615 casos, 0 fallos).
