# INVENTARIO Y MAPA DE FRONTERA: HITO-SHARED-SYNC (SS1)
## Sincronización Arquitectónica y Transferencia de Seguridad de Hardware

**Fecha**: 2026-09-28  
**Proyecto**: ABDAudioLab / ABDSharedCode  
**Responsable de Arquitectura**: Antigravity (Lead Architect & Planner)  
**Estado**: 🟡 **FASE SS1 (INVENTARIO Y LÍNEA DE BASE) COMPLETADA**  
**Principio Rector de Separación de Dominios**:  
> *«ABDSharedCode puede descubrir, enumerar, filtrar y presentar endpoints de hardware; ABDAudioLab conserva la autoridad exclusiva para consentir, despachar, registrar evidencia, bloquear exportación y gobernar sesiones metrológicas.»*

---

## 1. Delimitación Estricta de Frontera y Responsabilidades

| Dominio | Capa / Módulo | Responsabilidades Permitidas | Componentes Clave | Prohibiciones Taxativas |
|---|---|---|---|---|
| **Capa Compartida** | `ABDSharedCode` (`HardwareMidiDetect`, `HardwareDrivers`) | • Descubrimiento de puertos y hotplug.<br>• Apertura tipada de endpoints sin fallback.<br>• Clasificación física vs virtual.<br>• UI embebida / modal (JuceHardwareMidiPicker).<br>• Detección pasiva o bajo demanda explícita. | `JuceMidiHardwareBackend`, `HardwareMidiDetector`, `HardwareMidiHotplugMonitor`, `JuceHardwareMidiPicker`, `HardwareContractRegistry` | ⛔ Cero despacho automático.<br>⛔ Cero consentimiento u operador.<br>⛔ Cero autoridades metrológicas.<br>⛔ Cero fallback a device índice 0.<br>⛔ Cero SysEx broadcast automático indiscriminado. |
| **Capa de Aplicación / Laboratorio** | `ABDAudioLab` (`src/hardware/`, `src/core/`, `src/profiling/`) | • Preflight de sesión metrológica.<br>• Consentimiento de operador criptográfico y anti-TOCTOU.<br>• Scheduler, pacing y rate limiting.<br>• Despacho físico autorizado y evidencia forense.<br>• Recuperación fail-closed y bloqueo incondicional de `ExportReadiness`. | `HardwareTransportPreflightService`, `OperatorConsentService`, `HardwareDispatchScheduler`, `JuceMidiTransport`, `HardwareDispatchEvidenceRecord` | ⛔ No delegar en la capa compartida la autorización de despacho ni la gobernanza de sesión metrológica. |

---

## 2. Mapa Detallado de APIs y Componentes Compartidos

A continuación se detalla la matriz de componentes de hardware compartidos en `ABDSharedCode` y sus puentes en `ABDAudioLab`:

| Componente | Archivos / Símbolos | Responsabilidad Actual | Consumidores Reales | Nivel de Riesgo | Lección D2.7A Transferible | Cambio Propuesto en HITO-SHARED-SYNC | Compatibilidad | Test Requerido |
|---|---|---|---|:---:|---|---|---|---|
| **`JuceMidiHardwareBackend`** | `HardwareMidiDetect/JuceMidiHardwareBackend.h/.cpp`<br>`src/hardware/AudioLabMidiBackend.h` | Apertura de endpoints JUCE y puente con WebView2 / UI Picker. | • `DrawerHardwareTab`<br>• `JuceHardwareMidiPicker`<br>• Pruebas de UI | 🚨 **CRÍTICO** | Fallback silencioso de JUCE a device 0 ante identificadores o nombres ausentes. | **SS2**: Validación estricta con `MidiEndpointOpenResult` tipado. Cero fallback a índice 0. | **100% Compatible** (API enriquecida / aditiva). | Endpoint inexistente, id vacío, endpoint real exacto. |
| **`HardwareMidiDetector`** | `HardwareMidiDetect/HardwareMidiDetector.h/.cpp`<br>`src/hardware/MidiIdentityDetector.h/.cpp` | Escaneo de puertos y consulta SysEx Universal Identity Request. | • `DrawerHardwareTab`<br>• `MidiIdentityDetector`<br>• `HardwareManager` | ⚠️ **ALTO** | SysEx broadcast automático (0x7F) en todos los puertos puede perturbar loops virtuales (LoopBe1, loopMIDI). | **SS3 / SS4**: Exclusión por defecto de puertos virtuales para auto-inquiry; modelo formal de preflight. | **Aditivo** (conserva helpers heurísticos de nombres). | Escaneo con puertos virtuales presentes; exclusión verificada. |
| **`HardwareMidiHotplugMonitor`** | `HardwareMidiDetect/HardwareMidiHotplugMonitor.h`<br>`src/hardware/MidiDeviceHotplugMonitor.h` | Monitorización reactiva de inserción/extracción USB MIDI. | • `DrawerHardwareTab`<br>• `HardwareManager` | 🟡 **MEDIO** | Re-escaneo debe usar la misma política de exclusión virtual y apertura segura. | **SS4**: Integración con `MidiEndpointSafetyPolicy`. | **100% Compatible**. | Conexión/desconexión simulada en caliente. |
| **`JuceHardwareMidiPicker`** | `HardwareMidiDetect/JuceHardwareMidiPicker.h`<br>`HardwareMidiPickerResourceProvider` | Componente visual WebView2 para selección de hardware. | • `DrawerHardwareTab` (modal de selección) | 🟢 **BAJO** | Debe reflejar si un puerto es virtual o físico y advertir al usuario. | **SS4**: Presentación de etiquetas (`[Virtual]`, `[USB]`). | **100% Compatible**. | Smoke test de renderizado y callback de selección. |
| **`HardwareContractRegistry`** | `HardwareMidiDetect/HardwareContractRegistry.h/.cpp`<br>`src/core/SharedHardwareContractAdapter.h` | Carga de especificaciones JSON legacy de hardware. | • `HardwareManager`<br>• `SharedHardwareContractAdapter` | 🟡 **MEDIO** | HITO-10E ya eliminó contratos legacy migrados (`pro800`, `dx7`, `ds1`). La capa compartida debe seguir resolviendo solo metadatos de discovery. | **SS1**: Mantener limpio y desacoplado de `TargetProfile` v1.0. | **Conservada**. | Carga de directorio de contratos; ausencia de duplicados. |

---

## 3. Estado de la Línea de Base (Baseline de Pruebas)

- **ABDAudioLab Test Suite**:
  - Compilador: MSVC v18.4 (Visual Studio 2026), C++20, Catch2 v3.5.2.
  - Baseline Global Consolidada: **754 test cases, 269.224 assertions PASS (0 FAIL, 8 SKIPPED)**.
  - Banco Físico D2.7A: **12 test cases, 132/132 assertions PASS**.
  - Pruebas Específicas de MIDI / Identity / Scheduler:
    - `test_MidiIdentityDetector.cpp`
    - `test_TargetProfilePhysicalPreflightBench.cpp`
    - `test_TargetProfileHardwareScheduler.cpp`
    - `test_TargetProfileTransportFailureRecovery.cpp`

---

## 4. Diseño del Contrato de Apertura Tipada y Política de Seguridad (SS2, SS3, SS4)

### 4.1 Contrato de Apertura de Endpoints (`MidiEndpointOpenResult`)
```cpp
namespace abd::hwid
{

enum class MidiEndpointOpenOutcome
{
    Opened = 0,
    RequestedEndpointNotFound,
    RequestedEndpointAmbiguous,
    RequestedEndpointUnavailable,
    BackendOpenFailed,
    VirtualEndpointRejected,
    InvalidSelection
};

struct MidiEndpointOpenResult
{
    MidiEndpointOpenOutcome outcome { MidiEndpointOpenOutcome::InvalidSelection };
    std::string requestedStableDeviceId;
    std::string requestedDisplayName;
    std::string resolvedStableDeviceId;
    std::string diagnosticCode;
    std::string diagnosticMessage;

    [[nodiscard]] bool isSuccess() const noexcept { return outcome == MidiEndpointOpenOutcome::Opened; }
};

} // namespace abd::hwid
```
**Regla Inquebrantable**: `resolvedStableDeviceId` **únicamente** contendrá valor si coincide exactamente con `requestedStableDeviceId` o con el `identifier` de la coincidencia exacta. **Jamás se resolverá el dispositivo de índice 0 como fallback.**

### 4.2 Modelo de 5 Estados de Identidad para Descubrimiento (`HardwareMidiIdentityState`)
```cpp
namespace abd::hwid
{

enum class HardwareMidiIdentityState
{
    PortAvailable = 0,        // Puerto físico detectado y abierto; sin comprobación de identidad
    IdentityVerified,         // Identidad confirmada mediante SysEx Identity Reply coincidente
    IdentityUnavailable,      // No soporta inquiry o no respondió en tiempo
    IdentityMismatch,         // Respondió con fabricante/modelo diferente al esperado
    UserConfirmedUnverified   // Estado reservado para confirmación explícita por la capa superior
};

} // namespace abd::hwid
```

### 4.3 Clasificación de Endpoints y Política de Seguridad (`MidiEndpointSafetyPolicy`)
```cpp
namespace abd::hwid
{

enum class MidiEndpointKind
{
    PhysicalUsb = 0,
    PhysicalDinInterface,
    VirtualLoopback,
    VirtualDriver,
    Unknown
};

struct MidiEndpointSafetyPolicy
{
    bool allowVirtualEndpointsForManualRouting { true };
    bool allowVirtualEndpointsForAutomaticDiscovery { false };
    bool allowBroadcastSysEx { false };
    bool requireSingleTargetTopologyForBroadcast { true };
};

} // namespace abd::hwid
```

---

## 5. Plan de Ejecución de Fases SS2 a SS6

1. **Fase SS2 (Validación Estricta de Endpoint MIDI)**:
   - Implementar `MidiEndpointOpenResult` y `MidiEndpointOpenOutcome` en `ABDSharedCode/HardwareMidiDetect/JuceMidiHardwareBackend`.
   - Incorporar validación previa exhaustiva contra `juce::MidiOutput::getAvailableDevices()` evitando invocaciones ciegas a `openDevice(int)`.
   - Crear suite de tests unitarios de apertura estricta.

2. **Fase SS3 (Modelo de Preflight de Cinco Estados)**:
   - Añadir `HardwareMidiIdentityState` a `HardwareMidiDetect`.
   - Adaptar `HardwareMidiDetector::DiscoveredDevice` para exponer el estado formal sin alterar la API legacy `isSysExVerified`.

3. **Fase SS4 (Política de Puertos Virtuales y SysEx Broadcast)**:
   - Implementar clasificación heurística y regex de nombres virtuales (`LoopBe`, `loopMIDI`, `teVirtualMIDI`, `MIDI 2.0`).
   - Agregar `MidiEndpointSafetyPolicy` en `HardwareMidiDetector::DetectionConfig`.
   - Silenciar el broadcast automático (0x7F) en puertos virtuales por defecto.

4. **Fase SS5 (Contrato de Integración con ABDAudioLab)**:
   - Conectar `JuceMidiTransport` y `HardwareTransportPreflightService` con el nuevo contrato de `ABDSharedCode`.
   - Asegurar que la suite global de 754 test cases permanezca 100% verde.

5. **Fase SS6 (Certificación y Cierre)**:
   - Emitir `ACTA_HITO_SHARED_SYNC.md`.
   - Ejecución de baseline completa sin regresiones.

---

## 6. Certificación de Fase SS2 y SS2.1 (Validación Estricta de Endpoint y Ownership)

- **Estado**: ✅ **COMPLETADO Y CERTIFICADO (SS2 + SS2.1)** (Build #506 — 12 test cases, 79/79 assertions PASS).
- **Contratos Implementados**:
  - `MidiEndpointTypes.h` (`MidiEndpointOpenOutcome`, `MidiEndpointResolutionMethod`, `MidiEndpointSelectionRequest`, `MidiEndpointOpenResult`, `IMidiDeviceProvider`, `DefaultJuceMidiDeviceProvider`).
- **Comportamiento Verificado en `ABDSharedCode/HardwareMidiDetect/JuceMidiHardwareBackend`**:
  - **Precedencia estricta**: `StableDeviceId` exacto → `DisplayName` exacto (solo si el ID está vacío).
  - **Rechazo de contradicciones**: `ERR_MIDI_ENDPOINT_INVALID_SELECTION` si nombre e ID no concuerdan.
  - **Detección de ambigüedad**: `ERR_MIDI_ENDPOINT_AMBIGUOUS` ante colisiones en `DisplayName`.
  - **Fail-Closed Total**: ID o nombre inexistente devuelve `ERR_MIDI_ENDPOINT_NOT_FOUND` y **NUNCA abre el endpoint de índice 0 ni emite tráfico MIDI**.
  - **Eliminación de fallbacks inseguros**: Eliminado de raíz el fallback histórico a `outputs[0]` / `inputs[0]`.
  - **Ownership y Lifecycle Explícito (Modelo B - Backend Propietario)**:
    - El backend posee unívocamente el handle abierto (`isOutputOpen()`, `getOpenedOutputIdentifier()`, `getOpenedOutput()`).
    - Cierre explícito mediante `closeOpenedOutput()` y destructor que libera limpiamente sin punteros colgantes.
    - Re-apertura limpia: una nueva llamada a `openStrictOutput()` cierra preventivamente el handle anterior.
    - Fallos de apertura limpian y dejan el handle cerrado (`isOutputOpen() == false`).
  - **Semántica Rigurosa de Outcomes**:
    - `RequestedEndpointUnavailable` (`ERR_MIDI_ENDPOINT_UNAVAILABLE`): probado mediante `isEndpointAvailable()`, detectando endpoints enumerados pero bloqueados/no disponibles antes de abrir y con **0 llamadas a `openExactOutput()`**.
    - `BackendOpenFailed` (`ERR_MIDI_ENDPOINT_OPEN_FAILED`): probado ante fallo directo del driver en `openExactOutput()`.
  - **Política de Endpoints Virtuales**:
    - `VirtualEndpointRejected` reservado formalmente para SS4 (`MidiEndpointSafetyPolicy`).
    - En SS2.1, endpoints virtuales (e.g., loopMIDI, LoopBe1) pueden abrirse de forma manual legítima sin rechazo artificial mientras no haya política prohibitiva activa.
- **Batería de Pruebas Herméticas (`ABDSharedCode/tests/test_JuceMidiHardwareBackendStrictOpen.cpp`)**:
  - Casos 1 a 8: Precedencia, negativos de ID, negativos de DisplayName, selección vacía, ambigüedad, contradicción, backend failure.
  - Caso 9: *Successful Open Owns Exactly Resolved Endpoint* (índice != 0, 1 sola llamada, pertenencia estricta, sin colaterales, ciclo de vida cerrado).
  - Caso 10: *RequestedEndpointUnavailable Differentiated from BackendOpenFailed* (fallo pre-open, 0 llamadas a openDevice).
  - Caso 11: *Re-opening Closes Previous Endpoint and Failures Leave Handle Closed*.
  - Caso 12: *Virtual Endpoint Allowed in SS2 without Safety Policy (Reserved for SS4)*.

---

## 7. Certificación de Fase SS3 (Modelo de Preflight de Cinco Estados)

- **Estado**: ✅ **COMPLETADO Y CERTIFICADO** (Build #507 — 12 test cases, 57/57 assertions PASS).
- **Contratos Implementados**:
  - `HardwareContract.h`:
    - `enum class HardwareMidiIdentityState` (`PortAvailable`, `IdentityVerified`, `IdentityUnavailable`, `IdentityMismatch`, `UserConfirmedUnverified`).
    - Helper centralizado: `constexpr bool toLegacyIsSysExVerified(HardwareMidiIdentityState state) noexcept`.
    - Invariante ejecutable: `constexpr bool isSharedDiscoveryEmittable(HardwareMidiIdentityState state) noexcept` (retorna `false` exclusivamente para `UserConfirmedUnverified`).
    - Factory fuerte: `class SharedDiscoveryIdentityResult` con constructores privados que imposibilita instanciar `UserConfirmedUnverified` dentro de `ABDSharedCode`.
  - `HardwareMidiDetector.h`:
    - Campo aditivo `HardwareMidiIdentityState identityState` en `DiscoveredDevice` inicializado en `PortAvailable`.
    - Preservado `isSysExVerified` como API legacy sincronizada.
    - Método clasificador puro: `SharedDiscoveryIdentityResult classifyIdentityReply(...)`.
- **Frontera de Responsabilidad y Cero Tráfico Adicional**:
  - **Invariante de Emisión**: `ABDSharedCode` jamás construye, asigna, retorna ni persiste `UserConfirmedUnverified`.
  - **Invariante de Tráfico**: Cero llamadas nuevas a MIDI output; `buildDetectionQueries` devuelve exactamente la misma lista de consultas previa a SS3 (0 nuevos inquiries, 0 broadcast adicional, 0 bytes de tráfico extra).
- **Batería de Pruebas Herméticas (`ABDSharedCode/tests/test_HardwareMidiDetectorIdentityState.cpp`)**:
  - Caso 1: Endpoint enumerado sin inquiry ➔ `PortAvailable`, `isSysExVerified == false`.
  - Caso 2: Inquiry no solicitada ➔ `IdentityUnavailable`, `isSysExVerified == false`.
  - Caso 3: Inquiry sin respuesta ➔ `IdentityUnavailable`, `isSysExVerified == false`.
  - Caso 4: Inquiry no soportada / mensaje truncado ➔ `IdentityUnavailable`, `isSysExVerified == false`.
  - Caso 5: Identity Reply coincidente ➔ `IdentityVerified`, `isSysExVerified == true`.
  - Caso 6: Identity Reply contradictoria (mismatch) ➔ `IdentityMismatch`, `isSysExVerified == false`.
  - Caso 7: Shared discovery no puede emitir `UserConfirmedUnverified` (`isSharedDiscoveryEmittable == false`).
  - Caso 8: Factory `SharedDiscoveryIdentityResult` imposibilita instanciar `UserConfirmedUnverified`.
  - Caso 9: Invariante de tráfico (cero llamadas nuevas a MIDI output).
  - Caso 10: Invariante de consultas (`buildDetectionQueries` idéntico, 0 bytes nuevos).
  - Caso 11: Consumidor legacy (`toLegacyIsSysExVerified`) conserva la semántica histórica binaria.
  - Caso 12: Consumidor nuevo distingue `IdentityUnavailable` de `IdentityMismatch`.

---

## 8. Certificación de Fase SS4-Core (Política de Puertos Virtuales y SysEx Broadcast)

- **Estado**: ✅ **COMPLETADO Y CERTIFICADO** (Build #508 — 20 test cases, 27/27 assertions PASS).
- **Axioma Verificado**: *"Un puerto que puede seleccionarse manualmente no queda por ello autorizado para recibir interrogación automática; el discovery automático debe ser más restrictivo que el routing manual."*
- **Contratos Implementados**:
  - `HardwareMidiDetect/MidiEndpointSafetyPolicy.h` y `.cpp`:
    - `enum class MidiEndpointKind`: `PhysicalUsb`, `PhysicalDinInterface`, `VirtualLoopback`, `VirtualDriver`, `Unknown`.
    - `enum class SysExDiscoveryInquiryMode`: `Disabled` (default), `ManualExplicitOnly`, `PhysicalEndpointsWithExplicitOptIn`.
    - `struct MidiEndpointSafetyPolicy`:
      - `allowVirtualEndpointsForManualRouting = true` (permite enrutamiento manual sin ocultar endpoints legítimos).
      - `allowVirtualEndpointsForAutomaticDiscovery = false` (excluye puertos virtuales de sweeps automáticos).
      - `allowBroadcastSysEx = false` (bloqueo por defecto de broadcast).
      - `sysExInquiryMode = SysExDiscoveryInquiryMode::Disabled`.
      - `requireSingleTargetTopologyForBroadcast = true`.
    - `struct BroadcastInquiryAuthorization`: confirmación explícita del llamador (`callerExplicitlyOptedIn`, `callerConfirmedSingleTargetTopology`).
    - `enum class SysExInquiryDecision`: `Allowed`, `DisabledByPolicy`, `VirtualEndpointExcluded`, `UnknownEndpointExcluded`, `SingleTargetTopologyUnconfirmed`, `CallerOptInMissing`.
    - `class DefaultMidiEndpointClassifier`: clasificación conservadora con default fail-closed a `Unknown` (nunca asume `PhysicalUsb` ante la duda).
    - `evaluateUniversalInquiryEligibility`: función pura con **0 aperturas de puerto, 0 mensajes SysEx y 0 bytes transmitidos**.
    - `getEndpointKindLabel`: etiquetado para UI (`[USB]`, `[DIN]`, `[Virtual]`, `[Unknown]`).
- **Batería de Pruebas Herméticas (`ABDSharedCode/tests/test_MidiEndpointSafetyPolicy.cpp`)**:
  - Casos 1 a 6 (Clasificación): LoopBe1 (`VirtualLoopback`), loopMIDI (`VirtualDriver`), teVirtualMIDI (`VirtualDriver`), USB device (`PhysicalUsb`), DIN interface (`PhysicalDinInterface`), no clasificable (`Unknown`).
  - Casos 7 a 9 (Routing Manual): virtual permitido por defecto, rechazado solo si policy lo explicita, evaluación pura sin side-effects.
  - Casos 10 a 14 (Discovery Automático): LoopBe1 y loopMIDI excluidos de auto-discovery, Unknown excluido de broadcast inquiry, PhysicalUsb candidato sin inquiry si mode=Disabled, PhysicalUsb con opt-in y topología confirmada devuelve `Allowed`.
  - Casos 15 a 20 (Fronteras de Broadcast): `allowBroadcastSysEx=false` bloquea globalmente, caller opt-in ausente produce `CallerOptInMissing`, topología no confirmada produce `SingleTargetTopologyUnconfirmed`, puertos virtuales producen `VirtualEndpointExcluded`, puertos desconocidos producen `UnknownEndpointExcluded`, evaluación con 0 I/O y 0 allocations.

---

## 9. Certificación de Fase SS4.1 (Policy Wiring, Hotplug Propagation and Safe UI Presentation)

- **Estado**: ✅ **COMPLETADO Y CERTIFICADO** (Build #509 — 20 test cases, 43/43 assertions PASS).
- **Axioma Crítico de Emisión**: *"Allowed de Policy ≠ Emisión física. La capa compartida jamás emite SysEx por sí sola durante `scanAllPorts()`, `evaluateLists()`, hotplug, UI o selección. Su responsabilidad concluye al evaluar la elegibilidad tipada y, si procede, construir una intención tipada (`UniversalInquiryRequest`) que devuelve al llamador. Solo la capa superior con autoridad de sesión puede decidir si ejecuta o transmite."*
- **Integración Efectiva en Runtime Compartido**:
  - **`HardwareMidiDetector::DetectionConfig`**:
    - `endpointSafetyPolicy`: configuración de seguridad por defecto.
    - `performIdentityInquiry = false` (default seguro: no construye ni envía inquiries automáticamente).
    - `inquiryAuthorization`: afirmación explícita del llamador.
    - `buildDetectionQueries`: sobrecarga policy-aware que evalúa `evaluateUniversalInquiryEligibility` y retorna lista vacía si la decisión no es `Allowed`.
    - `scanAllPorts`: filtra y excluye endpoints virtuales de sweeps automáticos cuando `allowVirtualEndpointsForAutomaticDiscovery == false`.
  - **`DiscoveredDevice`**: enriquecido con `endpointKind` y `kindLabel`.
  - **`HardwareMidiHotplugMonitor`**:
    - Clasifica puertos hotplug mediante `DefaultMidiEndpointClassifier`.
    - Rellena `dev.endpointKind` y `dev.kindLabel` (`[USB]`, `[DIN]`, `[Virtual]`, `[Unknown]`).
    - **Invariante**: Notifica exclusivamente el evento; **cero aperturas de puerto y cero consultas SysEx automáticas**.
  - **`JuceHardwareMidiPicker`**:
    - Propaga `endpointKind` y `kindLabel` a la WebUI en `pushDevicesToWebUI`.
    - Mantiene endpoints virtuales visibles para selección manual explícita.
    - Si `allowVirtualEndpointsForManualRouting == false`, rechaza explícitamente sin redirigir ni ocultar silenciosamente.
    - La selección manual no activa `performIdentityInquiry`.
- **Batería de Pruebas Herméticas (`ABDSharedCode/tests/test_MidiEndpointSafetyPolicyWiring.cpp`)**:
  - Casos 1 al 6 (Detector): configuración default sin inquiries; LoopBe1, loopMIDI y Unknown excluidos de auto-discovery e inquiries; PhysicalUsb en modo Disabled con 0 inquiries; PhysicalUsb con opt-in y topología confirmada produce `Allowed` sin emitir I/O.
  - Casos 7 al 9 (Hotplug): hotplug virtual publica descriptor con `[Virtual]` sin inquiries ni aperturas; hotplug físico publica descriptor con `[USB]` sin inquiry automático; recálculo de policy sin abrir puertos.
  - Casos 10 al 15 (UI / Selector): etiquetas visibles `[Virtual]`, `[USB]`, `[DIN]`, `[Unknown]`; selección manual de loopMIDI sin inquiry; rechazo explícito si manual routing está deshabilitado.
  - Casos 16 al 20 (Invariantes transversales): `openStrictOutput` calls = 0; `sendMessageNow` calls = 0; `buildDetectionQueries` sin inquiry no autorizado; cero fallback a índice 0; D2.7A y D2.7B aislados y bloqueados.

---

## 10. Diseño Formal y Congelación de Contrato de Fase SS5 (Integración y Compatibilidad con ABDAudioLab)

- **Estado**: ❄️ **DISEÑO FORMAL CONGELADO (IMPLEMENTACIÓN PAUSADA HASTA APROBACIÓN)**.
- **Axioma Rector de SS5**: *"Un endpoint compartido puede ser elegible para discovery, pero no queda por ello autorizado para una sesión metrológica, para un despacho físico ni para una exportación."*
- **Frontera de Autoridades Inviolable**:
  - **`ABDSharedCode` produce**: Descriptor de endpoint, ID estable, display name, clasificación físico/virtual/desconocido, resultado de apertura tipada sin fallback, estado de identidad de discovery, elegibilidad de inquiry y diagnósticos compartidos.
  - **`ABDAudioLab` consume**: Selección exacta de endpoint, tipo de endpoint, estado de identidad, diagnósticos y policy de exclusión virtual.
  - **`ABDAudioLab` conserva exclusivamente**: Preflight de sesión metrológica, consentimiento del operador, blindaje anti-TOCTOU, scheduler y pacing, apertura/uso del transporte de sesión, evidencia forense (`HardwareDispatchEvidenceRecord`), fail-closed y `ExportReadiness`.

### A. DTOs de Frontera de Solo Lectura

```cpp
namespace abd::hwid
{

/**
 * @struct UniversalInquiryRequest
 * @brief Pure data representation of an inquiry intention without transmission capability.
 */
struct UniversalInquiryRequest
{
    MidiEndpointDescriptor endpoint;
    std::array<uint8_t, 6> bytes { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 };
    MidiEndpointSafetyPolicy policySnapshot;
    BroadcastInquiryAuthorization authorizationSnapshot;
    SysExInquiryDecision eligibilityDecision { SysExInquiryDecision::Allowed };
};

/**
 * @struct UniversalInquiryEligibilityResult
 * @brief Typed result of inquiry eligibility evaluation.
 */
struct UniversalInquiryEligibilityResult
{
    SysExInquiryDecision decision { SysExInquiryDecision::DisabledByPolicy };
    std::optional<UniversalInquiryRequest> request;
};

/**
 * @struct SharedMidiEndpointSnapshot
 * @brief Read-only boundary snapshot consumed by ABDAudioLab from ABDSharedCode.
 */
struct SharedMidiEndpointSnapshot
{
    std::string stableDeviceId;
    std::string displayName;
    MidiEndpointKind endpointKind { MidiEndpointKind::Unknown };
    HardwareMidiIdentityState identityState { HardwareMidiIdentityState::PortAvailable };
    bool isSysExVerified { false };
    bool eligibleForAutomaticDiscovery { false };
    SysExInquiryDecision inquiryDecision { SysExInquiryDecision::DisabledByPolicy };
    std::string diagnosticCode;
    std::string diagnosticMessage;
};

} // namespace abd::hwid
```

### B. Matriz de Mapeo de Identidad y Reglas de Transición Inviolables

| Estado `ABDSharedCode` | Estado Preflight `ABDAudioLab` | Consecuencia Metrológica |
| :--- | :--- | :--- |
| `PortAvailable` | Puerto disponible; identidad aún no determinada | **No permite dispatch**. Cero afirmación metrológica. |
| `IdentityVerified` | Identidad verificada para sesión | **Aún requiere consentimiento del operador y Scheduler**. |
| `IdentityUnavailable` | Identidad no disponible | Puede requerir intervención manual del operador según session policy. **Nunca transiciona automáticamente**. |
| `IdentityMismatch` | `PhysicalIdentityMismatchBlocked` | **Bloqueo fail-closed inmediato**. Cero llamadas al transporte. `ExportReadiness::Blocked`. |
| `UserConfirmedUnverified` | Confirmación formal del operador | **PROHIBIDO originarse en `ABDSharedCode`**. Exclusivo de `ABDAudioLab` tras consentimiento firmado. |

### C. Reglas de Puertos Virtuales y Unknown en Integración Metrológica

| Tipo de Endpoint | UI Manual | Discovery Compartido | Preflight Metrológico ABDAudioLab | SysEx Broadcast |
| :--- | :--- | :--- | :--- | :--- |
| `PhysicalUsb` | Permitido | Permitido según policy | Posible, sujeto a consentimiento y sesión | Solo bajo autorización explícita |
| `PhysicalDinInterface` | Permitido | Permitido según policy | Posible, sujeto a consentimiento y sesión | Solo bajo autorización explícita |
| `VirtualLoopback` | Permitido si policy lo permite | Excluido por defecto | **Rechazado tajantemente para banco físico** | **Estrictamente Prohibido** |
| `VirtualDriver` | Permitido si policy lo permite | Excluido por defecto | **Rechazado tajantemente para banco físico** | **Estrictamente Prohibido** |
| `Unknown` | Visible con advertencia | No elegible para inquiry | **No elegible para preflight físico automático** | **Estrictamente Prohibido** |

*Nota sobre Unknown*: Visible en UI con advertencia. No elegible para broadcast SysEx ni para preflight físico automático sin inspección de operador, policy local explícita y validación adicional fuera de ABDSharedCode. El consentimiento por sí solo no convierte Unknown en endpoint físico confiable.

---

## 11. Certificación de HITO-SHARED-SYNC / SS5 (Contrato de Integración y Compatibilidad Hermética)

### A. Componentes Implementados en ABDAudioLab
1. **Adaptador de Frontera**:
   - `src/hardware/adapter/SharedMidiHardwareAdapter.h`
   - `src/hardware/adapter/SharedMidiHardwareAdapter.cpp`
2. **DTOs y Tipos de Autoridad Local**:
   - `OperatorConfirmationContext`: `{ operatorId, targetContractId, selectedPortStableDeviceId, priorIdentityState, explicitConfirmationRecorded }`.
   - `OperatorConfirmationResult`: fail-closed, exige todos los campos no vacíos, `priorIdentityState == IdentityUnavailable` y confirmación afirmativa explícita.
   - `SharedEndpointInfo`: proyección local de `SharedMidiEndpointSnapshot` sin ceder autoridad.
3. **Invariantes de Frontera Validados**:
   - `SharedMidiHardwareAdapter::isSessionDispatchAllowed()` -> SIEMPRE `false`.
   - `SharedMidiHardwareAdapter::isPhysicalBenchEligible()` -> `false` para virtuales y Unknown.
   - `UniversalInquiryRequest` con constructor privado/friend: existe **si y solo si** `decision == Allowed`.
   - Cero bytes MIDI transmitidos, cero llamadas a `write()`, cero llamadas a `sendMessageNow()`.
   - `ExportReadiness` estrictamente en `Blocked`.

### B. Evidencia de Ejecución de la Suite Hermética (22 Casos)
- **Suite**: `src/tests/test_SharedMidiHardwareIntegrationContract.cpp`
- **Filtro**: `[no_dispatch]`
- **Resultado**:
  ```
  Filters: [no_dispatch]
  All tests passed (87 assertions in 22 test cases)
  ```
- **Filtro global `[shared]`**:
  ```
  Filters: [shared]
  All tests passed (336 assertions in 88 test cases)
  ```

### C. Cuadro de Invariantes Estructurales de SS5
1. **UniversalInquiryRequest Fail-Closed**: Invariante exacto `Allowed <-> request.has_value() == true`. Cero construcción sin evaluación previa aprobada.
2. **Inmutabilidad de Snapshots**: Copias locales mutadas no afectan el descriptor ni el snapshot compartido de origen.
3. **Bloqueo de Elevación de Autoridad**: No existe ruta pública ni API para convertir un snapshot o un policy `Allowed` en despacho de audio en ABDAudioLab.
4. **Hermeticidad Absoluta**: 0 endpoints reales abiertos, 0 hardware DeepMind 12D requerido, 0 bytes físicos.
5. **Aislamiento de Hito Metrológico**: D2.7B continúa **BLOQUEADO**. Autorización física activa: **0 bytes**.


