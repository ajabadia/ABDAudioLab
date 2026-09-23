# Matriz de Convergencia Guiado / Exploración — PARITY-01

**Proyecto:** ABDAudioLab (Universal Black-Box Musical Hardware & Synth Profiler)  
**Documento:** MATRIX_PARITY_01.md  
**Hito:** PARITY-01 (PARITY-01A / PARITY-01B)  
**Versión:** 1.2.0  
**Fecha de Certificación:** 2026-09-23  
**Estado:** ✅ CERTIFICADO — 2 test cases PASS | 35 assertions PASS (Build #411)  
**Referencia:** [ADR-001](docs/ADR-001_CONVERGENCIA_GUIADO_EXPLORACION.md) | [PLAN_PARITY_01.md](PLAN_PARITY_01.md)  

---

## 1. Definición de Estados y Rutas

### 1.1. Las Tres Rutas en Auditoría
* **Ruta 1 — Guiado Sistemático (GS):** Medición lanzada desde `SoundIdProfilingRunView` (`onStartClicked`).
* **Ruta 2 — Libre Sistemático (LS):** Medición lanzada desde `suiteList` (`onToggleSessionRunClicked`).
* **Ruta 3 — Toma Libre (TL):** Captura manual ad-hoc lanzada desde `btnFreeCapture` (`triggerFreeCapture()`).

### 1.2. Estados de Paridad
* 🟢 **Paridad Completa:** Idéntico código, contratos y resultados numéricos indistinguibles.
* 🟡 **Divergencia de Metadatos / Presentación:** Mismo motor DSP y de ejecución, pero bifurcación en flags (`r.kind`), paths o visibilidad.
* 🔴 **Divergencia Material:** Motores, buffers o algoritmos distintos.
* 🔵 **Verificado Estructuralmente:** Motor compartido confirmado; ejecución con audio real delegada a INTEGRATION-01.

---

## 2. Matriz de Auditoría de las Ocho Dimensiones

| ID | Dimensión Metrológica | Componentes en Código (`src/`) | Guiado Sistemático (GS) | Libre Sistemático (LS) | Toma Libre (TL) | Estado | Evidencia Recolectada |
|---|---|---|---|---|---|:---:|---|
| **D1** | **Selección de Target** | `gui/soundid/SoundIdHardwareCatalogSelector.h`<br>`MainContentComponent.cpp:3606` | Pasa por `resolveCanonicalTarget()` o inicialización demo. | Pasa por `resolveCanonicalTarget()` vía catálogo interactivo. | Usa el target activo pero no valida contrato estricto. | 🟢 | **BUILD #411:** `TargetSelectionState` idéntico en GS y LS (targetId, targetName, manufacturer, isDeterministic). 4/4 aserciones PASS. |
| **D2** | **Estado Inicial de Sesión** | `gui/session/ProfilingSessionController.cpp`<br>`gui/MainContentComponent.cpp` | `setupGuidedWorkflowInitialData()` inicializa estado específico; `setWorkflowMode(Guided)`. | Inicializado por defecto en `UiWorkflowMode::Classic`. | Opera sobre la sesión activa sin reinicializar el secuenciador. | 🟢 | **BUILD #411:** `ProfilingSession` idéntica en GS y LS: 3 TestCases, mismos IDs, misma nota MIDI C4, mismo gate 250ms. 9/9 aserciones PASS. |
| **D3** | **Receta / ExperimentPlan** | `core/ProfilingSession.h`<br>`ProfilingSessionController.cpp:~1019` | Construye sesión desde la cola. `r.kind = Measurement`. | Construye sesión desde la cola. `r.kind = Exploration`. | Sin plan declarado. | 🟡 | **BUILD #411:** Bifurcación documentada y fijada como línea base. `workflowMode::Guided` ≠ `workflowMode::Classic` en snapshot. 4/4 aserciones PASS. **Divergencia objetivo de HITO-CONVERGENCIA-01.** |
| **D4** | **Calibración y Preflight** | `gui/controllers/CanonicalCalibrationState.h`<br>`gui/NativeCalibrationPanel.h` | Exige calibración previa obligatoria en Paso 2. | Permite ver calibración o continuar con nominal (0 dB). | No exige calibración previa antes del disparo. | 🟡 | Inspección estática: GS exige `isReadyForProfiling()`. LS permite continuar sin calibración completada. Divergencia de UI, no de motor. |
| **D5** | **Despacho de Eventos** | `core/ProfilingSequencer.cpp`<br>`audio/LabAudioEngine.cpp` | `ProfilingSequencer` despacha notas deterministas con `gateMs` y `settlingMs`. | `ProfilingSequencer` despacha idénticas notas y tiempos. | Envía NoteOn(60, 0.8) en vivo y NoteOff tras 1200ms por timer. | 🔵 (GS vs LS)<br>🔴 (vs TL) | **BUILD #411:** Ambas rutas instancian `ProfilingSequencer` en estado `Idle`. Motor idéntico confirmado estructuralmente. Ejecución con audio real → **INTEGRATION-01** (requiere WASAPI/dispositivo real). |
| **D6** | **Ruta de Audio y Captura** | `core/ProfilingSequencer.cpp`<br>`audio/LabAudioEngine.cpp` | Render determinista por bloques fijos de 256 muestras. | Render determinista por bloques fijos de 256 muestras. | Grabación en streaming continuo desde el driver de audio. | 🔵 (GS vs LS)<br>🔴 (vs TL) | Mismo motor confirmado. Comparación de buffers float (diferencia ≤ 10⁻⁷) → **INTEGRATION-01**. |
| **D7** | **Análisis y Métricas** | `dsp/`<br>`measurement/`<br>`export/MeasuredPoint.h` | Calcula RMS, Pico, THD%, SNR, f0 y FFT. | Calcula idénticas métricas sobre los bloques del secuenciador. | No ejecuta análisis espectral sistemático ni genera LUT. | 🔵 (GS vs LS)<br>🔴 (vs TL) | Mismo pipeline de análisis confirmado. Comparación numérica de `MeasuredPoint` → **INTEGRATION-01**. |
| **D8** | **Evaluación e Informe** | `export/ReportExportService.cpp`<br>`gui/controllers/ReportExportUiController.cpp` | Genera `ProductionPackage` y reporte HTML. Busca `guided/session.json`. | Genera `ProductionPackage` y reporte HTML. No busca `guided/`. | No apta para exportación formal de producción. | 🟡 (GS vs LS)<br>🟢 (TL bloqueada) | **BUILD #411 (PARITY-01B):** `ExportReadiness::Decision::Blocked` sin target y con `Rejected`. `canProceed() == false`. 18/18 aserciones PASS. |

---

## 3. Evidencia Empírica — Build #411 (2026-09-23)

```
.\build\Release\ABDAudioLab_Tests.exe "[parity]"
Filters: [parity]
Randomness seeded to: 2529713386
===============================================================================
All tests passed (35 assertions in 2 test cases)
```

### Desglose por test case

| Test Case | Tag | Assertions | Resultado |
|---|---|:---:|---|
| PARITY-01A: Guiado y Libre comparten identico motor ProfilingSequencer (contrato estructural) | `[parity][parity-01a]` | 17 | ✅ PASS |
| PARITY-01B: Toma Libre queda clasificada como exploracion no certificable | `[parity][parity-01b]` | 18 | ✅ PASS |
| **TOTAL** | `[parity]` | **35** | **✅ 2/2 PASS** |

### Nota sobre D5/D6/D7
`ProfilingSequencer` hereda de `juce::Thread` y requiere callback de audio real (WASAPI/DirectSound) para completar su ciclo de medición. En entorno headless sin dispositivo de audio, el hilo bloquea esperando `audioDeviceIOCallbackWithContext`. La evidencia estructural (mismo tipo, mismo estado `Idle`, misma instanciación) es suficiente para PARITY-01. La ejecución con audio real queda delegada a la suite **INTEGRATION-01**.

---

## 4. Síntesis y Dictamen Final

### PARITY-01A — Guiado Sistemático vs Libre Sistemático
- ✅ **Motor único confirmado:** Ambas rutas instancian el mismo `ProfilingSequencer` con el mismo estado inicial `Idle`.
- ✅ **Contrato de sesión idéntico:** Misma `ProfilingSession`, mismos `TestCase`, mismos parámetros MIDI.
- ✅ **Target convergente:** Mismo `TargetSelectionState` para el mismo hardware.
- 🟡 **Única divergencia real:** `r.kind` en `buildExportMetadataRecord` (línea ~1019): `Measurement` vs `Exploration` según `workflowMode`. **Objetivo de HITO-CONVERGENCIA-01.**

### PARITY-01B — Toma Libre
- ✅ **Clasificada y bloqueada:** `ExportReadiness::Decision::Blocked` sin target o con `Rejected`.
- ✅ **No certificable para producción:** `canProceed() == false` en todos los escenarios de Toma Libre.

---

## 5. Próximos Pasos Post-Certificación

| Hito | Objetivo |
|---|---|
| **INTEGRATION-01** | Añadir `MockAudioEngine` con bucle de retorno para habilitar D5/D6/D7 en entorno headless. |
| **HITO-CONVERGENCIA-01** | Eliminar la bifurcación `r.kind` y unificar `workflowMode` como parámetro de presentación, no de clasificación metrológica. |
