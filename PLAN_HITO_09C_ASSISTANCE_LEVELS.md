# PLAN OPERATIVO: HITO-09C — Niveles de Asistencia (Rápido, Configurable, Avanzado) como Vistas del Mismo Contrato

**Estado:** 🟢 APROBADO CON AJUSTES PARA IMPLEMENTACIÓN  
**Fecha:** 2026-09-23  
**Proyecto:** ABDAudioLab  
**Base Certificada:** HITO-09B (Build #423 — 631 test cases | 623 PASS | 8 SKIPPED | 0 FAIL | 229.113 assertions)

---

## 1. Regla de Oro y Principio Rector de HITO-09C

> **Rápido, Configurable y Avanzado NO son tres motores de medición, tres sesiones distintas ni tres formatos de evidencia.**  
> **Son tres vistas / grados de interacción y detalle sobre el mismo contrato declarativo `MeasurementRecipe`.**  
>  
> La regla rectora de HITO-09B se preserva con rigor absoluto:  
> **Seleccionar / Editar receta $\neq$ Crear sesión $\neq$ Ejecutar audio.**

```
   ┌─────────────────────────────────────────────────────────────────┐
   │               Niveles de Interfaz (RecipeEditorView)            │
   │                                                                 │
   │   [ Rápido ]             [ Configurable ]         [ Avanzado ]  │
   │  (Solo lectura)        (Ajustes seguros)       (Políticas DSP)  │
   └──────────┬───────────────────────┬──────────────────────┬───────┘
              │                       │                      │
              └───────────────────────┼──────────────────────┘
                                      ▼
                      Misma familia de contrato Recipe
                                      ▼
                      WorkingRecipe editable en memoria
                    (RecipeBase original queda intacta)
                                      ▼
                      ExperimentPlan determinista derivado
                     (hashes nuevos si hay cambios reales)
                                      ▼
                          Mismo Preflight (Paso 3)
                        (comprobación de entorno)
                                      ▼
                      Confirmación explícita Stepper
                                      ▼
                     Misma ProfilingSession Canónica
                                      ▼
                        Mismo ProfilingSequencer
                                      ▼
                           Misma Evidencia LNL
```

---

## 2. Los Seis Ajustes Normativos Incorporados

### Ajuste 1: Misma Familia de Contrato, Plan Científico Derivado
Si el usuario modifica parámetros de una receta, **el `ExperimentPlan` resultante es un plan científico nuevo con hashes nuevos**:
- Preset original: `experimentPlanHash A`
- WorkingRecipe modificada: `experimentPlanHash B`
Ambos se validan con el mismo `MeasurementRecipeService`, se compilan con el mismo `ExperimentPlanCompiler` y se ejecutan a través del mismo pipeline canónico.

### Ajuste 2: Separación Estricta entre Nivel de Vista (`RecipeEditorView`) y Perfil de Receta (`assistanceLevel`)
Se diferencian explícitamente:
- `recipe.assistanceLevel`: Qué tipo de receta es por defecto (`Quick`, `Configurable`, `Advanced`).
- `RecipeEditorView`: Qué nivel de detalle y controles expone la UI en este momento (`Quick`, `Configurable`, `Advanced`).
El usuario puede abrir una receta de perfil `Quick` y conmutar la vista a `Configurable` o `Advanced` para examinar o ajustar parámetros sin alterar el perfil intrínseco de la receta base.

### Ajuste 3: `RecipeBase` Inmutable y `WorkingRecipe` en Memoria
- `RecipeBase`: Objeto cargado de disco que **nunca se muta**. Permanece como referencia fija y limpia.
- `WorkingRecipe`: Copia en memoria donde se aplican las ediciones. Si se modifica, se actualiza `provenance.authoringSource = "operator_manual"`.
- Toda validación, compilación y preparación se realiza sobre la `WorkingRecipe`.

### Ajuste 4: Descarte de Cambios al Conmutar Receta y Acción "Restablecer"
- **Cambio de receta**: Al seleccionar otra receta en el catálogo, se descartan los cambios temporales y se inicializa la nueva `WorkingRecipe` desde la nueva `RecipeBase`.
- **Restablecer preset (`resetToPreset`)**: Revierte inmediatamente la `WorkingRecipe` al estado original de `RecipeBase`, recuperando sus hashes iniciales sin requerir recargar de disco.
- **Indicador de estado visual**:
  - *"Preset original"* (cuando `WorkingRecipe == RecipeBase`).
  - *"Ajuste temporal no guardado"* (cuando existen modificaciones locales).

### Ajuste 5: Parámetros Delimitados (Puntos Arbitrarios Pospuestos a HITO-10)
Para evitar incorporar conocimiento específico de plugins en la UI antes de contar con `TargetProfile` declarativos (HITO-10):
- **Configurable permite**:
  - Repeticiones $[1 \dots 10]$.
  - Nota MIDI principal $[0 \dots 127]$, velocidad $[0.1 \dots 1.0]$.
  - Compuerta `gateMs` $[50.0 \dots 2000.0\text{ ms}]$, reposo `settlingMs` $[10.0 \dots 500.0\text{ ms}]$.
  - Selección de conjuntos de puntos normativos predefinidos (3 puntos, 11 puntos, exhaustivo).
- **Avanzado permite además**:
  - Umbrales de evaluación: `minimumSnrDb` $[20.0 \dots 120.0\text{ dB}]$, `maximumThdPercent` $[0.001\% \dots 20.0\%]$, `f0ToleranceCents` $[0.1 \dots 100.0\text{ cents}]$.
  - Políticas de operación: `calibrationPolicy` (`"Required"`, `"Optional"`, `"None"`), `analysisPolicy`.
  - Semilla determinista explícita (`seed`).
- **Pospuesto a HITO-10**: Creación de parámetros semánticos arbitrarios y mapeos `semanticId` $\to$ `pluginParameterId`.

### Ajuste 6: Validación y Recompilación al Confirmar Campo (Debounce/Commit)
- La edición en la UI valida la sintaxis localmente.
- Al confirmar el campo (Enter, pérdida de foco, cambio de selector):
  1. Valida exhaustivamente con `MeasurementRecipeService`.
  2. Si es válida: recompila `ExperimentPlan`, recalcula `recipeDocumentHash` y `experimentPlanHash`, y actualiza el resumen y compatibilidad.
  3. Si es inválida: retiene la última `WorkingRecipe` válida, muestra el error RFC 6901 junto al control y deshabilita *"Preparar prueba"* hasta que se ingrese un valor válido.

---

## 3. Arquitectura de Componentes Propuesta

Todos los nuevos componentes se ubican exclusivamente en `src/gui/recipes/`:

1. **`RecipeAssistanceLevelSelectorComponent` (`.h` / `.cpp`):**
   - Selector segmentado de 3 pestañas: `[ Rápido ]`, `[ Configurable ]`, `[ Avanzado ]`.
   - Modifica el `RecipeEditorView` activo y notifica mediante callback.

2. **`RecipeEditorComponent` (`.h` / `.cpp`):**
   - Expone los controles según `RecipeEditorView`:
     - *Quick*: Vista de lectura; tarjeta de resumen y compatibilidad visibles; controles deshabilitados.
     - *Configurable*: Sliders/Spinners acotados para repeticiones, nota MIDI, gateMs, settlingMs, selector de conjunto de puntos.
     - *Advanced*: Campos para SNR mínimo, THD máximo, tolerancia f0, selector de calibración y semilla.
   - Indicador visual *"Preset original"* / *"Ajuste temporal no guardado"*.
   - Botón *"Restablecer preset"* (`resetToPreset()`).

3. **`RecipeExecutionController` (Ampliación):**
   - Mantiene `RecipeBase` (inmutable) y `WorkingRecipe` (mutable).
   - Métodos de mutación segura:
     - `updateRepetitions(int reps)`
     - `updateNoteExcitation(int note, double vel, double gateMs, double settlingMs)`
     - `updatePointSet(const std::vector<profiling::MeasurementPointConfig>& points)`
     - `updateEvaluationPolicy(double snrDb, double thdPct, double f0TolCents)`
     - `updateCalibrationPolicy(const std::string& policy)`
     - `updateRandomSeed(std::optional<int> seed)`
     - `resetToPreset()`
     - `setEditorView(RecipeEditorView view)`

---

## 4. Batería de 10 Pruebas Normativas (`test_AssistanceLevelsRecipeViews.cpp`)

1. **Paridad Multinivel de Esquema:** Las recetas emitidas en los 3 niveles satisfacen estrictamente el esquema JSON Draft 2020-12.
2. **Determinismo de Mutación en Configurable:** Modificar repeticiones de 3 a 5 produce hashes reproducibles y escala el número de ventanas del plan.
3. **Control de Límites en Configurable:** Valores ilegales (reps = 0, nota > 127) son rechazados por el validador emitiendo el código RFC 6901 exacto.
4. **Mutación Avanzada de Políticas Metrológicas:** Modificar `minimumSnrDb` actualiza la política de evaluación sin alterar el plan de excitación (`targetEvents`).
5. **Invarianza de Edición:** Alterar parámetros en la UI no arranca el secuenciador, no genera audio y no reemplaza una sesión existente.
6. **Preparación Confirmada de Receta Mutada:** La confirmación en el Stepper (Paso 3) crea la `ProfilingSession` con los valores exactos editados por el operador.
7. **`RecipeBase` Permanece Idéntica:** Editar la `WorkingRecipe` no altera el `recipeDocumentHash` ni los datos del preset original en disco.
8. **Restablecer Preset:** Tras mutaciones, invocar `resetToPreset()` restituye idénticos `recipeDocumentHash` y `experimentPlanHash` que `RecipeBase`.
9. **Cambio de Receta:** Seleccionar otra receta descarta las mutaciones temporales e inicializa la nueva receta desde su `RecipeBase` limpia.
10. **Separación de Vista y Receta:** Conmutar entre `Quick`, `Configurable` y `Advanced` en la UI sin modificar parámetros mantiene intactos ambos hashes, no crea sesión y no inicia audio.

---

## 5. Criterio de Cierre de HITO-09C

1. Selector de niveles (`RecipeEditorView`) y editor (`RecipeEditorComponent`) integrados bajo `src/gui/recipes/`.
2. Preservación estricta de `RecipeBase` y edición en memoria mediante `WorkingRecipe`.
3. Revalidación determinista con recálculo de hashes y emisión de diagnósticos RFC 6901.
4. Acción de restablecimiento (`resetToPreset`) y descarte limpio al cambiar de receta.
5. Cero modificaciones en componentes heredados protegidos.
6. Suite global completa en Build Release con **0 FAIL** y los 8 SKIPPED justificados.
