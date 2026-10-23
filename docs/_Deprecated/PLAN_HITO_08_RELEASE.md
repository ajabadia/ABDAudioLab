# Plan de Release Operativo — HITO-08: Sellado de ABDAudioLab v2.1.0

**Proyecto:** ABDAudioLab (Universal Black-Box Musical Hardware & Synth Profiler)  
**Documento:** PLAN_HITO_08_RELEASE.md  
**Hito:** HITO-08-RELEASE-v2.1.0  
**Versión:** 1.0.0  
**Fecha de Creación:** 2026-09-22  
**Estado:** EN EJECUCIÓN  
**Commit base:** `62578cc` (post auditoría de coherencia del roadmap)

---

## 1. Misión

Convertir el estado certificado post-HITO-07 en un **release reproducible, instalable y trazable** bajo la versión **v2.1.0** del producto. Este hito no introduce nuevas funcionalidades; su propósito es hardening, documentación operativa y publicación.

---

## 2. Alcance Incluido

1. Fijar versión de producto `v2.1.0` en `BuildVersion.h` y `CMakeLists.txt`.
2. Preparar build Release x64 limpia.
3. Generar inventario de artefactos (binarios, fixtures, assets).
4. Calcular SHA-256 de binarios y artefactos clave.
5. Redactar release notes (`RELEASE_NOTES_v2.1.0.md`).
6. Documentar instalación y requisitos del sistema.
7. Registrar known issues.
8. Ejecutar suite completa y smoke test.
9. Verificar exportaciones (C++/HTML/JSON).
10. Definir procedimiento de rollback.
11. Crear tag inmutable `v2.1.0`.
12. Archivar evidencia del release (`ACTA_RELEASE_v2.1.0.md`).

---

## 3. Alcance Excluido

- Corrección de contraste en modo oscuro (known issue, no bloqueante).
- Expansión AU/CLAP/LV2 (experimental, fuera de v2.1.0).
- Aislamiento out-of-process completo de plugins.
- Nuevas capacidades de medición o DSP.
- Hardware físico adicional.
- Comparaciones subjetivas.

---

## 4. Baseline de Entrada

| Métrica | Valor |
|---------|-------|
| Build de referencia | Release #402 |
| Commit base | `62578cc` |
| Test cases | 615 |
| PASS | 607 |
| SKIPPED justificados | 8 (COM/WASAPI singletons) |
| FAIL | 0 |
| Assertions | 228.818 |
| Smoke visual | 13/13 PASS |
| HITO-07 | CERTIFICADO Y CERRADO |

---

## 5. Entorno de Compilación

| Componente | Versión / Referencia |
|-----------|---------------------|
| Compilador | MSVC x64, Visual Studio 18 (2026) |
| Estándar | C++20 |
| CMake | ≥ 3.22 |
| JUCE | 8.0.4 (`GIT_TAG 8.0.4`) |
| nlohmann/json | v3.11.3 |
| Catch2 | v3.5.2 |
| ARA SDK | releases/2.2.0 |
| ABDSharedCode | `master` (GitHub) |
| ABDScope | `main` (GitHub) |
| Configuración | Release |
| Arquitectura | x64 |

---

## 6. Fases de Ejecución

### Fase 1: Fijación de Versión y Manifiesto

**Estado: EN CURSO**

**Completado:**
- [x] `kAppVersion`: `"1.1.0"` → `"2.1.0"`
- [x] `project(ABDAudioLab VERSION 2.1.0 ...)`
- [x] ABDSharedCode pinned a SHA `65875eb8fbf65602b05981318608264a4c2432ec` (master @ 2026-09-22)
- [x] ABDScope pinned a SHA `0a296f3f3d161f65b41be4d02413ab858dddff55` (main @ 2026-09-22)
- [x] Líneas vacías residuales en `BuildVersion.h` limpiadas

**Pendiente:**
- [ ] Verificar versión en About dialog / splash tras build
- [ ] Verificar `PROJECT_VERSION` en metadata del binario
- [ ] Registrar commit completo (SHA-1 de 40 caracteres) en manifest
- [ ] Registrar SHAs de todas las dependencias en manifest

> **Nota sobre SemVer:** La versión v2.1.0 consolida las capacidades certificadas de hosting VST3, E2E, exportación unificada, telemetría y retirada de componentes legacy. No se presenta como un salto major derivado de una ruptura de compatibilidad; es un release minor con nuevas capacidades compatibles.

#### [MODIFY] [CMakeLists.txt](CMakeLists.txt)
- `project(ABDAudioLab VERSION 1.1.0 ...)` → `project(ABDAudioLab VERSION 2.1.0 ...)`

#### [NEW] MANIFEST_RELEASE_v2.1.0.json
- Versión del producto
- Commit exacto (SHA completo)
- Fecha/hora UTC de build
- Toolchain y configuración
- Arquitectura
- Versión de JUCE y dependencias
- Hash SHA-256 de `ABDAudioLab.exe`
- Hash SHA-256 de `ABDAudioLab_Tests.exe`
- Recuento de tests y assertions
- Known issues

---

### Fase 2: Build Release Limpia

1. Configurar un árbol de build nuevo para demostrar reproducibilidad:
   ```powershell
   cmake -B build-release-v2.1.0 -DCMAKE_BUILD_TYPE=Release
   cmake --build build-release-v2.1.0 --config Release
   ```
   Alternativamente, usar `build.bat Release` (que auto-incrementa el build number).
2. Verificar compilación sin errores ni warnings críticos.
3. Anotar build number asignado.
4. Verificar que el About dialog muestra `v2.1.0`.

> **Ejecuta el usuario.** El agente no lanza compilaciones.

---

### Fase 3: Suite Completa y Smoke Test

1. Ejecutar `.\build\Release\ABDAudioLab_Tests.exe` — esperar 0 FAIL.
2. Ejecutar `.\build\Release\ABDAudioLab.exe` — smoke visual (13 puntos).
3. Verificar exportación con target → paquete generado.
4. Verificar exportación sin target → `ERR_NO_TARGET_SELECTED`.
5. Anotar métricas exactas de la build de release.

> **Ejecuta el usuario.**

---

### Fase 4: Empaquetado y Hashes

1. Calcular SHA-256 de `ABDAudioLab.exe`.
2. Calcular SHA-256 de `ABDAudioLab_Tests.exe`.
3. Calcular SHA-256 de plugins fixture VST3 utilizados (ruta relativa, no absoluta).
4. Registrar hash del commit de cada dependencia.
5. Generar `MANIFEST_RELEASE_v2.1.0.json` con todos los hashes, metadatos y estado de recursos.
6. Verificar integridad: hashes del manifest coinciden con los binarios en disco.
7. No incluir rutas absolutas de máquina en el manifest — usar rutas relativas al proyecto.

---

### Fase 5: Release Notes y Known Issues

#### [NEW] RELEASE_NOTES_v2.1.0.md

Contenido:
- Resumen de la versión
- Cambios funcionales principales (HITO-01 a HITO-07)
- Mejoras de arquitectura
- Componentes retirados
- Dependencias y versiones
- Known issues (incluyendo contraste en modo oscuro)
- Requisitos de sistema e instalación
- Procedimiento de verificación rápida (smoke test)

---

### Fase 6: Acta de Release y Tag

#### [NEW] ACTA_RELEASE_v2.1.0.md

- Dictamen de certificación del release
- Métricas de la build de release
- Hashes de binarios
- Evidencia del smoke test
- Referencia a release notes
- Procedimiento de rollback
- Tag asociado

#### Tag inmutable

```bash
git tag -a v2.1.0 -m "Release ABDAudioLab v2.1.0"
git push origin v2.1.0
```

---

### Fase 7: Actualización Documental

#### [MODIFY] TASK.txt
- HITO-08: CERTIFICADO Y CERRADO

#### [MODIFY] docs/ROADMAP.md
- HITO-08: Certificado
- Versión del documento: incrementar a 2.3.0

---

## 7. Criterios de Salida

| Criterio | Verificación |
|---------|-------------|
| Versión `v2.1.0` fijada en `BuildVersion.h` y `CMakeLists.txt` | Inspección |
| Build Release compila sin errores | Salida de `build.bat` |
| Suite completa: 0 FAIL | Salida de tests |
| 8 SKIPPED justificados (COM/WASAPI) | Inspección |
| Smoke visual: 13/13 PASS | Evidencia visual/verbal |
| SHA-256 de binarios en manifest | Verificación cruzada |
| Release notes completas | Revisión |
| Known issues documentados | Revisión |
| Instalación documentada | Revisión |
| Rollback descrito | Revisión |
| Tag `v2.1.0` apunta al commit correcto | `git show v2.1.0` |

---

## 8. Rollback

Si se detecta un defecto bloqueante tras el tag:

1. **No se modifica el tag** — es inmutable.
2. Se corrige en un commit posterior.
3. Se publica `v2.1.1` (patch) con el fix.
4. Se documenta en las release notes de la nueva versión.

---

## 9. Artefactos Producidos

| Artefacto | Propósito |
|-----------|-----------|
| `PLAN_HITO_08_RELEASE.md` | Este documento |
| `MATRIX_HITO_08_RELEASE.md` | Matriz de dependencias y checklist |
| `RELEASE_NOTES_v2.1.0.md` | Notas de release para usuarios |
| `ACTA_RELEASE_v2.1.0.md` | Certificación formal del release |
| `MANIFEST_RELEASE_v2.1.0.json` | Manifiesto legible por máquina |
| Tag `v2.1.0` | Referencia inmutable en Git |
