# ACTA DE CERTIFICACIÓN FORMAL: HITO-09C

**Fecha:** 2026-09-23  
**Proyecto:** ABDAudioLab  
**Compilación:** Build #425 (Release x64)  
**Autoridad de Certificación:** Antigravity (Lead Architect & Metrologist) / ajabadia (Tándem Partner)  
**Estado:** CERTIFICADO Y AUDITADO (PASS)

---

## 1. Resumen Ejecutivo y Alcance Certificado

En cumplimiento estricto del plan aprobado en `PLAN_HITO_09C_ASSISTANCE_LEVELS.md` y de los seis ajustes normativos acordados con el usuario, se ha culminado con éxito la implementación y auditoría de **HITO-09C: Niveles de Asistencia (Rápido, Configurable, Avanzado) como Vistas del Mismo Contrato `MeasurementRecipe`**.

El principio rector del hito ha quedado blindado metrológica y arquitectónicamente:
> **Rápido, Configurable y Avanzado NO son tres motores de medición, tres sesiones distintas ni tres formatos de evidencia.**  
> Son tres vistas / grados de interacción sobre el mismo contrato declarativo `MeasurementRecipe`.  
>  
> La regla rectora de invariabilidad se cumple con rigor absoluto:  
> **Seleccionar / Editar receta $\neq$ Crear sesión $\neq$ Ejecutar audio.**

---

## 2. Cumplimiento de los Seis Ajustes Normativos Aprobados

### Ajuste 1: Misma Familia de Contrato, Plan Científico Derivado
- Si el operador muta parámetros de ensayo (repeticiones, notas, políticas), el sistema deriva un `ExperimentPlan` científico nuevo con hashes nuevos (`recipeDocumentHash`, `experimentPlanHash`).
- Tanto los presets base de fábrica como las recetas modificadas por el operador se validan por el mismo validador y se ejecutan exclusivamente por el mismo pipeline canónico.

### Ajuste 2: Separación Estricta entre Nivel de Vista (`RecipeEditorView`) y Perfil de Receta (`assistanceLevel`)
- Se implementó `RecipeEditorView` (`Quick`, `Configurable`, `Advanced`), desacoplado del metadato `recipe.assistanceLevel`.
- Conmutar de vista en la UI (mediante `RecipeAssistanceLevelSelectorComponent`) altera qué controles se muestran, sin modificar la receta, sin alterar los hashes canónicos y sin crear sesiones ni audio.

### Ajuste 3: `RecipeBase` Inmutable y `WorkingRecipe` en Memoria
- El preset original cargado desde disco se almacena en `RecipePreparationState::baseRecipe` y permanece estrictamente inmutable.
- Toda edición local se aplica exclusivamente sobre `RecipePreparationState::workingRecipe`, marcando `provenance.authoringSource = "operator_manual"`.

### Ajuste 4: Descarte al Cambiar de Receta y Acción "Restablecer Preset" (`resetToPreset`)
- Conmutar de receta en el catálogo descarta automáticamente las mutaciones locales no guardadas y carga la nueva receta base limpia.
- La acción `resetToPreset()` en `RecipeExecutionController` restituye de forma inmediata la `workingRecipe` a los valores y hashes exactos de `baseRecipe`.
- Indicadores visuales en `RecipeEditorComponent`: *"Preset original intacto"* / *"Ajuste temporal no guardado"*.

### Ajuste 5: Parámetros Delimitados (Puntos Arbitrarios Pospuestos a HITO-10)
- **Configurable expone**: Repeticiones $[1 \dots 10]$, nota MIDI $[0 \dots 127]$, velocidad $[0.1 \dots 1.0]$, `gateMs` $[50 \dots 2000\text{ ms}]$, `settlingMs` $[10 \dots 500\text{ ms}]$ y conjuntos de puntos normativos predefinidos (3 pts, 11 pts).
- **Avanzado expone además**: `minimumSnrDb`, `maximumThdPercent`, `f0ToleranceCents`, `calibrationPolicy` (`Required`, `Optional`, `None`), `analysisPolicy` y semilla determinista (`seed`).
- Pospuesta a HITO-10 la adición de parámetros semánticos arbitrarios que requerirán perfiles declarativos `TargetProfile`.

### Ajuste 6: Validación y Recompilación en Commit (Debounce/Commit)
- Toda modificación es revalidada exhaustivamente por capas en `MeasurementRecipeService::loadAndValidateJson()`.
- Si un valor viola los límites físicos (ej. reps = 0, nota > 127), la mutación es rechazada, se preserva la última receta válida, se emite el diagnóstico RFC 6901 (`ERR_SEMANTICS_INVALID_RANGE`) y se impide la preparación de sesión.

---

## 3. Resultados de la Batería Normativa (test_AssistanceLevelsRecipeViews.cpp)

Los 10 contratos han sido certificados con éxito determinista:

| # | Test Case | Contrato Verificado | Resultado |
|---|---|---|:---:|
| 1 | `Assistance Levels: Paridad Multinivel de Esquema` | Los 3 presets (`Quick`, `Configurable`, `Advanced`) serializan y validan exhaustivamente contra JSON Schema Draft 2020-12 con 0 errores. | **PASS** |
| 2 | `Assistance Levels: Determinismo de Mutación en Configurable` | Modificar repeticiones de 3 a 5 recalcula deterministamente `recipeDocumentHash` y `experimentPlanHash`, y escala el número de ventanas a $5 \times 3 = 15$. | **PASS** |
| 3 | `Assistance Levels: Control de Límites en Configurable` | Parámetros ilegales (reps = 0, nota = 150) son rechazados emitiendo diagnóstico RFC 6901 (`ERR_SEMANTICS_INVALID_RANGE`) sin corromper la receta de trabajo. | **PASS** |
| 4 | `Assistance Levels: Mutación Avanzada de Políticas Metrológicas` | Modificar umbrales SNR y política de calibración actualiza los hashes documentales sin alterar los eventos físicos de excitación (`targetEvents`). | **PASS** |
| 5 | `Assistance Levels: Invarianza de Edición` | Realizar múltiples mutaciones en memoria mantiene el secuenciador en `Idle`, sin audio ni creación prematura de sesión. | **PASS** |
| 6 | `Assistance Levels: Preparación Confirmada de Receta Mutada` | La confirmación en el Stepper crea una `ProfilingSession` con 12 casos de ensayo y nota MIDI 67, usando los valores exactos editados por el operador. | **PASS** |
| 7 | `Assistance Levels: RecipeBase Permanece Idéntica` | Mutar `workingRecipe` no altera en memoria ni documentalmente a `baseRecipe` (reps = 3, nota = 60). | **PASS** |
| 8 | `Assistance Levels: Restablecer Preset` | `resetToPreset()` revierte inmediatamente la receta de trabajo a los hashes y parámetros exactos de `baseRecipe`. | **PASS** |
| 9 | `Assistance Levels: Cambio de Receta Descarta Mutaciones` | Conmutar de receta en el catálogo descarta mutaciones de la receta previa e inicializa la nueva receta desde su base limpia. | **PASS** |
| 10 | `Assistance Levels: Separación Estricta entre Vista y Receta` | Alternar entre vistas `Quick`, `Configurable` y `Advanced` sin cambiar parámetros no modifica ningún hash y no arranca ejecución. | **PASS** |

---

## 4. Auditoría Global de la Base de Código (Build #425 Release)

| Métrica | Baseline HITO-09B (Build #423) | Cierre HITO-09C (Build #425) | Delta |
|---|:---:|:---:|:---:|
| **Test Cases Totales** | 631 | **641** | **+10** |
| **Test Cases Aprobados (PASS)** | 623 | **633** | **+10** |
| **Test Cases Omitidos (SKIPPED)** | 8 (justificados) | **8 (justificados)** | 0 |
| **Test Cases Fallidos (FAIL)** | 0 | **0** | **0** |
| **Aserciones Verificadas** | 229.113 | **229.196** | **+83** |
| **Tasa de Éxito de Aserciones** | 100.0% | **100.0%** | Invariante |

> Los 8 casos omitidos corresponden estrictamente a la suite `[ui_governance]`, cuyo aislamiento individual se requiere por diseño debido al singleton COM/WASAPI de JUCE en Windows.

---

## 5. Dictamen Final

Se certifica que **HITO-09C: Niveles de Asistencia (Rápido, Configurable, Avanzado) como Vistas del Mismo Contrato** queda formalmente **CERRADO Y AUDITADO**.

Queda habilitada la transición hacia las siguientes fases del roadmap:
- **HITO-09D**: Promoción de Toma Libre (exploración ad-hoc) a `MeasurementRecipe` formal.
- **HITO-10**: `TargetProfile` declarativos en JSON (desacoplamiento de capacidades de plugins respecto a código C++).
