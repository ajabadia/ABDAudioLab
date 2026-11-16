# Inventario Completo de la Interfaz C++ (ABDAudioLab)

**Generado**: 2026-10-14  
**Versión**: 1.0  
**Alcance**: Interfaz nativa JUCE/WebView2 completa — ~300 archivos C++ (GUI + Core/DSP/Hardware)

---

## 1. BARRA SUPERIOR (MainHeaderController)
**Archivos**: `MainHeaderController.h/.cpp`, `TopHeaderWidgets.h/.cpp`, `AudioMidiStatusPill.h/.cpp`, `HardwareSelectorPill.h/.cpp`, `ThemeToggleButton.h/.cpp`

| Elemento UI | Archivos | Función |
|-------------|----------|---------|
| **File Menu Button** (☰) | `MainHeaderController` | Abre menú contextual con acciones de sesión y sistema |
| **New Session** | `MainHeaderController::onNewSession` → `MainContentComponent::promptNewSession` | Crea nueva sesión limpia (reset todo) |
| **Open Session...** | `onOpenSession` → `handleOpenSession` | Carga sesión `.abdlabtest` via file picker |
| **Save Session** | `onSaveSession` → `handleSaveSession` | Guarda sesión actual |
| **Save Session As...** | `onSaveSessionAs` → `handleSaveSessionAs` | Guarda con nombre/ubicación |
| **Reanalyze Offline** | `onReanalyzeOffline` → `performOfflineReanalysis` | Re-procesa audio crudo de sesión actual offline |
| **Export Certification Report** | `onExportCertificationReport` → `requestExportCertificationReport` | Exporta informe HTML certificado |
| **Open Export Folder** | `onOpenExportFolder` → `openExportFolderInExplorer` | Abre carpeta exports en Explorer |
| **Open Experiments Folder** | `onOpenExperimentsFolder` | Abre carpeta experiments |
| **Open Reports Folder** | `onOpenReportsFolder` → `openReportsFolderInExplorer` | Abre carpeta reports |
| **Open Presets Folder** | `onOpenPresetsFolder` | Abre carpeta presets |
| **Open Loopback Profiles Folder** | `onOpenLoopbackProfilesFolder` | Abre carpeta perfiles `.abdloop` |
| **Set Working Directory...** | `onSetWorkingDirectory` → `promptSelectWorkingDirectory` | Selecciona directorio raíz de trabajo |
| **Open Working Directory** | `onOpenWorkingDirectoryFolder` → `openWorkingDirectoryInExplorer` | Abre directorio de trabajo en Explorer |
| **Exit Application** | `onExitApp` → `confirmAndExit` | Cierra app con confirmación si hay datos sin guardar |
| **Scan Plugin Directories** | `onScanPluginDirectories` → `pluginScanModal.showModal` | Modal para escanear directorios VST3 |
| **Scope Button** | `onScopeToggle` → `toggleScopeWebWindow` | Alterna ventana flotante Scope WebView2 |
| **Virtual Keyboard Button** | `onVirtualKeyboardToggle` → `toggleVirtualKeyboardWindow` | Alterna teclado MIDI virtual flotante |
| **Audio/MIDI Status Pill** | `AudioMidiStatusPill` + `updateAudioMidiStatus()` | Muestra dispositivo audio activo, SR, buffer, latencia, MIDI I/O; auto-refresh al cambiar dispositivo |
| **Calibration Pill** | `btnCalibratePill` + `updateCalibrationStatus()` | Estado calibración: ⬤ Sin calibrar / ✓ Calibrado (SR) / ⚠ Recalibrar / ⏭ Skip; click → Paso 2 |
| **Hardware Selector Pill** | `HardwareSelectorPill` + `onHardwareSelectorClicked` | Muestra target seleccionado; click → Paso 1 (Hardware Routing) |
| **Theme Toggle** | `ThemeToggleButton` + `onThemeToggled` | Alterna Dark (audiolab) / Light (audiolab-light) |
| **Notification Bell** | `NotificationBellButton` + `onNotificationBellClicked` | Badge contador avisos; click → overlay panel avisos cuarentena |
| **Configure Audio/MIDI** | `onConfigureAudioMidi` → `openAudioMidiSettings` | Abre `AudioDeviceSelectorComponent` JUCE nativo |

---

## 2. MENÚ ARCHIVO (File Menu - en MainHeaderController)
**Archivos**: `MainHeaderController.cpp`, `MainContentComponentPersistence.cpp`, `SessionIoController.h/.cpp`

| Acción | Archivos | Función |
|--------|----------|---------|
| **New Session** | `promptNewSession` / `performNewSessionReset` | Reset completo: calibración, cola tests, plotter, sessionManager, plugin, workflow stepper |
| **Open Session...** | `handleOpenSession` / `performOpenSessionFileChooser` | File picker → `SessionIoController` → `SessionSerializer::loadSession` → `applyLoadedSession` |
| **Save Session** | `handleSaveSession` → `SessionIoController::handleSaveSession` | Serializa `SessionManifest` + puntos medidos a `.abdlabtest` |
| **Save Session As...** | `handleSaveSessionAs` | File picker + save |
| **Reanalyze Offline** | `performOfflineReanalysis` | `SessionManager::reanalyzeSessionOffline` sobre audio crudo → refresca plotter + auto-save |
| **Export Certification Report** | `requestExportCertificationReport` | `CertificationReportExporter::exportReport` → HTML + assets |
| **Open Export/Experiments/Reports/Presets/Loopback Folders** | `*FolderInExplorer` | `juce::File::revealToUser()` / `startAsProcess()` |
| **Set Working Directory** | `promptSelectWorkingDirectory` → `applyWorkingDirectory` | `core::setAndPersistDataRootOverride` + actualiza stores |
| **Open Working Directory** | `openWorkingDirectoryInExplorer` | Abre carpeta raíz en Explorer |
| **Exit Application** | `confirmAndExit` | Diálogo confirmación → `systemRequestedQuit()` |

---

## 3. CAJÓN LATERAL IZQUIERDO (SlideInDrawer + SetupInfoTab)
**Archivos**: `SlideInDrawer.h/.cpp`, `DrawerSetupTab.h/.cpp`, `SetupTab.h/.cpp`

| Sección | Elementos | Función |
|---------|-----------|---------|
| **Pestaña Setup (Paso 0)** | `SetupInfoTab` / `DrawerSetupTab` | Telemetría audio (dispositivo, SR, buffer, latencia), MIDI I/O activos, directorio export, auto-trim, versión app |
| **Target Hardware Info** | `setTargetHardwareInfo()` | Foto, nombre, categoría, bus routing, estado conexión (CONNECTED/SELECTED) |
| **Detected Interface** | `setDetectedInterfaceInfo()` | Auto-detección interfaces (PreSonus, Roland AIRA MX-1, Generic) con estado Audio/MIDI real |
| **Hardware Catalog** | `drawer.setHardwareList()` + `catalogSelector` | Lista contratos desde `HardwareContractRegistry` con filtro, búsqueda, selección |
| **Quarantine Tab** | `drawer.setQuarantinedProfiles()` | Contratos retenidos (JSON inválido); botón reescanear catálogo |
| **Operator Notes** | `getOperatorNotes()` / `setOperatorNotes()` | Notas libres del operador (temperatura, warmup, etc.) |
| **Environment** | `getAmbientTemperature()` / `getWarmupTimeMinutes()` | Temperatura ambiente °C, tiempo calentamiento min |

---

## 4. WORKFLOW STEPPER BAR (SoundIdSidebarStepper)
**Archivos**: `SoundIdSidebarStepper.h/.cpp`, `WorkflowNavigationController.h/.cpp`

| Paso | Enum | Estado | Acción Click |
|------|------|--------|--------------|
| **0. System Info** | `Step::SystemInfo` | Completed/Current | `showInfoDrawer()` |
| **1. Target & Routing** | `Step::HardwareRouting` | Locked/Current/Completed | Navega a selección hardware |
| **2. Calibrate Loopback** | `Step::CalibrateLoopback` | Locked/Current/Completed/Skipped | Navega a `NativeCalibrationPanel` |
| **3. Run Session** | `Step::RunSession` | Locked/Current/Completed | Panel profiling activo |
| **4. Export Report** | `Step::ExportReport` | Locked/Current/Completed | Panel `ExportReportPanel` |
| **Collapse Toggle** | `onCollapseToggled` | Colapsado/Expandido | Alterna ancho sidebar (iconos solo vs texto) |
| **Session Summary Card** | `setSessionSummary()` | Resumen sesión | Target name, Loopback status (SNR), Progress points |

---

## 5. PANEL CENTRAL - ÁREA DINÁMICA (WorkflowNavigationController)
**Archivos**: `WorkflowNavigationController.h/.cpp`, `MainContentComponentHardware.cpp`

| Vista Activa | Componente | Trigger |
|--------------|------------|---------|
| **Paso 0: Studio Setup** | `SetupInfoTab` (en cajón) | Step 0 activo |
| **Paso 1: Hardware Routing** | `SoundIdHardwareCatalogSelector` + `SoundIdTargetView` | Step 1 activo / click Hardware Selector Pill |
| **Paso 2: Calibración** | `NativeCalibrationPanel` | Step 2 activo / click Calibration Pill / `workflowNavController.setStep(CalibrateLoopback)` |
| **Paso 3: Profiling Session** | `SoundIdSuiteList` + `SoundIdProfilingRunView` / `SoundIdResultsSummaryView` | Step 3 activo / click Run en SuiteList |
| **Paso 4: Export Report** | `ExportReportPanel` | Step 4 activo / `workflowNavController.setStep(ExportReport)` |

---

## 6. SELECCIÓN HARDWARE Y TARGET (Paso 1)
**Archivos**: `SoundIdHardwareCatalogSelector.h/.cpp`, `SoundIdTargetView.h/.cpp`, `catalogSelector` callbacks

| Elemento | Archivos | Función |
|----------|----------|---------|
| **Hardware List** | `catalogSelector.setContracts()` | Lista todos contratos `HardwareContractRegistry` con filtro búsqueda, categorías |
| **Function Selector** | `FunctionItem` por contrato | Selecciona función/perfil específico del hardware (ej. DS-1 "Distortion", "Boost") |
| **Virtual Plugin Mode** | `catalogSelector.isPluginVirtualMode()` | Toggle plugin VST3 vs hardware físico; carga plugin vía `PluginUiCoordinator` |
| **Auto-Detect Hardware** | `onAutoDetectRequested` → `drawer.triggerAutoDetect()` → `JuceHardwareMidiPicker` | Detección MIDI SysEx automática de hardware conectado |
| **Continue Button** | `onContinueRequested` | Lock hardware, crea test estándar automático, avanza a Paso 2/3 |
| **Target View** | `SoundIdTargetView` | Foto hardware, contrato, rango medición, modo control (MIDI CC, SysEx, Manual, Audio Only) |
| **Studio Topology** | `StudioTopologyController` (integrado) | Canvas interactivo ruteo Audio/MIDI con cables animados |

---

## 7. CALIBRACIÓN LOOPBACK NATIVA (Paso 2 - NativeCalibrationPanel)
**Archivos**: `NativeCalibrationPanel.h/.cpp`, `CalibrationPanelPainter*.cpp`, `NativeCalibrationPanelUiState.cpp`, `NativeCalibrationPanelLayout.cpp`, `NativeCalibrationPanelProfiles.cpp`, `NativeCalibrationPanelWorkflow.cpp`

| Sub-Vista | Botones / Controles | Función |
|-----------|---------------------|---------|
| **2A: Noise Baseline** | `btnStartMeasure` "Start Loopback Calibration" | Mide piso ruido entrada (sin señal) → valida < -85 dBFS / Below Measurement Floor |
| **2B: Physical Loopback** | `btnStartMeasure` (cambia a "Start Loopback Sweep") | Sweep Farina/Golay Output→Input → latencia sub-muestra, SNR, THD+N, flatness, trim gain |
| **Progress Bar** | `progressBar` + `timerCallback()` | Progreso real tiempo (ticks/segundos) con texto estado |
| **Live Input Peak** | Meter visual | Pico entrada en dBFS tiempo real |
| **Resultados** | `LoopbackReport` + `NoiseBaselineReport` | Latencia (samples/ms), SNR (dB), Noise Floor (dBFS), Gain Trim (dB), Phase inversion, Clipping |
| **Saved Profiles** | `btnToggleSavedProfiles` + `btnPrevProfile`/`btnNextProfile` + `btnViewProfileDetails` | Lista perfiles `.abdloop` guardados con match evaluation (★ = compatible actual) |
| **Actions Row** | `btnReuseCalibration` / `btnSaveCalibration` / `btnSkip` / `btnRetry` / `btnContinue` / `btnVerifyDigital` | Reutiliza perfil compatible / Guarda nuevo / Bypass / Reintenta / Avanza / Verifica latencia digital |
| **Profile Details Modal** | `onRequestShowProfileDetails` → `confirmationModal` | Detalles completos: device snapshot, routing, métricas, fecha, hash |
| **Preflight Check** | `onPreflightCheck` callback | Valida directorio trabajo antes de medir (modal confirmación) |

**Callbacks hacia MainContentComponent**:
- `onCalibrationApplied` → activa `ActiveCalibrationContext`, aplica auto-trim, actualiza stepper
- `onCalibrationProgress(stage, progress, statusText, currentDb)` → para telemetría WebUI
- `onCalibrationComplete(success, data, errorReason)` → guarda perfil auto/manual
- `onCalibrationSkipped` → marca step skipped, avanza
- `onCalibrationResetRequested` → desactiva calibración, reinicia paso 2A
- `onOpenProfileHistory` → popup menú perfiles guardados
- `onVerifyDigitalRequested` → verificación latencia digital

---

## 8. SESIÓN DE PROFILING (Paso 3) - SuiteList + Execution
**Archivos**: `SoundIdSuiteList.h/.cpp`, `SuiteQueueModelManager`, `SuiteListEventHandler`, `SessionExecutionCoordinator.h/.cpp`, `ProfilingSequencer.h/.cpp`, `SoundIdProfilingRunView.h/.cpp`, `SoundIdResultsSummaryView.h/.cpp`

### 8.1 Session Plan Queue (SoundIdSuiteList)
| Elemento | Función |
|----------|---------|
| **Queue Items** | Tests en cola con: título, stimulus type (LogFarinaSweep, SineWave1kHz, AmplitudeRamp, SyncPulses3, Silence), burst duration, capture mode, controles (steps por parámetro), total points |
| **RUN / SKIP Toggle** | Por test: ejecuta / salta |
| **Expand/Collapse** | Ver/ocultar puntos individuales del test |
| **Point Status** | 🟢 Queued / 🔵 Running / ✅ Measured / ⚠️ Invalidated / ❌ Annulled |
| **Re-Measure Button** | `btnRerunSelected` → `onRerunSelectedClicked(points)` → `startTargetedPatchSession` |
| **Point Range Selection** | Click+drag / Shift+click → selección múltiple puntos → Re-run / Annul / Delete |
| **Add Standard Test** | `btnAddStandard` → auto-puebla receta estándar del target seleccionado |
| **Add Custom Test** | `btnAddCustom` → `TestConfigModal` manual |
| **Edit Test** | `onEditTestClicked` → `TestConfigModal` |
| **Delete Test** | `onRequestDeleteTest` → confirmación |
| **Compact View** | `btnViewMode` → vista condensada |
| **Clear All** | `btnClear` → limpia cola completa |

### 8.2 Profiling Run View (SoundIdProfilingRunView)
| Elemento | Función |
|----------|---------|
| **RUN SESSION TESTS** | `btnRunSession` → `startProfilingSession()` → `SessionExecutionCoordinator::triggerStartSession` |
| **STOP** | `triggerStopSession()` → `SessionExecutionCoordinator::triggerStopSession` + `allNotesOff` |
| **Manual Step Confirm** | `confirmManualStep()` → `sessionCoordinator.confirmOperatorStep()` (para hardware analógico) |
| **Step Back / Repeat Step** | `stepBack()` / `repeatCurrentStep()` → navegación pasos operador |
| **Pause/Resume** | `pauseSession()` / `resumeSession()` → `togglePauseSession()` |
| **Live Progress** | Test actual, punto actual/total, stimulus, progress bar, tiempo estimado |
| **Pre-Scan Display** | `PreScanSpectrumAnalyzer` → curva FR preliminar antes de medición completa |

### 8.3 Session Execution Coordinator (Motor de Estado)
| Estado | Descripción |
|--------|-------------|
| `Idle` / `Starting` / `Running` / `Paused` / `Capturing` / `CancelRequested` / `Aborted` / `Completed` / `Failed` | Máquina de estados sesión |
| `WorkspaceInteractionMode` | `Guided` (SoundID) / `Classic` (Lab Bench) / `FreeCapture` |
| **Callbacks** | `onExecutionStateChanged`, `onSessionStateChanged`, `onCoordinatorStateChanged`, `onSessionAutoSaveRequested`, `onExecutionErrorTriggered` |
| **Free Capture** | `triggerFreeCapture()` captura ad-hoc sin receta |
| **Patch Session** | `startTargetedPatchSession(pointsToPatch)` re-mide puntos específicos |

---

## 9. EXPORT REPORT PANEL (Paso 4)
**Archivos**: `ExportReportPanel.h/.cpp`, `SessionReportManager.h/.cpp`, `CertificationReportExporter.h/.cpp`, `NamDatasetExporter.h/.cpp`

| Elemento | Función |
|----------|---------|
| **Metrics Preview** | `updateMetrics(avgSnrDb, noiseFloorDb, avgThdPercent, validPointCount, totalDurationSec)` muestra resumen |
| **Export Production Package** | `onExportRequested` → `requestExportProductionPackage()` → `CertificationReportExporter::exportProductionPackage()` → `.abdpack` firmado |
| **Open Certification HTML** | `onViewHtmlRequested` → `openCertificationReportHtml()` → lanza browser con `report_preview.html` |
| **Open Export Folder** | `onOpenFolderRequested` → `openExportFolderInExplorer()` |
| **Publish to Cloud** | `onPublishCloudRequested` → `publishCertificationToCloud()` → `exportProductionPackage()` + upload |
| **A/B Audition Toggle** | `onAuditionToggled(active)` → `prepareAuditionLut()` + `audioEngine.enableAuditionMode()` |
| **Audition Params** | `onAuditionParamsChanged(p1,p2)` → `audioEngine.setAuditionParameters()` (Cutoff, Resonance) |
| **Audition Waveform** | `onAuditionWaveformChanged(wf)` → `audioEngine.setAuditionWaveform()` (Sine, Saw, Pulse, Noise) |
| **Verify A/B** | `onVerifyAbRequested` → `openAudioABVerificationModal()` → modal comparación Target vs Modelo vs Residuo |

---

## 10. VENTANAS FLOTANTES AUXILIARES
**Archivos**: `MeasurementFloatingWindow.h/.cpp`, `MeasurementViewerPanel.h/.cpp`, `MeasurementComparisonPanel.h/.cpp`, `ScopeWebFloatingWindow.h`, `MidiKeyboardFloatingWindow`, `AboutModalDialog`, `OperatorStepModalDialog`, `ConfirmationModalDialog`

| Ventana | Trigger | Contenido |
|---------|---------|-----------|
| **Scope WebView2** | `toggleScopeWebWindow()` / Header Scope Button | `ScopeWebFloatingWindow` → WebView2 embebido con scope ABDShared (entrada audio, senoidal, XY, FFT) |
| **Virtual Keyboard** | `toggleVirtualKeyboardWindow()` / Header Keyboard Button | `MidiKeyboardFloatingWindow` → teclado MIDI virtual, envía MIDI a `audioEngine.postLiveMidiMessage()` |
| **Measurement Viewer** | `openMeasurementViewerWindow()` / File Menu | `MeasurementViewerPanel` → carga contenedor `.abdmeas` → curva temporal, curva frecuencia, metric cards, audio player (Captured/Stimulus/IR), verificación manifiesto FAIR, abre HTML report |
| **Measurement Comparison** | `openMeasurementComparisonWindow()` / File Menu | `MeasurementComparisonPanel` → comparador multivariante FAIR/LNL entre contenedores |
| **About Dialog** | `showAboutDialog()` / Setup Tab / Help Menu | `SoundIdSplashWindow` → splash art, versión, build, copyright, auto-updater status, check updates |
| **Operator Step Modal** | `confirmManualStep()` / hardware analógico | `OperatorStepModalDialog` → instrucciones operador, confirmar paso manual |
| **Confirmation Modal** | Múltiples (guardar perfil, overwrite, exit, etc.) | `ConfirmationModalDialog` → diálogo genérico sí/no/terciario con callback |
| **Audio AB Verification** | `openAudioABVerificationModal()` / Export Panel | `AudioABVerificationModal` → reproduce Target Real / Modelo / Residuo e[n] con transport controls |

---

## 11. PLUGIN HOSTING VST3 (PluginUiCoordinator)
**Archivos**: `PluginUiCoordinator.h/.cpp`, `PluginHostManager.h/.cpp`, `PluginWindowController.h/.cpp`, `PluginScanDirectoriesModal.h/.cpp`, `ExternalPluginFixture.h/.cpp`, `PluginWorkerMain.cpp` (out-of-process)

| Elemento | Archivos | Función |
|----------|----------|---------|
| **Load Plugin from File** | `catalogSelector.onLoadPluginFromFileRequested` → `FileChooser` → `PluginUiCoordinator::loadPlugin()` | Carga `.vst3`/.dll en `PluginHostManager` (out-of-process via `PluginWorkerMain.exe` IPC) |
| **Plugin Scan Modal** | `pluginScanModal.showModal()` | Escaneo directorios VST3 con progreso, cache persistente |
| **Plugin Identity** | `updatePluginIdentity()` | Badge título, bus description, imagen, categoría; sincroniza `ProfilingSessionController` |
| **Plugin GUI Window** | `PluginWindowController::showEditor()` | Ventana nativa HWND del editor VST3 (fuera de WebView2) |
| **Unload Plugin** | `pluginUiCoordinator.unloadPlugin()` | Libera instancia, reset estado |
| **Standard Test Available** | `suiteList.setStandardTestAvailable(true)` | Habilita "Add Standard Test" para plugin virtual |
| **IPC Worker** | `PluginWorkerMain.exe` (separate process) | Aislamiento crashes VST3; protocolo `IpcProtocol.h` Slice 2/3 |

---

## 12. MOTOR DE AUDIO Y TELEMETRÍA
**Archivos**: `LabAudioEngine.h/.cpp`, `LabStimulusGenerator.h/.cpp`, `ScopeCollector`, `ScopeWebFloatingWindow`

| Componente | Función |
|------------|---------|
| **AudioDeviceManager** | JUCE device management (ASIO/WASAPI), device selector, change listener |
| **Stimulus Generator** | `LabStimulusGenerator` → Farina log sweep, sine waves, amplitude ramps, sync pulses, silence, multitone |
| **Scope Collector** | Captura buffers audio entrada/salida → `ScopeWebFloatingWindow` para visualización tiempo real |
| **Audition Mode** | `enableAuditionMode()` + LUT grid → reproduce modelo emulado con parámetros ajustables |
| **MIDI Passthrough** | `postLiveMidiMessage()` → virtual keyboard / hardware MIDI out |
| **Auto-Trim** | `setInputAutoTrim(gain)` → aplica ganancia calibración loopback |
| **Mock Hardware** | `setMockHardware()` → simula hardware para testing sin dispositivo físico |

---

## 13. PERSISTENCIA Y DIRECTORIOS
**Archivos**: `SessionManager.h/.cpp`, `SessionSerializer.h/.cpp`, `SessionIoController.h/.cpp`, `LabDataDirectories.h/.cpp`, `LoopbackProfileStore.h/.cpp`, `CalibrationProfileStore.h/.cpp`

| Store | Función |
|-------|---------|
| **SessionManager** | Sesión activa en memoria: manifest + measured points + dirty flag + auto-save |
| **SessionSerializer** | Serialización JSON `.abdlabtest` con checksums, `MeasuredPoint` (IR, FR, THD, IMD, env, mod) |
| **SessionIoController** | Orquesta file pickers, save/load, new session, delete test, publish cloud |
| **LabDataDirectories** | Rutas deterministas: `dataRoot/exports`, `reports`, `experiments`, `presets`, `loopbackProfiles`, `contracts` |
| **LoopbackProfileStore** | Perfiles `.abdloop` (binario + JSON sidecar) con match evaluation |
| **CalibrationProfileStore** | Registros calibración con compatibilidad device+SR |

---

## 14. ANÁLISIS METROLÓGICO (DSP Core - Solo C++, sin UI)
**Archivos**: `math/FarinaDeconvolver.h/.cpp`, `math/WienerHammersteinFitter.h/.cpp`, `math/IntermodulationAnalyzer.h/.cpp`, `measurement/AnalogDutCharacterizer.h/.cpp`, `measurement/ComplexEnvelopeOrchestrator.h/.cpp`, `measurement/ObservableComparisonEngine.h/.cpp`

| Algoritmo | Función |
|-----------|---------|
| **Farina Deconvolution** | Respuesta impulso + FR desde sweep logarítmico (Golay complementary sequences) |
| **Wiener-Hammerstein** | Identificación sistemas no lineales bloque-orientados (LUT 2D SIMD) |
| **THD Paramétrico** | IEEE vs IEC, armónicos 2-10, ventana Hann, FFT 4096/8192 |
| **IMD 2-Tone** | SMPTE / CCIF / DIN / ITU-R, productos 2º/3er orden |
| **Level Sweep** | Thresholds 1% THD, 3% THD, P1dB compression |
| **Latencia Sub-Muestra** | Correlación Golay + interpolación parabólica |
| **Envelope Analysis** | ADSR extraction, curvas ataque/decay/sustain/release |
| **Modulation Matrix** | Estimación rutas modulación (source→dest→amount) |
| **FAIR/LNL Comparison** | Comparación nativo vs observable con holdout reservado |

---

## 15. HARDWARE CONTROL (MIDI/SysEx/NRPN - Solo C++)
**Archivos**: `hardware/HardwareController.h`, `MidiCcController.h/.cpp`, `AiraSysExController.h/.cpp`, `CasioCzVirtualController.h/.cpp`, `JunoTapeModem.h`, `FskAudioModem.h/.cpp`, `NRPNParser.h`, `MidiDeviceHotplugMonitor.h`, `AudioLabMidiBackend.h/.cpp`

| Controlador | Hardware Soportado | Protocolo |
|-------------|-------------------|-----------|
| **MidiCcController** | Genérico CC | MIDI CC bidireccional |
| **AiraSysExController** | Roland AIRA (Bitrazer, Demora, Torcido, Scooper) | SysEx Roland AIRA |
| **CasioCzVirtualController** | Casio CZ-101/1000/5000/1 | SysEx Casio (Nibble encoding) |
| **JunoTapeModem** | Roland Juno-106 | FSK "tape" modem via audio |
| **FskAudioModem** | Genérico | FSK audio para automatización |
| **NRPNParser** | Sintetizadores NRPN | Non-Registered Parameter Numbers |
| **MidiDeviceHotplugMonitor** | Todos | Detección hotplug MIDI I/O |
| **AudioLabMidiBackend** | Unificado | Abstracción backend MIDI JUCE |

---

## 16. GRÁFICOS Y VISUALIZADORES NATIVOS
**Archivos**: `SoundIdCurvePlotter.h/.cpp`, `PlotterFrequencyCurveRenderer.h/.cpp`, `PlotterHeatmapRenderer.h/.cpp`, `PlotterModulationTableRenderer.h/.cpp`, `SoundIdMeterStrip.h/.cpp`, `Waterfall3DComponent.h/.cpp`, `MatrixResolutionTableComponent.h/.cpp`, `RhythmicMetronomeComponent.h`

| Visualizador | Función |
|--------------|---------|
| **SoundIdCurvePlotter** | Plot principal: FR overlay target vs modelo, residuo e[n], THD/IMD vs freq, envelope ADSR |
| **Frequency Curve Renderer** | Renderizado curvas FR con zoom/pan, marcadores, leyenda |
| **Heatmap Renderer** | Heatmaps 2D: Freq vs Level (THD), Freq vs Time (waterfall) |
| **Modulation Table Renderer** | Tabla matriz modulación (source × dest × amount) |
| **SoundIdMeterStrip** | VU meters estéreo IN/OUT con clip LEDs, dBFS scale |
| **Waterfall 3D** | Espectrograma 3D tiempo-frecuencia-nivel (OpenGL) |
| **Matrix Resolution Table** | Tabla resolución puntos medidos por parámetro |
| **Rhythmic Metronome** | Click visual/audio para sincronización operador |

---

## 17. MODALES Y DIÁLOGOS ESPECIALIZADOS
**Archivos**: `TestConfigModal.h/.cpp`, `AboutModalDialog.h/.cpp`, `OperatorStepModalDialog.h/.cpp`, `ConfirmationModalDialog.h/.cpp`, `AudioABVerificationModal.h/.cpp`, `PluginScanDirectoriesModal.h/.cpp`, `SoundIdSplashScreen.h`

| Modal | Trigger | Función |
|-------|---------|---------|
| **Test Config Modal** | `btnAddCustom` / `onEditTestClicked` | Configuración test custom: stimulus, burst, capture mode, controles pasos |
| **About Dialog** | `showAboutDialog()` / Help Menu | Splash art, versión, build, copyright, auto-updater status |
| **Operator Step Modal** | Hardware analógico paso manual | Instrucciones operador, botón "Confirm Step" |
| **Confirmation Modal** | Save profile, overwrite, exit, delete | Diálogo genérico 2-3 botones con callback resultado |
| **Audio AB Verification** | Export Panel "Verify A/B" | Reproduce Target Real / Modelo / Residuo e[n] con transport controls |
| **Plugin Scan Modal** | Header "Scan Plugin Directories" | Gestión directorios VST3, escaneo progreso, cache |
| **Splash Screen** | Arranque app | Logo animado, progreso inicialización |

---

## 18. TEMA Y ESTILOS (SoundIdTheme)
**Archivos**: `SoundIdTheme.h/.cpp`, `AppTheme.h/.cpp`, `SoundIdThemeMode.cpp`

| Función | Detalle |
|---------|---------|
| **Theme Modes** | `Dark (audiolab)` / `Light (audiolab-light)` - tokens CSS-compatibles |
| **applyThemeMode()** | Aplica colores, fuentes, bordes, sombras a todos componentes registrados |
| **updateTheme()** | Propagación recursiva a ventanas flotantes, scope, keyboard, plotter, panels |
| **Color Tokens** | `--color-bg-base`, `--color-panel-bg`, `--color-accent` (verde #00e676 dark / #007a4d light), `--color-warning`, `--color-danger`, `--color-success` |

---

## 19. EQUIVALENCIA Y CORRESPONDENCIA CON LA ARQUITECTURA WEBVIEW2 (WebUI)

ABDAudioLab cuenta con un frontend moderno desacoplado basado en **WebView2 / HTML5 / CSS / Vanilla JS** servido desde `WebUI/` e integrado en C++ mediante `AudioLabWebComponent`:

| Subsistema C++ | Componente WebUI Equivalente | Conexión IPC / Responsabilidad |
|----------------|------------------------------|--------------------------------|
| **MainHeaderController** | `SynthNavBar` (`WebUI/src/app.js`) | Menú File, submenu unificado "Open Folder...", Tools, Help, Theme Toggle y Campana de avisos (`navBellBtn`) |
| **SoundIdSidebarStepper** | Stepper reactivo de 4 pasos (`WebUI/src/app.js`) | Pasos 1 a 4 con estados dinámicos, summary card persistente y colapsado lateral |
| **SetupInfoTab + Topology** | Stage 1 (`stage1_target.js`) | Consolida topología interactiva (`StudioTopology`), HUD de audio/MIDI activo y pinout cerrado |
| **Hardware Catalog & Target** | Stage 1 (`stage1_target.js`) | Selectores en cascada, auto-detección MIDI/USB y hosting VST3 con GUI flotante nativa |
| **NativeCalibrationPanel** | Stage 2 (`stage2_calibration.js`) | Modales dedicadas independientes para 2A (ruido) y 2B (Farina), vúmetro reactivo, bypass VST3 y hardware images |
| **SoundIdSuiteList / Profiling** | Stage 3 (`stage3_acquisition.js`) | Disparo y seguimiento reactivo de adquisición vía `acquisitionProgress` |
| **ExportReportPanel** | Stage 4 (`stage4_export.js`) | Visualización de curvas diezmadas, hash de integridad, audición A/B y exportación de paquetes C++20 |
| **SoundIdMeterStrip** | Sidebar derecho (`WebUI/src/app.js`) | Componente universal `VuMeter` de ABDShared, pico en dBFS y chip de estado dinámico |
| **ScopeWebFloatingWindow** | Ventana flotante ABDScope | Conectada vía IPC `toggleScopeWindow` con sincronización de tema en tiempo real |
| **SessionPersistenceService** | `SessionSerializer` + `SessionIoController` | Diálogos nativos de archivo disparados por IPC `saveSession`, `loadSession`, `newSession` |

---

## Resumen de Archivos Principales por Categoría

| Categoría | Archivos Clave |
|-----------|----------------|
| **Main Window & Layout** | `MainContentComponent.h/.cpp` + 7 splits (`*Hardware.cpp`, `*ProfilingSession.cpp`, `*Report.cpp`, `*Persistence.cpp`, `*PluginUiHost.cpp`, `*WorkflowMode.cpp`, `*UiGovernance.cpp`) |
| **WebView2 Host & Bridge** | `AudioLabWebComponent.h/.cpp`, `LabWebApplication.h`, `main_web.cpp` |
| **WebUI Frontend** | `WebUI/src/app.js`, `WebUI/src/stages/stage1_target.js` a `stage4_export.js` |
| **Header & Navigation** | `MainHeaderController.h/.cpp`, `SoundIdSidebarStepper.h/.cpp`, `WorkflowNavigationController.h/.cpp` |
| **Hardware & Target** | `SoundIdHardwareCatalogSelector.h/.cpp`, `SoundIdTargetView.h/.cpp`, `SlideInDrawer.h/.cpp`, `DrawerSetupTab.h/.cpp` |
| **Calibración Nativa** | `NativeCalibrationPanel.h/.cpp` + 5 painters/workflow/profile/layout/uisate |
| **Profiling Session** | `SoundIdSuiteList.h/.cpp`, `SessionExecutionCoordinator.h/.cpp`, `ProfilingSequencer.h/.cpp`, `SoundIdProfilingRunView.h/.cpp` |
| **Export & Reports** | `ExportReportPanel.h/.cpp`, `SessionReportManager.h/.cpp`, `CertificationReportExporter.h/.cpp` |
| **Plugin Hosting** | `PluginUiCoordinator.h/.cpp`, `PluginHostManager.h/.cpp`, `PluginWindowController.h/.cpp`, `PluginWorkerMain.cpp` |
| **Ventanas Flotantes** | `MeasurementFloatingWindow.h/.cpp`, `MeasurementViewerPanel.h/.cpp`, `MeasurementComparisonPanel.h/.cpp`, `ScopeWebFloatingWindow.h` |
| **Audio Engine** | `LabAudioEngine.h/.cpp`, `LabStimulusGenerator.h/.cpp` |
| **DSP/Metrología** | `math/*.h/.cpp`, `measurement/AnalogDutCharacterizer.h/.cpp`, `ComplexEnvelopeOrchestrator.h/.cpp` |
| **Hardware Control** | `hardware/*.h/.cpp` (MIDI CC, SysEx, NRPN, FSK, Hotplug) |
| **Persistencia** | `SessionManager.h/.cpp`, `SessionSerializer.h/.cpp`, `LoopbackProfileStore.h/.cpp` |
| **Visualizadores** | `SoundIdCurvePlotter.h/.cpp`, `Plotter*.h/.cpp`, `SoundIdMeterStrip.h/.cpp`, `Waterfall3DComponent.h/.cpp` |

---

**Total estimado**: ~180 archivos C++ de GUI + ~120 archivos core/DSP/hardware + frontend WebUI = **~300 archivos C++ y suite moderna WebView2**.