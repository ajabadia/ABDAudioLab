# Arquitectura de Perfilado de Hardware por Contrato: Init Patch, Desconstrucción Funcional y Sanity Check

**Proyecto:** ABDAudioLab (Universal Black-Box Musical Hardware Profiler)  
**Documento:** HARDWARE_PROFILING_BY_CONTRACT_DESIGN.md  
**Versión:** 1.0.0  
**Fecha:** 2026-10-21  
**Estado:** NORMATIVO / APROBADO PARA PLANIFICACIÓN  
**Alcance:** Modelado metrológico universal de sintetizadores, pedales y hardware sin hardcodear equipos en C++

---

## 1. El Problema Metrológico: La «Falacia del Preset Arbitrario»

### 1.1 El Riesgo de Medir sobre Presets Aleatorios
En el perfilado de audio analógico y digital, un error frecuente consiste en conectar un sintetizador (físico o VST3) con el último preset que estuviera cargado en memoria y lanzar un barrido de medición:
- Si el preset activo es un *Pad Ambiental*, tendrá el filtro a medio cerrar, una envolvente con ataque lento, modulación de LFO activa y efectos estéreo (chorus, delay, reverb).
- Si el operador o la sesión lanza un barrido de frecuencias para calibrar el filtro VCF, lo que se mide **no es el filtro analógico del sintetizador**, sino la suma convolucionada de la envolvente, el chorus desfasando la señal y el decaimiento de la reverb.
- Si otro usuario mide el mismo sintetizador con un preset de *Bass Lead*, obtendrá una respuesta en frecuencia, THD y dinámica completamente dispares.

> [!CAUTION]
> **La Falacia del Preset:** Medir un sintetizador en un estado arbitrario produce datos no reproducibles, carentes de validez científica e inútiles para entrenar emulaciones o generar modelos de caja negra fidedignos.

### 1.2 La Necesidad del «Estado Base» (Init Patch) y el Aislamiento Funcional
Para obtener un perfil metrológico fiel de un sintetizador, se requieren dos condiciones sine qua non:
1. **Llevar el hardware a su Estado Base Canónico («Init Patch»)**: Neutralizar modulaciones, apagar efectos de espacio/tiempo, fijar envolventes en sustain estático y abrir filtros.
2. **Desconstruir la máquina en Bloques Funcionales Aislados**: Medir los osciladores sin interferencia del filtro; medir el filtro con una onda pura de referencia; medir las envolventes con una entrada constante; y medir la saturación del previo sin modulación.

---

## 2. Principio Rector: Cero Hardcoding en C++

Queda terminantemente prohibido escribir código C++ específico por modelo de equipo:
```cpp
// ❌ PROHIBIDO EN ABDAudioLab:
if (targetName == "DeepMind12") {
    sendDeepMindInitSysex();
} else if (targetName == "KorgMS2000") {
    sendMS2000ResetCC();
}
```

Toda la inteligencia de inicialización, ruteo, mapeo de parámetros, temporización y desconstrucción debe residir en **Contratos Declarativos JSON**:
$$\underbrace{\text{MeasurementRecipe}}_{\text{Qué fenómeno físico se mide}} \;+\; \underbrace{\text{TargetProfile (Contrato JSON)}}_{\text{Cómo se pone a cero y se aísla ese hardware}} \;+\; \underbrace{\text{ExecutionEnvironment}}_{\text{Puertos físicos y audio}} \;\implies\; \underbrace{\text{ResolvedExecutionPlan}}_{\text{Secuencia universal}}$$

Añadir un nuevo sintetizador (Moog, Roland, Sequential, Eurorack o pedal) consiste **únicamente en redactar o importar un archivo JSON** en `contracts/hardware/`. El motor de ejecución C++ no se recompila ni se altera.

---

## 3. Arquitectura del Motor de Estado Base (Init Patch Engine)

Cada contrato de perfil de hardware (`contracts/hardware/<target>.json`) define la sección `lifecycle`:

```json
{
  "id": "behringer_deepmind12",
  "displayName": "Behringer DeepMind 12 (Analog Polysynth)",
  "deviceType": "AUTOMATED_MIDI_CC",
  "lifecycle": {
    "preSessionSetup": [
      {
        "description": "Panic / All Sound Off",
        "method": "MIDI_CC",
        "channel": 1,
        "cc": 120,
        "value": 0,
        "settlingDelayMs": 50
      },
      {
        "description": "Reset All Controllers",
        "method": "MIDI_CC",
        "channel": 1,
        "cc": 121,
        "value": 0,
        "settlingDelayMs": 50
      },
      {
        "description": "Cargar Init Patch vía SysEx",
        "method": "SYSEX_RAW",
        "sysexHexPayload": "F0 00 20 32 20 00 ... F7",
        "settlingDelayMs": 300
      },
      {
        "description": "Bypass Efectos Digitales (FX Bypass)",
        "method": "MIDI_CC",
        "channel": 1,
        "cc": 98,
        "value": 0,
        "settlingDelayMs": 100
      }
    ],
    "postSessionTeardown": [
      {
        "description": "All Notes Off",
        "method": "MIDI_CC",
        "channel": 1,
        "cc": 123,
        "value": 0
      }
    ]
  }
}
```

### 3.1 Taxonomía de Tipologías de Target y Políticas de Puesta a Cero (Límites Estrictos)

Para evitar ambigüedades, el sistema delimita con precisión matemática y operativa las cuatro tipologías de dispositivos, maximizando la automatización mediante botones y reduciendo la intervención manual al mínimo estrictamente necesario:

| Tipología de Target | Entorno Físico y Ruteo | Política de Puesta a Cero (Init State) | Experiencia en UI (Paso 3) | Sonda Preflight |
|---|---|---|---|---|
| **1. Plugin VST3 / Digital** (`PluginVST3`) | **100% In-The-Box (ITB)**.<br>Cero cables físicos. Búfer directo en memoria. | **100% Automática**.<br>Inyección instantánea de chunk binario de preset base o reseteo de lista de parámetros por host. | Se ejecuta automáticamente al entrar. Botón secundario disponible: **`[ Reload Default Init State ]`**. | Ping digital interno de $100\text{ ms}$. Verifica que el plugin procesa sin muting ni NaN. |
| **2. Sintetizador Hardware Digital / MIDI** (`HardwareDigital` / `AUTOMATED_MIDI_CC` / `AUTOMATED_SYSEX`) | **Audio Analógico + Control MIDI**.<br>DAC no requerido (el sinte genera audio). Synth Audio Out $\to$ ADC 1. Cable USB MIDI o 5-Pin DIN. | **Automatizable por Botón o Auto-disparo**.<br>Transmisión de volcado SysEx de banco Init o ráfaga de CCs (corte abierto, chorus off, sustain máx). | Muestra botón destacado: **`[ Send Init Patch via SysEx / MIDI ]`** con barra de progreso y retardo de estabilización ($300\text{ ms}$). | Disparo de nota MIDI C4 ($1.0\text{ s}$). Verifica en ADC 1 que suena el tono Init sin filtros no deseados. |
| **3. Hardware Híbrido / Semi-modular** (`HardwareHybrid` / `AUTOMATED_NRPN`) | **Audio Analógico + Control Híbrido**.<br>Conexión de audio y MIDI, más 1 o 2 controles físicos maestros (ej. Master Volume, conmutador de modo). | **Híbrida Asistida**.<br>Envío automático de parámetros por MIDI/SysEx + confirmación visual de perillas maestras físicas. | Botón **`[ Send MIDI Init ]`** + Checklist interactivo reducido solo para los interruptores físicos maestros. | Inyección de nota MIDI C4 + comprobación acústica en ADC 1. |
| **4. Hardware 100% Analógico / Manual** (`HardwareAnalogue` / `MANUAL_EURORACK` / `ANALOGUE_PEDAL`) | **Lazo Cerrado Analógico Puro**.<br>DAC Out 1 $\to$ Hardware Input $\to$ Hardware Output $\to$ ADC In 1. Cero puertos MIDI/USB. | **Manual Asistida por Pantalla**.<br>El operador debe colocar físicamente potenciómetros e interruptores en la posición base. | Renderizado vectorial de knobs mostrando el ángulo exacto requerido. Botón: **`[ Confirm Controls Aligned (Spacebar) ]`**. | Inyección de tono sinusoidal puro de $1\text{ kHz}$ a $-18\text{ dBFS}$ desde DAC Out 1. Verifica continuidad y ausencia de zumbido de masa. |

> [!IMPORTANT]
> **Regla de Máxima Automatización:** Siempre que el hardware cuente con interfaz de control (MIDI o USB), ABDAudioLab **prohíbe exigir giros manuales de potenciómetros para la puesta a cero**. El sistema envía el volcado SysEx o la secuencia de controladores en un solo clic mediante el botón **`[ Send Init Patch ]`**, ahorrando tiempo al usuario y garantizando exactitud científica bit a bit.

---

### 3.2 El Paso 3 como Gate Metrológico Obligatorio e Infranqueable

El Paso 3 (`Hardware Hookup & Preflight`) **NO es una pantalla informativa opcional ni prescindible**: es un **Gate Metrológico Estricto**.

1. **Prohibición de Salto Directo a Medición**:
   - Queda terminantemente prohibido avanzar del Paso 2 (`Interface Calibration`) al Paso 4 (`Run Acquisition`) sin haber validado el Paso 3.
   - El botón **`[ Proceed to Run Acquisition (Step 4) → ]`** nace **bloqueado (`setEnabled(false)`)** en todos los casos.
2. **Criterios Obligatorios para Desbloquear la Medición**:
   - **Condición 1 (Estado Base Verificado)**:
     - En VST3: Estado inicial cargado automáticamente.
     - En Hardware MIDI: Comando `[ Send Init Patch ]` ejecutado con éxito (ACK de transporte o timeout de transmisión completado).
     - En Hardware Analógico: Checkbox de knobs alineados confirmado por el operador.
   - **Condición 2 (Sonda de Señal Satisfactoria)**:
     - La prueba acústica/digital disparada por **`[ Test Signal Now ]`** debe devolver el veredicto **`PASS`** (nivel RMS $> -60\text{ dBFS}$, pico $< -0.5\text{ dBFS}$ y coherencia espectral validada).
3. **Mecanismo de Bypass para Operadores Avanzados**:
   - Si un usuario experto desea medir intencionadamente un equipo en un estado especial o forzar la captura, debe pulsar explícitamente el botón secundario **`[ Bypass Preflight ]`**.
   - En este caso, el Paso 3 se marca como `Skipped` y la exportación final sella en el manifest la etiqueta:  
     `AuditStatus: UnverifiedAcousticBaseline (Preflight Bypassed by Operator)`.

---

### 3.3 Métodos de Despacho Declarativo (`setupAction.method`)

El despachador universal soporta cuatro métodos de puesta a cero sin recompilación:


| Método (`method`) | Mecanismo Físico | Aplicación Típica |
|---|---|---|
| **`SYSEX_RAW`** | Envío de trama System Exclusive en hexadecimal | Carga completa de banco Init, volcado de memoria a búfer de edición temporal (Roland DT1, Korg SysEx, Behringer). |
| **`MIDI_CC` / `NRPN`** | Secuencia ordenada de controladores MIDI estándar o de alta resolución | Sintetizadores modernos que permiten inicializar módulos por CC (abrir filtro CC 74 = 127, desactivar chorus CC 93 = 0, sustain CC 79 = 127). |
| **`MANUAL_PROMPT`** | Tarjeta visual interactiva para el operador humano con confirmación (ACK) | Módulos Eurorack analógicos sin MIDI, pedales de guitarra clásicos, sintetizadores vintage pre-MIDI (Minimoog, MS-20 original). |
| **`VST3_STATE`** | Inyección de chunk binario de estado predefinido o reseteo de lista de parámetros | Plugins de instrumentos y procesadores digitales cargados en el host interno. |

### 3.4 Protocolo Universal de Cableado y Checklist Declarativo (`wiringGuide` y `MANUAL_PROMPT`)

El sistema debe guiar al usuario paso a paso en inglés, tanto para la conexión física de cables como para la posición inicial de potenciómetros y conmutadores antes de la primera medición. Todo se define en el contrato JSON:

#### Ejemplo A: Pedal Analógico de Distorsión/Overdrive (ej. BOSS DS-1 / Ibanez TS9)

```json
{
  "id": "boss_ds1_distortion",
  "displayName": "BOSS DS-1 Distortion (Analog Stompbox)",
  "deviceType": "ANALOGUE_PEDAL",
  "wiringGuide": {
    "steps": [
      "1. Connect Audio Interface Output 1 (DAC) to Pedal INPUT jack using a standard 1/4\" TS instrument cable.",
      "2. Connect Pedal OUTPUT jack to Audio Interface Input 1 (ADC) using a standard 1/4\" TS instrument cable.",
      "3. Connect a regulated 9V DC power supply (or ensure a fresh 9V battery is installed)."
    ],
    "stimulusOutput": "Audio Out 1 (DAC) -> Pedal INPUT",
    "responseInput": "Pedal OUTPUT -> Audio In 1 (ADC)",
    "notes": "Closed-loop instrument level routing (-20 dBu to -10 dBu nominal)."
  },
  "lifecycle": {
    "preSessionSetup": [
      {
        "description": "Step 1: Set [TONE] knob to 12 o'clock (center detent / neutral EQ)",
        "method": "MANUAL_PROMPT"
      },
      {
        "description": "Step 2: Set [LEVEL] knob to 12 o'clock (approximate unity gain)",
        "method": "MANUAL_PROMPT"
      },
      {
        "description": "Step 3: Set [DISTORTION / GAIN] knob to minimum (fully counter-clockwise, 7 o'clock)",
        "method": "MANUAL_PROMPT"
      },
      {
        "description": "Step 4: Press pedal footswitch to engage effect (verify red CHECK LED is illuminated)",
        "method": "MANUAL_PROMPT"
      }
    ]
  }
}
```

#### Ejemplo B: Sintetizador Hardware con Control MIDI (ej. Behringer DeepMind 12 / Korg MS2000)

```json
{
  "id": "korg_ms2000",
  "displayName": "Korg MS2000 Analog Modeling Synthesizer",
  "deviceType": "AUTOMATED_MIDI_CC",
  "wiringGuide": {
    "steps": [
      "1. Connect USB or MIDI Interface Output (5-Pin DIN) to Synthesizer MIDI IN port.",
      "2. Connect Synthesizer Main L/Mono Audio Output to Audio Interface Input 1 (ADC) using a balanced/unbalanced 1/4\" cable.",
      "3. Set Synthesizer Master Volume knob to 75% (approx. 3 o'clock position)."
    ],
    "stimulusOutput": "MIDI Out (Channel 1)",
    "responseInput": "Synth Main L/Mono -> Audio In 1 (ADC)",
    "notes": "Line level signal capture with automated SysEx program reset."
  },
  "lifecycle": {
    "preSessionSetup": [
      {
        "description": "Load Canonical Init Program via SysEx (Program Dump 0x40)",
        "method": "SYSEX_RAW",
        "sysexHexPayload": "F0 42 30 58 40 ... F7",
        "settlingDelayMs": 350
      },
      {
        "description": "Ensure Chorus/Flanger Mod FX is bypassed (CC 93 = 0)",
        "method": "MIDI_CC",
        "channel": 1,
        "cc": 93,
        "value": 0,
        "settlingDelayMs": 50
      }
    ]
  }
}
```

#### Ejemplo C: Módulo Modular Eurorack (ej. Doepfer A-120 VCF)

```json
{
  "id": "doepfer_a120_vcf",
  "displayName": "Doepfer A-120 24dB Low Pass Filter",
  "deviceType": "MANUAL_EURORACK",
  "wiringGuide": {
    "steps": [
      "1. Connect Audio Interface Output 1 (DAC) to Eurorack Audio In jack using a 3.5mm TS patch cable.",
      "2. Connect Eurorack Audio Out jack to Audio Interface Input 1 (ADC) using a 3.5mm TS patch cable.",
      "3. Disconnect all modulation patch cables from CV2 and CV3 inputs."
    ],
    "stimulusOutput": "Audio Out 1 (DAC) -> Eurorack Audio In",
    "responseInput": "Eurorack Audio Out -> Audio In 1 (ADC)",
    "notes": "Modular level signal (+/- 5V peak). Use proper input/output attenuation if needed."
  },
  "lifecycle": {
    "preSessionSetup": [
      {
        "description": "Step 1: Turn Resonance (Emphasis) knob fully counter-clockwise (value 0)",
        "method": "MANUAL_PROMPT"
      },
      {
        "description": "Step 2: Set Audio In Level attenuator knob to 2 o'clock position (nominal 0 dB)",
        "method": "MANUAL_PROMPT"
      },
      {
        "description": "Step 3: Turn Frequency (Cutoff) knob fully clockwise (100% open)",
        "method": "MANUAL_PROMPT"
      }
    ]
  }
}
```


---

## 4. Matriz de Desconstrucción y Aislamiento por Bloques Funcionales

Un sintetizador sustractivo típico se compone de una cadena en cascada:
$$\text{Oscilador (VCO/DCO)} \;\longrightarrow\; \text{Filtro (VCF)} \;\longrightarrow\; \text{Amplificador (VCA)} \;\longrightarrow\; \text{Efectos (FX)}$$

Para perfilarlo con rigor metrológico, el contrato desglosa el hardware en **Bloques Funcionales Aislados** (`functions`). Cada bloque define sus **Prerrequisitos de Aislamiento**:

```
┌────────────────────────────────────────────────────────────────────────┐
│                   MATRIZ DE AISLAMIENTO FUNCIONAL                      │
├────────────────────────────────┬───────────────────────────────────────┤
│ BLOQUE A MEDIR                 │ CONFIGURACIÓN DE AISLAMIENTO          │
├────────────────────────────────┼───────────────────────────────────────┤
│ 1. Oscilador (VCO/DCO)         │ • VCF: 100% abierto (Cutoff máx, Res 0)│
│    • Formas de onda puras      │ • VCA: Sustain 100%, Decay 0          │
│    • Espectro armónico         │ • Modulaciones: LFO = 0, PitchBend = 0 │
│    • Aliasing y jitter de fase │ • Efectos: BYPASS total               │
├────────────────────────────────┼───────────────────────────────────────┤
│ 2. Filtro (VCF)                │ • Entrada: Inyección externa calibrada│
│    • Función de transferencia  │   o 1 solo Oscilador Sawtooth puro    │
│    • Barrido Cutoff vs Res     │ • VCF Env: Modulación EG = 0          │
│    • Auto-oscilación           │ • VCA: Sustain 100% constante         │
│    • Curvas no lineales/LUT    │ • Efectos: BYPASS total               │
├────────────────────────────────┼───────────────────────────────────────┤
│ 3. Dinámica y Envolvente (VCA) │ • VCF: 100% abierto constante         │
│    • Curvatura Attack/Decay    │ • Oscilador: Tono continuo estable    │
│    • Tiempos reales (ms)       │ • Estímulo: Secuencia Gate escalón    │
│    • Sangrado / Bleed de VCA   │ • Efectos: BYPASS total               │
├────────────────────────────────┼───────────────────────────────────────┤
│ 4. Previo, Saturación y FX     │ • Estímulo: Multitono / Sweep dBFS    │
│    • THD+N vs Nivel de entrada │ • Análisis: Núcleos Volterra / Wiener │
│    • Respuesta temporal delay  │ • Medición de distorsión y color anal.│
└────────────────────────────────┴───────────────────────────────────────┘
```

### Declaración en el Esquema de Perfil

Cada función en el contrato declara cómo aislarse automáticamente:

```json
{
  "functions": [
    {
      "id": "vcf_core",
      "name": "Analog Low Pass Filter Core",
      "blockType": "SpectrumFilter",
      "isolationPrerequisites": [
        { "method": "MIDI_CC", "cc": 19, "value": 1.0, "description": "Osc 1 Level = Max" },
        { "method": "MIDI_CC", "cc": 20, "value": 0.0, "description": "Osc 2 Level = Off" },
        { "method": "MIDI_CC", "cc": 21, "value": 0.0, "description": "Noise Level = Off" },
        { "method": "MIDI_CC", "cc": 70, "value": 0.0, "description": "VCF Env Depth = 0" },
        { "method": "MIDI_CC", "cc": 79, "value": 1.0, "description": "VCA Sustain = Max" }
      ],
      "sweepParameters": [
        { "parameterKey": "cutoff", "steps": 32, "distribution": "logarithmic" },
        { "parameterKey": "resonance", "steps": 5, "values": [0.0, 0.25, 0.5, 0.75, 1.0] }
      ]
    }
  ]
}
```

---

## 5. Sonda de Verificación y Sanity Check Acústico Preflight

### 5.1 El Problema del Error Humano o Físico Oculto
Incluso con comandos de reseteo, pueden ocurrir fallos físicos:
- El operador olvidó conectar el cable de audio o lo conectó a una salida errónea.
- El sintetizador tiene un interruptor físico trasero que anula el SysEx.
- Un pedal analógico tiene un potenciómetro de mezcla mojada/seca (Dry/Wet) en una posición indebida.

Si el sistema comienza una campaña de medición de 15 minutos sin verificar el estado acústico real, se consumirá tiempo inútilmente y se generarán datos corrompidos.

### 5.2 Arquitectura del Preflight Sanity Check Obligatorio (Prueba Rápida de 1.5 Segundos)

Para evitar medir a ciegas, **el botón de iniciar la sesión de medición completa permanece bloqueado** hasta que una prueba acústica rápida de señal sea ejecutada y resulte satisfactoria. 

¿Es complicado técnicamente? **No, en absoluto.** En términos de DSP y metrología es un procedimiento elemental, robusto y de ejecución instantánea (< 1.5 segundos):

1. **Inyección de Tono Piloto**:
   - Para pedales / Eurorack: Inyección de un tono sinusoidal puro a $1\text{ kHz}$ a $-18\text{ dBFS}$ (o un barrido chirp ultracorto de $500\text{ ms}$).
   - Para sintetizadores MIDI: Disparo de una nota C4 (MIDI 60) a velocidad nominal 100 con compuerta de $1\text{ segundo}$.

2. **Evaluación de Tres Criterios Deterministas**:
   - **A. Presencia de Señal (Continuidad Física)**:  
     $$\text{RMS}_{\text{in}} > -60.0\text{ dBFS}$$
     *Si el nivel es inferior a $-80\text{ dBFS}$, el cable está desconectado, el pedal no tiene corriente (pila/fuente 9V) o el bypass está mal ruteado.*
   - **B. Margen Dinámico / No Recorte en ADC (Headroom)**:  
     $$\text{Peak}_{\text{in}} < -0.5\text{ dBFS}$$
     *Garantiza que la interfaz de audio no esté saturando digitalmente antes de empezar la medición científica.*
   - **C. Coherencia Espectral vs Zumbido de Masa**:  
     Se calcula el pico de Goertzel en $1\text{ kHz}$ frente al ruido de baja frecuencia ($50\text{ Hz} / 60\text{ Hz}$).
     *Si la energía está concentrada en $50\text{ Hz}$, avisa inmediatamente: «Ground loop / Cable buzz detected. Please check shielding».*

```
[Usuario conecta cables y ajusta knobs según pantalla]
                     │
                     ▼
         [Botón: "Test Signal Now"]
                     │
                     ▼
      [Inyección de Tono Piloto 1 kHz (-18 dBFS)]
                     │
                     ├──► 1. ¿Nivel RMS > -60 dBFS?
                     │    NO ──► ❌ ERROR: "No signal detected on Input 1. Check cable and power."
                     │
                     ├──► 2. ¿Pico < -0.5 dBFS?
                     │    NO ──► ⚠️ ALERTA: "ADC Input clipping! Reduce pedal output volume."
                     │
                     ├──► 3. ¿Energía en 1 kHz > Ruido de masa (50Hz)?
                     │    NO ──► ⚠️ ALERTA: "Mains hum detected. Check audio cable shielding."
                     │
                     ▼
        [✓ SIGNAL VERIFIED — GREEN LIGHT]
   (Level: -18.2 dBFS | SNR: 74 dB | THD: 0.15%)
                     │
                     ▼
       [Habilitar Botón: "Start Measurement →"]
```

### 5.3 Experiencia de Usuario y Manejo de Errores
- La interfaz muestra un vúmetro en vivo con un botón explícito: **`[Test Signal Now]`**.
- Si el usuario gira el potenciómetro o conecta el cable mientras mira la pantalla, puede pulsar **`[Re-test Signal]`** tantas veces como necesite.
- El botón **`[Start Measurement]`** solo pasa de estado deshabilitado (`disabled`) a activo cuando la verificación concluye con `PASS`.
- De este modo, es físicamente imposible que el usuario lance una sesión de 20 minutos con un cable desconectado o con el pedal apagado.


---

---

## 6. Integración en la Interfaz de Usuario: Flujo Canónico de 5 Pasos (Opción A)

Para respetar la secuencia física real del laboratorio y mantener una ergonomía visual impecable, el flujo de trabajo se estructura en **5 pasos canónicos** en el `SoundIdSidebarStepper`:

```
┌───────────────────────────────────┐
│ 1. Target & Routing               │  (Selección de dispositivo en catálogo y topología lógica)
├───────────────────────────────────┤
│ 2. Interface Calibration          │  (Lazo cerrado directo DAC➔ADC de la tarjeta: 2A Ruido + 2B Latencia)
├───────────────────────────────────┤
│ 3. Hardware Hookup & Preflight ★  │  (NUEVO: 3A Cableado, 3B Knobs iniciales y 3C Sonda de audio obligatoria)
├───────────────────────────────────┤
│ 4. Run Acquisition                │  (Batería de excitación: matriz de tomas y 8 pestañas de gráficos FFT/THD)
├───────────────────────────────────┤
│ 5. Export & Report                │  (Auditoría metrológica, manifest de producción y exportación)
└───────────────────────────────────┘
```

> [!NOTE]
> **Secuencia Física Sin Fricción:** En el Paso 2 el usuario calibra su tarjeta con un cable directo entre salidas y entradas de la interfaz. Al terminar el Paso 2, entra en el **Paso 3**, donde retira el cable de calibración, conecta el pedal o sintetizador a la tarjeta, ajusta los potenciómetros iniciales y realiza la prueba de señal antes de lanzar la adquisición (Paso 4).

---

### 6.1 Layout Visual de la Pantalla del Paso 3 (`Hardware Hookup & Preflight`)

El diseño mantiene **estricta coherencia con el sistema de diseño actual (`SoundIdTheme` / Técnica Refinada)**: tarjetas con esquinas redondeadas (`8.0f`), bordes sutiles (`borderSubtle`), paleta HSL curada (`accentGreen`, `accentBlue`, `accentAmber`, `accentRed`), tipografía "Inter" y serigrafía vectorial para perillas y faders.

```
┌────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ Step 3: Hardware Hookup & Preflight Verification                                                       │
│ Physical hardware cabling (3A), initial control alignment (3B), and acoustic preflight signal check (3C). │
├───────────────────────────────────────────────────┬────────────────────────────────────────────────────┤
│ 3A. PHYSICAL HARDWARE HOOKUP         [ PENDING ]  │ 3B. INITIAL CONTROL ALIGNMENT         [ PENDING ]  │
│                                                   │                                                    │
│ ┌─────────────────────────┐ Step-by-Step Guide:   │ Controls Baseline Setup (Init Patch):              │
│ │ [ESPACIO RESERVADO PARA │ 1. Connect Audio Out 1│                                                    │
│ │  DIAGRAMA DE CONEXIÓN   │    (DAC) to Pedal IN  │    ┌───────────┐    ┌───────────┐    ┌───────────┐ │
│ │  O ESQUEMA TOPOLÓGICO   │    via 1/4" TS cable. │    │   TONE    │    │   LEVEL   │    │DISTORTION │ │
│ │  SIMILAR A INTERACTIVE  │ 2. Connect Pedal OUT  │    │    (•)    │    │    (•)    │    │    /      │ │
│ │  STUDIO TOPOLOGY]       │    to Audio In 1 (ADC)│    │   12:00   │    │   12:00   │    │ 7:00(Min) │ │
│ │                         │    via 1/4" TS cable. │    │ (Neutral) │    │(Unity 0dB)│    │ (Gain: 0) │ │
│ └─────────────────────────┘ 3. Connect 9V power.  │    └───────────┘    └───────────┘    └───────────┘ │
│                                                   │                                                    │
│ [✓] Cables Connected & Hardware Powered           │ [✓] Footswitch Engaged (Red Check LED is ON)       │
├───────────────────────────────────────────────────┴────────────────────────────────────────────────────┤
│ 3C. ACOUSTIC PREFLIGHT SIGNAL VERIFICATION                                                             │
│                                                                                                        │
│ ┌───────────────────┐ ┌───────────────────┐ ┌───────────────────┐ ┌───────────────────┐              │
│ │ INPUT RMS LEVEL   │ │ SIGNAL-TO-NOISE   │ │ HARMONIC COHERENCE│ │ GROUND INTEGRITY  │              │
│ │ -18.2 dBFS        │ │ 76.4 dB           │ │ Peak: 1000 Hz     │ │ Clean (No 50Hz)   │              │
│ │ [■■■■■■■■□□ -18dB]│ │ > 60 dB tolerance │ │ Dominant Pilot OK │ │ Proper Shielding  │              │
│ └───────────────────┘ └───────────────────┘ └───────────────────┘ └───────────────────┘              │
│                                                                                                        │
│ ● PREFLIGHT STATUS: SIGNAL VERIFIED (READY FOR ACQUISITION)                                            │
│                                                                                                        │
│ [ Bypass Preflight ]           [ Test Signal Now (Step 3C)... ]    [ Proceed to Run Acquisition (Step 4) → ]│
└────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

---

### 6.2 Componentes Gráficos Coherentes con el Sistema de Diseño

1. **Tarjeta 3A (`PhysicalHardwareHookupComponent`)**:
   - **Lado izquierdo**: Contenedor con relación de aspecto adaptativa preparado para alojar el diagrama esquemático de conexiones (o una vista compacta vectorizada inspirada en `Interactive Studio Topology`).
   - **Lado derecho**: Lista de pasos secuenciales en inglés (`wiringGuide.steps`), formateados en tarjetas horizontales con borde fino (`borderCard`) e iconos de conector jack.
   - **Insignia de estado**: Badge `PENDING` (ámbar) / `CONNECTED` (verde).

2. **Tarjeta 3B (`InitialControlAlignmentComponent`)**:
   - Reutiliza el motor de renderizado vectorial de perillas y faders de [`OperatorStepModalDialog`](file:///d:/desarrollos/ABDSynths/ABDAudioLab/src/gui/OperatorStepModalDialog.h) y `SoundIdKnobComponent`.
   - Dibuja los potenciómetros con su ángulo real de giro, puntero de contraste y serigrafía superior e inferior (nombre del parámetro y valor nominal: `12:00`, `7:00 / Min`).
   - Checkbox interactivo de confirmación con estilo SoundId.

3. **Tarjeta 3C (`AcousticPreflightVerificationComponent`)**:
   - 4 cajas métricas que siguen exactamente la rejilla de `INTERFACE METROLOGY METRICS` del Paso 2.
   - Mini-vúmetro de barra horizontal estéreo en tiempo real.
   - **Botón `[ Test Signal Now (Step 3C)... ]`**: Botón primario en verde SoundId (`accentGreen`), accesible con `Barra Espaciadora` o tecla `T`.
   - **Guardrail**: El botón `[ Proceed to Run Acquisition (Step 4) → ]` permanece **bloqueado (`setEnabled(false)`)** hasta que la sonda reporte `PreflightResult::Pass`.
   - **Botón `[ Bypass Preflight ]`**: Botón secundario discreto (`surfaceSubtle`) con confirmación de advertencia para operadores avanzados, manteniendo paridad con el botón `Bypass Calibration` del Paso 2.

---

### 6.3 Lógica de Activación y Navegabilidad Idéntica al Paso 3 Actual

El nuevo Paso 3 (`Hardware Hookup & Preflight`) hereda **la misma lógica de activación, desbloqueo y guardas de navegación** que rige actualmente el avance desde el Paso 2:

1. **Condición de Entrada al Paso 3 (Desbloqueo desde Paso 2)**:
   - En el Paso 2 (`Interface Calibration`), cuando el usuario completa la calibración de la tarjeta (Step 2A + 2B) o hace clic en `Bypass Calibration`:
     ```cpp
     // Callback nativo al terminar o saltar Paso 2:
     nativeCalibrationPanel.onContinueToSession = [this] {
         workflowNavController.setStepStatus(Step::CalibrateLoopback, StepStatus::Completed);
         workflowNavController.setStepLocked(Step::HardwareHookup, false);
         workflowNavController.setStep(Step::HardwareHookup); // Pasa a Current
     };
     ```
   - Si el Paso 2 no ha concluido ni se ha omitido, el Paso 3 permanece en estado `Pending` y bloqueado (`isStepLocked == true`), impidiendo que el usuario salte directamente a medir hardware sin calibrar la tarjeta.

2. **Navegabilidad Bidireccional en el Sidebar (`isStepNavigable`)**:
   - Una vez que el Paso 3 se vuelve accesible, el usuario tiene libertad para hacer clic en el sidebar stepper y regresar al **Paso 1** (para consultar el perfil) o al **Paso 2** (para revisar la curva de loopback), exactamente igual que ocurre hoy entre los pasos activos.
   - El indicador en la tarjeta de resumen (`SessionSummaryCard`) muestra la telemetría en tiempo real: `Preflight: Pending` o `Preflight: Verified`.

3. **Condición de Salida hacia el Paso 4 (`Run Acquisition`)**:
   - Sigue exactamente la misma semántica de transición que el Paso 2:
     - Si la sonda resulta satisfactoria (`PreflightResult::Pass`), el Paso 3 pasa a `Completed` con su check verde (`✓`) y se activa el botón de avance hacia el Paso 4.
     - Si el usuario pulsa `Bypass Preflight` (operador avanzado), el Paso 3 pasa a `Skipped` (ámbar) y se permite la navegación al Paso 4 registrando la procedencia (`AuditStatus: UnverifiedAcousticBaseline`).
     - Al avanzar:
       ```cpp
       workflowNavController.setStepStatus(Step::HardwareHookup, StepStatus::Completed);
       workflowNavController.setStepLocked(Step::RunAcquisition, false);
       workflowNavController.setStep(Step::RunAcquisition);
       ```

---

### 6.4 Máquina de Estados de Navegación del Stepper

```
[Paso 1: Target & Routing]
            │
            ▼
[Paso 2: Interface Calibration] (Loopback directo de tarjeta)
            │ (onContinueToSession o onCalibrationSkipped)
            ▼
[Paso 3: Hardware Hookup & Preflight]
            ├── 3A: Usuario conecta jacks y corriente
            ├── 3B: Usuario alinea perillas según serigrafía
            └── 3C: Click [ Test Signal Now ] ──► Inyección tono 1 kHz (1.5s)
                         │
                         ├──► Fallo (Sin señal / Clipping / Hum) ──► Alerta roja + Proceed bloqueado
                         │
                         └──► PASS (o Bypass) ──► Badge verde + Proceed desbloqueado
                                                        │
                                                        ▼
                                           [Paso 4: Run Acquisition] (Las 32 tomas + gráficos FFT)
                                                        │
                                                        ▼
                                           [Paso 5: Export & Report]
```


### 6.5 Interacción Durante la Medición (Paso 4: `Run Acquisition`)

Cuando la receta requiera cambiar de posición un potenciómetro analógico durante la medición (ej. toma 8 de 32: subir Distorsión al 25%):
- El secuenciador en tiempo real (`ProfilingSequencer`) pausa el hilo de audio y pasa a estado `WaitingForOperator`.
- Se abre la ventana modal interactiva [`OperatorStepModalDialog`](file:///d:/desarrollos/ABDSynths/ABDAudioLab/src/gui/OperatorStepModalDialog.h):  
  *«STEP 8/32: Set [DISTORTION] knob to 10 o'clock (25% gain) and press SPACEBAR to continue.»*
- El operador gira la perilla física y presiona la **Barra Espaciadora**.
- El sistema aplica el tiempo de estabilización mecánica (50 ms) y reanuda la captura automáticamente.



---

## 7. Fases de Implementación en el Roadmap

Para planificar, seguir y verificar esta funcionalidad sin regresiones en la suite de pruebas (manteniendo la línea base de 684+ test cases en verde):

### Fase 1: Validación y Extensión Declarativa del Esquema JSON
- **Objetivo**: Asegurar que `hardware_profile.schema.json` valide formalmente `lifecycle.preSessionSetup`, `isolationPrerequisites` y las directivas de `SanityCheckProbe`.
- **Artefactos**: Esquemas JSON y tests unitarios de validación de sintaxis en `src/tests/test_TargetProfileHardwareValidation.cpp`.

### Fase 2: Ejecutor de Lifecycle en `ExperimentPlanCompiler`
- **Objetivo**: Que el compilador de planes inserte las tareas de puesta a cero como pasos preliminares obligatorios en el `ResolvedExecutionPlan`.
- **Artefactos**: `src/experiment/ExperimentPlanCompiler.cpp`, tests herméticos sin hardware real en `src/tests/test_ResolvedExecutionPlanLifecycle.cpp`.

### Fase 3: Tarjetas de Intervención Manual (`MANUAL_PROMPT`) en GUI
- **Objetivo**: Presentar al operador la lista de verificación interactiva de ajustes físicos para sintetizadores analógicos y pedales antes de la calibración.
- **Artefactos**: Componente visual en el Paso 2 (WorkflowNavigationController), conmutación accesible por teclado (barra espaciadora) y tests headless.

### Fase 4: Analizador Acústico de Sanity Check
- **Objetivo**: Algoritmo rápido (FFT + Goertzel + detector de retardo) que certifica en 1.5s que el tono de prueba no tiene modulación ni corte indeseado.
- **Artefactos**: `src/math/analytics/AcousticSanityProbe.h`, validado contra señales sintéticas con y sin chorus/delay residual.

### Fase 5: Contratos Certificados de Hardware Real
- **Objetivo**: Proveer contratos oficiales listos para usar:
  - `contracts/hardware/behringer_deepmind12.json` (SysEx Init + CCs)
  - `contracts/hardware/korg_ms2000.json` (SysEx 0x40 Init + CCs de corte)
  - `contracts/hardware/roland_aira_modular.json` (SysEx DT1 de módulos limpios)
  - `contracts/hardware/manual_eurorack_vcf.json` (Checklist de operador)
  - `contracts/hardware/generic_midi_synth.json` (CC 120/121/123 + CC 74/71)

---

## 8. Conclusión

Con este diseño:
1. **El usuario no necesita ser un experto en cada sintetizador**: El contrato del hardware sabe exactamente cómo poner a cero ese modelo específico y cómo aislar su filtro, sus osciladores y su envolvente.
2. **Cero modificaciones en C++ para nuevos equipos**: Cualquier usuario o fabricante puede añadir un sintetizador nuevo escribiendo un archivo JSON con los comandos SysEx, CC o instrucciones de perillas.
3. **Metrología repetible y reproducible**: Todas las mediciones parten de un estado base certificado y auditado acústicamente, garantizando modelos matemáticos y emulaciones fidedignas.
