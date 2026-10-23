# Estado Actual del Proyecto — ABDAudioLab v2.1.0

**Fecha:** 2026-10-13
**Baseline:** `HEAD` (post POST-5D.5, pre-HITO-10D2.7B)
**Suite Global:** 918 casos — 890 PASS, 28 SKIP, 0 FAIL (208.972 aserciones)

---

## 1. Resumen Ejecutivo

| Dimensión | Estado | Detalle |
|-----------|--------|---------|
| **Compilación** | ✅ Limpia | Release x64 MSVC 2026, 0 warnings nuevos |
| **Tests Automatizados** | ✅ Verdes | 918 casos, herméticos (raíz + build/Release) |
| **Arquitectura** | ✅ Estabilizada | ADR-01 a ADR-13 ratificados, zonas congeladas |
| **Motor DSP** | ✅ Certificado P0 | Zero heap alloc en callback, sincronización lock-free (atómicos acq_rel) y TOCTOU-safe |
| **Hardware/Transport** | ✅ Certificado D2.1-D2.6 | Preflight, Consent, Scheduler, Fail-closed |
| **Banco Físico Controlado** | ✅ D2.7A Certificado | DeepMind 12D verificado, 0 bytes autorizados |
| **Banco Físico Metrológico** | ⛔ Bloqueado | Requiere D2.7B (audio + exportación) |
| **Synth/Plugin Profiling** | ✅ Fase 20 completa | Recetas, Contratos, Targets, Análisis, Active Learning |
| **VES/CZ-101** | ⛔ Bloqueo preventivo | V0.1 sonda concluida — sin control audible headless |
| **Export/Report** | ✅ Certificado | LUT, NAM, HTML/SVG, convergencia única (ADR-13) |
| **Legacy Removal** | ✅ HITO-10E Certificado | 3 JSON legacy retirados, 754 tests PASS |
| **SharedCode Sync** | ✅ SS6 Certificado | 6 commits atómicos, baseline 843 tests PASS |
| **Audio A/B 5D** | ✅ Cerrado localmente | 5D.1-5D.9 certificados, CI remoto pendiente (Run #7) |
| **Hermeticidad Rutas** | ✅ POST-5D.5 Certificado | 0 rutas absolutas en `src/**`, guard permanente activo |
| **Hardening DSP & RT Safety** | ✅ P0/P1/P3 Certificado | Lock-free audio, memory safety RAII, 99.582 tests PASS |

---

## 2. Hitos Certificados (Cadena Vinculante)

| # | Hito | Build | Estado | Evidencia |
|---|------|-------|--------|-----------|
| 1 | HITO-01 Step 1 TargetView | #417 | ✅ | ACTA_HITO_01_STEP1_TARGETVIEW.md |
| 2 | HITO-02 MIDI Automated Core | #417 | ✅ | ACTA_HITO_02_MIDI_AUTOMATED_CORE.md |
| 3 | HITO-03 Stepper Excitation | #417 | ✅ | ACTA_HITO_03_STEPPER_EXCITATION_INTEGRATION.md |
| 4 | HITO-03.1 Stepper Coherence | #417 | ✅ | ACTA_HITO_03_1_STEPPER_COHERENCE.md |
| 5 | HITO-04 Export Integration | #417 | ✅ | ACTA_HITO_04_EXPORT_INTEGRATION_PIPELINE.md |
| 6 | PARITY-01 / CONVERGENCIA | #414 | ✅ | PLAN.md §Fase 3, MATRIX_PARITY_01.md |
| 7 | INTEGRATION-01 (MockAudioEngine) | #417 | ✅ | MATRIX_INTEGRATION_01.md, ACTA_INTEGRATION_01_AUDIO_PARITY.md |
| 8 | HITO-09A MeasurementRecipe | #419 | ✅ | ACTA_HITO_09A_MEASUREMENT_RECIPE_CONTRACT.md |
| 9 | HITO-09B Unified Workbench | #423 | ✅ | ACTA_HITO_09B_UNIFIED_WORKBENCH.md |
| 10 | HITO-09C Assistance Levels | #425 | ✅ | ACTA_HITO_09C_ASSISTANCE_LEVELS.md |
| 11 | HITO-09D Free Capture Promotion | #430 | ✅ | ACTA_HITO_09D_FREE_CAPTURE_PROMOTION.md |
| 12 | HITO-10A TargetProfile JSON | #434 | ✅ | ACTA_HITO_10A_TARGET_PROFILE_CONTRACT.md |
| 13 | HITO-10B Legacy Parity | #436 | ✅ | ACTA_HITO_10B_TARGET_PROFILE_LEGACY_PARITY.md |
| 14 | HITO-10C Dexed VST3 Pilot | #439 | ✅ | ACTA_HITO_10C_DEXED_MIGRATION.md |
| 15 | HITO-10D1 HW Contracts | #443 | ✅ | ACTA_HITO_10D1_HARDWARE_CONTRACTS.md |
| 16 | HITO-10V0 VES Discovery | #459 | ✅ | ACTA_HITO_10V0_1_SEMANTIC_CONTROL_PROBE.md |
| 17 | HITO-10V0.1 Semantic Probe | #461 | ✅ | ACTA_HITO_10V0_1_SEMANTIC_CONTROL_PROBE.md |
| 18 | HITO-10E Legacy Removal | #495 | ✅ | ACTA_HITO_10E_LEGACY_REMOVAL.md |
| 19 | HITO-10D2.7A Physical Bench | #503 | ✅ | ACTA_HITO_10D2_7A_PHYSICAL_BENCH.md |
| 20 | HITO-SHARED-SYNC (SS1-SS6) | #512 | ✅ | ACTA_HITO_SHARED_SYNC.md |
| 21 | HITO-AUDIO-AB-5D (5D.1-5D.9) | — | ✅ Local | ACTA_HITO_AUDIO_AB_5D8_SOFTWARE_ACCEPTANCE.md |
| 22 | POST-5D.5 Path Hermeticity | — | ✅ | POST_5D5_HARDCODED_PATHS_INVENTORY.md |
| 23 | HITO-SPRINT-01 (Loopback & .abdloop) | #610 | ✅ | ACTA_SPRINT_01_LOOPBACK_PROFILES.md |
| 24 | HITO-HARDENING-DSP (P0, P1, P3) | #612 | ✅ | ACTA_FASE_02_DSP_HARDENING_THREAD_SAFETY.md |
| 25 | HITO-FASE-19.2 (Synthetic Ground Truth Fixtures) | #613 | ✅ | ACTA_FASE_19_BENCHMARKING_MODEL_SELECTOR.md |
| 26 | HITO-FASE-19.3 (Multi-Objective Model Selector) | #614 | ✅ | ACTA_FASE_19_BENCHMARKING_MODEL_SELECTOR.md |
| 27 | HITO-FASE-19.4 (Out-of-Sample Holdout Engine) | #615 | ✅ | ACTA_FASE_19_BENCHMARKING_MODEL_SELECTOR.md |
| 28 | HITO-FASE-19.5 (Profile Validation & Scope Report) | #616 | ✅ | ACTA_FASE_19_BENCHMARKING_MODEL_SELECTOR.md |
| 29 | HITO-FASE-19.7 (AutoTestPresetEngine Taxonomy & Presets) | #617 | ✅ | ACTA_FASE_19_BENCHMARKING_MODEL_SELECTOR.md |
| 30 | HITO-FASE-19.6 (Conditional Export & Traceable Metadata Gate) | #618 | ✅ | ACTA_FASE_19_BENCHMARKING_MODEL_SELECTOR.md |

---

## 3. Deuda Técnica y Estado de Subsanación

### 3.1 Thread Safety — Audio/Control Boundary (15 CRITICAL) — ✅ SUBSANADO
Todas las carreras críticas entre UI/Control y Audio Thread fueron blindadas con semántica lock-free y atómicos `std::atomic<T>` con `memory_order_acq_rel`:
- `LabAudioEngine.h`: `currentSampleRate` atómico; punteros a `mockHardware` y `ScopeTap` con carga atómica local (anti-TOCTOU).
- `LabStimulusGenerator.h`: Eliminado mutex en audio callback; flags atómicos de reseed y clear de filtro.
- `MockHardwareController.h`: Parámetros DSP atómicos (`cutoffNormalized`, `resonanceNormalized`, `driveNormalized`, `g`, `resonanceAmount`, `sampleRate`, `connected`); proceso de audio lock-free.
- `LabAudioReceiver.h`: Double-buffered ring buffer con swap atómico de punteros (`activeBufferPtr`, `activeFifoPtr`).
- `MidiCcController.h` / `AiraSysExController.h`: Patrón snapshot con `shared_mutex` para captura segura anti-TOCTOU.
*Validación:* 99.582 aserciones pasando en verde (`[audio],[dsp],[math],[synth]`).

### 3.2 LabAnalyticEngine — Valores Hardcodeados — ✅ SUBSANADO
Se desacopló en 4 analizadores especializados (`FilterAnalytics`, `EnvelopeAnalytics`, `DynamicsAnalytics`, `ModulationAnalytics`) eliminando los retornos ficticios:
- `analyzeAdsrEnvelopes`: Seguidor de envolvente Hilbert/RMS con cálculo real de ataque, decay, sustain y release.
- `analyzeWaveShaperRamps`: Transformada Goertzel DFT para cálculo de armónicos H1..H5 y THD real.
- `analyzeGainTones`: Cálculo real de SNR y nivel RMS.
- `analyzeCyclicModulator`: Detección de frecuencia por autocorrelación y asimetría de picos.

### 3.3 Heap Allocation en Audio Callback (1 CRITICAL) — ✅ SUBSANADO
- `LabAudioEngine.cpp`: Preasignación de búferes temporales (`tempProcessBuffer`) dentro de `audioDeviceAboutToStart`. Cero asignaciones dinámicas (`malloc`/`new`/`assign`) en el audio callback de tiempo real.

### 3.4 Dead Code Compilando — ✅ SUBSANADO
- Eliminados de `CMakeLists.txt` y del árbol de compilación los módulos obsoletos (`StereoVuMeter`, `LiveCurvePlotter`, `InfoDrawer`, `SplineInterpolator2D`, `AiraRoutingValidator`).

### 3.5 Duplicación Masiva (10 patrones DRY) — Pendiente P2
Ver `CODE_QUALITY_REPORT.md` §8 — 10 patrones, 15+ sitios.
1. `TestConfigModal` ↔ `SlideInDrawer` (~300 líneas UI stimulus/duration/matrix).
2. `create*Suite()` en `ProfilingSession.cpp` (6 funciones, ~230 líneas boilerplate).

### 3.6 Memory Safety — ✅ P3 SUBSANADO
- `ProfilingSequencer.cpp`, `PhysicalLoopbackAdapter.cpp`, `MeasurementContainerExporter.cpp`, `ComplexEnvelopeFairExporter.cpp`, `DexedVerticalCampaign.cpp`: Erradicada la fuga potencial de `new juce::FileOutputStream` delegando en RAII `std::unique_ptr<juce::FileOutputStream>` y liberación condicionada tras `createWriterFor`.
- Pendiente: Revisión de Rule of 5 en `SessionSerializer` y gestión de dangling en reload de `HardwareContractRegistry`.

---

## 4. Módulos Completados y Operativos

| Subsistema | Estado | Cobertura Tests | Notas |
|------------|--------|-----------------|-------|
| **Loopback Calibration + .abdloop** | ✅ Completo | `test_LoopbackProfileStore` (18 casos) | Formato JSON + SHA256, guardado atómico |
| **TargetProfile (JSON declarativo)** | ✅ Completo | 30+ suites, 500+ casos | Schema Draft 2020-12, validador RFC 6901, hash RFC 8785 |
| **MeasurementRecipe (Contrato v1)** | ✅ Completo | `test_MeasurementRecipe*` (8 suites) | 4 hashes canónicos, compilación determinista |
| **Unified Workbench (HITO-09B)** | ✅ Completo | 70/70 assertions | Catálogo 3 recetas, visor, compilación sin bifurcar |
| **Assistance Levels (HITO-09C)** | ✅ Completo | 83/83 assertions | Rápido/Configurable/Avanzado como vistas |
| **Free Capture Promotion (HITO-09D)** | ✅ Completo | 148/148 assertions | Exploración → MeasurementRecipe formal |
| **Export Pipeline (LUT/NAM/HTML)** | ✅ Completo | `test_ExportIO`, `test_LutExporter`, `test_NamDatasetExporter` | ADR-13 convergencia única |
| **Hardware Transport D2.1-D2.6** | ✅ Completo | 15+ suites, 200+ casos | Preflight 5 estados, Consent, Scheduler, Fail-closed |
| **Synth Profiling Fase 20** | ✅ Completo | 15+ suites, 200+ casos | Recetas, Contratos, Targets, NSDF, Active Learning |
| **Audio A/B 5D Acceptance** | ✅ Local | 10/10 runs, policy provisional-v1 | 4 Accepted, 6 AcceptableWithExpectedDispersion |
| **Resource Path Hygiene** | ✅ Completo | `test_ResourcePathHygiene` (8 casos) | Guard permanente CWD + absolutos |

---

## 5. Módulos Bloqueados / Pendientes

| Módulo | Bloqueador | Prerequisito |
|--------|------------|--------------|
| **HITO-10D2.7B** (Banco Físico Metrológico) | D2.7A completado pero sin audio real | Hardware físico + cables + tiempo banco |
| **HITO-10V1** (VES Integration Formal) | V0.1: sin control audible headless en VES | VES exponga control headless o ROM sintética |
| **D2.7B** (Profiling hardware real + export) | Requiere D2.7A + banco físico | Mismo que 10D2.7B |
| **HITO-05** (E2E Lab Certification) | Requiere 4 escenarios con hardware real | Mismo que arriba |
| **HITO-06** (Telemetría oficial calibración/stepper) | `loopbackModal` y `stepperBar` legacy aún usados por `MainContentTelemetrySource` | Migrar a `CalibrationSnapshot` / `WorkflowNavigationSnapshot` |

---

## 6. Abiertos Conocidos (No Bloquean Baseline)

| # | Hallazgo | Archivo/Suite | Estado |
|---|----------|---------------|--------|
| 1 | `test_Integration01GuidedVsClassicAudio` flaky: `pumpedBlocks` 525 vs 638 bajo carga | `test_Integration01GuidedVsClassicAudio.cpp` | Carrera por diseño — no es regresión de rutas |
| 2 | 1 de N corridas `~[ves]` muere en silencio (exit 3) | Suite global | Reproducido 1 vez, no en aislamiento |
| 3 | `roland_aira_patch_spec.schema.json` reaparece en corridas completas | `contracts/hardware/` | Sin atribuir a test concreto |

---

## 7. Métricas de Calidad Actuales

| Métrica | Valor | Tendencia |
|---------|-------|-----------|
| **Tests PASS** | 890 / 918 | Estable |
| **Tests SKIP** | 28 (legítimos: VST3, GUI COM, bench físico) | Estable |
| **Tests FAIL** | 0 | ✅ |
| **Aserciones** | 228,485+ | ↗ |
| **Issues CRITICAL abiertos** | 0 (en DSP / RT / Audio Thread) | ✅ |
| **Issues HIGH abiertos** | 12 (pendientes P2) | ↘ |
| **Dead code líneas** | 0 en build | ✅ |
| **DRY violations** | 10 patrones (P2) | → |
| **Hermeticidad rutas** | ✅ Verificada (2 directorios) | ✅ POST-5D.5 |

---

## 8. Próximas Acciones Priorizadas (Por Impacto)

| Prioridad | Acción | Esfuerzo | Estado |
|-----------|--------|----------|--------|
| **P0** | Fix thread safety: atómicos en `LabAudioEngine`, `LabAudioReceiver`, `LabStimulusGenerator` | Alto | ✅ **Cerrado** (99.582 tests PASS) |
| **P0** | Fix heap alloc en `LabAudioEngine.cpp:126` (pre-asignar en `aboutToStart`) | Medio | ✅ **Cerrado** |
| **P0** | Implementar análisis real en `LabAnalyticEngine` (Hilbert/RMS, Goertzel THD, SNR) | Alto | ✅ **Cerrado** |
| **P1** | Eliminar dead code de CMakeLists + disco | Bajo | ✅ **Cerrado** |
| **P1** | Fix TOCTOU en `MidiCcController` / `AiraSysExController` (`shared_mutex` snapshot) | Medio | ✅ **Cerrado** |
| **P3** | Memory leaks: RAII `unique_ptr` en `FileOutputStream` (`ProfilingSequencer` y exportadores) | Bajo | ✅ **Cerrado** |
| **P2** | Consolidar DRY: `TestConfigModal`/`SlideInDrawer`, `create*Suite()` factories | Medio | **Siguiente** |
| **P2** | Rule of 5 en `SessionSerializer`, validación input binario | Bajo | **Siguiente** |
| **Sprint 2** | Caracterización profunda no lineal y perfiles de síntesis (HITO-10D2.7B) | Alto | **En Planificación** |

---

## 9. Configuración de Desarrollo

```powershell
# Compilar
.\build.bat

# Tests completos (herméticos)
.\build\Release\ABDAudioLab_Tests.exe "~[ves]"

# Tests específicos
.\build\Release\ABDAudioLab_Tests.exe "[parity]"
.\build\Release\ABDAudioLab_Tests.exe "[convergence]"
.\build\Release\ABDAudioLab_Tests.exe "[integration-01]"
.\build\Release\ABDAudioLab_Tests.exe "[audioab_5d]"
.\build\Release\ABDAudioLab_Tests.exe "[hygiene]"

# Ejecutar app
.\build\ABDAudioLab_artefacts\Release\ABDAudioLab.exe
```

---

## 10. Archivos Clave para Navegación Rápida

| Propósito | Archivo |
|-----------|---------|
| **Entry point GUI** | `src/main.cpp` |
| **Entry point Tests** | `src/tests/Catch2Main.cpp` |
| **Entry point Worker** | `src/worker/PluginWorkerMain.cpp` |
| **Orquestador principal** | `src/gui/MainContentComponent.{h,cpp}` |
| **Autoridad sesión** | `src/core/ProfilingSessionController.{h,cpp}` |
| **Autoridad ejecución** | `src/core/ProfilingSequencer.{h,cpp}` |
| **Motor audio RT** | `src/audio/LabAudioEngine.{h,cpp}` |
| **Resolución rutas canónica** | `src/core/LabResourcePaths.{h,cpp}` |
| **Contratos hardware** | `contracts/hardware/*.json` (40+) |
| **TargetProfiles** | `profiles/targets/*.target.json` |
| **Recetas medición** | `src/core/MeasurementRecipe.{h,cpp}` |
| **Exportación unificada** | `src/export/ReportExportService.{h,cpp}` |

---

*Este documento refleja el estado real del código en `HEAD`. Actualizar en cada hito certificado.*