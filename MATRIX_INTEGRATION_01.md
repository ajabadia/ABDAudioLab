# Matriz de Paridad E2E de Audio y DSP — INTEGRATION-01

**Proyecto:** ABDAudioLab (Universal Black-Box Musical Hardware & Synth Profiler)  
**Documento:** MATRIX_INTEGRATION_01.md  
**Hito:** INTEGRATION-01  
**Versión:** 1.0.0  
**Fecha:** 2026-09-23  
**Estado:** PLANIFICACIÓN / PENDIENTE DE EJECUCIÓN DEL HARNESS  
**Referencia:** [PLAN_INTEGRATION_01.md](PLAN_INTEGRATION_01.md) | [MATRIX_PARITY_01.md](MATRIX_PARITY_01.md)

---

## 1. Niveles de Verificación de Paridad E2E

### Nivel 1: Control Exacto (Entrada y Secuencia)

| Dimensión | Parámetro Auditado | Criterio de Aceptación | Estado | Evidencia |
|---|---|---|:---:|---|
| **D1** | `TargetSelectionState` | Igualdad exacta (targetId, manufacturer, deterministic flag) | ⚪ Pendiente | — |
| **D2** | `ExperimentPlan` / Receta | Misma nota C4, vel 64, gate 250ms, settling 50ms, 3 repeticiones | ⚪ Pendiente | — |
| **D2** | `sequenceHash` | Hash idéntico | ⚪ Pendiente | — |
| **D4** | Estado de calibración | Misma política digital/preflight satisfecha | ⚪ Pendiente | — |
| **D5** | Eventos MIDI (NoteOn) | Conteo exacto, mismo número de nota y velocidad | ⚪ Pendiente | — |
| **D5** | Eventos MIDI (NoteOff) | Conteo exacto, orden idéntico | ⚪ Pendiente | — |
| **D5** | Despacho de bloques | Conteo exacto de bloques de 256 muestras procesados | ⚪ Pendiente | — |

---

### Nivel 2: Audio Canónico (Fidelidad de Señal y Buffers)

| Dimensión | Parámetro Auditado | Criterio de Aceptación | Estado | Evidencia |
|---|---|---|:---:|---|
| **D6** | Canales y Sample Rate | 2 canales, 48.000 Hz exactos | ⚪ Pendiente | — |
| **D6** | Conteo total de muestras | Número idéntico de muestras capturadas en ambas rutas | ⚪ Pendiente | — |
| **D6** | Diferencia Máxima ($\max \|\Delta s\|$) | $\le 10^{-7}$ (bit-exacto en punto flotante) | ⚪ Pendiente | — |
| **D6** | Error Cuadrático Medio ($\text{RMSE}$) | $\le 10^{-7}$ | ⚪ Pendiente | — |
| **D6** | Huella Canónica SHA-256 | Hash SHA-256 idéntico sobre serialización canónica float32 LE | ⚪ Pendiente | — |

---

### Nivel 3: Salida Científica y Metrología DSP

| Dimensión | Métrica Auditada | Tolerancia Admisible | Estado | Evidencia |
|---|---|---|:---:|---|
| **D7** | Nivel Pico (dBFS) | $\Delta \le 0.001\text{ dB}$ | ⚪ Pendiente | — |
| **D7** | Nivel RMS (dBFS) | $\Delta \le 0.001\text{ dB}$ | ⚪ Pendiente | — |
| **D7** | SNR (dB) | $\Delta \le 0.01\text{ dB}$ | ⚪ Pendiente | — |
| **D7** | THD (%) | $\Delta \le 0.001\%$ | ⚪ Pendiente | — |
| **D7** | Frecuencia Fundamental ($f_0$) | $\Delta \le 0.01\text{ Hz}$ | ⚪ Pendiente | — |
| **D7** | `MeasuredPoint` vector | Mismo número de puntos e idénticos campos acústicos | ⚪ Pendiente | — |
| **D8** | `EvaluationSnapshot` | Idéntico `selectionStatus` y conjunto de advertencias | ⚪ Pendiente | — |
| **D8** | Manifest RFC 8785 | Idéntico salvo timestamps y sessionId | ⚪ Pendiente | — |
| **D8** | `ProductionPackage` | Estructura canónica idéntica | ⚪ Pendiente | — |

---

## 2. Diagnóstico de Primer Punto de Falla (Failure Localization)

En caso de cualquier diferencia durante la ejecución de la prueba, el harness identificará de inmediato la etapa de bifurcación:

1. `FAIL_LEVEL_1_PLAN`: Discrepancia en la receta o parámetros iniciales.
2. `FAIL_LEVEL_1_MIDI`: Despacho de eventos MIDI no idéntico (orden, timestamps o mensajes).
3. `FAIL_LEVEL_2_AUDIO`: Diferencia numérica o hash discordante en los buffers de audio.
4. `FAIL_LEVEL_3_DSP`: Desviación en el análisis acústico o cálculo de métricas.
5. `FAIL_LEVEL_3_EVAL`: Discrepancia en la evaluación o veredicto emitido.
6. `FAIL_LEVEL_3_EXPORT`: Diferencia en manifest, staging o empaquetado de producción.
