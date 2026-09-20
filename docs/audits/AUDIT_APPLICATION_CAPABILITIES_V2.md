# Matriz V2: Capacidades Funcionales, Comportamentales y Cadenas de Ejecución de ABDAudioLab

**Versión**: 2.0 (Auditoría Exhaustiva de Comportamiento y Grafos de Ejecución)  
**Fecha**: 2026-09-20  
**Proyecto**: ABDAudioLab  
**Herramientas Utilizadas**: `codebase-memory-mcp` (Grafo de 13.828 nodos y 35.072 aristas), MSVC Release, Catch2 v3.5.2  
**Commit Base**: `a77cc8e`  

---

## 1. Justificación y Cambio de Enfoque (V1 vs V2)

La Matriz V1 se limitaba a inventariar **pantallas y vistas visuales**, lo que generaba un punto ciego crítico: trataba la "excitación MIDI" o la "captura de audio" como meros botones de interfaz, sin rastrear la cadena completa de ejecución física y matemática.

La **Matriz V2** adopta un enfoque holístico de **capacidades de comportamiento y flujo de datos**, estructurado en 7 familias operativas:
1. **Generación de Estímulos y Excitación (Acústica y MIDI)**
2. **Captura, Sincronización y Audio I/O en Tiempo Real**
3. **Análisis Espectral, Métricas Metrológicas y Evaluación de Modelos**
4. **Orquestación, Campañas de Ensayo y Gobernanza de Estados**
5. **Persistencia de Sesiones, Fixity Criptográfico y Exportación**
6. **Validación Acústica A/B y Automatización de QA**
7. **Presentación, Topología y Ergonomía del Operador**

---

## 2. Inventario Integral de Capacidades (28 Capacidades Rastreadas)

### Familia 1: Generación de Estímulos y Excitación

| ID | Capacidad | Entrada / Trigger | Símbolos de Código | Cadena de Ejecución | Estado y Persistencia | Tests Asociados | Estado de Confirmación |
|---|---|---|---|---|---|---|---|
| **F-01** | **Carga e Inicialización de VST3** | Selección de plugin en catálogo | `PluginUiCoordinator::loadPlugin`, `PluginHostManager` | `catalogSelector` $\to$ `PluginUiCoordinator` $\to$ `hostManager.loadPluginInstanceAsync` $\to$ `AudioPluginInstance::prepareToPlay` $\to$ `audioEngine.setActivePluginInstance` | `TargetSelectionState` en `ProfilingSessionSnapshot`; guardado en `.abdlabtest` | `[PluginUiCoordinator]`, `[PluginHost]` | **Confirmada en código y UI** |
| **F-02** | **Excitación MIDI Manual (Teclado Virtual)** | Clic o teclado en `MidiKeyboardFloatingWindow` | `MidiKeyboardFloatingWindow`, `LabAudioEngine::postLiveMidiMessage` | Teclado flotante $\to$ `postLiveMidiMessage` $\to$ `liveMidiCollector.addMessageToQueue` $\to$ `plugin->processBlock` | Efímero (tiempo real) | `test_UiCoordinatorGovernance.cpp` (ST-03) | **Confirmada en ejecución real** |
| **F-03** | **Excitación MIDI Automatizada (Secuencias Factoriales)** | `ProfilingSequencer::startSession` con `ExcitationMode::MidiNotes` | `MidiExcitationSequence`, `NoteExcitationRecipe`, `ProfilingHardwareDispatcher::sendNoteOn` | `SessionPlan` $\to$ `ProfilingSequencer::run` $\to$ `dispatcher->sendNoteOn` $\to$ `audioEngine.postLiveMidiMessage` / hardware $\to$ temporizador `gateMs` $\to$ `dispatcher->sendNoteOff` | `MidiExcitationSequence::sequenceHash`, guardado en `SessionManifest::measuredPoints` | `test_UiCoordinatorGovernance.cpp`, `test_Vst3DexedRealHosting_T2.cpp` | **Confirmada en código y tests** (Pendiente ST-11..ST-13) |
| **F-04** | **Cancelación Segura MIDI (Panic / All Notes Off)** | Pausa, Aborto, Overload o Detención de Sesión | `ProfilingSequencer::stopSession`, `ProfilingHardwareDispatcher::sendAllNotesOff` | Botón Stop/Pause o `receiver.isOverloadTriggered()` $\to$ bucle de 16 canales $\to$ `sendAllNotesOff(ch)` $\to$ silencio garantizado | Estado de sesión `Idle` o `Cancelled` | `test_UiCoordinatorGovernance.cpp` | **Confirmada en código y tests** |
| **F-05** | **Generación de Estímulos Acústicos Analógicos** | Inicio de sweep/pulso en cola de ensayos | `LabStimulusGenerator::setStimulus`, `StimulusType` | `ProfilingSequencer` $\to$ `generator.setStimulus(LogFarinaSweep / SyncPulses / Ramp / Sine)` $\to$ `audioDeviceIOCallback` | Tipo de estímulo en `QueueItem` | `test_FarinaDeconvolver.cpp`, `test_MeasurementStimulusCapture.cpp` | **Confirmada en código y tests** |
| **F-06** | **Automatización de Parámetros por MIDI CC / SysEx / NRPN** | Ensayo con `ControlStepConfig` | `ProfilingHardwareDispatcher::executeLifecycleActions`, `HardwareSetupAction` | `ProfilingSequencer` $\to$ `dispatcher->setParameter` / `sendMidiMessage` $\to$ settling delay $\to$ verificación acústica | Parámetros en `MeasuredPoint` | `test_HardwareProtocols.cpp`, `test_MidiAndSysExContracts_T3.cpp` | **Confirmada en código y tests** |

---

### Familia 2: Captura, Sincronización y Audio I/O en Tiempo Real

| ID | Capacidad | Entrada / Trigger | Símbolos de Código | Cadena de Ejecución | Estado y Persistencia | Tests Asociados | Estado de Confirmación |
|---|---|---|---|---|---|---|---|
| **F-07** | **Captura en Búfer Circular con Trigger de Umbral** | Activación del ensayo acústico | `LabResponseReceiver::armCapture`, `LabAudioEngine` | `Sequencer` $\to$ `receiver.armCapture(samples, threshold)` $\to$ `audioDeviceIOCallback` captura hasta llenar búfer $\to$ `receiver.isFinished()` | Muestras grabadas en memoria; exportadas a WAV | `test_AudioEngineBounds.cpp`, `test_MeasurementAudioThroughCapture.cpp` | **Confirmada en código y tests** |
| **F-08** | **Sincronización MIDI/Audio y Detección de Onset** | Muestra de audio grabada tras Note-On | `MidiAudioSynchronizer::detectOnsetSample`, `ChainTransportCalibration` | Audio buffer $\to$ `computeNoveltyFunction` (Spectral Flux/Hilbert) $\to$ detección de pico $\to$ cálculo de retraso de transporte | `TimingMetrics::attackTimeMs`, `transportOffsetMs` en reporte | `test_Phase20_11_5_PhysicalEstimatorsAndRelease.cpp` | **Confirmada en código y tests** |
| **F-09** | **Compensación de Latencia de Hardware y Plugin** | Configuración de plugin o calibración de tarjeta | `AudioPluginInstance::getLatencySamples`, `LabResponseReceiver::setLatencyCompensationSamples` | Plugin/Hardware reporta muestras de latencia $\to$ `receiver.setLatencyCompensationSamples` alinea fase | Almacenado en sesión de calibración | `test_Phase20_12_FineLatency_T1.cpp` | **Confirmada en código y tests** |
| **F-10** | **Calibración Loopback Físico y Trim Digital** | `Start Calibration` en Paso 2 | `NativeCalibrationPanel`, `LabAudioEngine::calibratePluginDigitalTrim` | Calibración $\to$ generación de tono 1 kHz $\to$ medición de ganancia de ida y vuelta $\to$ ajuste de ganancia trim | Guardado en `AnalogCalibrationState` | `test_LoopbackCalibration_T4.cpp`, ST-04 | **Confirmada en código y UI** |
| **F-11** | **Medidores Peak/RMS Continuos en Tiempo Real** | Callback de audio (100 Hz polling) | `SoundIdMeterStrip`, `MainContentTelemetrySource`, `DiagnosticsTelemetryPoller` | `audioDeviceIOCallback` calcula RMS/Peak $\to$ `telemetrySource.poll()` $\to$ `meterStrip.setLevels(...)` | Telemetría efímera sanitizada (sin NaN/Inf) | `test_DiagnosticsTelemetryPoller.cpp`, ST-03 | **Confirmada en ejecución real** |
| **F-12** | **Analizador de Espectro FFT Continuo (Hann 512 bandas)** | Callback de audio | `SoundIdCurvePlotter::getSpectrumAnalyzer`, `LabAudioEngine` | Búfer de 1024 muestras $\to$ ventana Hann $\to$ FFT $\to$ magnitudes dB $\to$ `curvePlotter.repaint()` | Visualización continua | `test_SoundIdViews.cpp`, ST-03 | **Confirmada en ejecución real** |

---

### Familia 3: Análisis Espectral, Métricas Metrológicas y Modelos

| ID | Capacidad | Entrada / Trigger | Símbolos de Código | Cadena de Ejecución | Estado y Persistencia | Tests Asociados | Estado de Confirmación |
|---|---|---|---|---|---|---|---|
| **F-13** | **Desconvolución de Farina y Cálculo de Respuesta al Impulso** | Fin de sweep logarítmico | `FarinaDeconvolver::deconvolve`, `LabAnalyticEngine` | Búfer grabado + señal inversa $\to$ FFT $\to$ producto complejo $\to$ IFFT $\to$ IR lineal + armónicos | Almacenado en `MeasuredPoint::impulseResponse` | `test_FarinaDeconvolver.cpp`, `test_HarmonicAndImdAnalysis.cpp` | **Confirmada en código y tests** |
| **F-14** | **Cálculo de ESR, Correlación Espectral, THD y SNR** | Fin de ensayo de modelado | `AudioABComparator::compare`, `ModelEvaluation` | Señal Target vs Señal Candidata $\to$ alineación $\to$ correlación cruzada $\to$ cálculo de error RMS normalizado | Almacenado en `ModelEvaluationSummaryState` | `test_AudioABComparator.cpp`, `test_AudioABValidationRuns.cpp` | **Confirmada en código y tests** |
| **F-15** | **Validación Cruzada con Dataset de Holdout** | Evaluación de candidato | `ModelHoldoutValidation`, `HoldoutDataset` | Dataset out-of-sample no visto $\to$ simulación del modelo $\to$ verificación de generalización | `ValidationUiSummary` con veredicto | `test_ModelHoldoutValidation.cpp`, `test_HoldoutSequence.cpp` | **Confirmada en código y tests** |
| **F-16** | **Motor de Veredictos Metrológicos (Gatekeeper)** | Finalización de evaluación | `ModelEvaluationBuilder::build`, `synth::ApprovalStatus` | Métricas + auditoría $\to$ evaluación de umbrales $\to$ emisión de status: `Approved`, `ApprovedWithWarnings`, `Inconclusive`, `Rejected` | Hash canónico SHA-256 en snapshot | `test_ModelEvaluation.cpp`, `test_TargetAuditor.cpp` | **Confirmada en código y tests** |

---

### Familia 4: Orquestación, Campañas y Gobernanza

| ID | Capacidad | Entrada / Trigger | Símbolos de Código | Cadena de Ejecución | Estado y Persistencia | Tests Asociados | Estado de Confirmación |
|---|---|---|---|---|---|---|---|
| **F-17** | **Cola de Ensayos y Formulación de Recetas** | Adición de pruebas en Paso 1 / Setup | `SoundIdSuiteList`, `QueueItem`, `MeasurementRecipe` | UI $\to$ `suiteList.addTestToQueue` $\to$ serialización en receta $\to$ pase a `ProfilingSequencer` | `SuiteQueueModelManager`, guardado en sesión | `test_SoundIdSuiteListRefactor.cpp`, `test_MeasurementPresetRecipes.cpp` | **Confirmada en código y UI** |
| **F-18** | **Secuenciador Asíncrono de Campaña en Hilo Dedicado** | Clic en *Iniciar Medición* | `ProfilingSequencer::Thread`, `SessionExecutionCoordinator` | `Start` $\to$ arranca hilo $\to$ bucle de trials $\to$ configuración de target $\to$ disparo $\to$ captura $\to$ análisis $\to$ guardado | `CoordinatorState`, progreso [0..100%] | `test_SessionExecutionCoordinator.cpp`, `test_ProfilingSequencerModulation.cpp` | **Confirmada en código y tests** |
| **F-19** | **Pausa, Reanudación y Paso a Paso con Operador** | Botones de transporte o confirmación manual | `ProfilingSequencer::pauseSession`, `confirmOperatorStep` | `pauseSession()` $\to$ `AllNotesOff` $\to$ espera en `resumeEvent` $\to$ `resumeSession()` | Estado `SessionPaused` | `test_PauseResume.cpp`, `test_OperatorCardsContainerComponent.cpp` | **Confirmada en código y tests** |
| **F-20** | **Snapshots Inmutables y Publicación Monotónica a GUI** | Ciclo de ejecución | `ProfilingSessionController::getCurrentSnapshot`, `ProfilingSessionSnapshot` | Notificación de worker $\to$ construcción de snapshot $\to$ incremento monotónico $\to$ listeners de UI actualizan vistas | `ProfilingSessionSnapshot` inmutable | `test_ProfilingSessionController.cpp`, `test_UiCompositionSeam6.cpp` | **Confirmada en código y tests** |

---

### Familia 5: Persistencia, Procedencia e Informes

| ID | Capacidad | Entrada / Trigger | Símbolos de Código | Cadena de Ejecución | Estado y Persistencia | Tests Asociados | Estado de Confirmación |
|---|---|---|---|---|---|---|---|
| **F-21** | **Persistencia de Sesión Completa en Contenedor ZIP (`.abdlabtest`)** | Guardar sesión (Ctrl+S / Menú) | `SessionSerializer::saveSession`, `SessionPersistenceService` | Sesión $\to$ serialización JSON canónico + WAV de capturas $\to$ compresión ZIP $\to$ archivo único | Archivo `.abdlabtest` en disco | `test_SessionSerializer.cpp`, `test_SessionPersistenceService.cpp` | **Confirmada en código y tests** |
| **F-22** | **Exportación de Informes de Certificación HTML** | Clic en *Exportar Certificado* | `CertificationReportExporter::exportReport` | Métricas + datos de sesión $\to$ render de plantilla HTML interactiva con gráficos embebidos | Archivo HTML autocontenido | `test_CertificationReportExporter.cpp`, `test_CertificationReportExport.cpp` | **Confirmada en código y tests** |
| **F-23** | **Paquete de Producción con Fixity SHA-256 e Inmutabilidad** | Paso 4 (*ExportReport*) | `ReportExportService::generateProductionPackage` | Valida veredicto $\to$ genera C++, JSON, LUT $\to$ calcula SHA-256 de cada archivo $\to$ `checksums.sha256` | Carpeta inmutable de producción | `test_ReportExportService.cpp`, `test_ReportExportUiController.cpp` | **Confirmada en código y tests** |
| **F-24** | **Guardas Metrológicas de Bloqueo de Exportación** | Intento de exportar modelo inválido | `ProfilingSessionController::exportModel`, `synth::isExportAllowed` | Verificación de `approvalStatus` $\to$ Si `InvalidMeasurement`, `Rejected` o `Tampered` $\to$ bloqueo con excepción controlada | `ExportAvailabilityState::exportBlockReason` | `test_ReportExportService.cpp`, `test_ProfilingSessionController.cpp` | **Confirmada en código y tests** |

---

### Familia 6: Validación Acústica A/B y QA

| ID | Capacidad | Entrada / Trigger | Símbolos de Código | Cadena de Ejecución | Estado y Persistencia | Tests Asociados | Estado de Confirmación |
|---|---|---|---|---|---|---|---|
| **F-25** | **Comparador Acústico A/B en Tiempo Real** | Botón Audición A/B en UI | `AudioABVerificationModal`, `LabAudioEngine::loadAuditionLut` | Diálogo modal $\to$ conmutación de bus A (Target) y B (Modelo LUT) $\to$ audición instantánea sin clicks | Efímero | `test_AudioABComparator.cpp`, ST-08 | **Confirmada en código y UI** |
| **F-26** | **Batería de Validación Acústica Automatizada (AB-01 a AB-05)** | Pipeline CI o comando de test acústico | `test_AudioABValidationRuns.cpp`, `AudioABComparator` | Ejecución de 15 corridas (3 por preset) $\to$ análisis espectral, THD, SNR, alineación $\to$ emisión de veredictos | Log acústico + tabla de resultados | `test_AudioABValidationRuns.cpp` (15/15 PASS/WARN) | **Confirmada en ejecución real** |

---

### Familia 7: Presentación, Topología y Ergonomía

| ID | Capacidad | Entrada / Trigger | Símbolos de Código | Cadena de Ejecución | Estado y Persistencia | Tests Asociados | Estado de Confirmación |
|---|---|---|---|---|---|---|---|
| **F-27** | **SoundIdTargetView Integrada en Step 1** | Navegación a Paso 1 | `soundid::SoundIdTargetView`, `WorkflowNavigationController` | Stepper selecciona Paso 1 $\to$ layout responsivo $\to$ muestra tarjeta de target, 155 parámetros y estado de auditoría | Sincronizado reactivamente con snapshot | `test_SoundIdViews.cpp`, ST-01 | **Confirmada en código, tests y UI** |
| **F-28** | **Ventanas Auxiliares (Topología de Estudio y Scope Web)** | Menú Ver / Botones de cabecera | `StudioTopologyController`, `ScopeWebFloatingWindow` | Apertura de ventana $\to$ canvas gráfico vectorial de buses o visualizador web Lissajous | Estado de visibilidad de ventana | `test_StudioTopologyWindow`, ST-09 | **Confirmada en código y UI** |

---

## 3. Grafo de Ejecución Crítico: Excitación MIDI Automatizada (F-03)

```mermaid
graph TD
    UI[SoundIdSuiteList / MeasurementRecipe] -->|Define Recipe: Notas C1-C6, Vel 1-127, Gate 250ms| Plan[NoteExcitationRecipe / ExperimentPlan]
    Plan -->|Pasa receta validada| Seq[ProfilingSequencer (Hilo Dedicado)]
    Seq -->|1. Armar captura acústica con trigger 0V| Rec[LabResponseReceiver]
    Seq -->|2. Inyectar Note On| Disp[ProfilingHardwareDispatcher]
    Disp -->|Si es plugin virtual VST3| Engine[LabAudioEngine::postLiveMidiMessage]
    Disp -->|Si es hardware físico| HW[HardwareController::sendMidiMessage]
    Engine -->|liveMidiCollector| Plug[Dexed.vst3 processBlock]
    Plug -->|Audio renderizado| Rec
    Seq -->|3. Temporizador gateMs transcurrido| Disp
    Disp -->|4. Inyectar Note Off| Engine
    Disp -->|4. Inyectar Note Off| HW
    Rec -->|5. Búfer lleno / Análisis onset| Sync[MidiAudioSynchronizer::detectOnsetSample]
    Sync -->|6. Retraso de transporte y ataque neto| Obs[SynthObservation / MeasuredPoint]
    Obs -->|7. Persistencia inmutable| Store[SessionManager / .abdlabtest]
```

---

| Test ID | Alcance | Requisito Mapeado | Capacidad | Condición de Aceptación |
|---|---|---|---|---|
| **ST-01** | Carga y Visualización de Dexed en `TargetView` | `REQ-TARGET-LOAD` | F-01, F-27 | `SoundIdTargetView` muestra Dexed VST3, 155 params, auditoría OK |
| **ST-02** | Apertura de Editor Nativo VST3 | `REQ-PLUGIN-GUI` | F-01 | Editor se abre flotante sin solapamiento ni congelación |
| **ST-03** | Excitación MIDI Manual y Medición | `REQ-MIDI-MANUAL` | F-02, F-11, F-12 | Teclado virtual genera audio, `meterStrip` reacciona, FFT dibuja espectro |
| **ST-04** | Calibración Loopback en Paso 2 | `REQ-CALIB-LOOPBACK` | F-10 | `NativeCalibrationPanel` ejecuta calibración y muestra offset |
| **ST-05** | Formulación de Cola de Ensayos | `REQ-QUEUE-FORMULATION` | F-17 | Receta estándar cargada en `SoundIdSuiteList` con puntos planificados |
| **ST-06** | Ejecución de Ensayo en Paso 3 | `REQ-SESSION-EXEC` | F-18 | `ProfilingSequencer` ejecuta trial con barra de progreso y salud acústica |
| **ST-07** | Pausa y Reanudación de Campaña | `REQ-SESSION-PAUSE` | F-19 | Sesión se pausa, emite All-Notes-Off y reanuda en el mismo trial |
| **ST-08** | Audición A/B en Paso 4 | `REQ-AUDITION-AB` | F-25 | Conmutación A/B en tiempo real sin artefactos |
| **ST-09** | Exportación de Paquete de Producción | `REQ-EXPORT-PACKAGE` | F-23, F-24 | Genera carpeta con C++, JSON, LUT y `checksums.sha256` |
| **ST-10** | Persistencia y Recarga de Sesión | `REQ-PERSIST-SESSION` | F-21 | Guarda `.abdlabtest` y lo reabre reconstruyendo la sesión exacta |
| **ST-11** | **Excitación MIDI Interna Automatizada** | `REQ-MIDI-AUTO` | F-03 | Secuencia Note-On automática a Dexed sin intervención manual |
| **ST-12** | **Temporización de Compuerta (Note Off)** | `REQ-MIDI-GATE` | F-03 | `gateMs` exacto; Note-Off recibido puntualmente sin notas colgadas |
| **ST-13** | **Matriz Factorial de Velocidades** | `REQ-MIDI-AUTO` | F-03 | Disparo reproducible en 3 velocidades (32, 64, 127) con dinámica |
| **ST-14** | **Registro de Latencia y Settling** | `REQ-MIDI-LATENCY` | F-08 | `MidiAudioSynchronizer` calcula y persiste el offset de transporte |
| **ST-15** | **Detección de Overload / Clipping** | `REQ-SAFETY-OVERLOAD` | F-04 | Sobrecarga de volumen aborta el ensayo y apaga notas de inmediato |
| **ST-16** | **Parada de Emergencia (Panic)** | `REQ-MIDI-PANIC` | F-04 | Clic en Cancelar envía All Notes Off a los 16 canales MIDI (CC 123 val 0) |
| **ST-17** | **Reanudación por Checkpoints** | `REQ-SESSION-CHECKPOINT`| F-18, F-19 | Recuperación de campaña interrumpida desde el último trial exitoso |
| **ST-18** | **Trazabilidad de Hash de Secuencia** | `REQ-MIDI-HASH` | F-03, F-21 | `MidiExcitationSequence::sequenceHash` persistido canónicamente |
| **ST-19** | **Excitación MIDI Externa a Hardware** | `REQ-MIDI-HW` | F-03, F-06 | Transmisión por puerto MIDI físico de canal y CC correctos |
| **ST-20** | **Rollback de Emergencia sin Notas Activas** | `REQ-ROLLBACK-SAFETY` | F-04, F-27 | Conmutar feature flag a `Disabled` limpia notas y restaura layout clásico |

---

## 5. Mapeo Canónico de Identificadores y Capacidades

- **F-03**: Excitación MIDI Automatizada (`REQ-MIDI-AUTO`, `REQ-MIDI-GATE`, `REQ-MIDI-HASH`) $\to$ **ST-11, ST-12, ST-13, ST-18, ST-19**
- **F-04**: Cancelación Segura MIDI (`REQ-MIDI-PANIC`, `REQ-SAFETY-OVERLOAD`, `REQ-ROLLBACK-SAFETY`) $\to$ **ST-15, ST-16, ST-20**
- **F-08**: Sincronización MIDI/Audio y Latencia (`REQ-MIDI-LATENCY`) $\to$ **ST-14**
- **F-15**: Validación Cruzada con Holdout (`REQ-HOLDOUT-VALIDATION`) $\to$ **Suite de Veredicto y Evaluación**
- **F-18 / F-19**: Orquestación y Checkpoints (`REQ-SESSION-CHECKPOINT`) $\to$ **ST-17**

