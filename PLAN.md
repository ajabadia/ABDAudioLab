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
| **15** | **HITO-10E** | `TargetProfile`: Retirada segura de duplicados legacy con preservación de fallbacks de compatibilidad (Fases E1 a E7 completadas y certificadas). | ✅ **CERTIFICADO** (Build #495 — 754 test cases, 269.224 assertions PASS, 0 FAIL, 8 SKIPPED) |
| **16** | **HITO-10D2.7A** | `Banco Físico Controlado`: Preflight, Identidad, Consentimiento y Observación Sin Medición Metrológica (D2.7A.1 a D2.7A.6). | ✅ **CERTIFICADO** (Build #503 — 12 test cases, 132/132 assertions PASS) |
| **17** | **HITO-10D2.7B** | `Banco Físico Metrológico`: Profiling de hardware real con audio y exportación certificada. | ⏳ **PENDIENTE (Bloqueado tras D2.7A)** |
| **18** | **HITO-SHARED-SYNC** | `Enriquecimiento de ABDSharedCode`: Transferencia del fix de fallback JUCE Win32, 5 estados ortogonales de preflight y exclusión de puertos virtuales. | ✅ **CERTIFICADO (SS6 — 2026-09-28 — 852 tests / 269.610 assertions, commit pendiente de autorización)** |

---

## 📌 HITO-10D2.7A: Banco Físico Controlado (Preflight, Identidad, Consentimiento y Observación)

**Axioma rector:** *«Un cable conectado no convierte una capacidad hipotética en una medición válida; el sistema debe demostrar identidad, consentimiento, trazabilidad y bloqueo seguro antes de poder confiar en cualquier resultado exportable.»*

**Objetivo:** Demostrar que el pipeline hermético D2.1–D2.6 se comporta de forma segura y determinista contra dispositivos físicos reales conectados, **sin producir mediciones metrológicas exportables ni alterar memorias de hardware**.

### Tareas Atómicas
- [x] **D2.7A.1:** Inventario del banco físico controlado y contrato de seguridad de prueba no destructiva. *(CERTIFICADO — 18/18 gates verificados con hardware real: Behringer DeepMind 12D).*
- [x] **D2.7A.2:** Preflight físico de solo lectura: selección de puerto, disponibilidad, identidad y diagnóstico de los cinco estados canónicos. *(CERTIFICADO — Build #499; 4 test cases, 25/25 assertions PASS; "DeepMind12D" verificado con 0 bytes transmitidos; clasificación: PhysicalIdentityUnavailableButOperatorConfirmed).*
- [x] **D2.7A.3:** Consentimiento físico: operador, target, puerto, plan, digest de comando, momento de aprobación y criterios de revocación. *(CERTIFICADO — Aprobación formal otorgada por el operador para B0 01 00, commandDigest sha256:5d09261b0204e9f5d61cb52c9a14051c0e5ab3666a77aa7bf70dab53b9f0589b y CanonicalV1 de 535 bytes).*
- [x] **D2.7A.4:** Prueba de despacho mínimo controlado: un único CC seguro o mensaje de identificación permitido, con pacing y evidencia forense. *(CERTIFICADO — Build #502; 7 test cases, 79/79 assertions PASS; exactamente 1 mensaje CC de 3 bytes wire B0 01 00 emitido a DeepMind12D; transporte cerrado inmediatamente con 0 bytes adicionales; evidencia forense inmutable OK).*
- [x] **D2.7A.5:** Prueba de fallo deliberado no destructivo: desconexión de puerto, timeout o identidad no coincidente, verificando fail-closed y `ExportReadiness` bloqueado. *(CERTIFICADO — Build #503; 12 test cases, 132/132 assertions PASS; 0 bytes físicos emitidos; 5 familias de fallo validadas herméticamente; ExportReadiness::Blocked permanente).*
- [x] **D2.7A.6:** Acta de banco controlado: resultado por target, puertos usados, hashes de plan, evidencia y limitaciones. *(CERTIFICADO — Emitida ACTA_HITO_10D2_7A_PHYSICAL_BENCH.md).*

**Autorización de transmisión física:** ⛔ **CONCLUIDA Y REVOCADA — 0 BYTES FÍSICOS AUTORIZADOS EN EL SISTEMA.**

### Límites Obligatorios y Reglas de Protección
- ⛔ No realizar barridos de parámetros.
- ⛔ No iniciar profiling automatizado completo.
- ⛔ No exportar mediciones.
- ⛔ No ejecutar SysEx potencialmente destructivo.
- ⛔ No escribir bancos, patches o memoria del dispositivo.
- ⛔ No asumir ACK si el dispositivo no lo ofrece explícitamente.
- ⛔ No marcar D2.7B como certificado desde D2.7A.

### Clasificación de Salida por Target Físico
1. `PhysicalPreflightVerified`
2. `PhysicalIdentityUnavailableButOperatorConfirmed`
3. `PhysicalIdentityMismatchBlocked`
4. `PhysicalTransportDisconnectedFailClosed`
5. `PhysicalResponseTimeoutFailClosed`
6. `PhysicalControlledDispatchObserved`
7. `UnsupportedOrUnsafeForCurrentBench`

---

## 📌 HITO-10E: Retirada Segura de Duplicados Legacy (Cerrado)
**Axioma rector:** *«Ningún contrato, campo, fixture ni fallback legacy se elimina porque parezca redundante; solo se retira cuando existe un reemplazo canónico, los consumidores han migrado, la equivalencia está probada y la ausencia del legacy mantiene la suite global verde.»*

- **E1 (Certificado):** Inventario exhaustivo de contratos y consumidores (`docs/audits/TARGETPROFILE_LEGACY_INVENTORY.md` y `test_TargetProfileLegacyInventory.cpp` — 31/31 assertions PASS). *(0 líneas borradas).*
- **E2 (Certificado y Congelado):** Matriz de equivalencia y análisis de riesgos (32 entradas filesystem: 31 perfiles + 1 schema; 3 perfiles migrados + 28 no migrados) formalizada en `docs/audits/TARGETPROFILE_EQUIVALENCE_MATRIX.md`. *(0 líneas borradas).*
- **E3 (Certificado):** Adaptador unidireccional estricto `TargetProfileLegacyAdapter` (`TargetProfile` -> `HardwareContract` legacy de solo lectura) y `test_TargetProfileLegacyAdapter.cpp` (228 assertions en 17 casos). Extirpado `fromLegacyHardwareContract` de runtime. *(0 líneas borradas).*
- **E4 (Certificado):** Integración controlada en `HardwareContractRegistry`: carga canónica all-or-nothing, precedencia canónica sobre legacy, procedencia tipada (`HardwareContractResolutionSource`), indexación de aliases y fail-closed estricto ante colisiones no certificadas (`test_HardwareContractRegistryCanonicalResolution.cpp` — 15 assertions en 10 casos). *(0 líneas borradas).*
- **E5 (Certificado):** Paridad funcional extremo a extremo (`test_TargetProfileLegacyEndToEndParity.cpp`) y ausencia controlada hermética (`test_TargetProfileLegacyControlledAbsence.cpp`) simulada sobre fixture temporal con cero alteraciones en el árbol de trabajo (156 assertions en 9 casos).
- **E5.1 (Certificado):** Cobertura exhaustiva de consumidores directos en ausencia hermética (`test_TargetProfileLegacyConsumerParity.cpp` y `test_TargetProfileLegacyHotplugParity.cpp` — 104 assertions en 7 casos; 560 assertions acumuladas en HITO-10E; baseline global 754 tests / 269.222 assertions PASS). Superado el Gate de Retirada.
- **E6 (Certificado):** Retirada física de los 3 archivos JSON legacy migrados (`behringer_pro800.json`, `yamaha_dx7.json`, `boss_ds1_distortion.json`). Reconciliación métrica en 29 entradas filesystem (28 perfiles + 1 schema). Correcciones auxiliares de estabilidad RAII en `PluginWindowController` y resiliencia de portapapeles en `test_SmokeStep4UI.cpp`.
- **E7 (Certificado):** Verificación global de baseline completa (754 test cases, 746 PASS, 8 SKIPPED históricos, 0 FAIL, 269.224 assertions PASS), emisión de `docs/audits/ACTA_HITO_10E_LEGACY_REMOVAL.md`. Commit atómico `77c7bb9` verificado. Hito 10E concluido.

## 📌 HITO-SHARED-SYNC: Enriquecimiento de ABDSharedCode (Sincronización de Seguridad)

**Axioma rector:** *«ABDSharedCode puede descubrir, enumerar, filtrar y presentar endpoints de hardware; ABDAudioLab conserva la autoridad exclusiva para consentir, despachar, registrar evidencia, bloquear exportación y gobernar sesiones metrológicas.»*

**Objetivo:** Transferir a `ABDSharedCode` las mejoras de seguridad de hardware descubiertas en D2.7A sin trasladar autoridad metrológica:
1. Validación estricta previa contra `getAvailableDevices()` evitando fallback silencioso al device índice 0 en Windows.
2. Contrato tipado de apertura de endpoints (`MidiEndpointOpenResult`).
3. Adopción del modelo formal de 5 Estados de Preflight / Identidad para descubrimiento.
4. Clasificación y política explícita de exclusión de puertos virtuales en consultas SysEx broadcast.

### Tareas Atómicas
- [x] **SS1:** Inventario de APIs compartidas, consumidores, contratos y baseline. *(COMPLETADO — Emitido `docs/audits/ABD_SHARED_CODE_HARDWARE_SYNC_INVENTORY.md`).*
- [x] **SS2:** Validación estricta de endpoint MIDI en `JuceMidiHardwareBackend` (fail-closed, 0 fallback a device 0). *(CERTIFICADO — Build #505; 12 test cases, 79/79 assertions PASS; suite hermética `test_JuceMidiHardwareBackendStrictOpen.cpp`).*
- [x] **SS3:** Modelo de preflight de cinco estados aditivo en `HardwareMidiDetect`. *(CERTIFICADO — Build #507; 12 test cases, 57/57 assertions PASS; suite hermética `test_HardwareMidiDetectorIdentityState.cpp`).*
- [x] **SS4-core:** Clasificación de endpoints virtuales y política configurable de SysEx broadcast. *(CERTIFICADO — Build #508; 20 test cases, 27/27 assertions PASS; suite hermética `test_MidiEndpointSafetyPolicy.cpp`).*
- [x] **SS4.1:** Integración del clasificador con hotplug, UI y labels — cero aperturas, cero inquiries. *(CERTIFICADO — Build #508; 20 test cases, 43/43 assertions PASS; suite `test_MidiEndpointSafetyPolicyWiring.cpp`).*
- [x] **SS5:** Contrato hermético `ABDSharedCode → ABDAudioLab` — `SharedMidiHardwareAdapter`, frontera de autoridad, `ExportReadiness::Blocked`, `JuceMidiTransport::write()` = 0. *(CERTIFICADO — Build #510; 22 test cases, 87/87 assertions PASS; suite `test_SharedMidiHardwareIntegrationContract.cpp`).*
- [x] **SS6:** Certificación técnica global, baseline verde, acta de cierre y preparación de commit atómico. *(CERTIFICADO — Build #512; Baseline no-VES: 843 test cases: 835 PASS, 8 SKIPPED, 0 FAIL; 269.619 assertions PASS; salida 0; 0 modales; acta completada; sellado en 6 commits atómicos).*
- [x] **SS6.1:** Estabilización del runner global y baseline completa no interactiva. *(CERTIFICADO — Supresión de modales CRT en TestMain.cpp, safe callback en test_FskAudioModem.cpp, SKIP explícito en test_TargetProfilePhysicalPreflightBench.cpp, aislamiento de [ves]).*

---

## 📌 HITO-AUDIO-AB-5D: Acceptance Matrix and Canonical Preset Validation (Cerrado y Certificado)

**Axioma rector:** *«Una comparación A/B no certifica equivalencia porque dos renders "suenen parecidos"; debe declarar la excitación, congelar el contexto de render, medir diferencias reproducibles y emitir un veredicto trazable con tolerancias apropiadas al comportamiento esperado del preset.»*

**Fronteras de Seguridad:**
- Entorno: 100% en memoria sobre streams de audio software y artefactos de QA.
- Hardware físico: ⛔ 0 bytes autorizados.
- D2.7B y HITO-10V1: ⛔ BLOQUEADOS.
- ExportReadiness: Blocked (permanente).

### Tareas Atómicas
- [x] **5D.1:** Contrato de corrida A/B y schema de resultado. *(COMPLETADO — Emitido `docs/qa/audio-ab-5d-acceptance-spec.md`).*
- [x] **5D.2:** Catálogo de cinco presets acústicos canónicos. *(COMPLETADO — Emitido `docs/qa/audio-ab-5d-canonical-preset-matrix.md`).*
- [x] **5D.3:** Protocolos de excitación determinista y reset. *(COMPLETADO — Emitido `docs/qa/audio-ab-5d-excitation-protocol.md`).*
- [x] **5D.4:** Matriz inicial de 10 corridas software. *(COMPLETADO — 10/10 test cases PASS, determinismo intra-motor 10/10 bit-exact, cero clipping, métricas observadas extraídas en `test_AudioABCanonicalPresetRuns5D.cpp`).*
- [x] **5D.5:** Métricas, alineación acotada y diagnósticos tipados. *(COMPLETADO — Implementado `AudioABMetrics5D.h`, estructuras de métricas, serializador JSON, diagnósticos tipados y 10 tests de casos límite en `test_AudioABMetricsAndDiagnostics5D.cpp` con 39/39 assertions PASS).*
- [x] **5D.6:** Policy de tolerancias por clase de preset (calibración provisional). *(COMPLETADO — Formalizada especificación en docs/qa/audio-ab-5d-tolerance-policy.md, schema JSON docs/qa/audio-ab-5d-tolerance-policy.schema.json, implementación en AudioABTolerancePolicy5D.{h,cpp} y 12 tests en test_AudioABTolerancePolicy5D.cpp con 29 assertions PASS).*
- [x] **5D.7:** Fixtures herméticas y pruebas unitarias de comparator/verdict. *(CERTIFICADO — 18/18 test cases PASS, 158/158 assertions PASS en `test_AudioABComparatorAndVerdict5D.cpp`; baseline global no-VES certificada: 893 test cases: 884 PASS, 9 SKIPPED, 0 FAIL; 269.896 assertions PASS).*
- [x] **5D.8:** Ejecución de matriz QA, revisión de deltas y acta de aceptación. *(CERTIFICADO — 10/10 corridas canónicas evaluadas bajo policy provisional-v1; 4 PASS [Accepted], 6 WARN [AcceptableWithExpectedDispersion], 0 FAIL; 10 reportes JSON persistidos con hash en `docs/qa/runs/`; emitido `ACTA_HITO_AUDIO_AB_5D8_SOFTWARE_ACCEPTANCE.md`).*
- [x] **5D.9:** CI dedicado, baseline y certificación. *(CERTIFICADO — Configurado `.github/workflows/audio-ab-5d-ci.yml`, `docs/qa/audio-ab-5d-baseline-manifest.json`, `docs/qa/audio-ab-5d-artifacts.sha256`, suite de integridad `test_AudioABBaselineManifestValidation5D.cpp` y emitido `ACTA_HITO_AUDIO_AB_5D.md`).*
- [x] **POST-5D.1:** Publicación, CI remoto y saneamiento documental. *(COMPLETADO — Sustitución de enlaces file:/// por enlaces relativos en actas, ajuste de formulación de exclusión VES no permanente, gate 6 no-VES en CI).*

---

### Microhito POST-5D.5 — Incidente de Reproducibilidad CI: Hermetización de Rutas y Baseline No-VES

> **Naturaleza del microhito:** Incidente de reproducibilidad técnica de CI aislado e independiente del alcance funcional de Audio A/B 5D. No amplía el hito ni altera sus resultados ni evidencia local.  
> **Diagnóstico del incidente:** El fallo de Gate 6 en CI (Run #6) confirmó que la baseline local no era hermética debido a la deuda técnica de rutas absolutas personales (`D:/desarrollos/ABDSynths/...`) codificadas en tests preexistentes, fixtures y código de soporte. Gates 1–5 certifican de forma inmutable el contenido específico de Audio A/B 5D; Gate 6 impedía certificar la reproducibilidad global no-VES.  
> **Objetivo:** Eliminar dependencias fijas `D:/desarrollos/ABDSynths/...` de tests, fixtures y soporte que participan en la baseline `~[ves]`, logrando que la suite completa se ejecute limpiamente desde un clon en una ruta arbitraria de Windows.  
> **Límites inviolables y fuera de alcance:**  
> - ⛔ Prohibido cambiar tolerancias de Audio A/B 5D.  
> - ⛔ Prohibido alterar reportes JSON, manifest o hashes canónicos de 5D.  
> - ⛔ Prohibido marcar Gate 6 como opcional o tolerar fallos (`--allow-running-no-tests`, `|| true`, `continue-on-error`).  
> - ⛔ Prohibido ejecutar MIDI físico (0 bytes autorizados).  
> - ⛔ Prohibido desbloquear D2.7B, VES o ExportReadiness.

#### Matriz de Tareas y Criterios de Aceptación (POST-5D.5)

| Tarea | Denominación | Entregable | Criterio de Aceptación |
|:---:|---|---|---|
| **POST-5D.5.1** | Inventario de referencias absolutas | `docs/audits/POST_5D5_HARDCODED_PATHS_INVENTORY.md` | ✅ **CERTIFICADO:** 26 referencias activas identificadas, clasificadas y con decisión inequívoca de migración. Cero diffs en `docs/qa/`. |
| **POST-5D.5.2** | Resolver único de rutas de repositorio | `src/core/LabResourcePaths.{h,cpp}` + `src/tests/test_LabResourcePaths.cpp` | ✅ **COMPLETO CON DESVÍO:** el módulo canónico ya existía y **no** se creó `TestPathResolver.h` (se descartó por duplicar la jerarquía). Añadidas `optionalRepoResource()` y `sharedAssetsDir()` para poder usarlo en producción sin lanzar. |
| **POST-5D.5.3** | Migración de tests `TargetProfile*` | Suites `test_TargetProfile*.cpp` y presets migrados | ✅ **COMPLETO:** `TransportSafety` (R01–R06), `TransportPolicy` (R07), `DexedHosting` (R14, R15), `DexedBehavior` (R16) y `SynthTargetLifecycleAdapter` (R17) resuelven vía `core::repoResource()`. Cero paths absolutos personales en `src/`. |
| **POST-5D.5.4** | Migración de assets y contratos compartidos | Suites `test_MidiDeviceHotplugMonitor`, `test_MidiIdentityDetector`, `test_OfficialLutGeneration` | ✅ **COMPLETO:** `contracts/hardware/` versionado en el repo (40 contratos + schemas). `test_ContractsSnapshotDrift` hace SKIP limpio sin `ABDSharedAssets`. |
| **POST-5D.5.5** | Separación build/runtime para VST3 y assets | `test_SynthTargetLifecycleAdapter.cpp` y fallbacks de GUI/session | ✅ **COMPLETO:** `ReferenceSynth.vst3` pasa a rutas sintéticas (el test solo valida normalización, nunca toca disco). Producción migrada a `optionalRepoResource`/`sharedAssetsDir`. Los VST3 de terceros y el worker **no son migrables**: quedan en allowlist con motivo. |
| **POST-5D.5.6** | Limpieza y validación de workflow CI | `.github/workflows/audio-ab-5d-ci.yml` | ✅ **COMPLETO:** workflow limitado a checkout, dependencias, build y Gates 1–6. 0 supresores. |
| **POST-5D.5.7** | Guardrail permanente contra rutas absolutas | `src/tests/test_ResourcePathHygiene.cpp` | ✅ **COMPLETO (6 casos):** dos barridos independientes — `getCurrentWorkingDirectory` y literales `D:/desarrollos` / `C:/Users/` — sobre `src/tests` y `src/` producción, con invariantes anti-vacuidad. *(El nombre de fichero previsto `test_HardcodedPathGuardrail.cpp` nunca existió; el barrido por CWD sin el de absolutos fue un diseño intermedio REFUTADO, ver §10.3 del inventario.)* |
| **POST-5D.5.8** | Prueba de hermeticidad local en condiciones hostiles | Evidencia de ejecución local | `ABDAudioLab_Tests.exe "~[ves]"` ejecutado desde cwd arbitrario, build limpio, sin variables de entorno locales personales. 100% PASS sin regresión en Gates 1–5 (`[audioab_5d]`). |
| **POST-5D.5.9** | Regresión CI (Run #7) y evidencia de cierre | GitHub Actions Run #7 exitoso + `ACTA_HITO_AUDIO_AB_5D.md` actualizado | Run #7 verde en Gates 1–6; publicación de artefactos QA; tag `hito-audio-ab-5d-certified-ci` creado sobre el commit certificado; acta sellada local y remotamente. |

#### Diseño Técnico del Resolver (`TestPathResolver`)

> ⚠️ **SUPERADO — no implementado tal cual.** El módulo canónico real es
> `src/core/LabResourcePaths.{h,cpp}` (`abdaudiolab::core::repoResource`). `TestPathResolver.h`
> se escribió y se **eliminó** por duplicar esa jerarquía. Contraste completo en
> `docs/audits/POST_5D5_HARDCODED_PATHS_INVENTORY.md` §10.

**Jerarquía de resolución:**
1. **Variable de entorno explícita de test:** `ABDAUDIOLAB_REPO_ROOT`.
2. **Ruta calculada desde el ejecutable de tests:**  
   `juce::File::getSpecialLocation(juce::File::currentExecutableFile)`  
   → Subir directorios según estructura de build (`../../`, `../../../`)  
   → Validar la presencia de marcadores propios del repositorio:
     - `profiles/targets/`
     - `fixtures/`
     - `CMakeLists.txt`
     - `docs/`
     *(No se acepta una carpeta únicamente por llamarse `ABDAudioLab`; debe comprobarse la presencia real de sus recursos).*
3. **Ruta de trabajo actual:** `juce::File::getCurrentWorkingDirectory()` solo como fallback de desarrollo auxiliar si los anteriores no aplican y contiene los marcadores.
4. **Error determinista:** Lanzar excepción explicativa o emitir aserción fallida descriptiva detallando qué marcador faltó y qué rutas candidatas fueron exploradas (cero fallbacks silenciosos a `D:/`).

**Signatura canónica del Helper:**
```cpp
namespace test_support
{
    juce::File resolveRepoRootForTests();
    juce::File resolveRepoResource(const juce::String& relativePath);
    juce::File resolveSharedAssetsRootForTests();
}
```

**Política para dependencias y assets externos:**
- Para `ABDSharedCode`: se mantiene checkout pinneado como hermano de repositorio (`path: ABDSharedCode`, ref pinneada en CI).
- Para contratos de hardware (`ABDSharedAssets/contracts`): ABDAudioLab dispone de su propio snapshot versionado en `contracts/hardware/`. Los tests deben consumir este contrato versionado localmente o resolver a través de `resolveSharedAssetsRootForTests()` con fallback documentado a la copia interna del repositorio. Cero creación de carpetas vacías artificiales.

#### Condiciones Estrictas para Lanzamiento de Run #7
- [ ] Cero rutas personales codificadas en el alcance de Gate 6 y codebase de tests.
- [ ] Recursos de ABDAudioLab resueltos desde raíz verificable mediante marcadores.
- [ ] Recursos externos pinneados, inyectados o versionados.
- [ ] Sin modificaciones en hashes, IDs, tolerancias ni reportes 5D.
- [ ] Workflow sin hacks de directorios ficticios.
- [ ] Baseline `~[ves]` 100% verde en local ejecutada desde cwd arbitrario.
- [ ] Gates 1–5 siguen 100% verdes.

## 📌 Documento Rector de Roadmap Persistente
Para la especificación completa, reglas normativas, delimitación de responsabilidades y contratos de fail-closed y emulación, consultar el documento permanente:
👉 [`docs/ROADMAP_HARDWARE_AND_EMULATION.md`](docs/ROADMAP_HARDWARE_AND_EMULATION.md)



