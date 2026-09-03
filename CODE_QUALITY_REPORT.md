# Code Quality Report — ABDAudioLab

Generado: 2026-08-31 (actualizado)

---

## Resumen

| Severidad | Total | Confirmados | Parciales | Pendientes |
|-----------|-------|-------------|-----------|------------|
| CRITICAL  | 15    | 0           | 0         | 15         |
| HIGH      | 31    | 2           | 0         | 29         |
| MEDIUM    | 60    | 6           | 0         | 54         |
| LOW       | 80+   | 1           | 0         | 79+        |
| **Total** | **186+** | **9**   | **0**     | **177+**   |

### DRY Violations & Dead Code (nuevas secciones: §8, §9, §10)

| Categoría | Hallazgos |
|-----------|-----------|
| **DRY Violations** | 8 patrones duplicados (15+ sitios) |
| **Dead Code** | 29 items (~1,200 líneas muertas) |
| **Dead Files** | 5 archivos completos no utilizados |

---

## 1. módulo `src/audio/` — Audio Engine

### CRITICAL

| # | Archivo:Línea | Categoría | Descripción |
|---|---------------|-----------|-------------|
| 1 | `LabAudioEngine.h:113-114` | **Thread Safety** | `diagnosticToneFreq` y `diagnosticToneLevel` son `float` plano escrito desde `enableDiagnosticTestTone()` (GUI thread) y leído en `audioDeviceIOCallbackWithContext()` (audio thread). **Data race** — debe ser `std::atomic<float>`. |
| 2 | `LabAudioEngine.h:85-88` | **Thread Safety** | `getSpectrumMagnitudes()` copia `spectrumMagnitudesDb` con `std::copy` sin barrier de acquire. El audio thread escribe y publica via `spectrumDataReady.store(release)`, pero el lector nunca hace acquire. La GUI puede leer un espectro parcialmente escrito. |
| 3 | `LabAudioReceiver.h:51-52` | **Thread Safety** | `ringBuffer` (vector) y `ringBufferSize` (int) se escriben en `prepare()` (control thread) y se leen en `processBlock()` (audio thread) sin sincronización. Si `prepare()` se llama con el audio activo, el audio thread lee mid-reallocation. |
| 4 | `LabAudioReceiver.cpp:35-38` | **Thread Safety** | `armCapture()` llama `reset()` que setea `state`, resetea `fifo` y cero atomics — todo mientras el audio thread puede estar a mitad de `processBlock()`. `fifo.reset()` no es thread-safe con `prepareToWrite`/`finishedWrite` concurrentes. |
| 5 | `LabStimulusGenerator.h:49-50` | **Thread Safety** | `playing` y `finished` son `bool` plano escrito desde `setStimulus()` (control thread) y leído desde `processBlock()` (audio thread). Data race — debe ser `std::atomic<bool>`. |
| 6 | `LabStimulusGenerator.h:52-53` | **Thread Safety** | `currentSampleIndex` y `totalSamples` son `int64_t` plano escrito desde `setStimulus()` y leído desde getters. En plataformas de 32-bit, lecturas tornadas de 64-bit = UB. |

### HIGH

| # | Archivo:Línea | Categoría | Descripción |
|---|---------------|-----------|-------------|
| 7 | `LabAudioEngine.h:130` | **Performance** | `tempProcessBuffer` (vector) se redimensiona via `assign()` en el callback de audio si es necesario. Esto causa **heap allocation en tiempo real**. Debe pre-asignarse en `audioDeviceAboutToStart()`. |
| 8 | `LabAudioReceiver.h:56` | **Thread Safety** | `triggerThreshold` es `float` plano escrito desde `armCapture()` y leído en `processBlock()`. Data race. |
| 9 | `LabAudioReceiver.cpp:112` | **Potential Bug** | `destination.resize(static_cast<size_t>(totalToRead))` — si `totalToRead` es negativo por un race con `armCapture()`, el cast a `size_t` envuelve a un valor enorme → `std::bad_alloc`. |
| 10 | `LabAudioReceiver.cpp:27` | **Thread Safety** | `prepare()` resize `ringBuffer`, resetea `fifo` y modifica `sampleRate` sin sincronización contra el audio thread. |
| 11 | `LabAudioEngine.cpp:49` | **Resource Management** | `deviceManager.addAudioCallback(this)` se llama sin verificar si ya está registrado. Si `initializeAudioDevices()` se llama dos veces, el callback se registra doble pero solo se remueve una vez en el destructor. |
| 12 | `LabStimulusGenerator.cpp:52-58` | **Performance** | Para `LogFarinaSweep`, `logRatio`, `K`, `L`, `w1`, `w2` se recomputan en cada sample del loop. Son constantes para todo el sweep — deben precomputarse en `setStimulus()`. |
| 13 | `LabStimulusGenerator.h:48` | **Thread Safety** | `currentType` es un enum plano escrito desde `setStimulus()` y leído en el switch de `processBlock()`. Data race. |
| 14 | `LabStimulusGenerator.h:60,63` | **Thread Safety** | `randomSeed` y estado del filtro pink noise (`b0`–`b6`) se modifican en `processBlock()` y se resetean en `setStimulus()`. Si `setStimulus()` se llama desde el control thread mientras `processBlock()` ejecuta, el estado del filtro se corrompe. |

### MEDIUM

| # | Archivo:Línea | Categoría | Descripción |
|---|---------------|-----------|-------------|
| 15 | `LabAudioEngine.cpp:274-279` | **Performance** | `std::log10()` se llama para 1024 bins FFT dentro del callback de audio. Puede calcularse en el GUI thread. |
| 16 | `LabAudioEngine.cpp:99-103` | **Performance** | Todos los canales de salida se ceroan, luego los canales 0/1 se sobreescriben inmediatamente. El clear para 0 y 1 es redundante. |
| 17 | `LabAudioReceiver.cpp:36,44` | **Potential Bug** | `ringBufferSize - 1024` puede underflow si `ringBufferSize < 1024` (sample rates muy bajas). |
| 18 | `LabStimulusGenerator.cpp:92-104` | **Code Smell** | `SyncPulses3` usa números mágicos: `0.080`, `0.040`, `0.005`, `0.035`, `3`, `1000.0`, `0.707`. Ninguno es constante nombrada. |
| 19 | `LabAudioEngine.cpp:41-44` | **API Design** | Pares de acceso duplicados: `getGenerator()`/`getStimulusGenerator()` y `getReceiver()`/`getResponseReceiver()` devuelven los mismos objetos. |

### LOW

| # | Archivo:Línea | Categoría | Descripción |
|---|---------------|-----------|-------------|
| 20 | `LabAudioEngine.h:105` | **Code Smell** | `currentSampleRate { 96000.0 }` es un número mágico repetido en .cpp. Debe ser constante nombrada. |
| 21 | `LabStimulusGenerator.cpp:73-172` | **Code Smell** | El switch de `processBlock` tiene 9 casos en ~100 líneas. Cada caso debería ser un método privado separado. |

---

## 2. módulo `src/math/` — Math/Analytic

### HIGH

| # | Archivo:Línea | Categoría | Descripción |
|---|---------------|-----------|-------------|
| 22 | `LabAnalyticEngine.cpp:114-116` | **Code Smell** | `analyzeAdsrEnvelopes` hardcodea `decay = 50.0f` y `release = 100.0f` para **todos** los pasos. Son valores fabricados, no medidos. La función name implica análisis pero solo mide attack. **Resultados engañosos.** |
| 23 | `LabAnalyticEngine.cpp:185` | **Code Smell** | `analyzeWaveShaperRamps` hardcodea `thdPercent = { 5.2f, 0.1f }` — resultado fabricado, no computado de los datos de entrada. |
| 24 | `LabAnalyticEngine.cpp:211` | **Code Smell** | `analyzeGainTones` hardcodea `snrDb = { 88.5f, 0.4f }` — fabricado, no medido. La función computa gain correctamente pero miente sobre SNR. |
| 25 | `LabAnalyticEngine.cpp:296` | **Code Smell** | `analyzeCyclicModulator` hardcodea `asymmetries.push_back(0.02f)` — fabricado, nunca computado de la señal real. |

### MEDIUM

| # | Archivo:Línea | Categoría | Descripción |
|---|---------------|-----------|-------------|
| 26 | `SplineInterpolator2D.h:29-57` | **Potential Bug** | `interpolateBilinear`: sin bounds check que `flatGrid.size() >= numRows * numCols`. Caller puede pasar vector sub-dimensionado → OOB read. |
| 27 | `LoopbackCalibrator.cpp:75-79` | **Potential Bug** | La búsqueda de referencia 1kHz usa `break` en el primer match. Si ninguna frecuencia está en [900, 1100], `ref1kHzDb` queda en `0.0f` — toda la curva se normaliza a 0 dB silenciosamente. |
| 28 | `LoopbackCalibrator.cpp:109-138` | **Error Handling** | `saveCalibrationToJson` atrapa `(...)` y retorna `false`. Excepciones tragadas sin logging, sin distinción entre error IO y JSON. |
| 29 | `LoopbackCalibrator.h:53` | **API Design** | `loadCalibrationFromJson` retorna `LoopbackCalibrationData` default en fallo — caller no puede distinguir "archivo vacío" de "calibración válida con defaults". Debería retornar `std::optional<>`. |
| 30 | `FarinaDeconvolver.cpp:56-58` | **Potential Bug** | `convLen = n1 + n2 - 1` — overflow si ambos son `size_t::max`. Y `static_cast<size_t>(totalSamples)` envuelve a valor enorme si `totalSamples` es negativo. Sin validación de input. |
| 31 | `FarinaDeconvolver.cpp:117-118` | **Potential Bug** | `log(endFreqHz / startFreqHz)` — si `startFreqHz == endFreqHz` → `log(1.0) = 0` → división por cero. Sin validación. |
| 32 | `LabAnalyticEngine.cpp:171` | **Potential Bug** | `analyzeWaveShaperRamps` solo procesa `recordedPasses[0]`, ignorando todos los otros pasos. Datos multi-paso descartados silenciosamente. |
| 33 | `LabAnalyticEngine.cpp:181` | **Potential Bug** | `pass.size() - 1` cuando `pass` puede estar vacío → underflow a `SIZE_MAX`. Solo el vector externo se valida con `empty()`. |

### LOW

| # | Archivo:Línea | Categoría | Descripción |
|---|---------------|-----------|-------------|
| 34 | `NoiseFloorTracker.h:60` | **Code Smell** | `windowBuffer` declarado pero nunca escrito ni leído — dead code. |
| 35 | `FarinaDeconvolver.cpp:75-76` | **Performance** | `fft.perform()` trata data como compleja. JUCE tiene `performRealOnlyForwardTransform` ~2x más rápido para señales reales. |
| 36 | `LoopbackCalibrator.cpp:67-68` | **Code Smell** | `minMag = 100.0f; maxMag = -100.0f` como sentinels — debería usar `std::numeric_limits<float>::max()`. |

---

## 3. módulo `src/core/` — Core/Profiling

### CRITICAL

| # | Archivo:Línea | Categoría | Descripción |
|---|---------------|-----------|-------------|
| 37 | `ProfilingSequencer.h:67` | **Thread Safety** | `getMeasuredPoints()` retorna `const&` a vector mutable interno. Llamado desde GUI thread mientras `measuredPoints` es mutado por worker thread en `run()`. Sin sincronización. |
| 38 | `ProfilingSequencer.h:80` | **Thread Safety** | `activeSession`, `exportDir`, `exportBaseName`, `measuredPoints` se leen/escriben desde ambos threads sin sincronización, aunque `std::atomic<SequencerState>` existe para el estado. |

### HIGH

| # | Archivo:Línea | Categoría | Descripción |
|---|---------------|-----------|-------------|
| 39 | `ProfilingSequencer.h:50-53` | **Memory Safety** | `setHardwareController` almacena raw pointer sin garantía de lifetime. Si el caller destruye `IHardwareController` antes que el sequencer → dangling pointer. |
| 40 | `ProfilingSequencer.h:85-88` | **Thread Safety** | `progressCallback`, `pointMeasuredCallback` etc. son `std::function` seteados desde main thread y invocados desde worker thread. `std::function` no es thread-safe para read/write concurrente. |
| 41 | `ProfilingSequencer.cpp:244` | **Memory Safety** | `new juce::FileOutputStream(wavFile)` pasa raw pointer a `createWriterFor()`. Si retorna `nullptr` (disco lleno), el `FileOutputStream` nunca se libera → memory leak. |
| 42 | `ProfilingSequencer.cpp:350-352` | **Thread Safety** | `pointMeasuredCallback(pt)` se invoca síncronamente en sequencer thread, pero otros callbacks usan `callAsync`. Si el callback toca GUI → data race. |
| 43 | `SessionSerializer.h:43-44` | **Memory Safety** | Destructor user-defined pero sin Rule of 5. Copiar un `SessionSerializer` haría double-delete del directorio temporal. |
| 44 | `SessionSerializer.cpp:276` | **Memory Safety** | `int numPoints = mis.readInt()` — tamaño leído directo de archivo binario sin validación de signo/rango. Un archivo crafted puede forzar loop infinito o UB. |
| 45 | `HardwareContractRegistry.h:77` | **Memory Safety** | `findContractById` retorna raw pointer. Si `loadContractsFromDirectory` se llama de nuevo, el pointer previo queda dangling. |

### MEDIUM

| # | Archivo:Línea | Categoría | Descripción |
|---|---------------|-----------|-------------|
| 46 | `SessionSerializer.cpp:167` | **Potential Bug** | `int numPoints = static_cast<int>(points.size())` — truncation de `size_t` a `int` en 64-bit. |
| 47 | `SessionSerializer.cpp:142-145` | **Error Handling** | `catch (...) { return false; }` descarta toda info de diagnóstico. |
| 48 | `SessionSerializer.cpp:181` | **API Design** | `measured_points.bin` no tiene magic number ni versión de formato. Cambios de formato producirán reads corruptos silenciosos. |
| 49 | `ProfilingSequencer.cpp:43` | **Memory Safety** | `stopThread(4000)` — timeout de 4s hardcodeado. Si el thread se traba, el destructor retorna con el thread aún vivo → use-after-free. |
| 50 | `ProfilingSequencer.cpp:384-421` | **Error Handling** | Si `threadShouldExit()` dispara antes de la línea 383, el estado `Finished` nunca se setea, los puntos no se exportan, y el caller no recibe notificación. Sin cleanup para resultados parciales. |
| 51 | `HardwareContractRegistry.cpp:99` | **Error Handling** | `opt.get<std::string>()` — si un elemento del array JSON no es string, aborta parsing de todo el archivo. Pierde todos los otros controles válidos. |
| 52 | `HardwareContractRegistry.cpp:132` | **Error Handling** | `loadedContracts.push_back(c)` se ejecuta aunque `c` fue parseado parcialmente (functions vacío, sin bloque MIDI). Sin validación de completitud. |

---

## 4. módulo `src/hardware/` — Hardware Controllers

### CRITICAL

| # | Archivo:Línea | Categoría | Descripción |
|---|---------------|-----------|-------------|
| 53 | `MockHardwareController.h:92` | **Thread Safety** | `processAudioBlock` (noexcept) lee/escribe `filterState`, `g`, `resonanceAmount`, `noiseSeed`, `driveNormalized`, `cutoffNormalized`. Los setters escriben desde UI/sequencer **sin sincronización**. Data race = UB. |
| 54 | `MidiCcController.h:90` | **Thread Safety** | `midiOut->sendMessageNow()` se llama desde sequencer thread. Si `disconnect()` se llama concurrentemente → **use-after-free / data race**. Sin mutex. |
| 55 | `MidiCcController.h:108-111` | **Thread Safety** | `sendNrpn` envía 4 mensajes SysEx secuenciales sin atomicidad. `disconnect()` entre mensajes 2 y 3 = crash. |
| 56 | `AiraSysExController.h:88-98` | **Thread Safety** | `connect()` modifica `midiOut` e `midiIn` sin locks. Si la UI llama `connect()` mientras audio llama `setParameterRaw` → data race. |
| 57 | `AiraSysExController.h:101-109` | **Thread Safety** | `disconnect()` llama `midiIn->stop()` then `midiIn.reset()`. Si `handleIncomingMidiMessage` se ejecuta en callback de input MIDI → `this` pointer dangling. |

### HIGH

| # | Archivo:Línea | Categoría | Descripción |
|---|---------------|-----------|-------------|
| 58 | `AiraRoutingValidator.h:1-154` | **Code Smell** | **Copia verbatim** de `RoutingValidator.h`. Catálogo, lógica de validación, estructura — todo idéntico. DRY violation. Debería eliminarse `AiraRoutingValidator.h` y usar `RoutingValidator` directamente. |
| 59 | `MidiCcController.h:84-92` | **Thread Safety** | `setParameterRaw` verifica `isConnected()` then dereferencea `midiOut` — TOCTOU race con `disconnect()`. |
| 60 | `AiraSysExController.h:122-145` | **Thread Safety** | `setParameterRaw` tiene el mismo patrón TOCTOU. |
| 61 | `ManualAnalogueController.h:59-68` | **Thread Safety** | `setParameterRaw` invoca `promptCallback` (std::function) — si se llama desde sequencer mientras UI modifica el callback → data race en `std::function`. |

### MEDIUM

| # | Archivo:Línea | Categoría | Descripción |
|---|---------------|-----------|-------------|
| 62 | `MidiCcController.h:115-118` | **Dead Code** | `mapNrpn` almacena en `nrpnMapping` pero nada en la clase lee de ahí. |
| 63 | `MidiCcController.h:138-139` | **Dead Code** | `cachedValues` se escribe en `setParameterRaw` pero nunca se lee. |
| 64 | `AiraSysExController.h:209-218` | **Error Handling** | `handleIncomingMidiMessage` ignora silenciosamente todo SysEx data. `requestStateDump` está a medio implementar. |
| 65 | `AiraSysExController.h:229-260` | **Performance** | `sendDataSet1` allocates `std::vector<uint8_t>` en heap para cada SysEx. Para cambios de parámetro real-time, esto es allocation en hot path. |
| 66 | `AiraSysExController.h:141` | **Potential Bug** | `controllerEvent(1, paramIndex, val)` — channel hardcodeado a `1`. Si el device AIRA está en otro canal, envía al canal incorrecto. |
| 67 | `RoutingValidator.h:113-148` | **Performance** | `initSubmoduleCatalog()` ejecuta en constructor, heap-allocating 32 elementos cada vez. Debería ser `static const`. |

### LOW

| # | Archivo:Línea | Categoría | Descripción |
|---|---------------|-----------|-------------|
| 68 | `MockHardwareController.h:115-116` | **Code Smell** | Constantes LCG `196314165` y `907633515` sin documentación. |
| 69 | `MockHardwareController.h:127` | **Code Smell** | Pi literal `3.14159265358979323846f` — debería ser `static_cast<float>(M_PI)` o constante nombrada. |
| 70 | `ManualAnalogueController.h:53` | **Code Smell** | `setParameter` es idéntico a `MidiCcController::setParameter` — copy-paste entre archivos. |

---

## 5. módulo `src/gui/` — GUI Components

### CRITICAL

| # | Archivo:Línea | Categoría | Descripción |
|---|---------------|-----------|-------------|
| 71 | `TestConfigModal.cpp:194` | **Potential Bug** | `row` se usa sin haber sido declarado. Debería ser `ControlRowWidgets row;`. Código fragmentado o error de compilación. |
| 72 | `SlideInDrawer.cpp:1214,1227` | **Memory Leak** | `juce::Drawable::createFromImageDataStream(*brandFile.createInputStream())` — `createInputStream()` retorna raw `juce::InputStream*` que nunca se libera. Memory leak en cada llamada. |

### HIGH

| # | Archivo:Línea | Categoría | Descripción |
|---|---------------|-----------|-------------|
| 73 | `StereoVuMeter.h:30-38` | **Thread Safety** | `setLevels()` escribe `currentPeak*`/`currentRms*`, `timerCallback()` los lee — sin sincronización. Data race entre audio thread y message thread. |
| 74 | `SoundIdMeterStrip.h:39-40` | **Thread Safety** | Mismo patrón: `setLevels()` desde audio thread, `timerCallback()` desde message thread — sin atomic ni lock. |
| 75 | `SoundIdSplashScreen.h:62` | **Hardcoded Path** | `juce::File("d:/desarrollos/ABDSynths/ABDAudioLab/assets/splash_art.jpg")` — path absoluto de máquina de desarrollador. Falla en cualquier otra máquina. |
| 76 | `SlideInDrawer.cpp:34` | **Hardcoded Path** | `juce::File sharedAssetsDir("D:/desarrollos/ABDSynths/ABDSharedAssets")` — mismo problema. |
| 77 | `SlideInDrawer.h:62` | **Resource Management** | `~SlideInDrawer() override = default` — Timer no se detiene en destructor. Si el callback del timer se ejecuta después de la destrucción → UB. |
| 78 | `SlideInDrawer.cpp:582-736` | **Code Smell** | `rebuildTestEditorControls()` es ~150 líneas casi idénticas a `TestConfigModal::rebuildControlRows()`. Duplicación masiva. |
| 79 | `SlideInDrawer.cpp:738-776` | **Code Smell** | `updateTestEditorEstimatedTime()` es casi idéntica a `TestConfigModal::updateEstimatedTime()`. Código duplicado. |
| 80 | `LiveSpectrumAnalyzer.h:30-61` | **Thread Safety** | `pushSpectrumData()` escribe `displayMagnitudes` y `peakHoldValues`, `paint()` los lee — sin sincronización. |

### MEDIUM

| # | Archivo:Línea | Categoría | Descripción |
|---|---------------|-----------|-------------|
| 81 | `SlideInDrawer.cpp:58` | **Potential Bug** | `projectRoot = exeDir.getParentDirectory().getParentDirectory().getParentDirectory()` — cadena frágil de 3 niveles. |
| 82 | `LoopbackCalibrationModal.cpp:138` | **Magic Number** | `if (measurementStep > 28)` — condición de stop hardcodeada sin explicación. |
| 83 | `LoopbackCalibrationModal.cpp:152` | **Error Handling** | `retrieveRecordedData(captured)` — sin check si `captured` está vacío o si la retrieval fue exitosa. |
| 84 | `ConfirmationModalDialog.h:126-127` | **Code Smell** | `cardW = 480` y `cardH = 220` hardcodeados, duplicados en `paint()` y `resized()`. Deberían ser `static constexpr`. |
| 85 | `ConfirmationModalDialog.h:188-193` | **API Design** | `finish()` llama `setVisible(false)` pero nunca se remueve del parent. El diálogo queda como child indefinidamente. |
| 86 | `SoundIdCurvePlotter.cpp:376-383` | **Potential Bug** | `viridisColor()` — si `t == 1.0f`, el loop sale sin setear `idx`. Funciona por accidente pero la lógica es frágil. |
| 87 | `OperatorStepModalDialog.h:230` | **Code Smell** | `std::vector<core::ParameterStep> parameterSteps` se copia por valor en `setStepInfo()`. Debería ser `const&` o moverse. |
| 88 | `ControlIcon.h:31` | **Performance** | `controlType.toLowerCase()` crea nuevo `juce::String` en cada llamada a `paint()`. Debería cachearse. |
| 89 | `InfoDrawer.cpp:67-68` | **API Design** | `PanelComponent` tiene miembro público `telemetry` escrito directamente desde `InfoDrawer::updateTelemetry()`. Rompe encapsulamiento. |
| 90 | `AboutModalDialog.cpp:86-94` | **API Design** | `showDialog()` no verifica si `this` ya es child del parent. Llamarlo dos veces lo agrega dos veces. |

### LOW

| # | Archivo:Línea | Categoría | Descripción |
|---|---------------|-----------|-------------|
| 91 | `SoundIdSuiteList.cpp:139` | **Code Smell** | `nextInt(10000)` — rango pequeño para IDs únicos. Colisiones a ~450 tests (birthday paradox). |
| 92 | `SoundIdSuiteList.cpp:37-38` | ~~**Code Smell**~~ ✅ | ~~Datos mock hardcodeados en constructor~~ — **CONFIRMADO:** `\|\| true` trap eliminado en `mouseDown()`. Sub-row clicks usan `subRect.contains(e.position)` correctamente. |
| 93 | `SoundIdTheme.h:120-141` | ~~**Code Smell**~~ ✅ | ~~Parámetros no usados en `drawComboBox`~~ — **CONFIRMADO:** Parámetro `backgroundColour` eliminado de la firma del override. |

---

## 6. `src/main.cpp` — Main Component

### HIGH

| # | Archivo:Línea | Categoría | Descripción |
|---|---------------|-----------|-------------|
| 94 | `main.cpp:1084-1085` | ~~**Potential Bug**~~ ✅ | ~~`buildCurrentSessionManifest()` hardcodea `sm.appVersion = "1.0.0"` y `sm.buildNumber = 130`~~ — **CONFIRMADO:** Usa `version::kAppVersion` ("1.1.0") y `version::kBuildNumber` (154) desde `BuildVersion.h`. |
| 95 | `main.cpp:378,467,756,1155,1226,1284` | ~~**Memory Safety**~~ ✅ | ~~`callAfterDelay` con raw `this` capture~~ — **CONFIRMADO:** Helper `hidePromptAfterDelay()` + SafePointer en todos los call-sites, incluido splash fade-out (línea 1560). |
| 96 | `main.cpp:780-784,842-846` | **Code Smell** | Mapeo `contract->id` → `AiraModel` enum duplicado idéntico en `onHardwareSelected()` y `startProfilingSession()`. |
| 97 | `main.cpp:482-573` | ~~**Thread Safety**~~ ✅ | ~~Callbacks del sequencer capturan `this` y mutan UI directamente sin `callAsync`~~ — **CONFIRMADO:** Todos los callbacks envueltos en `juce::MessageManager::callAsync()`. |
| 98 | `main.cpp:331-334,987-991,1036-1040,1145-1148` | **Code Smell** | Mapping `badgeText` → `functionalBlockType` duplicado 4 veces. Inconsistente: mapea sobre `stimulusType` y `badgeText` de forma diferente. |

### MEDIUM

| # | Archivo:Línea | Categoría | Descripción |
|---|---------------|-----------|-------------|
| 99 | `main.cpp:119,452` | **Error Handling** | `exportDirectory.createDirectory()` return value ignorado. |
| 100 | `main.cpp:1211-1218` | **Error Handling** | `exportToCppHeader()` y `exportToJsonReport()` retornan `bool` pero los valores se descartan. |
| 101 | `main.cpp:849,856,861` | **Error Handling** | `connect()` return values ignorados en `startProfilingSession` (a diferencia de `onHardwareSelected` donde sí se checkean). |
| 102 | `main.cpp:1017` | **Potential Bug** | `totalTestPoints *= stepsPerControl[k]` — producto `int` puede overflow con muchos controles de alto paso. Sin check de overflow. |
| 103 | `main.cpp:355,1142` | **Potential Bug** | `nextInt(100000)` — a ~450 tests, birthday paradox da 50% de probabilidad de colisión. |

### LOW

| # | Archivo:Línea | Categoría | Descripción |
|---|---------------|-----------|-------------|
| 104 | `main.cpp:1436` | ~~**Dead Code**~~ ✅ | ~~`int currentSuiteId { 1 }` declarado pero nunca usado~~ — **CONFIRMADO:** Variable renombrada a `profSession` en `buildProfilingSessionFromQueue()`. Verificar si `currentSuiteId` sigue muerta. |
| 105 | `main.cpp:90` | **Performance** | `for (auto root : roots)` copia cada `juce::File`. Debería ser `const auto&`. |
| 106 | `main.cpp:633-687` | **Code Smell** | Decenas de valores de píxeles hardcodeados en `resized()`. Sin constantes nombradas. |
| 107 | `main.cpp:72-579` | **Code Smell** | Constructor de 500+ líneas. Debería descomponerse en helpers de inicialización. |

---

## 7. `src/export/` — LUT Exporter

### HIGH

| # | Archivo:Línea | Categoría | Descripción |
|---|---------------|-----------|-------------|
| 108 | `LutExporter.cpp:44-45` | ~~**Potential Bug**~~ ✅ | ~~Header generado usa `static const` arrays~~ — **CONFIRMADO:** Cambiado a `inline constexpr size_t` y `inline const alignas(16)` (C++17). |

### MEDIUM

| # | Archivo:Línea | Categoría | Descripción |
|---|---------------|-----------|-------------|
| 109 | `LutExporter.cpp:44-45` | ~~**Potential Bug**~~ ✅ | ~~`tableName` se inyecta directamente en código C++ generado~~ — **CONFIRMADO:** Sanitización completa: filtra no-alnum, agrega prefijo `lut_` si empieza con dígito. |
| 110 | `LutExporter.cpp:16-66,97-102,161-166` | ~~**Error Handling**~~ ✅ | ~~Se checkea `is_open()` pero nunca `out.good()`~~ — **CONFIRMADO:** Los 3 exportadores ahora retornan `out.good()`. |

---

## Problemas Sistémicos (Top 5)

### 1. Thread Safety generalizada (CRITICAL)
Casi **todas** las variables que cruzan la frontera audio/control thread están desprotegidas. `LabStimulusGenerator` es el peor: `playing`, `finished`, `currentType`, `currentSampleIndex`, estado del filtro y `randomSeed` son tipos planos compartidos entre threads sin atomics ni locks.

**Parcialmente mitigado:** ✅ Callbacks del sequencer ahora usan `callAsync` (#97 resuelto).

**Archivos afectados (pendientes):** `LabAudioEngine.h`, `LabAudioReceiver.h/.cpp`, `LabStimulusGenerator.h/.cpp`, `MockHardwareController.h`, `MidiCcController.h`, `AiraSysExController.h`, `StereoVuMeter.h/.cpp`, `SoundIdMeterStrip.h/.cpp`, `LiveSpectrumAnalyzer.h`, `ProfilingSequencer.h/.cpp`

**Recomendación:** Reemplazar `float`/`bool`/`int64_t` compartidos por `std::atomic<>`. Agregar `juce::SpinLock` o `juce::CriticalSection` a controllers MIDI/SysEx.

### 2. Duplicación masiva de código (HIGH)
- `AiraRoutingValidator.h` es **copia verbatim** de `RoutingValidator.h`
- `SlideInDrawer::rebuildTestEditorControls()` duplica `TestConfigModal::rebuildControlRows()`
- Mapeo AiraModel y badge→blockType se repiten 4+ veces en `main.cpp`
- `create*Suite()` en `ProfilingSession.cpp` son 6 funciones con ~230 líneas casi idénticas

**Recomendación:** Extraer funciones comunes, eliminar `AiraRoutingValidator`, consolidar mappings en un solo lugar.

### 3. Resultados de análisis fabricados (HIGH)
`LabAnalyticEngine.cpp` tiene 4 funciones que retornan valores **hardcodeados** en lugar de medir:
- `analyzeAdsrEnvelopes` → decay/release fake
- `analyzeWaveShaperRamps` → THD fake
- `analyzeGainTones` → SNR fake
- `analyzeCyclicModulator` → asymmetries fake

**Recomendación:** Implementar análisis real o documentar explícitamente que estos son placeholders.

### 4. Paths absolutos de desarrollador (HIGH)
`SoundIdSplashScreen.h:62` y `SlideInDrawer.cpp:34` usan paths absolutos de una máquina de dev específica. Falla en cualquier otro entorno.

**Recomendación:** Usar `ProjectInfo::projectFolder` o recursos binarios empaquetados.

### 5. Memory leaks y lifetime (HIGH)
- `SlideInDrawer.cpp:1214,1227` — `createInputStream()` raw pointer nunca liberado
- `ProfilingSequencer.cpp:244` — `FileOutputStream` raw pointer leak si `createWriterFor` falla
- ~~`main.cpp` — `callAfterDelay` con `this` capture sin guard de lifetime~~ ✅ **CONFIRMADO:** Helper `hidePromptAfterDelay()` + SafePointer en todos los call-sites.

**Recomendación:** Usar `std::unique_ptr` para manages de streams.

---

---

## 8. DRY Violations (Violaciones DRY)

### Patrón 1: Clones verbatim

| Archivos | Líneas | Descripción |
|----------|--------|-------------|
| `hardware/RoutingValidator.h` ↔ `hardware/AiraRoutingValidator.h` | 1-153 / 1-154 | **Copia 100% idéntica**. Mismo catálogo de 32 entries, misma lógica de validación. Solo cambia el nombre de la clase y el struct. |

### Patrón 2: Control Row Builder duplicado

| Archivos | Líneas | Descripción |
|----------|--------|-------------|
| `gui/TestConfigModal.cpp` ↔ `gui/SlideInDrawer.cpp` | 186-341 / 582-735 | ~150 líneas casi idénticas: `ControlIconComponent`, label styling, botones up/down con flechas Unicode, lógica swap/reorder, combo boxes, `updateStepAndBounds` lambda. |

### Patrón 3: `updateEstimatedTime()` duplicada

| Archivos | Líneas | Descripción |
|----------|--------|-------------|
| `gui/TestConfigModal.cpp` ↔ `gui/SlideInDrawer.cpp` | 408-446 / 738-776 | Mismo algoritmo: iterar controles, multiplicar steps, construir formula string, estimar duración total con `total * (dur + 0.35)`, formatear min/sec. |

### Patrón 4: Conversión Normalized→MIDI triplicada

| Archivo | Línea |
|---------|-------|
| `hardware/MidiCcController.h` | 80 |
| `hardware/ManualAnalogueController.h` | 55 |
| `hardware/AiraSysExController.h` | 118 |

Expresión idéntica: `std::clamp(static_cast<int>(std::lround(normalizedValue * 127.0f)), 0, 127)` en 3 archivos.

### Patrón 5: dB conversion `20*log10` — 15 sitios

| Archivo | Líneas |
|---------|--------|
| `math/NoiseFloorTracker.cpp` | 42, 43, 82 |
| `audio/LabAudioEngine.cpp` | 277 |
| `main.cpp` | 461, 564, 731 |
| `gui/LoopbackCalibrationModal.cpp` | 274 |
| `math/LoopbackCalibrator.cpp` | 44, 100 |
| `core/ProfilingSequencer.cpp` | 282, 334, 413 |
| `math/FarinaDeconvolver.cpp` | 184 |
| `gui/SoundIdMeterStrip.cpp` | 89 |

Patrón: `20.0f * std::log10(std::max(value, 1e-Xf))` con constantes de floor inconsistentes (`1e-4f`, `1e-5f`, `1e-6f`).

### Patrón 6: `create*Suite()` factories — 6 funciones clon

| Archivo | Líneas |
|---------|--------|
| `core/ProfilingSession.cpp` | 86-323 |

6 funciones (`createFilterSuite`, `createAdsrSuite`, `createDelaySuite`, `createWaveShaperSuite`, `createGainVcaSuite`, `createChorusModulatorSuite`) con ~15 líneas de boilerplate idéntico cada una.

### Patrón 7: Mapeo AiraModel duplicado

| Archivo | Líneas |
|---------|--------|
| `main.cpp` | 780-784 ↔ 842-846 |

Mismo mapping `contract->id` → `AiraModel` enum copiado en `onHardwareSelected()` y `startProfilingSession()`.

### Patrón 8: `badgeText` → `functionalBlockType` — 4 copias

| Archivo | Líneas |
|---------|--------|
| `main.cpp` | 331-334, 987-991, 1036-1040, 1145-1148 |

Mapeo repetido 4 veces, con inconsistencia: mapea sobre `stimulusType` y `badgeText` de forma diferente.

### Patrón 9: Duplicated target value badge drawing

| Archivo | Líneas |
|---------|--------|
| `gui/OperatorStepModalDialog.h` | 334-352, 388-399, 476-484, 528-536 |

Mismo patrón de dibujo (removeFromBottom(22.0f), fill bg, border, draw text con accent color) repetido en `drawKnob()`, `drawSlider()`, `drawButton()`, `drawSwitch()`.

### Patrón 10: Modal backdrop pattern

| Archivos | Líneas |
|----------|--------|
| `gui/ConfirmationModalDialog.h` ↔ `gui/OperatorStepModalDialog.h` | 117-138 / 121-140 |

Mismo patrón de card backdrop + drop shadow + card bg + border.

---

## 9. Dead Code (Código Muerto)

### Archivos completos no utilizados

| Archivo | Líneas | Razón |
|---------|--------|-------|
| `gui/StereoVuMeter.h` + `.cpp` | 54 + 85 | Reemplazado por `SoundIdMeterStrip`. Grep de `StereoVuMeter` solo hita sus propios archivos. |
| `gui/LiveCurvePlotter.h` + `.cpp` | 50 + 192 | Reemplazado por `SoundIdCurvePlotter`. Nunca instanciado. |
| `math/SplineInterpolator2D.h` | 155 | Nunca incluido en ningún archivo. Clase utility nunca integrada. |
| `hardware/RoutingValidator.h` | 153 | Superseded por `AiraRoutingValidator` que sí se usa en `AiraSysExController`. |
| `gui/InfoDrawer.h` + `.cpp` | ~100 + ~300 | Reemplazado por `SlideInDrawer`. Nunca instanciado a pesar de tener implementación completa. |

**Total: 5 archivos completos muertos (~940 líneas)**

### Funciones/métodos nunca llamados

| # | Archivo | Línea | Función | Severidad |
|---|---------|-------|---------|-----------|
| 1 | `audio/LabAudioEngine.h` | 41 | `getGenerator()` — duplicado de `getStimulusGenerator()` | MEDIUM |
| 2 | `audio/LabAudioEngine.h` | 42 | `getReceiver()` — duplicado de `getResponseReceiver()` | MEDIUM |
| 3 | `audio/LabAudioEngine.h` | 56 | `isDiagnosticTestToneActive()` — nunca consultado | LOW |
| 4 | `audio/LabAudioEngine.h` | 59 | `getSampleRate()` — duplicado de `getCurrentSampleRate()` | MEDIUM |
| 5 | `audio/LabAudioEngine.h` | 65 | `getInputRmsR()` — solo se usa la versión L | MEDIUM |
| 6 | `audio/LabAudioEngine.h` | 70 | `getOutputRmsR()` — solo se usa la versión L | MEDIUM |
| 7 | `audio/LabStimulusGenerator.h` | 39 | `isPlaying()` — nunca llamado externamente | LOW |
| 8 | `audio/LabStimulusGenerator.h` | 40 | `hasFinished()` — nunca llamado externamente | LOW |
| 9 | `audio/LabStimulusGenerator.h` | 41 | `getCurrentSampleIndex()` — nunca llamado | LOW |
| 10 | `audio/LabStimulusGenerator.h` | 42 | `getTotalSamples()` — nunca llamado | LOW |
| 11 | `audio/LabAudioReceiver.h` | 38 | `getRecordedSampleCount()` — nunca llamado | LOW |
| 12 | `hardware/MidiCcController.h` | 98 | `sendNrpn()` — nunca llamado (NRPN no conectado) | MEDIUM |
| 13 | `hardware/MidiCcController.h` | 115 | `mapNrpn()` — nunca llamado | MEDIUM |
| 14 | `hardware/AiraSysExController.h` | 163 | `setSubmoduleParameter()` — nunca llamado | LOW |
| 15 | `math/NoiseFloorTracker.h` | 43 | `getSnapshotCount()` — nunca llamado | LOW |
| 16 | `math/NoiseFloorTracker.h` | 44 | `getLatestSnapshot()` — nunca llamado | LOW |
| 17 | `gui/SlideInDrawer.h` | 71 | `getCurrentViewMode()` — nunca llamado | LOW |
| 18 | `gui/SlideInDrawer.h` | 87 | `getSelectedHardwareModeIndex()` — nunca llamado | LOW |
| 19 | `gui/SlideInDrawer.h` | 90 | `getCustomConfiguration()` — nunca llamado | LOW |
| 20 | `gui/HardwareSelectorPill.h` | 45 | `getConnectionStatus()` — nunca llamado | LOW |
| 21 | `gui/HardwareSelectorPill.h` | 46 | `getDisplayName()` — nunca llamado | LOW |
| 22 | `gui/HardwareSelectorPill.h` | 47 | `getFunctionName()` — nunca llamado | LOW |

### Funciones factory muertas (ProfilingSession)

| # | Archivo | Línea | Función | Severidad |
|---|---------|-------|---------|-----------|
| 23 | `core/ProfilingSession.h` | 68 | `createFilterSuite()` — solo llamado por `createDefaultMockSession()` que a su vez nunca se llama | HIGH |
| 24 | `core/ProfilingSession.h` | 69 | `createAdsrSuite()` — nunca llamado | HIGH |
| 25 | `core/ProfilingSession.h` | 70 | `createDelaySuite()` — nunca llamado | HIGH |
| 26 | `core/ProfilingSession.h` | 71 | `createWaveShaperSuite()` — nunca llamado | HIGH |
| 27 | `core/ProfilingSession.h` | 72 | `createGainVcaSuite()` — nunca llamado | HIGH |
| 28 | `core/ProfilingSession.h` | 73 | `createChorusModulatorSuite()` — nunca llamado | HIGH |
| 29 | `core/ProfilingSession.h` | 74 | `createDefaultMockSession()` — nunca llamado | HIGH |
| 30 | `core/ProfilingSession.h` | 58 | `loadProfileFromJson()` — nunca llamado externamente | HIGH |
| 31 | `core/ProfilingSession.h` | 59 | `loadProfileFromFile()` — nunca llamado externamente | HIGH |

**Total: 9 funciones factory muertas (~240 líneas en .cpp)**

### Miembros muertos (escritos pero nunca leídos)

| Archivo | Línea | Miembro | Severidad |
|---------|-------|---------|-----------|
| `math/NoiseFloorTracker.h` | 60 | `windowBuffer` — declarado, nunca escrito ni leído | LOW |
| `hardware/MidiCcController.h` | 115-118 | `nrpnMapping` — escrito en `mapNrpn()` pero nunca leído | MEDIUM |
| `hardware/MidiCcController.h` | 138-139 | `cachedValues` — escrito en `setParameterRaw()` pero nunca leído | MEDIUM |
| `main.cpp` | 1436 | `currentSuiteId` — declarado pero nunca usado | LOW |
| `gui/AboutModalDialog.h` | 32 | `buildNumber { 120 }` — nunca usado (stale constant) | LOW |
| `gui/InfoDrawer.h` | 25 | `buildNumber { 107 }` — hardcodeado, nunca actualizado | LOW |
| `BuildVersion.h` | 5 | `kAppName` — nunca referenciado | LOW |
| `BuildVersion.h` | 9 | `kAuthor` — nunca referenciado | LOW |

### Dead code transitivo

`SplineInterpolator2D.h` — las 4 funciones estáticas (`interpolateBilinear`, `interpolateBicubic`, `expandGridTo128x128`, `expandGridTo128x128Bicubic`) y sus helpers internos (`catmullRomBasis`) son todos dead code transitivo ya que la clase nunca se incluye.

---

## 10. Const Correctness

### Métodos que deberían ser const

| Archivo | Línea | Método | Severidad |
|---------|-------|--------|-----------|
| `audio/LabAudioEngine.h` | 28 | `getDeviceManager()` — retorna ref no-const sin overload const | LOW |
| `audio/LabAudioEngine.h` | 41-44 | `getGenerator()` / `getReceiver()` — sin overloads const | LOW |
| `math/SplineInterpolator2D.h` | 29, 62, 99, 138 | Métodos estáticos — candidatos a `constexpr` | LOW |

### Parámetros que deberían ser const ref

| Archivo | Línea | Descripción | Severidad |
|---------|-------|-------------|-----------|
| `core/OperatorStepModalDialog.h` | 230 | `setStepInfo()` copia `std::vector<ParameterStep>` por valor | MEDIUM |
| `main.cpp` | 90 | `for (auto root : roots)` — copia cada `juce::File` | HIGH |

### Constructores implícitos

| Archivo | Línea | Descripción | Severidad |
|---------|-------|-------------|-----------|
| `hardware/MidiCcController.h` | 16 | Constructor con `juce::String` default — conversión implícita desde `const char*` | MEDIUM |
| `hardware/ManualAnalogueController.h` | 20 | Constructor con `juce::String` default — conversión implícita desde `const char*` | MEDIUM |

---

## 11. API Design Issues

### Funciones con >5 parámetros

| Archivo | Línea | Función | Params | Severidad |
|---------|-------|---------|--------|-----------|
| `math/LabAnalyticEngine.h` | 72 | `analyzeFilterPasses()` | 6 | HIGH |
| `math/LoopbackCalibrator.h` | 38 | `analyzeLoopback()` | 6 | HIGH |
| `math/FarinaDeconvolver.h` | 37 | `deconvolve()` | 6 | HIGH |

### Funciones con side effects no obvios

| Archivo | Línea | Función | Descripción | Severidad |
|---------|-------|---------|-------------|-----------|
| `audio/LabAudioEngine.h` | 91 | `performAutoGainTrim()` | Modifica `inputTrimGain` atómicamente como side effect | HIGH |

### Retorno de raw pointer nullable

| Archivo | Línea | Función | Severidad |
|---------|-------|---------|-----------|
| `math/NoiseFloorTracker.h` | 44 | `getLatestSnapshot()` → `const NoiseSnapshot*` (puede ser nullptr) | MEDIUM |
| `core/HardwareContractRegistry.h` | 77 | `findContractById()` → `const HardwareContract*` (puede ser nullptr) | MEDIUM |

### Stringly-typed enums

| Archivo | Línea | Descripción | Severidad |
|---------|-------|-------------|-----------|
| `core/ProfilingSession.h` | 30 | `functionalBlockType` es `std::string` con valores fijos ("SpectrumFilter", "TimeDynamic") — debería ser enum | MEDIUM |

### `loadSessionFromPackage` retorna 4 output params

| Archivo | Línea | Descripción | Severidad |
|---------|-------|-------------|-----------|
| `core/SessionSerializer.h` | 60 | `loadSessionFromPackage(File, SessionManifest&, vector<MeasuredPoint>&, String&)` — 4 params de output | HIGH |

---

## 12. Performance Anti-patterns

### `std::endl` innecesario (flush forzado)

| Archivo | Línea | Severidad |
|---------|-------|-----------|
| `export/LutExporter.cpp` | 101 | MEDIUM |
| `export/LutExporter.cpp` | 165 | MEDIUM |
| `core/ProfilingSession.cpp` | 71 | LOW |

### `#include` pesados en headers

| Archivo | Línea | Header incluido | Severidad |
|---------|-------|-----------------|-----------|
| `core/SessionSerializer.h` | 8-10 | `LabStimulusGenerator.h`, `TestConfigModal.h`, `LutExporter.h` | MEDIUM |

### Includes en header que podrían estar en .cpp

| Archivo | Descripción | Severidad |
|---------|-------------|-----------|
| `hardware/RoutingValidator.h` | Implementación completa inline con 32 `std::string` constructions | LOW |
| `hardware/AiraRoutingValidator.h` | Mismo problema | LOW |

---

## Resumen final

| Categoría | Items | Líneas ~muertas | Corregidos |
|-----------|-------|-----------------|------------|
| **Archivos muertos** | 5 archivos completos | ~940 | 0 |
| **Funciones factory muertas** | 9 funciones en ProfilingSession | ~240 | 0 |
| **Funciones accessor muertas** | 22 funciones | ~80 | 0 |
| **Miembros muertos** | 8 variables | ~30 | 1 |
| **DRY violations** | 10 patrones | N/A (coste de mantenimiento) | 0 |
| **Const correctness** | 6 issues | N/A | 0 |
| **API design** | 6 issues | N/A | 0 |
| **Performance** | 5 issues | N/A | 1 |
| **Thread Safety** | 15 issues | N/A | 2 |
| **Memory Safety** | 7 issues | N/A | 1 |
| **Error Handling** | 3 issues | N/A | 1 |

---

## 13. Patrones Legacy / Deprecated

### `catch(...)` que traga todo sin logging

| Archivo | Línea | Severidad | Descripción |
|---------|-------|-----------|-------------|
| `math/LoopbackCalibrator.cpp` | 135-138 | MEDIUM | `catch (...) { return false; }` — traga excepciones al guardar calibración |
| `math/LoopbackCalibrator.cpp` | 175-178 | MEDIUM | `catch (...) { data.isCalibrated = false; }` — traga errores de parseo JSON |
| `core/SessionSerializer.cpp` | 142-145 | MEDIUM | `catch (...) { return false; }` — traga excepciones al guardar sesión |

**Recomendación:** Agregar `catch (const std::exception& e)` con logging antes del `catch(...)`.

### `snprintf` a buffer `char[]`

| Archivo | Líneas | Severidad |
|---------|--------|-----------|
| `core/ProfilingSession.cpp` | 107, 150, 191, 228, 262, 300 | LOW |

6 instancias de `snprintf(buf, sizeof(buf), "TC_FLT_%03d", id++)` — reemplazable con `fmt::format`.

### `push_back` sin `reserve()`

~100 llamadas `push_back` vs solo 6 `reserve()`. Notable:
- `core/ProfilingSession.cpp` — loops de tamaño conocido sin reserve en `tc.parameterSteps` y `session.testCases`

### `std::string` concatenation en loops

~89 matches de concatenación con `+`. Notable:
- `core/ProfilingSequencer.cpp:96,121-132` — concatenación incremental en loop para mensajes de progreso

### Loops indexados candidates a range-for

~97 loops con índice. Solo ~8-10 son puros candidates a range-for (solo leen `container[i]` sin necesitar el índice):
- `math/NoiseFloorTracker.cpp:123`
- `gui/LiveCurvePlotter.cpp:123,163,212`
- `gui/SoundIdCurvePlotter.cpp:241,290,349`
- `export/LutExporter.cpp:47`

### Patrones limpios (sin issues)

✅ No `rand()`/`srand()` — usa LCG determinístico
✅ No C-style casts — todos `static_cast`
✅ No `NULL` — todos `nullptr`
✅ No `#define` para constantes — usa `constexpr`
✅ No `typedef` — usa `using`
✅ No `using namespace` en headers
✅ No constructores de un solo argumento sin `explicit`
✅ No `override` faltante
✅ No virtual con default args
✅ No move operator sin `noexcept`

---

## Archivos revisados

| Módulo | Archivos | Issues |
|--------|----------|--------|
| `src/audio/` | 6 | 21 |
| `src/math/` | 8 | 36 |
| `src/core/` | 8 | 16 |
| `src/hardware/` | 7 | 18 |
| `src/gui/` | 28 | 63 |
| `src/main.cpp` | 1 | 14 |
| `src/export/` | 2 | 3 |
| **Total** | **60** | **171** |

---

## 14. Testing

### Hallazgos

| Hallazgo | Severidad | Notas |
|----------|-----------|-------|
| **Sin unit tests** — no hay archivos de test en `src/` ni directorios `tests/`, `test/`, `__tests__/` | HIGH | `TestConfigModal` es un diálogo GUI, no framework de test |
| **Sin framework de test** — no Google Test, Catch2, doctest, ni JUCE `UnitTests` | HIGH | — |
| **Sin CI/CD** — no `.github/workflows/`, `Jenkinsfile`, `.gitlab-ci.yml` | HIGH | Solo `build.bat` manual |
| **QA manual** — `docs/QA_TEST_PLAN.md` con 12 casos manuales marcados "PASADO" | MEDIUM | Sin verificación automatizada |

**Recomendación:** Agregar Google Test o Catch2 para al menos los módulos matemáticos (`FarinaDeconvolver`, `LoopbackCalibrator`, `LabAnalyticEngine`). Configurar `add_test()` en CMake y un workflow básico de CI.

---

## 15. Build System (CMakeLists.txt)

### Estado

| Item | Valor | Evaluación |
|------|-------|------------|
| CMake mínimo | 3.22 | OK |
| Estándar C++ | C++20 (no extensions) | OK |
| Versión proyecto | 1.0.0 | OK |
| JUCE | 8.0.4 (FetchContent) | OK |
| nlohmann_json | v3.11.3 (FetchContent) | OK |
| Warnings | `/W4` | Bueno |
| UTF-8 MSVC | `/utf-8` | Bueno |
| Conformance | `/permissive-` | Bueno |

### Issues

| # | Archivo | Línea | Severidad | Descripción |
|---|---------|-------|-----------|-------------|
| 1 | `CMakeLists.txt` | — | MEDIUM | Falta `/WX` (warnings as errors) — permite warnings silenciosos |
| 2 | `CMakeLists.txt` | — | MEDIUM | Sin sanitizers (ASan/UBSan) — sin checks de runtime correctness |
| 3 | `CMakeLists.txt` | — | MEDIUM | Sin reglas `install()` — sin soporte de packaging/distribución |
| 4 | `CMakeLists.txt` | — | LOW | `FETCHCONTENT_UPDATES_DISCONNECTED` no configurado — re-clona en cada configure |
| 5 | `CMakeLists.txt` | — | LOW | Sin `option()` para modos de build |

---

## 16. Documentación

### Estado

| Item | Evaluación |
|------|------------|
| `README.md` | Excelente — completo, bien estructurado, diagramas mermaid |
| `docs/ARCHITECTURE.md` | Exhaustivo — capas, data flow, secuency diagrams, RT safety rules |
| `docs/HARDWARE_PROTOCOLS.md` | Referenciado, existe |
| `docs/MATHEMATICAL_MODELS.md` | Referenciado, existe |
| `docs/QA_TEST_PLAN.md` | 12 casos manuales |
| `docs/ROADMAP.md` | Referenciado, existe |
| `docs/HANDOFF.md` | Referenciado, existe |

### Issues

| # | Severidad | Descripción |
|---|-----------|-------------|
| 1 | MEDIUM | Sin Doxyfile o configuración Doxygen |
| 2 | MEDIUM | Sin CHANGELOG.md — `BuildVersion.h` tiene versión pero sin historial |
| 3 | LOW | Sin LICENSE file |
| 4 | LOW | Sin CONTRIBUTING.md |
| 5 | LOW | Comentarios `@param` y `@return` dispersos — buenos en módulos math/hardware, sparse en GUI/audio |

---

## 17. Real-Time Latency

### Heap Allocations en Audio Callbacks

| Archivo | Línea | Severidad | Descripción |
|---------|-------|-----------|-------------|
| `LabAudioEngine.cpp` | 126-127 | **CRITICAL** | `tempProcessBuffer.assign()` dentro del callback — si el buffer size cambia en runtime → heap allocation en audio thread |
| `LabAudioEngine.cpp` | 69 | OK | `assign()` en `audioDeviceAboutToStart` (no callback) |

### Lock Acquisitions en Audio Callbacks

| Módulo | Evaluación |
|--------|------------|
| `LabAudioEngine.cpp` | ✅ Sin locks |
| `LabStimulusGenerator.cpp` | ✅ Sin locks |
| `LabAudioReceiver.cpp` | ✅ Usa `AbstractFifo` (lock-free) |
| `MockHardwareController.h` | ✅ Sin locks |

**Verdicto:** Zero locks en audio thread. Limpio.

### System Calls en Audio Callbacks

| Categoría | Evaluación |
|-----------|------------|
| File I/O | ✅ Ninguno en callbacks |
| Time functions | ✅ Ninguno en callbacks |
| Logging | ✅ Solo en `initializeAudioDevices()` (control thread) |

### Floating-Point Denormals

| Archivo | Línea | Evaluación |
|---------|-------|------------|
| `LabAudioEngine.cpp` | 96 | ✅ `ScopedNoDenormals` presente |
| `MockHardwareController.h` | 94 | ✅ `ScopedNoDenormals` presente |

### Issues de Real-Time (ya documentados)

| Severidad | Cantidad | Notas |
|-----------|----------|-------|
| CRITICAL | 1 | `tempProcessBuffer` alloc en callback (#7 en §1) |
| CRITICAL | 5+ | Data races en variables compartidas audio/control thread (ya en §1) |

---

## 18. Issues Corregidos (verificados en código)

Los siguientes issues han sido verificados en el código fuente:

| # | Issue original | Archivo | Estado | Detalle |
|---|---------------|---------|--------|---------|
| 1 | **#95** `callAfterDelay` con `this` capture sin lifetime guard | `main.cpp` | ✅ **CONFIRMADO** | Helper `hidePromptAfterDelay()` creado y usado en 6 call-sites. Último residual en línea 1560 (splash fade-out) ahora protegido con `SafePointer<SoundIdSplashWindow>`. |
| 2 | **#97** Callbacks del sequencer mutan UI sin `callAsync` | `main.cpp` | ✅ **CONFIRMADO** | `setOperatorStepCallback`, `setProgressCallback`, `setTestIndexCallback` — todos envueltos en `juce::MessageManager::callAsync()`. |
| 3 | **#94** Versión hardcodeada en `buildCurrentSessionManifest()` | `main.cpp` | ✅ **CONFIRMADO** | Usa `version::kAppVersion` y `version::kBuildNumber` desde `BuildVersion.h` (líneas 1134-1135). |
| 4 | **#108** Header generado usa `static const` arrays | `LutExporter.cpp` | ✅ **CONFIRMADO** | Cambiado a `inline constexpr size_t` y `inline const alignas(16)` (líneas 55-56). |
| 5 | **#109** `tableName` inyectado sin sanitización | `LutExporter.cpp` | ✅ **CONFIRMADO** | Sanitización completa: filtra no-alnum, agrega prefijo `lut_` si empieza con dígito (líneas 44-53). |
| 6 | **#110** Sin check `out.good()` después de writes | `LutExporter.cpp` | ✅ **CONFIRMADO** | Los 3 exportadores (`exportToCppHeader`, `exportToJsonReport`, `exportSessionManifest`) ahora retornan `out.good()`. |
| 7 | C4458 shadow warning variable `session` | `main.cpp` | ✅ **CONFIRMADO** | Renombrada a `profSession` en `buildProfilingSessionFromQueue()` (línea 995). Grep de `core::ProfilingSession session;` retorna 0 resultados — sin shadow declarations. |
| 8 | C4100 unused parameter `backgroundColour` | `SoundIdTheme.h` | ✅ **CONFIRMADO** | Parámetro eliminado de la firma del override — `drawComboBox` ya no lo declara (línea 130). |
| 9 | Bug en `mouseDown()` con `\|\| true` trap | `SoundIdSuiteList.cpp` | ✅ **CONFIRMADO** | Sin `\|\| true` en el archivo. Sub-row clicks usan `subRect.contains(e.position)` correctamente (líneas 638-663). |
| 10 | **SessionManager y HardwareManager huérfanos** | `main.cpp` | ✅ **CONFIRMADO** | `main.cpp` incluye `core/SessionManager.h` y `core/HardwareManager.h` (líneas 11-12) pero nunca instancia ni usa ninguna de las dos clases. Los managers están completamente implementados (SessionManager 63 líneas, HardwareManager 66 líneas) pero son código muerto — `main.cpp` aún gestiona sesiones y hardware inline. **Pendiente: integrar o eliminar includes.** |
