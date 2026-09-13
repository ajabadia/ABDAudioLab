# Arquitectura de Software — ABDAudioLab

**Proyecto:** ABDAudioLab — Universal Black-Box Musical Hardware & Synth Profiler  
**Versión del documento:** 2.0.0  
**Actualizado:** 2026-09-13  
**Tecnología:** C++20, JUCE 8.0.4, CMake 4.4.0, MSVC 2026 (AVX2), ABDScope (WebView2), ABDSharedCode  

---

## 1. Alcance y Estado de esta Arquitectura

Este documento describe la arquitectura de software **actualmente integrada y operativa en el ejecutable**. Refleja las refactorizaciones de modularización arquitectónica (Fase 4), la integración del ecosistema compartido (`ABDSharedCode`), el desacoplamiento de controladores, la adopción de telemetría unificada vía WebView2, y la **Capa de Perfilado de Sintetizadores Digitales y Plugins VST3/AU (Fase 20)**.

### Estado de Verificación

| Estado | Significado |
|---|---|
| **Implementado** | El código forma parte del producto, de sus dependencias CMake o de sus módulos compartidos. |
| **Verificado en simulación** | Suite automatizada completa (Catch2 / CTest): **141/141 test cases pasando (134.498 aserciones)** sin fallos ni regresiones. |
| **Pendiente de banco** | Pruebas con interfaz de audio física conectada por USB/Thunderbolt, cables patch o sintetizadores de hardware reales. |

---

## 2. Diagrama de Capas

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│                       PRESENTACIÓN JUCE (Modern SoundID UI)                  │
│  • MainContentComponent (Orquestador desacoplado)                            │
│  • SoundIdSuiteList & suite/ (Cola de ensayos por lotes y renderizadores)    │
│  • SlideInDrawer & drawers/ (Cajón lateral: File, Hardware, Setup, Telemetría)│
│  • SoundIdCurvePlotter (Visualizador 2D logarítmico con bandas de dispersión)│
│  • SoundIdMeterStrip (Vúmetros In/Out, Peak dBfs y Trim digital)             │
│  • Diálogos Modales desacoplados (.h / .cpp): OperatorStep, About, Calibración│
└──────────────────────────────────────┬───────────────────────────────────────┘
                                       │
                                       ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│                    CORE ORCHESTRATION & SESSION MANAGERS                     │
│  ┌───────────────────────────┐          ┌─────────────────────────────────┐  │
│  │    core::SessionManager   │          │     core::HardwareManager       │  │
│  │ • Serializador ZIP (.abd) │          │ • Orquestador de dispositivos   │  │
│  │ • Recovery & dirty state  │          │ • SharedContractAdapter         │  │
│  │ • Manifiestos de sesión   │          │ • Catálogo normativo RF-25/RF-26│  │
│  └─────────────┬─────────────┘          └────────────────┬────────────────┘  │
│                │                                         │                   │
│                ▼                                         ▼                   │
│       ProfilingSequencer  ◄───────────────►    IHardwareController (Pure)    │
│  (FSM en hilo de background)                 (AiraSysEx, MidiCc, Manual, Mock)│
└────────────────┬─────────────────────────────────────────┬───────────────────┘
                 │                                         │
                 ▼                                         ▼
┌───────────────────────────────────┐    ┌─────────────────────────────────────┐
│             AUDIO ENGINE          │    │         TELEMETRÍA (ABDScope)       │
│  • LabAudioEngine (Callback RT)   │    │  • ScopeDataCollector (Taps SPSC)   │
│  • LabStimulusGenerator (9 tipos) │    │  • ScopeFrameSerializer (JSON wire) │
│  • LabAudioReceiver (Lock-Free)   │    │  • JuceWebScopeComponent (30 Hz)    │
│  • LabAnalyticEngine (µ, σ, THD)  │    │  • ScopeWebFloatingWindow (WebView2)│
└────────────────┬──────────────────┘    └─────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│        DIGITAL SYNTH & VST3/AU PLUGIN PROFILING SUBSYSTEM (Fase 20)          │
│                                                                              │
│  ┌─────────────────────────────┐         ┌────────────────────────────────┐  │
│  │   Recetas Científicas       │         │   Target Contracts             │  │
│  │ • NoteExcitation (27 tomas) │ ──────► │ • HardwareMidiContract (Decl.) │  │
│  │ • ParameterStep / Ramp      │         │ • PluginContractDiscovery(Dyn.)│  │
│  │ • LocalPerturbation (±Δ)    │         │ • NormalizedTargetContract     │  │
│  └─────────────┬───────────────┘         └───────────────┬────────────────┘  │
│                │                                         │                   │
│                └────────────────────┬────────────────────┘                   │
│                                     ▼                                        │
│                        ┌─────────────────────────┐                           │
│                        │   ISynthTarget (Puro)   │                           │
│                        └────────────┬────────────┘                           │
│                                     │                                        │
│             ┌───────────────────────┴───────────────────────┐                │
│             ▼                                               ▼                │
│  ┌─────────────────────────────┐                 ┌────────────────────────┐  │
│  │ SyntheticSynthFixture (RAM) │                 │ PluginSynthTarget (RAM)│  │
│  │ • Ground-Truth determinista │                 │ • juce::AudioProcessor │  │
│  │ • Inyección de fallos       │                 │ • Sub-bloque MidiBuffer│  │
│  └─────────────┬───────────────┘                 └───────────┬────────────┘  │
│                │                                             │               │
│                └────────────────────┬────────────────────────┘               │
│                                     ▼                                        │
│                        ┌─────────────────────────┐                           │
│                        │ CapturedAudioBlock      │                           │
│                        │ (Bloque aislado en RAM) │                           │
│                        └────────────┬────────────┘                           │
│                                     │                                        │
│                 ┌───────────────────┴───────────────────┐                    │
│                 ▼                                       ▼                    │
│  ┌─────────────────────────────┐         ┌────────────────────────────────┐  │
│  │   SynthPitchEstimator       │         │   SynthEnvelopeAnalyzer        │  │
│  │ • NSDF parabólica corregida │         │ • Seguidor de picos instantáneo│  │
│  │ • Invarianza C4-C5 / Cents  │         │ • AnalysisPolicy (τ = 40 ms)   │  │
│  └─────────────┬───────────────┘         └───────────────┬────────────────┘  │
│                │                                         │                   │
│                └────────────────────┬────────────────────┘                   │
│                                     ▼                                        │
│                        ┌─────────────────────────┐                           │
│                        │ SynthObservation        │                           │
│                        │ (Métricas + Observab.)  │                           │
│                        └─────────────────────────┘                           │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Desglose Arquitectónico por Capas

### 3.1 Capa de Presentación (UI Desacoplada)
Tras la refactorización arquitectónica de Fase 4, `MainContentComponent` redujo su tamaño y delegó sus responsabilidades en clases especializadas:
- **`core::SessionManager`**: Gestiona la carga/guardado de contenedores `.abdlabtest`, comprobación SHA-256, salvaguardas de sobreescritura y recuperación de sesión.
- **`core::HardwareManager`**: Centraliza la selección de sintetizadores, la vinculación con el adaptador de contratos compartidos y el enrutamiento de controladores.
- **Subcomponentes del Cajón (`src/gui/drawers/`)**: `DrawerFileSessionTab`, `DrawerHardwareTab`, `DrawerSetupTab` y `TestEditorPanel`.
- **Renderizadores de Fila de Ensayos (`src/gui/suite/`)**: `SuiteDataModels.h`, `SuiteIcons.h`, `SuiteRowLayout.h/.cpp` y `SuiteRowRenderer.h/.cpp`.
- **Diálogos Modales en `.h` / `.cpp`**: `OperatorStepModalDialog`, `AboutModalDialog`, `TestConfigModal`, `LoopbackCalibrationModal`, `NativeCalibrationPanel`.

### 3.2 Capa de Hardware Clásica: Arquitectura en Tres Niveles
El acceso y control de hardware analógico/digital para inyección de audio se estructura conforme a la [Guía de Integración de Hardware](HARDWARE_INTEGRATION_GUIDE.md):
1. **Nivel 0 (`ABDSharedAssets/contracts/`)**: Fuente única de la verdad con esquemas JSON normativos de hardware.
2. **Nivel 1 (`ABDSharedCode`)**: `HardwareContractRegistry`, `HardwareMidiDetector` y `HardwareMidiHotplugMonitor`.
3. **Nivel 2 (`ABDAudioLab`)**: `IHardwareController` con implementaciones `AiraSysExController`, `MidiCcController`, `ManualAnalogueController` y `MockHardwareController`.

### 3.3 Motor de Audio en Tiempo Real (DSP RT-Safety)
`LabAudioEngine` implementa `juce::AudioIODeviceCallbackWithContext` y garantiza aislamiento estricto:
- **Zero Heap Allocation en hilo de audio**: Prohibición de `new`, `malloc`, `std::vector::push_back` o llamadas a I/O de disco durante el procesamiento.
- **Dual-Buffer estéreo independiente**: Buffers pre-asignados `tempProcessBufferL` y `tempProcessBufferR` con capacidad para 16.384 muestras por canal.
- **Subrutinas `noexcept` inline**: Descomposición del callback en pasos atómicos.
- **Inyección denormal**: `juce::ScopedNoDenormals` activado al inicio del callback.

### 3.4 Telemetría Unificada (ABDScope WebView2)
Toda la telemetría gráfica en vivo se sirve a través de `ScopeWebFloatingWindow` usando WebView2:
- Taps SPSC lock-free: `Hardware In (DUT)`, `Stimulus Generator` y `Diagnostic 1kHz`.
- Despacho de paquetes JSON serializados a 30 fps mediante puente nativo `window.__pushScopeFrame()`.

### 3.5 Capa de Perfilado de Sintetizadores y Plugins (Fase 20)
Implementada bajo `src/synth/`, resuelve el perfilado de fuentes autónomas de sonido mediante el **Principio de Separación en Tres Capas**:

$$\text{Receta Científica} \longrightarrow \text{TargetContract} \longrightarrow \text{ISynthTarget} \longrightarrow \text{Audio Observado}$$

1. **Recetas Científicas (`ExperimentRecipe`)**:
   - `NoteExcitationRecipe`: Matriz factorial de 27 tomas (3 velocidades $\times$ 3 duraciones $\times$ 3 repeticiones) para caracterizar pitch, latencia, envolvente y repetibilidad.
   - `ParameterStepRecipe`, `ParameterRampRecipe`, `LocalPerturbationRecipe` ($\pm\Delta$, Jacobiano), `PRBSExcitationRecipe` (Fase 20.3).
   - Recetas agnósticas: desconocen nombres de marcas, plugins o CCs hardcodeados.
2. **Contratos del Target (`TargetContract`)**:
   - **Hardware**: `HardwareMidiContract` declarativo cargado desde `ABDSharedAssets`.
   - **Plugins VST3/AU**: `PluginContractDiscovery` que extrae en caliente los parámetros del plugin y genera un `DiscoveredPluginContract` (evidencia cruda) y un `NormalizedTargetContract` (interpretación normalizada).
   - Clasificación ontológica honesta: `Declared` | `Inferred` | `UserConfirmed` | `Unknown`.
   - Distinción técnica entre `supportsSampleAccurateParameterTransport` (host) y `supportsSampleAccurateParameterProcessing` (plugin).
3. **Targets Aislados (`ISynthTarget`)**:
   - `SyntheticSynthFixture` / `SyntheticSynthTarget`: Simulador analítico en RAM con inyección determinista de fallos (jitter, clipping, caída de notas, deriva térmica).
   - `PluginSynthTarget`: Hospedaje directo de `juce::AudioProcessor` en RAM con despacho sub-bloque en `juce::MidiBuffer` y lectura directa en RAM sin pasar por drivers físicos de sonido.
4. **Análisis Metrológico y Desacoplamiento de Portadora**:
   - `MidiAudioSynchronizer`: Detección de onset y cálculo de latencia de transporte/procesamiento con referencia de tiempo de disparo.
   - `SynthPitchEstimator`: Estimación por función de diferencia cuadrada normalizada (NSDF) con corrección parabólica: $\delta = \frac{\gamma - \alpha}{2(2\beta - \alpha - \gamma)}$ con precisión $\pm 0.005$ cents.
   - `SynthEnvelopeAnalyzer`: Seguidor de picos instantáneo con caída exponencial ($\tau = 40\text{ ms}$) gobernado por `AnalysisPolicy` (umbrales de pico 97%, sostenido +2%, reposo 4%), desacoplando la envolvente lenta de la frecuencia portadora.
5. **Auditoría Previa y Active Learning (Fases 20.2–20.4)**:
   - `TargetAuditor`: Detección de reiniciabilidad (`Resettable`), determinismo (`Deterministic` vs `Stochastic`), persistencia de estado entre notas (`StatefulBehaviorDetected`) y prueba de round-trip de estado binario.
   - `AdaptiveExperimentPlanner`: Active learning guiado por ganancia esperada de información $\arg\max \frac{\text{EIG}(x)}{\text{Cost}(x)}$, partición inmutable (`ExplorationSet`, `IdentificationSet`, `HoldoutSet`) y parada formal ($\frac{\Delta U}{\text{coste}} < \epsilon$).

---

## 4. Exportación y Generación de Código

ABDAudioLab exporta directamente a formatos de producción:
- **`LutExporter`**: Tablas C++ alineadas `alignas(16) static const AbdBatchedPoint` para emuladores VST3 y reportes JSON.
- **`NamDatasetExporter`**: Generación de datasets calibrados normalizados (`input.wav`, `target.wav`, `nam_dataset_manifest.json`) alineados mediante correlación cruzada pre-roll para entrenamiento en Neural Amp Modeler / RTNeural.
- **`CertificationReportExporter`**: Reportes de certificación técnica en HTML5 y gráficos vectoriales SVG autocontenidos.

---

## 5. Referencias y Documentación Relacionada

- [Guía de Integración de Hardware](HARDWARE_INTEGRATION_GUIDE.md) — Protocolo detallado de controladores y contratos JSON.
- [Roadmap del Proyecto](ROADMAP.md) — Planificación de fases, hitos y subfases 20.1–20.7.
- [Protocolos de Hardware](HARDWARE_PROTOCOLS.md) — Especificaciones de tramas SysEx, tablas MIDI CC y control de sintetizadores.
- [Modelos Matemáticos](MATHEMATICAL_MODELS.md) — Algoritmos Farina Sweep, Wiener-Hammerstein, NSDF, envolventes y Jacobiano local.
- [Documento de Traspaso Técnico](HANDOFF.md) — Estado operativo, compilación y suite de pruebas.
