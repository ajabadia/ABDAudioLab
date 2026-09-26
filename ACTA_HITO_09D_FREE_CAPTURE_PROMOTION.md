# ACTA DE CERTIFICACIÓN FORMAL: HITO-09D

**Fecha:** 2026-09-24  
**Proyecto:** ABDAudioLab  
**Compilación:** Build #430 (Release x64)  
**Autoridad de Certificación:** Antigravity (Lead Architect & Metrologist) / ajabadia (Tándem Partner)  
**Estado:** CERTIFICADO Y AUDITADO (PASS)

---

## 1. Resumen Ejecutivo y Alcance Certificado

En cumplimiento estricto del plan de ingeniería [PLAN_HITO_09D_FREE_CAPTURE_PROMOTION.md](file:///d:/desarrollos/ABDSynths/ABDAudioLab/PLAN_HITO_09D_FREE_CAPTURE_PROMOTION.md) y de los principios de honestidad metrológica aprobados con el usuario, se ha culminado con éxito la implementación, integración en interfaz y verificación normativa de **HITO-09D: Promoción de Toma Libre (Exploration) a MeasurementRecipe Formal**.

El principio rector de la metrología acústica ha quedado sellado:
> **Una Toma Libre (`Exploration`) histórica permanece siempre `Exploration`, nunca se reclasifica retroactivamente y permanece bloqueada para exportación como medición formal.**  
>  
> La promoción explícita es un acto voluntario de ingeniería que sintetiza una **nueva** `MeasurementRecipe` temporal, revisable en el Paso 2 (`Step::CalibrateLoopback` / "2. Calibration & Setup" mediante `RecipeEditorComponent` en modo `Configurable`), preflighteable en el Paso 3, y ejecutable solo tras confirmación explícita del operador en una nueva sesión formal.  
>  
> La regla de oro se cumple sin excepciones:  
> **Cero invención de puntos $\cdot$ Cero mutación retroactiva $\cdot$ Hashes canónicos deterministas independientes del reloj (Opción B).**

---

## 2. Cumplimiento de los Seis Principios Metrológicos Normativos

### Principio 1: Inmutabilidad de la Exploración Histórica
- La captura exploratoria original (`ExplorationRecord`) preserva su `purpose = MeasurementPurpose::Exploration` y `ExportReadiness::Decision::Blocked`.
- No se altera ningún archivo de audio, gráfico ni reporte de la sesión de toma libre.

### Principio 2: Cero Invención de Puntos de Control (`PromotionDraft`)
- Si una toma libre contiene controles sin mapeo semántico inequívoco (`semanticId` vacío o nulo), el servicio de promoción **NO inventa parámetros por defecto ni asume valores arbitrarios**.
- En dicho escenario, se genera un `PromotionDraft` con diagnóstico formal RFC 6901 (`ERR_PROMOTION_CONTROL_UNDECLARED`), bloqueando la creación de `ExperimentPlan`, la resolución de entorno y la preparación de `ProfilingSession`.

### Principio 3: Proveniencia Criptográfica Limpia
- `sourceRecipeDocumentHash` permanece estrictamente vacío en recetas promovidas desde exploración (reservado exclusivamente para recetas derivadas de otra receta).
- Se establecen las claves de proveniencia extendidas:
  - `provenance.authoringSource = "promoted_from_exploration"`
  - `provenance.sourceKind = "exploration"`
  - `provenance.sourceExplorationId = <explorationId>`
  - `provenance.sourceExplorationHash = <sha256_canónico>`
  - `provenance.promotionToolVersion = "ABDAudioLab v2.1.0"`

### Principio 4: Hash Canónico RFC 8785 de la Exploración (`sourceExplorationHash`)
- Se implementó la serialización canónica RFC 8785 de `ExplorationRecord` mediante `synth::Sha256::computeHex`.
- El hash incluye únicamente la evidencia estable: `explorationId`, `targetId`, `targetName`, `sampleRate`, `blockSize`, `transientPeakDbFs`, `sustainedRmsDbFs`, `fundamentalFrequencyHz`, notas MIDI, controles observados y `sha256AudioEvidence`.
- Excluye punteros en memoria, rutas temporales de archivo, marcas temporales de reloj de la UI y estados volátiles.

### Principio 5: Opción B para `promotedAt` (Determinismo Hash Absoluto)
- `promotedAt` / `promotedAtIso8601` vive **exclusivamente fuera** de `MeasurementRecipe`, residiendo en `PromotionRecord`.
- Gracias a este diseño, dos promociones de la misma exploración realizadas en días, horas o husos horarios distintos generan exactamente la misma `MeasurementRecipe` y el mismo `recipeDocumentHash` idéntico bit a bit.

### Principio 6: Navegación y Revisión Visual en el Stepper (Paso 2 `Step::CalibrateLoopback`)
- Al pulsar *"Promover a Receta Formal"*, la aplicación navega a `Step::CalibrateLoopback` ("2. Calibration & Setup"), donde en la composición actual reside `RecipeEditorComponent` configurado en `RecipeEditorView::Configurable`.
- El Paso 1 (`Step::HardwareRouting` / "1. Target & Routing") preserva intacto el target y su conexionado, evitando obligar al usuario a reseleccionarlo.
- La receta promovida se muestra de inmediato con sus notas, compuertas, repeticiones y controles observados, permitiendo editar parámetros o avanzar a preflight.

---

## 3. Resultados de la Batería Normativa (test_FreeCaptureRecipePromotion.cpp)

Los 11 contratos han sido certificados con éxito determinista:

| # | Test Case | Contrato Verificado | Resultado |
|---|---|---|:---:|
| 1 | `Promotion: Promoción Exitosa de Exploración Válida` | Genera `MeasurementRecipe` válida con 1 punto de control observado, excitación MIDI correspondiente y proveniencia declarada. | **PASS** |
| 2 | `Promotion: Cero Invención de Puntos (PromotionDraft)` | Controles sin `semanticId` generan `PromotionDraft` no ejecutable con `ERR_PROMOTION_CONTROL_UNDECLARED`. | **PASS** |
| 3 | `Promotion: Cero Invención por Falta de Parámetros` | Exploración sin controles ni notas genera `ERR_PROMOTION_MISSING_EXCITATION` y `ERR_PROMOTION_NO_CONTROLS`. | **PASS** |
| 4 | `Promotion: Invarianza Criptográfica del Hash Canónico` | Rutas temporales de archivo o punteros distintos producen exactamente el mismo `sourceExplorationHash`. | **PASS** |
| 5 | `Promotion: Determinismo de Promoción y Opción B (Sin promotedAt en Recipe)` | Dos promociones en timestamps distintos producen exactamente el mismo `recipeDocumentHash` e idéntica receta. | **PASS** |
| 6 | `Promotion: Inmutabilidad de la Exploración Original` | Tras la promoción, la exploración original mantiene su ID, propósito `Exploration` y bloqueo de exportación. | **PASS** |
| 7 | `Promotion: Invarianza - Promover NO Crea Sesión ni Ejecuta Audio` | La llamada al servicio genera modelos en memoria sin arrancar secuencias, audio ni sesiones en curso. | **PASS** |
| 8 | `Promotion: Compilación y Preflight de Receta Promovida` | La receta promovida se compila deterministamente a `ExperimentPlan` de 1 punto y pasa preflight de target compatible. | **PASS** |
| 9 | `Promotion: Rechazo de Target Incompatible en Receta Promovida` | Un target distinto al de la exploración es diagnosticado en preflight con `ERR_TARGET_MISMATCH`. | **PASS** |
| 10 | `Promotion: Integración con RecipeExecutionController` | `promoteExploration(...)` carga la receta en modo `Configurable`, habilitando revisión en el Stepper antes de confirmar. | **PASS** |
| 11 | `Promotion: Navegación a Revisión de Recipe en el Stepper` | Al promover, el Stepper muestra `RecipeEditorComponent` en Paso 2 (`Step::CalibrateLoopback`) en modo `Configurable`, visible, con receta mostrada, sin reelegir target y listo para preflight. | **PASS** |

---

## 4. Auditoría Global de la Base de Código (Build #430 Release)

| Métrica | Baseline HITO-09C (Build #425) | Cierre HITO-09D (Build #430) | Delta |
|---|:---:|:---:|:---:|
| **Test Cases Totales** | 641 | **652** | **+11** |
| **Test Cases Aprobados (PASS)** | 633 | **644** | **+11** |
| **Test Cases Omitidos (SKIPPED)** | 8 (justificados) | **8 (justificados)** | 0 |
| **Test Cases Fallidos (FAIL)** | 0 | **0** | **0** |
| **Aserciones Verificadas** | 229.196 | **229.351** | **+155** |
| **Tasa de Éxito de Aserciones** | 100.0% | **100.0%** | Invariante |

> Los 8 casos omitidos corresponden estrictamente a la suite `[ui_governance]`, cuyo aislamiento individual se requiere por diseño debido al singleton COM/WASAPI de JUCE en Windows.

---

## 5. Dictamen Final

Se certifica que **HITO-09D: Promoción de Toma Libre a MeasurementRecipe Formal** queda formalmente **CERRADO Y AUDITADO**.

Queda completado en su totalidad el macro-hito de Recetas de Medición (HITO-09A, HITO-09B, HITO-09C, HITO-09D) y habilitada la transición al siguiente hito del roadmap:
- **HITO-10**: `TargetProfile` declarativos en JSON (desacoplamiento de capacidades de plugins respecto a código C++).
