# Matriz de Dependencias y Auditoría de Duplicados — HITO-07

**Proyecto:** ABDAudioLab (Universal Black-Box Musical Hardware & Synth Profiler)  
**Documento:** MATRIX_HITO_07_DEPENDENCIES.md  
**Hito Asociado:** HITO-07-SAFE-REMOVAL-DUPLICATES  
**Versión:** 1.0.0  
**Fecha de Creación:** 2026-09-22  
**Estado:** AUDITORÍA COMPLETADA — FASE DE PLANIFICACIÓN  

---

## 1. Inventario de Símbolos y Componentes Auditados

| Símbolo | Ubicación de Archivo | Categoría de Artefacto | Estado Visual Actual |
|---|---|---|---|
| `SoundIdGuidedWorkflowContainer` | `src/gui/soundid/SoundIdGuidedWorkflowContainer.h/.cpp` | Contenedor alternativo bifurcado (3 pasos) | Inactivo / Invisible (`btnWorkflowModeToggle` oculto) |
| `LoopbackCalibrationModal` (`loopbackModal`) | `src/gui/LoopbackCalibrationModal.h/.cpp` | Diálogo modal de calibración legacy | Inactivo / Invisible (100% huérfano) |
| `WorkflowStepperBar` (`stepperBar`) | `src/gui/WorkflowStepperBar.h/.cpp` | Stepper horizontal clásico | Inactivo / Invisible (`setVisible(false)`) |
| `HardwareRoutingPanel` (`hardwareRoutingPanel`) | `src/gui/HardwareRoutingPanel.h/.cpp` | Panel de enrutamiento hardware legacy | Inactivo / Invisible (reemplazado por catalogSelector + targetView) |

---

## 2. Matriz Exhaustiva de Dependencias y Consumidores

| Símbolo | Consumidores Directos | Tipo de Acoplamiento | Sustituto Canónico Oficial | Nivel de Riesgo | Acción Planificada |
|---|---|---|---|---|---|
| **`SoundIdGuidedWorkflowContainer`** | Cero (`0` referencias restantes) | Ninguno (archivos eliminados) | Flujo Stepper canónico (Paso 0 a 4) + `SoundIdSidebarStepper` | **PASS (Retirada Certificada)** | **Fase 1 completada:** Miembro, layout, telemetría y archivos `.h/.cpp` eliminados; CMake limpio |
| **`LoopbackCalibrationModal` (`loopbackModal`)** | Cero (`0` referencias funcionales restantes) | Ninguno (archivos eliminados) | `NativeCalibrationPanel` (Paso 2) + `CanonicalCalibrationState` | **PASS (Retirada Certificada)** | **Fase 2 completada:** Miembro, callbacks, `addChildComponent`, test modal y archivos `.h/.cpp` eliminados; CMake limpio |
| **`WorkflowStepperBar` (`stepperBar`)** | `MainContentComponent.h/.cpp`, `LoadedSessionApplier.h`, `test_LoadedSessionApplier.cpp`, `test_E2E_HermeticWorkflows.cpp`, `test_SmokeStep4UI.cpp` | Miembro embebido en GUI; Enums de tipos (`Step`, `StepStatus`) usados en lógica de sesión y tests | `SoundIdSidebarStepper` + `WorkflowNavigationController` + `CanonicalWorkflowState` | **Alto** (Riesgo de rotura de compilación por enums compartidos) | **Fase 3:** Conservar definiciones de enum/tipos (o migrar a `CanonicalWorkflowState`), desacoplar instancia GUI de `MainContentComponent` |
| **`HardwareRoutingPanel` (`hardwareRoutingPanel`)** | `MainContentComponent.h/.cpp` (múltiples fallbacks `if (hwId.isEmpty()) hwId = hardwareRoutingPanel.getSelectedHardwareId()`) | Miembro embebido, callbacks de selección, fallbacks de consulta | `SoundIdHardwareCatalogSelector` (`catalogSelector`) + `ProfilingSessionController::selectTarget()` | **Alto** (Fallbacks de consulta de hardware y locks) | **Fase 4:** Purgar fallbacks redirigiendo a `catalogSelector` / `ProfilingSessionController`, luego retirar panel |

---

## 3. Desglose Detallado por Componente

### 3.1. `SoundIdGuidedWorkflowContainer`
- **Ruta de Archivos:** `src/gui/soundid/SoundIdGuidedWorkflowContainer.h`, `src/gui/soundid/SoundIdGuidedWorkflowContainer.cpp` (**ELIMINADOS**).
- **Referencias Restantes:** 0 en todo el repositorio (`src` y `CMakeLists.txt`).
- **Sustituto Canónico:** Flujo Stepper integrado (Paso 0 a 4) + `SoundIdSidebarStepper` + `WorkflowNavigationController`.
- **Estado de Fase 1:** **PASS (Retirada Certificada)**.
- **Evidencia Automatizada:**
  * Build Release: Clean / Exit code 0 (Build #394).
  * `[e2e]`: 8 casos / 284 aserciones PASS.
  * `[stepper]`: 25 casos / 89 aserciones PASS.
  * `[diagnostics]`: 5 casos / 117 aserciones PASS.
  * Suite Global: 617 casos totales (609 PASS / 8 SKIPPED justificados / 0 FAIL / 228.921 aserciones).

### 3.2. `LoopbackCalibrationModal` (`loopbackModal`)
- **Ruta de Archivos:** `src/gui/LoopbackCalibrationModal.h`, `src/gui/LoopbackCalibrationModal.cpp` (**ELIMINADOS**).
- **Referencias Restantes:** 0 referencias de código funcional en todo el repositorio (`src` y `CMakeLists.txt`).
- **Sustituto Canónico:** `NativeCalibrationPanel` (Paso 2) + `CanonicalCalibrationState`.
- **Estado de Fase 2:** **PASS (Retirada Certificada)**.
- **Evidencia Automatizada:**
  * Build Release: Clean / Exit code 0 (Build #395).
  * `[diagnostics]`: 5 casos / 116 aserciones PASS (test actualizado validando estado canónico sin modales).
  * `[e2e]`: 8 casos / 284 aserciones PASS.
  * `[stepper]`: 25 casos / 89 aserciones PASS.
  * Suite Global: 617 casos totales (609 PASS / 8 SKIPPED justificados / 0 FAIL / 228.920 aserciones).

### 3.3. `WorkflowStepperBar` (`stepperBar`)
- **Ruta de Archivos:** `src/gui/WorkflowStepperBar.h`, `src/gui/WorkflowStepperBar.cpp`.
- **Peligro Crítico Identificado:**
  - La clase define enums normativos: `WorkflowStepperBar::Step` y `WorkflowStepperBar::StepStatus`.
  - Estos enums son importados y utilizados por:
    - `LoadedSessionApplier.h` (`SessionWorkflowApplyState`).
    - `test_LoadedSessionApplier.cpp` (14 aserciones).
    - `test_E2E_HermeticWorkflows.cpp` (6 aserciones en los 4 arquetipos E2E).
    - `test_SmokeStep4UI.cpp` (3 aserciones).
- **Estrategia de Retirada Segura:**
  - No borrar físicamente la cabecera `WorkflowStepperBar.h` mientras los enums se utilicen en contratos de sesión.
  - Retirar la **instancia GUI** `stepperBar` de `MainContentComponent` (que solo recibía llamadas espejo y estaba en `setVisible(false)`).
  - Si se extraen los enums, deben migrarse a un header puro (p.ej. `CanonicalWorkflowTypes.h` o `CanonicalWorkflowState.h`) manteniendo typedefs de compatibilidad para cero roturas.

### 3.4. `HardwareRoutingPanel` (`hardwareRoutingPanel`)
- **Ruta de Archivos:** `src/gui/HardwareRoutingPanel.h`, `src/gui/HardwareRoutingPanel.cpp`.
- **Uso en Runtime:** Invisible.
- **Peligro Crítico Identificado:**
  - Existen fallbacks defensivos en `MainContentComponent.cpp`:
    - `if (hwId.isEmpty()) hwId = hardwareRoutingPanel.getSelectedHardwareId();`
  - Métodos como `startProfilingSession`, `loadSessionFromDisk`, etc., consultan este panel como fallback histórico.
- **Estrategia de Retirada Segura:**
  - Auditar y redirigir cada uno de los 6 fallbacks hacia la autoridad canónica: `catalogSelector` o `profilingSessionController.getCurrentSnapshot().target`.
  - Solo tras verificar que ningún flujo recurre a `hardwareRoutingPanel`, retirar el componente visual.

---

## 4. Matriz de Riesgo y Orden de Ejecución

| Fase | Componente Objetivo | Riesgo Técnico | Dependencias Críticas | Criterio de Verificación |
|---|---|---|---|---|
| **Fase 1** | `SoundIdGuidedWorkflowContainer` | **Bajo** | Ningún test dependía de él; modo inactivo | **PASS (Certificado)**: Release Build #394, 0 residuales, 617 tests (609 PASS / 8 SKIP / 0 FAIL) |
| **Fase 2** | `LoopbackCalibrationModal` (`loopbackModal`) | **Bajo** | Callbacks residuales en `MainContentComponent` | **PASS (Certificado)**: Release Build #395, 0 residuales funcionales, 617 tests (609 PASS / 8 SKIP / 0 FAIL / 228.920 aserciones) |
| **Fase 3** | `stepperBar` (instancia de UI) | **Medio** | Desacoplar llamadas espejo; aislar enums normativos en `CanonicalWorkflowTypes.h` | **PASS (Certificado)**: Release Build #398, tipos canónicos extraídos, stepperBar retirado, 615 tests (607 PASS / 8 SKIP / 0 FAIL / 228.813 aserciones) |
| **Fase 4A** | `hardwareRoutingPanel` (Fallbacks) | **Alto** | Sustituir 6 fallbacks con `resolveCanonicalTarget()`, validar target vacío en exportación | **PASS (Certificado)**: Release Build #401, 616 tests (608 PASS / 8 SKIP / 0 FAIL / 228.827 aserciones) |
| **Fase 4B** | `HardwareRoutingPanel` (Física) | **Bajo** | Retirar miembro `hardwareRoutingPanel`, constructor/callbacks y archivos de CMakeLists | Pendiente de autorización |

---

## 5. Criterios de No Regresión Inviolables

1. **Cero impacto en Audio y DSP:** Ni un solo archivo bajo `src/dsp/` ni `src/audio/` será modificado.
2. **Cero alteración de contratos RFC 8785 y Hashes:** Se mantiene intacta la serialización y hashes canónicos.
3. **Cero rotura en Suites Automatizadas:**
   - 617 casos totales.
   - 609 PASS.
   - 8 SKIPPED justificados (COM/WASAPI singletons en UI Governance).
   - 0 FAIL.
   - 228.920 aserciones PASS (Baseline consolidada tras retirada de LoopbackCalibrationModal).
4. **Cero regresión en telemetría canónica:** SEAM-01 a SEAM-06 certificados intactos.
