# Matriz de Dependencias y Checklist — HITO-08: Release v2.1.0

**Proyecto:** ABDAudioLab  
**Documento:** MATRIX_HITO_08_RELEASE.md  
**Versión:** 1.0.0  
**Fecha:** 2026-09-23  
**Estado:** CERTIFICADO Y SELLADO

---

## 1. Dependencias de Entrada

| Dependencia | Estado | Evidencia |
|-------------|--------|-----------|
| HITO-07 certificado y cerrado | ✅ | `ACTA_HITO_07_SAFE_REMOVAL.md`, Build #402 |
| Auditoría de coherencia del roadmap | ✅ | Commit `62578cc` |
| Git limpio y sincronizado | ✅ | `main` → GitHub |
| Suite baseline: 615/607/8/0/228.818 | ✅ | Build #402 |
| Smoke visual: 13/13 PASS | ✅ | Evidencia en acta HITO-07 |

---

## 2. Stack de Dependencias Externas

| Componente | Versión | Fuente | Pinned |
|-----------|---------|--------|--------|
| JUCE | 8.0.4 | `GIT_TAG 8.0.4` | ✅ Sí |
| nlohmann/json | v3.11.3 | `GIT_TAG v3.11.3` | ✅ Sí |
| Catch2 | v3.5.2 | `GIT_TAG v3.5.2` | ✅ Sí |
| ARA SDK | 2.2.0 | `GIT_TAG releases/2.2.0` | ✅ Sí |
| ABDSharedCode | `65875eb` | `GIT_TAG 65875eb8...` (master @ 2026-09-22) | ✅ Sí — SHA |
| ABDScope | `0a296f3` | `GIT_TAG 0a296f3f...` (main @ 2026-09-22) | ✅ Sí — SHA |
| MSVC | VS 18 (2026) | Instalación local | — |
| CMake | ≥ 3.22 | Instalación local | — |

> [!NOTE]
> `ABDSharedCode` y `ABDScope` han sido fijados a commits SHA exactos en `CMakeLists.txt` para garantizar reproducibilidad del release v2.1.0.

---

## 3. Checklist de Release por Fase

### Fase 1: Fijación de Versión

- [x] `BuildVersion.h`: `kAppVersion` → `"2.1.0"`
- [x] `BuildVersion.h`: limpiar líneas vacías residuales (11-33)
- [x] `CMakeLists.txt`: `project(VERSION)` → `2.1.0`
- [x] ABDSharedCode pinned a SHA `65875eb8fbf65602b05981318608264a4c2432ec`
- [x] ABDScope pinned a SHA `0a296f3f3d161f65b41be4d02413ab858dddff55`
- [x] Verificar versión en About dialog tras build: **v2.1.0 • Build 406** verificado en UI
- [x] Registrar commit completo (`62578cc95dbd744f450323b1db77add518de0099`) en manifest

### Fase 2: Build Release

- [x] Ejecutar `build.bat Release`
- [x] Compilación sin errores (0 errores)
- [x] Anotar build number: **406**

### Fase 3: Validación

- [x] `ABDAudioLab_Tests.exe`: 0 FAIL
- [x] Confirmar métricas: 615 casos, 607 PASS, 8 SKIP justificados, 0 FAIL, 228.818 assertions
- [x] Smoke visual en Build #406: versión, temas y navegación verificados
- [x] Exportación con target: PASS (ReferenceSynth — paquete generado)
- [x] Exportación sin target: guardia activa ("cannot export: no measured points")

### Fase 4: Empaquetado

- [x] SHA-256 de `ABDAudioLab.exe`: `9DD043222807BCB052D2449DFCD3E106547B85C4D8A33899120246E7216DEE70`
- [x] SHA-256 de `ABDAudioLab_Tests.exe`: `FD876A4A5DA3B5F02CAEEEEF969BB5608659203834E501CC1C615621F8F2B4D2`
- [x] SHA-256 de `ABDAudioLab_PluginWorker.exe`: `AEDA6035AFF8926A7527A45CA94DB724CE04DA36C7BC582B13D2E550050DEF8D`
- [x] `MANIFEST_RELEASE_v2.1.0.json` generado
- [x] Hashes y rutas relativas verificados contra binarios

### Fase 5: Documentación

- [x] `RELEASE_NOTES_v2.1.0.md` redactado
- [x] Known issues documentados (KI-01, KI-02; KI-03 resuelto)
- [x] Requisitos de instalación documentados
- [x] Procedimiento de smoke test incluido
- [x] Delimitaciones metodológicas documentadas (AU/CLAP/LV2 fuera, out-of-process fuera, paridad guiado/no-guiado pendiente)

### Fase 6: Sellado

- [x] `ACTA_RELEASE_CANDIDATE_v2.1.0.md` emitida y promovida a CERTIFICADA Y SELLADA
- [x] Smoke test visual final sobre Build #406 — SUPERADO
- [x] ACTA actualizada a estado definitivo (23-Sep-2026)
- [x] Tag `v2.1.0` creado
- [x] Tag pusheado a GitHub

### Fase 7: Cierre

- [x] `TASK.txt` actualizado
- [x] `docs/ROADMAP.md` actualizado
- [x] Versión del documento ROADMAP → 2.3.0

---

## 4. Known Issues para v2.1.0

| ID | Descripción | Severidad | Origen | Bloqueante |
|----|------------|-----------|--------|------------|
| KI-01 | Contraste insuficiente en ciertos paneles del modo oscuro | Baja | Preexistente (pre-HITO-07) | No |
| KI-02 | 8 test cases SKIPPED por conflictos COM/WASAPI singleton | Info | Limitación de entorno de test | No |

---

## 5. Archivos Modificados en HITO-08

| Archivo | Operación | Fase |
|---------|-----------|------|
| `src/BuildVersion.h` | MODIFY — versión 2.1.0 | 1 |
| `CMakeLists.txt` | MODIFY — project VERSION | 1 |
| `PLAN_HITO_08_RELEASE.md` | NEW — este plan | 1 |
| `MATRIX_HITO_08_RELEASE.md` | NEW — esta matriz | 1 |
| `MANIFEST_RELEASE_v2.1.0.json` | NEW — manifiesto de release | 4 |
| `RELEASE_NOTES_v2.1.0.md` | NEW — notas de release | 5 |
| `ACTA_RELEASE_v2.1.0.md` | NEW — acta de certificación | 6 |
| `TASK.txt` | MODIFY — estado HITO-08 | 7 |
| `docs/ROADMAP.md` | MODIFY — HITO-08 certificado | 7 |

---

## 6. Criterio de Rollback

| Escenario | Acción |
|-----------|--------|
| Defecto bloqueante post-tag | No modificar tag; corregir y publicar v2.1.1 |
| Build no reproducible | Investigar dependencias flotantes (ABDSharedCode/ABDScope) |
| Regresión en suite | Revertir al commit pre-versión; no etiquetar |
