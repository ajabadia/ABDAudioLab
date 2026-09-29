# INVENTARIO DE BANCO FÍSICO CONTROLADO Y CONTRATO DE SEGURIDAD (HITO-10D2.7A)

**Hito**: HITO-10D2.7A (Banco Físico Controlado — Preflight, Identidad, Consentimiento y Observación Sin Medición Metrológica)  
**Tarea**: D2.7A.1 — Inventario del banco físico controlado y contrato de seguridad de prueba no destructiva  
**Fecha de Apertura**: 2026-09-27  
**Estado del Documento**: `Draft / Pending Operator Verification`  
**Autorización de Transmisión MIDI**: ⛔ **NO CONCEDIDA (0 bytes autorizados)**  
**Responsable de Arquitectura**: Antigravity (Lead Architect & Planner)  
**Operador Autorizado de Banco**: Alejandro Abadía (`ajabadia@gmail.com`)  
**Entorno de Ejecución**: Windows 11 / ABDAudioLab v2.1.0 (Build #495)  

---

## 1. Axioma Rector y Declaración de Propósito

> *«Un cable conectado no convierte una capacidad hipotética en una medición válida; el sistema debe demostrar identidad, consentimiento, trazabilidad y bloqueo seguro antes de poder confiar en cualquier resultado exportable.»*

El objetivo estricto de **HITO-10D2.7A** es verificar que la cadena hermética de despacho físico (D2.1 a D2.6):
- Detecta correctamente los puertos físicos del sistema.
- Diagnostica de forma veraz los cinco estados de preflight e identidad.
- Requiere y valida consentimiento explícito con digest forense antes de emitir cualquier byte.
- Ejecuta como máximo un único despacho mínimo no destructivo con pacing regulado.
- Reacciona de forma determinista y *fail-closed* ante incidencias deliberadas (desconexión, timeout, mismatch).
- **Mantiene rigurosamente bloqueado `ExportReadiness` (0 reportes metrológicos, 0 exportaciones, 0 perfiles generados).**

---

## 2. Contrato de Seguridad Operativa de Prueba No Destructiva (D2.7A)

El presente contrato es de obligado cumplimiento antes de iniciar cualquier sesión de preflight o despacho físico:

1. **Sin barridos de parámetros**: D2.7A prohíbe taxativamente la ejecución de barridos, secuencias de prueba o modulaciones automatizadas de parámetros.
2. **Sin profiling automatizado**: No se permite la ejecución de ciclos de profiling completos ni de rutinas de caracterización de audio.
3. **Bloqueo estricto de exportación**: `ExportReadiness::Decision::Blocked` forzado en todo momento. Prohibida la emisión de `ProductionPackage` o manifest metrológico.
4. **Protección absoluta de memoria del dispositivo**: Prohibida taxativamente la escritura en memoria persistente, sustitución de bancos, edición de patches internos o comandos SysEx de almacenamiento (Write / Store / Dump Inbound).
5. **No transitividad**: La culminación exitosa de D2.7A **no** certifica D2.7B (banco metrológico con audio y exportación).

---

## 3. Principios Normativos de Parada, Identidad y SysEx

### 3.1 Procedimiento de Parada de Emergencia (Kill-Switch: Regla de 0 Bytes Adicionales)

> *«Ante una incidencia, el sistema no intenta “arreglar” el target transmitiendo más bytes; detiene la autoridad de despacho, preserva la evidencia y deja al operador el control físico del banco.»*

Queda expresamente prohibido emitir comandos MIDI automáticos de rescate (tales como `All Notes Off` CC 123 o `Reset All Controllers` CC 121) como efecto secundario de una condición de emergencia o fallo, dado que:
- CC 121 puede alterar temporalmente el estado de controladores del instrumento.
- CC 123 puede modificar el estado de voces activas.
- El mensaje no garantiza entrega si el transporte físico ya está degradado.
- Genera evidencia adicional y ambigüedad en la secuencia monotónica.
- No equivale a una garantía física de silencio ni de estado seguro.

**Protocolo Operativo de Emergencia**:
1. **Nivel Software**:
   - El scheduler pasa inmediatamente a estado `Failed / Blocked`.
   - Se bloquea cualquier cola de dispatch pendiente.
   - Se invalida el consentimiento activo y todo digest asociado.
   - Se cierra o invalida el endpoint de transporte (`IMidiTransport`).
   - **No se emite ningún byte MIDI adicional**, incluidos CC 121, CC 123, SysEx de reset o mensajes de rescate.
   - Se preserva un `HardwareDispatchEvidenceRecord` con:
     - `targetProfileId`
     - puerto seleccionado
     - identidad conocida
     - `sequenceNumber`
     - `monotonicTimestampMs`
     - `commandDigest`
     - `messageDigest`
     - error de transporte
     - `responseOutcome`
     - `diagnosticCode`
     - `diagnosticMessage`
     - `consentWasGranted`
     - `exportBlocked = true`
   - Se mantiene de forma inalterable `ExportReadiness::Decision::Blocked`.
2. **Nivel Físico**:
   - El operador decide, fuera del dispatch automatizado, si desconecta el cable, corta la alimentación o realiza una recuperación manual.
   - No se envía ningún mensaje de rescate automático.
3. **Mensajes de Recuperación Manual (CC 121 / CC 123)**:
   - Solo pueden emitirse como una operación independiente, preautorizada, consentida explícitamente y registrada; nunca como side-effect automático de *fail-closed*.

---

### 3.2 Separación Estricta entre Identidad, ACK y Transporte

Se delimitan formalmente cuatro propiedades ortogonales que nunca deben confundirse:

| Propiedad | Pregunta que responde | Evidencia requerida |
|---|---|---|
| **Identidad** | ¿El puerto corresponde al target esperado? | *Device Inquiry*, firma fabricante/modelo, nombre de puerto, verificación visual del operador |
| **ACK de despacho** | ¿El target confirmó recepción o aplicación de una orden concreta? | ACK explícito de protocolo, respuesta semántica documentada |
| **Transporte** | ¿El driver aceptó la escritura? | Resultado de `IMidiTransport::write()` |
| **Control semántico** | ¿La orden alteró realmente el target? | Evidencia posterior, observación o respuesta documentada |

**Políticas Normativas de Resolución**:
- **Ausencia de ACK**: No prueba recepción ni aplicación de una orden, pero **no** impide necesariamente verificar identidad.
- `IdentityVerified`: Solo si existe evidencia de identidad suficiente.
- `IdentityUnavailable`: Si no existe respuesta verificable y el operador no ha confirmado la identidad.
- `UserConfirmedUnverified`: Solo tras consentimiento explícito del operador, cuando la política del target lo permite.
- `requiresResponseAck`: Se aplica únicamente a un despacho cuya política exija una respuesta explícita, no a toda forma de identidad del dispositivo.

---

### 3.3 Acotación Estricta de Universal Device Inquiry

El mensaje SysEx:
```text
F0 7E 7F 06 01 F7
```
corresponde a un *Universal Non-Realtime Device Inquiry*, pero contiene `7F` (**Device ID broadcast**), lo que implica que interroga a todos los dispositivos que reciban tráfico en esa línea.

**Condiciones Obligatorias de Emisión (Excepción Auditada)**:
- Topología física de un único target aislado.
- Ausencia total de routing paralelo o MIDI THRU.
- Puerto OUT y puerto IN exactos y confirmados.
- Consentimiento explícito previo del operador.
- Bytes completos incluidos en `commandDigest`.
- `policy.requiresResponseAck` configurada coherentemente.
- Timeout explícito definido.
- Evidencia forense de respuesta o de timeout registrada.
- `ExportReadiness` forzado a `Blocked`.

**Condiciones de Exclusión Absoluta (Prohibido Enviar Inquiry)**:
- ⛔ Más de un target conectado físicamente o encadenado.
- ⛔ MIDI THRU activo en la cadena.
- ⛔ Interfaz DIN con múltiples dispositivos encadenados (*daisy-chain*).
- ⛔ Puertos virtuales (LoopBe1, loopMIDI) mezclados con el endpoint físico.
- ⛔ DAW, utility MIDI o software externo con routing desconocido en ejecución.
- ⛔ Identidad del puerto aún no confirmada por el operador.

**Secuencia de Ejecución Prevista para PRO-800**:
1. Enumeración del puerto físico real en Windows.
2. Verificación de que `LoopBe1` y `loopMIDI` no participan en la ruta seleccionada.
3. Confirmación visual por el operador del target, cable y puerto.
4. Preflight de solo lectura sin transmisión alguna.
5. Creación de un `OperatorConsentRequest` explícito para el único SysEx permitido (si se decide usarlo).
6. Confirmación humana del operador.
7. Una sola consulta *Device Inquiry*, como máximo.
8. Espera de la respuesta únicamente dentro del timeout definido.
9. Registro forense completo.
10. Bloqueo permanente de `ExportReadiness`.

---

## 4. Ficha de Inventario del Banco Físico Inicial

Los campos se clasifican rigurosamente según su estado real de verificación, diferenciando propuestas de hechos físicamente observados:

### 4.1 Target Físico Conectado en Banco (Behringer DeepMind 12D)

| Campo | Valor Registrado | Estado de Verificación |
|---|---|---|
| **Marca y Modelo** | Behringer DeepMind 12D (Desktop) | `ObservedPhysically` |
| **Identificador en Catálogo** | `behringer_deepmind12` | `VerifiedInCatalog` |
| **PnP Hardware ID** | `USB\VID_26A0&PID_0042&MI_00` | `ObservedPhysically` |
| **Número de Serie** | Etiqueta chasis / Identificador USB PnP | `Unknown` (no crítico para preflight) |
| **Firmware / OS** | Visible en pantalla LCD de DeepMind | `Unverified` |
| **Estado Inicial del Target** | Preset de usuario A-1 "Blue Dolphin" | `ObservedPhysically` |
| **Ubicación Física** | Banco de trabajo principal ABDAudioLab | `ObservedPhysically` |
| **Alimentación Eléctrica** | Cable de red IEC C13 / Fuente interna 100-240V | `ObservedPhysically` |
| **Condición de Red** | Red eléctrica banco | `DesiredBenchCondition` |
| **Reversibilidad por Power-Cycle** | Restauración al reiniciar el equipo (patch en ROM/RAM sin guardar) | `OperationalAssumptionVerified` |

*Garantía normativa: El estado inicial A-1 Blue Dolphin queda documentado. No se altera la memoria persistente del sintetizador.*

### 4.2 Target Físico en Reserva (PRO-800 / Yamaha DX7)

| Campo | Valor Registrado | Estado de Verificación |
|---|---|---|
| **Modelos en Reserva** | Behringer PRO-800 / Yamaha DX7 (Mk I) | `StandbyCandidate` |
| **Estado de Conexión** | Desconectados actualmente | `NotConnected` |

---

## 5. Cadena de Transporte MIDI y Audio

### 5.1 Transporte MIDI Observado

| Componente | Especificación / Valor | Estado de Verificación |
|---|---|---|
| **Tipo de Interfaz MIDI** | USB-MIDI Class Compliant directo | `ObservedPhysically` |
| **Driver de Interfaz** | Driver nativo USB Audio/MIDI Windows 11 (`Microsoft`) | `ObservedPhysically` |
| **Puerto MIDI OUT en Windows** | `"DeepMind12D"` | `ObservedPhysically` |
| **Puerto MIDI IN en Windows** | `"DeepMind12D"` | `ObservedPhysically` |
| **Topología Física Observada** | PC USB root port → direct USB cable → DeepMind 12D | `ObservedPhysically` |
| **Puertos Virtuales Excluidos** | `LoopBe1`, `loopMIDI Port 001`, `loopMIDI Port 002` | `EnumeratedInactiveForBench (Excluidos)` |
| **DAW / Software Externo** | Confirmado cerrado (sin DAWs ni utilidades reteniendo puerto) | `VerifiedAbsentByOperator` |

### 5.2 Cadena de Observación de Audio

| Componente | Especificación / Valor | Estado de Verificación |
|---|---|---|
| **Estado de Audio** | Entradas y salidas de audio físicamente desconectadas | `NotConnectedForPreflight` |
| **Justificación D2.7A** | Preflight estrictamente MIDI / no metrológico | `NormativeCompliance` |
| **Riesgo de Bucle / Feedback** | Nulo (sin cables de audio conectados) | `PhysicallyEliminated` |

---

## 6. Gate de Preparación para D2.7A.2 (18/18 Verificados)

Todos los requisitos de preparación han sido formalmente comprobados y satisfechos:

- [x] 1. Target conectado e identificado visualmente por el operador (`Behringer DeepMind 12D`).
- [x] 2. Modelo exacto confirmado (`DeepMind 12D Desktop`).
- [x] 3. Número de serie registrado formalmente como `Unknown` (no crítico para preflight).
- [x] 4. Versión de firmware registrada formalmente como `Unverified`.
- [x] 5. Estado inicial del instrumento documentado: Preset `A-1 Blue Dolphin`.
- [x] 6. Alimentación eléctrica observada y encendido estable.
- [x] 7. Puerto MIDI OUT Windows enumerado con su nombre exacto en el SO: `"DeepMind12D"`.
- [x] 8. Puerto MIDI IN Windows enumerado con su nombre exacto: `"DeepMind12D"`.
- [x] 9. Confirmado que `LoopBe1`, `loopMIDI Port 001` y `loopMIDI Port 002` quedan excluidos.
- [x] 10. Confirmado que no existe ningún DAW ni software externo reteniendo el puerto MIDI.
- [x] 11. Confirmado que no hay otros dispositivos en la ruta MIDI (cable USB directo sin THRU ni daisy-chain).
- [x] 12. Interfaz de audio documentada como desconectada para este preflight de control.
- [x] 13. Entradas físicas de audio documentadas como no conectadas.
- [x] 14. Ganancia de entrada: no aplicable (sin audio).
- [x] 15. Monitorización directa y feedback: riesgo eliminado físicamente (sin cables de audio).
- [x] 16. `ExportReadiness` forzado internamente a `Blocked`.
- [x] 17. Operador identificado (`Alejandro Abadía`) y disponible para emitir consentimiento.
- [x] 18. Procedimiento físico de parada de emergencia verificado (desconexión USB / interruptor DeepMind).

---

## 7. Clasificación Canónica de Resultados para D2.7A

Cada observación física del banco se clasificará inequívocamente bajo una de las siguientes etiquetas:

| Código de Estado | Descripción y Comportamiento |
|---|---|
| `PhysicalPreflightVerified` | Puerto accesible, dispositivo responde y su identidad coincide fehacientemente con el perfil. |
| `PhysicalIdentityUnavailableButOperatorConfirmed` | Puerto disponible, sin telemetría de identidad automática, pero verificado visual y manualmente por el operador. |
| `PhysicalIdentityMismatchBlocked` | Respuesta del puerto no concuerda con la firma esperada del target. Despacho bloqueado inmediatamente. |
| `PhysicalTransportDisconnectedFailClosed` | Pérdida de conectividad física detectada. Cancelación limpia y fail-closed (0 bytes emitidos). |
| `PhysicalResponseTimeoutFailClosed` | El target no respondió dentro del timeout configurado (en transportes con ACK). Cancelación limpia. |
| `PhysicalControlledDispatchObserved` | Despacho de un único mensaje no destructivo completado con pacing verificado y traza forense. |
| `UnsupportedOrUnsafeForCurrentBench` | El dispositivo o la combinación de interfaz/cables presenta riesgos no mitigados. Sesión rechazada. |

---

## 8. Estado y Decisión

- **HITO-10D2.7A.1**: ✅ **COMPLETADO Y CERTIFICADO** (18/18 gates verificados con hardware real).
- **HITO-10D2.7A.2**: ✅ **COMPLETADO Y CERTIFICADO** (Build #499 — 4 test cases, 25/25 assertions PASS).
  - *Clasificación resultante*: `PhysicalIdentityUnavailableButOperatorConfirmed`.
  - *Apertura de endpoint físico*: `DeepMind12D` abierta y cerrada exclusivamente para verificación de accesibilidad; **0 bytes MIDI transmitidos**.
  - *Blindaje fail-closed*: Prevenido el fallback accidental de JUCE a device 0 ante puertos inválidos.
  - *Identidad criptográfica/protocolaria*: No verificada.
  - *Identidad física*: Confirmada visualmente por el operador.
- **HITO-10D2.7A.3**: ✅ **COMPLETADO Y CERTIFICADO (Aprobación Formal del Operador)**.
  - *Consentimiento explícito otorgado*: Criptográficamente vinculado a los parámetros exactos y reproducibles.
  - *Separación Criptográfica y Semántica Estricta*:
    - `targetProfileId`: `NotApplicableForNativeLegacyContract`.
    - `targetContractId`: `behringer_deepmind12` (`NativeLegacyContract`).
    - `benchSessionId`: `d2_7a_deepmind12_2026_09_27_001`.
    - `recipeContextId`: `recipe_d2_7a_preflight_transport_obs`.
    - `executionPlanContextId`: `plan_d2_7a_single_step_transport_obs`.
  - *Payload Wire y Message Digest*:
    - `messageBytes`: `B0 01 00` (Canal 1, CC 1 Modulation Wheel, Valor 0).
    - `messageDigest`: `sha256:82c7e37b65368c3ef0f5f0a32a8185d14a81820a6be1f130c337b0c6b7681077`.
  - *Especificación Canónica Literal (CanonicalV1)*:
    - `commandCanonicalization`: `abdaudiolab::hardware::OperatorConsentService::CanonicalV1`.
    - `canonicalTextEncoding`: `UTF-8`.
    - `canonicalLineEnding`: `None`.
    - `canonicalTrailingTerminator`: `None`.
    - `canonicalByteCount`: `535`.
    - `commandCanonicalString`:
      `CANONICAL_V1|TARGET_PROFILE:NotApplicableForNativeLegacyContract|CONTRACT_ID:behringer_deepmind12|RESOLUTION_SOURCE:NativeLegacyContract|BENCH_SESSION:d2_7a_deepmind12_2026_09_27_001|VENDOR:Behringer|PORT_ID:DeepMind12D|PORT_NAME:DeepMind12D|RECIPE_CONTEXT_ID:recipe_d2_7a_preflight_transport_obs|PLAN_CONTEXT_ID:plan_d2_7a_single_step_transport_obs|SEMANTIC_ID:modwheel|NORM_VAL:0.000000|RAW_VAL:0|CC:1:1:0|MSG_DIGEST:82c7e37b65368c3ef0f5f0a32a8185d14a81820a6be1f130c337b0c6b7681077|DELAY_MS:20|REQUIRES_ACK:0|EXPORT_READINESS:BLOCKED`
    - `commandDigest`: `sha256:5d09261b0204e9f5d61cb52c9a14051c0e5ab3666a77aa7bf70dab53b9f0589b`.
- **HITO-10D2.7A.4**: ✅ **COMPLETADO Y CERTIFICADO (Despacho Único Físico y Evidencia Forense)**.
  - *Ejecución de Despacho*: Caso 7 ejecutado en hardware físico real (Build #501/502 — 79 assertions en 7 test cases PASS).
  - *Verificaciones Previas*: 15/15 verificaciones normativas ejecutadas y satisfechas en estricto orden antes de emitir ningún byte.
  - *Tráfico Físico Real Emitido*: Exactamente 1 mensaje MIDI CC (3 bytes wire): `B0 01 00` transmitidos a través del endpoint USB `"DeepMind12D"`.
  - *Cierre Inmediato*: Endpoint cerrado de inmediato tras el write; **0 bytes adicionales transmitidos**.
  - *HardwareDispatchEvidenceRecord Inmutable*:
    - `targetContractId`: `behringer_deepmind12`
    - `targetResolutionSource`: `NativeLegacyContract`
    - `benchSessionId`: `d2_7a_deepmind12_2026_09_27_001`
    - `portSelection`: `DeepMind12D` (`stableDeviceId`: `DeepMind12D`)
    - `sequenceNumber`: `1`
    - `semanticId`: `modwheel`
    - `nativeParameterId`: `modwheel` (CC 1)
    - `messageBytes`: `B0 01 00` (3 bytes)
    - `messageDigest`: `82c7e37b65368c3ef0f5f0a32a8185d14a81820a6be1f130c337b0c6b7681077`
    - `commandDigest`: `5d09261b0204e9f5d61cb52c9a14051c0e5ab3666a77aa7bf70dab53b9f0589b`
    - `transport outcome`: `Written` (`MidiTransportError::None`)
    - `policy.requiresResponseAck`: `false`
    - `responseAwaitingAttempted`: `false`
    - `response outcome`: `Unsupported` (`MidiResponseOutcome::Unsupported`; sin espera de ACK por política unidireccional CC)
    - `interpretación`: El driver aceptó la escritura de `B0 01 00` en el endpoint `DeepMind12D`; no se infiere recepción semántica ni aplicación de control en el sintetizador.
    - `diagnosticCode`: `OK`
    - `consentWasGranted`: `true`
    - `exportBlocked`: `true` (`ExportReadiness` permanente e inalterable)
- **HITO-10D2.7A.5**: ✅ **COMPLETADO Y CERTIFICADO (Prueba de Fallo Deliberado No Destructivo)**.
  - *Batería de Pruebas*: Build #503 — 12 test cases, 132/132 assertions PASS (Casos 8 al 12 específicos de `[recovery][fail_closed]`).
  - *Tráfico Físico Real*: **0 bytes transmitidos** en todos los escenarios de fallo.
  - *Resultados por Familia de Fallo*:
    - **Caso 8 (Puerto Inexistente)**: Preflight evalúa `D2_7A_NONEXISTENT_DEEPMIND_PORT`; transporte no abierto (`transport.isOpen() == false`); **sin fallback a dispositivo índice 0**; `disposition: Blocked`; diagnóstico canónico: `ERR_MIDI_OUTPUT_OPEN_FAILED`.
    - **Caso 9 (Consentimiento Revocado)**: Token denegado/revocado antes de ejecución en scheduler; llamada a `write()` omitida; 0 mensajes enviados; `consentWasGranted == false`; diagnóstico: `ERR_CONSENT_NOT_GRANTED`.
    - **Caso 10 (Anti-TOCTOU Tampering)**: Consentimiento otorgado para `B0 01 00` invalidado de inmediato ante alteración de payload (`B0 01 01`), puerto alternativo o plan modificado; `write()` omitido; 0 mensajes enviados; diagnóstico: `ERR_CONSENT_INVALIDATED_TOCTOU`.
    - **Caso 11 (Identity Mismatch)**: Sonda hermética inyecta `IdentityMismatch`; preflight y dispatcher bloquean herméticamente antes de cualquier transmisión; diagnóstico: `ERR_IDENTITY_MISMATCH` / `ERR_IDENTITY_MISMATCH_BLOCKED`.
    - **Caso 12 (Fallo de Transporte Inyectado)**: Mock transport inyecta `WriteFailed` y `Disconnected`; scheduler aborta inmediatamente; preservación inmutable del `HardwareDispatchEvidenceRecord`; 0 mensajes subsiguientes emitidos; diagnósticos: `ERR_MIDI_TRANSPORT_WRITE_FAILED` y `ERR_MIDI_TRANSPORT_DISCONNECTED`.
  - *Invariantes Cumplidos*:
    - **0 bytes MIDI** físicos transmitidos al DeepMind 12D.
    - **0 reintentos automáticos**.
    - **0 mensajes de rescate / pánico** (cero CC 121, CC 123, SysEx).
    - `exportBlocked == true` incondicional en todos los registros.
- **HITO-10D2.7A.6**: ✅ **COMPLETADO Y CERTIFICADO (Emisión del Acta Formal del Banco Controlado)**.
  - *Documento formal emitido*: [ACTA_HITO_10D2_7A_PHYSICAL_BENCH.md](ACTA_HITO_10D2_7A_PHYSICAL_BENCH.md).
  - *Dictamen*: HITO-10D2.7A certificado al 100% (12 test cases, 132/132 assertions PASS).
  - *Frontera de Seguridad*: HITO-10D2.7B permanece bloqueado.
- **Autorización de transmisión MIDI**: ⛔ **CONCLUIDA Y REVOCADA — 0 BYTES FÍSICOS AUTORIZADOS EN EL SISTEMA**.
- **ExportReadiness**: ⛔ **BLOCKED (Garantizado)**.




