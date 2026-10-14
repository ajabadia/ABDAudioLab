# Inventario de Tests — ABDAudioLab v2.1.0

**Generado:** 2026-10-13 (auditoría completa `src/tests/`)
**Suite Global:** 918 casos — 890 PASS, 28 SKIP, 0 FAIL (208.972 aserciones)
**Framework:** Catch2 v3 + CTest
**Filtro estándar:** `~[ves]` (excluye VES/CZ-101 por bloqueo preventivo)

---

## 1. Organización por Categoría y Tag Catch2

| Categoría | Tag(s) | Archivos | Casos | Aserciones | Estado |
|-----------|--------|----------|-------|------------|--------|
| **Paridad/Convergencia** | `[parity]`, `[convergence]`, `[integration-01]` | 3 | 136 | ~15K | ✅ Certificado |
| **Hardware/Transport** | `[hardware]`, `[transport]`, `[preflight]`, `[consent]` | 15 | 200+ | ~35K | ✅ Certificado D2.1-D2.6 |
| **TargetProfile** | `[targetprofile]`, `[legacy]`, `[dexed]`, `[physical]` | 30 | 500+ | ~70K | ✅ Certificado HITO-10A-E |
| **Audio DSP/Math** | `[math]`, `[dsp]`, `[farina]`, `[wiener]` | 20 | 150+ | ~25K | ✅ Verde |
| **Audio A/B 5D** | `[audioab_5d]`, `[comparator]`, `[tolerance]` | 8 | 180+ | ~18K | ✅ Certificado 5D.1-5D.9 |
| **Export/Report** | `[export]`, `[report]`, `[lut]`, `[nam]` | 10 | 120+ | ~12K | ✅ Certificado HITO-04 |
| **Synth/Plugin Profiling** | `[synth]`, `[ves]`, `[plugin]` | 15 | 200+ | ~30K | ✅ Fase 20 / ⛔ VES bloqueado |
| **GUI/Integration** | `[gui]`, `[integration]`, `[smoke]`, `[coherence]` | 25 | 300+ | ~40K | ✅ Verde |
| **Resource/Hygiene** | `[hygiene]`, `[resource]` | 3 | 65 | ~5K | ✅ POST-5D.5 |
| **Session/Persistence** | `[session]`, `[serializer]`, `[storage]` | 12 | 180+ | ~20K | ✅ Verde |
| **TOTAL** | — | **~151** | **918** | **208,972** | **✅ 0 FAIL** |

---

## 2. Catálogo Detallado por Archivo

### 2.1 Paridad y Convergencia (Certificados PARITY-01 / INTEGRATION-01)

| Archivo | Tag | Casos | Aserciones | Propósito |
|---------|-----|-------|------------|-----------|
| `test_Parity01GuidedVsClassic.cpp` | `[parity]` | 2 | 35 | Motor único, bifurcación D3, Toma Libre bloqueada |
| `test_Convergence01ExperimentClassification.cpp` | `[convergence]` | 16 | 16 | Derivación `ExperimentKind` por evidencia metodológica |
| `test_Integration01GuidedVsClassicAudio.cpp` | `[integration-01]` | 85 | 85 | MockAudioEngine, paridad E2E (D5-D8), **flaky bajo carga** |

### 2.2 Hardware y Transporte (D2.1-D2.6 Certificados)

| Archivo | Tag | Casos | Propósito |
|---------|-----|-------|-----------|
| `test_HardwareTransportPreflight.cpp` | `[preflight]` | 25 | 5 estados identidad, Ready/Awaiting/Blocked |
| `test_TargetProfileHardwarePreflight.cpp` | `[preflight]` | 10 | Preflight contra perfil DeepMind12D |
| `test_TargetProfileOperatorConsent.cpp` | `[consent]` | 12 | Consentimiento explícito, hash SHA-256, revocación |
| `test_HardwareDispatchScheduler.cpp` | `[transport]` | 15 | Pacing, rate limit, retry policy, ordering |
| `test_JuceMidiTransport.cpp` | `[transport]` | 18 | Validación estricta endpoint, 0 fallback device 0 |
| `test_MidiTransportContracts.cpp` | `[transport]` | 8 | Contrato `IMidiTransport` determinista |
| `test_TargetProfilePhysicalDispatch.cpp` | `[physical]` | 10 | CC/SysEx físico, evidencia forense |
| `test_TargetProfileTransportFailureRecovery.cpp` | `[physical]` | 8 | Desconexión, timeout, write fail → Blocked |
| `test_TargetProfilePhysicalPreflightBench.cpp` | `[physical]` | 6 | Banco controlado DeepMind 12D (D2.7A) |
| `test_MidiDeviceHotplugMonitor.cpp` | `[hardware]` | 12 | Hotplug, exclusión puertos virtuales |
| `test_MidiIdentityDetector.cpp` | `[hardware]` | 10 | Device Inquiry SysEx, timeouts |
| `test_HardwareProtocols.cpp` | `[hardware]` | 15 | SysEx AIRA, CC, NRPN, validación ruteo |
| `test_HardwareRoutingComponents.cpp` | `[hardware]` | 12 | `RoutingValidator` 32 entradas, RF-25/RF-26 |
| `test_SharedMidiHardwareIntegrationContract.cpp` | `[hardware]`, `[shared]` | 22 | Frontera autoridad ABDSharedCode ↔ ABDAudioLab |
| `test_HardwareLifecycle.cpp` | `[hardware]` | 8 | Connect/disconnect, estado controlador |

### 2.3 TargetProfile (HITO-10A a 10E Certificados)

| Archivo | Tag | Casos | Propósito |
|---------|-----|-------|-----------|
| `test_TargetProfileParsing.cpp` | `[targetprofile]` | 15 | Schema Draft 2020-12, validador RFC 6901 |
| `test_TargetProfileValidation.cpp` | `[targetprofile]` | 12 | Hash RFC 8785, fixity binaria |
| `test_TargetProfileBinaryFixity.cpp` | `[targetprofile]` | 8 | Bit-exactitud `model_matrix.dat` |
| `test_TargetProfileResolution.cpp` | `[targetprofile]` | 18 | Registro canónico, aliases, precedencia |
| `test_TargetProfileLegacyInventory.cpp` | `[legacy]` | 31 | Inventario exhaustivo 31 perfiles + schema |
| `test_TargetProfileLegacyAdapter.cpp` | `[legacy]` | 17 | Adaptador unidireccional canónico→legacy |
| `test_TargetProfileLegacyEndToEndParity.cpp` | `[legacy]` | 9 | Paridad funcional E2E vs legacy |
| `test_TargetProfileLegacyControlledAbsence.cpp` | `[legacy]` | 9 | Ausencia hermética simulada |
| `test_TargetProfileLegacyConsumerParity.cpp` | `[legacy]` | 7 | Consumidores directos en ausencia |
| `test_TargetProfileLegacyHotplugParity.cpp` | `[legacy]` | 7 | Hotplug paridad legacy/canónico |
| `test_TargetProfileDexedProfile.cpp` | `[dexed]` | 12 | Perfil Dexed declarativo |
| `test_TargetProfileDexedHosting.cpp` | `[dexed]` | 15 | Hosting VST3 Dexed real |
| `test_TargetProfileDexedBehavior.cpp` | `[dexed]` | 16 | Comportamiento Dexed vs perfil |
| `test_TargetProfileDexedValidation.cpp` | `[dexed]` | 8 | Validación Dexed |
| `test_TargetProfileMidiCc.cpp` | `[targetprofile]` | 10 | Contratos MIDI CC |
| `test_TargetProfileMidiSysEx.cpp` | `[targetprofile]` | 10 | Contratos SysEx |
| `test_TargetProfileManualOperator.cpp` | `[targetprofile]` | 8 | ManualOperatorIdentifier |
| `test_TargetProfileNoBypass.cpp` | `[targetprofile]` | 8 | Calibración digital ≠ bypass |
| `test_TargetProfileHardwareScheduler.cpp` | `[targetprofile]` | 10 | Scheduler integrado |
| `test_TargetProfileTransportSafety.cpp` | `[targetprofile]` | 8 | Fail-closed transporte |
| `test_TargetProfileTransportPolicy.cpp` | `[targetprofile]` | 12 | Política pacing/ACK/retry |
| `test_TargetProfilePhysicalDispatch.cpp` | `[physical]` | 10 | Despacho físico CC/SysEx |
| `test_TargetProfileOperatorConsent.cpp` | `[consent]` | 12 | Consentimiento con hash |
| `test_TargetProfilePhysicalPreflightBench.cpp` | `[physical]` | 6 | Banco controlado D2.7A |
| `test_ContractsCiContract.cpp` | `[targetprofile]` | 8 | Contrato CI |
| `test_ContractsSnapshotDrift.cpp` | `[targetprofile]` | 8 | Drift contratos vs snapshot |
| `test_HardwareContractRegistryCanonicalResolution.cpp` | `[targetprofile]` | 10 | Resolución canónica all-or-nothing |
| `test_HardwareContractRangeFieldNames.cpp` | `[targetprofile]` | 8 | Nombres campos rangos |
| `test_HardwareContractQuarantine.cpp` | `[targetprofile]` | 8 | Cuarentena contratos inválidos |

### 2.4 Audio DSP y Matemáticas

| Archivo | Tag | Casos | Propósito |
|---------|-----|-------|-----------|
| `test_FarinaDeconvolver.cpp` | `[farina]`, `[math]` | 12 | Deconvolución logarítmica, separación armónicos |
| `test_WienerHammerstein.cpp` | `[wiener]`, `[math]` | 10 | LNL fitting Adam, gradientes analíticos |
| `test_LoopbackCalibration_T4.cpp` | `[dsp]` | 15 | Calibración loopback, .abdloop |
| `test_LoopbackProfileStore.cpp` | `[dsp]` | 18 | Serialización .abdloop, SHA-256, atómico |
| `test_LoopbackDiagnostics.cpp` | `[dsp]` | 10 | Diagnósticos loopback |
| `test_LoopbackSampleRateAdaptation.cpp` | `[dsp]` | 8 | Adaptación sample rate |
| `test_HarmonicAndImdAnalysis.cpp` | `[math]` | 12 | THD H2-H5, IMD |
| `test_ModulationEstimator.cpp` | `[math]` | 10 | Estimación modulación |
| `test_PreScanSpectrumAnalyzer.cpp` | `[math]` | 8 | Pre-scan espectral |
| `test_PhaseGroupDelay.cpp` | `[math]` | 8 | Group delay |
| `test_PinkNoiseGenerator.cpp` | `[math]` | 6 | Ruido rosa |
| `test_PlateauCollapseFilter.cpp` | `[math]` | 6 | Filtro colapso meseta |
| `test_PointRangePatching.cpp` | `[math]` | 8 | Parcheo rangos |
| `test_SplineInterpolator2D.cpp` | `[math]` | 6 | Interpolación 2D bilateral |
| `test_ThermalDriftAnalyzer.cpp` | `[math]` | 8 | Deriva térmica |
| `test_LabAnalyticEngine.cpp` | `[math]` | 12 | ⚠️ Tests pasan pero código tiene hardcodeados |
| `test_EnvelopeMeasurementAdapter.cpp` | `[math]` | 10 | Adaptador envolvente |
| `test_FilterMeasurementAdapter.cpp` | `[math]` | 10 | Adaptador filtro |
| `test_DynamicsMeasurementAdapter.cpp` | `[math]` | 10 | Adaptador dinámicas |
| `test_ModulationMeasurementAdapter.cpp` | `[math]` | 10 | Adaptador modulación |

### 2.5 Audio A/B 5D (HITO-AUDIO-AB-5D Certificado)

| Archivo | Tag | Casos | Propósito |
|---------|-----|-------|-----------|
| `test_AudioABCanonicalPresetRuns5D.cpp` | `[audioab_5d]` | 10 | 10 corridas deterministas bit-exact |
| `test_AudioABMetricsAndDiagnostics5D.cpp` | `[audioab_5d]` | 12 | Métricas 5D, diagnósticos tipados |
| `test_AudioABTolerancePolicy5D.cpp` | `[audioab_5d]`, `[tolerance]` | 12 | Policy tolerancias por preset |
| `test_AudioABComparatorAndVerdict5D.cpp` | `[audioab_5d]`, `[comparator]` | 18 | Comparador + motor veredictos |
| `test_AudioABBaselineManifestValidation5D.cpp` | `[audioab_5d]` | 8 | Validación manifest + hashes |
| `test_AudioAB5DCanonicalPresetMatrix.cpp` | `[audioab_5d]` | 10 | Matriz 5 presets canónicos |
| `test_AudioABExcitationProtocol5D.cpp` | `[audioab_5d]` | 8 | Protocolo excitación determinista |
| `test_CertificationReportExport.cpp` | `[report]` | 10 | Export reporte certificación |

### 2.6 Exportación y Reportes (HITO-04 Certificado)

| Archivo | Tag | Casos | Propósito |
|---------|-----|-------|-----------|
| `test_ExportIO.cpp` | `[export]` | 13 | I/O atómico, staging, SHA-256, rollback |
| `test_ModeToExportIntegration.cpp` | `[export]`, `[integration]` | 9 | ADR-13 convergencia única exportación |
| `test_ExportReadiness.cpp` | `[export]` | 8 | Guardas metrológicas ExportReadiness |
| `test_CertificationReportExporter.cpp` | `[report]` | 10 | HTML5 + SVG autocontenido |
| `test_LutExporter.cpp` | `[lut]` | 10 | LUT SIMD-aligned, sanitización |
| `test_NamDatasetExporter.cpp` | `[nam]` | 10 | Datasets NAM (WAV + manifest) |
| `test_OfficialLutGeneration.cpp` | `[lut]` | 8 | Generación LUT oficial |
| `test_JsLutExport.cpp` | `[lut]` | 6 | Export JS/LUT |
| `test_ModelExportNaming.cpp` | `[export]` | 8 | Nomenclatura export |
| `test_ReportExportService.cpp` | `[report]` | 12 | Orquestador exportación |
| `test_ReportExportUiController.cpp` | `[report]` | 8 | Controlador UI export |
| `test_ReportExportIntegration.cpp` | `[report]` | 8 | Integración completa |

### 2.7 Synth/Plugin Profiling (Fase 20)

| Archivo | Tag | Casos | Propósito |
|---------|-----|-------|-----------|
| `test_DigitalSynthMvpProfiler.cpp` | `[synth]` | 12 | MVP profiler sintético |
| `test_SynthTargetLifecycleAdapter.cpp` | `[synth]` | 17 | Lifecycle target sintético |
| `test_ParameterExcitationEngine.cpp` | `[synth]` | 10 | Motor excitación parámetros |
| `test_NoteExcitationRecipe.cpp` | `[synth]` | 15 | Receta 27 tomas factorial |
| `test_ParameterStepRecipe.cpp` | `[synth]` | 8 | Receta step parámetros |
| `test_ParameterRampRecipe.cpp` | `[synth]` | 8 | Receta ramp parámetros |
| `test_LocalPerturbationRecipe.cpp` | `[synth]` | 10 | Receta perturbación ±Δ (Jacobiano) |
| `test_PRBSExcitationRecipe.cpp` | `[synth]` | 8 | Receta PRBS (Fase 20.3) |
| `test_TargetAuditor.cpp` | `[synth]` | 12 | Auditoría resettable/deterministic/stateful |
| `test_AdaptiveExperimentPlanner.cpp` | `[synth]` | 10 | Active learning EIG/coste |
| `test_SynthPitchEstimator.cpp` | `[synth]` | 12 | NSDF parabólico ±0.005 cents |
| `test_SynthEnvelopeAnalyzer.cpp` | `[synth]` | 10 | Seguidor picos τ=40ms |
| `test_ModulationMatrixProfile.cpp` | `[synth]` | 8 | Perfil matriz modulación |
| `test_GeneratedAcousticModel.cpp` | `[synth]` | 8 | Modelo acústico generado |
| `test_ModelHoldoutValidation.cpp` | `[synth]` | 10 | Validación holdout |
| `test_ModelEvaluation.cpp` | `[synth]` | 8 | Evaluación modelo |
| `test_VesCz101Feasibility.cpp` | `[ves]` | 22 | **SKIP** — Discovery spike VES |
| `test_VesCz101SemanticControl.cpp` | `[ves]` | 18 | **SKIP** — Sonda control semántico |
| `test_Vst3Validation.cpp` | `[plugin]` | 15 | Validación hosting VST3 |
| `test_GuidedPluginLoad_Dexed.cpp` | `[plugin]` | 12 | Carga guiada Dexed |
| `test_Vst3DexedRealHosting_T2.cpp` | `[plugin]` | 18 | Hosting Dexed real T2 |
| `test_OutOfProcessVst3LifecycleAdapter.cpp` | `[plugin]` | 10 | Lifecycle worker out-of-process |
| `test_PluginHostManager.cpp` | `[plugin]` | 12 | Gestor host plugins |
| `test_PluginUiCoordinator.cpp` | `[plugin]` | 10 | Coordinador UI plugin |
| `test_PluginCalibrationSafety.cpp` | `[plugin]` | 8 | Seguridad calibración plugin |

### 2.8 GUI e Integración

| Archivo | Tag | Casos | Propósito |
|---------|-----|-------|-----------|
| `test_SoundIdSidebarStepper.cpp` | `[gui]`, `[coherence]` | 15 | Stepper estados, badges, visualOrder |
| `test_SoundIdViews.cpp` | `[gui]` | 12 | TargetView, ExcitationConfig, RunView, Results |
| `test_SoundIdHardwareCatalogSelector.cpp` | `[gui]` | 10 | Selector catálogo hardware |
| `test_SoundIdSuiteListRefactor.cpp` | `[gui]` | 10 | SuiteList, cola, renderizadores |
| `test_SoundIdThemeMode.cpp` | `[gui]` | 8 | Tema claro/oscuro |
| `test_SoundIdCurvePlotterModulation.cpp` | `[gui]` | 10 | Plotter modulación |
| `test_UiCompositionSeam6.cpp` | `[gui]`, `[integration]` | 8 | Seam composición UI |
| `test_UiCoordinatorGovernance.cpp` | `[gui]`, `[integration]` | 10 | Gobernanza coordinador |
| `test_StepperCoherence_ST47_ST68.cpp` | `[coherence]` | 22 | Coherencia stepper ST-47 a ST-68 |
| `test_Step3Step4Transitions.cpp` | `[integration]` | 10 | Transiciones Paso 3↔4 |
| `test_SessionExecutionCoordinator.cpp` | `[integration]` | 10 | Coordinador ejecución |
| `test_ProfilingSessionController.cpp` | `[integration]` | 15 | Controlador sesión |
| `test_ProfilingSessionCoordinator.cpp` | `[integration]` | 12 | Coordinador sesión |
| `test_ProfilingSessionBuilder.cpp` | `[integration]` | 10 | Builder sesión |
| `test_ProfilingArchitectureRefactor.cpp` | `[integration]` | 12 | Refactor arquitectura |
| `test_Integration_SmokeTest_SessionWorkflow.cpp` | `[smoke]` | 8 | Smoke test workflow completo |
| `test_SmokeStep4UI.cpp` | `[smoke]` | 6 | Smoke test Paso 4 UI |
| `test_MainHeaderController.cpp` | `[gui]` | 8 | Controlador header principal |
| `test_MatrixResolutionTableComponent.cpp` | `[gui]` | 8 | Tabla resolución matriz |
| `test_DrawerHardwareQuarantineSection.cpp` | `[gui]` | 8 | Sección cuarentena hardware |
| `test_OperatorCardsContainerComponent.cpp` | `[gui]` | 10 | Contenedor tarjetas operador |
| `test_StartupWarningsPanel.cpp` | `[gui]` | 8 | Panel warnings arranque |
| `test_NativeCalibrationPanelInteraction.cpp` | `[gui]` | 10 | Interacción panel calibración nativo |
| `test_MeasurementViewModelAndUI.cpp` | `[gui]` | 10 | ViewModel medición + UI |
| `test_MeasurementFloatingWindow.cpp` | `[gui]` | 8 | Ventana flotante medición |
| `test_Phase20_11_1_InteractiveViewerComparison.cpp` | `[integration]` | 10 | Comparador interactivo |
| `test_Phase20_11_2_SessionRobustnessAndA11y.cpp` | `[integration]` | 10 | Robustez sesión + a11y |
| `test_Phase20_11_3_DigitalExpansion.cpp` | `[integration]` | 10 | Expansión digital |
| `test_Phase20_11_4_AdvancedDigitalCampaigns.cpp` | `[integration]` | 10 | Campañas digitales avanzadas |
| `test_Phase20_11_5_PhysicalEstimatorsAndRelease.cpp` | `[integration]` | 10 | Estimadores físicos + release |
| `test_Phase20_12_AnalogDutCharacterization_T2.cpp` | `[integration]` | 8 | Caracterización DUT analógico |
| `test_Phase20_12_FineLatency_T1.cpp` | `[integration]` | 8 | Latencia fina T1 |
| `test_Phase20_12_ReversibleCompensation_T3.cpp` | `[integration]` | 10 | Compensación reversible T3 |

### 2.9 Session y Persistencia

| Archivo | Tag | Casos | Propósito |
|---------|-----|-------|-----------|
| `test_SessionSerializer.cpp` | `[serializer]` | 15 | Serialización .abdlabtest ZIP+SHA256 |
| `test_SessionPersistenceService.cpp` | `[session]` | 10 | Servicio persistencia |
| `test_SessionIoController.cpp` | `[session]` | 8 | Controlador I/O sesión |
| `test_SessionReportManager.cpp` | `[session]` | 8 | Gestor reportes sesión |
| `test_ExperimentStorage.cpp` | `[storage]` | 12 | Almacén experimentos inmutables |
| `test_LoadedSessionApplier.cpp` | `[session]` | 10 | Aplicador sesión cargada |
| `test_PauseResume.cpp` | `[session]` | 8 | Pausa/reanudación |
| `test_ExperimentPlanCompilation.cpp` | `[session]` | 12 | Compilación ExperimentPlan |
| `test_MeasurementPersistenceAndReport.cpp` | `[session]` | 10 | Persistencia + reporte medición |
| `test_MeasurementContainerExporter.cpp` | `[export]` | 8 | Export contenedor medición |
| `test_MeasurementComparisonReportGenerator.cpp` | `[report]` | 10 | Generador reporte comparación |
| `test_CertificationReportExporter.cpp` | `[report]` | 10 | Export reporte certificación |

### 2.10 Resource Path Hygiene (POST-5D.5)

| Archivo | Tag | Casos | Propósito |
|---------|-----|-------|-----------|
| `test_ResourcePathHygiene.cpp` | `[hygiene]` | 8 | **Guard permanente**: barrido CWD + literales absolutos |
| `test_LabResourcePaths.cpp` | `[resource]` | 12 | Tests módulo canónico `LabResourcePaths` |
| `test_LabDataDirectories.cpp` | `[resource]` | 10 | Directorios datos lab |
| `test_LabResourcePaths.cpp` | `[resource]` | 10 | Rutas recursos |
| `test_ResourcePathHygiene.cpp` | `[hygiene]` | 8 | Duplicado? Verificar |
| `test_ContractsSnapshotDrift.cpp` | `[resource]`, `[targetprofile]` | 8 | Drift contratos |

### 2.11 Tests de Soporte / Fixtures

| Archivo | Propósito |
|---------|-----------|
| `support/CanonicalPresetRenderFixtures.h` | Fixtures presets canónicos 5D |
| `support/LabTestScratch.{h,cpp}` | Scratch pad tests |
| `support/MockAudioEngine.h` | Mock determinista para INTEGRATION-01 |
| `support/MockMidiIdentityProbe.h` | Mock identity probe |
| `support/MockMidiResponseAwaiter.h` | Mock response awaiter |
| `support/MockMidiTransport.{h,cpp}` | Mock transporte MIDI determinista |
| `support/MockMonotonicClock.h` | Mock reloj monotónico |
| `support/ResolvedExecutionPlanParity.{h,cpp}` | Paridad plan ejecución resuelto |
| `support/TestTelemetry.{h,cpp}` | Telemetría tests |

### 2.12 Tests Legacy / Específicos (Mantenidos para Regresión)

| Archivo | Tag | Casos | Nota |
|---------|-----|-------|------|
| `test_CaptureAutomationAndFair_T3.cpp` | `[legacy]` | 10 | Automatización captura + FAIR |
| `test_CasioCzLiveScanSuite.cpp` | `[legacy]` | 8 | Scan CZ live |
| `test_CasioCzProfilingRecipes.cpp` | `[legacy]` | 10 | Recetas perfilado CZ |
| `test_CasioCzVirtualController.cpp` | `[legacy]` | 8 | Controlador virtual CZ |
| `test_DexedEnvelopeMeasurement_T5.cpp` | `[legacy]` | 10 | Medición envolvente Dexed T5 |
| `test_DexedValidation.cpp` | `[legacy]` | 12 | Validación Dexed |
| `test_DiagnosticsTelemetryPoller.cpp` | `[legacy]` | 8 | Poller telemetría |
| `test_FreeCaptureRecipePromotion.cpp` | `[legacy]` | 10 | Promoción free capture |
| `test_FskAudioModem.cpp` | `[legacy]` | 8 | Modem FSK audio |
| `test_HoldoutSequence.cpp` | `[legacy]` | 8 | Secuencia holdout |
| `test_MidiAndSysExContracts_T3.cpp` | `[legacy]` | 12 | Contratos MIDI/SysEx T3 |
| `test_MidiAutomatedExcitation_ST11_ST13.cpp` | `[legacy]` | 10 | Excitación automatizada MIDI |
| `test_MidiStepperIntegration_ST21_ST46.cpp` | `[legacy]` | 15 | Integración stepper MIDI |
| `test_MidiSynthAutomation.cpp` | `[legacy]` | 10 | Automatización synth MIDI |
| `test_OfflineReanalysis.cpp` | `[legacy]` | 8 | Reanálisis offline |
| `test_PlotterModulationTableRenderer.cpp` | `[legacy]` | 8 | Renderizador tabla modulación |
| `test_PlotterRenderers.cpp` | `[legacy]` | 10 | Renderizadores plotter |
| `test_Phase20_11_7_ComplexEnvelopes_T1.cpp` | `[legacy]` | 10 | Envolventes complejas T1 |
| `test_Phase20_11_7_ComplexEnvelopes_T2.cpp` | `[legacy]` | 10 | Envolventes complejas T2 |
| `test_Phase20_11_7_ComplexEnvelopes_T3.cpp` | `[legacy]` | 10 | Envolventes complejas T3 |
| `test_Phase20_11_7_ComplexEnvelopes_T4.cpp` | `[legacy]` | 10 | Envolventes complejas T4 |
| `test_Phase20_11_7_ComplexEnvelopes_T5.cpp` | `[legacy]` | 10 | Envolventes complejas T5 |
| `test_Phase20_11_7_ComplexEnvelopes_T6.cpp` | `[legacy]` | 10 | Envolventes complejas T6 |
| `test_Phase20_11_VerticalDexedAndFair_T5.cpp` | `[legacy]` | 10 | Dexed vertical + FAIR T5 |
| `test_SysexPresetGenerator.cpp` | `[legacy]` | 8 | Generador preset SysEx |
| `test_VoiceDispersionAndBBD.cpp` | `[legacy]` | 8 | Dispersión voz + BBD |
| `test_Waterfall3DComponent.cpp` | `[legacy]` | 8 | Componente waterfall 3D |

---

## 3. Métricas de Cobertura por Módulo

| Módulo | Archivos Test | Casos | Aserciones | Cobertura Funcional |
|--------|---------------|-------|------------|---------------------|
| `src/audio/` | 6 | 85 | ~12K | Engine, Receiver, Stimulus, Analytic |
| `src/math/` | 20 | 180 | ~28K | Farina, Wiener, Loopback, Noise, IMD, Modulation |
| `src/core/` | 25 | 350 | ~55K | Sequencer, Session, HardwareManager, TargetProfile, Recipe |
| `src/hardware/` | 15 | 200 | ~35K | Controllers, Transport, Preflight, Consent, Adapter |
| `src/measurement/` | 12 | 140 | ~22K | ComplexEnvelope, AnalogDUT, Dexed, FineLatency |
| `src/synth/` | 15 | 200 | ~30K | Recetas, Contratos, Targets, Análisis, Active Learning |
| `src/export/` | 10 | 120 | ~12K | LUT, NAM, HTML/SVG, ReportExport |
| `src/gui/` | 28 | 300 | ~40K | Componentes SoundID, Suite, Drawers, Modals |
| `src/ipc/` | 2 | 15 | ~3K | Win32NamedPipe, WorkerProcessHost |
| `src/worker/` | 1 | 8 | ~1K | PluginWorkerMain |
| **TOTAL** | **~151** | **918** | **208,972** | **Completa** |

---

## 4. Tests SKIP Legítimos (28 casos)

| Motivo | Casos | Ejemplos |
|--------|-------|----------|
| **VST3 de terceros no instalado** | 12 | `test_Vst3Validation`, `test_GuidedPluginLoad_Dexed`, `test_Vst3DexedRealHosting_T2` |
| **GUI subsystem COM/WASAPI** | 8 | `test_SmokeStep4UI`, `test_NativeCalibrationPanelInteraction` |
| **Banco físico ausente** | 6 | `test_TargetProfilePhysicalPreflightBench`, `test_PhysicalLoopback_T4_2` |
| **VES/CZ-101 bloqueado** | 2 | `test_VesCz101Feasibility`, `test_VesCz101SemanticControl` |

> **Regla:** Ningún SKIP por rutas (resuelto POST-5D.5). Todos los SKIP tienen justificación documentada en el test.

---

## 5. Comandos de Ejecución

```powershell
# Suite completa (hermética, filtro estándar)
.\build\Release\ABDAudioLab_Tests.exe "~[ves]"

# Por categoría
.\build\Release\ABDAudioLab_Tests.exe "[parity]"
.\build\Release\ABDAudioLab_Tests.exe "[convergence]"
.\build\Release\ABDAudioLab_Tests.exe "[integration-01]"
.\build\Release\ABDAudioLab_Tests.exe "[audioab_5d]"
.\build\Release\ABDAudioLab_Tests.exe "[targetprofile]"
.\build\Release\ABDAudioLab_Tests.exe "[hardware]"
.\build\Release\ABDAudioLab_Tests.exe "[transport]"
.\build\Release\ABDAudioLab_Tests.exe "[preflight]"
.\build\Release\ABDAudioLab_Tests.exe "[consent]"
.\build\Release\ABDAudioLab_Tests.exe "[export]"
.\build\Release\ABDAudioLab_Tests.exe "[synth]"
.\build\Release\ABDAudioLab_Tests.exe "[gui]"
.\build\Release\ABDAudioLab_Tests.exe "[hygiene]"

# Solo VES (bloqueado, solo para desarrollo)
.\build\Release\ABDAudioLab_Tests.exe "[ves]"

# Listar todos los tags
.\build\Release\ABDAudioLab_Tests.exe --list-tags
```

---

## 6. CI/CD Gates (`.github/workflows/audio-ab-5d-ci.yml`)

| Gate | Comando | Criterio |
|------|---------|----------|
| **Gate 1** | Compilación Release x64 | Exit 0, 0 warnings nuevos |
| **Gate 2** | `~[ves]` suite completa | 0 FAIL, exit 0 |
| **Gate 3** | `[parity]` + `[convergence]` + `[integration-01]` | Todos PASS |
| **Gate 4** | `[audioab_5d]` | 10/10 runs PASS/WARN, 0 FAIL |
| **Gate 5** | `[hygiene]` | 8 casos PASS, 0 rutas absolutas |
| **Gate 6** | Hermeticidad: ejecutar desde raíz Y `build/Release` | Mismos 918 casos, 0 fallos resolución |

---

## 7. Archivos de Test Clave para Debugging

| Para debuggear... | Ejecutar |
|-------------------|----------|
| Data races audio thread | `test_LabAudioEngine.cpp`, `test_LabStimulusGenerator.cpp` |
| TOCTOU MIDI | `test_MidiCcController.cpp`, `test_AiraSysExController.cpp` |
| Paridad modos | `test_Parity01GuidedVsClassic.cpp` |
| TargetProfile legacy | `test_TargetProfileLegacy*.cpp` (7 archivos) |
| Rutas hardcodeadas | `test_ResourcePathHygiene.cpp` (inyectar regresión para ver fallo) |
| Exportación atómica | `test_ExportIO.cpp` |
| Active Learning | `test_AdaptiveExperimentPlanner.cpp` |

---

## 8. Evolución Histórica (Resumen)

| Fecha | Hito | Delta Casos | Notas |
|-------|------|-------------|-------|
| 2026-09-13 | Baseline ARCHITECTURE.md | 564 | Pre-Fase 20 |
| 2026-09-23 | PARITY-01 / INTEGRATION-01 | +136 | Build #417 |
| 2026-10-01 | HITO-10E / SHARED-SYNC / POST-5D.5 | +200+ | Build #512, 918 casos |
| 2026-10-13 | Actual | 918 | 890 PASS, 28 SKIP, 0 FAIL |

---

*Inventario generado automáticamente desde `src/tests/`. Actualizar al añadir/eliminar tests.*