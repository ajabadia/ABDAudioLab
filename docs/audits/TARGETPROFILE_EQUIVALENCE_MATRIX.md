# MATRIZ FORMAL DE EQUIVALENCIA, PRECEDENCIA Y RIESGOS: HITO-10E (FASE E2)
## TargetProfile Canónico vs HardwareContract Legacy

**Fecha:** 2026-09-26  
**Proyecto:** ABDAudioLab  
**Autoridad Metrológica:** Antigravity (Lead Architect & Metrologist) / ajabadia (Tándem Partner)  
**Estado:** FORMALIZADO Y VINCULANTE (Fase E2 Congelada)  

---

## 1. Axioma Rector y Regla de Unidireccionalidad Estricta

> *«Un HardwareContract v2.0 es una proyección parcial de compatibilidad de solo lectura. Queda terminantemente prohibido generar en runtime objetos TargetProfile a partir de HardwareContract legacy, pues dicha proyección inventaría o degradaría invariantes canónicas de seguridad, consentimiento, preflight y políticas de transporte.»*

### Reglas Rectoras
1. **Fuente de Verdad Única**: La autoridad canónica reside exclusivamente en `TargetProfile` (JSON Schema Draft 2020-12 validado por `TargetProfileService`).
2. **Adaptador Unidireccional de Compatibilidad (`toLegacyHardwareContract`)**: El runtime solo admite la proyección $TargetProfile \rightarrow HardwareContract$ para alimentar temporalmente a componentes legacy de UI, catálogo y exportación.
3. **Prohibición de `fromLegacyHardwareContract` en Runtime**: Ningún componente de resolución, despacho, preflight, scheduling o persistencia admitirá la inferencia inversa. Los 29 perfiles legacy no migrados se consumen de forma estricta como `NativeLegacyContract`.
4. **Drafts Offline de Migración (no ejecutables)**: Toda futura migración de los 29 perfiles no migrados se realizará fuera de línea mediante borradores explícitos que exigen revisión humana, escritura en JSON canónico y verificación de schema con paridad formal.

---

## 2. Reconciliación de los 11 Consumidores de Producción y Alcance de Migración (Fase E4)

En Fase E1 se inventariaron 11 consumidores de producción en el árbol `src/`. La siguiente tabla clasifica formalmente la relación de cada uno de ellos respecto a `HardwareContractRegistry` y su alcance en la Fase E4:

| # | Componente Consumidor | Archivo Fuente | Clasificación Arquitectónica | Alcance en Fase E4 |
|:---:|---|---|---|---|
| **1** | `HardwareContractRegistry` | `src/core/HardwareContractRegistry.h` | **Contenedor Directo** | **Objetivo Central de E4**: Carga atómica de perfiles canónicos adaptados y resolución con procedencia tipada. |
| **2** | `HardwareManager` | `src/core/HardwareManager.h` | **Consumidor Directo** | Consumo transparente de `HardwareContractRegistry` con precedencia canónica adaptada. |
| **3** | `SoundIdHardwareCatalogSelector` | `src/gui/soundid/SoundIdHardwareCatalogSelector.h` | **Consumidor Directo** | Consumo transparente de perfiles vía `getContracts()`. |
| **4** | `DrawerHardwareTab` | `src/gui/drawers/DrawerHardwareTab.h` | **Consumidor Directo** | Consumo transparente de catálogo de hardware vía `registry`. |
| **5** | `ModulationPresetExporter` | `src/export/ModulationPresetExporter.h` | **Consumidor Directo** | Consulta transparente de controles vía `findContractById()`. |
| **6** | `AutoTestPresetEngine` | `src/core/AutoTestPresetEngine.h` | **Consumidor Directo** | Consulta transparente de recetas y presets de test vía `registry`. |
| **7** | `MidiIdentityDetector` & `MidiDeviceHotplugMonitor` | `src/hardware/MidiIdentityDetector.h` | **Consumidor Directo** | Consulta transparente de `midiIdentification` vía `registry`. |
| **8** | `MainContentComponent` | `src/gui/MainContentComponent.h` | **Consumidor Indirecto** | Acceso a través de `HardwareManager::getContractRegistry()`. Sin cambios de API. |
| **9** | `HardwareDeviceDisplayCardComponent` | `src/gui/HardwareDeviceDisplayCardComponent.h` | **Consumidor Indirecto** | Recibe `HardwareContract` ya resuelto desde `DrawerHardwareTab`. Sin cambios. |
| **10** | `SlideInDrawer` | `src/gui/SlideInDrawer.h` | **Consumidor Indirecto** | Contenedor GUI de tabs de hardware. Sin cambios. |
| **11** | `PluginHardwareContractAdapter` | `src/core/plugins/PluginHardwareContractAdapter.h` | **Consumidor en Deprecación** | Fuera del alcance de E4 (marcado para retiro en hitos de plugins; el hosting canónico de plugins ya usa `TargetContractDiscovery`). |

> [!NOTE]
> Los consumidores de test (`test_HardwareContractRegistry.cpp`, `test_Integration_SmokeTest_SessionWorkflow.cpp`, etc.) validan contratos de compatibilidad y mantienen sus aserciones intactas en el baseline.

---

## 3. Precedencia Formal de Resolución y Detección de Colisiones

El algoritmo formal de resolución para `lookup(id)` en el runtime (a implementar en Fase E4) sigue este árbol de decisión estricto:

```text
lookup(targetId)
│
├─ 1. ¿Existe TargetProfile canónico certificado en TargetProfileService?
│  │
│  ├─ NO:
│  │  ├─ ¿Existe contrato en el repositorio legacy (contracts/hardware/)?
│  │  │  ├─ SÍ -> Retornar contrato legacy nativo.
│  │  │  │        Diagnóstico: NativeLegacyContract
│  │  │  └─ NO -> Retornar error de objetivo no encontrado.
│  │  │           Diagnóstico: NotFound
│  │
│  └─ SÍ:
│     ├─ ¿Existe colisión de ID en el repositorio legacy (contracts/hardware/)?
│     │  │
│     │  ├─ SÍ y el perfil figura en la "Lista de Paridad Certificada" (Pro-800, DX7, DS-1):
│     │  │  └─ Proyectar TargetProfile canónico -> HardwareContract vía toLegacyHardwareContract().
│     │  │     Diagnóstico: CanonicalTargetProfileAdapted
│     │  │
│     │  ├─ SÍ y NO figura en la lista de paridad certificada:
│     │  │  └─ FALLO EXPLÍCITO (Fail-Closed). Prohibido operar bajo ambigüedad.
│     │  │     Diagnóstico: CanonicalLegacyParityViolation (ERR_CANONICAL_LEGACY_PARITY_UNPROVEN)
│     │  │
│     │  └─ NO (solo existe el canónico):
│     │     └─ Proyectar TargetProfile canónico -> HardwareContract vía toLegacyHardwareContract().
│     │        Diagnóstico: CanonicalTargetProfileAdapted
```

### Política de Colisión y Resolución de IDs
1. **Identificadores Canónicos**: Siguen el estándar `hw-<vendor>-<model>-canonical` (ej. `hw-behringer-pro800-canonical`, `hw-yamaha-dx7-canonical`, `hw-boss-ds1-canonical`).
2. **Identificadores Legacy**: Siguen el estándar en snake_case (ej. `behringer_pro800`, `yamaha_dx7`, `boss_ds1_distortion`).
3. **Mapeo de Alias (`acceptedUniqueIds`)**: `TargetProfile.identity.acceptedUniqueIds` incluye los identificadores legacy históricos. La búsqueda resolverá por alias al perfil canónico con precedencia absoluta, emitiendo el diagnóstico `CanonicalTargetProfileAdapted`.

---

## 4. Tabla Campo-a-Campo: TargetProfile Canónico $\rightarrow$ HardwareContract Legacy

| Campo `TargetProfile` (Canónico) | Campo `HardwareContract` (Legacy) | Tipo de Transformación | Pérdida de Información / Notas de Compatibilidad |
|---|---|---|---|
| `schemaVersion: "1.0"` | `schemaVersion: "2.0"` | Asignación fija | El schema canónico v1.0 se proyecta a la especificación legacy v2.0. |
| `targetProfileId` | `id` | Copia directa | Preserva el ID canónico (o alias resuelto). |
| `displayName` | `displayName`, `model` | Copia directa | Nombre descriptivo del dispositivo. |
| `vendor` | `manufacturer`, `brand` | Copia directa | Marca/fabricante del dispositivo. |
| `targetKind == "SoftwarePlugin"` | `deviceType = "SOFTWARE_PLUGIN"` | Mapeo condicional | Identifica plugins virtuales. |
| `targetKind != "SoftwarePlugin"` | Determinado por transporte (`AUTOMATED_MIDI_CC`, `AUTOMATED_SYSEX`, `ANALOGUE_PEDAL`) | Inferencia de control | Proyecta la categoría de hardware según los identificadores técnicos presentes. |
| `parameters[i].semanticId` | `functions[0].controls[i].name` | Copia | Identificador semántico proyectado al nombre del control. |
| `parameters[i].displayName` | `functions[0].controls[i].name` | Fallback | Si `displayName` está presente, prevalece sobre `semanticId`. |
| `parameters[i].normalizedRange` | `minVal = range.first`, `maxVal = range.second` | Cast `double` $\rightarrow$ `float` | Conversión de precisión para DTO legacy. |
| `parameters[i].technicalIdentifier (MidiCc)` | `controlMethod = "MIDI_CC"`, `type = "MidiCC"`, `ccNumber` | Proyección de unión | Se conservan número de CC y método de control. Se pierde la comprobación de canal estricta legacy. |
| `parameters[i].technicalIdentifier (MidiSysEx)` | `controlMethod = "SYSEX"`, `type = "Knob"`, `sysexAddress` | Proyección de unión | La plantilla de mensaje SysEx se asigna a `sysexAddress`. |
| `parameters[i].technicalIdentifier (ManualOperator)` | `controlMethod = "MANUAL"`, `type = widget` | Proyección de unión | Tipo de widget (Knob, Slider, Switch). |
| `parameters[i].technicalIdentifier (Vst3Parameter)` | `controlMethod = "MANUAL"`, `type = "Normalized"`, `ccNumber = index` | Proyección de unión | Asigna índice de parámetro de plugin. |

---

## 5. Campos No Representables en Legacy (Riesgos del Adaptador Inverso)

Esta tabla documenta taxativamente por qué la función inversa `fromLegacyHardwareContract` **no es segura ni admisible en el runtime**:

| Dominio Canónico `TargetProfile` | Representabilidad en Legacy `HardwareContract` | Severidad | Riesgo Metrológico al Intentar Reconstrucción Inversa |
|---|:---:|:---:|---|
| **`TransportPolicy`** (Pacing, Delays, Retries, ACK) | **NULA** | **CRÍTICA** | En legacy no existen `minimumInterMessageDelayMs`, `maxRetries`, `responseTimeoutMs` ni `requiresResponseAck`. Una reconstrucción inventaría valores que pueden saturar buffers físicos o causar dropouts MIDI. |
| **`Preflight 5-States`** (Identidad Binaria y Hardware) | **NULA** | **CRÍTICA** | Legacy carece de la máquina de estados de preflight (Declared, Verified, UserConfirmed, Failed, Bypassed). La reconstrucción degradaría la garantía de identidad a una asunción ciega. |
| **Consentimiento del Operador y Hashes Anti-TOCTOU** | **NULA** | **CRÍTICA** | Legacy no almacena hashes SHA-256 de parámetros ni digests de tramas. Imposible garantizar la prevención de TOCTOU en pedales o hardware manual. |
| **Evidencia Forense y Fixity Binaria** | **NULA** | **CRÍTICA** | `TargetProfile` exige fixity binaria (SHA-256 de ROMs en emulaciones, manifests criptográficos). Legacy no modela esta evidencia. |
| **Políticas de Calibración y Medición** | **PARCIAL** | **ALTA** | `warmupTimeMs`, `defaultSettlingTimeMs` y `recommendedCalibrationPolicy` no tienen representación homogénea en legacy. |
| **Semántica Fina de SysEx (Checksum, Framing)** | **PARCIAL** | **ALTA** | Legacy solo tiene un string `sysexAddress` informal sin definición de checksum policy ni endianness de variables nibblizadas. |

---

## 6. Perfiles con Paridad Formal Certificada y Retirados en E6 (3 Perfiles)

Los siguientes 3 perfiles cuentan con paridad estructural, de ejecución y de consumidores directos demostrada en HITO-10D1, HITO-10E3, E5 y E5.1:

1. **Behringer PRO-800**
   - Canónico: `profiles/targets/behringer_pro800.target.json`
   - Legacy: `contracts/hardware/behringer_pro800.json` (Retirado en E6)
   - Transporte: MIDI CC automatizado (CC 19, CC 20, etc.).
   - Estado: `Retired (E6)` (Retirado físicamente del sistema de archivos con compatibilidad histórica por alias preservada).
2. **Yamaha DX7 (Mark I)**
   - Canónico: `profiles/targets/yamaha_dx7.target.json`
   - Legacy: `contracts/hardware/yamaha_dx7.json` (Retirado en E6)
   - Transporte: MIDI SysEx automatizado con plantilla estructurada.
   - Estado: `Retired (E6)` (Retirado físicamente del sistema de archivos con compatibilidad histórica por alias preservada).
3. **BOSS DS-1 Distortion**
   - Canónico: `profiles/targets/boss_ds1_distortion.target.json`
   - Legacy: `contracts/hardware/boss_ds1_distortion.json` (Retirado en E6)
   - Transporte: Operador Manual con prompts de confirmación guiada.
   - Estado: `Retired (E6)` (Retirado físicamente del sistema de archivos con compatibilidad histórica por alias preservada).

---

## 7. Perfiles Que NO Pueden Entrar en Ruta Canónica (28 Perfiles Legacy Activos)

De los 28 documentos de perfil de hardware activos en `contracts/hardware/`, todos permanecen en la ruta canónica exclusiva de `NativeLegacyContract`:
- `casio_cz101.json` (Hardware físico legacy; NO confundir con VES emulado cuyo HITO-10V1 está bloqueado).
- `roland_juno106.json`, `roland_juno60.json`, `behringer_deepmind12.json`, `korg_ms2000.json`, `korg_microkorg.json`, `roland_aira_bitrazer.json`, `manual_eurorack_vcf.json`, etc. (Total: 28 perfiles).

**Fórmulas Aritméticas de Inventario:**
$$\text{legacyProfileDocumentCount (31)} = \text{migratedLegacyProfileCount (3)} + \text{unmigratedLegacyProfileCount (28)}$$
$$\text{legacyFilesystemEntryCount (32)} = \text{legacyProfileDocumentCount (31)} + \text{legacySchemaDocumentCount (1)}$$

**Dictamen Vinculante:**
- Permanecen clasificados como **`Keep / Critical Risk`** en el inventario.
- Se cargan exclusivamente a través de la ruta `NativeLegacyContract`.
- Queda prohibida cualquier migración automática implícita.

---

## 8. Diagnósticos Estables del Sistema de Perfiles

El sistema de resolución y carga emitirá obligatoriamente los siguientes códigos y diagnósticos tipados:

| Código de Diagnóstico | Significado Arquitectónico | Severidad | Acción del Sistema |
|---|---|:---:|---|
| **`CanonicalTargetProfileAdapted`** | Perfil canónico certificado resuelto y proyectado hacia vista legacy para consumidor compatible. | Info | Ejecución autorizada en todos los subsistemas. |
| **`NativeLegacyContract`** | Contrato legacy nativo (uno de los 29 no migrados) cargado directamente desde disco sin transformación. | Warning | Ejecución autorizada bajo el motor legacy. |
| **`CanonicalLegacyParityViolation`** | Existe colisión entre un perfil canónico y un contrato legacy no homologados en la lista de paridad. | **Error Fatal** | Bloqueo preventivo inmediato (Fail-Closed). |
| **`CanonicalProfileInvalid`** | Archivo `.target.json` con error de schema o validación de semántica canónica. | **Error Fatal** | Rechazo de carga. |
| **`LegacyProfileInvalid`** | Archivo `.json` legacy con error de parsing o corrupción sintáctica. | Error | Advertencia en log y omisión del perfil. |
| **`NotFound`** | El identificador solicitado no existe en el catálogo canónico ni en el repositorio legacy. | Error | Notificación de target desconocido. |
