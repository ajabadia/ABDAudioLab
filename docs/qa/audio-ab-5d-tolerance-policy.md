# Política Provisional de Tolerancias Audio A/B por Clase de Preset (Fase 5D)

**Documento:** `docs/qa/audio-ab-5d-tolerance-policy.md`  
**Identificador de Policy:** `audio-ab-5d-provisional-v1`  
**Estado:** Provisional — Basado en Evidencia Experimental (Corridas RUN_5D_01 a RUN_5D_10)  
**Fecha de Entrada en Vigor:** 2026-09-29  
**Revisión Obligatoria:** Previa a la emisión del Acta de Aceptación (Fase 5D.8)  
**Axioma Rector:** *«La policy de tolerancias separa la variación esperable de la clase acústica de una regresión o fallo estructural; los invariantes de integridad de render nunca se relajan bajo tolerancias de preset.»*

---

## 1. Delimitación y Alcance de la Calibración

1. **Carácter Provisional:** Esta calibración se deriva empíricamente de la matriz inicial de 10 corridas software herméticas ejecutadas en el Build #513. No constituye una norma inmutable ni certifica equivalencia universal; establece el marco formal para evaluar si un candidato DSP se comporta dentro de la envolvente acústica esperada.
2. **Entorno Estrictamente Hermético:** Cero interacción con hardware físico, cero bytes MIDI transmitidos, D2.7B y HITO-10V1 permanentemente bloqueados.

---

## 2. Jerarquía de Evaluación y Prioridad de Veredictos

La evaluación de veredicto sigue un orden de precedencia estricto e inviolable:

$$\text{Hard FAIL} \succ \text{Class FAIL} \succ \text{WARN} \succ \text{PASS}$$

1. Si se incumple cualquier **Hard Limit** (invariante estructural), el veredicto es **`FAIL` inmediato e incondicional**, independientemente de las métricas de correlación o espectro.
2. Si se respetan los Hard Limits, se evalúan las **Tolerancias de Clase**:
   - Si todas las métricas aplicables satisfacen los umbrales de **`PASS`**, el veredicto es **`PASS`**.
   - Si alguna métrica excede el umbral de `PASS` pero todas se mantienen dentro de los umbrales de **`WARN`**, el veredicto es **`WARN`** con diagnóstico contextual documentado.
   - Si cualquier métrica excede el umbral de `WARN`, el veredicto es **`FAIL`** (Class FAIL).

---

## 3. Capa 1: Límites Duros de Integridad (Hard Limits)

Condiciones estructurales no negociables que producen `FAIL` inmediato para todas las clases:

| Condición | Razón Técnica | Diagnóstico Emitido |
|---|---|---|
| $A_1 \neq A_2$ (Intra-motor) | El motor de renderizado no es determinista ante la misma semilla y reset. | `DIAG_FAIL_NONDETERMINISTIC_RENDER` |
| $|\text{Lag}| > 128\text{ muestras}$ | Desalineación temporal inaceptable que supera la ventana autorizada. | `DIAG_FAIL_TEMPORAL_DESYNC` |
| $\text{NoteOff} \le \text{NoteOn}$ | Secuencia o control de eventos MIDI corrupto / violación de orden. | `DIAG_FAIL_TEMPORAL_DESYNC` |
| $\text{Unexpected Clipping}$ | El motor B produce muestras que superan $0.0\text{ dBFS}$ ($|y| > 1.0$) no presentes en A. | `DIAG_FAIL_UNEXPECTED_CLIPPING` |
| Warm-up no silencioso | Ruido residual o falta de reposo en filtros previo al NoteOn. | `DIAG_FAIL_TEMPORAL_DESYNC` |
| Colapso de Envolvente | Señal B colapsa a silencio ($< -120\text{ dBFS}$) mientras A tiene energía activa. | `DIAG_FAIL_ENVELOPE_COLLAPSE` |

---

## 4. Semántica y Convención de Correlación Cruzada

Para evitar ambigüedades matemáticas con señales invertidas de fase ($A = -B$), el sistema calcula y registra de forma explícita dos métricas complementarias:

1. **`peakSignedCrossCorrelation` ($\rho_{\text{signed}} \in [-1.0, 1.0]$):**  
   Correlación cruzada normalizada clásica con signo. Una inversión de fase de $180^\circ$ produce $\rho_{\text{signed}} \approx -1.0$.
2. **`peakAbsoluteCrossCorrelation` ($\rho_{\text{abs}} = |\rho_{\text{signed}}| \in [0.0, 1.0]$):**  
   Magnitud de la correlación para análisis de similitud morfológica general.

> [!IMPORTANT]
> **Norma de Aceptación:**  
> Las políticas de tolerancia de la Fase 5D utilizan como criterio rector **`peakSignedCrossCorrelation`**. Una señal con fase invertida producirá `FAIL` en clases de referencia directa (`CleanReference`), garantizando que la polaridad del audio quede rigurosamente preservada.

---

## 5. Capa 2: Matriz Provisional de Tolerancias por Clase (`audio-ab-5d-provisional-v1`)

### 5.1 Tabla de Umbrales Formales

| Clase Acústica | Correlación PASS ($\ge$) | Correlación WARN ($\ge$) | $\Delta$ Espectral PASS ($\le$) | $\Delta$ Espectral WARN ($\le$) | $\Delta$ RMS PASS ($\le$) | $\Delta$ RMS WARN ($\le$) | Lag Máx. PASS ($\le$) | Lag Máx. WARN ($\le$) |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **`CleanReference`** | `0.9999` | `0.9990` | `0.01 dB` | `0.05 dB` | `0.05 dB` | `0.10 dB` | `0` | `1` |
| **`GentleModulation`** | `0.9950` | `0.9900` | `0.25 dB` | `0.50 dB` | `0.20 dB` | `0.40 dB` | `1` | `4` |
| **`AggressiveNonlinear`** | `0.9850` | `0.9750` | `0.60 dB` | `1.00 dB` | `0.30 dB` | `0.60 dB` | `4` | `16` |
| **`LowLevelDynamic`** | `0.9995` | `0.9990` | `0.01 dB` | `0.05 dB` | `0.05 dB` | `0.10 dB` | `0` | `1` |
| **`HighDensitySpectral`** | `0.8000` | `0.7500` | `0.75 dB` | `1.25 dB` | `0.50 dB` | `1.00 dB` | `4` | `16` |

### 5.2 Tratamiento de Métricas No Aplicables (`MetricNotApplicable`)
- Métricas como **THD** y **SNR** solo son obligatorias cuando la excitación del preset permite un cálculo acústicamente fundamentado (e.g. THD en `CleanReference`, SNR en `LowLevelDynamic`).
- Cuando una métrica no es aplicable, se registra explícitamente como `std::nullopt` (`"MetricNotApplicable"` en JSON) y **se excluye de la evaluación de gates**, impidiendo que un valor falso como $0.0$ distorsione el veredicto.

---

## 6. Validación contra la Matriz Canónica Inicial (5D.4)

| Corrida ID | Preset | Correlación Obs. | $\Delta$ Espectral Obs. | Lag Obs. | $\Delta$ RMS Obs. | Veredicto Resultante | Diagnósticos Emitidos |
|---|---|:---:|:---:|:---:|:---:|:---:|---|
| `RUN_5D_01` / `02` | `CleanReference` | `1.00000` | `0.0000 dB` | `0` | `0.000 dB` | **`PASS`** | `DIAG_OK_IDENTITY` |
| `RUN_5D_03` / `04` | `GentleModulation` | `0.99751` | `0.1633 dB` | `0` | `0.033 dB` | **`PASS`** | `DIAG_WARN_PHASE_DISPERSION`, `DIAG_OK_CLASS_TOLERANCE` |
| `RUN_5D_05` / `06` | `AggressiveNonlinear` | `0.98944` | `0.3923 dB` | `-3` | `0.009 dB` | **`PASS`** | `DIAG_WARN_HARMONIC_SPREAD`, `DIAG_OK_CLASS_TOLERANCE` |
| `RUN_5D_07` / `08` | `LowLevelDynamic` | `1.00000` | `0.0002 dB` | `0` | `0.004 dB` | **`PASS`** | `DIAG_OK_IDENTITY` |
| `RUN_5D_09` / `10` | `HighDensitySpectral` | `0.83647` | `0.5209 dB` | `-2` | `0.222 dB` | **`PASS`** | `DIAG_WARN_PHASE_DISPERSION`, `DIAG_WARN_HARMONIC_SPREAD`, `DIAG_OK_CLASS_TOLERANCE` |

*Resultado:* El 100% de las 10 corridas de la línea base 5D.4 resuelve en **`PASS`**, confirmando que los umbrales provisionales ofrecen una caracterización ajustada sin generar falsos positivos de rechazo.
