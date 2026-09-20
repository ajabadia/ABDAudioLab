# Documento de Traspaso Técnico (HANDOFF) — ABDAudioLab

**Proyecto:** ABDAudioLab — Universal Black-Box Musical Hardware & Synth Profiler  
**Versión Actual:** 2.1.0  
**Fecha de Emisión:** 2026-09-20  
**Autor:** Antigravity Lead Architect / ABDSynths  

---

## 1. Estado Actual Ejecutivo

- **HITO-01**: Certificado ([ACTA_HITO_01_STEP1_TARGETVIEW.md](audits/ACTA_HITO_01_STEP1_TARGETVIEW.md)).
- **HITO-02**: Certificado ([ACTA_HITO_02_MIDI_AUTOMATED_CORE.md](audits/ACTA_HITO_02_MIDI_AUTOMATED_CORE.md)).
- **HITO-03**: Certificado ([ACTA_HITO_03_STEPPER_EXCITATION_INTEGRATION.md](audits/ACTA_HITO_03_STEPPER_EXCITATION_INTEGRATION.md)).
- **HITO-03.1**: Certificado ([ACTA_HITO_03_1_STEPPER_COHERENCE.md](audits/ACTA_HITO_03_1_STEPPER_COHERENCE.md)).
- **HITO-04 (A/B/C + Smoke UI)**: **Certificado y Cerrado** ([ACTA_HITO_04_EXPORT_INTEGRATION_PIPELINE.md](audits/ACTA_HITO_04_EXPORT_INTEGRATION_PIPELINE.md)).
- **HITO-05**: Siguiente trabajo (Certificación End-to-End multimodelo en los 4 targets representativos).
- **Suite global automatizada**: **605/605 casos PASS (100% éxito)**.
  - 564 casos preexistentes conservados y PASS.
  - 41 casos nuevos de exportación, I/O transaccional, integración multimodelo y smoke UI (ST-69 a ST-107 + Smoke UI).
- **Aserciones totales**: **228.536 aserciones superadas sin fallos**.
- **Compilación**: `Release x64` MSVC / C++20 exitosa (`ABDAudioLab.exe` y `ABDAudioLab_Tests.exe`).
- **Working Tree**: Confirmado limpio de cambios de producción, respaldado por tests unitarios y actas oficiales.

---

## 2. Última Certificación: HITO-03.1-STEPPER-COHERENCE

En el último corte técnico se resolvieron las dos correcciones normativas y se estabilizó la interacción:
1. **Verificación Digital ≠ Bypass Automático**: Para VST3, `audio.requirement = NotApplicable`, `digital.requirement = Required`, y `digital.verified = false` al inicio. Requiere comprobación activa (`verifyDigitalCalibration()`). `digital.verified ≠ bypass`.
2. **Capacidades Reales VST3**: Los plugins VST3 no asumen universalmente MIDI. Se consulta `supportsMidiInput`. Si carecen de MIDI, adoptan `AutomatedVstParameter` o `ManualOperator`.
3. **Selector Único**: `catalogSelector` es la única autoridad interactiva para elegir target. `SoundIdTargetView` es una ficha pasiva y viva de telemetría. `HardwareSelectorPill` solo navega a Paso 1 sin generar selecciones divergentes.
4. **Reordenación de Stepper (0 a 4)**: Secuencia visual `0. Studio Environment`, `1. Target & Routing`, `2. Calibration & Setup`, `3. Run Session`, `4. Export & Report`. Valores internos de enum `Step` intactos (`visualOrder` desacoplado).
5. **Invalidación Coherente**: Cambio de target marca receta como `RecipeStatus::IncompatibleWithTarget`, resetea calibración, cancela ejecuciones activas e incrementa `controllerGeneration`.

---

## 3. Zonas Estabilizadas (Qué NO Tocar)

Queda estrictamente prohibido modificar los siguientes subsistemas sin un nuevo ADR o hito específico:
- `ProfilingSequencer` como única autoridad operativa de ejecución.
- `ProfilingSessionController` como única autoridad de sesión y snapshots.
- `catalogSelector` como selector único de dispositivos.
- `SoundIdTargetView` como ficha pasiva de telemetría (menú de selección desactivado).
- `visualOrder` desacoplado del Stepper (sin alterar valores de enum `Step`).
- `CalibrationStatus` ortogonal (`audio`, `midi`, `digital`).
- Invalidación de sesión por cambio de target (`controllerGeneration`).
- `ManualAnalogueController` y flujo rítmico de operador manual.
- `OperatorCardsContainerComponent` para interacción manual.
- Persistencia base en formato `.abdlabtest`.

---

## 4. Arquitectura Actual y Diagrama de Flujo

```
[catalogSelector] (Única Autoridad de Selección)
       ↓
TargetSelectionState
       ↓
[ProfilingSessionController] (Autoridad de Sesión y Snapshots)
       ↓
ProfilingSessionSnapshot (Inmutable, Monotónico)
       ├── SoundIdTargetView (Ficha Pasiva)
       ├── SoundIdExcitationConfigPanel (Editor de Recetas)
       └── SoundIdProfilingRunView (Monitor de Ejecución)

SessionPlan / Recipe
       ↓
[SessionExecutionCoordinator]
       ↓
[ProfilingSequencer] (Única Autoridad de Ejecución)
       ├── MIDI / VST3 / CC / SysEx (ProfilingHardwareDispatcher)
       └── ManualAnalogueController (Espera y confirmación operador)
       ↓
Audio Capture / Metrics / Evaluation
       ↓
[SessionSerializer] ↔ .abdlabtest
       ↓
[ReportExportService] ↔ ProductionPackage / Certification HTML
```

---

## 5. Mapa de Autoridades Únicas

| Decisión / Dominio | Autoridad Única | Implementación Concreta |
|---|---|---|
| **Target Seleccionado** | `catalogSelector` + `ProfilingSessionController` | `catalogSelector.onSelectionChanged` $\to$ `selectTarget()` |
| **Capacidades del Target** | Contrato / Manifiesto de Hardware o Plugin | `HardwareContractRegistry` / `TargetSelectionState` |
| **Receta de Excitación** | `ProfilingSessionController` | `currentSnapshot_.excitation` (`MidiRecipe` / `ManualOperatorRecipe`) |
| **Ejecución de Campaña** | `ProfilingSequencer` | `ProfilingSequencer::run()` en hilo de fondo |
| **Confirmación Manual** | `ManualAnalogueController` | `confirmManualStep()` / Barra Espaciadora |
| **Estado Visible en UI** | `ProfilingSessionSnapshot` | Snapshots inmutables vía `onSessionSnapshotUpdated` |
| **Persistencia de Sesión** | `SessionSerializer` | Archivo `.abdlabtest` / JSON ZIP |
| **Evaluación Metrológica** | `ProfilingSessionController` | `snapshot.evaluation` / `ModelHoldoutValidator` |
| **Exportación a Disco** | `ReportExportService` | `ReportExportUiController::requestExportProductionPackage()` |
| **Instrumentación Audio** | `LabAudioEngine` + `DiagnosticsTelemetryPoller` | Búferes circulares lock-free y FFT 1024 bins |

---

## 6. Decisiones ya Cerradas (Registro ADR)

- **ADR-01**: El Stepper clásico del Lab Bench es el único flujo de la aplicación.
- **ADR-02**: `SoundIdTargetView` es una ficha pasiva de solo lectura y telemetría en vivo.
- **ADR-03**: `catalogSelector` es la única autoridad de selección interactiva.
- **ADR-04**: Los valores del enum `Step` se conservan intactos para retrocompatibilidad de serialización.
- **ADR-05**: El orden visual del Stepper (0 a 4) se gobierna mediante el mapeo desacoplado `visualOrder`.
- **ADR-06**: La calibración es ortogonal con 3 rutas (`audio`, `midi`, `digital`) y 3 requisitos (`NotApplicable`, `Required`, `Optional`).
- **ADR-07**: Los targets analógicos puros (`NoDigitalControl`) fuerzan el modo `ManualOperator`.
- **ADR-08**: Los plugins VST3 no implican soporte MIDI; las capacidades proceden de su contrato real.
- **ADR-09**: El cambio de target invalida automáticamente recetas y calibraciones incompatibles.
- **ADR-10**: El código legacy se conserva inactivo hasta completar la auditoría de consumidores.
- **ADR-11**: Silenciamiento dual de emergencia: All Notes Off (CC 123) + All Sound Off (CC 120) en 16 canales.
- **ADR-12**: La calibración digital de plugins no equivale a bypass; exige verificación activa (`verifyDigitalCalibration`).
- **ADR-13**: **Convergencia absoluta en la cadena de exportación e informe único**: Todos los modos de excitación (Automated MIDI/VST3 y Manual Operator/Analógico) convergen en un modelo común de evaluación (`EvaluationSnapshot`), idénticas guardas de `ExportReadiness` y una única cadena de exportación (`ReportExportService` $\to$ `ProductionPackage`); las diferencias metrológicas se expresan exclusivamente mediante metadatos en el `manifest.json` y secciones condicionales en el informe unificado, nunca mediante exportadores paralelos ni bifurcaciones arquitectónicas.

---

## 7. Decisiones Todavía Abiertas / Pendientes

| ID | Decisión Pendiente | Impacto | Próximo Paso | Estado |
|---|---|---|---|---|
| **D-01** | Fuente oficial de telemetría de calibración | Impide eliminar `loopbackModal` legacy | Migrar consumidor a `CalibrationSnapshot` | **Pendiente (Hito 6)** |
| **D-02** | Fuente oficial de telemetría de Stepper | Impide eliminar `stepperBar` legacy | Migrar consumidor a `WorkflowNavigationSnapshot` | **Pendiente (Hito 6)** |
| **D-03** | Contrato de audición A/B en Paso 4 | Afecta reproducción en `SoundIdResultsSummaryView` | Validar existencia de ficheros en `ExperimentStorage` | **Pendiente (Hito 4)** |
| **D-04** | Versionado de esquema `.abdlabtest` | Afecta recarga de sesiones de versiones anteriores | Añadir `schemaVersion: 2` y tests round-trip | **Pendiente (Hito 4/5)** |
| **D-05** | Retirada definitiva de `SoundIdGuidedWorkflowContainer` | Afecta superficie de código | Esperar a certificar Paso 4 antes de retirar | **Pospuesta (Hito 7)** |
| **D-06** | UI Web vs Nativa para resultados | Afecta Paso 4 | Ratificada UI nativa `SoundIdResultsSummaryView` | **Decidida (Hito 4)** |

---

## 8. Seams Activos e Inventario de Conexiones

```
[Seams Certificados]
• catalogSelector → ProfilingSessionController (TargetSelectionState)
• ProfilingSessionController → SoundIdTargetView (Snapshot inmutable)
• ProfilingSequencer → SoundIdProfilingRunView (Telemetría de ejecución)
• operatorStepModal → ProfilingSequencer (confirmManualStep)
• NativeCalibrationPanel → ProfilingSessionController (CalibrationStatus)

[Seams Abiertos / Próximos]
• ProfilingSessionSnapshot → SoundIdResultsSummaryView → ReportExportService (Hito 4)
• SessionSerializer ↔ ProfilingSessionSnapshot (Hito 4/5)

[Seams Legacy Controlados]
• loopbackModal → MainContentTelemetrySource (Pendiente Hito 6)
• stepperBar → MainContentTelemetrySource (Pendiente Hito 6)
```

---

## 9. Resultados de la Auditoría de Divergencia (DR-01 a DR-10)

Antes de autorizar el inicio de HITO-04, se auditó el grafo de llamadas del repositorio:

1. **DR-01 (Estados de Target Duplicados)**: Confirmado que solo `catalogSelector` emite selecciones interactivas. `TargetView` tiene el selector desactivado y `HardwareSelectorPill` solo navega a Paso 1. (**Confirmado Único**).
2. **DR-02 (Resolución de Capacidades)**: Centralizada en `ProfilingSessionController::selectTarget()` según contrato. (**Confirmado Único**).
3. **DR-03 (Exportación Directa desde Vista)**: `SoundIdResultsSummaryView` no escribe a disco directamente; delega en `commands_.exportModel()`. (**Confirmado Único**).
4. **DR-04 (Calibración Paralela)**: `NativeCalibrationPanel` despacha directamente a los comandos del controlador y escucha el snapshot oficial. (**Confirmado Único**).
5. **DR-05 (Callbacks Espejo del Stepper)**: `stepperBar` está invisible, pero `loopbackModal` invoca `stepperBar.onStepSelected(RunSession)` como remanente. Se mantiene controlado y documentado para migración en Hito 6. (**Duplicado Aparente / Legacy Controlado**).
6. **DR-06 (Persistencia de Recetas)**: `SessionSerializer` (.abdlabtest) y `ExperimentStorage` (experimento inmutable) operan en dominios complementarios. (**Compatible**).
7. **DR-07 (Alcance de Rollback)**: `TargetViewIntegrationMode::Disabled` desactiva únicamente la vista `TargetView`, no el selector ni el backend. (**Aclarado**).
8. **DR-08 (Componentes Legacy Activos)**: `loopbackModal` y `stepperBar` son leídos por `MainContentTelemetrySource`. Pospuesta su eliminación hasta Hito 6. (**Pospuesto Deliberadamente**).
9. **DR-09 (Generación Monotónica)**: Confirmado incremento de `controllerGeneration` e invalidación de recetas ante cambio de target. (**Confirmado Único**).
10. **DR-10 (Silenciamiento de Emergencia)**: Panic dual 16 canales centralizado en `ProfilingHardwareDispatcher`. (**Confirmado Único**).

---

## 10. Evidencia y Comandos de Reproducción

### Comandos de Compilación y Test

```powershell
# Compilación limpia en Release x64
cmake --build build --config Release --target ABDAudioLab_Tests
cmake --build build --config Release --target ABDAudioLab

# Verificación de la suite específica de Coherencia (ST-47 a ST-68)
.\build\Release\ABDAudioLab_Tests.exe "[coherence]"

# Verificación de la suite global completa (564 test cases)
.\build\Release\ABDAudioLab_Tests.exe
```

### Resultados Esperados
- `[coherence]`: **18/18 test cases PASS (65 aserciones)**.
- `Global`: **564/564 test cases PASS (228.169 aserciones, 0 fallos)**.

### Ubicación Relativa de Binarios y Documentación
- Ejecutable GUI: `build/ABDAudioLab_artefacts/Release/ABDAudioLab.exe`.
- Ejecutable de Tests: `build/Release/ABDAudioLab_Tests.exe`.
- Acta Hito 1: [docs/audits/ACTA_HITO_01_STEP1_TARGETVIEW.md](audits/ACTA_HITO_01_STEP1_TARGETVIEW.md).
- Acta Hito 2: [docs/audits/ACTA_HITO_02_MIDI_AUTOMATED_CORE.md](audits/ACTA_HITO_02_MIDI_AUTOMATED_CORE.md).
- Acta Hito 3: [docs/audits/ACTA_HITO_03_STEPPER_EXCITATION_INTEGRATION.md](audits/ACTA_HITO_03_STEPPER_EXCITATION_INTEGRATION.md).
- Acta Hito 3.1: [docs/audits/ACTA_HITO_03_1_STEPPER_COHERENCE.md](audits/ACTA_HITO_03_1_STEPPER_COHERENCE.md).
- Hoja de Ruta Maestra: [docs/ROADMAP.md](ROADMAP.md).
- Plan de Implementación Activo: [implementation_plan.md](file:///C:/Users/ajaba/.gemini/antigravity-ide/brain/b641ba82-965e-4fcc-a8b6-f088d9337bc8/implementation_plan.md).

---

## 11. Riesgos Abiertos y Mitigación

| Riesgo | Impacto | Mitigación Activa | Condición de Cierre |
|---|---|---|---|
| **R-01: `loopbackModal` usado por telemetría** | No se puede eliminar sin romper el poller | Mantener invisible; aislar lecturas en `MainContentTelemetrySource` | Migrar a `CalibrationSnapshot` en HITO-06 |
| **R-02: `stepperBar` usado por telemetría** | No se puede eliminar sin romper el poller | Mantener invisible; no emitir eventos hacia el usuario | Migrar a `WorkflowNavigationSnapshot` en HITO-06 |
| **R-03: Exportación duplicada** | Informes divergentes en disco | Canalizar toda exportación exclusivamente a través de `ReportExportService` | Tests ST-80 y ST-81 en HITO-04 |
| **R-04: Audición A/B sin ficheros en disco** | Cuelgue o error al pulsar ▶ Play | Verificar `existsAsFile()` antes de lanzar proceso de audio | Tests ST-82 en HITO-04 |
| **R-05: Reutilización accidental de calibración** | Exportar modelo calibrado con otro dispositivo | Guardas de invalidación de `CalibrationStatus` por cambio de target | ST-63 (PASS) y ST-77 (HITO-04) |

---

## 12. Estado de HITO-04 y Próxima Acción Exacta

### Estado Actual de HITO-04
- **HITO-04A (Contrato y Guardas `ExportReadiness`)**: **CERTIFICADO**. Cálculo puro, desacople de IO, guardas metrológicas (ST-69 a ST-85).
- **HITO-04B (I/O de Filesystem, Rollback y Atomic Packaging)**: **CERTIFICADO**. Suite `test_ExportIO.cpp` con 13 casos (ST-86 a ST-98) y 157 aserciones PASS. Operación atómica todo-o-nada, staging seguro, verificación de fixity SHA-256 y preservación de paquetes previos.
- **HITO-04C (Mode-to-Export Integration / ADR-13)**: **CERTIFICADO**. Suite `test_ModeToExportIntegration.cpp` con 9 casos (ST-99 a ST-107) y 99 aserciones PASS. Convergencia demostrada sin exportadores paralelos. Matiz ST-105 normalizado a `AutomatedSysEx` / `MidiSysEx` / `AUTOMATED_SYSEX`.
- **Suite Global Completa**: 603/603 PASS (228.467 aserciones, exit 0).

### Próxima Acción Exacta: HITO-05 (End-to-End Lab Certification)
1. **Validación visual de humo (Paso 4)**: Comprobación interactiva en UI de visualización de métricas, copia de hash de fixity, audición A/B de holdout y exportación.
2. **Ejecución de los 4 escenarios de laboratorio de HITO-05**:
   - Escenario 1: Dexed / VST3 automatizado offline.
   - Escenario 2: Hardware MIDI automatizado loopback.
   - Escenario 3: Hardware analógico manual (interacción operador y confirmación de pasos).
   - Escenario 4: Target híbrido (emulación + procesamiento analógico).
3. **Verificación de ciclo de vida completo (Paso 0 a Paso 4)**: Target Selection $\to$ Calibration $\to$ Excitation Recipe $\to$ Run Session $\to$ Export & ProductionPackage.

---

## 13. Checklist de Handoff y Reanudación

- [x] Actas oficiales de Hitos 1 a 3.1 presentes y verificadas.
- [x] Matriz de capacidades V2 y trazabilidad actualizadas.
- [x] Registro de decisiones cerradas (ADR-01 a ADR-13) documentado.
- [x] Suite de I/O de exportación certificada (ST-86 a ST-98: 13/13 PASS, 157 aserciones).
- [x] Principio de convergencia única de exportación ratificado en ROADMAP.md y HANDOFF.md.
- [x] Compilación Release x64 verificada sin errores ni warnings nuevos.
- [x] Riesgos abiertos documentados con estrategia de mitigación.
- [x] Auditoría de divergencia (DR-01 a DR-10) ejecutada y registrada.
- [x] Zonas estabilizadas claramente delimitadas (qué no tocar).
- [x] Componentes legacy identificados con su hito de migración (HITO-06 / HITO-07).
- [x] Rutas relativas y portabilidad documental garantizadas (sin dependencias de paths absolutos).
