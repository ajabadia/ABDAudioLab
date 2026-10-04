# Acta de Release — ABDAudioLab v2.1.0

> **Estado:** `CERTIFICADA Y SELLADA`  
> **Fecha de Sellado:** 23 de septiembre de 2026  
> **Autor / Rol:** Antigravity (Lead Architect & Supervisor) en tándem con Usuario  
> **Hito Operativo:** HITO-08 — Documentación operativa de release y sellado de versión  

---

## 1. Identificación y Parámetros del Build

| Parámetro | Valor Certificado |
|---|---|
| **Nombre del Producto** | ABDAudioLab |
| **Versión SemVer** | `2.1.0` |
| **Número de Build** | `406` |
| **Configuración** | `Release` (Optimizada) |
| **Arquitectura** | `x64` |
| **Compilador / Toolchain** | MSVC v18.4 (Visual Studio 2026 Developer Prompt) / C++20 |
| **Commit Base de Configuración** | `62578cc95dbd744f450323b1db77add518de0099` |
| **Tag Final** | `v2.1.0` *(Creado y publicado tras smoke visual satisfactorio)* |

---

## 2. Inventario de Artefactos y Hashes Criptográficos (SHA-256)

Los siguientes binarios fueron producidos por la compilación oficial de Release y sus sumas criptográficas han sido verificadas contra el disco:

| Binario / Artefacto | Ruta Relativa Oficial | Tamaño (Bytes) | Hash SHA-256 |
|---|---|---|---|
| **ABDAudioLab.exe** | `build/ABDAudioLab_artefacts/Release/ABDAudioLab.exe` | 8.986.112 B | `9DD043222807BCB052D2449DFCD3E106547B85C4D8A33899120246E7216DEE70` |
| **ABDAudioLab_Tests.exe** | `build/Release/ABDAudioLab_Tests.exe` | 14.567.424 B | `FD876A4A5DA3B5F02CAEEEEF969BB5608659203834E501CC1C615621F8F2B4D2` |
| **ABDAudioLab_PluginWorker.exe** | `build/ABDAudioLab_artefacts/Release/ABDAudioLab_PluginWorker.exe` | 3.827.712 B | `AEDA6035AFF8926A7527A45CA94DB724CE04DA36C7BC582B13D2E550050DEF8D` |

> **Nota de integridad de rutas:** El binario ejecutado para validación de la aplicación principal y el binario hasheado corresponden unívocamente a `build/ABDAudioLab_artefacts/Release/ABDAudioLab.exe`.

---

## 3. Certificación de Dependencias Pinneadas

Para asegurar reproducibilidad absoluta e inmunidad ante cambios en ramas externas remotas:

- **ABDSharedCode**: Pinned a SHA `65875eb8fbf65602b05981318608264a4c2432ec` (origin: `master` @ 2026-09-22).
- **ABDScope**: Pinned a SHA `0a296f3f3d161f65b41be4d02413ab858dddff55` (origin: `main` @ 2026-09-22).
- **JUCE**: Tag inmutable `8.0.4`.
- **nlohmann/json**: Tag inmutable `v3.11.3`.
- **Catch2**: Tag inmutable `v3.5.2`.

---

## 4. Resultados de la Verificación Automatizada (Suite Completa)

Ejecución limpia de `ABDAudioLab_Tests.exe` en la Release Candidate:

```text
===============================================================================
test cases:    615 |    607 passed | 8 skipped
assertions: 228818 | 228818 passed
===============================================================================
Exit code: 0
```

- **Total Test Cases:** 615
- **Passed:** 607 (100% de los casos ejecutables)
- **Skipped justificados:** 8 (Pruebas de UI Governance que requieren proceso independiente debido a restricciones de reinicialización del subsistema gráfico/audio WASAPI de JUCE)
- **Failed:** 0
- **Total Assertions:** 228.818 evaluadas y aprobadas (0 fallos)

---

## 5. Delimitación Metodológica y Restricciones de Alcance

Para preservar la honestidad técnica y evitar afirmaciones no certificadas empíricamente:
1. **Exclusión de Formatos Secundarios:** Únicamente se certifica el hosting de plugins **VST3**. AU, CLAP y LV2 no forman parte del release.
2. **Aislamiento out-of-process:** El sandboxing IPC completo con proceso separado permanece fuera de este release.
3. **No Afirmación de Equivalencia Guiado vs. No-Guiado:** El Modo Guiado y el Modo Exploración comparten la misma arquitectura canónica (`ProfilingSessionController`, `WorkflowNavigationController`) y el mismo motor de análisis DSP. Sin embargo, **no se afirma equivalencia funcional ni de resultados idénticos** sin una prueba formal dedicada de paridad *Guided-vs-Exploration* (comparando receta, eventos, audio, métricas y veredicto), la cual queda como objetivo de validación posterior.

---

## 6. Registro de Known Issues (Incidencias Conocidas)

| Identificador | Severidad | Estado | Descripción |
|---|---|---|---|
| **KI-01** | Baja / Cosmético | Documentado (No bloqueante) | Contraste insuficiente en determinados labels y sliders en Modo Oscuro. No interfiere con la captura, auditoría o exportación de modelos. |
| **KI-02** | Baja / Entorno Test | Documentado (No bloqueante) | 8 tests de UI Governance permanecen como SKIPPED en la suite global por restricciones arquitectónicas de JUCE en modo monoproceso; requieren aislamiento de proceso para su validación. |
| **KI-03** | Resuelto | Eliminado | Dependencias `ABDSharedCode` y `ABDScope` pinneadas a commits exactos en CMake. |

---

## 7. Plan de Cierre y Tareas Pendientes para Sellado Definitivo

La transición de `RELEASE CANDIDATE` a `RELEASE CERTIFICADO Y SELLADO` requiere completar la siguiente secuencia ordenada:

1. [x] Generar `MANIFEST_RELEASE_v2.1.0.json`.
2. [x] Redactar `RELEASE_NOTES_v2.1.0.md`.
3. [x] Crear `ACTA_RELEASE_CANDIDATE_v2.1.0.md`.
4. [x] **Smoke Test Visual de Build #406 — SUPERADO (23-Sep-2026):**
   - [x] Iniciado `build/ABDAudioLab_artefacts/Release/ABDAudioLab.exe`.
   - [x] Versión `v2.1.0 · Build 406 (Sep 22 2026 21:37:29)` confirmada en diálogo About.
   - [x] Alternancia Modo Claro / Modo Oscuro: funciona correctamente.
   - [x] Guardia `ERR_NO_TARGET_SELECTED` emitida al exportar sin target activo.
   - [x] Exportación con `ReferenceSynth`: paquete generado (C++ Header, Telemetry JSON, HTML Report, Manifest).
   - [x] Navegación por pasos del sidebar: fluida y consistente.
5. [x] **Consolidación en Git:**
   - Documentos HITO-08 registrados en commit final de sellado en `main`.
6. [x] **Creación del Tag Inmutable:**
   - Tag git inmutable creado: `v2.1.0`
   - Tag publicado en repositorio remoto.
7. [x] **Actualización del Acta:**
   - Encabezado promovido a **CERTIFICADA Y SELLADA** con fecha 23-Sep-2026.

---

## 8. Procedimiento de Rollback y Gestión Post-Release

1. **Inmutabilidad del Tag Publicado:** Una vez publicado, el tag `v2.1.0` no debe modificarse ni eliminarse del repositorio público, preservando la trazabilidad exacta del artefacto liberado.
2. **Defectos Críticos Post-Release:** En caso de detectarse anomalías críticas en producción, la corrección se aplicará en un commit posterior sobre `main` y se publicará como versión parche `v2.1.1`.
3. **Rollback Local Pre-Tag:** Si la anomalía se detectase durante el smoke visual previo a la creación del tag, se descartan los cambios locales y se reanuda desde el commit certificado `62578cc95dbd744f450323b1db77add518de0099`.
