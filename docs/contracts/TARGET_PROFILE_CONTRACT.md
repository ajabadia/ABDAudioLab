# Contrato Normativo: TargetProfile y Resolución Declarativa de Targets

**Versión del Esquema:** 1.0  
**Fecha:** 2026-09-24  
**Estado:** VIGENTE / NORMATIVO  
**Referencia:** [RFC 8785 (JSON Canonicalization Scheme)](https://www.rfc-editor.org/info/rfc8785/) | [RFC 6901 (JSON Pointer)](https://www.rfc-editor.org/info/rfc6901/)

---

## 1. Principio y Ecuación Fundamental

> **«La receta expresa la intención científica; el perfil declara cómo ese target puede realizarla; el entorno confirma si puede hacerlo hoy; el motor común sigue siendo el único que ejecuta.»**

$$\underbrace{\text{MeasurementRecipe}}_{\text{Qué se quiere medir (semanticId)}} \;+\; \underbrace{\text{TargetProfile}}_{\text{Cómo lo mapea el Target (transporte)}} \;+\; \underbrace{\text{ExecutionEnvironment}}_{\text{Dónde se ejecuta hoy}} \;\implies\; \underbrace{\text{ResolvedExecutionPlan}}_{\text{Secuencia física ejecutable}}$$

---

## 2. Separación de las Tres Identidades

1. **`targetProfileId` (Contrato Declarativo):**  
   Identificador persistente del archivo JSON de perfil (ej. `org.abd.reference-synth.v1`, `org.abd.dexed.v1`). Permanece invariable ante actualizaciones del binario del target.
2. **`canonicalTargetId` (Familia o Target Lógico):**  
   Identifica la familia lógica del dispositivo o plugin (ej. `reference-synth`, `vst3-dexed-3f015740-d7709eec`).
3. **`ExecutionEnvironment` (Instancia Concreta de Ejecución):**  
   Registra el hash SHA-256 binario del target efectivamente cargado en memoria, versión observada, ruta en el sistema operativo, controlador de audio, tamaño de bloque y frecuencia de muestreo.

### Política de Fixity Binaria y Compatibilidad
El perfil declara la regla de integridad mediante `identity.binaryIdentityPolicy`:
- `not-applicable`: Para targets sintéticos en código o hardware analógico.
- `warn-on-mismatch`: El preflight genera advertencia (`ApprovedWithWarnings`) si el binario difiere de las referencias previas, pero no bloquea la sesión.
- `require-audit-on-change`: Si el hash del binario ha cambiado respecto al último auditado, se exige reauditoría previa antes de certificar.
- `strict-bit-exact`: Rechazo terminante si el binario no coincide bit a bit.

---

## 3. Arquitectura de Transporte Discriminado (`TechnicalIdentifier`)

Queda estrictamente prohibida la ambigüedad en los identificadores técnicos. Se modela mediante unión discriminada por tipo de transporte:

| Transporte (`kind`) | Campos Requeridos | Uso Previsto |
|---|---|---|
| **`InternalParameter`** | `parameterKey` (string) | Fixtures sintéticas, motores internos (`ReferenceSynth`). |
| **`VST3Parameter`** | `parameterIndex` (int $\ge 0$), `parameterId` (opcional) | Plugins VST3 (`Dexed`). |
| **`MidiContinuousController`** | `channel` ($1 \dots 16$), `controllerNumber` ($0 \dots 127$) | Hardware digital con automatización MIDI CC. |
| **`MidiSysEx`** | `messageTemplate` (hex/template), `valueEncoding` | Sintetizadores clásicos gobernados por System Exclusive. |
| **`ManualOperator`** | `instructionId` (string), `confirmationPrompt` (string) | Dispositivos puramente analógicos con intervención del operador. |

---

## 4. Cadena de Autoridad y Cero Bifurcación

```
TargetProfileService       --> Carga, valida y canonicaliza TargetProfile
MeasurementRecipeService   --> Carga, valida y canonicaliza MeasurementRecipe
ExperimentPlanCompiler     --> ÚNICA AUTORIDAD: combina Recipe + Profile + Environment
                               y produce ResolvedExecutionPlan
ProfilingSessionController --> Recibe la sesión preparada
ProfilingSequencer         --> Ejecuta la sesión en el hilo de audio en tiempo real
```

- `TargetProfileService` nunca compila ni ejecuta.
- `TargetProfile` nunca crea sesiones ni despacha audio ni MIDI.
- `ExperimentPlanCompiler` es la única autoridad de resolución y no contiene nombres de plugins en código (`if (targetName == "Dexed")` está estrictamente prohibido).

---

## 5. Hash Canónico de Perfil (`canonicalProfileHash`)

Se calcula aplicando SHA-256 sobre la representación normalizada RFC 8785 del documento JSON de perfil:
- Invariante ante el orden de claves.
- Invariante ante saltos de línea (`\n` vs `\r\n`) o espacios en blanco.
- Permite verificar la integridad de los perfiles declarativos cargados desde disco.

---

## 6. Modelo de Capacidades de Audio y Buses Efectivos (Matiz de Canales)

Para evitar falsos positivos de incompatibilidad, el contrato distingue explícitamente entre la capacidad física total del dispositivo y los buses lógicos asignados al target:

1. **Capacidad del Dispositivo Físico:**
   - `ExecutionEnvironment.deviceAvailableInputChannels`: número total de entradas de la interfaz de audio.
   - `ExecutionEnvironment.deviceAvailableOutputChannels`: número total de salidas de la interfaz de audio.
   - Una interfaz física con 8 o 16 canales no es por sí misma incompatible con un target estéreo.

2. **Buses Efectivos Asignados al Target:**
   - `ExecutionEnvironment.targetInputChannelLayout`: bus efectivo entregado al target (ej: `"stereo"`, 2 canales).
   - `ExecutionEnvironment.targetOutputChannelLayout`: bus efectivo capturado/observado desde el target.

3. **Requisitos Declarativos:**
   - `TargetProfile.audioOutput.supportedChannelCounts`: recuentos de canales admitidos por el target.
   - `TargetProfile.audioOutput.requiredChannelCount`: recuento requerido para operar.
   - `MeasurementRecipe.observationRequirements`: configuración que exige la prueba.

**Criterio Normativo de Compatibilidad:**
$$\text{TargetProfile requiere estéreo} \;+\; \text{Bus efectivo target = estéreo} \;+\; \text{Recipe admite observación estéreo} \;\implies\; \mathbf{Compatible}$$

---

## 7. Semántica de Control de Hardware, Cuantización y Seguridad (HITO-10D1)

### 7.1 Cuantización MIDI CC Congelada
Para el mapeo de parámetros normalizados en `[0.0, 1.0]` a valores nativos de controlador continuo de 7 bits (`[0..127]`), el contrato fija normativamente la fórmula matemática:
$$\text{rawValue} = \mathrm{round}(\text{normalizedValue} \times 127.0)$$

- **Puntos canónicos:**
  - $0.000\dots \longrightarrow \text{CC } 0$
  - $0.500\dots \longrightarrow \text{CC } 64$
  - $1.000\dots \longrightarrow \text{CC } 127$
- **Rango normativo:** `channel` $\in [1..16]$, `controllerNumber` $\in [0..127]$.
- **Política de Invarianza:** Esta regla de redondeo queda congelada como parte de la semántica del contrato. Cualquier cambio futuro a `floor()` o `ceil()` altera físicamente el comportamiento del hardware y se considera un **breaking change** que exige:
  1. Incremento de versión mayor del contrato (`schemaVersion >= "2.0"`).
  2. Migración explícita de perfiles.
  3. Recálculo y nueva auditoría de hashes de plan resuelto (`resolvedExecutionPlanHash`).

### 7.2 Seguridad Hermética y Delimitación SysEx
Para el control mediante MIDI System Exclusive:
- **Delimitadores estrictos:** La plantilla debe iniciar obligatoriamente con el byte `F0` (System Exclusive Start) y finalizar con `F7` (EOX - End of Exclusive).
- **Carga útil 7-bit:** Ningún byte intermedio dentro de la plantilla literal puede tener el bit más significativo activado ($> 0\text{x}7\text{F}$).
- **Catálogo cerrado de tokens:** Únicamente se admiten `{deviceId}`, `{value7bit}`, `{valuenibblemsb}`, `{valuenibblelsb}`, `{checksum}`, `{xx}`.
- **Algoritmos de Checksum:** Los algoritmos implementados y auditados son:
  - Yamaha: `(-sum) & 0x7F`
  - Roland: `(128 - (sum % 128)) & 0x7F`
- **Axioma Hermético:** En la fase de resolución declarativa (HITO-10D1), la carga, validación y resolución del plan opera **estrictamente en memoria**. Ningún byte de SysEx es transmitido físicamente a interfaces MIDI del sistema. El envío real queda estrictamente reservado a la fase de despacho físico con preflight y confirmación explícita (HITO-10D2).

### 7.3 Aislamiento de Hilos de Audio para ManualOperator
Para targets que requieren intervención manual del operador (ej. pedales analógicos con potenciómetros físicos):
- **Clasificación:** El hardware analógico con controles físicos manuales (como el BOSS DS-1) se clasifica como hardware analógico con control manual guiado (circuito activo, audio mono o estéreo según topología).
- **Settling Time:** Se exige un tiempo de asentamiento de estabilización humana mínimo ($\ge 500\,\text{ms}$).
- **Desacoplo de Tiempo Real:** La interacción del operario opera a nivel de máquina de estados de alto nivel (`WaitingForOperator`) mediante tarjetas informativas en la interfaz gráfica (`OperatorCardsContainerComponent`). **Bajo ninguna circunstancia** se permite la espera humana (`sleep`, `wait`, bloqueo interactivo) dentro de `audioDeviceIOCallbackWithContext()` o cualquier callback del motor de audio en tiempo real.


