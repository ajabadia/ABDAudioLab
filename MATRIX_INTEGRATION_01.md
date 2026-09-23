# Matriz de Paridad E2E de Audio y DSP — INTEGRATION-01

**Proyecto:** ABDAudioLab (Universal Black-Box Musical Hardware & Synth Profiler)  
**Documento:** MATRIX_INTEGRATION_01.md  
**Hito:** INTEGRATION-01  
**Versión:** 1.0.0  
**Fecha:** 2026-09-23  
**Estado:** CERTIFICADO (Build #417: 85/85 assertions PASS en INTEGRATION-01, 228.954 assertions suite global)  
**Referencia:** [PLAN_INTEGRATION_01.md](PLAN_INTEGRATION_01.md) | [MATRIX_PARITY_01.md](MATRIX_PARITY_01.md)

---

## 1. Niveles de Verificación de Paridad E2E

### Nivel 1: Control Exacto (Entrada y Secuencia)

| Dimensión | Parámetro Auditado | Criterio de Aceptación | Estado | Evidencia |
|---|---|---|:---:|---|
| **D1** | `TargetSelectionState` | Igualdad exacta (targetId, manufacturer, deterministic flag) | 🟢 **PASS** | TargetId="synthetic_fixture_integration01", deterministic=true idéntico en GS y LS |
| **D2** | `ExperimentPlan` / Receta | Misma nota C4, vel 64, gate 250ms, settling 50ms, 3 repeticiones | 🟢 **PASS** | 3 trials canónicos idénticos (C4, vel 0.5f, gate 250ms, settling 50ms) |
| **D2** | `sequenceHash` | Hash idéntico | 🟢 **PASS** | SequenceHash RFC 8785 determinista verificado |
| **D4** | Estado de calibración | Misma política digital/preflight satisfecha | 🟢 **PASS** | Digital headroom auto-trim verificado sin divergencias |
| **D5** | Eventos MIDI (NoteOn) | Conteo exacto, mismo número de nota y velocidad | 🟢 **PASS** | NoteOn ch=1, note=60, vel=0.50f idéntico en todas las repeticiones |
| **D5** | Eventos MIDI (NoteOff) | Conteo exacto, orden idéntico | 🟢 **PASS** | NoteOff ch=1, note=60, orden estricto preservado |
| **D5** | Despacho de bloques | Conteo exacto de bloques de 256 muestras procesados | 🟢 **PASS** | 452 bloques bombeados idénticos en ambas rutas (115.712 muestras) |

---

### Nivel 2: Audio Canónico (Fidelidad de Señal y Buffers)

| Dimensión | Parámetro Auditado | Criterio de Aceptación | Estado | Evidencia |
|---|---|---|:---:|---|
| **D6** | Canales y Sample Rate | 2 canales, 48.000 Hz exactos | 🟢 **PASS** | Estéreo (L/R) @ 48.000 Hz, bloque 256 |
| **D6** | Conteo total de muestras | Número idéntico de muestras capturadas en ambas rutas | 🟢 **PASS** | Canal L: 115.712 smp / Canal R: 115.712 smp en GS y LS |
| **D6** | Diferencia Máxima ($\max \|\Delta s\|$) | $\le 10^{-7}$ (bit-exacto en punto flotante) | 🟢 **PASS** | $\max \|\Delta s\| = 0.0000000$ (Bit-exacto IEEE-754) |
| **D6** | Error Cuadrático Medio ($\text{RMSE}$) | $\le 10^{-7}$ | 🟢 **PASS** | $\text{RMSE} = 0.0000000$ en ambos canales |
| **D6** | Huella Canónica SHA-256 | Hash SHA-256 idéntico sobre serialización canónica float32 LE | 🟢 **PASS** | Hash canónico SHA-256 idéntico bit a bit entre GS y LS |

---

### Nivel 3: Salida Científica y Metrología DSP

| Dimensión | Métrica Auditada | Tolerancia Admisible | Estado | Evidencia |
|---|---|---|:---:|---|
| **D7** | Nivel Pico y RMS | $\Delta \le 0.001\text{ dB}$ | 🟢 **PASS** | Mu/Sigma Peak ($\Delta = 0.0000000$) y RMS idénticos |
| **D7** | SNR (dB) | $\Delta \le 0.01\text{ dB}$ | 🟢 **PASS** | SNR idéntico en los 3 trials ($\Delta = 0.0000\text{ dB}$) |
| **D7** | THD (%) | $\Delta \le 0.001\%$ | 🟢 **PASS** | THD idéntico en los 3 trials ($\Delta = 0.00000\%$) |
| **D7** | `MeasuredPoint` vector | Mismo número de puntos e idénticos campos acústicos | 🟢 **PASS** | 3 puntos medidos idénticos campo a campo |
| **D3/D8**| `ExperimentKind` | Clasificación formal idéntica independiente de UI | 🟢 **PASS** | Ambos clasificados como `Measurement` (HITO-CONVERGENCIA-01) |

---

## 2. Diagnóstico de Primer Punto de Falla (Failure Localization)

En caso de cualquier diferencia durante la ejecución de la prueba, el harness identificará de inmediato la etapa de bifurcación:

1. `FAIL_LEVEL_1_PLAN`: Discrepancia en la receta o parámetros iniciales.
2. `FAIL_LEVEL_1_MIDI`: Despacho de eventos MIDI no idéntico (orden, timestamps o mensajes).
3. `FAIL_LEVEL_2_AUDIO`: Diferencia numérica o hash discordante en los buffers de audio.
4. `FAIL_LEVEL_3_DSP`: Desviación en el análisis acústico o cálculo de métricas.
5. `FAIL_LEVEL_3_EVAL`: Discrepancia en la evaluación o veredicto emitido.
6. `FAIL_LEVEL_3_EXPORT`: Diferencia en manifest, staging o empaquetado de producción.
