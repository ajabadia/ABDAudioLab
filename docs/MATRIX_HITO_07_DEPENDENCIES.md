# Matriz de Dependencias y Auditoría de Duplicados — HITO-07

**Proyecto:** ABDAudioLab (Universal Black-Box Musical Hardware & Synth Profiler)  
**Documento:** MATRIX_HITO_07_DEPENDENCIES.md  
**Hito Asociado:** HITO-07-SAFE-REMOVAL-DUPLICATES  
**Versión:** 1.2.0  
**Fecha de Creación:** 2026-09-22  
**Estado:** EJECUCIÓN EN CURSO — FASE 5 PENDIENTE

---

## 1. Inventario de Símbolos y Componentes Auditados

| Símbolo | Ubicación de Archivo | Categoría de Artefacto | Estado |
|---|---|---|---|
| `SoundIdGuidedWorkflowContainer` | `src/gui/soundid/SoundIdGuidedWorkflowContainer.h/.cpp` | Contenedor alternativo bifurcado (3 pasos) | **ELIMINADO** (Fase 1) |
| `LoopbackCalibrationModal` (`loopbackModal`) | `src/gui/LoopbackCalibrationModal.h/.cpp` | Diálogo modal de calibración legacy | **ELIMINADO** (Fase 2) |
| `WorkflowStepperBar` (`stepperBar`) | `src/gui/WorkflowStepperBar.h/.cpp` | Stepper horizontal clásico | Instancia GUI **ELIMINADA** (Fase 3); archivos conservados (tipos normativos) |
| `HardwareRoutingPanel` (`hardwareRoutingPanel`) | `src/gui/HardwareRoutingPanel.h/.cpp` | Panel de enrutamiento hardware legacy | **ELIMINADO** (Fase 4) |

---

## 2. Matriz Exhaustiva de Dependencias y Consumidores

| Símbolo | Consumidores Directos | Tipo de Acoplamiento | Sustituto Canónico Oficial | Estado |
|---|---|---|---|---|
| **`SoundIdGuidedWorkflowContainer`** | 0 referencias | Ninguno (archivos eliminados) | Flujo Stepper canónico + `SoundIdSidebarStepper` | **PASS (Certificado)** |
| **`LoopbackCalibrationModal`** | 0 referencias funcionales | Ninguno (archivos eliminados) | `NativeCalibrationPanel` + `CanonicalCalibrationState` | **PASS (Certificado)** |
| **`WorkflowStepperBar` (`stepperBar`)** | Archivos conservados; instancia GUI eliminada | Tipos migrados a `CanonicalWorkflowTypes.h` | `SoundIdSidebarStepper` + `WorkflowNavigationController` | **PASS (Certificado)** |
| **`HardwareRoutingPanel`** | 0 referencias funcionales | Ninguno (archivos eliminados) | `resolveCanonicalTarget()` + `ProfilingSessionController` + `catalogSelector` | **PASS (Certificado)** |

---

## 3. Desglose Detallado por Componente

### 3.1. `SoundIdGuidedWorkflowContainer`
- **Ruta de Archivos:** `src/gui/soundid/SoundIdGuidedWorkflowContainer.h/.cpp` (**ELIMINADOS**).
- **Referencias Restantes:** 0 en todo el repositorio.
- **Estado de Fase 1:** **PASS (Retirada Certificada)**.
- **Evidencia (Build #394):** Suite Global: 617 casos (609 PASS / 8 SKIP / 0 FAIL).

### 3.2. `LoopbackCalibrationModal` (`loopbackModal`)
- **Ruta de Archivos:** `src/gui/LoopbackCalibrationModal.h/.cpp` (**ELIMINADOS**).
- **Estado de Fase 2:** **PASS (Retirada Certificada)**.
- **Evidencia (Build #395):** Suite Global: 617 casos (609 PASS / 8 SKIP / 0 FAIL / **228.920 assertions**).

### 3.3. `WorkflowStepperBar` (`stepperBar`)
- **Ruta de Archivos:** `src/gui/WorkflowStepperBar.h/.cpp` (conservados; instancia GUI eliminada).
- **Tipos Migrados:** `CanonicalStep`, `CanonicalStepStatus` a `src/gui/controllers/CanonicalWorkflowTypes.h`.
- **Sustituto Canónico:** `SoundIdSidebarStepper` + `WorkflowNavigationController`.
- **Estado de Fase 3:** **PASS (Retirada Certificada)**.
- **Evidencia (Build #398):** Suite Global: 615 casos (607 PASS / 8 SKIP / 0 FAIL / **228.813 assertions**).
- **Delta vs. Fase 2:** -2 casos / -107 assertions (retirada test modal + consolidación tipos).

### 3.4. `HardwareRoutingPanel` (`hardwareRoutingPanel`)
- **Ruta de Archivos:** `src/gui/HardwareRoutingPanel.h/.cpp` (**ELIMINADOS**).

#### Fase 4A — Sustitución de Fallbacks (Build #401)
- `resolveCanonicalTarget()` implementado; 6 interacciones (4 lecturas + 2 sincronización) redirigidas a autoridad canónica.
- `ERR_NO_TARGET_SELECTED` añadido en `ReportExportUiController`.
- **Evidencia:** 616 casos (608 PASS / 8 SKIP / 0 FAIL / **228.827 assertions**).
- **Delta vs. Fase 3:** +1 caso / +14 assertions (nuevos tests de paridad canónica).

#### Fase 4B — Retirada Física (Build #402)
- `HardwareRoutingPanel.h/.cpp` eliminados del disco.
- Include y miembro `gui::HardwareRoutingPanel hardwareRoutingPanel` retirados de `MainContentComponent.h`.
- 31 líneas de callbacks/sincronización eliminadas de `MainContentComponent.cpp`.
- `TEST_CASE("HardwareRoutingPanel - Contract and selection workflow")` retirado del test file.
- Cobertura activa **conservada**: 2 TEST_CASE de `HardwareWiringDiagramComponent` y `HardwareDeviceDisplayCardComponent`.
- CMakeLists.txt: 2 entradas de fuentes eliminadas + test file corregido (conservado con test cases activos).
- Comentario documental obsoleto actualizado en `SoundIdHardwareCatalogSelector.h`.
- `git diff --check` → exit 0 limpio. 0 referencias funcionales residuales en `src/`.
- **Evidencia (Build #402):**
  * `[e2e]`: 284/284 assertions · 8 casos — PASS
  * `[diagnostics]`: 9/9 assertions · 3 casos — PASS
  * `[stepper]`: 89/89 assertions · 25 casos — PASS
  * Suite Global: 615 casos (607 PASS / 8 SKIP / 0 FAIL / **228.818 assertions**)
- **Delta vs. Fase 4A:** -1 caso / -9 assertions (exactamente el TEST_CASE del panel con sus 9 aserciones; los 2 test cases activos conservados confirmados).

---

## 4. Matriz de Riesgo y Orden de Ejecución

| Fase | Componente Objetivo | Riesgo | Build | Resultado |
|---|---|---|---|---|
| **Fase 1** | `SoundIdGuidedWorkflowContainer` | Bajo | #394 | **PASS (Certificado)**: 617 casos (609/8/0) |
| **Fase 2** | `LoopbackCalibrationModal` | Bajo | #395 | **PASS (Certificado)**: 617 casos (609/8/0) / 228.920 assertions |
| **Fase 3** | `stepperBar` (instancia de UI) | Medio | #398 | **PASS (Certificado)**: 615 casos (607/8/0) / 228.813 assertions |
| **Fase 4A** | `hardwareRoutingPanel` (Fallbacks) | Alto | #401 | **PASS (Certificado)**: 616 casos (608/8/0) / 228.827 assertions |
| **Fase 4B** | `HardwareRoutingPanel` (Física) | Bajo | #402 | **PASS (Certificado)**: 615 casos (607/8/0) / 228.818 assertions |
| **Fase 5** | Limpieza final y sanidad CMake | Bajo | Pendiente | Pendiente |

---

## 5. Criterios de No Regresión Inviolables

1. **Cero impacto en Audio y DSP:** Ningún archivo bajo `src/dsp/` ni `src/audio/` modificado en ninguna fase.
2. **Cero alteración de contratos RFC 8785 y Hashes:** Serialización y hashes canónicos intactos.
3. **Cero FAIL en Suites Automatizadas:** 0 fallos en todas las fases certificadas.
4. **8 SKIPPED justificados y permanentes:** Tests `[ui_governance]` con singleton COM/WASAPI de JUCE. No representan regresión.
5. **Baseline de aserciones por build (trazabilidad completa):**
   - Build #395 (post-Fase 2): **228.920 assertions** / 617 casos
   - Build #398 (post-Fase 3): **228.813 assertions** / 615 casos (delta: -107 / -2 casos)
   - Build #401 (post-Fase 4A): **228.827 assertions** / 616 casos (delta: +14 / +1 caso)
   - Build #402 (post-Fase 4B): **228.818 assertions** / 615 casos (delta: -9 / -1 caso)
6. **Cero regresión en telemetría canónica:** SEAM-01 a SEAM-06 certificados intactos en todos los builds.
