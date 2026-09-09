# Arquitectura de Software — ABDAudioLab

**Proyecto:** ABDAudioLab — Universal Black-Box Musical Hardware Profiler  
**Versión del documento:** 1.2.0  
**Actualizado:** 2026-09-06  
**Tecnología:** C++20, JUCE 8, CMake, ABDScope (WebView2), ABDSharedCode  

---

## 1. Alcance y Estado de esta Arquitectura

Este documento describe la arquitectura de software **actualmente integrada y operativa en el ejecutable**. Refleja las refactorizaciones de modularización arquitectónica (Fase 4), la integración del ecosistema compartido (`ABDSharedCode`), el desacoplamiento de controladores y la adopción de telemetría unificada vía WebView2.

### Estado de Verificación

| Estado | Significado |
|---|---|
| **Implementado** | El código forma parte del producto, de sus dependencias CMake o de sus módulos compartidos. |
| **Verificado en simulación** | Hay pruebas automatizadas unitarias (Catch2 / CTest) sin dispositivo físico (36/36 tests pasando). |
| **Pendiente de banco** | Requiere interfaz de audio conectada por USB/Thunderbolt, cables patch o sintetizadores de hardware reales. |

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
└───────────────────────────────────┘    └─────────────────────────────────────┘
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

### 3.2 Capa de Hardware: Arquitectura en Dos Niveles
El acceso y control de hardware se estructura en dos niveles conforme a la [Guía de Integración de Hardware](HARDWARE_INTEGRATION_GUIDE.md):

1. **Nivel 1 — Core Compartido (`ABDSharedCode` / `ABDSharedAssets`)**:
   - `HardwareContractRegistry`: Descubrimiento y lectura de contratos normativos JSON (`contracts/hardware/*.json`).
   - `HardwareMidiDetector`: Detección en caliente vía *Universal SysEx Identity Inquiry* (`F0 7E <dev> 06 01 F7`) y heurísticas USB.
   - `HardwareMidiHotplugMonitor`: Suscripción a eventos de conexión/desconexión de dispositivos MIDI.

2. **Nivel 2 — Fachada de Laboratorio (`ABDAudioLab`)**:
   - `IHardwareController`: Interfaz base pura que aísla al secuenciador de los protocolos concretos.
   - `AiraSysExController`: Controlador para la serie modular Roland AIRA por USB SysEx DT1/RQ1 con cálculo oficial de checksum de 7 bits (`(128 - (sum % 128)) & 0x7F`).
   - `MidiCcController`: Automatización de sintetizadores estándar mediante MIDI Continuous Controller y 14-bit NRPN.
   - `ManualAnalogueController`: Guía paso a paso para sintetizadores analógicos y Eurorack con metrónomo y visualizador de controles vectoriales.
   - `MockHardwareController`: Simulación interna en DSP de filtro resonante y ruido térmico para pruebas unitarias automatizadas sin hardware conectado.
   - `RoutingValidator`: Validación topológica que impide conexiones destructivas o no permitidas (RF-25/RF-26).

### 3.3 Motor de Audio en Tiempo Real (DSP RT-Safety)
`LabAudioEngine` implementa `juce::AudioIODeviceCallbackWithContext` y garantiza aislamiento estricto:
- **Zero Heap Allocation en hilo de audio**: Prohibición de `new`, `malloc`, `std::vector::push_back` o llamadas a I/O de disco durante el procesamiento.
- **Dual-Buffer estéreo independiente**: Buffers pre-asignados `tempProcessBufferL` y `tempProcessBufferR` con capacidad para 16.384 muestras por canal.
- **Subrutinas `noexcept` inline**: Descomposición del callback en pasos atómicos (`renderDiagnosticTone`, `renderStimulusAndRoute`, `processInputAndMetrics`, `accumulateFft`, `updateTelemetryTaps`).
- **Inyección denormal**: `juce::ScopedNoDenormals` activado al inicio del callback.

### 3.4 Telemetría Unificada (ABDScope WebView2)
Se ha retirado definitivamente el visor nativo legacy de C++. Toda la telemetría gráfica en vivo se sirve a través de `ScopeWebFloatingWindow` usando WebView2:
- Taps SPSC lock-free: `Hardware In (DUT)`, `Stimulus Generator` y `Diagnostic 1kHz`.
- Despacho de paquetes JSON serializados a 30 fps mediante puente nativo `window.__pushScopeFrame()`.

---

## 4. Exportación y Generación de Código

ABDAudioLab exporta directamente a formatos de producción:
- **`LutExporter`**: Tablas C++ alineadas `alignas(16) static const AbdBatchedPoint` para emuladores VST3 y reportes JSON.
- **`NamDatasetExporter`**: Generación de datasets calibrados normalizados (`input.wav`, `target.wav`, `nam_dataset_manifest.json`) alineados mediante correlación cruzada pre-roll para entrenamiento en Neural Amp Modeler / RTNeural.
- **`CertificationReportExporter`**: Reportes de certificación técnica en HTML5 y gráficos vectoriales SVG autocontenidos.

---

## 5. Referencias y Documentación Relacionada

- [Guía de Integración de Hardware](HARDWARE_INTEGRATION_GUIDE.md) — Protocolo detallado de controladores y contratos JSON.
- [Roadmap del Proyecto](ROADMAP.md) — Planificación de fases y seguimiento de tareas.
- [Protocolos de Hardware](HARDWARE_PROTOCOLS.md) — Especificaciones de tramas SysEx y tablas MIDI CC.
- [Modelos Matemáticos](MATHEMATICAL_MODELS.md) — Algoritmos Farina Sweep, Wiener-Hammerstein (Sasai Adam) y splines 2D.
