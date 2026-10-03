# R3 — POST-RELEASE STABILIZATION & BACKLOG TRIAGE
## ABDAudioLab v2.1.0 ➔ v2.1.1 Baseline & Planning

**Fecha:** 4 de Octubre de 2026  
**Fase:** R3 — Post-Release Stabilization & Backlog Triage  
**Documento:** `docs/audits/POST_V2_1_0_BACKLOG_TRIAGE.md`  
**Estado:** 🟢 **ACTIVO & EN GOBERNANZA**  

---

## 1. Congelación de Baseline de Release v2.1.0

La versión de producción **ABDAudioLab v2.1.0** se encuentra formalmente sellada, etiquetada y publicada remotamente bajo las siguientes coordenadas inmutables:

| Parámetro | Valor Certificado |
|---|---|
| **Commit Certificado de Release** | `0b76616a9a52a0f755e260ed0cf421db2de8f33b` (`refs/tags/v2.1.0^{}`) |
| **Tag Anotado Inmutable** | `v2.1.0` (Objeto: `8df6db9a5c03f7ba27d8f9160341581c52dc1ed2`) |
| **Commit de Hardening Previo** | `f013037` |
| **Commits Documentales en `main`** | `93a8159` (Addendum de gobernanza) ➔ `a23962c` (Linaje) ➔ `564cb97` (Armonización final) |
| **SHA-256 `ABDAudioLab.exe`** | `C94A0BDB3A67CCE77D8459E5E103E9BF1CDA3321CF5C745FEA11379E7CF6C89E` (9.326.080 bytes) |
| **SHA-256 `ABDAudioLab_PluginWorker.exe`** | `DDD2976C98442DBBCA2209AB67DD9A0412BC939783B15FFBD056425FF3B1913E` (3.805.696 bytes) |
| **Suite Global Catch2** | **949 test cases: 922 PASS, 27 SKIPPED legítimos, 0 FAIL** (211.374/211.374 assertions PASS) |
| **Suite de Higiene (`[hygiene]`)** | **22 test cases: 152/152 assertions PASS, 0 FAIL** (0 rutas absolutas) |
| **Smoke Test Operativo** | **4 puntos certificados:** arranque limpio, transición de splash, ausencia de apertura MIDI, 0 procesos huérfanos |

---

## 2. Registro de Known Issues Post-Release

| ID | Área | Descripción | Impacto | Acción Recomendada |
|---|---|---|---|---|
| **KI-01** | *Entorno Externo* | 27 test cases en estado `SKIPPED` por ausencia de plugins VST3 externos (Dexed / VES) en la máquina de build. | **Bajo** (No es fallo de código; comportamiento esperado ante fixtures ausentes). | Documentar requisitos de entorno y hashes de las fixtures en v2.1.1. |
| **KI-02** | *UI / Accesibilidad* | Contraste de texto secundario en subpaneles específicos bajo Modo Oscuro no alcanza plenamente WCAG AA. | **Bajo** (Cosmético; legibilidad general preservada). | Triage para ciclo posterior de diseño (v2.2.0). |
| **KI-03** | *Gobernanza Git* | Tag `v2.1.0` requirió actualización forzada en remoto; documentado en addendum administrativo. | **Cerrado** (Trazabilidad registrada en `main`). | Mantener tag `v2.1.0` 100% inmovilizado; nuevas releases usarán nuevo SemVer (v2.1.1). |

---

## 3. Matriz de Clasificación del Backlog Post-v2.1.0

Evaluación sistemática de ítems pendientes antes de iniciar cualquier desarrollo:

| Ítem de Backlog | Clasificación | Severidad | Evidencia Técnica | ¿Elegible para v2.1.1? | Justificación |
|---|---|---|---|:---:|---|
| **Dexed / VES no instalados** | Limitación de entorno | Baja | 27 SKIPPED legítimos en suite Catch2 | 🟡 **Documentación solamente** | No convertir en fallo; documentar preflight en scripts de setup. |
| **Script de verificación SHA-256** | Deuda de reproducibilidad | Media | Verificación manual de hashes en release | ✅ **SÍ (Core de v2.1.1)** | Script automatizado para verificar integridad de binarios de release. |
| **Claridad de telemetría de tests** | Deuda técnica menor | Baja | Reporte de Catch2 en consola | ✅ **SÍ (Core de v2.1.1)** | Clarificar reporte distinguiendo explícitamente SKIPPED legítimos de PASS. |
| **D2.7B.2 Adquisición Física** | Operación hardware | Alta | Plan de seguridad D2.7B | ⛔ **NO** | Estrictamente bloqueada. Requiere banco de pruebas aislado fuera de release. |
| **DeepMind MCP Handshake** | Integración externa | Alta | D2.7B.1 / mcp_config.json | ⛔ **NO** | Congelada. MCP server permanece `disabled: true`. |
| **Nuevas features de GUI / DSP** | Evolución funcional | Media/Baja | Backlog de roadmap | ⛔ **NO** | Scope creep prohibido; v2.1.1 es microhito de estabilización. |

---

## 4. Definición de Alcance para v2.1.1

### Denominación: `v2.1.1 — Release Integrity & External Fixture Readiness`

**Objetivo:** Consolidar la reproducibilidad y verificación automatizada de la release, y documentar formalmente las condiciones de entorno para fixtures externas, con **cero impacto en el motor de audio y cero riesgo de regresión**.

### Límites de Alcance (In / Out)

```
[v2.1.1 DENTRO DE ALCANCE]
├── 1. Script automatizado tools/verify-release-hashes.ps1 para validar SHA-256 de binarios.
├── 2. Documentación formal de fixtures externas (Dexed/VES) en docs/qa/external-fixtures.md.
└── 3. Claridad en mensajes de resumen de suite para evitar ambigüedades PASS vs. SKIPPED.

[v2.1.1 FUERA DE ALCANCE (ESTRICTAMENTE EXCLUIDO)]
├── ⛔ Cero modificaciones al hilo de audio en tiempo real o DSP.
├── ⛔ Cero reapertura de puertos MIDI físicos o sintetizadores hardware.
├── ⛔ DeepMind 12D permanece en reposo pasivo por USB (0 bytes TX/RX).
├── ⛔ Servidor MCP deepmind12 permanece disabled: true.
└── ⛔ Cero alteraciones o movimientos del tag inmutable v2.1.0.
```

---

## 5. Criterios de Entrada y Definition of Done (DoD) para v2.1.1

### Criterios de Entrada (Entry Criteria)
- [x] v2.1.0 sellada e inmovilizada en `0b76616`.
- [x] Suite Catch2: 922 PASS, 27 SKIPPED, 0 FAIL.
- [x] Higiene: 0 rutas absolutas.
- [x] DeepMind 12D en reposo pasivo garantizado.

### Definition of Done (DoD) para v2.1.1
1. **Manifiesto de Integridad:** `docs/release/release-integrity-v2.1.0.json` versionado y referenciando el commit certificado `0b76616a9a52a0f755e260ed0cf421db2de8f33b`.
2. **Script de Verificación:** `tools/verify-release-hashes.ps1` compara los binarios locales contra el manifiesto versionado, emitiendo exit code:
   - `0`: Todos los artefactos coinciden en tamaño y SHA-256.
   - `1`: Falta algún artefacto o difiere su tamaño o hash.
   - `2`: Parámetros de invocación o manifiesto inexistente/inválido.
   Probado empíricamente en los tres escenarios (0, 1 y 2).
3. **Guía de Fixtures Externas:** `docs/qa/external-fixtures.md` clasifica rigurosamente: fixture obligatoria (interna), fixture opcional (Dexed/VES), fixture ausente (SKIPPED legítimo) y fixture con drift. No inventa hashes para plugins no instalados.
4. **Suite Global de Referencia Preservada:**
   - 949 test cases totales.
   - 922 PASS.
   - 27 SKIPPED legítimos y esperados.
   - 0 FAIL.
   - **211.366 assertions PASS** (baseline justificado tras la reconciliación técnica de las 8 aserciones de advertencias espurias filtradas en `f013037`).
   - *Criterio de Rechazo:* Ningún test adicional en FAIL, ningún incremento injustificado de SKIPPED, ningún descenso no justificado de assertions PASS.
5. **Higiene Intacta:** 152/152 aserciones de `[hygiene]` en verde (0 rutas absolutas).
6. **Aislamiento Hardware & DeepMind:** Cero llamadas MIDI, cero bytes transmitidos, servidor MCP `disabled: true`.

---

## 5. Auditoría de Reconciliación de Cobertura (Baseline Assertions)

### Discrepancia Auditada
- **Baseline v2.1.0 inicial:** 211.374 assertions PASS (949 test cases: 922 PASS, 27 SKIPPED, 0 FAIL).
- **Ejecución v2.1.1 post-71028d7:** 211.366 assertions PASS (949 test cases: 922 PASS, 27 SKIPPED, 0 FAIL).
- **Diferencia:** −8 assertions PASS.

### Respuestas a la Investigación Técnica Requerida
1. **¿Qué commit introdujo el descenso?**
   - El commit `f013037478e02d59ed412cffe007fc5eb07966ca` (`fix(gui): notification bell overlay, hardware schema filtering and headless window safety`). Todos los commits posteriores (`fb11ca3` a `71028d7`) son exclusivamente documentales, de CI, manifiestos JSON y tooling PowerShell sin impacto en el código C++ ni en los tests.
2. **¿Qué archivos de test cambiaron entre el baseline de 211.374 y la ejecución actual?**
   - Únicamente `src/tests/test_MeasurementFloatingWindow.cpp`. Ningún otro archivo en `src/tests/` fue modificado.
3. **¿Se eliminó algún REQUIRE, CHECK, REQUIRE_FALSE, STATIC_REQUIRE o assertion macro?**
   - **NO**. No se eliminó ninguna línea de aserción ni macro de test. La modificación en `test_MeasurementFloatingWindow.cpp` únicamente añadió el parámetro de seguridad headless `/*addToDesktop=*/ false` en 8 instanciaciones de ventana para evitar dependencias del gestor de ventanas en entornos desatendidos.
4. **¿Se cambió algún filtro, tag, configuración de build o condición de compilación?**
   - **NO**. La suite global se ejecuta con el filtro idéntico `"~[ves]"`, con las mismas opciones de compilación Release de MSVC.
5. **¿A qué subsistema pertenecían las ocho assertions?**
   - A las aserciones dinámicas en bucles sobre advertencias y perfiles inválidos de `HardwareContractRegistry`. En `f013037` se introdujo el filtrado preventivo de 8 documentos en `contracts/hardware/` que no son perfiles de hardware (6 archivos `*.schema.json` y 2 documentos auxiliares de subsistemas con esquemas ajenos). Previamente, `loadContractsFromDirectory()` intentaba procesarlos como sintes de hardware, fallaba la validación estructural y emitía 8 avisos erróneos en `getWarnings()` / `invalidLegacyProfiles`, sobre los cuales iteraban aserciones dinámicas en bucle.
6. **¿La variación fue intencional, segura y documentada, o representa una pérdida involuntaria de cobertura?**
   - Es una variación segura, consecuencia directa de la corrección del fallo que generaba advertencias espurias en el arranque. No se ha suprimido ninguna verificación de comportamiento de producción; al contrario, se eliminó ruido sintético que ensuciaba el canal de advertencias del sistema.

### Veredicto y Cierre Técnico
- **Resolución adoptada:** Opción B (Actualización justificada y formal del baseline a **211.366 assertions PASS**).
- **Estado de Criterios DoD:**
  - *Herramienta de verificación de release (`verify-release-hashes.ps1`):* Exit codes 0, 1 y 2 demostrados y conformes.
  - *Manifiesto de release (`release-integrity-v2.1.0.json`):* Alineado con los artefactos sellados de v2.1.0.
  - *Guía de fixtures (`external-fixtures.md`):* Taxonomía precisa y sin inventar hashes externos.
  - *Higiene del código:* 22 test cases, 152/152 assertions PASS, 0 rutas absolutas.
  - *Suite global Catch2:* 949 test cases (922 PASS, 27 SKIPPED, 0 FAIL), **211.366 assertions PASS**.
  - *Aislamiento hardware:* DeepMind pasivo en USB, MCP desactivado, 0 bytes MIDI.


