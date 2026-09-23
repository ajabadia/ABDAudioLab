# Plan de Ejecución — PARITY-01: Auditoría de Convergencia Guiado/Exploración

**Proyecto:** ABDAudioLab (Universal Black-Box Musical Hardware & Synth Profiler)  
**Documento:** PLAN_PARITY_01.md  
**Hito:** PARITY-01 (PARITY-01A / PARITY-01B)  
**Versión:** 1.1.0  
**Fecha de Actualización:** 2026-09-23  
**Estado:** EN CURSO (Fase 2: Implementación de Test de Caracterización)  
**Referencia Arquitectónica:** [ADR-001](docs/ADR-001_CONVERGENCIA_GUIADO_EXPLORACION.md)  
**Hoja de Ruta:** [docs/ROADMAP.md v2.4.0](docs/ROADMAP.md) §3b  

---

## 1. Misión y Axioma de Trabajo

> **Axioma:** *«El usuario elige cuánta ayuda necesita; el software siempre ejecuta la misma ciencia.»*  
> **Regla de no regresión:** *«Caracterizar primero; comparar después; migrar después; retirar al final.»*

La inspección estática ha confirmado la hipótesis: el motor de ejecución y análisis ya es único (`ProfilingSequencer` y `ReportExportService`). Las diferencias residen en la presentación de UI y en bifurcaciones puntuales de metadatos (`r.kind`) y evidencias.

PARITY-01 se estructura en dos sub-auditorías complementarias:
* **PARITY-01A:** Comparación científica de paridad entre **Guiado Sistemático** y **Libre Sistemático**.
* **PARITY-01B:** Clasificación formal y blindaje de la **Toma Libre** como exploración no metrológica.

---

## 2. PARITY-01A — Guiado Sistemático vs. Libre Sistemático

### 2.1. Caso Testigo Controlado (Benchmark Test Case)
* **Target:** `ReferenceSynth` (o `SyntheticAudioFixture` interno determinista)
* **Frecuencia de Muestreo:** 48.000 Hz
* **Tamaño de Bloque (Buffer):** 256 muestras
* **Preset / Algoritmo:** Sine pura (onda senoidal sin modulación ni efectos)
* **Estímulo:**
  * Nota MIDI: C4 (Note Number 60)
  * Velocidad MIDI: 64
  * Compuerta (`gateMs`): 250 ms
  * Estabilización (`settlingMs`): 50 ms
  * Repeticiones: 3 iteraciones

### 2.2. Rutas Comparadas
* **Ruta A (Guiado Sistemático):** `SoundIdProfilingRunView::onStartClicked` $\to$ `startProfilingSession(false)`.
* **Ruta B (Libre Sistemático):** `suiteList::onToggleSessionRunClicked` $\to$ `startProfilingSession(false)`.

### 2.3. Puntos de Extracción y Comparación
1. `ExperimentPlan` / items de la cola de pruebas.
2. `ProfilingSession` construida.
3. Despacho y secuencia MIDI (`sequenceHash` y `ExecutionTrace`).
4. Buffers de audio crudo procesados (diferencia flotante máxima $\le 10^{-7}$ y SHA-256).
5. Métricas acústicas extraídas (RMS, Pico, THD%, SNR, f0, correlación espectral $\rho$).
6. `EvaluationSnapshot` y veredicto emitido (`SelectionStatus`).
7. Manifest emitido RFC 8785 y reporte HTML.
8. Elegibilidad de exportación y paquete `ProductionPackage`.

---

## 3. PARITY-01B — Clasificación Formal de la Toma Libre

La **Toma Libre** (`btnFreeCapture` $\to$ `triggerFreeCapture()`) no es una campaña metrológica y no debe ser tratada como equivalente. Esta auditoría verifica y fija sus límites:

1. **No Certificable:** No puede recibir `r.kind = ExperimentKind::Measurement` ni veredicto `SelectionStatus::Accepted`.
2. **Exportación Bloqueada:** Las guardas de `ExportReadinessEvaluator` deben impedir la generación de paquetes de producción (`ProductionPackage`) a partir de tomas libres aisladas.
3. **Trazabilidad Explícita:** Debe mantener explícitamente `confirmationStatus = "unknown"` y `displayValue = "Posición no declarada"`.
4. **Camino de Formalización:** El sistema debe ofrecer la opción de convertir la captura libre en una receta declarativa formal si el usuario desea certificarla.

---

## 4. Test de Caracterización Unitario (Catch2)

* **Archivo:** `src/tests/test_Parity01GuidedVsClassic.cpp`
* **Secciones del Test:**
  1. `[parity][systematic]`: Ejecuta el Caso Testigo en modo Guiado y en modo Libre Sistemático sobre `SyntheticAudioFixture`, validando la igualdad estricta de las 8 dimensiones metrológicas.
  2. `[parity][metadata_divergence]`: Caracteriza y documenta la divergencia actual en `r.kind` (`Measurement` vs `Exploration`) para fijar la línea base antes de cualquier refactor.
  3. `[parity][free_capture]`: Verifica que la toma libre sea rechazada por las guardas de exportación formal de producción.

---

## 5. Criterios de Salida de PARITY-01

1. Test `test_Parity01GuidedVsClassic.cpp` compilado y en verde (100% PASS).
2. Matriz `MATRIX_PARITY_01.md` actualizada con evidencia empírica.
3. Acta oficial `ACTA_PARITY_01_CONVERGENCE_AUDIT.md` emitida con dictamen vinculante para **HITO-09**.
4. Cero código de producción eliminado o alterado prematuramente (respeto a la regla de caracterización previa).
5. Suite completa de 615 tests baseline intacta.
