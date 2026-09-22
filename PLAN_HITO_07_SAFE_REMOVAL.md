# Plan de Retirada Segura de Duplicados y Componentes Legacy — HITO-07

**Proyecto:** ABDAudioLab (Universal Black-Box Musical Hardware & Synth Profiler)  
**Documento:** PLAN_HITO_07_SAFE_REMOVAL.md  
**Hito:** HITO-07-SAFE-REMOVAL-DUPLICATES  
**Versión:** 2.0.0  
**Fecha de Creación:** 2026-09-22  
**Estado:** COMPLETADO Y CERTIFICADO — HITO-07 CERRADO

---

## 1. Misión y Objetivos

El **HITO-07** tuvo como propósito la retirada limpia, segura y progresiva de los cuatro componentes de interfaz gráfica duplicados heredados de fases tempranas de experimentación, consolidando la arquitectura del Stepper canónico de 5 pasos y eliminando código muerto sin generar regresiones.

### Componentes Retirados:
1. **`SoundIdGuidedWorkflowContainer`** — Contenedor bifurcado de 3 pasos. **RETIRADO (Fase 1).**
2. **`LoopbackCalibrationModal` (`loopbackModal`)** — Diálogo modal legacy de calibración. **RETIRADO (Fase 2).**
3. **`WorkflowStepperBar` (`stepperBar`)** — Instancia visual del stepper horizontal. **INSTANCIA RETIRADA (Fase 3)**; archivos `.h/.cpp` conservados por tipos normativos.
4. **`HardwareRoutingPanel` (`hardwareRoutingPanel`)** — Panel de routing legacy con fallbacks defensivos de target. **RETIRADO (Fase 4).**

---

## 2. Principios No Negociables y Criterios de No Regresión

1. **Inmutabilidad Metrológica y DSP:** Cero cambios en `src/dsp/` y `src/audio/` en ninguna fase. ✅
2. **Conservación de Persistencia y Hashes:** Formato `.abdlabtest`, `manifest.json` y SHA-256/RFC 8785 intactos. ✅
3. **Preservación de Seams Certificados:** `SEAM-01` a `SEAM-06` (HITO-06) intactos. ✅
4. **Cero FAIL en Suites Automatizadas:** 0 fallos en todas las fases. ✅
5. **Retirada Atómica por Fases:** Cada componente retirado en fase aislada con build + tests. ✅

---

## 3. Ejecución por Fases — Resultado Final

### Fase 1: Retirada de `SoundIdGuidedWorkflowContainer` — PASS (Build #394)
- 617 casos totales (609 PASS / 8 SKIP / 0 FAIL). Cero referencias residuales.

### Fase 2: Retirada de `LoopbackCalibrationModal` — PASS (Build #395)
- 617 casos (609 PASS / 8 SKIP / 0 FAIL / 228.920 assertions). Cero referencias funcionales.

### Fase 3: Desacoplamiento Instancia UI `stepperBar` — PASS (Build #398)
- Tipos normativos (`CanonicalStep`, `CanonicalStepStatus`) migrados a `CanonicalWorkflowTypes.h`.
- 615 casos (607 PASS / 8 SKIP / 0 FAIL / 228.813 assertions).

### Fase 4A: Sustitución de Fallbacks `hardwareRoutingPanel` — PASS (Build #401)
- `resolveCanonicalTarget()` implementado. 6 interacciones redirigidas a autoridad canónica.
- `ERR_NO_TARGET_SELECTED` en `ReportExportUiController`.
- 616 casos (608 PASS / 8 SKIP / 0 FAIL / 228.827 assertions).

### Fase 4B: Retirada Física `HardwareRoutingPanel` — PASS (Build #402)
- Archivos eliminados, include/miembro/callbacks retirados, CMake limpio.
- 615 casos (607 PASS / 8 SKIP / 0 FAIL / 228.818 assertions).

### Fase 5: Verificación Global y Certificación — COMPLETADA
- `git diff --check` → exit 0 (warnings de CRLF normalization únicamente).
- 0 referencias funcionales legacy en `src/` y `CMakeLists.txt`.
- Smoke visual: pendiente de confirmación por el usuario.
- Acta emitida: `ACTA_HITO_07_SAFE_REMOVAL.md`.

---

## 4. Autoridad Canónica Final Consolidada

| Función | Autoridad |
|---|---|
| Navegación de workflow | `WorkflowNavigationController` + `SoundIdSidebarStepper` |
| Estado de calibración | `CanonicalCalibrationState` |
| Tipos de pasos | `CanonicalWorkflowTypes.h` |
| Selección de hardware | `SoundIdHardwareCatalogSelector` (`catalogSelector`) |
| Resolución de target | `resolveCanonicalTarget()` + `ProfilingSessionController` |
| Exportación | `ReportExportService` → `ProductionPackage` |

---

## 5. Baseline de Aserciones por Build (Trazabilidad)

| Build | Post-Fase | Casos | PASS | SKIP | FAIL | Assertions |
|---|---|---|---|---|---|---|
| #394 | Fase 1 | 617 | 609 | 8 | 0 | — |
| #395 | Fase 2 | 617 | 609 | 8 | 0 | 228.920 |
| #398 | Fase 3 | 615 | 607 | 8 | 0 | 228.813 |
| #401 | Fase 4A | 616 | 608 | 8 | 0 | 228.827 |
| **#402** | **Fase 4B** | **615** | **607** | **8** | **0** | **228.818** |
