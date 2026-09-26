# PLAN OPERATIVO: PARITY-01 — Auditoría de Convergencia Guiado/Exploración

**Estado:** 🟢 CARACTERIZACIÓN Y PARIDAD E2E DE AUDIO CERTIFICADAS (Build #417)  
**Resultado:** 3 suites PASS ([parity] 35/35, [convergence] 16/16, [integration-01] 85/85) | 0 FAIL (Build #417 — 2026-09-23)  

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

| Paso | Hito | Objetivo | Estado |
|:---:|---|---|:---:|
| **1** | **HITO-CONVERGENCIA-01** | Derivar `ExperimentKind` de evidencia metodológica (eliminar bifurcación D3 por `workflowMode`) y retirar dependencia exclusiva de `guided/`. | ✅ **CERTIFICADO** (Build #414 — 16/16 assertions PASS) |
| **2** | **INTEGRATION-01** | `MockAudioEngine` / harness determinista para verificar paridad E2E (D5-eventos, D6-audio, D7-DSP, D8-manifest canónico). | ✅ **CERTIFICADO** (Build #417 — 85/85 assertions PASS) |
| **3** | **HITO-09A** | Contrato declarativo versionado de recetas metrológicas (`MeasurementRecipe`), validación C++ estricta, compilación determinista y cadena de 4 hashes canónicos. | ✅ **CERTIFICADO** (Build #419 — 103/103 assertions PASS) |
| **4** | **HITO-09B** | Banco de Trabajo Unificado: consumidor visual del contrato declarativo (catálogo de 3 recetas, visor legible, compilación y despacho sin tocar Guided/Classic ni bifurcar ejecución). | ✅ **CERTIFICADO** (Build #423 — 70/70 assertions PASS) |
| **5** | **HITO-09C** | Niveles de Asistencia (Rápido, Configurable, Avanzado) como vistas del mismo contrato. | ✅ **CERTIFICADO** (Build #425 — 83/83 assertions PASS) |
| **6** | **HITO-09D** | Promoción de Toma Libre (Exploración) a `MeasurementRecipe` formal. | ✅ **CERTIFICADO** (Build #430 — 148/148 assertions PASS) |
| **7** | **HITO-10A** | `TargetProfile` declarativos en JSON: Contrato, schema Draft 2020-12, modelo C++, validador RFC 6901, hash RFC 8785 y piloto `ReferenceSynth`. | ✅ **CERTIFICADO** (Build #434 — 117/117 assertions PASS) |
| **8** | **HITO-10B** | `TargetProfile`: Paridad declarativa exacta vs legacy (caracterización congelada como oráculo y paridad exacta de `ResolvedExecutionPlan`). | ✅ **CERTIFICADO** (Build #436 — 189/189 assertions PASS) |
| **9** | **HITO-10C** | `TargetProfile`: Migración piloto de plugin real (`Dexed` VST3) y auditoría de fixity binaria. | ✅ **CERTIFICADO** (Build #439 — 38.630/38.630 assertions PASS) |
| **10** | **HITO-10D1** | `TargetProfile`: Hardware digital (`MidiCcIdentifier`, `MidiSysExIdentifier`) y analógico manual (`ManualOperatorIdentifier`): Contratos y resolución hermética. | ✅ **CERTIFICADO** (Build #443 — 315/315 assertions PASS) |
| **12** | **HITO-10V0** | `VES Capability & Transport Feasibility Spike`: Discovery, hosting y estabilidad básica de VES/CZ-101 (`test_VesCz101Feasibility.cpp`). | ✅ **CERTIFICADO (DISCOVERY SPIKE)** (Build #459 — 22/22 assertions PASS) |
| **13** | **HITO-10V0.1** | `VES CZ-101 Semantic Control & Audible Output Probe`: Demostración de estado CZ-101 válido, NoteOn audible, control semántico SysEx/CC, repetibilidad audible y cero ROM en artefactos (`test_VesCz101SemanticControl.cpp`). | ✅ **CERTIFICADO (SONDA CONCLUIDA — BLOQUEO PREVENTIVO DE V1)** (Build #461) |
| **14** | **HITO-10V1** | `FirmwareEmulated Targets / VES Integration`: Formalización declarativa y transporte según resultados de V0.1. | ⛔ **BLOQUEADO (Sin control audible headless en VES)** |
| **15** | **HITO-10E** | `TargetProfile`: Retirada segura de duplicados legacy con preservación de fallbacks de compatibilidad (Fases E1 a E5 certificadas; E6 pendiente de autorización). | 🟢 **FASES E1-E5 CERTIFICADAS (E6 BLOQUEADA)** |

---

## 📌 HITO-10E: Retirada Segura de Duplicados Legacy
**Axioma rector:** *«Ningún contrato, campo, fixture ni fallback legacy se elimina porque parezca redundante; solo se retira cuando existe un reemplazo canónico, los consumidores han migrado, la equivalencia está probada y la ausencia del legacy mantiene la suite global verde.»*

- **E1 (Certificado):** Inventario exhaustivo de contratos y consumidores (`docs/audits/TARGETPROFILE_LEGACY_INVENTORY.md` y `test_TargetProfileLegacyInventory.cpp` — 31/31 assertions PASS). *(0 líneas borradas).*
- **E2 (Certificado y Congelado):** Matriz de equivalencia y análisis de riesgos (32 entradas filesystem: 31 perfiles + 1 schema; 3 perfiles migrados + 28 no migrados) formalizada en `docs/audits/TARGETPROFILE_EQUIVALENCE_MATRIX.md`. *(0 líneas borradas).*
- **E3 (Certificado):** Adaptador unidireccional estricto `TargetProfileLegacyAdapter` (`TargetProfile` -> `HardwareContract` legacy de solo lectura) y `test_TargetProfileLegacyAdapter.cpp` (228 assertions en 17 casos). Extirpado `fromLegacyHardwareContract` de runtime. *(0 líneas borradas).*
- **E4 (Certificado):** Integración controlada en `HardwareContractRegistry`: carga canónica all-or-nothing, precedencia canónica sobre legacy, procedencia tipada (`HardwareContractResolutionSource`), indexación de aliases y fail-closed estricto ante colisiones no certificadas (`test_HardwareContractRegistryCanonicalResolution.cpp` — 15 assertions en 10 casos). *(0 líneas borradas).*
- **E5 (Certificado):** Paridad funcional extremo a extremo (`test_TargetProfileLegacyEndToEndParity.cpp`) y ausencia controlada hermética (`test_TargetProfileLegacyControlledAbsence.cpp`) simulada sobre fixture temporal con cero alteraciones en el árbol de trabajo (156 assertions en 9 casos; 456 assertions acumuladas en HITO-10E; 613 assertions en `[hardware]`). *(0 archivos reales borrados).*
- **E6 (Bloqueada):** Retirada selectiva y condicionada de los 3 archivos JSON migrados (`behringer_pro800.json`, `yamaha_dx7.json`, `boss_ds1_distortion.json`). *Requiere autorización formal explícita del usuario.*
- **E7:** Certificación global de baseline sin dependencias legacy residuales y emisión de `ACTA_HITO_10E_LEGACY_REMOVAL.md`.

---

## 📌 Documento Rector de Roadmap Persistente
Para la especificación completa, reglas normativas, delimitación de responsabilidades y contratos de fail-closed y emulación, consultar el documento permanente:
👉 [`docs/ROADMAP_HARDWARE_AND_EMULATION.md`](docs/ROADMAP_HARDWARE_AND_EMULATION.md)


