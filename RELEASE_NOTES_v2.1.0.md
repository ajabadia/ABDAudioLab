# ABDAudioLab v2.1.0 — Release Notes

**Versión del Producto:** `v2.1.0`  
**Estado:** Release Candidate (Build #404)  
**Fecha:** Septiembre 2026  
**Arquitectura:** Windows x64 (MSVC 2026, C++20)  
**Commit Base:** `62578cc95dbd744f450323b1db77add518de0099`  

---

## 1. Resumen Ejecutivo

ABDAudioLab v2.1.0 consolida el ciclo de maduración técnica y operativa desarrollado a lo largo de los hitos **HITO-01 a HITO-07**. Esta versión no introduce roturas de compatibilidad pública (SemVer minor bump compatible), sino que establece una base certificada, reproducible y limpia con hosting VST3 nativo, pipeline de pruebas de audio determinista, empaquetado y exportación unificada de modelos, soporte multi-tema (claro/oscuro) y la retirada definitiva de código legacy e intermedios experimentales.

---

## 2. Hitos y Capacidades Consolidadas

| Hito | Área | Capacidades Principales |
|---|---|---|
| **HITO-01** | *Infraestructura & Contratos* | Definición de contratos canónicos de sesión, estados de perfilado y desacoplamiento del hilo de audio. |
| **HITO-02** | *Hosting VST3 & Telemetría* | Carga, auditoría y ejecución de plugins VST3 reales (validado con Dexed). Medición de latencia, fijación de presets deterministas y captura offline sin underruns. |
| **HITO-03** | *Validación Audio A/B* | Motor de análisis de correlación espectral, THD, SNR y verificación bit-exact para filtros y envolventes canónicas. |
| **HITO-04** | *Exportación Unificada* | Exportador unificado de paquetes de producción (`.zip`), modelos C++, tablas LUT y manifiesto formal con reporte de certificación. |
| **HITO-05** | *Arquitectura UI Canónica* | Orquestación fina con `ProfilingSessionController`, navegación gobernada y sincronización reactiva sin colisiones de estado. |
| **LIGHT-MODE-01** | *Diseño de Presentación* | Implementación de tema dual Claro/Oscuro con contraste optimizado y tokens visuales semánticos. |
| **HITO-06** | *Pruebas End-to-End Herméticas* | Suite de pruebas E2E herméticas sin dependencias externas en tiempo de ejecución. |
| **HITO-07** | *Safe Removal & Refactor Canónico* | Retirada segura de 4 componentes obsoletos (`SoundIdGuidedWorkflowContainer`, `LoopbackCalibrationModal`, instancia visual de `WorkflowStepperBar`, `HardwareRoutingPanel`) y unificación en la arquitectura canónica. |

---

## 3. Retirada Segura de Componentes Legacy (HITO-07)

Se certificó la eliminación de componentes experimentales redundantes en favor de la arquitectura canónica unificada:
- `SoundIdGuidedWorkflowContainer` ➔ Reemplazado por navegación canónica vía `WorkflowNavigationController`.
- `LoopbackCalibrationModal` ➔ Unificado en `CanonicalCalibrationState`.
- Instancia visual redundante de `WorkflowStepperBar` ➔ Unificada en `SoundIdSidebarStepper`.
- `HardwareRoutingPanel` ➔ Reemplazado por `SoundIdHardwareCatalogSelector` y `resolveCanonicalTarget()`.

---

## 4. Stack de Dependencias Técnicas Pinneadas

Para garantizar la reproducibilidad determinista de la build, todas las dependencias externas han sido fijadas a etiquetas inmutables o commits SHA exactos:

| Dependencia | Versión / Commit SHA | Método de Fijación |
|---|---|---|
| **ABDSharedCode** | `65875eb8fbf65602b05981318608264a4c2432ec` | Commit exacto en `CMakeLists.txt` (master @ 2026-09-22) |
| **ABDScope** | `0a296f3f3d161f65b41be4d02413ab858dddff55` | Commit exacto en `CMakeLists.txt` (main @ 2026-09-22) |
| **JUCE** | `8.0.4` | Git tag oficial |
| **nlohmann/json** | `v3.11.3` | Git tag oficial |
| **Catch2** | `v3.5.2` | Git tag oficial |
| **ARA SDK** | `releases/2.2.0` | Git tag oficial |
| **Toolchain** | MSVC v18.4 (Visual Studio 2026) / C++20 | Windows SDK 10.0.28000.0 |

---

## 5. Delimitación Metodológica del Alcance (Scope Boundaries)

1. **Formatos de Plugins Soportados:** Se certifica exclusivamente el formato **VST3**. Los formatos AU, CLAP y LV2 quedan explícitamente fuera del alcance de este release.
2. **Aislamiento de Procesos:** La versión opera con hosting in-process y worker auxiliar básico; el aislamiento *out-of-process* completo (sandboxing estricto con IPC de baja latencia) no forma parte de este hito.
3. **Paridad Modo Guiado vs. Modo Exploración:** El pipeline de procesamiento y los modelos son compartidos por la arquitectura subyacente. No obstante, **no se afirma equivalencia de resultados demostrada matemáticamente** entre ambos modos de interacción sin una prueba formal de paridad *Guided-vs-Exploration* (planificada para ciclos posteriores).

---

## 6. Known Issues (Incidencias Conocidas)

- **KI-01 (Cosmético / No bloqueante):** Contraste tipográfico insuficiente en ciertos paneles secundarios bajo Modo Oscuro. La legibilidad básica y la funcionalidad están preservadas, pero no satisface plenamente WCAG AA. Corregible en ciclo menor de diseño.
- **KI-02 (Entorno de Pruebas / No bloqueante):** 8 casos de prueba de *UI Governance* permanecen como `SKIPPED` en la ejecución monolítica de la suite debido a restricciones arquitectónicas de JUCE sobre reinicialización de singletons COM/WASAPI en un único proceso; requieren aislamiento de proceso para su validación.
- **KI-03 (Resuelto):** Dependencias externas flotantes eliminadas; `ABDSharedCode` y `ABDScope` quedan fijados por SHA exacto en `CMakeLists.txt`.

---

## 7. Requisitos de Sistema e Instalación

### Requisitos Mínimos
- **Sistema Operativo:** Windows 10 (64-bit) o Windows 11 (64-bit).
- **CPU:** Procesador x64 compatible con AVX2 (Intel Core 6ª gen o AMD Ryzen en adelante).
- **RAM:** 4 GB mínimo (8 GB recomendado para sesiones extensas de perfilado).
- **Audio:** Controlador compatible con WASAPI / ASIO para baja latencia.

### Instalación
1. Extraer los artefactos compilados en el directorio deseado (ej. `C:\Program Files\ABDAudioLab\`).
2. Asegurar que `ABDAudioLab_PluginWorker.exe` permanezca en la misma carpeta junto a `ABDAudioLab.exe`.
3. Ejecutar `ABDAudioLab.exe`.

---

## 8. Procedimiento de Verificación Rápida (Smoke Test Visual)

Para certificar la correcta inicialización en un entorno limpio:
1. Lanzar `build/ABDAudioLab_artefacts/Release/ABDAudioLab.exe`.
2. Verificar en el diálogo de bienvenida / cabecera que figure la versión **2.1.0 (Build 404)**.
3. Comprobar la alternancia de temas Claro/Oscuro mediante el selector de tema.
4. Navegar por los 5 pasos del sidebar stepper (`Target`, `Calibrate`, `Profile`, `Review`, `Export`).
5. Intentar exportar sin target seleccionado y verificar que se emite la guarda canónica `ERR_NO_TARGET_SELECTED`.
6. Seleccionar un target VST3 disponible y comprobar la actualización del estado de auditoría y habilitación del botón de exportación.
