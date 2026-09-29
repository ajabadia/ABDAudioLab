# Especificación de Contrato de Aceptación Audio A/B (Fase 5D)

**Documento:** `docs/qa/audio-ab-5d-acceptance-spec.md`  
**Hito Rector:** `HITO-AUDIO-AB-5D: Acceptance Matrix and Canonical Preset Validation`  
**Estado:** V1.0 — Congelado para Diseño y Especificación  
**Fecha:** 2026-09-29  
**Axioma Rector:** *«Una comparación A/B no certifica equivalencia porque dos renders "suenen parecidos"; debe declarar la excitación, congelar el contexto de render, medir diferencias reproducibles y emitir un veredicto trazable con tolerancias apropiadas al comportamiento esperado del preset.»*

---

## 1. Delimitación de Autoridad y Fronteras de Seguridad

1. **Aislamiento Absoluto de Hardware:**
   - La Fase 5D opera **exclusivamente en memoria** y sobre streams de audio sintetizados por software.
   - **0 bytes MIDI transmitidos** a través de puertos del sistema operativo.
   - Ningún componente de la suite 5D interactúa con sintetizadores físicos (`DeepMind12D`, `PRO-800`, `DX7`, etc.).
2. **Bloqueos Normativos Vigentes:**
   - `D2.7B` (Banco Físico Metrológico) permanece estrictamente **BLOQUEADO**.
   - `HITO-10V1` (Emulación VES / CZ-101 Headless) permanece estrictamente **BLOQUEADO**.
   - `ExportReadiness::Blocked` permanente: los resultados de QA acústico A/B no constituyen ni habilitan exportaciones metrológicas de hardware.
3. **Propósito Estricto de QA:**
   - Establecer un marco formal de regresión, caracterización acústica y validación entre motores DSP (e.g. referencia vs implementación, float32 vs fixed/LUT, mono vs dispersión estocástica multivoz).

---

## 2. Contrato Formal de la Corrida A/B (`AudioABRunContract`)

Cada ejecución A/B debe estar identificada por un contexto declarativo inmutable compuesto por los siguientes campos normativos:

### 2.1 Identificadores de Corrida y Presets
- `runId` (`std::string`): UUIDv4 o digest canónico identificando unívocamente la corrida.
- `presetId` (`std::string`): Identificador normativo del preset evaluado (e.g. `PRESET_CLEAN_REF_01`).
- `presetClass` (`std::string`): Clase acústica formal del preset:
  - `CleanReference`
  - `GentleModulation`
  - `AggressiveNonlinear`
  - `LowLevelDynamic`
  - `HighDensitySpectral`
- `presetDocumentHash` (`std::string`): Hash SHA-256 de los parámetros normalizados del preset.

### 2.2 Contexto de Renderizado DSP
- `engineA` (`std::string`): Nombre y versión del motor o rama bajo prueba A (e.g. `ReferenceAnalyticalEngine_v1.0`).
- `engineB` (`std::string`): Nombre y versión del motor o rama bajo prueba B (e.g. `JunoDivergenceDSP_v2.1`).
- `buildIdentity` (`std::string`): Identificador de compilación del runner (e.g. `ABDAudioLab_Build_512_Release_x64`).
- `sampleRate` (`double`): Frecuencia de muestreo (canónicos iniciales: `44100.0` y `48000.0` Hz).
- `blockSize` (`int`): Tamaño de bloque de procesamiento en muestras (canónicos: `64`, `256`, `512`).
- `channelLayout` (`std::string`): Disposición de canales (canónicos: `Mono` o `StereoInterleaved`).
- `deterministicSeed` (`uint64_t`): Semilla pseudoaleatoria explícita para generadores de ruido o modelos de dispersión de componentes.

### 2.3 Protocolo Temporal de Excitación
- `renderLengthSamples` (`size_t`): Longitud total del buffer renderizado en muestras.
- `warmupSamples` (`size_t`): Muestras iniciales de estabilización térmica / filtros previas al evento musical (descartadas para métricas de correlación transitoria).
- `tailSamples` (`size_t`): Muestras de decaimiento / cola acústica tras el NoteOff para medir release y piso de ruido.
- `alignmentPolicy` (`std::string`): Política de sincronización temporal autorizada (ver Sección 4).

### 2.4 Vector de Métricas Multidimensionales
- `temporalMetrics`:
  - `lagOffsetSamples` (`int`): Desplazamiento temporal detectado por correlación cruzada máxima.
  - `maxCrossCorrelation` (`double`): Coeficiente de correlación normalizada en el punto de alineamiento óptimo ($\in [-1.0, 1.0]$).
  - `envelopeRmsDiffDb` (`double`): Diferencia RMS de envolvente calculada en ventanas de 10 ms.
- `amplitudeMetrics`:
  - `rmsA_dBfs` / `rmsB_dBfs` (`float`): Nivel RMS en dBFS de cada motor.
  - `peakA_dBfs` / `peakB_dBfs` (`float`): Pico absoluto en dBFS de cada motor.
  - `crestFactorA` / `crestFactorB` (`double`): Relación pico-a-RMS.
  - `clippingObservedA` / `clippingObservedB` (`bool`): Detección de rebase $\ge 0.0\text{ dBFS}$ ($|y| \ge 1.0$).
- `spectralMetrics`:
  - `meanSpectralDiffDb` (`double`): Diferencia espectral media FFT en dB en la banda pasante útil.
  - `spectralCentroidDiffHz` (`double`): Divergencia en centroide espectral.
  - `spectralRollOffDiffHz` (`double`): Divergencia en frecuencia de corte al 85% de energía.
  - `thdPercentA` / `thdPercentB` (`double`): Distorsión armónica total sobre estímulos tonales.
  - `snrDbA` / `snrDbB` (`double`): Relación señal-ruido observada en reposo.
- `eventMetrics`:
  - `eventOrderPreserved` (`bool`): Garantía de que NoteOn precede a NoteOff sin intercalación de eventos espurios.
  - `activeVoiceCountParity` (`bool`): Conteo idéntico de voces activadas.

---

## 3. Veredictos y Códigos Diagnósticos Tipados

El motor de decisión emite uno de tres veredictos formales:
- `PASS`: La divergencia acústica se encuentra estrictamente dentro de los límites admisibles para la clase del preset.
- `WARN`: Existe divergencia perceptible o medible justificada por la naturaleza del preset (e.g. fase en modulación BBD, o armónicos superiores en no linealidades controladas) sin pérdida de estabilidad estructural ni clipping.
- `FAIL`: Ruptura estructural, clipping inesperado, pérdida de envolvente, divergencia tonal inaceptable o fallo de eventos.

### Códigos de Diagnóstico Estandarizados

| Código | Veredicto Mínimo | Causa Raíz / Significado |
|---|:---:|---|
| `DIAG_OK_IDENTITY` | `PASS` | Señales bit-a-bit o idénticas dentro de la precisión de punto flotante ($\text{Corr} \ge 0.9999$). |
| `DIAG_OK_CLASS_TOLERANCE` | `PASS` | Señales coherentes dentro del margen calibrado de la clase acústica. |
| `DIAG_WARN_PHASE_DISPERSION` | `WARN` | Correlación temporal atenuada por modulación de retardo (LFO/BBD) con energía global y envolvente preservadas. |
| `DIAG_WARN_HARMONIC_SPREAD` | `WARN` | Aparición de armónicos no lineales dentro de los márgenes dinámicos admisibles. |
| `DIAG_FAIL_UNEXPECTED_CLIPPING` | `FAIL` | Una o ambas señales superan $0.0\text{ dBFS}$ sin estar documentado en el preset. |
| `DIAG_FAIL_TEMPORAL_DESYNC` | `FAIL` | El desalineamiento temporal excede la ventana máxima autorizada ($\Delta t > \tau_{\max}$). |
| `DIAG_FAIL_ENVELOPE_COLLAPSE` | `FAIL` | Caída de envolvente o nivel RMS divergente más allá de la tolerancia ($\Delta\text{RMS} > \text{Tol}_{\text{RMS}}$). |
| `DIAG_FAIL_PITCH_DIVERGENCE` | `FAIL` | Desplazamiento tonal fundamental o inestabilidad espectral grave. |
| `DIAG_FAIL_EVENT_ORDER` | `FAIL` | Pérdida de sincronización de eventos MIDI NoteOn/NoteOff. |
| `DIAG_FAIL_DENORMAL_OR_SILENCE` | `FAIL` | Detección de silenciamiento inesperado, NaN, Inf o presencia de números subnormales. |

---

## 4. Regla de Oro sobre la Alineación Temporal

> [!CRITICAL]
> **Ventana Estricta de Alineación:**  
> La alineación temporal mediante correlación cruzada **solo está autorizada para compensar una latencia fija o buffer jitter de procesamiento dentro de un límite estricto de $\pm 128$ muestras**.  
> **Queda estrictamente prohibido usar la alineación para enmascarar:**
> - Envolventes de ataque o decaimiento divergentes.
> - Duraciones erróneas de notas.
> - Pérdida de eventos NoteOff.
> - Deriva progresiva de reloj (*clock drift*).
> - Clipping o saturación.

---

## 5. Schema JSON de Evidencia (`audio_ab_acceptance_result.schema.json`)

Los resultados exportados de las corridas herméticas de QA se ajustan al siguiente contrato tipado en JSON:

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "title": "AudioABAcceptanceResult",
  "type": "object",
  "required": [
    "runId",
    "presetId",
    "presetClass",
    "engineA",
    "engineB",
    "sampleRate",
    "blockSize",
    "verdict",
    "diagnosticCode",
    "metrics",
    "artifactHashes"
  ],
  "properties": {
    "runId": { "type": "string" },
    "presetId": { "type": "string" },
    "presetClass": { "type": "string", "enum": ["CleanReference", "GentleModulation", "AggressiveNonlinear", "LowLevelDynamic", "HighDensitySpectral"] },
    "engineA": { "type": "string" },
    "engineB": { "type": "string" },
    "sampleRate": { "type": "number" },
    "blockSize": { "type": "integer" },
    "verdict": { "type": "string", "enum": ["PASS", "WARN", "FAIL"] },
    "diagnosticCode": { "type": "string" },
    "metrics": {
      "type": "object",
      "required": ["maxCrossCorrelation", "rmsDiffDb", "meanSpectralDiffDb", "lagOffsetSamples"],
      "properties": {
        "maxCrossCorrelation": { "type": "number" },
        "rmsDiffDb": { "type": "number" },
        "meanSpectralDiffDb": { "type": "number" },
        "lagOffsetSamples": { "type": "integer" },
        "peakDiffDb": { "type": "number" },
        "thdDiffPercent": { "type": "number" }
      }
    },
    "artifactHashes": {
      "type": "object",
      "required": ["renderHashA", "renderHashB"],
      "properties": {
        "renderHashA": { "type": "string" },
        "renderHashB": { "type": "string" }
      }
    }
  }
}
```
