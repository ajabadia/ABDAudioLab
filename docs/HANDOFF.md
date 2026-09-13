# Documento de Traspaso Técnico (HANDOFF) — ABDAudioLab

**Proyecto:** ABDAudioLab — Universal Black-Box Musical Hardware & Synth Profiler  
**Versión Actual:** 2.0.0 (Vertical Slice 1 de Fase 20 Completado)  
**Fecha:** 2026-09-13  
**Autor:** Antigravity Lead Architect / ABDSynths  

---

## 1. Resumen del Proyecto y Objetivo

**ABDAudioLab** es una plataforma y banco de pruebas científico para ingeniería inversa, perfilado *Black-Box* y modelado *Grey-Box* de hardware musical (sintetizadores, pedales, procesadores de efectos, módulos Eurorack) y generadores autónomos de sonido (sintetizadores virtuales VST3/AU y sintetizadores digitales físicos):

1. **Perfilado de Circuitos y Efectos (Inyección de Audio)**:
   - Inyección de barridos logarítmicos de Farina, impulsos Dirac Delta y multitonos.
   - Deconvolución armónica y cálculo de THD%.
   - Modelado Wiener-Hammerstein (LNL) con optimizador Adam en C++20.
   - Generación de tablas LUT alineadas (`alignas(16) static const AbdBatchedPoint`) y datasets calibrados para Neural Amp Modeler (NAM / RTNeural).

2. **Perfilado de Sintetizadores Digitales y Plugins (Fase 20 — Generación Autónoma)**:
   - Principio desacoplado en tres capas: $\text{Receta Científica} \longrightarrow \text{TargetContract} \longrightarrow \text{ISynthTarget} \longrightarrow \text{Audio Observado}$.
   - Protocolo factorial sin sesgo de 27 tomas por ensayo ($3\text{ velocidades} \times 3\text{ duraciones} \times 3\text{ repeticiones}$).
   - Estimador de tono NSDF parabólico con resolución sub-cent ($\pm 0.005$ cents).
   - Extractor de envolvente ADSR desacoplado de la frecuencia portadora ($\tau = 40\text{ ms}$, seguidor de picos instantáneo) gobernado por `AnalysisPolicy`.
   - Detección formal de observabilidad: clasificación de fallos en `NOT_OBSERVED_IN_ANCHOR`, `CLIPPING_DETECTED`, `JITTER_DETECTED`, etc.

---

## 2. Entorno de Compilación y Ejecución Reproducible

### Requisitos del Sistema
- **Sistema Operativo**: Windows 11 Pro (x64) o Windows 10 (64-bit).
- **Compilador**: Microsoft Visual C++ 2026 (MSVC v14.4x) / Visual Studio 18 2026 con soporte completo C++20.
- **Flags de Compilador**: `/DWIN32 /D_WINDOWS /EHsc /arch:AVX2`.
- **CMake**: Versión 4.4.0 (mínimo 3.22).
- **Framework de Audio**: JUCE 8.0.4.
- **Instrucciones SIMD**: AVX2 activado de forma nativa.
- **Tarjeta de Sonido**: Compatible con Windows Audio / WASAPI (modo exclusivo recomendado a 24-bit / 96 kHz o 48 kHz).

### Comandos de Compilación y Ejecución

```powershell
# Compilación completa en Release
.\build.bat

# Ejecución de la suite específica del Profiler de Sintetizadores (5 test cases, 49 aserciones)
.\build\Release\ABDAudioLab_Tests.exe "[synth][profiler]"

# Ejecución de la suite global completa de regresión (141 test cases, 134.498 aserciones)
.\build\Release\ABDAudioLab_Tests.exe

# Lanzamiento de la aplicación GUI
.\build.bat run
```

### Ubicación de Binarios
- Aplicación GUI Standalone: `build/Release/ABDAudioLab.exe` (o `build/ABDAudioLab_artefacts/Release/ABDAudioLab.exe`).
- Binario de Tests Automatizados: `build/Release/ABDAudioLab_Tests.exe`.
- **SHA256 del Ejecutable de Tests Auditado**: `F3537A4FD294A741A467D83079F5075ED61DE2499F231913C4865CF7CB073BEA`.

---

## 3. Estructura del Código Fuente

```
ABDAudioLab/
├── CMakeLists.txt              # Configuración CMake, JUCE 8.0.4, C++20 y módulos
├── build.bat                   # Script de compilación automática MSVC
├── docs/                       # Documentación técnica, guías y especificaciones
│   ├── ARCHITECTURE.md         # Arquitectura del sistema y desglose de capas
│   ├── ROADMAP.md              # Roadmap de desarrollo y seguimiento de fases
│   ├── MATHEMATICAL_MODELS.md  # Fundamentos matemáticos (Farina, LNL, NSDF, ADSR, Jacobiano)
│   ├── HARDWARE_INTEGRATION_GUIDE.md # Guía de integración three-tier
│   ├── HARDWARE_PROTOCOLS.md   # Especificaciones SysEx, CC, NRPN y MIDI
│   └── HANDOFF.md              # Este documento de traspaso técnico
├── src/
│   ├── main.cpp                # Punto de entrada GUI Standalone
│   ├── synth/                  # Submódulo de Perfilado de Sintetizadores y Plugins (Fase 20)
│   │   ├── ISynthTarget.h             # Interfaz pura abstracta para targets (fixture, plugin, hardware)
│   │   ├── SyntheticSynthFixture.h    # Simulador analítico determinista con inyección de fallos
│   │   ├── SyntheticSynthTarget.h     # Adaptador de fixture a ISynthTarget
│   │   ├── PluginSynthTarget.h        # Hospedaje directo en RAM de juce::AudioProcessor (sample-accurate)
│   │   ├── SynthPresetState.h         # Identidad binaria, metadatos y hashes de presets
│   │   ├── MidiExcitationSequence.h   # Eventos de excitación MIDI y duraciones
│   │   ├── MidiAudioSynchronizer.h    # Sincronización desacoplada, onset y latencia
│   │   ├── SynthPitchEstimator.h/.cpp # Estimador NSDF con vértice parabólico corregido
│   │   ├── SynthEnvelopeAnalyzer.h/.cpp # Seguidor de picos instantáneo gobernado por AnalysisPolicy
│   │   ├── DigitalSynthMvpProfiler.h/.cpp # Orquestador de la matriz factorial y veredictos
│   │   ├── Sha256.h                   # Generador de hashes criptográficos autónomo FIPS 180-4
│   │   └── SynthObservation.h         # Modelos de observación y estados de observabilidad
│   ├── audio/                  # Motor de audio en tiempo real
│   │   ├── LabAudioEngine.h/.cpp       # Callback de audio, AudioDeviceManager y tono 1kHz
│   │   ├── LabStimulusGenerator.h/.cpp # Generador de estímulos (Farina, Dirac, Ruido)
│   │   └── LabAudioReceiver.h/.cpp     # Receptor lock-free (FIFO) y trigger por umbral
│   ├── hardware/               # Capa de abstracción de hardware clásica
│   │   ├── HardwareController.h        # Interfaz abstracta pura IHardwareController
│   │   ├── MockHardwareController.h    # Simulación DSP en memoria para tests automáticos
│   │   ├── AiraSysExController.h       # Controlador Roland AIRA (SysEx RQ1/DT1 y CC)
│   │   ├── MidiCcController.h          # Controlador genérico MIDI CC
│   │   └── ManualAnalogueController.h  # Controlador interactivo con asistente rítmico
│   ├── math/                   # Motor matemático y estadístico
│   │   ├── FarinaDeconvolver.h/.cpp    # Deconvolución logarítmica y THD %
│   │   ├── WienerHammersteinFitter.h/.cpp # Ajuste LNL con optimizador Adam en C++20
│   │   └── LabAnalyticEngine.h/.cpp    # Extracción de (µ, σ) para los 5 bloques funcionales
│   ├── core/                   # Secuenciador, sesiones y gestor de hardware
│   │   ├── SessionManager.h/.cpp       # Carga/guardado ZIP (.abdlabtest) y recovery
│   │   ├── SessionSerializer.h/.cpp    # Serialización JSON y checksum SHA-256
│   │   ├── HardwareManager.h/.cpp      # Orquestador y selector de hardware
│   │   └── ProfilingSequencer.h/.cpp   # Máquina de estados en hilo de fondo
│   ├── gui/                    # Interfaz de usuario SoundID
│   │   ├── AppTheme.h                  # Tokens de color y tipografía estandarizados
│   │   ├── MainContentComponent.h/.cpp # Ventana principal y orquestación
│   │   ├── suite/                      # Lista de ensayos y renderizadores de fila
│   │   ├── drawers/                    # Paneles laterales (File, Hardware, Setup)
│   │   └── webview/                    # Telemetría en vivo vía WebView2 (ABDScope)
│   ├── export/                 # Generación de código y datasets
│   │   ├── LutExporter.h/.cpp          # Exportador de .h C++ (alignas 16) y .json
│   │   ├── NamDatasetExporter.h/.cpp   # Exportador de datasets para Neural Amp Modeler
│   │   └── CertificationReportExporter.h/.cpp # Reportes técnicos HTML5/SVG
│   └── tests/                  # Suite completa de tests automatizados (Catch2 v3)
└── exported_luts/              # Carpeta de salida de LUTs y reportes generados
```

---

## 4. Principios y Reglas Críticas de Diseño

1. **Zero-Allocation en el Hilo de Audio (RT-Safety)**:
   - Prohibido `new`, `malloc`, redimensionamiento de contenedores o llamadas de sincronización bloqueante en `processBlock`. Búferes preasignados en `prepareToPlay()`.
2. **Protección contra Denormales**:
   - Todo callback de audio inicia con `juce::ScopedNoDenormals noDenormals;` (`RNF-14`).
3. **Principio de Separación en Tres Capas (Fase 20)**:
   $$\text{Receta Científica} \longrightarrow \text{TargetContract} \longrightarrow \text{ISynthTarget} \longrightarrow \text{Audio Observado}$$
   - Cero hardcodeo de nombres, marcas, CCs o números de patch en el motor metrológico. Toda la semántica proviene del contrato.
4. **Política de Análisis Metrológico (`AnalysisPolicy`)**:
   - Las constantes de decaimiento ($\tau = 40\text{ ms}$) y umbrales de ataque/sustain/release pertenecen al extractor de análisis, no a los sintetizadores evaluados, y se registran en el manifiesto con su `analysisPolicyId` y `analysisPolicyVersion`.
5. **Sample Accuracy: Transporte vs Procesamiento**:
   - Se audita independientemente la capacidad del host de posicionar eventos en la muestra exacta (`transportedSampleAccurate`) frente a la respuesta temporal del procesador del target (`processedSampleAccurate`).

---

## 5. Próximos Pasos (Fases 20.2 & 20.3)

1. **Fase 20.2: TargetAuditor**:
   - Implementar `TargetAuditor.h/.cpp` con matriz de validación previa:
     - Detección de reset determinista (`Deterministic` vs `DeterministicAfterReset`).
     - Detección de persistencia de estado residual entre notas (`StatefulBehaviorDetected`).
     - Clasificación de aleatoriedad (`Deterministic`, `StochasticWithSeed`, `StochasticUnseeded`).
     - Prueba de round-trip de estado binario (`A -> render -> save -> B -> render -> restore A -> render -> compare A`).
2. **Fase 20.3: Parameter Excitation Engine**:
   - Construir jerarquía de recetas: `ParameterStepRecipe`, `ParameterRampRecipe`, `LocalPerturbationRecipe` ($\pm\Delta$), `FactorialInteractionRecipe`, `PRBSExcitationRecipe`.
   - Trazabilidad de los 4 estados por evento: `Requested -> AcceptedByHost -> AppliedByTarget -> ObservedInAudio`.
   - Partición estricta de conjuntos: `ExplorationSet`, `IdentificationSet` y `HoldoutSet` inmutable.
