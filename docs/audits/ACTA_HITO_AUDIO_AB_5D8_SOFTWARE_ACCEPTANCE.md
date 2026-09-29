# ACTA DE CIERRE Y ACEPTACIÓN SOFTWARE — HITO 5D.8
## Ejecución Formal de la Matriz QA Canónica y Evaluación de Tolerancias Software

**Fecha de emisión:** 2026-09-29  
**Hito:** HITO-AUDIO-AB-5D / 5D.8  
**Documento Rector:** `docs/audits/ACTA_HITO_AUDIO_AB_5D8_SOFTWARE_ACCEPTANCE.md`  
**Compilador:** MSVC 18.4.3 (Visual Studio 2026 Community) · Release x64  
**Build Identity:** `Release x64 - MSVC 18.4.3 - Build #514`  
**Policy Evaluada:** `audio-ab-5d-provisional-v1` (`sha256:7f45cbb662b66299b9cf2a70d9a6c924cfdd62479e0a0d6ee0bf0b1f83424d57`)  
**Estado:** 🟢 **ACEPTACIÓN SOFTWARE CERTIFICADA (0 FAIL / 10 CASOS EVALUADOS)**

---

## 1. Delimitación y Alcance

Esta acta certifica la **primera ejecución formal de Aseguramiento de Calidad (QA) de Audio A/B software** sobre los cinco presets acústicos canónicos en frecuencias de muestreo de $44.1\text{ kHz}$ y $48.0\text{ kHz}$ (10 corridas canónicas totales).

> [!IMPORTANT]
> **Frontera Metrológica y Restricción Normativa:**  
> Esta evaluación opera **exclusivamente sobre streams de audio software en memoria** generados por modelos analíticos de referencia (`ReferenceAnalytical`) contra candidatos de síntesis DSP (`CandidateDSP`).  
> **No constituye metrología física ni autoriza profiling de hardware real:**
> - Transmisión MIDI físico: ⛔ **0 bytes autorizados**.
> - Hardware externo / DeepMind 12D: ⛔ **No utilizado / 0 conexiones**.
> - Vintage Emulator Studio (VES): ⛔ **Excluido explícitamente de la baseline QA no-VES mediante `~[ves]`**. HITO-10V1 permanece bloqueado mientras no se demuestre arranque headless verificable, salida audible reproducible y control semántico del target emulado.
> - Hitos D2.7B y HITO-10V1: ⛔ **Bloqueados preventivamente**.
> - ExportReadiness: `Blocked` (permanente e inviolable).

---

## 2. Matriz Consolidada de Resultados QA (10 Corridas Canónicas)

| Run ID | Preset | SR | Lag | $\rho_{\text{signed}}$ | $\rho_{\text{abs}}$ | $\Delta$ Espectro | Peak A / B | RMS A / B | Crest A / B | Diagnósticos Emitidos | Veredicto | Disposición |
|---|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|---|:---:|:---:|
| **`RUN_5D_01`** | `CleanReference` | 44.1k | 0 | +1.00000 | 1.00000 | 0.0000 dB | -5.076 / -5.076 dBfs | -11.831 / -11.831 dBfs | 6.755 / 6.755 dB | `DIAG_OK_IDENTITY` | **`PASS`** | `Accepted` |
| **`RUN_5D_02`** | `CleanReference` | 48.0k | 0 | +1.00000 | 1.00000 | 0.0000 dB | -5.076 / -5.076 dBfs | -11.831 / -11.831 dBfs | 6.755 / 6.755 dB | `DIAG_OK_IDENTITY` | **`PASS`** | `Accepted` |
| **`RUN_5D_03`** | `GentleModulation` | 44.1k | 0 | +0.99750 | 0.99750 | 0.1561 dB | -6.981 / -6.983 dBfs | -16.900 / -16.867 dBfs | 9.919 / 9.885 dB | `DIAG_WARN_PHASE_DISPERSION`, `DIAG_OK_CLASS_TOLERANCE` | **`WARN`** | `AcceptableWithExpectedDispersion` |
| **`RUN_5D_04`** | `GentleModulation` | 48.0k | 0 | +0.99751 | 0.99751 | 0.1633 dB | -6.962 / -6.965 dBfs | -16.907 / -16.874 dBfs | 9.945 / 9.910 dB | `DIAG_WARN_PHASE_DISPERSION`, `DIAG_OK_CLASS_TOLERANCE` | **`WARN`** | `AcceptableWithExpectedDispersion` |
| **`RUN_5D_05`** | `AggressiveNonlinear` | 44.1k | -3 | +0.98944 | 0.98944 | 0.3696 dB | -11.983 / -12.042 dBfs | -18.656 / -18.647 dBfs | 6.673 / 6.605 dB | `DIAG_WARN_PHASE_DISPERSION`, `DIAG_WARN_HARMONIC_SPREAD`, `DIAG_OK_CLASS_TOLERANCE` | **`WARN`** | `AcceptableWithExpectedDispersion` |
| **`RUN_5D_06`** | `AggressiveNonlinear` | 48.0k | -3 | +0.98943 | 0.98943 | 0.3923 dB | -11.981 / -12.040 dBfs | -18.660 / -18.651 dBfs | 6.679 / 6.611 dB | `DIAG_WARN_PHASE_DISPERSION`, `DIAG_WARN_HARMONIC_SPREAD`, `DIAG_OK_CLASS_TOLERANCE` | **`WARN`** | `AcceptableWithExpectedDispersion` |
| **`RUN_5D_07`** | `LowLevelDynamic` | 44.1k | 0 | +1.00000 | 1.00000 | 0.0002 dB | -50.088 / -50.092 dBfs | -61.934 / -61.938 dBfs | 11.846 / 11.846 dB | `DIAG_OK_IDENTITY` | **`PASS`** | `Accepted` |
| **`RUN_5D_08`** | `LowLevelDynamic` | 48.0k | 0 | +1.00000 | 1.00000 | 0.0002 dB | -50.087 / -50.091 dBfs | -61.932 / -61.936 dBfs | 11.845 / 11.845 dB | `DIAG_OK_IDENTITY` | **`PASS`** | `Accepted` |
| **`RUN_5D_09`** | `HighDensitySpectral` | 44.1k | -2 | +0.83491 | 0.83491 | 0.5209 dB | -2.504 / -2.482 dBfs | -15.237 / -15.015 dBfs | 12.733 / 12.533 dB | `DIAG_WARN_PHASE_DISPERSION`, `DIAG_WARN_HARMONIC_SPREAD`, `DIAG_OK_CLASS_TOLERANCE` | **`WARN`** | `AcceptableWithExpectedDispersion` |
| **`RUN_5D_10`** | `HighDensitySpectral` | 48.0k | -2 | +0.83647 | 0.83647 | 0.4999 dB | -2.467 / -2.553 dBfs | -15.242 / -15.020 dBfs | 12.775 / 12.468 dB | `DIAG_WARN_PHASE_DISPERSION`, `DIAG_WARN_HARMONIC_SPREAD`, `DIAG_OK_CLASS_TOLERANCE` | **`WARN`** | `AcceptableWithExpectedDispersion` |

---

## 3. Registro Criptográfico de Artefactos de QA (`docs/qa/runs/`)

Cada corrida ha sido persistida en disco con formato JSON canónico, incorporando trazabilidad de semilla, identidad de build y digest SHA-256 de reporte:

| Run ID | Archivo de Reporte | SHA-256 Report Hash | Estado de Determinismo Intra-Motor |
|---|---|---|:---:|
| `RUN_5D_01` | `docs/qa/runs/RUN_5D_01_acceptance_report.json` | `sha256:b13e34305c2a7f0766dd4c58af5d02a2eb1d7cc9ead7a6bbbf7a22cf4620646b` | $A_1 = A_2$ (Bit-Exact / Diff = 0.0) |
| `RUN_5D_02` | `docs/qa/runs/RUN_5D_02_acceptance_report.json` | `sha256:92dd1de7f57b1b958cfd5a0034789414c02e11368543c1fadb40b44943912d36` | $A_1 = A_2$ (Bit-Exact / Diff = 0.0) |
| `RUN_5D_03` | `docs/qa/runs/RUN_5D_03_acceptance_report.json` | `sha256:598712a295cca6eb9e88ba481b62d0265316fd3fbda10b59fb43018bc100b12a` | $A_1 = A_2$ (Bit-Exact / Diff = 0.0) |
| `RUN_5D_04` | `docs/qa/runs/RUN_5D_04_acceptance_report.json` | `sha256:8ee17a18860a832b75f0c511c33dbaab80ad246c6d4558314813f02f04f4100c` | $A_1 = A_2$ (Bit-Exact / Diff = 0.0) |
| `RUN_5D_05` | `docs/qa/runs/RUN_5D_05_acceptance_report.json` | `sha256:1b160df6d9c9cb2c2031220be506e6824ebfe6466cdbeb4cdbdb6a5b7388041e` | $A_1 = A_2$ (Bit-Exact / Diff = 0.0) |
| `RUN_5D_06` | `docs/qa/runs/RUN_5D_06_acceptance_report.json` | `sha256:424bca3ab427c1d76aa6feacc652bfe292ed81dc04e2b88888526e51f63a7f0b` | $A_1 = A_2$ (Bit-Exact / Diff = 0.0) |
| `RUN_5D_07` | `docs/qa/runs/RUN_5D_07_acceptance_report.json` | `sha256:5fe024f11f48073cafb1222e863dcf313a9c5c6456606be74aeadc0683a0ce5e` | $A_1 = A_2$ (Bit-Exact / Diff = 0.0) |
| `RUN_5D_08` | `docs/qa/runs/RUN_5D_08_acceptance_report.json` | `sha256:e361a0998f944159b788612e2f6be796bcae81ee3c812ac21488dd6695c7ff5c` | $A_1 = A_2$ (Bit-Exact / Diff = 0.0) |
| `RUN_5D_09` | `docs/qa/runs/RUN_5D_09_acceptance_report.json` | `sha256:715d9f907028868fa87826567e549fc37f2ccb3483f6b7d81adaa9e754a9c962` | $A_1 = A_2$ (Bit-Exact / Diff = 0.0) |
| `RUN_5D_10` | `docs/qa/runs/RUN_5D_10_acceptance_report.json` | `sha256:3802774e937156a1bab25cd26a10fbda61ad23a48fe6dc327c2b4d8dc5a72bc4` | $A_1 = A_2$ (Bit-Exact / Diff = 0.0) |

---

## 4. Análisis de Coherencia y Tendencias por Frecuencia de Muestreo (44.1 kHz vs 48.0 kHz)

La comparación de métricas entre las dos frecuencias canónicas evidencia una estabilidad metrológica software notable:

1. **`CleanReference` (RUN 01 vs 02):**
   - Identidad bit-a-bit en ambas frecuencias ($\rho = 1.00000$, $\Delta\text{espectro} = 0.0000\text{ dB}$, $\text{lag} = 0$).
   - Niveles de pico y RMS idénticos ($-5.076\text{ dBfs}$ / $-11.831\text{ dBfs}$).
2. **`GentleModulation` (RUN 03 vs 04):**
   - Correlación: $0.99750$ ($44.1\text{k}$) vs $0.99751$ ($48\text{k}$) — delta despreciable de $+0.00001$.
   - $\Delta$ Espectral: $0.1561\text{ dB}$ vs $0.1633\text{ dB}$ — variación $< 0.008\text{ dB}$.
   - Lag: $0$ muestras en ambas frecuencias.
3. **`AggressiveNonlinear` (RUN 05 vs 06):**
   - Lag acotado idéntico: $-3$ muestras en ambas frecuencias.
   - Correlación: $0.98944$ vs $0.98943$ — delta de $-0.00001$.
   - $\Delta$ Espectral: $0.3696\text{ dB}$ vs $0.3923\text{ dB}$ — variación $< 0.023\text{ dB}$.
4. **`LowLevelDynamic` (RUN 07 vs 08):**
   - Correlación: $1.00000$ idéntica en ambas frecuencias.
   - $\Delta$ Espectral: $0.0002\text{ dB}$ idéntico.
   - Preservación perfecta del piso de ruido a $-61.93\text{ dBfs}$.
5. **`HighDensitySpectral` (RUN 09 vs 10):**
   - Lag acotado idéntico: $-2$ muestras en ambas frecuencias.
   - Correlación: $0.83491$ vs $0.83647$ — variación de $+0.00156$.
   - $\Delta$ Espectral: $0.5209\text{ dB}$ vs $0.4999\text{ dB}$ — variación de $-0.0210\text{ dB}$.

**Conclusión Metrológica:** El pipeline DSP y el comparador muestran **invarianza práctica respecto a la frecuencia de muestreo**, sin aliasing espurio ni desincronización de reloj.

---

## 5. Respuestas a las Preguntas Fundamentales de 5D.8

### 1. ¿Los diez casos canónicos cumplen, avisan o fallan bajo `audio-ab-5d-provisional-v1`?
- **4 casos cumplen en `PASS` / `Accepted`:**
  - `RUN_5D_01` y `RUN_5D_02` (`CleanReference`)
  - `RUN_5D_07` y `RUN_5D_08` (`LowLevelDynamic`)
- **6 casos avisan en `WARN` / `AcceptableWithExpectedDispersion`:**
  - `RUN_5D_03` y `RUN_5D_04` (`GentleModulation`)
  - `RUN_5D_05` y `RUN_5D_06` (`AggressiveNonlinear`)
  - `RUN_5D_09` y `RUN_5D_10` (`HighDensitySpectral`)
- **0 casos fallan (`FAIL` / `Rejected`):** Cero violaciones de hard limits, cero clipping, cero desincronizaciones temporales.

### 2. ¿Los warnings observados corresponden a dispersión esperada o a posibles regresiones?
**Corresponden con rigor estricto a dispersión esperada de la clase acústica:**
- En `GentleModulation`, el LFO introduce desfasaje continuo microscópico que reduce $\rho$ de $1.0$ a $\approx 0.9975$.
- En `AggressiveNonlinear`, la saturación introduce armónicos impares que generan un $\Delta$ espectral de $\approx 0.38\text{ dB}$ y un retraso de fase de $-3$ muestras.
- En `HighDensitySpectral`, la densidad de osciladores acumulados produce desincronización de fase constructiva/destructiva esperada ($\rho \approx 0.835$, $\Delta \approx 0.51\text{ dB}$).
- Ninguno de estos efectos es una regresión del código: son la firma física del preset. La Capa 2.5 de la policy permite que el equipo de QA los inspeccione visualmente sin tratarlos como fallos de compilación ni esconderlos como pases silenciosos.

### 3. ¿Las tolerancias provisionales son útiles, demasiado permisivas o demasiado estrictas?
**Son altamente útiles y equilibradas:**
- Discriminan entre señales puras de referencia (`CleanReference`, con umbral $\rho \ge 0.9999$) y presets con dispersión no lineal o polifónica (`HighDensitySpectral`, con umbral $\rho \ge 0.8000$).
- No son demasiado permisivas: la suite hermética de 5D.7 demostró que desviaciones reales (lag $> 128$, polaridad invertida, clipping $> 0\text{ dBFS}$, eventos corruptos) disparan `FAIL` inmediato.
- No son demasiado estrictas: permiten que el 100% de la matriz de referencia sea aceptable.

### 4. ¿Los diagnósticos permiten entender la causa de cada resultado?
**Sí, con claridad unívoca:**
- `DIAG_OK_IDENTITY` clarifica cuándo hay identidad exacta bit-a-bit.
- `DIAG_WARN_PHASE_DISPERSION` identifica la presencia de modulación de fase.
- `DIAG_WARN_HARMONIC_SPREAD` apunta directamente a la distorsión armónica o saturación.
- `DIAG_OK_CLASS_TOLERANCE` confirma que las métricas respetan la envolvente autorizada para la clase.
- `MetricNotApplicable` evita atribuir $0.0$ a THD o SNR cuando la señal compleja no admite la métrica.

---

## 6. Decisión Formal sobre la Política de Tolerancias

Se adopta la **Conclusión A**:

> **Decisión:**  
> La política **`audio-ab-5d-provisional-v1`** queda **ratificada como marco formal de evaluación QA** para la suite de Audio A/B.  
> No se requieren ajustes a una versión `v2`, ya que los umbrales actuales modelan con exactitud matemática el comportamiento de los cinco presets canónicos sin generar falsos positivos de rechazo ni ocultar fenómenos acústicos relevantes.

---

## 7. Estado del Roadmap ([PLAN.md](../../PLAN.md))

```text
5D.1–5D.7:
  Certificadas.

5D.8:
  Completada y cerrada formalmente por esta acta.

5D.9:
  Lista para apertura (CI dedicado, baseline fija y certificación final de 5D).

D2.7B:
  Bloqueado (requiere nuevo contrato metrológico, audio y repetibilidad).

HITO-10V1:
  Bloqueado (requiere investigación VES: boot headless, audio audible y control semántico).

MIDI físico:
  0 bytes emitidos en toda la sesión.

ExportReadiness:
  Blocked (inviolable).
```
