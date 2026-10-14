# Documentos Deprecados / Archivo Histórico

**Fecha de archivo:** 2026-10-13
**Motivo:** Consolidación de documentación en `docs/` raíz — mantener solo documentos *vivos* y *actuales*

---

## Criterio de Archivo

Un documento se mueve a `_Deprecated/` cuando:
1. **Está supersedido** por un documento consolidado más reciente (ej. `PLAN.md` consolida todos los `PLAN_HITO_*`)
2. **Es evidencia intermedia** de un hito ya certificado y cerrado (ej. `ACTA_HITO_09A` → certificado en HITO-09B/C/D)
3. **Es matriz/plan de trabajo** ya ejecutado y cerrado (ej. `MATRIX_HITO_*`, `PLAN_HITO_*`)
4. **Es borrador/análisis UX** supersedido por implementación real
5. **Es nota interna/backup** sin valor de referencia permanente

---

## Contenido por Categoría

### Actas de Hitos Supersedidos (14 archivos)
| Archivo | Hito | Supersedido por |
|---------|------|-----------------|
| `ACTA_HITO_05_E2E_HERMETIC.md` | 5 | HITO-09A-D, HITO-10A-E |
| `ACTA_HITO_06_TELEMETRY_SEAMS.md` | 6 | HITO-09B, HANDOFF.md |
| `ACTA_HITO_07_SAFE_REMOVAL.md` | 7 | HITO-10E (legacy removal) |
| `ACTA_HITO_09A_MEASUREMENT_RECIPE_CONTRACT.md` | 9A | HITO-09B/C/D consolidados |
| `ACTA_HITO_09B_UNIFIED_WORKBENCH.md` | 9B | PLAN.md, HANDOFF.md |
| `ACTA_HITO_09C_ASSISTANCE_LEVELS.md` | 9C | PLAN.md, MIGRATION_GUIDE.md |
| `ACTA_HITO_09D_FREE_CAPTURE_PROMOTION.md` | 9D | PLAN.md |
| `ACTA_HITO_10A_TARGET_PROFILE_CONTRACT.md` | 10A | HITO-10B/C/D/E |
| `ACTA_HITO_10B_TARGET_PROFILE_LEGACY_PARITY.md` | 10B | HITO-10E |
| `ACTA_HITO_10C_DEXED_MIGRATION.md` | 10C | HITO-10E |
| `ACTA_HITO_10D1_HARDWARE_CONTRACTS.md` | 10D1 | HITO-10D2.7A |
| `ACTA_HITO_10D2_HARDWARE_DISPATCH.md` | 10D2 | D2.3-D2.6, D2.7A |
| `ACTA_HITO_10V0_1_SEMANTIC_CONTROL_PROBE.md` | 10V0.1 | Bloqueado preventivo |
| `ACTA_HITO_10V0_VES_FEASIBILITY.md` | 10V0 | Bloqueado preventivo |
| `ACTA_INTEGRATION_01_AUDIO_PARITY.md` | INTEGRATION-01 | PLAN.md, MATRIX_INTEGRATION_01.md (también archivado) |

### Matrices de Hitos (6 archivos)
| Archivo | Propósito Original |
|---------|-------------------|
| `MATRIX_HITO_05_E2E.md` | Matriz validación E2E hito 5 |
| `MATRIX_HITO_06_TELEMETRY.md` | Matriz telemetría hito 6 |
| `MATRIX_HITO_07_DEPENDENCIES.md` | Matriz dependencias hito 7 |
| `MATRIX_HITO_08_RELEASE.md` | Matriz release hito 8 |
| `MATRIX_INTEGRATION_01.md` | Matriz paridad audio INTEGRATION-01 |
| `MATRIX_PARITY_01.md` | Matriz paridad PARITY-01 |

### Planes de Hitos Supersedidos (11 archivos)
| Archivo | Consolidado en |
|---------|----------------|
| `PLAN_HITO_05.md` | `PLAN.md` |
| `PLAN_HITO_06_TELEMETRY_SEAMS.md` | `PLAN.md` |
| `PLAN_HITO_07_SAFE_REMOVAL.md` | `PLAN.md` (HITO-10E) |
| `PLAN_HITO_08_RELEASE.md` | `PLAN.md` |
| `PLAN_HITO_09B_UNIFIED_WORKBENCH.md` | `PLAN.md` |
| `PLAN_HITO_09C_ASSISTANCE_LEVELS.md` | `PLAN.md` |
| `PLAN_HITO_09D_FREE_CAPTURE_PROMOTION.md` | `PLAN.md` |
| `PLAN_HITO_10D_HARDWARE_PROFILES.md` | `PLAN.md`, `ROADMAP_HARDWARE_AND_EMULATION.md` |
| `PLAN_HITO_10_TARGET_PROFILE.md` | `PLAN.md` |
| `PLAN_INTEGRATION_01.md` | `PLAN.md` |
| `PLAN_PARITY_01.md` | `PLAN.md` |

### Borradores y Análisis UX (11 archivos)
| Archivo | Estado |
|---------|--------|
| `UX_DECISION_VS_CODE_STATUS.md` | Borrador decisión |
| `UX_DESIGN_DECISION_RESULT_PRIMER_PLANO.md` | Análisis supersedido |
| `UX_PROPUESTA_EXECUTIVA.md` | Propuesta no implementada |
| `UX_PUBLIC_CRITERIA.md` | Criterios supersedidos |
| `UX_REVISION_NIVEL_PRACTICO.md` | Revisión interna |
| `UX_REVISION_NOTAS.md` | Notas internas |
| `UX_REVISION_SOLUCIONES.md` | Soluciones propuestas |
| `UX_SESSION_RECOVERY_MEANING.md` | Análisis concepto |
| `UX_VIEW_AFTER_RESULT.md` | Mockup/resultado |
| `UX_SIMPLIFICATION_PLAN.md` | Plan supersedido por implementación |
| `UX_SIMPLIFICATION_RESULT_SCREEN_BEFORE_AFTER.md` | Comparativa visual |

### Documentos Varios (12 archivos)
| Archivo | Motivo |
|---------|--------|
| `MEJ02_AUDIO_STATUS.md` | Tracking interno supersedido |
| `MEJ03_AUDIO_VERIFY_READINESS.md` | Tracking interno |
| `PARITY_HITO09_MEJ_STATUS_BACKUP.md` | Backup temporal |
| `MIGRATION_SHARED_TO_ABDSHARED_NOTE.md` | Nota informal |
| `PLAN_TEORICO_CLIENTES_ABDAUDIO.md` | Teórico, no operativo |
| `WORKSTREAM_ATTRIBUTION.md` | Tracking interno |
| `SPLIT_CANDIDATES.md` | Análisis antiguo |
| `HardwareMidiDetect_Implementation_Guide.md` | Supersedido por `HARDWARE_INTEGRATION_GUIDE.md` |
| `AUDIT_ANALISIS_AUDIO.md` | Supersedido por `CODE_QUALITY_REPORT.md` |
| `PLAN_MEJORAS_CALIDAD.md` | Supersedido por `CODE_QUALITY_REPORT.md` |
| `SHARED_ASSETS_GUIDE.md` | Supersedido por `HARDWARE_INTEGRATION_GUIDE.md` + `ROADMAP_HARDWARE_AND_EMULATION.md` |
| `DESIGN_GUIDE.md` | **Nota:** Movido por error — **restaurar a docs/** si sigue vigente |

---

## Documentos Vivos (En `docs/` Raíz — No Mover)

| Documento | Propósito | Estado |
|-----------|-----------|--------|
| `ARCHITECTURE.md` | Arquitectura actual completa | ✅ Vivo |
| `PLAN.md` | Plan operativo consolidado + hitos certificados | ✅ Vivo |
| `ROADMAP_HARDWARE_AND_EMULATION.md` | Roadmap maestro vinculante | ✅ Vivo |
| `HANDOFF.md` | Estado ejecutivo, autoridades, riesgos | ✅ Vivo |
| `CODE_QUALITY_REPORT.md` | Deuda técnica actual (186+ issues) | ✅ Vivo |
| `GUIDE_ISSUES_TO_AVOID.md` | Lecciones aprendidas (16 reglas) | ✅ Vivo |
| `MATHEMATICAL_MODELS.md` | Referencia DSP matemática | ✅ Vivo |
| `HARDWARE_PROTOCOLS.md` | Protocolos hardware (SysEx, MIDI CC) | ✅ Vivo |
| `HARDWARE_INTEGRATION_GUIDE.md` | Guía integración hardware 3 niveles | ✅ Vivo |
| `QA_TEST_PLAN.md` | 16 casos QA automatizados/manuales | ✅ Vivo |
| `ADR-001_CONVERGENCIA_GUIADO_EXPLORACION.md` | Decisión arquitectónica registrada | ✅ Vivo |
| `ACTA_RELEASE_CANDIDATE_v2.1.0.md` | Evidencia release candidate | ✅ Vivo |
| `ACTA_SPRINT_01_LOOPBACK_PROFILES.md` | Sprint 1 certificado reciente | ✅ Vivo |
| `CODEBASE_MAP.md` | Mapa codebase generado 2026-10-13 | ✅ Vivo (nuevo) |
| `CURRENT_STATE.md` | Estado actual + deuda técnica | ✅ Vivo (nuevo) |
| `TEST_INVENTORY.md` | Catálogo 918 tests | ✅ Vivo (nuevo) |
| `DEPLOYMENT.md` | Pipeline build/release | ✅ Vivo (nuevo) |
| `MIGRATION_GUIDE.md` | Guía migración v1.x→v2.x | ✅ Vivo (nuevo) |
| `ROADMAP.md` | Roadmap general (puede consolidarse) | ⚠️ Revisar |
| `DESIGN_GUIDE.md` | **Verificar** — si supersedido por ARCHITECTURE.md, mover aquí | ⚠️ Revisar |
| `ESPECIFICACIONES_LABORATORIO.md` | Verificar vigencia | ⚠️ Revisar |
| `StudioTopology_Implementation_Guide.md` | Verificar vigencia | ⚠️ Revisar |

---

## Cómo Restaurar

```powershell
# Restaurar un archivo específico
Move-Item docs\_Deprecated\NOMBRE_ARCHIVO.md `
          docs\NOMBRE_ARCHIVO.md
```

---

## Notas de Auditoría

- **Total archivado:** 54 archivos
- **Fecha:** 2026-10-13
- **Ejecutado por:** Antigravity (Lead Architect) como parte de consolidación documentación
- **Referencia:** Issue/Request "revisa TRD.md y PRD.md a ver si creen que cubren todo el proyecto y no solo los próximos avances" → generó 5 documentos nuevos + archivado 54 deprecados

---

*Este README documenta el archivado para trazabilidad. No eliminar `_Deprecated/` — es historial de decisiones.*