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

### Definition of Done (DoD)
1. **Script de Verificación:** `tools/verify-release-hashes.ps1` compara los binarios locales contra los hashes oficiales certificados y devuelve exit code 0/1.
2. **Guía de Fixtures Externas:** Documento que explica exactamente por qué 27 tests se marcan SKIPPED y cómo instalar opcionalmente Dexed/VES sin alterar la suite base.
3. **Suite Intacta:** Los 949 test cases mantienen el 100% de aserciones en verde (211.374/211.374 PASS).
4. **Higiene Intacta:** 152/152 aserciones de `[hygiene]` en verde.
5. **Aislamiento Hardware:** Cero llamadas MIDI, cero bytes transmitidos.
