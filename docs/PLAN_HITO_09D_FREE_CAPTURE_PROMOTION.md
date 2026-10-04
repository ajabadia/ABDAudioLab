# PLAN OPERATIVO: HITO-09D — Promoción de Toma Libre (Exploración) a MeasurementRecipe Formal (Revisado)

**Proyecto:** ABDAudioLab  
**Versión de Esquema:** 1.0 (JSON Schema Draft 2020-12 / RFC 8785)  
**Fecha:** 2026-09-23  
**Estado:** REVISADO — Aprobado condicionado con las dos correcciones obligatorias integradas  
**Dependencias:** HITO-09A (Contrato), HITO-09B (Banco de Trabajo), HITO-09C (Niveles de Asistencia), ADR-001 (Convergencia)

---

## 1. Contexto y Principio Rector

En HITO-09A, HITO-09B e HITO-09C se construyó el pipeline canónico de recetas declarativas:
- **HITO-09A:** Contrato declarativo inmutable, esquemas JSON Schema Draft 2020-12, compilación unidireccional y 4 hashes canónicos.
- **HITO-09B:** Banco de Trabajo Unificado como adaptador visual desacoplado en el Stepper (Paso 2), bajo la regla: `Seleccionar receta ≠ Crear sesión ≠ Ejecutar`.
- **HITO-09C:** Tres niveles de asistencia (`Rápido`, `Configurable`, `Avanzado`) como vistas del mismo contrato común, con `RecipeBase` inmutable y `WorkingRecipe` en memoria con revalidación determinista en commit.

El objetivo de **HITO-09D** es cerrar el puente metodológico entre la experimentación ad-hoc y la medición formal:
```
Toma Libre (Exploration ad-hoc)
       ↓
Operador prueba notas, parámetros y ajustes en tiempo real
       ↓
Decide que la prueba merece repetirse sistemáticamente
       ↓
Acción explícita: "Promover a receta"
       ↓
RecipePromotionService analiza el ExplorationContext
       ↓
¿Hay target y controles científicos declarados?
│
├─ Sí (Caso A: Datos suficientes)
│  → WorkingRecipe formal válida
│  → provenance: authoringSource = "promoted_from_exploration", sourceExplorationHash = "..."
│  → Paso 2 del Stepper en modo Configurable (revisión y ajuste)
│  → Paso 3 del Stepper (preflight y compatibilidad de entorno)
│  → Confirmación explícita del operador
│  → Nueva ProfilingSession (Measurement) → ProfilingSequencer → Audio, DSP y Evidencia
│
└─ No (Caso B: Exploración incompleta / control no declarado)
   → PromotionDraft no ejecutable
   → Diagnóstico estructurado: ERR_PROMOTION_CONTROL_UNDECLARED
   → UI informa de la insuficiencia y ofrece opciones (elegir preset base, esperar a HITO-10)
   → Exploración histórica permanece intacta como evidencia no certificable
   → NO se crea ProfilingSession, NO se genera experimentPlanHash ejecutable
```

### Reglas Metrológicas Obligatorias
1. **Regla D8 y Separación Histórica:**
   - La exploración original **sigue siendo siempre `Exploration`**.
   - La receta promovida es una **nueva propuesta de `Measurement`**.
   - Los futuros `ExecutionRecord` pertenecerán a la nueva ejecución, sin reescribir la exploración histórica.
2. **Corrección 1 (Trazabilidad Estricta y Opción B de Metadatos Temporales):**
   - Una exploración no es una receta. **Prohibido usar `sourceRecipeDocumentHash` para referenciar una exploración.**
   - `sourceRecipeDocumentHash` queda reservado exclusivamente para derivaciones de otra `MeasurementRecipe`.
   - **Opción B (Determinismo Científico):** `promotedAt` vive fuera de `MeasurementRecipe`, en un `PromotionRecord` (o futuro `ExecutionRecord`), evitando que la hora de interacción humana altere el `recipeDocumentHash` canónico.
   - Para recetas promovidas desde Toma Libre se utiliza:
     ```json
     "provenance": {
       "authoringSource": "promoted_from_exploration",
       "sourceKind": "exploration",
       "sourceExplorationId": "...",
       "sourceExplorationHash": "sha256:...",
       "promotionToolVersion": "...",
       "documentationRef": "Promovido desde Toma Libre (Exploration)"
     }
     ```
   - **Definición de `sourceExplorationHash`:**
     - SHA-256 canónico (RFC 8785) de un `ExplorationRecord`.
     - **INCLUYE:** `schemaVersion`, `explorationSessionId`, target resuelto (`targetId`, `targetName`, `targetDeviceType`), `sampleRate`, `channels`, notas y velocidades observadas (`observedNotes`), controles declarados con valor normalizado y confirmación (`controlSnapshots`), y traza de eventos discretos (`relevantEventTrace`).
     - **EXCLUYE:** rutas temporales de archivos, punteros, direcciones de memoria, estado de UI, posición de ventanas, timestamps de render o locales, y cadenas volátiles de diagnóstico.
3. **Corrección 2 (Cero Invención de Datos Científicos):**
   - **Prohibido crear automáticamente puntos ficticios como `filter_cutoff@0.5`** cuando los controles no hayan sido declarados en la captura (`confirmationStatus == "unknown"`, `unspecified_param` o falta de `normalizedValue`).
   - Sin controles declarados con `semanticId` y valor normalizado, la promoción produce un `PromotionDraft` no ejecutable con diagnóstico `ERR_PROMOTION_CONTROL_UNDECLARED`.
   - `PromotionDraft` **no contiene** `ExperimentPlan`, `ResolvedExecutionPlan` ni `ProfilingSession`, y queda bloqueado para ejecución.

---

## 2. Decisiones de Diseño y Arquitectura

### 2.1. Dominio: `RecipePromotionService`
Se crea `src/profiling/RecipePromotionService.h` y `.cpp`:
- Estructura pura de dominio sin dependencias de UI ni de callbacks de audio.
- Modela `ExplorationRecord`: registro canónico estable para cálculo criptográfico RFC 8785.
- Modela `PromotionRecord`: almacena `promotedAtIso8601` y metadata temporal de la acción de promoción fuera de la receta.
- Modela `ExplorationContext`:
  - Identidad de target (`targetId`, `targetName`, `deviceType`).
  - Restricciones de entorno observadas (`sampleRate`, `channels`, capacidades).
  - Excitación observada (notas MIDI ensayadas, velocidad, `gateMs`, `settlingMs`, repeticiones).
  - Puntos de control observados (`std::vector<measurement::ControlStateSnapshot>`).
  - Identidad de exploración (`explorationSessionId`, `explorationHash`).
  - Traza de eventos discretos (`relevantEventTrace`).
- Modela `PromotionDraft`:
  ```cpp
  struct PromotionDraft
  {
      std::string targetId;
      std::string targetName;
      std::string targetDeviceType;
      std::vector<NoteExcitationConfig> observedNotes;
      std::vector<measurement::ControlStateSnapshot> undeclaredSnapshots;
      std::string sourceExplorationId;
      std::string sourceExplorationHash;
      std::string diagnosticReason;
  };
  ```
- Modela `PromotionResult`:
  ```cpp
  struct PromotionResult
  {
      std::optional<MeasurementRecipe> promotedRecipe;
      std::string recipeDocumentHash;
      std::optional<PromotionRecord> promotionRecord; // Metadata temporal separada (Opción B)
      std::optional<PromotionDraft> draft;
      std::vector<ValidationDiagnostic> diagnostics;

      [[nodiscard]] bool isExecutableRecipe() const noexcept
      {
          return promotedRecipe.has_value()
              && std::none_of(
                  diagnostics.begin(),
                  diagnostics.end(),
                  [](const ValidationDiagnostic& diagnostic)
                  {
                      return diagnostic.severity == DiagnosticSeverity::Error;
                  });
      }
  };
  ```
- Método principal:
  ```cpp
  PromotionResult promoteExploration(const ExplorationContext& ctx,
                                     const std::string& customRecipeName = "",
                                     const std::string& promotedAtIso8601 = "") const;
  ```
- **Lógica de evaluación en el servicio:**
  1. Si `controlSnapshots` está vacío o todas las instantáneas carecen de `normalizedValue` (o su `controlId == "unspecified_param"` o `confirmationStatus == "unknown"`):
     - Construye `PromotionDraft` (sin `MeasurementRecipe`, `ExperimentPlan`, `ResolvedExecutionPlan` ni `ProfilingSession`).
     - Emite diagnóstico de severidad `Error`:
       - `code = "ERR_PROMOTION_CONTROL_UNDECLARED"`.
       - `jsonPointer = "/exploration/controlStateSnapshots/0"`.
       - `message = "La exploración contiene controles o posiciones no declaradas. No es posible crear una MeasurementRecipe ejecutable sin un semanticId de parámetro declarado."`
     - Retorna `PromotionResult` con `isExecutableRecipe() == false`.
  2. Si existen controles declarados válidos con `normalizedValue`:
     - Extrae los puntos de medición `MeasurementPointConfig` exclusivamente de los controles declarados.
     - Construye `MeasurementRecipe` válida con `authoringSource = "promoted_from_exploration"`, `sourceKind = "exploration"`, `sourceExplorationId` y `sourceExplorationHash`.
     - Serializa y valida mediante `MeasurementRecipeService::loadAndValidateJson()`.
     - Construye `PromotionRecord` desacoplado con `promotedAtIso8601`.
     - Retorna `PromotionResult` con la receta y su `recipeDocumentHash` canónico.

### 2.2. Contrato de Receta y Esquema JSON
- `provenance.authoringSource`: se añade `"promoted_from_exploration"`.
- `MeasurementRecipe.provenance` **admite**:
  - `sourceKind`
  - `sourceExplorationId`
  - `sourceExplorationHash`
  - `promotionToolVersion`
  - `documentationRef`
- `MeasurementRecipe.provenance` **NO admite**:
  - `promotedAt`
  - `promotedAtIso8601`
  - `promotionRecordId`
- `sourceRecipeDocumentHash`: se reserva exclusivamente para una `MeasurementRecipe` derivada de otra `MeasurementRecipe`.

Y, de forma separada:
- `PromotionRecord` **admite**:
  - `kind`
  - `promotionRecordId`
  - `promotedAtIso8601`
  - `sourceExplorationId`
  - `sourceExplorationHash`
  - `promotedRecipeDocumentHash`
  - `promotionToolVersion`

**Principio de diseño:**
- `MeasurementRecipe`: qué experimento se quiere repetir (intención científica reproducible).
- `PromotionRecord`: cuándo y desde qué exploración se creó esa receta (registro histórico de interacción).

Bloque de proveniencia canónico en la receta promovida:
```json
{
  "provenance": {
    "authoringSource": "promoted_from_exploration",
    "sourceKind": "exploration",
    "sourceExplorationId": "exp_sess_7a8b9c",
    "sourceExplorationHash": "sha256:...",
    "promotionToolVersion": "1.0.0",
    "documentationRef": "Promovido desde Toma Libre (Exploration)"
  }
}
```

Registro externo de promoción (`PromotionRecord`):
```json
{
  "kind": "abd.promotion-record",
  "promotionRecordId": "promotion_7a8b9c",
  "promotedAtIso8601": "2026-09-24T07:23:00+02:00",
  "sourceExplorationId": "exp_sess_7a8b9c",
  "sourceExplorationHash": "sha256:...",
  "promotedRecipeDocumentHash": "sha256:...",
  "promotionToolVersion": "1.0.0"
}
```

### 2.3. Integración en `RecipeExecutionController`
Se amplía `RecipeExecutionController`:
```cpp
RecipePreparationState promoteExploration(const profiling::MeasurementRecipe& promotedRecipe,
                                         const std::string& recipeDocumentHash);
```
- Establece `baseRecipe` y `workingRecipe` con la receta promovida.
- Fija la vista inicial en `RecipeEditorView::Configurable`.
- Deriva y compila deterministamente el `ExperimentPlan`.
- **Invarianza rectora:** No crea `ProfilingSession`, no inicia audio y no altera la sesión activa en el motor de audio.
- Deja la receta lista para inspección o ajuste por el operador en el Paso 2.

### 2.4. Integración en la Interfaz (Toma Libre y Navegación en el Stepper)
- En `MainContentComponent`, dentro de la barra de gobernanza cuando `getWorkspaceInteractionMode() == Free`:
  - Botón `"Promover a receta"` (`btnPromoteToRecipe` / `gui::strings::PROMOTE_TO_RECIPE`).
  - Habilitado únicamente cuando existe una exploración activa o recientemente cerrada (`activeMeasurementSession != nullptr` o contexto disponible).
  - Al pulsar:
    1. Construye el `ExplorationContext` desde `SessionExecutionCoordinator`.
    2. Ejecuta `RecipePromotionService::promoteExploration(...)`.
    3. Si `isExecutableRecipe()` es verdadero (Caso A):
       - Inyecta la receta en `recipeExecutionController.promoteExploration(...)`.
       - Conmuta la UI al **Paso 2 (`Step::CalibrateLoopback` / "2. Calibration & Setup")**, donde en la composición actual reside el panel de revisión `RecipeEditorComponent` en vista `Configurable` (desacoplado del Paso 1 `Step::HardwareRouting`, que preserva intacto el target y conexionado observados sin obligar al usuario a reseleccionarlo).
       - Muestra banner informativo:  
         *"Nueva receta temporal derivada de exploración en Paso 2 (Calibration & Setup). La exploración original no ha sido certificada como medición. Esta receta no se ha guardado aún en disco."*
    4. Si `isExecutableRecipe()` es falso (Caso B - Incompleta):
       - Muestra diálogo/banner con el diagnóstico `ERR_PROMOTION_CONTROL_UNDECLARED`:  
         *"La exploración se ha conservado. No puede convertirse todavía en una receta ejecutable porque faltan puntos de control declarados. Opciones: elegir un preset existente, seleccionar un conjunto de puntos normativo o esperar a TargetProfile."*
       - No permite avanzar al Paso 3 ni preparar prueba.

---

## 3. Batería de Pruebas Normativas (`test_FreeCaptureRecipePromotion.cpp`)

Se implementarán 11 contratos normativos:

| # | Test Case | Contrato Verificado |
|---|---|---|
| 1 | `Promotion: Extracción Válida de Parámetros Observados` | Target, notas y controles observados se serializan en una `MeasurementRecipe` 100% conforme a Draft 2020-12. |
| 2 | `Promotion: Trazabilidad Criptográfica de Procedencia` | `authoringSource` es `"promoted_from_exploration"`, `sourceExplorationHash` registra la exploración de origen, `sourceRecipeDocumentHash` permanece vacío y genera hashes RFC 8785 deterministas. |
| 3 | `Promotion: Resiliencia ante Controles No Declarados (Sin Invención)` | Exploraciones con `confirmationStatus == "unknown"` no inventan parámetros (`filter_cutoff`), emiten `ERR_PROMOTION_CONTROL_UNDECLARED` y devuelven `PromotionDraft` no ejecutable. |
| 4 | `Promotion: Carga en RecipeExecutionController` | `promoteExploration()` puebla `workingRecipe`, fija la vista en `Configurable` y deriva un `ExperimentPlan` determinista. |
| 5 | `Promotion: Invarianza de Estado (Sin Ejecución Prematura)` | Promover una exploración NO crea sesión ejecutable, no inicia audio y no arranca el secuenciador. |
| 6 | `Promotion: Edición Posterior de la Receta Promovida` | La receta promovida en memoria admite ajustes posteriores (ej. repeticiones, nota, gate) actualizando deterministamente sus hashes en memoria. |
| 7 | `Promotion: Preparación Confirmada en el Stepper` | Tras validar el entorno físico en el Paso 3, la confirmación formal del operador crea una `ProfilingSession` clasificada como `Measurement`. |
| 8 | `Promotion: Blindaje e Inmutabilidad de la Exploración Original` | La sesión de Toma Libre original permanece clasificada como `Exploration` y bloqueada para exportación de producción. |
| 9 | `Promotion: Promoción con Control Declarado` | Genera una `WorkingRecipe` válida y completa cuando los controles tienen `semanticId` y valor normalizado declarado por el operador. |
| 10 | `Promotion: Promoción Incompleta Bloquea Preparación` | Un `PromotionDraft` no puede alcanzar el Paso 3 como receta preparada, no habilita "Preparar prueba" y no genera `experimentPlanHash` ejecutable. |
| 11 | `Promotion: Navegación a Revisión de Recipe en el Stepper` | Tras pulsar "Promover a Receta Formal", `RecipeEditorComponent` se muestra en el Paso 2 (`Step::CalibrateLoopback`) en modo `Configurable`, sin obligar a reseleccionar target y permitiendo continuar a preflight. |

---

## 4. Archivos Afectados

### Nuevos Archivos:
- `src/profiling/RecipePromotionService.h`
- `src/profiling/RecipePromotionService.cpp`
- `src/tests/test_FreeCaptureRecipePromotion.cpp`
- `PLAN_HITO_09D_FREE_CAPTURE_PROMOTION.md`

### Archivos a Modificar:
- `docs/contracts/measurement-recipe.schema.json`: Ampliar enum de `authoringSource` y admitir campos de exploración (`sourceKind`, `sourceExplorationId`, `sourceExplorationHash`, etc.).
- `src/profiling/MeasurementRecipe.h`: Actualizar `RecipeProvenance` con campos de exploración.
- `src/profiling/MeasurementRecipeService.cpp`: Registrar nuevas claves de proveniencia en `kAllowedProvKeys` y en serialización RFC 8785.
- `src/gui/session/UiStrings.h`: Nuevas cadenas `PROMOTE_TO_RECIPE`, `TOOLTIP_PROMOTE_TO_RECIPE`.
- `src/gui/recipes/RecipeExecutionController.h` / `.cpp`: Añadir método `promoteExploration()`.
- `src/gui/MainContentComponent.h` / `.cpp`: Añadir `btnPromoteToRecipe` en el modo libre y conectar el flujo condicional (Caso A vs Caso B).
- `CMakeLists.txt`: Registrar `RecipePromotionService.cpp` y `test_FreeCaptureRecipePromotion.cpp`.
