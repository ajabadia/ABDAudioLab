# ROADMAP: Despacho Físico, Emulación de Firmware y Retirada Legacy

> *«Muchos tipos de target y transporte, una sola receta, una sola sesión, un solo secuenciador, una sola evaluación y una sola exportación.»*

---

## 1. Estado Actual Sellado

- **HITO-09:** Recetas de medición — **CERTIFICADO**.
- **HITO-10A:** TargetProfile JSON + ReferenceSynth — **CERTIFICADO** (Build #434).
- **HITO-10B:** Paridad declarativa vs legacy — **CERTIFICADO** (Build #436).
- **HITO-10C:** Dexed VST3 real, perfil declarativo, fixity binaria, draft y respuesta acústica — **CERTIFICADO** (Build #439).
- **HITO-10D1:** CC, SysEx y ManualOperator: contratos, validación y resolución hermética — **CERTIFICADO** (Build #443).
- **D2.1:** TransportPolicy declarativa — **COMPLETADA** (Build #445).
- **D2.2:** IMidiTransport, tipos de transporte y MockMidiTransport determinista — **COMPLETADA** (Build #446).

### Línea de Base Global Consolidada (Oficial Previa a D2):
```text
Build #443:
  684 test cases
  676 PASS
  8 SKIPPED justificados
  0 FAIL
  268.496 assertions PASS
```
*Nota:* Cuando se ejecute la suite global después de D2.1, D2.2 y subsiguientes, esa conformará la nueva baseline oficial.

---

## 2. HITO-10D2: Despacho Físico Seguro

### D2.3 — Preflight e Identidad
**Objetivo:** Saber si el transporte está disponible y si el target puede verificarse antes de enviar ninguna orden de medición.

**Archivos:**
- `src/hardware/preflight/HardwareTransportPreflightTypes.h`
- `src/hardware/preflight/HardwareTransportPreflightService.h`
- `src/hardware/preflight/HardwareTransportPreflightService.cpp`
- `src/hardware/preflight/IMidiIdentityProbe.h`
- `src/tests/test_TargetProfileHardwarePreflight.cpp`

**Regla de Desacoplamiento:**
- `IMidiTransport`: envía mensajes (salida).
- `IMidiIdentityProbe`: consulta identidad y recibe respuesta (entrada/handshake).
- `HardwareTransportPreflightService`: combina transporte, perfil, entorno y resultado de identidad.

**Estados Normativos de Identidad:**
- `PortAvailable`: el puerto existe y abre.
- `IdentityVerified`: el hardware respondió y coincide con el perfil.
- `IdentityUnavailable`: el puerto abre, pero no hay identidad verificable.
- `IdentityMismatch`: el hardware respondió, pero no coincide con el perfil.
- `UserConfirmedUnverified`: solo aparece después de confirmación humana en D2.4.

**Invariantes de D2.3 (Prohibiciones):**
- NO pide confirmación al usuario.
- NO envía CC de medición.
- NO envía SysEx de medición.
- NO crea una ProfilingSession.
- NO arranca audio.
- NO modifica el secuenciador.

**Salida Esperada de D2.3:**
- `Ready`: identidad verificada.
- `AwaitingUserConfirmation`: identidad no disponible, pero el perfil permite continuar con confirmación humana.
- `Blocked`: puerto no abre, identidad no coincide o el perfil exige verificación estricta.

---

### D2.4 — Consentimiento Explícito
**Objetivo:** Que ningún comando físico se transmita sin autorización adecuada.

**Archivos:**
- `src/hardware/consent/OperatorConsentRequest.h`
- `src/hardware/consent/OperatorConsentService.h`
- `src/hardware/consent/OperatorConsentService.cpp`
- `src/tests/test_TargetProfileOperatorConsent.cpp`

La UI solo debe mostrar un resumen y recoger la decisión. No construye mensajes MIDI ni transmite nada.

**Resumen de Consentimiento:**
- TargetProfile seleccionado.
- Puerto MIDI.
- Target identificado o no verificable.
- Parámetro a cambiar.
- Valor científico normalizado.
- Valor físico resultante.
- CC o SysEx que se enviará.
- Política de pacing.
- Riesgos o advertencias.
- Si requiere confirmación reforzada.

**Reglas de Consentimiento:**
- **SysEx:** confirmación explícita obligatoria.
- **Perfil importado:** no transmite sin consentimiento explícito.
- **Usuario cancela:** 0 bytes enviados.
- **IdentityUnavailable:** requiere confirmación para pasar a `UserConfirmedUnverified`, si la política lo permite.
- **IdentityMismatch:** no se puede autorizar bajo ningún concepto (fail-closed).

---

### D2.5 — Scheduler y Despacho Bajo Secuenciador
**Objetivo:** Conectar el transporte real al pipeline existente sin crear otro motor.

**Archivos previstos:**
- `src/hardware/transport/HardwareDispatchScheduler.h`
- `src/hardware/transport/HardwareDispatchScheduler.cpp`
- `src/hardware/transport/JuceMidiTransport.h`
- `src/hardware/transport/JuceMidiTransport.cpp`

**Cadena de Autoridad Obligatoria:**
```text
ProfilingSequencer
  → ProfilingHardwareDispatcher existente
    → HardwareDispatchScheduler
      → IMidiTransport
        → MIDI físico
```
*Prohibición estricta:* No debe existir `UI → JuceMidiTransport → MIDI físico`.

**Parámetros de Pacing (`TargetProfile.transportPolicy`):**
- `minimumInterMessageDelayMs`
- `messageRateLimitPerSecond`
- `responseAckRequired`
- `responseAckTimeoutMs`
- `retryPolicy`
- `maxRetries`

**Reglas de Responsabilidad:**
- `IMidiTransport`: solo intenta transmitir.
- `HardwareDispatchScheduler`: controla el ritmo (pacing).
- `ProfilingSequencer`: decide cuándo se ejecuta cada trial.
- `UI`: solo presenta estado y recibe confirmación.

---

### D2.6 — Fallos, Timeout y Fail-Closed
**Objetivo:** Que un fallo físico nunca termine en un informe falso o exportable.

**Archivos:**
- `src/tests/test_TargetProfilePhysicalDispatch.cpp`
- `src/tests/test_TargetProfileTransportFailureRecovery.cpp`

**Batería de Pruebas Obligatoria:**
- **CC:** mensaje correcto, canal correcto, valor correcto, orden correcto y pacing correcto.
- **SysEx:** bytes exactos, delimitadores, checksum, pacing y consentimiento.
- **Desconexión:** transición a `Failed` o `Aborted`.
- **Fallo de escritura:** `ERR_MIDI_TRANSPORT_WRITE_FAILED`.
- **Timeout:** `ERR_MIDI_TARGET_NO_RESPONSE` o estado documentado.
- **Fallo:** `ExportReadiness::Decision::Blocked`.
- **Fallo:** no `ProductionPackage`.
- **Fallo:** registro forense preservado.

**Registro Forense Obligatorio ante Error:**
- `targetProfileId`
- `recipeDocumentHash`
- `resolvedExecutionPlanHash`
- Puerto seleccionado
- Estado de identidad
- Mensaje o hash de mensaje
- `sequenceNumber`
- Timestamp monotónico
- Resultado del write
- Timeout observado
- Diagnóstico RFC 6901 / código de error
- Estado final de sesión

---

### D2.7 — Cierre de HITO-10D2
Solo cerrar formalmente cuando:
1. Preflight real validado.
2. Consentimiento real validado.
3. Mock de transporte validado.
4. Pacing validado.
5. CC físico validado.
6. SysEx físico validado o explícitamente limitado.
7. ManualOperator validado.
8. Timeouts y desconexiones validados.
9. Exportación bloqueada ante fallo.
10. Suite global verde (PASS).
11. Acta formal `ACTA_HITO_10D2_HARDWARE_DISPATCH.md` emitida.

---

## 3. HITO-10V: Firmware-Emulated Targets / VES Integration

### Nombre y Justificación
- **Identificador:** `HITO-10V` (o `HITO-10C2`: Emulación de Hardware por Firmware / ROM).
- *Razón:* Deja claro que es una vía de validación virtual hermética y no un paso de despacho físico.

### Objetivo
Integrar **Vintage Emulator Studio (VES)** como target de software que emula una máquina vintage concreta (ej. **Casio CZ-101**), conservando:
- Misma `MeasurementRecipe`.
- Mismo `TargetProfile` lógico.
- Mismo `ExperimentPlanCompiler`.
- Misma `ProfilingSession`.
- Mismo `ProfilingSequencer`.
- Mismo DSP.
- Misma evaluación.
- Misma exportación.

### Diferencia de Endpoints:
- **Hardware físico:** `IMidiTransport` $\rightarrow$ puerto MIDI real $\rightarrow$ equipo real $\rightarrow$ interfaz de audio.
- **VES emulado:** `InProcessVirtualMidiTransport` $\rightarrow$ MIDI/SysEx interno del plugin $\rightarrow$ VES VST3 $\rightarrow$ audio interno en memoria.

*Nota legal sobre VES:* VES se presenta como una colección de emulaciones de instrumentos vintage basada en MAME e incluye Casio CZ-101 entre los modelos soportados; las ROM no se incluyen y deben ser aportadas por el usuario legalmente.

### Requisitos de HITO-10V

#### A. Identidad del Emulador
Guardar y comprobar:
- Plugin UID de VES.
- Versión de VES.
- Hash del binario VES.
- Máquina emulada: ej. `casio-cz101`.
- Hash de ROM.
- Tamaño de ROM.
- Hash del estado inicial o preset.
- Sample rate, block size, canales.
- Política de reset.

#### B. Perfil Lógico (`profiles/targets/casio_cz101_ves.target.json`)
Debe declarar:
- `targetKind`: `"FirmwareEmulatedPlugin"`.
- `canonicalTargetId`: `"casio-cz101"`.
- `emulatorHost`: `"Vintage Emulator Studio"`.
- `emulatorMachineId`: `"cz101"`.
- `transport`: `"InProcessVirtualMidi"`.
- `audio`: `"InProcessPluginAudio"`.
- `requiresPhysicalMidiPort`: `false`.
- `requiresPhysicalAudioInterface`: `false`.
- `romIdentityRequired`: `true`.

#### C. Seguridad Legal y Técnica
**Prohibiciones Estrictas:**
- NUNCA incluir ROM dentro de `TargetProfile`.
- NUNCA descargar ROM automáticamente.
- NUNCA adjuntar ROM a perfiles compartidos.
- NUNCA redistribuir ROM mediante ABDAudioLab.

**Datos Permitidos:**
- `romHash`
- `romSize`
- `machineId`
- Mensaje de diagnóstico si ROM no encontrada o distinta.
- Versión mínima o recomendada de VES.

#### D. Qué Probar en HITO-10V
1. VES se carga como VST3.
2. La máquina CZ-101 se selecciona correctamente.
3. La ROM requerida existe y su hash coincide.
4. El perfil CZ-101 se valida.
5. SysEx virtual llega a VES internamente.
6. El audio aparece en el buffer interno.
7. El estado se puede reiniciar.
8. La receta se repite con el mismo plan.
9. El audio es bit-exacto o se clasifica según la repetibilidad observada.
10. La evidencia se etiqueta como `FirmwareEmulated / VirtualTarget`, no `HardwarePhysical`.

#### E. Uso Esperado y Límites
VES debe servir para probar perfiles CZ-101, validar SysEx sin riesgo para hardware, ejecutar CI headless y comparar emulación vs unidad física.
*Límite científico:* No afirma automáticamente *"Esto certifica todos los CZ-101 físicos"*. Certifica estrictamente esa versión de VES, esa ROM, esa máquina emulada y ese perfil.

---

## 4. HITO-10E: Retirada Segura de Legacy

*Condición vinculante:* No debe empezar hasta que D2 esté plenamente certificado.

**Objetivo:** Retirar datos duplicados, sin retirar capacidades y manteniendo fallback mientras haya targets no migrados.

**Secuencia Vinculante:**
1. Inventariar `contracts/hardware` legacy.
2. Clasificar cada dato (UI, detección, capacidad, CC, SysEx, manual, routing, política de medición).
3. Migrar un target cada vez a `TargetProfile`.
4. Comparar legacy vs declarativo.
5. Mantener fallback temporal.
6. Retirar únicamente los datos duplicados que ya tengan paridad certificada.
7. Nunca eliminar un contrato legacy porque "parece que ya no se usa".

---

## 5. Orden de Ejecución Vinculante

```text
1. D2.3: Preflight e identidad (CERTIFICADO)
2. D2.4: Consentimiento explícito (CERTIFICADO)
3. D2.5: Scheduler y despacho integrado (CERTIFICADO)
4. D2.6: Pruebas de despacho, timeout y fail-closed (CERTIFICADO)
5. D2.7A: Banco Físico Controlado — Preflight, Identidad, Consentimiento y Observación (EN CURSO)
6. D2.7B: Banco Físico Metrológico (Audio + Exportación) (BLOQUEADO TRAS D2.7A)
7. HITO-10V: VES / Casio CZ-101 emulado (Firmware-Emulated Target) (BLOQUEO PREVENTIVO TRAS V0.1)
8. HITO-10E: Retirada gradual de duplicados legacy (CERTIFICADO)
9. HITO-SHARED-SYNC: Enriquecimiento y Transferencia hacia ABDSharedCode (PROGRAMADO)
```

---

## 6. Enriquecimiento y Transferencia hacia ABDSharedCode (`HITO-SHARED-SYNC`)

### 6.1. Delimitación de Autoridad y Responsabilidades entre Capas

| Capa | Módulos Clave | Propósito Arquitectónico |
|---|---|---|
| **Capa Compartida** (`ABDSharedCode/HardwareMidiDetect` y `HardwareDrivers`) | `HardwareMidiDetector`, `HardwareMidiHotplugMonitor`, `JuceHardwareMidiPicker`, `MidiCcController`, `HardwareContractRegistry` | **Descubrimiento y UI:** Detectar puertos conectados en caliente (*hotplug*), consultar identidades estándar y ofrecer los componentes gráficos / WebView para seleccionar puertos en la interfaz de usuario. |
| **Capa de Gobernanza Metrológica** (`ABDAudioLab` D2.1–D2.6) | `HardwareTransportPreflightService`, `OperatorConsentService`, `HardwareDispatchScheduler`, `JuceMidiTransport` | **Seguridad y Ejecución Crítica:** Garantizar que ningún byte se transmite sin consentimiento explícito (con hash criptográfico SHA-256), controlar la tasa de envío (*pacing*), gestionar timeouts, aislar fallos de transporte (*fail-closed*) e impedir la exportación metrológica inválida. |

> **Principio Rector:** No se están duplicando funciones, sino delimitando la autoridad:  
> * `ABDSharedCode` se encarga de descubrir y conectar.  
> * `ABDAudioLab` (D2) se encarga de auditar, gobernar y proteger el hardware físico durante las sesiones de medición científica.

### 6.2. Alimentación del Laboratorio desde ABDSharedCode
El preflight de `HITO-10D2.7A` ya reutiliza y valida el contrato nativo compartido: en el test de coincidencia dirigida por contrato (*Contract-Driven Matching*), el sistema consulta el catálogo nativo (`contracts/hardware/behringer_deepmind12.json`) y empareja automáticamente `"DeepMind12D"` usando los `portNameMatches` de la capa compartida.

### 6.3. Paquete de Transferencia y Mejoras Hacia ABDSharedCode

1. **Blindaje contra el bug de fallback de JUCE en Windows (`JuceMidiHardwareBackend`)**:
   * *Hallazgo:* `juce::MidiOutput::openDevice` en Windows realiza un fallback silencioso al puerto en índice 0 si el identificador no existe o es inválido (evaluando `getIntValue()` que devuelve `0`).
   * *Acción:* Trasladar la validación estricta previa contra `getAvailableDevices()` implementada en `JuceMidiTransport` a `ABDSharedCode/HardwareMidiDetect/JuceMidiHardwareBackend.cpp` para proteger todas las herramientas compartidas contra conexiones accidentales.

2. **Adopción del modelo canónico de 5 Estados de Preflight**:
   * *Situación actual:* `HardwareMidiDetector` en `ABDSharedCode` opera con un enfoque binario (*detectado / no detectado*).
   * *Acción:* Enriquecer la capa compartida con los cinco estados ortogonales formalizados en D2:
     * `PortAvailable`: endpoint presente y accesible.
     * `IdentityVerified`: hardware responde a sondeo y concuerda con firma.
     * `IdentityUnavailable`: endpoint accesible sin telemetría de identidad automática.
     * `IdentityMismatch`: respuesta de hardware colisiona con el target esperado.
     * `UserConfirmedUnverified`: dispositivo no verificable por protocolo pero confirmado visualmente por el operador.

3. **Gobernanza de SysEx Broadcast y Exclusión de Puertos Virtuales**:
   * *Hallazgo:* Los puertos virtuales (`LoopBe`, `loopMIDI`, `teVirtualMIDI`) provocan bucles y colisiones cuando reciben transmisiones no controladas de sondeo (*Universal Device Inquiry* `F0 7E 7F 06 01 F7`).
   * *Acción:* Incorporar a la capa compartida de detección la exclusión explícita y configurable de endpoints virtuales durante ráfagas de escaneo automático.

