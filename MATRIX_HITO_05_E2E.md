# Matriz de Pruebas E2E de Laboratorio — HITO-05 (Revisada)

**Proyecto:** ABDAudioLab (Universal Black-Box Musical Hardware & Synth Profiler)  
**Documento:** MATRIX_HITO_05_E2E.md  
**Hito Asociado:** HITO-05-END-TO-END-LAB-CERTIFICATION  
**Versión:** 2.1.0  
**Fecha de Emisión:** 2026-09-20 (Revisión 2)  

---

## 1. Escenarios Principales (E2E-01 a E2E-04)

```
┌─────────┬───────────────────┬───────────────────────┬─────────────┬─────────────────┬───────────────────────────────┐
│ ID      │ Target            │ Control               │ Excitación  │ Dominio Audio   │ Artefactos Obligatorios       │
├─────────┼───────────────────┼───────────────────────┼─────────────┼─────────────────┼───────────────────────────────┤
│ E2E-01  │ Dexed VST3        │ VST3 Param / MIDI     │ Automática  │ Digital Offline │ ProductionPackage + Manifest  │
│ E2E-02  │ Hardware MIDI     │ MIDI DIN / USB        │ Automática  │ DAC → HW → ADC  │ ProductionPackage + Latencias │
│ E2E-03  │ Eurorack / Moog   │ Manual Prompt         │ Manual      │ DAC → HW → ADC  │ ProductionPackage + Logs Op.  │
│ E2E-04  │ Target Híbrido    │ MIDI + Panel Analógico│ Mixta       │ Audio Físico    │ ProductionPackage Dual-Origin │
└─────────┴───────────────────┴───────────────────────┴─────────────┴─────────────────┴───────────────────────────────┘
```

---

## 2. Desglose Atómico por Escenario

### 2.1. Escenario E2E-01: Dexed VST3 Automatizado (Digital Offline)

| Caso ID | Fase / Paso | Requisito / Comprobación | Criterio de Éxito Canónico | Estado |
|---|---|---|---|---|
| **E2E-01.1** | Paso 1 (Target) | Selección de Dexed VST3 en `catalogSelector`. | Inspección dinámica de UID, fabricante, versión y recuento de parámetros (`parameterCount > 0`). | **PASS** |
| **E2E-01.2** | Paso 1 (Capacidades) | Inspección de soporte MIDI y parámetros. | Consulta de `supportsMidiInput`; categorización de parámetros automatizables. | **PASS** |
| **E2E-01.3** | Paso 2 (Calibración) | Calibración digital ortogonal. | `digital.requirement = Required`, `verifyDigitalCalibration()` exitoso, `digital.verified = true`. | **PASS** |
| **E2E-01.4** | Paso 2 (Receta) | Generación de receta de excitación digital. | `recipe.status == RecipeStatus::Valid`, dominio acotado de notas/parámetros. | **PASS** |
| **E2E-01.5** | Paso 3 (Ejecución) | Render offline digital multihilo. | Render determinista; `physicalRoundTripLatencyMs = NotApplicable`, `pluginLatencySamples = reportado`, `bufferLatencySamples = calculado`. | **PASS** |
| **E2E-01.6** | Paso 3 (Evaluación) | Evaluación holdout contra modelo. | ESR < -30 dB, correlación $\rho > 0.999$, `SelectionStatus::Accepted`, hash canónico v2.0 generado. | **PASS** |
| **E2E-01.7** | Paso 4 (Persistencia) | Almacenamiento y recarga de sesión. | Round-trip semántico de `.abdlabtest` exitoso (manifest, puntos y hash idénticos); stepper transiciona a Paso 4. | **PASS** |
| **E2E-01.8** | Paso 4 (Exportación) | Generación de paquete de producción. | 4 artefactos obligatorios (`_lut.h`, `_telemetry.json`, `_Certification_Report.html`, `_manifest.json`), manifest con `AUTOMATED_VST_PARAM`. | **PASS** |

---

### 2.2. Escenario E2E-02: Hardware MIDI Automatizado (Loopback / ADC)

| Caso ID | Fase / Paso | Requisito / Comprobación | Criterio de Éxito Canónico | Estado |
|---|---|---|---|---|
| **E2E-02.1** | Paso 1 (Target) | Conexión y apertura de puerto MIDI hardware. | Puerto MIDI de salida y entrada realmente abierto e identificado; target activo único. | **PASS** |
| **E2E-02.2** | Paso 1 (Capacidades) | Verificación de protocolo y compuertas temporales. | `gateMs >= 200`, `settlingMs >= 50`, Panic en 16 canales probado en teardown. | **PASS** |
| **E2E-02.3** | Paso 2 (Calibración) | Calibración de latencia y jitter de ida y vuelta. | Medición de offset temporal (samples/ms), calibración de niveles ADC (-18 dBFS RMS). | **PASS** |
| **E2E-02.4** | Paso 2 (Receta) | Construcción de campaña de excitación MIDI. | `sequenceHash` calculado sobre eventos; inmutabilidad garantizada. | **PASS** |
| **E2E-02.5** | Paso 3 (Ejecución) | Ejecución secuenciada con captura de audio. | Secuencia monotónica, sin clipping (`< 0.99f`), Panic 16ch garantizado al finalizar o abortar. | **PASS** |
| **E2E-02.6** | Paso 3 (Evaluación) | Comparación espectral y evaluación de envolvente. | Métricas compensadas por latencia física; veredicto `SelectionStatus::Accepted` o `AcceptedWithWarnings`. | **PASS** |
| **E2E-02.7** | Paso 4 (Persistencia) | Persistencia con metadatos de hardware físico. | Round-trip semántico en `.abdlabtest` preserva `hardwareId`, `sequenceHash` y calibración. | **PASS** |
| **E2E-02.8** | Paso 4 (Exportación) | Exportación atómica 1-clic con procedencia MIDI. | `ProductionPackage` generado; manifest contiene `AUTOMATED_MIDI`, latencia declarada y checksums de fixity. | **PASS** |

---

### 2.3. Escenario E2E-03: Hardware Analógico Manual (Eurorack / Moog)

| Caso ID | Fase / Paso | Requisito / Comprobación | Criterio de Éxito Canónico | Estado |
|---|---|---|---|---|
| **E2E-03.1** | Paso 1 (Target) | Selección de módulo analógico en `catalogSelector`. | `TargetKind::HardwareAnalogue`, routing de audio asignado, cero puertos MIDI. | **PASS** |
| **E2E-03.2** | Paso 1 (Capacidades) | Modo de control de target manual. | `targetControlMode = TargetControlMode::NoDigitalControl`, `HardwareMethod::MANUAL_PROMPT`. | **PASS** |
| **E2E-03.3** | Paso 2 (Calibración) | Calibración analógica guiada para operador. | Nivel de ruido base verificado (<-70 dBFS), impedancia y niveles dentro de rango. | **PASS** |
| **E2E-03.4** | Paso 2 (Receta) | Receta descompuesta en pasos de perilla/switch. | Instrucciones de operador legibles (e.g. "Cutoff a 1 kHz, Resonancia al 50%"). | **PASS** |
| **E2E-03.5** | Paso 3 (Ejecución) | Flujo rítmico asistido con tarjetas de operador. | `WaitingForOperator` -> `confirmManualStep()` obligatorio -> settling -> captura ADC. Sin vía alternativa de disparo. | **PASS** |
| **E2E-03.6** | Paso 3 (Evaluación) | Evaluación con salvaguarda de confirmación. | Sin confirmación del operador, no se generan `MeasuredPoint` válidos y se bloquea `ExportReadiness`. | **PASS** |
| **E2E-03.7** | Paso 4 (Persistencia) | Persistencia de sesión con notas de laboratorio. | Round-trip semántico en `.abdlabtest` conserva `operatorNotes`, `operatorId`, temperatura y timestamps. | **PASS** |
| **E2E-03.8** | Paso 4 (Exportación) | Exportación atómica con manifest analógico. | `ProductionPackage` generado; manifest declara `MANUAL_EURORACK`, omite hashes MIDI y preserva `operatorNotes`. | **PASS** |

---

### 2.4. Escenario E2E-04: Target Híbrido (MIDI + Control Manual)

| Caso ID | Fase / Paso | Requisito / Comprobación | Criterio de Éxito Canónico | Estado |
|---|---|---|---|---|
| **E2E-04.1** | Paso 1 (Target) | Selección de sintetizador híbrido en `catalogSelector`. | `TargetKind::Hybrid`, puertos MIDI y canales de audio físico asignados simultáneamente. | **PASS** |
| **E2E-04.2** | Paso 1 (Capacidades) | Capacidades mixtas detectadas. | Control de notas por MIDI, control tímbrico por potenciómetros manuales de panel. | **PASS** |
| **E2E-04.3** | Paso 2 (Calibración) | Calibración dual ortogonal. | `audio.requirement = Required` y `midi.requirement = Required`; ambas verificadas antes de avanzar. | **PASS** |
| **E2E-04.4** | Paso 2 (Receta) | Receta combinada con eventos MIDI y pausas de perilla. | Eventos MIDI estructurados con prompts de perilla intercalados. | **PASS** |
| **E2E-04.5** | Paso 3 (Ejecución) | Coordinación por un único `ProfilingSequencer`. | Disparo automático de notas MIDI, pausa para ajuste manual con confirmación, reanudación y captura. Cero bucles paralelos. | **PASS** |
| **E2E-04.6** | Paso 3 (Evaluación) | Evaluación multivariante contra holdout híbrido. | Cálculo de ESR y correlación; hash canónico v2.0 vinculando procedencia MIDI y analógica. | **PASS** |
| **E2E-04.7** | Paso 4 (Persistencia) | Persistencia completa de estado dual. | Round-trip semántico en `.abdlabtest` almacena `sequenceHash` y `operatorNotes` simultáneamente. | **PASS** |
| **E2E-04.8** | Paso 4 (Exportación) | Paquete único con procedencia combinada (ADR-13). | Un solo exportador (`ReportExportService`); manifest `HYBRID` con secciones de procedencia combinadas. | **PASS** |

---

## 3. Casos Comunes Transversales (E2E-C01 a E2E-C10)

| Caso ID | Nombre / Foco | Regla No Negociable Comprobada | Estado |
|---|---|---|---|
| **E2E-C01** | Único Target Activo | Al seleccionar un nuevo target en `catalogSelector`, el target previo se desvincula instantáneamente. | **PASS** |
| **E2E-C02** | Calibration Readiness | El Stepper prohíbe avanzar a Paso 3 si la calibración requerida no está verificada (`isReadyForSession() == false`). | **PASS** |
| **E2E-C03** | Compatibilidad de Receta | Una receta incompatible invalida el botón de arranque (`recipe.status == RecipeStatus::IncompatibleWithTarget`). | **PASS** |
| **E2E-C04** | Snapshot Integral | `EvaluationSnapshot` contiene todos los metadatos obligatorios antes de permitir la exportación. | **PASS** |
| **E2E-C05** | Aprobación de Exportación | `ExportReadiness` valida veredicto (`SelectionStatus`), completitud y hash antes de habilitar la salida. | **PASS** |
| **E2E-C06** | Artefactos de Producción | Generación obligatoria del cuarteto canónico (`_lut.h`, `_telemetry.json`, `_Certification_Report.html`, `_manifest.json`) con fixity SHA-256 inyectada en el manifest. | **PASS** |
| **E2E-C07** | Procedencia en Manifest | El archivo `_manifest.json` reporta con total veracidad el modo operativo y las condiciones de laboratorio. | **PASS** |
| **E2E-C08** | Checksums Válidos | Todos los artefactos exportados poseen hashes SHA-256 de 64 caracteres hex que coinciden con su contenido real. | **PASS** |
| **E2E-C09** | Recarga de Sesión | La recarga de cualquier `.abdlabtest` (round-trip semántico) sitúa al usuario directamente en el Paso 4. | **PASS** |
| **E2E-C10** | Cero Archivos Huérfanos | Tras cualquier exportación (exitosa o abortada), no quedan directorios temporales `.staging_` ni `.backup_`. | **PASS** |

---

## 4. Casos Negativos y de Resiliencia (E2E-N01 a E2E-N10)

| Caso ID | Escenario de Error | Umbral o Regla Canónica | Comportamiento Esperado del Sistema | Estado |
|---|---|---|---|---|
| **E2E-N01** | Cambio de target durante sesión activa | Regla de único target | La sesión activa se cancela inmediatamente; se incrementa `controllerGeneration` y se resetea la calibración. | **PASS** |
| **E2E-N02** | Calibración incompatible o fallida | Guardas de Paso 2 | Si falla la calibración requerida, el Stepper bloquea el avance a Paso 3 con mensaje descriptivo. | **PASS** |
| **E2E-N03** | Receta corrupta o inválida | `RecipeStatus::InvalidParameters` | Receta con notas fuera de rango o parámetros inexistentes es rechazada y no se puede secuenciar. | **PASS** |
| **E2E-N04** | Cancelación anticipada de campaña | Incompletitud de datos | Si el usuario detiene la campaña a mitad de camino, `sessionStatus` es `Cancelled`; exportación bloqueada. | **PASS** |
| **E2E-N05** | Detección de Clipping en ADC | `LabAudioReceiver::clippingThreshold` (0.99f / ~ -0.1 dBFS) | Cualquier muestra $|x| \ge 0.99f$ marca la medición como saturada; se advierte al operador y no se aprueba el punto. | **PASS** |
| **E2E-N06** | Timeout o desconexión MIDI | Fallo de transmisión | Falta de respuesta aborta el ciclo de forma segura y emite Panic All-Notes-Off en 16 canales. | **PASS** |
| **E2E-N07** | Operador no confirma paso manual | `WaitingForOperator` sin confirmación | El secuenciador permanece en pausa segura; no se capturan datos y se bloquea `ExportReadiness`. | **PASS** |
| **E2E-N08** | Ausencia de señal física de audio | `LabAudioReceiver::silenceThresholdLinear` (0.0001f / -80 dBFS) | Nivel máximo de bloque bajo -80 dBFS genera alerta de cableado desconectado antes de iniciar la campaña. | **PASS** |
| **E2E-N09** | Plugin VST3 externo no instalado | Dependencia externa no detectada | El harness detecta la ausencia de `Dexed.vst3` y marca el test como `SKIPPED`, sin fallar la suite hermética. | **PASS** |
| **E2E-N10** | Adulteración de hash criptográfico | `canonicalEvaluationHash` alterado | Modificación del JSON tras el cálculo de hash dispara `HashMismatch` y bloquea la exportación. | **PASS** |

---

## 5. Matriz de Evidencia Requerida por Escenario

Para que un escenario se considere formalmente aceptado en el Acta final, su registro de ejecución debe capturar la siguiente telemetría estructurada:

```text
===============================================================================
PLANTILLA DE TELEMETRÍA DE LABORATORIO (POR ESCENARIO E2E)
===============================================================================
Escenario ID:                   [E2E-01 | E2E-02 | E2E-03 | E2E-04]
Commit Git:                     [Hash exacto SHA-1]
Configuración de Build:         [Release x64 MSVC]
Target Identifier:              [ID canónico de dispositivo]
Target Kind:                    [HardwareDigital | HardwareAnalogue | Hybrid | Vst3]
Control Mode:                   [AutomatedMidi | AutomatedSysEx | ManualOperator | AutomatedVstParameter]
Excitation Mode:                [Automated | Manual | Hybrid]

Parámetros de Entorno (Paso 0):
  - Audio Driver / Device:      [Nombre de dispositivo / WASAPI / ASIO / Mock]
  - Sample Rate / Block Size:   [e.g. 48000 Hz / 256 samples]
  - Latencia Base Medida:       [samples / ms]

Calibración (Paso 2):
  - Audio Verified:             [true / false / N/A]
  - MIDI Verified:              [true / false / N/A]
  - Digital Verified:           [true / false / N/A]
  - Recipe Status:              [Valid / IncompatibleWithTarget]

Ejecución y Metrología (Paso 3):
  - Puntos Medidos Totales:     [Total N]
  - SNR Medido / THD:           [dB / %]
  - ESR (dB):                   [e.g. -34.8 dB]
  - Correlación Espectral:      [e.g. 0.9994]
  - Veredicto de Selección:     [SelectionStatus::Accepted | AcceptedWithWarnings | Rejected]
  - Hash Canónico SHA-256:      [64 caracteres hex]

Exportación y Persistencia (Paso 4):
  - Manifest deviceType:        [AUTOMATED_VST_PARAM | AUTOMATED_MIDI | MANUAL_EURORACK | HYBRID]
  - ProductionPackage Artifacts:[_lut.h, _telemetry.json, _Certification_Report.html, _manifest.json]
  - Limpieza Staging:           [0 archivos residuales .staging_ o .backup_]
  - Sesión .abdlabtest:         [Round-trip semántico verificado; Stepper transiciona a Paso 4]
===============================================================================
```
