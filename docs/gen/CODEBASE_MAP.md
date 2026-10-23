# Mapa del Codebase — ABDAudioLab

**Generado:** 2026-10-13 (basado en auditoría completa de `src/`, `tests/`, `docs/`, `CMakeLists.txt`)
**Versión del proyecto:** 2.1.0
**Estado:** Documento vivo — actualizar en cada hito certificado

---

## 1. Estructura de Directorios (Fuente)

```
src/
├── audio/                      # Motor DSP en tiempo real
│   ├── LabAudioEngine.{h,cpp}         # Callback JUCE, zero-allocation, dual-buffer
│   ├── LabAudioReceiver.{h,cpp}       # Captura lock-free (AbstractFifo)
│   ├── LabStimulusGenerator.{h,cpp}   # 9 tipos de estímulo (Farina, Pink, Sync, etc.)
│   ├── LabAnalyticEngine.{h,cpp}      # ⚠️ Análisis con valores hardcodeados (ver deuda)
│   └── LabEnvelopeCurveAnalyzer.{h,cpp}  # ADSR extraction
├── core/                       # Orquestación central
│   ├── ProfilingSequencer.{h,cpp}      # FSM única de ejecución (thread background)
│   ├── ProfilingSessionController.{h,cpp}  # Autoridad de sesión y snapshots
│   ├── ProfilingSessionCoordinator.{h,cpp}  # Coordinación modo asistido/libre
│   ├── SessionManager.{h,cpp}          # Serialización .abdlabtest (ZIP+SHA256)
│   ├── HardwareManager.{h,cpp}         # Catálogo dispositivos + adaptador contratos
│   ├── SessionSerializer.{h,cpp}       # Persistencia binaria + JSON
│   ├── LabResourcePaths.{h,cpp}        # ⚡ RESOLUCIÓN CANÓNICA DE RUTAS (POST-5D.5)
│   ├── ExperimentPlan.{h,cpp}          # Plan canónico (ADR-001)
│   ├── ProfilingSession.{h,cpp}        # Modelos de datos de sesión
│   ├── HardwareContractRegistry.{h,cpp} # Registro contratos JSON (Nivel 1/2)
│   ├── OperatorStepModalDialog.{h,cpp} # Diálogo paso manual
│   ├── MeasurementRecipe.{h,cpp}       # Recetas declarativas (HITO-09A)
│   ├── ExperimentRecipe.{h,cpp}        # Recetas científicas (Fase 20)
│   ├── TargetProfile.{h,cpp}           # Perfiles declarativos JSON (HITO-10A)
│   ├── TargetProfileContracts.{h,cpp}  # Contratos + validación schema
│   ├── TargetProfileLegacyAdapter.{h,cpp} # Adaptador legacy → canónico (HITO-10E)
│   ├── WorkflowNavigationController.{h,cpp} # Navegación stepper
│   ├── ActiveCalibrationContext.{h,cpp}  # Contexto calibración activa
│   ├── SessionExecutionCoordinator.{h,cpp}  # Máquina estados ejecución
│   └── UiStrings.h                     # Catálogo centralizado textos UI
├── export/                     # Exportación producción
│   ├── LutExporter.{h,cpp}         # Tablas LUT SIMD-aligned (C++ headers)
│   ├── NamDatasetExporter.{h,cpp}  # Datasets NAM (WAV + manifest JSON)
│   ├── CertificationReportExporter.{h,cpp}  # HTML5 + SVG autocontenido
│   └── ReportExportService.{h,cpp}     # Orquestador exportación unificada
├── gui/                        # Presentación JUCE (SoundID UI)
│   ├── MainContentComponent.{h,cpp}   # Orquestador principal (desacoplado)
│   ├── drawers/                    # Cajón lateral (File, Hardware, Setup, Telemetría)
│   ├── measurement/                # Visores comparación, curvas, dinámicas
│   ├── plugins/                    # PluginWindowController, scan modal
│   ├── presentation/               # SessionStatusPresenter
│   ├── recipes/                    # Catálogo, editor, selector asistencia
│   ├── session/                    # Controladores sesión + stepper
│   ├── soundid/                    # Componentes SoundID (Stepper, TargetView, RunView)
│   ├── suite/                      # Cola ensayos, renderizadores fila
│   ├── StereoVuMeter.h             # ⚠️ DEAD CODE — no instanciado
│   ├── LiveCurvePlotter.{h,cpp}    # ⚠️ DEAD CODE — reemplazado por SoundIdCurvePlotter
│   ├── InfoDrawer.{h,cpp}          # ⚠️ DEAD CODE — reemplazado por SlideInDrawer
│   └── ... (30+ componentes UI)
├── hardware/                   # Controladores hardware (3 niveles)
│   ├── IHardwareController.h         # Interfaz pura
│   ├── AiraSysExController.{h,cpp}   # Roland AIRA (SysEx)
│   ├── MidiCcController.{h,cpp}      # MIDI CC genérico
│   ├── ManualAnalogueController.{h,cpp}  # Operador manual (barra espaciadora)
│   ├── MockHardwareController.{h,cpp}   # Simulador determinista (tests)
│   ├── HardwareController.h          # Factory + base común
│   ├── MidiIdentityDetector.{h,cpp}  # Device Inquiry (SysEx)
│   ├── MidiDeviceHotplugMonitor.{h,cpp} # Hotplug MIDI
│   ├── RoutingValidator.h            # Validador ruteo AIRA (32 entradas)
│   ├── AiraRoutingValidator.h        # ⚠️ DEAD CODE — clon verbatim de RoutingValidator
│   ├── adapter/                      # SharedMidiHardwareAdapter (HITO-SHARED-SYNC)
│   ├── consent/                      # OperatorConsentService (D2.4)
│   ├── preflight/                    # HardwareTransportPreflightService (D2.3)
│   └── transport/                    # IMidiTransport, JuceMidiTransport, Scheduler (D2.5)
├── ipc/                        # Out-of-process plugin isolation
│   ├── Win32NamedPipe.{h,cpp}        # Named Pipes bidireccionales
│   ├── WorkerProcessHost.{h,cpp}     # Host del worker
│   └── IpcProtocol.h                 # Protocolo serialización
├── math/                       # Algoritmos DSP y análisis
│   ├── FarinaDeconvolver.{h,cpp}     # Deconvolución logarítmica
│   ├── WienerHammersteinFitter.{h,cpp}  # LNL fitting (Adam)
│   ├── LoopbackCalibrator.{h,cpp}    # Calibración loopback + .abdloop
│   ├── NoiseFloorTracker.{h,cpp}     # Piso de ruido 1/3 octava
│   ├── IntermodulationAnalyzer.{h,cpp} # IMD analysis
│   ├── ModulationEstimator.{h,cpp}   # Estimación modulación
│   ├── ModulationMatrixProfile.{h,cpp} # Perfil matriz 4x8
│   ├── SplineInterpolator2D.h        # ⚠️ DEAD CODE — nunca incluido
│   ├── PinkNoise.h                   # Generador ruido rosa
│   ├── PreScanSpectrumAnalyzer.{h,cpp} # Pre-scan espectral
│   ├── ThermalDriftAnalyzer.h        # Deriva térmica
│   ├── AudioABComparator.h           # Comparador A/B 5D (HITO-AUDIO-AB-5D)
│   ├── AudioABMetrics5D.{h,cpp}      # Métricas 5 dimensiones
│   ├── AudioABTolerancePolicy5D.{h,cpp} # Policy tolerancias por preset
│   ├── AudioABVerdictEngine.h        # Motor veredictos
│   ├── LabAnalyticEngine.{h,cpp}     # ⚠️ VALORES HARDCODEADOS (ver deuda)
│   ├── LabEnvelopeCurveAnalyzer.{h,cpp}
│   └── analytics/                    # Dynamics, Envelope, Filter, Modulation Analytics
├── measurement/                # Subsistema medición complejo
│   ├── AnalogChainCompensationContracts.h
│   ├── AnalogChainReversibleCompensator.{h,cpp}
│   ├── AnalogDutCharacterizationContracts.h
│   ├── AnalogDutCharacterizer.{h,cpp}
│   ├── ComplexEnvelopeAnalyzer.{h,cpp}
│   ├── ComplexEnvelopeCaptureSession.{h,cpp}
│   ├── ComplexEnvelopeContracts.h
│   ├── ComplexEnvelopeExportContracts.h
│   ├── ComplexEnvelopeFairExporter.{h,cpp}
│   ├── ComplexEnvelopeHtmlReportGenerator.{h,cpp}
│   ├── ComplexEnvelopeOrchestrator.{h,cpp}
│   ├── ComplexEnvelopeOrchestratorContracts.h
│   ├── ComplexEnvelopeSvgRenderer.{h,cpp}
│   ├── CoordinatorStateMachine.{h,cpp}
│   ├── DexedParametricCampaignContracts.h
│   ├── DexedParametricCampaignCoordinator.{h,cpp}
│   ├── DexedVerticalCampaign.{h,cpp}
│   ├── FineLatencyAnalyzer.{h,cpp}
│   ├── FineLatencyContracts.h
│   ├── LoopbackCalibrator.{h,cpp}    # Duplicado con math/
│   ├── MeasurementCaptureCoordinator.{h,cpp}
│   ├── MeasurementComparisonReportGenerator.{h,cpp}
│   ├── MeasurementContainerExporter.{h,cpp}
│   ├── MeasurementContracts.h
│   ├── MeasurementDspUtils.{h,cpp}
│   ├── MeasurementPresetRecipes.{h,cpp}
│   ├── MeasurementRecipeCanonicalHash.{h,cpp}
│   ├── MeasurementRecipeParsing.{h,cpp}
│   ├── MeasurementRecipeRegression.{h,cpp}
│   ├── MeasurementRecipeValidation.{h,cpp}
│   ├── MeasurementStimulusCapture.{h,cpp}
│   └── ... (20+ archivos)
├── synth/                      # Perfilado sintetizadores/plugins (Fase 20)
│   ├── ExperimentRecipe.h            # Recetas científicas base
│   ├── NoteExcitationRecipe.{h,cpp}  # 27 tomas factorial
│   ├── ParameterStepRecipe.{h,cpp}
│   ├── ParameterRampRecipe.{h,cpp}
│   ├── LocalPerturbationRecipe.{h,cpp}  # Jacobiano ±Δ
│   ├── PRBSExcitationRecipe.{h,cpp}   # Fase 20.3
│   ├── TargetContract.h              # Contratos normalizados
│   ├── HardwareMidiContract.{h,cpp}  # Hardware declarativo
│   ├── PluginContractDiscovery.{h,cpp} # Discovery VST3 dinámico
│   ├── NormalizedTargetContract.{h,cpp}
│   ├── ISynthTarget.h                # Interfaz pura target
│   ├── SyntheticSynthFixture.{h,cpp} # Simulador RAM ground-truth
│   ├── SyntheticSynthTarget.{h,cpp}
│   ├── PluginSynthTarget.{h,cpp}     # Host VST3 in-process
│   ├── MidiAudioSynchronizer.{h,cpp} # Sync MIDI-Audio
│   ├── SynthPitchEstimator.{h,cpp}   # NSDF parabólico ±0.005 cents
│   ├── SynthEnvelopeAnalyzer.{h,cpp} # Seguidor picos τ=40ms
│   ├── SynthObservation.{h,cpp}      # Métricas observables
│   ├── TargetAuditor.{h,cpp}         # Auditoría pre-perfil (resettable, deterministic)
│   ├── AdaptiveExperimentPlanner.{h,cpp} # Active learning EIG/coste
│   ├── ParameterExcitationEngine.{h,cpp}
│   └── CapturedAudioBlock.h          # Bloque audio aislado RAM
├── tests/                      # Suite Catch2 v3 (918 casos, ~209K aserciones)
│   ├── support/                      # Fixtures, mocks, helpers
│   ├── test_*.cpp                    # 150+ archivos de test
│   └── test_ResourcePathHygiene.cpp  # Guard POST-5D.5 (8 casos)
└── worker/
    └── PluginWorkerMain.cpp          # Entry point ABDAudioLab_PluginWorker.exe
```

---

## 2. Mapa de Dependencias Internas (Críticas)

```
┌─────────────────────────────────────────────────────────────────┐
│                        MAIN CONTENT                              │
│  MainContentComponent → ProfilingSessionController              │
│                           ↓                                     │
│              ┌──────────────┴──────────────┐                    │
│              ▼                             ▼                    │
│      ProfilingSequencer              HardwareManager           │
│         (Thread)                      ↓                        │
│              │              ┌───────────┴───────────┐          │
│              │              ▼                       ▼          │
│              │      HardwareContractRegistry    IHardwareController
│              │              │                       │
│              │              ▼                       ▼
│              │       contracts/              AiraSysEx / MidiCc /
│              │       hardware/                 Manual / Mock
│              ▼
│      LabAudioEngine ←──── LabStimulusGenerator
│         │                    │
│         │              LabAudioReceiver (lock-free)
│         ▼
│   LabAnalyticEngine ────► FarinaDeconvolver
│         │                    │
│         ▼                    ▼
│   WienerHammersteinFitter  NoiseFloorTracker
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                    SYNTH/PROFILING (Fase 20)                    │
│  ExperimentRecipe → TargetContract → ISynthTarget               │
│       ↓                  ↓                ↓                     │
│  NoteExcitation    HardwareMidi/    SyntheticSynthFixture       │
│  ParameterStep     PluginContract   PluginSynthTarget           │
│  LocalPerturbation Discovery                                   │
│       ↓                  ↓                ↓                     │
│       └───────────────→ CapturedAudioBlock ←──────────────────┘
│                              ↓
│              ┌──────────────┴──────────────┐
│              ▼                             ▼
│       SynthPitchEstimator           SynthEnvelopeAnalyzer
│              ↓                             ↓
│              └──────────────┬──────────────┘
│                             ▼
│                    SynthObservation
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. Módulos Críticos y su Estado

| Módulo | Archivos | Estado | Deuda Conocida |
|--------|----------|--------|----------------|
| **LabAudioEngine** | 2 | ✅ Operativo | Data races (#1-6 CODE_QUALITY), heap alloc en callback (#7) |
| **LabAudioReceiver** | 2 | ✅ Operativo | Race prepare()/processBlock() (#3,10) |
| **LabStimulusGenerator** | 2 | ✅ Operativo | 7 data races (#5,6,12-14), recompute en loop (#12) |
| **LabAnalyticEngine** | 2 | ⚠️ Parcial | **4 funciones retornan valores hardcodeados** (#22-25) |
| **ProfilingSequencer** | 2 | ✅ Operativo | Thread safety callbacks (#37,38,40,45), raw ptr HW (#39) |
| **SessionSerializer** | 2 | ✅ Operativo | Rule of 5 faltante (#43), validación input binario (#44) |
| **HardwareContractRegistry** | 2 | ✅ Operativo | Raw ptr dangling (#45), error handling JSON (#51,52) |
| **MockHardwareController** | 2 | ✅ Operativo | **CRITICAL**: 7 data races sin atómicos (#53) |
| **MidiCcController** | 2 | ✅ Operativo | TOCTOU (#58,59), use-after-free disconnect (#54) |
| **AiraSysExController** | 2 | ✅ Operativo | TOCTOU (#60), heap alloc hot path (#65), dead code (#64) |
| **ProfilingSessionController** | 2 | ✅ Operativo | Duplicación masiva lógica UI (#78,79) |
| **SlideInDrawer** | 2 | ✅ Operativo | Memory leak createInputStream (#72), duplicación (#78,79) |
| **SoundIdMeterStrip** | 2 | ✅ Operativo | Data race setLevels/timerCallback (#74) |
| **StereoVuMeter** | 2 | 💀 DEAD | Nunca instanciado, en CMakeLists |
| **LiveCurvePlotter** | 2 | 💀 DEAD | Reemplazado por SoundIdCurvePlotter |
| **InfoDrawer** | 2 | 💀 DEAD | Reemplazado por SlideInDrawer |
| **AiraRoutingValidator** | 1 | 💀 DEAD | Clon 100% idéntico de RoutingValidator |
| **SplineInterpolator2D** | 1 | 💀 DEAD | Nunca incluido en ningún .cpp |
| **LoopbackCalibrator (measurement/)** | 2 | ⚠️ Duplicado | Existe también en `math/` — divergencia posible |

---

## 4. Dependencias Externas

| Dependencia | Versión | Uso | Gestión |
|-------------|---------|-----|---------|
| **JUCE** | 8.0.4 | Framework audio/GUI, DSP, MIDI, WebView2 | FetchContent (CMake) |
| **nlohmann/json** | 3.11.3 | Serialización JSON, contratos | FetchContent |
| **Catch2** | v3 | Testing automatizado | FetchContent |
| **RTNeural** | — | Inferencia modelos NAM (export) | Header-only (opcional) |
| **ABDSharedCode** | Pinneado | HardwareMidiDetect, contratos | Git submodule / checkout hermano |
| **ABDSharedAssets** | Versionado | Contratos hardware JSON (40+) | Snapshot en `contracts/hardware/` |
| **WebView2 Runtime** | Evergreen | ABDScope (osciloscopio web) | System / Fixed version |
| **VST3 SDK** | 3.7.x | Hosting plugins | Sistema / JUCE module |

---

## 5. Formatos de Datos Propios

| Formato | Extensión | Ubicación | Descripción |
|---------|-----------|-----------|-------------|
| **Sesión** | `.abdlabtest` | `exports/` | ZIP: manifest.json + measured_points.bin + metadata |
| **Loopback** | `.abdloop` | `%APPDATA%/ABDAudioLab/loopback_profiles/` | JSON + SHA256, curva compensación inversa |
| **Perfil Target** | `.target.json` | `contracts/hardware/` / `profiles/targets/` | TargetProfile declarativo (schema Draft 2020-12) |
| **Contenedor Clonación** | `.abdprofile` | `exports/` | ZIP: metadata.json + infrastructure_snap.json + model_matrix.dat + validation_samples.wav |
| **LUT Export** | `.h` / `.json` | `exported_luts/` | `alignas(16) AbdBatchedPoint` + reporte JSON |
| **NAM Dataset** | `.wav` + `.json` | `exports/nam/` | input.wav, target.wav, manifest.json |
| **Certificación** | `.html` + `.svg` | `exports/reports/` | HTML5 autocontenido + gráficos vectoriales |

---

## 6. Tests — Organización por Categoría

| Categoría | Archivos | Casos | Tags Catch2 |
|-----------|----------|-------|-------------|
| **Paridad/Convergencia** | 3 | 136 | `[parity]`, `[convergence]`, `[integration-01]` |
| **Hardware/Transport** | 15 | 200+ | `[hardware]`, `[transport]`, `[preflight]`, `[consent]` |
| **TargetProfile** | 30 | 500+ | `[targetprofile]`, `[legacy]`, `[dexed]`, `[physical]` |
| **Audio DSP/Math** | 20 | 150+ | `[math]`, `[dsp]`, `[farina]`, `[wiener]` |
| **Audio A/B 5D** | 8 | 180+ | `[audioab_5d]`, `[comparator]`, `[tolerance]` |
| **Export/Report** | 10 | 120+ | `[export]`, `[report]`, `[lut]`, `[nam]` |
| **Synth/Plugin Profiling** | 15 | 200+ | `[synth]`, `[ves]`, `[plugin]` |
| **GUI/Integration** | 25 | 300+ | `[gui]`, `[integration]`, `[smoke]`, `[coherence]` |
| **Resource/Hygiene** | 3 | 65 | `[hygiene]`, `[resource]` |
| **Session/Persistence** | 12 | 180+ | `[session]`, `[serializer]`, `[storage]` |
| **TOTAL** | ~150 | **918** | **~209K aserciones** |

---

## 7. Puntos de Entrada (Entry Points)

| Ejecutable | Main | Propósito |
|------------|------|-----------|
| `ABDAudioLab.exe` | `src/main.cpp` → `MainContentComponent` | Aplicación GUI principal |
| `ABDAudioLab_Tests.exe` | `src/tests/Catch2Main.cpp` | Suite Catch2 completa |
| `ABDAudioLab_PluginWorker.exe` | `src/worker/PluginWorkerMain.cpp` | Worker out-of-process VST3 |

---

## 8. Configuración de Build (CMake)

| Aspecto | Configuración |
|---------|---------------|
| **CMake mínimo** | 3.22 |
| **C++ Standard** | 20 (no extensions) |
| **Compilador** | MSVC 2026 x64 (AVX2) |
| **JUCE** | 8.0.4 via FetchContent |
| **Warnings** | `/W4` `/utf-8` `/permissive-` |
| **Tests** | `add_test()` + CTest, filtro `~[ves]` |
| **Script oficial** | `build.bat` (raíz repo) |
| **Artefactos** | `build/ABDAudioLab_artefacts/Release/` |

---

## 9. Documentación Relacionada (Viva)

| Documento | Ubicación | Propósito |
|-----------|-----------|-----------|
| **ARQUITECTURA** | `docs/ARCHITECTURE.md` | Capas, diagramas, data flow, RT safety |
| **PLAN OPERATIVO** | `docs/PLAN.md` | Hitos certificados, secuencia vinculante |
| **ROADMAP** | `docs/ROADMAP_HARDWARE_AND_EMULATION.md` | Fases D2, VES, Legacy removal |
| **HANDOFF** | `docs/HANDOFF.md` | Estado ejecutivo, autoridades, riesgos |
| **CODE_QUALITY** | `docs/CODE_QUALITY_REPORT.md` | 186+ issues, 10 patrones DRY, 29 dead code |
| **GUIDE_ISSUES** | `docs/GUIDE_ISSUES_TO_AVOID.md` | 16 lecciones aprendidas (thread safety, TOCTOU, etc.) |
| **MATHEMATICAL_MODELS** | `docs/MATHEMATICAL_MODELS.md` | Farina, Wiener-Hammerstein, NSDF, Active Learning |
| **QA_TEST_PLAN** | `docs/QA_TEST_PLAN.md` | 16 casos QA (QA-01 a QA-16) |
| **HARDWARE_PROTOCOLS** | `docs/HARDWARE_PROTOCOLS.md` | SysEx, MIDI CC, control sintetizadores |
| **CONTRACTOS** | `docs/contracts/` | MEASUREMENT_RECIPE, TARGET_PROFILE, FAIR-LNL schemas |

---

## 10. Métricas de Código (Aprox.)

| Métrica | Valor |
|---------|-------|
| **Archivos .h/.cpp (src/)** | ~350 |
| **Líneas código producción** | ~85,000 |
| **Líneas tests** | ~120,000 |
| **Casos de test** | 918 |
| **Aserciones** | 208,972 |
| **Cobertura funcional** | Core DSP, Hardware, Synth, Export, GUI, Session |
| **Deuda técnica (CODE_QUALITY)** | 186+ issues (15 CRITICAL, 31 HIGH) |

---

## 11. Próximas Actualizaciones Requeridas

- [ ] Al certificar **HITO-10D2.7B** (Banco Físico Metrológico): añadir módulos measurement/ validados
- [ ] Al certificar **HITO-10V**: documentar `InProcessVirtualMidiTransport`, VES integration
- [ ] Al cerrar **HITO-SHARED-SYNC**: documentar APIs transferidas a ABDSharedCode
- [ ] Al resolver **LabAnalyticEngine hardcodeados**: marcar como resuelto en esta tabla
- [ ] Al eliminar **dead code** (StereoVuMeter, LiveCurvePlotter, InfoDrawer, AiraRoutingValidator, SplineInterpolator2D): remover de CMakeLists y esta tabla

---

*Este documento debe mantenerse sincronizado con la realidad del código. Última verificación: 2026-10-13 contra `HEAD`.*