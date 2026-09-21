# Plan de Certificación E2E de Laboratorio — HITO-05 (Revisado)

**Proyecto:** ABDAudioLab (Universal Black-Box Musical Hardware & Synth Profiler)  
**Hito:** HITO-05-END-TO-END-LAB-CERTIFICATION  
**Versión Base:** v2.1.0 (BuildVersion 353, commit `cfb6a8f`)  
**Responsable de Arquitectura:** Antigravity (Lead Architect & Planner)  
**Fecha de Emisión:** 2026-09-20 (Revisión 2)  
**Estado:** Aprobado para Planificación con Ajustes Normativos  

---

## 1. Misión y Alcance del Hito 5

El **HITO-05** tiene como objetivo certificar el flujo metrológico completo e ininterrumpido de ABDAudioLab a través de los **cinco pasos del Stepper canónico (0 a 4)**:

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                             FLUJO STEPPER CANÓNICO (0..4)                        │
│                                                                                  │
│   Paso 0: Studio Environment                                                     │
│     │     (Dispositivos de audio, buffer, sample rate, niveles base)             │
│     ▼                                                                            │
│   Paso 1: Target & Routing                                                       │
│     │     (Selección de target único vía catalogSelector, inspección de puertos) │
│     ▼                                                                            │
│   Paso 2: Calibration & Setup                                                    │
│     │     (Calibración ortogonal: Audio, MIDI, Digital; generación de receta)    │
│     ▼                                                                            │
│   Paso 3: Run Session                                                            │
│     │     (Ejecución de campaña: automatizada digital / MIDI o manual operador)  │
│     ▼                                                                            │
│   Paso 4: Export & Report                                                        │
│           (Inspección de métricas, audición A/B, exportación atómica, .abdlabtest)  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

Esta certificación se realiza sobre **cuatro escenarios representativos** de la física y control del laboratorio musical:
1. **E2E-01**: **Dexed VST3 Automatizado** (Plugin VST3 + automatización de parámetros/MIDI -> render digital offline -> paquete + HTML).
2. **E2E-02**: **Hardware MIDI Automatizado** (Sintetizador digital/híbrido hardware -> MIDI automatizado con gates -> ADC -> paquete + latencia).
3. **E2E-03**: **Hardware Analógico Manual** (Eurorack/Moog sin control digital -> prompts de operador -> confirmación -> ADC -> paquete + log operador).
4. **E2E-04**: **Target Híbrido** (Sintetizador analógico controlado por MIDI -> notas automáticas + ajustes manuales de corte/resonancia -> paquete + procedencia dual).

---

## 2. Contabilidad Rigurosa de Suites y Pruebas

Para evitar mezclar estados cualitativos dispares en una única cifra, el cómputo de pruebas se desglosa formalmente en:

```text
===============================================================================
CONTABILIDAD RIGUROSA DE PRUEBAS - HITO-05
===============================================================================
1. Suite Base Certificada (Hitos 1..4):
   - 605 test cases / 228.536 aserciones (100% PASS, commit cfb6a8f / c4614ac).
   - Inmutable: actúa como barrera de regresión de toda la plataforma.

2. Suite E2E Hermética (Nuevos Tests):
   - Contabilizada de forma independiente (E2E-H01 en adelante).
   - Basada en mocks controlados y fixtures sintéticos en memoria.
   - Ejecución obligatoria en CI: siempre PASS (0 fallos tolerados).

3. Fixture VST3 Controlado:
   - SyntheticVst3 / mock de plugin con estado binario congelado y SHA-256 conocido.
   - Valida el ciclo completo de hosting sin requerir software comercial de terceros.

4. Suite de Integración Real (Dependencias Externas):
   - Dexed.vst3 instalado en el host y hardware físico (MIDI DIN/USB, interfaces ADC/DAC).
   - Estados posibles por escenario:
       * PASS: Dependencia presente y flujo completado con éxito.
       * SKIPPED: Dependencia externa ausente en host (justificado explícitamente).
       * FAIL: Dependencia presente pero falla la ejecución o el contrato.
   - SKIPPED NUNCA se contabiliza como PASS ni como CERTIFIED.
   - El hito se considerará "Parcialmente Certificado" si existen escenarios físicos en SKIPPED.
===============================================================================
```

---

## 3. Nomenclatura Canónica de Artefactos de Exportación (ADR-13)

La salida de `ReportExportService::exportReport` con `includeProductionPackage = true` genera de forma atómica el paquete estándar de producción:

### 3.1. Artefactos Obligatorios del `ProductionPackage`
1. **Cabecera C++ de Modelo / LUT**: `[BaseName]_lut.h` (`ReportArtifactKind::CppLutHeader`)
   - Tabla de búsqueda optimizada SIMD o código de inferencia en C++ autónomo.
2. **Telemetría de Sesión JSON**: `[BaseName]_telemetry.json` (`ReportArtifactKind::TelemetryJson`)
   - Registro estadístico de SNR, THD, ruido base, latencias y puntos medidos.
3. **Certificado Formal de Calibración**: `[BaseName]_Certification_Report.html` (`ReportArtifactKind::CertificationHtml`)
   - Informe visual autocontenido con metrología, condiciones de laboratorio y veredicto.
4. **Manifiesto Canónico con Fixity Criptográfica**: `[BaseName]_manifest.json` (`ReportArtifactKind::SessionManifestJson`)
   - Manifiesto v2.0 que incluye en `packageArtifacts` los registros de fixity (nombre, tamaño en bytes y hash SHA-256) de todos los archivos del paquete, más el hash global del manifiesto (`manifestSha256`).

### 3.2. Metadatos Específicos por Modo en el Manifiesto
- **Modo Manual Analógico (`MANUAL_EURORACK`)**:
  - `hardware.deviceType = "MANUAL_EURORACK"`
  - `laboratoryConditions.operatorNotes` (instrucciones, confirmaciones del operador, temperatura ambiente).
  - Omite campos de secuencia MIDI irrelevantes.
- **Modo Automatizado MIDI (`AUTOMATED_MIDI` / `AUTOMATED_SYSEX`)**:
  - `hardware.deviceType = "AUTOMATED_MIDI"` o `"AUTOMATED_SYSEX"`
  - `sequenceHash` SHA-256 de los eventos MIDI programados.
  - Telemetría de compuertas temporales (`gateMs`, `settlingMs`) y latencia de ida y vuelta.
- **Modo Híbrido (`HYBRID`)**:
  - `hardware.deviceType = "HYBRID"`
  - Procedencia dual: contiene simultáneamente `sequenceHash` y `laboratoryConditions.operatorNotes`.

### 3.3. Artefactos Condicionales / Opcionales
- `checksums.sha256`: Archivo digest complementario generado a solicitud del operador.
- `Target_Audio.wav` / `Model_Audio.wav` / `Residual_Audio.wav`: Capturas PCM de audición A/B si `includeAuditionData = true`.
- Datasets de entrenamiento NAM o matrices LUT multidimensionales extendidas.

---

## 4. Normalización de Enums y Contratos Canónicos del Código

Para eliminar cualquier ambigüedad terminológica entre la documentación y el código fuente:

| Concepto Metrológico | Enum Canónico en C++ | Archivo Fuente | Valores Válidos |
|---|---|---|---|
| **Auditoría de Target** | `synth::ApprovalStatus` | [TargetAuditor.h](file:///d:/desarrollos/ABDSynths/ABDAudioLab/src/synth/TargetAuditor.h) | `Approved`, `ApprovedWithWarnings`, `Rejected`, `Unsupported` |
| **Selección de Modelo** | `synth::SelectionStatus` | [ModelEvaluationTypes.h](file:///d:/desarrollos/ABDSynths/ABDAudioLab/src/synth/ModelEvaluationTypes.h) | `Accepted`, `AcceptedWithWarnings`, `Inconclusive`, `Rejected`, `InvalidMeasurement` |
| **Veredicto UI Resumen** | `core::ValidationUiSummary::Verdict` | [EvaluationContracts.h](file:///d:/desarrollos/ABDSynths/ABDAudioLab/src/gui/session/EvaluationContracts.h) | `pass`, `warn`, `fail`, `inconclusive` |
| **Paso de Stepper** | `gui::WorkflowStepperBar::Step` | [WorkflowStepperBar.h](file:///d:/desarrollos/ABDSynths/ABDAudioLab/src/gui/WorkflowStepperBar.h) | `StudioEnvironment` (0), `HardwareRouting` (1), `Calibration` (2), `RunSession` (3), `ExportReport` (4) |
| **Estado de Receta** | `gui::session::RecipeStatus` | [ProfilingSessionContracts.h](file:///d:/desarrollos/ABDSynths/ABDAudioLab/src/gui/session/ProfilingSessionContracts.h) | `Valid`, `IncompatibleWithTarget`, `InvalidParameters`, `NotGenerated` |

---

## 5. Definición de "Round-Trip Semántico" para `.abdlabtest`

El contenedor `.abdlabtest` es un paquete ZIP (`juce::ZipFile`) que encapsula archivos JSON y binarios:
- Debido a timestamps de compresión del sistema de archivos o reordenación de claves JSON al serializar, los bytes del contenedor ZIP pueden variar entre escrituras sucesivas.
- Por tanto, la verificación de recarga se define como **Round-Trip Semántico**:
  1. Identidad estricta de `SessionManifest` (`hardwareId`, `targetModule`, `operatorNotes`).
  2. Número idéntico y contenido equivalente de todos los `MeasuredPoint` (SNR, THD, frecuencias, niveles).
  3. Preservación bit-exacta del `canonicalEvaluationHash` SHA-256.
  4. Transición idéntica del Stepper hacia `Step::ExportReport`.
- El término **bit-exact** se reserva exclusivamente para buffers PCM de audio congelados y hashes criptográficos de fixity.

---

## 6. Tratamiento de Latencias y Capacidades de Dexed

1. **Parámetros de Dexed**:
   - En lugar de fijar dogmáticamente un recuento de 2238 parámetros, el criterio de verificación exige:
     * Inspección dinámica de UID, nombre de fabricante, versión de plugin y recuento de parámetros expuestos por la instancia instanciada (`parameterCount > 0`).
     * Coincidencia exacta con el descriptor esperado del fixture controlado o del build específico de Dexed.
2. **Latencias en Ruta Digital Offline**:
   - `physicalRoundTripLatencyMs = NotApplicable` (no interviene DAC/ADC analógico).
   - `pluginLatencySamples = instancia->getLatencySamples()` (reportado dinámicamente por el plugin).
   - `bufferLatencySamples = blockSize` (calculado matemáticamente a partir del bloque de renderizado).

---

## 7. Umbrales Canónicos de Audio

En concordancia directa con [LabAudioReceiver.cpp](file:///d:/desarrollos/ABDSynths/ABDAudioLab/src/audio/LabAudioReceiver.cpp#L80-L166):
- **Umbral de Saturación / Clipping (`E2E-N05`)**: `clippingThreshold = 0.99f` (~ -0.1 dBFS). Cualquier muestra $|x| \ge 0.99f$ se cataloga como saturación y marca la medición como sobrecargada.
- **Umbral de Silencio / Desconexión (`E2E-N08`)**: `silenceThresholdLinear = 0.0001f` (-80 dBFS). Si el nivel máximo del bloque no supera este umbral, el sistema reporta señal ausente o cableado desconectado.

---

## 8. Criterios de Entrada y Salida

### Criterios de Entrada (Entry Criteria):
1. Commit HITO-04 certificado (`cfb6a8f`, acta `c4614ac`).
2. Working tree 100% limpio.
3. Suite base: 605/605 PASS, 0 warnings.
4. `PLAN_HITO_05.md` y `MATRIX_HITO_05_E2E.md` aprobados con ajustes normativos.
5. Fixtures canónicos v2.0 operativos.
6. Directorio temporal RAII limpio.

### Criterios de Salida (Exit Criteria):
1. 100% de los casos de prueba E2E ejecutados con veredicto `PASS` o `SKIPPED (justificado)`.
2. 0 defectos bloqueantes P0/P1.
3. Suite hermética en 100% PASS (sin regresiones sobre los 605 tests base).
4. `ProductionPackage` verificado con sus 4 artefactos obligatorios para cada escenario activo.
5. Preservación de procedencia verídica en los manifests.
6. Round-trip semántico comprobado en persistencia `.abdlabtest`.
7. Redacción de `ACTA_HITO_05_END_TO_END_LAB_CERTIFICATION.md`.
8. Actualización de `ROADMAP.md` y `HANDOFF.md`.
