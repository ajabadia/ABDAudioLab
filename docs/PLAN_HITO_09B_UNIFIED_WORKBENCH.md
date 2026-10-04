# PLAN OPERATIVO: HITO-09B — Banco de Trabajo Unificado como Adaptador de Presentación

**Estado:** 🟢 APROBADO PARA IMPLEMENTACIÓN (2026-09-23)  
**Dependencias:** HITO-09A (Certificado — Build #419 / 624 tests PASS)  
**Autoridad:** Antigravity (Lead Architect & Metrologist) / ajabadia (Tándem Partner)

---

## 1. Principio Rector y Declaración Fundamental

> **El Banco de Trabajo Unificado no es un nuevo motor de medición ni un tercer flujo de ejecución paralelo:**  
> Es un **adaptador visual verificable** del contrato declarativo:  
> `MeasurementRecipe` $\to$ `ExperimentPlan` $\to$ `ResolvedExecutionPlan` $\to$ `ProfilingSession`.

La aplicación mantiene una **única autoridad de ejecución y evaluación**:
```
Guiado (SoundID 3-Step) ──┐
Libre (Classic SuiteList) ┼─→ ProfilingSession (semánticamente equivalente)
Banco de Trabajo (HITO-09B)┘          ↓
                               mismo preflight existente
                                      ↓
                               mismo ProfilingSequencer
                                      ↓
                               mismo audio / DSP (tiempo real)
                                      ↓
                             misma evaluación (EvaluationSnapshot)
                                      ↓
                             misma exportación (ProductionPackage)
```

La nueva pantalla no es “otra aplicación dentro de la aplicación”. Es simplemente una manera estructurada, declarativa y reproducible de responder a la pregunta dentro del flujo de pasos existente:
> *«¿Qué prueba científica quieres configurar y ejecutar?»*

---

## 2. Integración en el Ciclo de Pasos Existente: Elegir Receta $\neq$ Crear Sesión $\neq$ Ejecutar

El Banco de Trabajo Unificado no sustituye el recorrido de pasos; enriquece la selección y preparación de la prueba integrándose en el ciclo existente:

```
[Paso 1: Target]
   │  El usuario selecciona sintetizador, plugin VST3 o dispositivo outboard.
   ▼
[Paso 2: Selección e Inspección de Receta — BANCO DE TRABAJO]
   │  El usuario selecciona una receta declarativa integrada:
   │  • VCF Rápido (3 puntos)
   │  • VCF Estándar (11 puntos)
   │  • Sintetizador Exhaustivo (barrido completo multi-nota / multi-parámetro)
   │  MeasurementRecipeService valida y ExperimentPlanCompiler compila la propuesta.
   │  RecipeSummaryCardComponent muestra el contenido estructurado y el experimentPlanHash.
   │  [REGLA: La selección NO crea ProfilingSession, NO inicia audio y NO reemplaza la sesión activa].
   ▼
[Paso 3: Revisión, Compatibilidad y Preflight]
   │  La aplicación evalúa resolveExecutionPlan() contra el target y entorno actuales:
   │  • Qué parámetros y notas se excitarán;
   │  • Tiempos de gate, settling y repeticiones;
   │  • Si el target es compatible (sample rates, canales, capabilities);
   │  • Si falta calibración o si el preflight está bloqueado.
   │  El usuario confirma explícitamente "Preparar sesión / Continuar".
   │  [SÓLO TRAS CONFIRMACIÓN: Se crea una nueva ProfilingSession limpia y se inyecta en ProfilingSessionController].
   ▼
[Paso 4: Ejecución]
   │  El ProfilingSequencer existente ejecuta la sesión mediante el hilo de audio de producción.
   ▼
[Paso 5: Resultados]
   │  EvaluationSnapshot captura las métricas metrológicas (ESR, correlación, THD, SNR).
   ▼
[Paso 6: Evidencia]
   │  Manifest, traza de 4 hashes canónicos y ProductionPackage.
```

### Reglas de Aislamiento y No-Bifurcación
1. **Seleccionar receta $\neq$ crear sesión $\neq$ ejecutar:**
   - La selección en el catálogo solo genera un estado de preparación (`RecipePreparationState`).
   - Nunca inicia el secuenciador ni salta etapas del Stepper.
2. **"Misma ProfilingSession" significa misma semántica y pipeline canónico, no la misma instancia C++:**
   - Cada preparación genera una instancia nueva y limpia; no se reutilizan instancias previas si arrastran estado de calibración, ensayos previos o evidencia sucia.
3. **Invarianza Estricta de Componentes Heredados:**
   - Se permiten archivos nuevos bajo `src/gui/recipes/`.
   - Se prohíben modificaciones en los componentes visuales protegidos:
     `btnModeToggle`, `SoundIdProfilingRunView`, `suiteList`, `btnFreeCapture`, `src/gui/soundid/`, `src/gui/suite/`.

---

## 3. Alcance y Exclusiones de HITO-09B

### Incluido
1. **Catálogo visual de recetas integradas (`RecipeCatalogModel`, `RecipeCatalogComponent`):** Carga automática desde `presets/profiling/` a través de `MeasurementRecipeService`.
2. **Selección de las tres recetas normativas:**
   - `quick_vcf_3pts.json` (Nivel: Rápido)
   - `standard_vcf_11pts.json` (Nivel: Configurable)
   - `exhaustive_synth_full.json` (Nivel: Avanzado)
3. **Tarjeta de Inspección y Resumen (`RecipeSummaryCardComponent`):**
   - Nombre, ID de receta y nivel de asistencia.
   - Excitación: notas MIDI, velocidades, compuerta `gateMs`, reposo `settlingMs`, repeticiones.
   - Puntos de medición: parámetros y valores normalizados.
   - Requisitos de entorno: canales y sample rates permitidos.
   - Políticas: calibración requerida y umbrales de evaluación (SNR, THD, tolerancia f0).
   - Identidad criptográfica: `recipeDocumentHash` y `experimentPlanHash` compilado (abreviado, con opción de copiar).
4. **Verificación de Compatibilidad de Entorno en Tiempo Real:**
   - Invoca `ExperimentPlanCompiler::resolveExecutionPlan(plan, env, &recipe)`.
   - Diagnósticos RFC 6901 explícitos en pantalla si el target actual carece de capabilities o sample rate compatible (ej. `ERR_CAPABILITY_SAMPLE_RATE_UNSUPPORTED`).
5. **Coordinador de Preparación (`RecipeExecutionController`):**
   - Administra el ciclo: `selectRecipe` $\to$ `resolveAgainstCurrentEnvironment` $\to$ `prepareSessionAfterConfirmation`.
   - Entrega la `ProfilingSession` al controlador existente únicamente cuando el usuario confirma la preparación en el flujo normal.
6. **Batería de Pruebas de Paridad y Comportamiento:**
   - Paridad tripartita (carga, compilación, resolución).
   - Verificación de no-creación prematura de sesión.
   - Verificación de preflight y preparación confirmada.

### Excluido (Postergado a HITO-09C o HITO-10)
- Guardar, abrir, importar o editar JSON desde la interfaz de usuario.
- Creación de recetas nuevas desde la UI.
- Retirada de los flujos Guided o Classic.
- Modificación de la máquina de estados de ejecución o de la lógica de tiempo real.
- Creación de `TargetProfile` declarativos (alcance de HITO-10).

---

## 4. Arquitectura de Componentes Propuesta

```
                         MeasurementRecipeService
                                    │
                              (carga y valida)
                                    ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│                    src/gui/recipes/ (Nuevos Componentes)                     │
│                                                                              │
│   ┌──────────────────────────┐           ┌───────────────────────────────┐   │
│   │    RecipeCatalogModel    │           │  RecipeSummaryCardComponent   │   │
│   │                          │           │                               │   │
│   │ • Descubre presets       │           │ • Resumen legible de receta   │   │
│   │ • Gestiona estado por    │──────────>│ • Excitación / Medición       │   │
│   │   receta integrada       │ (notifica)│ • experimentPlanHash          │   │
│   └─────────────┬────────────┘           │ • Estado de compatibilidad    │   │
│                 │                        └───────────────┬───────────────┘   │
│                 ▼                                        │                   │
│   ┌──────────────────────────┐                           │                   │
│   │  RecipeCatalogComponent  │                           ▼                   │
│   │                          │           ┌───────────────────────────────┐   │
│   │ • Lista visual (Quick,   │──────────>│   RecipeExecutionController   │   │
│   │   Standard, Exhaustive)  │(selección)│                               │   │
│   └──────────────────────────┘           │ • selectRecipe()              │   │
│                                          │ • resolveCurrentEnvironment() │   │
│                                          │ • prepareAfterConfirmation()  │   │
│                                          └───────────────┬───────────────┘   │
└──────────────────────────────────────────────────────────┼───────────────────┘
                                                           ▼
                                            ProfilingSessionController
                                            (Controlador Existente del Stepper)
                                                           ↓
                                                   ProfilingSequencer
```

### 4.1. Contrato del Controlador de Preparación
```cpp
namespace abdaudiolab::gui::recipes
{

struct RecipePreparationState
{
    profiling::MeasurementRecipe recipe;
    std::string recipeDocumentHash;

    synth::ExperimentPlan experimentPlan;
    std::string experimentPlanHash;

    std::optional<profiling::ResolvedExecutionPlan> resolvedPlan;
    std::optional<std::string> resolvedExecutionPlanHash;

    std::vector<profiling::ValidationDiagnostic> diagnostics;
    bool isEnvironmentCompatible { false };
    bool isSessionPrepared { false };
};

struct PreparationResult
{
    bool success { false };
    std::string errorMessage;
    std::optional<core::ProfilingSession> preparedSession;
};

class RecipeExecutionController
{
public:
    explicit RecipeExecutionController(session::ProfilingSessionController& sessionController);
    ~RecipeExecutionController() = default;

    RecipePreparationState selectRecipe(const profiling::MeasurementRecipe& recipe,
                                        const std::string& recipeDocumentHash);

    RecipePreparationState resolveAgainstCurrentEnvironment(const profiling::ExecutionEnvironment& env,
                                                            const gui::session::TargetSelectionState& targetState);

    PreparationResult prepareSessionAfterConfirmation(const gui::session::TargetSelectionState& targetState);

    [[nodiscard]] const RecipePreparationState& getCurrentState() const noexcept { return currentState_; }
    void clearPreparation() noexcept;

private:
    session::ProfilingSessionController& sessionController_;
    RecipePreparationState currentState_;
};

} // namespace abdaudiolab::gui::recipes
```

---

## 5. Protocolo de Pruebas de Paridad y Validación

La suite `test_UnifiedWorkbenchRecipeAdapter.cpp` implementará 7 pruebas formales:

### 1. Paridad de Carga
```
Mismo archivo integrado en disco
  → Carga visual mediante RecipeCatalogModel
  → Carga headless mediante MeasurementRecipeService
  → recipeDocumentHash idéntico bit a bit
  → MeasurementRecipe resultante con campos idénticos
```

### 2. Paridad de Compilación
```
Misma MeasurementRecipe seleccionada
  → Compilación disparada desde RecipeExecutionController::selectRecipe()
  → Compilación directa con ExperimentPlanCompiler::compileToExperimentPlan()
  → experimentPlanHash idéntico
  → Mismo número y orden de TargetEvents y ObservationWindows
```

### 3. Paridad de Resolución de Entorno
```
Mismo ExperimentPlan + mismo ExecutionEnvironment
  → Resolución coordinada por RecipeExecutionController::resolveAgainstCurrentEnvironment()
  → Resolución directa headless con ExperimentPlanCompiler::resolveExecutionPlan()
  → resolvedExecutionPlanHash idéntico
  → ResolveExecutionPlanResult idéntico
```

### 4. Diagnóstico de Incompatibilidad de Target sin Bloqueo
- Si el target admite 44.100 Hz y la receta exige 48.000 Hz:
  - La receta se muestra como válida documentalmente.
  - La resolución emite `ERR_CAPABILITY_SAMPLE_RATE_UNSUPPORTED` con puntero `/environment/sampleRate`.
  - La UI informa la causa exacta y no permite preparar la sesión, sin bloquear el catálogo.

### 5. Resiliencia ante Recetas Corruptas
- Si existe una receta corrupta en el directorio de presets:
  - El catálogo la marca como no disponible con su error correspondiente.
  - No construye un plan parcial ni lanza excepciones.
  - Las demás recetas del catálogo continúan operando con total normalidad.

### 6. Invarianza: Selección de Receta NO Crea Sesión Ejecutable
- Dado: una receta seleccionada en el catálogo;
- Entonces:
  - No se inicia `ProfilingSequencer`.
  - No se crea `ExecutionRecord`.
  - No se produce audio ni se abren dispositivos.
  - El Stepper no salta a Ejecución.
  - La sesión activa existente no se reemplaza silenciosamente.

### 7. Preparación Confirmada
- Dado: una receta seleccionada, target compatible y confirmación en el punto adecuado del Stepper;
- Entonces:
  - Crea una única `ProfilingSession` semánticamente equivalente a la ruta headless.
  - Pasa por el preflight existente.
  - Respeta el orden de transición del Stepper.
  - Genera el mismo `resolvedExecutionPlanHash` que la ruta headless.

---

## 6. Criterio de Salida de HITO-09B

Para emitir el acta formal de cierre de HITO-09B se requerirá:
1. Las 3 recetas normativas (`quick_vcf_3pts`, `standard_vcf_11pts`, `exhaustive_synth_full`) integradas y visibles en el catálogo.
2. Cada receta cargada y validada exclusivamente por `MeasurementRecipeService`.
3. `RecipeSummaryCardComponent` renderiza el contenido estructurado sin inventar lógica científica.
4. La selección de receta no crea sesiones ni ejecuta audio.
5. El Paso 3 del Stepper conserva la autoridad de revisión, compatibilidad, calibración y preflight.
6. Solo la confirmación en el Stepper crea la `ProfilingSession`.
7. Paridad demostrada en test: selección visual y carga headless producen idénticos `recipeDocumentHash`, `experimentPlanHash` y `resolvedExecutionPlanHash`.
8. CERO modificaciones en `btnModeToggle`, `SoundIdProfilingRunView`, `suiteList`, `btnFreeCapture`.
9. Ninguna regresión en las suites anteriores (`PARITY-01`, `CONVERGENCIA-01`, `INTEGRATION-01`, `HITO-09A`).
10. Suite global en Build Release: 0 FAIL y los 8 SKIPPED justificados por aislamiento COM/WASAPI.
