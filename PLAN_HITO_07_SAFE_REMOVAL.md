# Plan de Retirada Segura de Duplicados y Componentes Legacy — HITO-07

**Proyecto:** ABDAudioLab (Universal Black-Box Musical Hardware & Synth Profiler)  
**Documento:** PLAN_HITO_07_SAFE_REMOVAL.md  
**Hito:** HITO-07-SAFE-REMOVAL-DUPLICATES  
**Versión:** 1.0.0  
**Fecha de Creación:** 2026-09-22  
**Estado:** PROPUESTA DE ARQUITECTURA — LISTO PARA REVISIÓN  

---

## 1. Misión y Objetivos

El **HITO-07** tiene como propósito la retirada limpia, segura y progresiva de componentes de interfaz gráfica duplicados y contenedores paralelos heredados de fases tempranas de experimentación, consolidando la arquitectura del Stepper canónico integrado de 5 pasos (Pasos 0 a 4) y eliminando código muerto sin generar regresiones en el flujo metrológico, la telemetría canónica ni la suite de tests.

### Componentes en Alcance de Auditoría y Retirada:
1. **`SoundIdGuidedWorkflowContainer`**: Contenedor bifurcado de 3 pasos (aloja duplicados de top header, target view, run view y results view).
2. **`LoopbackCalibrationModal` (`loopbackModal`)**: Diálogo modal legacy de calibración (completamente sustituido por `NativeCalibrationPanel` y `CanonicalCalibrationState`).
3. **`WorkflowStepperBar` (`stepperBar`)**: Instancia de UI del stepper horizontal (sustituido visualmente por `SoundIdSidebarStepper` y `WorkflowNavigationController`).
4. **`HardwareRoutingPanel` (`hardwareRoutingPanel`)**: Panel de routing legacy con fallbacks defensivos de target.

---

## 2. Principios No Negociables y Criterios de No Regresión

1. **Inmutabilidad Metrológica y DSP:** Cero cambios en algoritmos matemáticos, excitación, de-convolución Farina, SNR, THD ni audio engine.
2. **Conservación de Persistencia y Hashes:** El formato `.abdlabtest`, `manifest.json` y los cálculos SHA-256 / RFC 8785 no se modifican.
3. **Preservación de Seams Certificados:** `SEAM-01` a `SEAM-06` certificados en HITO-06 se mantienen idénticos.
4. **Cero Regresión en Suites Automatizadas:**
   - La suite completa de 617 casos de prueba debe mantenerse 100% en verde: **609 PASS / 8 SKIPPED (justificados) / 0 FAIL / 228.921 aserciones**.
   - Los 8 skips (COM/WASAPI singleton en `[ui_governance]`) deben permanecer estrictamente registrados como justificados.
5. **Retirada Atómica por Fases:** No se borrará todo en un único commit masivo. Cada componente se retirará en una fase aislada con validación de compilación y tests.

---

## 3. Plan de Retirada por Fases (Secuencia Operativa)

### Fase 1: Retirada de `SoundIdGuidedWorkflowContainer` (Riesgo Bajo)
- **Diagnóstico:** Componente inactivo, no utilizado por ningún test. Se activa únicamente si `currentWorkflowMode == Guided`, pero el toggle `btnWorkflowModeToggle` está oculto.
- **Acciones:**
  1. Retirar la instanciación de `guidedWorkflowContainer` y el toggle `btnWorkflowModeToggle` de `MainContentComponent.h` y `MainContentComponent.cpp`.
  2. Retirar las llamadas condicionales en `resized()`, `updateTelemetry()` y `setWorkflowMode()`.
  3. Eliminar los archivos físicos `src/gui/soundid/SoundIdGuidedWorkflowContainer.h` y `src/gui/soundid/SoundIdGuidedWorkflowContainer.cpp`.
  4. Limpiar las entradas correspondientes en `CMakeLists.txt`.
- **Validación:** Compilación Release limpia + suite global (617 tests).

---

### Fase 2: Retirada de `LoopbackCalibrationModal` (Riesgo Bajo)
- **Diagnóstico:** Suscripción de callbacks residuales en `MainContentComponent.cpp` (L1429-1469). El componente nunca se hace visible; la calibración real ocurre en `NativeCalibrationPanel` (Paso 2) y la telemetría se nutre de `CanonicalCalibrationState`.
- **Acciones:**
  1. Eliminar el miembro `loopbackModal` de `MainContentComponent.h`.
  2. Retirar los lambdas de `onCalibrationApplied` / `onCalibrationSkipped` de `loopbackModal` y el `addChildComponent(loopbackModal)` de `MainContentComponent.cpp`.
  3. Actualizar `test_DiagnosticsTelemetryPoller.cpp` retirando el test de comprobación de modal destruida (ya no aplica al no existir `loopbackModal`).
  4. Eliminar los archivos físicos `src/gui/LoopbackCalibrationModal.h` y `src/gui/LoopbackCalibrationModal.cpp`.
  5. Limpiar las entradas en `CMakeLists.txt`.
- **Validación:** Compilación Release limpia + filtro `[diagnostics]` + suite global.

---

### Fase 3: Desacoplamiento de la Instancia UI `stepperBar` (Riesgo Medio)
- **Diagnóstico:** La barra horizontal `stepperBar` está oculta (`setVisible(false)`). Sin embargo, sus enums `WorkflowStepperBar::Step` y `WorkflowStepperBar::StepStatus` son usados profusamente en `LoadedSessionApplier.h` y en tests (`test_E2E_HermeticWorkflows.cpp`, `test_LoadedSessionApplier.cpp`, `test_SmokeStep4UI.cpp`).
- **Acciones:**
  1. **Conservar Enums/Tipos:** Mantener `WorkflowStepperBar::Step` y `StepStatus` (o extraerlos a una cabecera de tipos de workflow si se decide desacoplar la vista del modelo).
  2. **Eliminar la instancia gráfica:** Retirar el miembro `stepperBar` de `MainContentComponent.h`.
  3. **Eliminar llamadas espejo:** Purgar todas las llamadas `stepperBar.setStepStatus(...)`, `stepperBar.setCurrentStep(...)`, `stepperBar.repaint()` en `MainContentComponent.cpp`, confiando exclusivamente en `workflowNavController` y `SoundIdSidebarStepper`.
  4. Limpiar callbacks espejo (`stepperBar.onStepSelected`).
- **Validación:** Compilación Release limpia + filtro `[stepper]` + filtro `[e2e]` + suite global.

---

### Fase 4: Auditoría y Desacoplamiento de `hardwareRoutingPanel` (Riesgo Alto)
- **Diagnóstico:** Panel oculto que contiene fallbacks de target (`getSelectedHardwareId()`) invocados por `startProfilingSession`, `loadSessionFromDisk`, etc.
- **Acciones:**
  1. Auditar cada uno de los fallbacks `if (hwId.isEmpty()) hwId = hardwareRoutingPanel.getSelectedHardwareId();`.
  2. Reemplazarlos por la consulta a la autoridad canónica:
     `profilingSessionController.getCurrentSnapshot().target.targetId`.
  3. Desvincular callbacks de `hardwareRoutingPanel` en `MainContentComponent.cpp`.
  4. Retirar el miembro `hardwareRoutingPanel` y su `addChildComponent`.
  5. Retirar los archivos `src/gui/HardwareRoutingPanel.h/.cpp` y actualizar `CMakeLists.txt`.
- **Validación:** Compilación Release limpia + suite global + validación E2E en los 4 arquetipos de hardware/plugin.

---

### Fase 5: Verificación Global y Certificación de HITO-07
- **Acciones:**
  1. Ejecución de la suite completa: 617 casos totales (609 PASS / 8 SKIPPED / 0 FAIL / 228.921 aserciones).
  2. Ejecución del smoke visual interactivo en `ABDAudioLab.exe`.
  3. Generación de `ACTA_HITO_07_SAFE_REMOVAL.md`.
  4. Actualización de `docs/ROADMAP.md` y `TASK.txt`.

---

## 4. Matriz de Controles y Tests de Guardia

| Control ID | Área Afectada | Criterio de Guardia | Test Asociado |
|---|---|---|---|
| **CG-01** | `SoundIdGuidedWorkflowContainer` | Cero referencias restantes en código y CMake | Compilación `Release` sin advertencias |
| **CG-02** | `LoopbackCalibrationModal` | Calibración opera 100% en `NativeCalibrationPanel` | `[diagnostics]` (117 assertions) |
| **CG-03** | `stepperBar` | Navegación de 5 pasos intacta vía `WorkflowNavigationController` | `[stepper]` (89 assertions) + `[e2e]` (284 assertions) |
| **CG-04** | `hardwareRoutingPanel` | Cero fallbacks a panel inactivo; target siempre canónico | `test_E2E_HermeticWorkflows.cpp` |
| **CG-05** | Regresión Global | Métricas idénticas a baseline HITO-06 | 617 casos / 609 PASS / 8 SKIP / 0 FAIL |

---

## 5. Próximo Paso Recomendado

Una vez revisada y aprobada esta propuesta por el usuario:
- **Paso 1:** Iniciar la **Fase 1** (Retirada de `SoundIdGuidedWorkflowContainer`).
- **Paso 2:** Compilar en Release y ejecutar suite completa para certificar el paso antes de avanzar a la Fase 2.
