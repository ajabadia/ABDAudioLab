# PLAN OPERATIVO: PARITY-01 — Auditoría de Convergencia Guiado/Exploración

**Hito:** PARITY-01 (PARITY-01A / PARITY-01B)  
**Estado:** 🟢 CARACTERIZACIÓN UNITARIA CERTIFICADA | ⏳ PARIDAD E2E DE AUDIO PENDIENTE (INTEGRATION-01)  
**Resultado:** 2 test cases PASS | 35 assertions PASS | 0 FAIL (Build #411 — 2026-09-23)  

---

## Regla de Oro
*«Caracterizar primero; comparar después; migrar después; retirar al final.»*

---

## Fases

### ✅ Fase 1: Inspección Estática (COMPLETADA)
- Motor único confirmado: `ProfilingSequencer` compartido por GS y LS.
- Bifurcación localizada: `ProfilingSessionController.cpp:~1019` — `r.kind = (workflowMode == Guided) ? Measurement : Exploration`.
- Copia condicional localizada: `ProfilingSessionController.cpp:761` — archivos `guided/`.
- Toma Libre identificada como captura ad-hoc sin plan metrológico.

### ✅ Fase 2: Test de Caracterización (COMPLETADA)
- Archivo: [`src/tests/test_Parity01GuidedVsClassic.cpp`](src/tests/test_Parity01GuidedVsClassic.cpp)
- Registrado en `CMakeLists.txt` (línea 899).

### ✅ Fase 3: Validación (COMPLETADA — Build #411)

```
.\build\Release\ABDAudioLab_Tests.exe "[parity]"
All tests passed (35 assertions in 2 test cases)
```

| Test | Assertions | Estado |
|---|:---:|---|
| PARITY-01A: contrato estructural (D1, D2, D3, D5-struct) | 17 | ✅ PASS |
| PARITY-01B: Toma Libre no certificable (D8) | 18 | ✅ PASS |

### Nota sobre D5/D6/D7 con audio real
`ProfilingSequencer` es `juce::Thread` que bloquea esperando `audioDeviceIOCallbackWithContext` (WASAPI). Sin hardware real, el hilo no termina. La evidencia estructural es suficiente para PARITY-01. La validación numérica de buffers y métricas se delega a **INTEGRATION-01**.

---

## Hallazgos Certificados

1. **Motor único** (D1, D2, D5): `ProfilingSequencer` idéntico en GS y LS. `SequencerState::Idle` al arrancar ambas rutas.
2. **Sesión idéntica** (D2): misma `ProfilingSession`, mismos 3 `TestCases`, misma nota MIDI C4, mismo gate 250ms.
3. **Bifurcación única** (D3): `r.kind` cambia según `workflowMode`. Divergencia de presentación, no de motor. **Objetivo de HITO-CONVERGENCIA-01.**
4. **Toma Libre bloqueada** (D8): `ExportReadiness::Decision::Blocked` confirmado con `canProceed() == false`.

---

## Próximos Hitos (Secuencia Vinculante)

| Paso | Hito | Objetivo | Dependencia |
|:---:|---|---|---|
| **1** | **HITO-CONVERGENCIA-01** | Derivar `ExperimentKind` de evidencia metodológica (eliminar bifurcación D3 por `workflowMode`) y retirar dependencia de `guided/`. | PARITY-01 completado ✅ |
| **2** | **INTEGRATION-01** | `MockAudioEngine` / harness determinista para verificar paridad E2E (D5-eventos, D6-audio, D7-DSP, D8-manifest canónico). | HITO-CONVERGENCIA-01 |
| **3** | **HITO-09** | Banco de trabajo unificado con presets declarativos JSON y 3 niveles de asistencia (Rápido, Configurable, Avanzado). | INTEGRATION-01 |
