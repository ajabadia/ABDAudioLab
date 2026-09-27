# INVENTARIO Y MATRIZ DE RIESGOS DE CONTRATOS LEGACY: HITO-10E
## TargetProfile — Retirada Segura de Duplicados Legacy (Fase E1)

**Fecha:** 2026-09-25  
**Proyecto:** ABDAudioLab  
**Autoridad de Auditoría:** Antigravity (Lead Architect & Metrologist) / ajabadia (Tándem Partner)  
**Estado:** AUDITADO Y CONGELADO (PROHIBIDA RETIRADA SIN PARIDAD DEMOSTRADA)  

---

## 1. Axioma Rector y Regla de Retirada

> *«Ningún contrato, campo, fixture ni fallback legacy se elimina porque parezca redundante; solo se retira cuando existe un reemplazo canónico, los consumidores han migrado, la equivalencia está probada y la ausencia del legacy mantiene la suite global verde.»*

### Reglas de Oro de HITO-10E
1. **Fase E1 no modifica código de producción**: Establece el inventario completo, traza consumidores y clasifica riesgos.
2. **Fase E2 no borra código**: Asigna el estado de migración (`Keep`, `Deprecate`, `Migrate`, `Removable`).
3. **Fase E3 crea puentes temporales**: Un adaptador explícito por cada contrato con consumidores activos.
4. **Fase E4 integra perfiles canónicos adaptados dentro de la interfaz legacy existente**: Los consumidores continúan temporalmente en la frontera `HardwareContractRegistry` / `HardwareContract`. La migración directa de consumidores hacia `TargetProfileService` queda fuera de alcance de E4 y no es un prerrequisito para retirar los tres JSON ya homologados.
5. **Fase E5 prueba paridad y ausencia**: Comprueba equivalencia y verifica 0 referencias.
6. **Fase E6 retirada selectiva**: Borrado estrictamente condicionado, un único grupo a la vez.
7. **Fase E7 certificación global**: Baseline completa en verde (0 FAIL).

---

## 2. Taxonomía de Duplicados

| Categoría | Denominación | Definición y Política de Retirada |
|:---:|---|---|
| **A** | **Duplicado Textual** | Dos constantes, cadenas o campos idénticos sin semántica adicional. Candidato directo a unificación. |
| **B** | **Duplicado de Representación** | Mismo dato en JSON, DTO C++ y fixture (ej. JSON Schema 2.0 vs TargetProfile Draft 2020-12). Necesario para serialización. |
| **C** | **Duplicado de Compatibilidad** | Campo antiguo leído por perfiles legacy y traducido al modelo nuevo. Requiere preservación de fallback. |
| **D** | **Duplicado de Frontera** | La misma información aparece en el dominio externo y en el interno (JSON $\rightarrow$ Parser $\rightarrow$ TargetProfile). Conservar separación. |
| **E** | **Duplicado de Evidencia** | Datos repetidos deliberadamente para trazabilidad (hashes de receta, plan y comandos). **ESTRICTAMENTE PROHIBIDO FUSIONAR**. |
| **F** | **Duplicado Aparente con Semántica Distinta** | Mismo nombre o valor similar, pero distinto momento temporal, autoridad o garantía metrológica. **PROHIBIDO FUSIONAR**. |

---

## 3. Matriz Exhaustiva de Artefactos y Contratos Legacy

| legacyId | path | symbol | kind | currentConsumers | canonicalReplacement | semanticParity | removalRisk | migrationState | requiredProof | owner |
|---|---|---|---|---|---|:---:|:---:|:---:|---|---|
| `LEGACY-JSON-MIGRATED-01` | `contracts/hardware/behringer_pro800.json` | `HardwareProfile` JSON Schema 2.0 | Perfil legacy | `HardwareContractRegistry`, tests de compatibilidad | `profiles/targets/behringer_pro800.target.json` | Proven | Low | `Retired (E6)` | Retirado físicamente en E6 tras certificar paridad E2E y de consumidores directos en E5.1. | Profiling / Hardware |
| `LEGACY-JSON-MIGRATED-02` | `contracts/hardware/yamaha_dx7.json` | `HardwareProfile` JSON Schema 2.0 | Perfil legacy | `HardwareContractRegistry`, tests de compatibilidad | `profiles/targets/yamaha_dx7.target.json` | Proven | Low | `Retired (E6)` | Retirado físicamente en E6 tras certificar paridad E2E y de consumidores directos en E5.1. | Profiling / Hardware |
| `LEGACY-JSON-MIGRATED-03` | `contracts/hardware/boss_ds1_distortion.json` | `HardwareProfile` JSON Schema 2.0 | Perfil legacy | `HardwareContractRegistry`, tests de compatibilidad | `profiles/targets/boss_ds1_distortion.target.json` | Proven | Low | `Retired (E6)` | Retirado físicamente en E6 tras certificar paridad E2E y de consumidores directos en E5.1. | Profiling / Hardware |
| `LEGACY-JSON-UNMIGRATED-28` | `contracts/hardware/*.json` (28 perfiles restantes) | `HardwareProfile` JSON Schema 2.0 | Perfil legacy | `HardwareContractRegistry`, `DrawerHardwareTab`, UI de hardware | `profiles/targets/<target>.target.json` (Pendiente) | Partial | **Critical** | `Keep` | **PROHIBIDO BORRAR**. Contiene los metadatos exclusivos de Juno, DeepMind, CZ-101, AIRA, etc. Requiere migración individual a TargetProfile. | Hardware Registry |
| `LEGACY-SCHEMA-01` | `contracts/hardware/hardware_profile.schema.json` | Schema JSON Draft-07 (v2.0) | Schema legacy | Validador de perfiles legacy | `docs/contracts/TARGET_PROFILE_CONTRACT.md` (Draft 2020-12) | Partial | High | `Keep` | Necesario para validar los 28 perfiles no migrados al cargar el directorio legacy. | Hardware Registry |
| `LEGACY-CORE-REGISTRY-01` | `src/core/HardwareContractRegistry.h` | `class HardwareContractRegistry` | Loader / Registry legacy | `ProfilingSession`, `ProfilingSequencer`, `DrawerHardwareTab`, `HardwareManager`, `ModulationPresetExporter`, tests | `TargetProfileService` + `TargetProfileResolution` | Partial | **High** | `Keep` (Con adaptador bridge) | Migrar consumidores uno a uno a `TargetProfileService`. Cero regresiones en `[hardware]`. | Core / Hardware |
| `LEGACY-CORE-DTO-01` | `src/core/HardwareContractRegistry.h` | `struct HardwareContract` | DTO legacy | Subsistemas de UI y secuenciación legacy | `TargetProfile` (`src/profiling/TargetProfile.h`) | Partial | **High** | `Keep` | Puente de compatibilidad `LegacyHardwareContractAdapter` (E3). | Core / Hardware |
| `LEGACY-CORE-DTO-02` | `src/core/HardwareContractRegistry.h` | `struct HardwareControl` | DTO legacy | `ProfilingHardwareDispatcher`, UI sliders/knobs | `TargetParameterMapping` + `TechnicalIdentifier` | Partial | Medium | `Keep` | Equivalencia funcional comprobada en `ResolvedExecutionPlan`. | Core / Hardware |
| `LEGACY-CORE-DTO-03` | `src/core/HardwareContractRegistry.h` | `struct HardwareLifecycleContract` | DTO legacy | `ProfilingSequencer` (setup/teardown) | `TransportPolicy` + `ExecutionPolicy` | Partial | Medium | `Keep` | Mapeo de tiempos de estabilización (settling delays) a `minimumInterMessageDelayMs`. | Core / Sequencing |
| `LEGACY-CORE-DTO-04` | `src/core/HardwareContractRegistry.h` | `struct MidiIdentityContract` | DTO legacy | `HardwareMidiDetector`, `MidiIdentityDetector` | `HardwareTransportPreflightService` | Proven | Medium | `Deprecate` | Los tests de HITO-10D2 ya usan el modelo canónico de 5 estados de identidad. | Hardware / Preflight |
| `LEGACY-ADAPTER-SHARED-01` | `src/core/SharedHardwareContractAdapter.h` | `class SharedHardwareContractAdapter` | Bridge adapter | `HardwareManager`, detección de dispositivos MIDI | Modelo canónico unificado | Partial | Medium | `Keep` | Puente activo entre `ABDSharedCode` y el dominio local. | Core / Adapters |
| `LEGACY-ADAPTER-PLUGIN-01` | `src/core/plugins/PluginHardwareContractAdapter.h` | `class PluginHardwareContractAdapter` | Adapter plugin-a-hardware | VST3 hosting legacy | `TargetContractDiscovery` + `TargetProfileService` | Proven | Low | `Deprecate` | `Dexed` y `ReferenceSynth` ya usan `TargetProfileService` y `TargetContractDiscovery`. | Plugins / Hosting |
| `LEGACY-DOMAIN-SYSEX-01` | `src/synth/SysExContracts.h` | `enum class Dx7SysExFormat`, `Dx7VoiceDecoder` | Decoder de dominio específico | `test_MidiAndSysExContracts_T3.cpp`, DX7 SysEx packing | Ninguno (Lógica de dominio de FM/DX7 legítima) | Proven | **Critical** | `Keep` (Permanente) | NO ES DUPLICADO. Es la lógica canónica de decodificación y empaquetado de voz DX7. | Synth / FM Domain |
| `LEGACY-DOMAIN-CASIO-01` | `src/measurement/adapters/casio/CasioCz101SysExContracts.h` | Framing, checksum y nibbles CZ-101 | Decoder de dominio específico | Casio CZ-101 adapters, tests de SysEx Casio | Ninguno (Lógica de dominio Casio legítima) | Proven | **Critical** | `Keep` (Permanente) | NO ES DUPLICADO. Es la lógica matemática de checksum de 7 bits y empaquetado CZ. | Measurement / Casio |
| `LEGACY-TEST-FIXTURES-01` | `src/tests/test_TargetProfileLegacyParity.cpp` | Oráculo congelado de caracterización legacy | Fixture de test | Suite `[legacy]` | Ninguno (Oráculo de regresión permanente) | Proven | **Critical** | `Keep` (Permanente) | Garantiza que ningún cambio futuro degrade la paridad matemática del oráculo congelado. | Testing / Metrology |

---

## 4. Análisis de Riesgos y Ecuaciones Métricas Formales Post-Retirada (E6)

### Reconciliación Aritmética de Entradas Filesystem Post-E6
- **`legacyFilesystemEntryCount`**: **29** archivos en `contracts/hardware/`.
- **`legacySchemaDocumentCount`**: **1** archivo de validación (`hardware_profile.schema.json`).
- **`legacyProfileDocumentCount`**: **28** documentos JSON de perfil de hardware activos.
- **`migratedLegacyProfileCount`**: **0** en `contracts/hardware/` (3 retirados físicamente en E6; resueltos desde `profiles/targets/`).
- **`unmigratedLegacyProfileCount`**: **28** perfiles que continúan resolviéndose exclusivamente como `NativeLegacyContract`.
- **`canonicalTargetProfileCount`**: **5** archivos canónicos certificados en `profiles/targets/`.

$$\text{legacyProfileDocumentCount (28)} = \text{migratedLegacyProfileCount (0)} + \text{unmigratedLegacyProfileCount (28)}$$
$$\text{legacyFilesystemEntryCount (29)} = \text{legacyProfileDocumentCount (28)} + \text{legacySchemaDocumentCount (1)}$$

### Grupo 1: Perfiles JSON en `contracts/hardware/`
- **Total de perfiles físicos remanentes:** 28 documentos.
- **Migrados y retirados en E6 con autorización:** 3 perfiles (`behringer_pro800.json`, `yamaha_dx7.json`, `boss_ds1_distortion.json`).
- **Pendientes de migrar:** 28 perfiles (marcados como `Keep` estricto `NativeLegacyContract`).
- **Invariante post-retirada:** Los 3 IDs históricos (`behringer_pro800`, `yamaha_dx7`, `boss_ds1_distortion`) resuelven mediante `acceptedUniqueIds` hacia `CanonicalTargetProfileAdapted` con paridad metrológica total.

### Grupo 2: `HardwareContractRegistry` y DTOs legacy
- **Total de consumidores en producción reconciliados:** 11 archivos de cabecera y controladores.
- **Decisión:** Mantener compatibilidad total y procedencia tipada implementada en Fase E4.

### Grupo 3: Decodificadores SysEx de dominio (`SysExContracts.h`, `CasioCz101SysExContracts.h`)
- **Clasificación:** **Categoría F (Duplicado aparente con semántica distinta)**.
- **Decisión:** Se congelan como código permanente de dominio (`Keep Permanente`).

---

## 5. Estado de Retirada E6 y Transición a E7

1. **Fase E6**: Ejecutada con éxito tras autorización explícita:
   - 3 JSON legacy retirados del sistema de archivos.
   - Inventario físico reconciliado en 29 entradas (28 perfiles + 1 schema).
   - Gates de inventario, resolución, ausencia y consumidores directos verificados en verde.
2. **Fase E7**: Pendiente de confirmación de baseline global para emisión del acta final y commit atómico.
