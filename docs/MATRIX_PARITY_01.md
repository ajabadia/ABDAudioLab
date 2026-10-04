# Matriz de Convergencia Guiado / Exploración — PARITY-01

**Proyecto:** ABDAudioLab (Universal Black-Box Musical Hardware & Synth Profiler)  
**Documento:** MATRIX_PARITY_01.md  
**Hito:** PARITY-01 (PARITY-01A / PARITY-01B)  
**Versión:** 1.4.0  
**Fecha de Última Actualización:** 2026-09-23  
**Estado:** CARACTERIZACIÓN UNITARIA Y CONVERGENCIA SEMÁNTICA CERTIFICADAS — PARIDAD E2E DE AUDIO PENDIENTE (INTEGRATION-01)  
**Referencia:** [ADR-001](docs/ADR-001_CONVERGENCIA_GUIADO_EXPLORACION.md) | [PLAN_PARITY_01.md](PLAN_PARITY_01.md)

---

## 1. Definición de Estados y Rutas

### 1.1. Las Tres Rutas en Auditoría
* **Ruta GS — Guiado Sistemático:** Medición desde `SoundIdProfilingRunView` (`onStartClicked`).
* **Ruta LS — Libre Sistemático:** Medición desde `suiteList` (`onToggleSessionRunClicked`).
* **Ruta TL — Toma Libre:** Captura ad-hoc desde `btnFreeCapture` (`triggerFreeCapture()`).

### 1.2. Estados de Paridad
* 🟢 **PASS unitario / estructural:** Convergencia demostrada en test hermético.
* 🟡 **Divergencia confirmada:** Diferencia real documentada; corrección necesaria.
* 🔵 **Motor común / pendiente E2E:** Motor compartido confirmado; evidencia de audio/DSP pendiente.
* ⚪ **Pendiente:** Sin test de contraste finalizado.

---

## 2. Matriz de Auditoría — Estado por Dimensión

| ID | Dimensión | Componentes clave | GS | LS | TL | Estado | Evidencia / Cierre |
|---|---|---|---|---|---|:---:|---|
| **D1** | **Selección de Target** | `SoundIdHardwareCatalogSelector.h`<br>`MainContentComponent.cpp:3606` | `resolveCanonicalTarget()` | `resolveCanonicalTarget()` | Usa target activo sin contrato estricto | 🟢 PASS unitario | Build #411: `TargetSelectionState` idéntico (targetId, targetName, manufacturer, isDeterministic). 4 assertions PASS. |
| **D2** | **Sesión Inicial** | `ProfilingSessionController.cpp`<br>`MainContentComponent.cpp` | `setupGuidedWorkflowInitialData()` + `setWorkflowMode(Guided)` | `setWorkflowMode(Classic)` por defecto | Opera sobre sesión activa sin reinicializar | 🟢 PASS unitario | Build #411: `ProfilingSession` idéntica (3 TestCases, C4, gate 250ms, settlingMs). 9 assertions PASS. |
| **D3** | **Plan / Metadatos** | `ProfilingSessionController.cpp`<br>`ProfilingSessionContracts.h` | `deduceExperimentKindFromSnapshot()` | `deduceExperimentKindFromSnapshot()` | Sin `ExperimentPlan` $\to$ `Exploration` | 🟢 **PASS convergencia** | Build #414 (HITO-CONVERGENCIA-01): `r.kind` ya no bifurca por `workflowMode`. Deducción canónica por plan/receta válida. 16 assertions PASS. |
| **D4** | **Calibración / Preflight** | `CanonicalCalibrationState.h`<br>`NativeCalibrationPanel.h` | Exige calibración previa en UI | Valida preflight en controlador | Sin requisito de calibración | 🟢 **PASS auditado (2 niveles)** | Build #414 (CONVERGENCE-01E): `isReadyForProfiling()` auditado a nivel preflight sin degradar la intención metrológica formal. |
| **D5** | **Despacho — Motor** | `ProfilingSequencer.h` | Mismo `ProfilingSequencer` | Mismo `ProfilingSequencer` | Timer manual (NoteOn/NoteOff ad-hoc) | 🔵 Motor común | Build #411: ambas rutas instancian `ProfilingSequencer` en `SequencerState::Idle`. |
| **D5** | **Despacho — Eventos efectivos** | `ProfilingSequencer.cpp`<br>`ProfilingHardwareDispatcher.cpp` | Notas deterministas con `gateMs` y `settlingMs` | Ídem | NoteOn(60, 0.8) + NoteOff tras 1200ms | 🔵 Pendiente E2E | Requiere traza de integración con audio real. → **INTEGRATION-01** |
| **D6** | **Audio / Captura** | `ProfilingAudioCapture.cpp`<br>`LabAudioEngine.cpp` | Bloques fijos de 256 muestras (deterministas) | Ídem | Streaming continuo desde driver | 🔵 Pendiente E2E | Comparación de buffers float (diff ≤ 10⁻⁷) + SHA-256. → **INTEGRATION-01** |
| **D7** | **DSP / Métricas** | `dsp/`<br>`measurement/`<br>`export/MeasuredPoint.h` | RMS, Pico, THD%, SNR, f0, FFT | Ídem (mismo pipeline) | Sin análisis espectral sistemático | 🔵 Pendiente E2E | Comparación numérica de `MeasuredPoint` (SNR, THD%, ganancia). → **INTEGRATION-01** |
| **D8** | **Exportación** | `ReportExportService.cpp`<br>`ReportExportUiController.cpp` | `ProductionPackage` + busca `guided/` | `ProductionPackage` sin `guided/` | **No exportable** | 🟡 (GS vs LS)<br>🟢 **PASS TL** | Build #411 (PARITY-01B): `ExportReadiness::Decision::Blocked`, `canProceed() == false`. 18 assertions PASS. |

---

## 3. Evidencia Empírica — Build #411 (2026-09-23)

```
.\build\Release\ABDAudioLab_Tests.exe "[parity]"
All tests passed (35 assertions in 2 test cases)
```

| Test Case | Assertions | Dimensiones cubiertas | Estado |
|---|:---:|---|---|
| PARITY-01A: contrato estructural | 17 | D1, D2, D3, D5-motor | ✅ PASS |
| PARITY-01B: Toma Libre no certificable | 18 | D8 | ✅ PASS |
| **TOTAL** | **35** | | **2/2 PASS** |

> **Limitación de alcance:** D5-eventos, D6, D7 requieren `audioDeviceIOCallbackWithContext` (WASAPI/DirectSound). En entorno headless `ProfilingSequencer::run()` bloquea indefinidamente. La evidencia con audio real es competencia de **INTEGRATION-01**.

---

## 4. Dictamen Honesto

```
Build #411: PASS
Casos de test: 2/2
Assertions: 35/35

Convergencia demostrada (PASS):
  ✅ D1 — TargetSelectionState idéntico
  ✅ D2 — ProfilingSession idéntica
  ✅ D5 — Mismo ProfilingSequencer (motor estructural)
  ✅ D8 — Toma Libre bloqueada para exportación

Divergencia confirmada (requiere corrección):
  ❌ D3 — ExperimentKind depende de workflowMode, no de la evidencia metodológica

Paridad pendiente de demostración empírica:
  ⏳ D4 — Calibración/preflight (contraste GS vs LS)
  ⏳ D5 — Despacho efectivo de eventos
  ⏳ D6 — Audio capturado (buffers, SHA-256)
  ⏳ D7 — Métricas DSP (SNR, THD%, MeasuredPoint)
  ⏳ D8 — EvaluationSnapshot, ProductionPackage, manifest RFC 8785 (GS vs LS)

Etiqueta correcta:
  PARITY-01: CARACTERIZACIÓN UNITARIA CERTIFICADA
  PARIDAD E2E DE AUDIO: PENDIENTE DE INTEGRATION-01
```

---

## 5. Hallazgo Central — D3 (el más valioso)

El siguiente fragmento es incorrecto para la arquitectura objetivo:

```cpp
// ProfilingSessionController.cpp:~1019
r.kind = (workflowMode == Guided)
       ? core::ExperimentKind::Measurement
       : core::ExperimentKind::Exploration;
```

**Por qué es un error:** Dos usuarios pueden ejecutar el mismo target, la misma cola, la misma receta, el mismo preflight, el mismo `ProfilingSequencer` y la misma captura, y aun así recibir clasificaciones distintas únicamente por el botón de entrada a la UI.

**Regla canónica objetivo** (a implementar en HITO-CONVERGENCIA-01):

```
ExperimentKind::Measurement
  ← ExperimentPlan válido declarado
  ← Preflight cumple política de calibración
  ← Ejecución genera evidencia trazable

ExperimentKind::Exploration
  ← Sin ExperimentPlan formal, o
  ← Preflight insuficiente, o
  ← Sin evidencia trazable completa

Nunca:
  Guided  → Measurement
  Classic → Exploration
```

---

## 6. Sobre Toma Libre — Mejora de UX Pendiente

El bloqueo técnico (`canProceed() == false`) es necesario pero insuficiente. La interfaz debe hacerlo visible:

```
┌─ Toma Libre — Exploración no certificable ──────────────────────┐
│                                                                  │
│  Esta captura no contiene un ExperimentPlan declarado y no       │
│  puede generar un paquete de producción.                         │
│                                                                  │
│  [Convertir esta toma en receta]   [Crear plan de medición]      │
└──────────────────────────────────────────────────────────────────┘
```

---

## 7. Próximos Pasos (Secuencia Segura)

| Paso | Hito | Objetivo | Requisito previo |
|:---:|---|---|---|
| 1 | **HITO-CONVERGENCIA-01** | Corregir clasificación `ExperimentKind` desde evidencia metodológica. Retirar dependencia de `guided/`. | PARITY-01 certificado ✅ |
| 2 | **HITO-CONVERGENCIA-01** | Tests de clasificación (GS→Measurement, LS→Measurement, TL→Exploration, plan incompleto→bloqueado). | Paso 1 |
| 3 | **INTEGRATION-01** | `MockAudioEngine` con bucle de retorno. D5-eventos, D6, D7: misma receta GS vs LS, trazas reales, audio, métricas, EvaluationSnapshot, manifest. | HITO-CONVERGENCIA-01 completado |
| 4 | **HITO-09** | Banco de trabajo único, presets y macrotareas. | INTEGRATION-01 PASS |
