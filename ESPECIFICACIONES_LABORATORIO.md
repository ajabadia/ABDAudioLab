# ESPECIFICACIONES TÉCNICAS — LABORATORIO DE PERFILADO DE HARDWARE MUSICAL

**Proyecto:** ABDAudioLab — Plataforma Universal de Ingeniería Inversa y Perfilado de Hardware Musical (Universal Black-Box Hardware Profiler)

| Campo | Valor |
|---|---|
| **Versión del documento** | 1.1 |
| **Estado** | Aprobado para desarrollo (Actualizado con specs SysEx, FSK y papers de investigación) |
| **Audiencia** | Equipo de desarrollo (junior), QA, diseño |
| **Fecha** | 2026-09-01 |
| **Fuentes y Recursos** | `docs/google ia research/001.txt`, [AIRA_Modular_Effects-master](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/google%20ia%20research/AIRA_Modular_Effects-master/README.md) (Mugenkidou SysEx spec), [alltheFSKs-master](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/google%20ia%20research/alltheFSKs-master/README.md), [audio-latency-examiner-main](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/google%20ia%20research/audio-latency-examiner-main/README.md), [NeuralAudio-main](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/google%20ia%20research/NeuralAudio-main/README.md), [134-AES00.pdf](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/google%20ia%20research/134-AES00.pdf) (Angelo Farina Swept-Sine), [Wiener-Hammerstein model...pdf](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/google%20ia%20research/Wiener-Hammerstein%20model%20and%20its%20learning%20for%20nonlinear%20digital%20pre-distortion%20of%20optical%20transmitters-with-annotations.pdf) (Takeo Sasai et al.), [Plan Master PDF](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/google%20ia%20research/Especificaciones_Tecnicas_Laboratorio_Universal.pdf), `juce-audio-hybrid-plugin` skill |
| **Idioma** | Español (código e identificadores en inglés) |

---

## ÍNDICE

1. [Introducción](#1-introducción)
2. [Visión del producto y alcance](#2-visión-del-producto-y-alcance)
3. [Arquitectura general](#3-arquitectura-general)
4. [Entorno técnico y stack](#4-entorno-técnico-y-stack)
5. [Estructura de carpetas del proyecto](#5-estructura-de-carpetas-del-proyecto)
6. [Glosario](#6-glosario)
7. [Dominio: hardware, submódulos y efectos base](#7-dominio-hardware-submódulos-y-efectos-base)
8. [Modelo de datos del parche](#8-modelo-de-datos-del-parche)
9. [Especificación del protocolo FSK](#9-especificación-del-protocolo-fsk)
10. [Especificación del laboratorio](#10-especificación-del-laboratorio)
11. [Especificación del motor analítico](#11-especificación-del-motor-analítico)
12. [Especificación de la WebUI](#12-especificación-de-la-webui)
13. [Requisitos no funcionales](#13-requisitos-no-funcionales)
14. [Criterios de aceptación por fase](#14-criterios-de-aceptación-por-fase)
15. [Flujo de trabajo del equipo](#15-flujo-de-trabajo-del-equipo)
16. [Riesgos y preguntas abiertas](#16-riesgos-y-preguntas-abiertas)
17. [Anexos](#17-anexos)

---

## 1. INTRODUCCIÓN

### 1.1 Propósito

Este documento define, con el máximo nivel de detalle, las especificaciones funcionales y no funcionales de **ABDAudioLab**: una plataforma universal de ingeniería inversa y perfilado de hardware musical (sintetizadores, módulos, pedales de efectos). El sistema inyecta estímulos de audio controlados en un hardware físico, analiza la señal de retorno y extrae el comportamiento del dispositivo en forma de tablas numéricas reutilizables por emuladores de software.

### 1.2 Alcance y Delimitación de Proyectos

> [!IMPORTANT]
> **DELIMITACIÓN ESTRICTA DE PROYECTOS:**
> 1. **ABDAudioLab (Este Proyecto)**: Es **única y exclusivamente la plataforma y robot de perfilado de hardware / laboratorio científico**. Su función es inyectar estímulos de audio, medir la respuesta acústica/eléctrica de cualquier hardware (Roland AIRA, Eurorack analógico, pedales) y exportar tablas LUT (.h/.json) para modelos DSP. Su interfaz (sea C++ o WebUI) es un **Panel de Control y Monitorización del Laboratorio** (progreso, gráficos de respuesta en frecuencia, logs y estado del robot).
> 2. **Roland AIRA Modular Customizer / Patch Editor (Proyecto Independiente)**: El editor gráfico completo de parches para la serie AIRA (con arrastre de cables, gestión de presets de usuario y CAD modular para músicos) será un **PROYECTO SEPARADO E INDEPENDIENTE**. No forma parte de este repositorio.

**Incluido en ABDAudioLab:**

- Aplicación de escritorio Windows: el Laboratorio (ejecutable nativo C++20/JUCE 8/CMake).
- Perfilado automatizado de hardware digital con control MIDI USB / SysEx.
- Perfilado asistido de hardware analógico (operador humano guiado para Eurorack).
- Exportación de resultados en formato `.h` (LUT `alignas(16)`) y `.json`.
- Consola y panel de control/monitorización del laboratorio.
- Soporte para los 4 módulos Roland AIRA Modular como primer perfil oficial de calibración.

**Excluido de ABDAudioLab (Pertenecen a otros proyectos):**

- **El Editor / Customizer interactivo de parches Roland AIRA** (Proyecto independiente).
- **El plugin VST3 emulador** (Fase 4 del proyecto padre — el laboratorio solo produce sus datos numéricos de entrada).
- Soporte macOS/Linux.
- Descifrado automático completo del bitstream FSK (el laboratorio consume el diccionario; su producción es una subtarea de calibración, sección 9.6).

### 1.3 Audiencia

Equipo de desarrollo **junior**. Este documento asume conocimientos básicos de C++, JavaScript y control de versiones con Git, pero **no** asume conocimientos previos de DSP, MIDI o síntesis modular: todos los conceptos de dominio están definidos en el glosario (sección 6) y en el anexo 17.

### 1.4 Cómo leer este documento

- Las secciones están numeradas jerárquicamente (ej. `10.3.2`). Las referencias cruzadas usan esa numeración.
- Los requisitos verificables llevan identificadores del tipo `RF-nn` (requisito funcional), `RNF-nn` (requisito no funcional) y `CA-nn` (criterio de aceptación).
- Los diagramas ASCII son parte normativa de la especificación.
- Los esquemas JSON son especificación de datos, no código de implementación.

---

## 2. VISIÓN DEL PRODUCTO Y ALCANCE

### 2.1 Visión

> ABDAudioLab es una plataforma universal de ingeniería inversa y perfilado de hardware musical: inyecta estímulos controlados en un hardware físico, escucha la respuesta y extrae su comportamiento en tablas numéricas reutilizables por cualquier emulador de software. No es una herramienta "para Roland": es una plataforma universal de perfilado de hardware musical.

### 2.2 Objetivos del producto

| ID | Objetivo | Métrica de éxito |
|---|---|---|
| O-1 | Extraer curvas de comportamiento de hardware musical de forma desatendida | Un perfil completo (1 parámetro, 16 pasos) en < 10 min sin intervención |
| O-2 | Capturar la inestabilidad orgánica del hardware (ruido térmico, ACB simulado) | σ medido con ≥ 16 ventanas por punto de prueba |
| O-3 | Ser agnóstico al hardware | Un mismo ejecutable perfila hardware digital (MIDI) y analógico (manual) sin recompilar |
| O-4 | Resultados listos para producción | Los `.h` generados compilan sin edición manual en un proyecto JUCE |

### 2.3 Fuera de alcance (esta versión)

- Editor configurador de parches completo (Fase 2 del proyecto padre). La WebUI del laboratorio solo controla y monitoriza el laboratorio.
- Plugin VST3 emulador (Fase 4 del proyecto padre). El laboratorio solo produce sus datos de entrada.
- Descifrado automático completo del bitstream FSK (sección 9.6 define la subtarea de calibración).
- Soporte macOS/Linux.

---

## 3. ARQUITECTURA GENERAL

### 3.1 Capas del sistema

```
        +----------------------------------------------+
        |            CAPA A — WEBUI (Navegador)        |
        |  Panel de control y monitorización del lab   |
        +----------------------+-----------------------+
                               | JSON (WebSocket, localhost)
                               v
        +----------------------------------------------------------+
        |                CAPA B — CORE NATIVO (C++20/JUCE 8)       |
        |                                                          |
        |  +--------------------+        +--------------------+    |
        |  | Secuenciador       |        | Motor Analítico    |    |
        |  | (máquina estados)  |------->| (hilo background)  |    |
        |  +--------+-----------+        +---------+----------+    |
        |           |                              ^               |
        |  +--------v-----------+        +---------+----------+    |
        |  | Generador Estímulos|        | Receptor de Audio  |    |
        |  | (hilo audio)       |        | (hilo audio)       |    |
        |  +--------+-----------+        +---------+----------+    |
        |           |                              ^               |
        |           v     FIFO lock-free           |               |
        |           +------------------------------+               |
        +-----------|------------------------------+---------------+
                    v                              |
        +----------------------------------------------------------+
        |              CAPA C — HARDWARE FÍSICO                    |
        |   Salida audio tarjeta ──► Entrada audio del hardware    |
        |   Salida audio hardware ──► Entrada audio tarjeta        |
        |   MIDI USB bidireccional (opcional, hardware digital)    |
        +----------------------------------------------------------+
```

### 3.2 Responsabilidades por capa

| Capa | Responsabilidad | Prohibido |
|---|---|---|
| **A — WebUI** | Renderizar estado, enviar acciones del usuario, monitorizar progreso | Toda lógica de negocio, cálculo de parámetros, acceso a hardware |
| **B — Core nativo** | Máquina de estados, generación de estímulos, captura, análisis, exportación | Renderizar, persistir estado de UI, lógica de presentación |
| **C — Hardware** | Convertir estímulos en audio y devolver audio procesado | — |

### 3.3 Decisiones de arquitectura (ADR resumido)

| ID | Decisión | Justificación |
|---|---|---|
| AD-1 | Ejecutable nativo único (no plugin) para el laboratorio | Acceso directo a ASIO; el plugin emulador es otro proyecto |
| AD-2 | Un solo motor de renderizado en WebUI (SVG) | Evita desfases de coordenadas entre motores duplicados (lección del proyecto previo) |
| AD-3 | JSON como única fuente de verdad del parche y del perfil de pruebas | Serialización trivial, legible por humanos e IAs |
| AD-4 | Zero-allocation en el hilo de audio | Evita glitches de audio (sección 13.1) |
| AD-5 | FIFO lock-free entre hilo de audio y hilo de análisis | El análisis pesado nunca bloquea el hilo de audio |
| AD-6 | Módulos de código "tontos" y puros | Cada módulo testeable y explicable de forma aislada (lección del proyecto previo) |
| AD-7 | Data-driven: el comportamiento del robot lo dicta `TestProfile.json` | Añadir hardware nuevo = añadir JSON, sin recompilar |

---

## 4. ENTORNO TÉCNICO Y STACK

### 4.1 Stack

| Componente | Elección | Versión | Notas |
|---|---|---|---|
| Lenguaje | C++ | C++20 | Estándar obligatorio (`CMAKE_CXX_STANDARD 20`) |
| Framework de app/audio | JUCE | 8.0.4 | Vía `FetchContent` de CMake |
| Sistema de build | CMake | ≥ 3.22 | Generador Visual Studio 2022 |
| SO objetivo | Windows 11 | — | Driver Roland Win10 compatible |
| Driver de audio | ASIO (exclusivo) | — | Fallback DirectSound solo para desarrollo |
| Comunicación WebUI | WebSocket | — | `localhost:8080` (configurable) |
| Control hardware | MIDI USB | — | Puertos nativos JUCE |
| Librería matemática externa | chowdsp_utils | main | Interpolación bilineal/bicúbica de LUTs |
| Tasa de muestreo objetivo | 96 kHz | — | 24 o 32-bit float; fallback 48 kHz |
| Control de versiones | Git | — | Flujo descrito en sección 15 |

### 4.2 Entorno de desarrollo

- Windows 11, Visual Studio 2022 (generador CMake).
- CMake ≥ 3.22.
- Tarjeta de sonido con ASIO y entrada/salida de línea.
- Cable de audio minijack (salida tarjeta → entrada hardware; salida hardware → entrada tarjeta).
- Cable USB A-B para MIDI (hardware digital).
- Hardware de prueba: 1 módulo Roland AIRA Modular (basta uno; los cuatro comparten cerebro, ver 7.1).

### 4.3 Inicialización del AudioDeviceManager y Cadena de Fallback (Golden Pattern)

Para evitar fallos silenciosos de inicialización del driver ASIO/WASAPI bajo Windows (donde `deviceManager.getCurrentAudioDevice()` queda en `nullptr` a pesar de compilar correctamente):

1. **Cadena jerárquica de 3 pasos en el arranque**:
   - Paso 1: Intentar restaurar la configuración XML guardada previamente: `deviceManager.initialise(2, 2, xml.get(), true)`.
   - Paso 2: Si el dispositivo sigue siendo nulo, intentar dispositivos por defecto: `deviceManager.initialiseWithDefaultDevices(2, 2)`.
   - Paso 3: Fallback explícito: `deviceManager.initialise(2, 2, nullptr, true)`.
2. **Persistencia de estado**: Guardar el XML de configuración activa al cerrar la aplicación o cambiar de dispositivo (`deviceManager.createStateXml()`).
3. **Selector interactivo**: Integrar `juce::AudioDeviceSelectorComponent` en el menú/modal de configuración para que el usuario pueda conmutar drivers ASIO, tamaño de búfer y mapeo de canales de entrada/salida de forma interactiva.
4. **Tono de diagnóstico atómico (Test Tone)**: Incluir un oscilador de prueba a 440 Hz / 1 kHz gobernado por un flag atómico (`std::atomic<bool> enableTestTone_`) inyectado directamente antes de la salida física para verificar inmediatamente que el DAC/tarjeta emite sonido independientemente del secuenciador o del hardware conectado.

---

## 5. ESTRUCTURA DE CARPETAS DEL PROYECTO

```
ABDAudioLab/
├── CMakeLists.txt                  # Configuración central (sección 4)
├── .gitignore                      # Excluye build/, LUTs pesadas (ver 15.3)
├── ESPECIFICACIONES_LABORATORIO.md # Este documento
├── cmake/                          # Scripts CMake auxiliares
├── data/                           # Perfiles de prueba (JSON, sección 10.1)
├── exported_luts/                  # Salida del robot (.h y .json)
├── modules/                        # Dependencias (FetchContent las rellena)
└── src/
    ├── main.cpp                    # Punto de entrada, máquina de estados
    ├── audio/                      # Hilo de audio
    │   ├── LabStimulusGenerator.h  # Generador de estímulos (10.3)
    │   └── LabAudioReceiver.h      # Receptor con threshold trigger (10.4)
    ├── math/                       # Hilo background
    │   └── LabAnalyticEngine.h     # Motor analítico (sección 11)
    ├── hardware/                   # Contratos y controladores
    │   ├── HardwareController.h    # Contrato base (10.7)
    │   ├── MidiCcController.h      # Implementación MIDI USB
    │   └── ManualAnalogueController.h # Implementación manual guiada
    └── network/                    # WebSocket server
        └── WebUiWebSocketServer.h
```

**Regla de dependencias (obligatoria):** `audio/` no depende de `math/` ni de `network/`. La comunicación entre `audio/` y `math/` es exclusivamente la FIFO lock-free (13.2). `network/` solo habla con la máquina de estados de `main.cpp`.

---

## 6. GLOSARIO

| Término | Definición |
|---|---|
| **ACB** | *Analog Circuit Behavior*. Tecnología de Roland que emula el comportamiento de circuitos analógicos (deriva térmica, ruido de componentes) en su DSP. En este proyecto, cualquier inestabilidad dinámica intencionada del hardware se trata como "comportamiento tipo ACB". |
| **Baudios** | Velocidad de transmisión del flujo FSK (bits por segundo). |
| **Bin (FFT)** | Cada una de las bandas de frecuencia en las que la FFT divide el espectro. |
| **Bitstream** | Flujo de bytes serializado que representa un parche. |
| **Bloque funcional** | Clasificación de un parámetro según qué se mide de él (sección 10.2). |
| **Catenaria** | Curva que describe un cable colgante; se usa para dibujar los cables virtuales. |
| **FIFO lock-free** | Cola circular sin bloqueos entre hilos; en JUCE, `AbstractFifo`. |
| **FSK** | *Frequency Shift Keying*: codificación de datos conmutando entre dos frecuencias audibles (una para 0, otra para 1). |
| **GRF knobs** | Las 6 perillas grandes retroiluminadas del panel frontal de los módulos AIRA. |
| **Hann (ventana)** | Función de suavizado aplicada antes de la FFT para evitar el emborronamiento espectral. |
| **Jack** | Conector de entrada o salida de un submódulo virtual. |
| **LUT** | *Look-Up Table*: tabla de consulta que mapea valor de perilla → valor físico (ms, Hz). |
| **Parche** | Estado completo de la configuración del hardware: slots, cables y mapeos físicos. |
| **Perfil** | Resultado del análisis de un hardware: el conjunto de tablas extraídas. |
| **Slot** | Cada una de las 6 posiciones donde se puede cargar un submódulo virtual. |
| **SNR** | *Signal-to-Noise Ratio*: relación señal/ruido, en dB. |
| **Submódulo** | Bloque virtual (LFO, filtro, ADSR…) cargable en un slot. |
| **THD** | *Total Harmonic Distortion*: distorsión armónica total, en %. |
| **Zero-allocation** | Prohibición de reservas de memoria dinámica en el hilo de audio. |

---

## 7. DOMINIO: HARDWARE, SUBMÓDULOS Y EFECTOS BASE

### 7.1 Uniformidad del hardware

| ID | Especificación |
|---|---|
| RF-01 | El sistema tratará los 4 módulos AIRA (Torcido, Bitrazer, Demora, Scooper) como el mismo hardware lógico: mismo procesador, mismo firmware, mismos 6 slots, mismos 31 submódulos, misma entrada REMOTE IN, mismos puertos MIDI USB. |
| RF-02 | El byte de identificación de un submódulo en el bitstream FSK será idéntico cualquiera que sea el módulo físico destino. |
| RF-03 | El único dato específico por módulo físico será el **ID de cabecera** del parche (indica al firmware el modelo destino y cómo mapear los GRF knobs). |
| RF-04 | El perfilado de los 31 submódulos se podrá realizar íntegramente con un solo módulo físico. |
| RF-05 | Cada módulo ejecuta además un **efecto base** permanente (no borrable) que en parche vacío conecta entradas físicas → efecto base → salidas (ver 7.4). |

### 7.2 Catálogo de submódulos virtuales (31 total)

Catálogo cerrado y fijo por el firmware v1.50 de Roland. El usuario solo puede cargar lo que el hardware soporta; el catálogo es idéntico en los 4 módulos. Los 31 submódulos están confirmados contra la página oficial de Roland del Customizer y la cobertura de lanzamiento (Roland US, Sound on Sound, Matrixsynth): LFO, ADSR, NOISE, SAMPLE & HOLD, RING MOD, FILTER 6 dB, FILTER 12 dB, TONE, AMP, MIXER, STEREO MIXER, CURVE CONV, GATE DIVIDER, TRIG TO CV DELAY TIME, MIDI CLOCK TO GATE (los 15 de lanzamiento) más los añadidos hasta v1.50.

**Generadores y osciladores (5):**

| # | Submódulo | Tipo de bloque (10.2) | Salidas conocidas | Entradas conocidas |
|---|---|---|---|---|
| 1 | LFO | CyclicModulator | 3 (Sine Out, Triangle Out, Saw Out — CV) | 0 |
| 2 | Sample & Hold | CyclicModulator | 2 (S&H Out 1, S&H Out 2 — audio procesado por sample & hold) | 2 (Signal In, Trigger In) |
| 3 | Noise Generator | Generador | 1 (Noise Out) | 0 |
| 4 | Saw Oscillator | Generador | 1 (Saw Out) | 1 (Sync/Reset In) |
| 5 | Sqr Oscillator | Generador | 1 (Square Out) | 1 (Sync/Reset In) |

**Filtros y modelado (7):**

| # | Submódulo | Tipo de bloque | Notas |
|---|---|---|---|
| 6 | Filter −6 dB | SpectrumFilter | Submódulo propio (el par −6/−12 dB del Customizer son dos entradas del catálogo) |
| 7 | Filter −12 dB | SpectrumFilter | — |
| 8 | Filter 18 dB | SpectrumFilter | — |
| 9 | Filter 24 dB | SpectrumFilter | — |
| 10 | Formant Filter | SpectrumFilter | — |
| 11 | Tone | SpectrumFilter | 2 (Tone Out 1, Tone Out 2 — audio con el tono modificado) |
| 12 | Tube Clip | WaveShaper | Saturación tipo válvula |

**Dinámica y tiempo (5):**

| # | Submódulo | Tipo de bloque | Notas |
|---|---|---|---|
| 13 | ADSR | TimeDynamic | Envolvente clásica 4 etapas |
| 14 | Enveloper | TimeDynamic | Envelope follower |
| 15 | Compressor | AmplitudeGain | — |
| 16 | Noise Gate | AmplitudeGain | — |
| 17 | Short Delay | TimeDynamic | — |

**Utilidades y ruteo (6):**

| # | Submódulo | Tipo de bloque | Notas |
|---|---|---|---|
| 18 | Mixer | AmplitudeGain | Suma de entradas |
| 19 | Cross Fader | AmplitudeGain | — |
| 20 | Switcher | Utilidad de ruteo | — |
| 21 | 3 Band EQ | SpectrumFilter | — |
| 22 | VCA | AmplitudeGain | Amplificador controlado por voltaje |
| 23 | Logic Operation | Utilidad de lógica | Operaciones AND, OR, XOR, NOT sobre señales de control |

**Utilidades de conversión y sincronización (8):**

| # | Submódulo | Tipo de bloque | Notas |
|---|---|---|---|
| 24 | Ring Mod | AmplitudeGain | Multiplicador de dos señales (señal × portadora); 2 entradas, 1 salida |
| 25 | AMP | AmplitudeGain | Amplificador simple de volumen; 1 entrada, 1 salida |
| 26 | Stereo Mixer | AmplitudeGain | Mezclador estéreo; 2 entradas estéreo, 1 salida estéreo |
| 27 | Curve Conv | Utilidad de conversión | Conversor de curvas de respuesta CV (ej. lineal→exponencial); 1 entrada CV, 1 salida CV |
| 28 | Gate Divider | Utilidad de conversión | Divisor de señal de gate; 1 entrada gate, 1 salida gate |
| 29 | Trig to CV Delay Time | Conversor de control | Convierte trigger en CV para controlar el tiempo de delay; 1 entrada trigger, 1 salida CV |
| 30 | MIDI Clock to Gate | Conversor de control | Sincroniza al MIDI clock por USB y emite gate; 1 salida gate |
| 31 | MIDI Note to CV/Gate | Conversor de control | Permite usar el módulo como sintetizador Eurorack autónomo vía notas MIDI por USB |

**Verificación de recuento:** 5 (generadores) + 7 (filtros) + 5 (dinámica) + 6 (ruteo) + 8 (conversión) = **31** ✓

> **Nota para desarrollo:** cada submódulo tendrá un **ID numérico fijo (1 byte)** y un número fijo de entradas/salidas definido de fábrica. Ambos datos se descubren en la captura FSK diferencial (9.6) y se registran en el diccionario (anexo 17.2). El validador de la WebUI (12.4) consume ese diccionario.

### 7.3 Técnicas de diseño de parches (contexto de producto)

El catálogo es cerrado: no se puede programar DSP nuevo en el hardware. El valor del producto es **liberar el potencial oculto** combinando los bloques existentes. Ejemplo canónico: una envolvente de 8 etapas se construye encadenando 2 ADSR (disparando el segundo cuando el primero termina su fase de decay/sustain) y sumando sus salidas con el Mixer, usando Logic Operation para la sincronización. La WebUI no debe impedir estas composiciones: solo debe impedir conexiones **físicamente imposibles** (12.4).

### 7.4 Efectos base (4)

Cada módulo ejecuta un **efecto base permanente** ("submódulo maestro" no borrable). En un parche vacío de fábrica: entradas físicas → efecto base → salidas. El Customizer permite interceptar ese cableado insertando los 6 slots.

| ID | Especificación |
|---|---|
| RF-06 | El efecto base se modelará como un "submódulo maestro permanente" presente siempre, no borrable, con sus propias entradas y salidas. |
| RF-07 | En un parche vacío, el ruteo por defecto será: entradas físicas → efecto base → salidas físicas. |
| RF-08 | El laboratorio podrá perfilar los efectos base enviando un parche estructuralmente vacío (0 submódulos en los 6 slots) y verificando por MIDI CC que el efecto no esté en bypass. |

Las especificaciones de estímulo y análisis por efecto base están en el anexo 17.3.

### 7.5 GRF knobs

Las 6 perillas grandes retroiluminadas del panel frontal. Su asignación (qué parámetro interno controla cada perilla) es dinámica y depende del parche cargado; forma parte del modelo de datos del parche (8.2, campo `knob_map`).

---

## 8. MODELO DE DATOS DEL PARCHE

### 8.1 Principios

| ID | Especificación |
|---|---|
| RF-09 | El parche se representará como un único objeto JSON plano, independiente de JUCE y de la WebUI. Es la **única fuente de la verdad**. |
| RF-10 | No se guardarán posiciones en píxeles ni ningún dato de renderizado en el parche. Solo coordenadas lógicas (`slot`, `jack`). |
| RF-11 | Un único motor de datos y un único serializador servirán para los 4 módulos; lo único específico por módulo es el ID de cabecera (RF-03). |

### 8.2 Esquema del parche

```json
{
  "hardware_id": "bitrazer",
  "slots": [
    { "index": 0, "module_id": "LFO",         "params": [64, 32] },
    { "index": 1, "module_id": "FILTER_24DB", "params": [127, 0] },
    { "index": 2, "module_id": null,          "params": [] },
    { "index": 3, "module_id": null,          "params": [] },
    { "index": 4, "module_id": null,          "params": [] },
    { "index": 5, "module_id": null,          "params": [] }
  ],
  "connections": [
    { "from": "slot_0.out_wave", "to": "slot_1.in_cutoff" }
  ],
  "knob_map": {
    "grf_1": "slot_1.param_0",
    "grf_2": null,
    "grf_3": null,
    "grf_4": null,
    "grf_5": null,
    "grf_6": null
  }
}
```

### 8.3 Reglas del esquema

| Campo | Regla |
|---|---|
| `hardware_id` | Uno de: `torcido`, `bitrazer`, `demora`, `scooper`. Determina el byte de cabecera del bitstream (RF-03). |
| `slots` | Exactamente 6 elementos. `index` ∈ [0, 5]. `module_id` ∈ catálogo (7.2) o `null` (slot vacío). `params` es el array de valores internos del submódulo (0–127 por parámetro); vacío si el slot está vacío. |
| `connections` | Lista plana. `from` y `to` son direcciones lógicas con formato `slot_N.<jack>` o `hardware.<jack>` (entradas/salidas físicas). Un elemento conecta exactamente una salida con una entrada. |
| `knob_map` | Mapa fijo de 6 claves (`grf_1`…`grf_6`). Cada valor es una referencia a un parámetro interno (`slot_N.param_M`) o `null` (perilla sin asignar). |

---

## 9. ESPECIFICACIÓN DEL PROTOCOLO FSK

### 9.1 Concepto

FSK (*Frequency Shift Keying*): los bytes del parche se codifican conmutando una portadora entre **dos frecuencias audibles puras** (una representa 0, otra representa 1). El hardware tiene un demodulador interno que recompone los bytes y reconfigura sus DSPs. La transmisión es **unidireccional** (JUCE → hardware).

### 9.2 Parámetros a descubrir por calibración

| Parámetro | Símbolo | Método de obtención |
|---|---|---|
| Frecuencia para bit 0 | f0 | Análisis espectral de un `.wav` de fábrica |
| Frecuencia para bit 1 | f1 | Ídem |
| Baudios | — | Medición de duración de bit en el `.wav` |
| Estructura de trama (cabecera, payload, checksum si existe) | — | Captura diferencial (9.6) |

### 9.3 Flujo de transmisión (JUCE → hardware)

| Paso | Acción |
|---|---|
| 1 | La WebUI genera el JSON del parche (8.2). |
| 2 | El serializador lo traduce a un bloque de bytes según el diccionario Roland. |
| 3 | El modulador FSK convierte los bits en audio (bit 0 → f0, bit 1 → f1) en el hilo de audio. |
| 4 | El audio se emite por la salida de la tarjeta asignada al `REMOTE IN` del módulo. |
| 5 | La UI muestra progreso y bloquea edición hasta terminar la transmisión. |

| ID | Especificación |
|---|---|
| RF-12 | La transmisión FSK será **unidireccional y "ciega"**: el hardware no devuelve confirmación por audio. |
| RF-13 | Durante una transmisión FSK la UI bloqueará la edición del parche. |
| RF-14 | El módulo puede congelarse brevemente mientras reconfigura sus DSPs tras recibir un parche; la UI lo comunicará como estado "Reconfigurando…". |

### 9.4 Especificación del modulador

- Un bit 0 se emite como onda senoidal pura a f0; un bit 1, a f1. Fase continua entre bits.
- La modulación ocurre en el hilo de audio (en el callback de procesamiento), nunca en un hilo secundario.
- La señal se emite por la salida de la tarjeta asignada al `REMOTE IN`.

### 9.5 Lectura y Configuración vía MIDI SysEx (Bidireccionalidad Confirmada)

A diferencia del canal de audio FSK (que es unidireccional y ciego), los módulos Roland AIRA Modular **sí disponen de implementación SysEx bidireccional nativa por USB** (documentada por ingeniería inversa en [AIRA_Modular_Effects-master](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/google%20ia%20research/AIRA_Modular_Effects-master/README.md)). 

Mediante los comandos SysEx Roland `RQ1` (Data Request 1) y `DT1` (Data Set 1), el laboratorio y editor pueden:
1. **Volcar el estado interno actual (Dump Request)** sin necesidad de partir de un estado ciego.
2. **Configurar submódulos y cables virtuales de forma instantánea** vía USB sin esperar la modulación analógica de audio.

### 9.6 Subtarea de calibración FSK (captura diferencial)

Procedimiento normalizado (para el canal de audio FSK analógico por `REMOTE IN`):

| Paso | Acción | Artefacto |
|---|---|---|
| 1 | Parche vacío (0 submódulos, solo conexiones nativas) → Transfer → grabar a 48/96 kHz 24-bit | `patch_vacio.wav` |
| 2 | Añadir solo un LFO en Slot 1, sin cables → grabar | `modulo_LFO_slot1.wav` |
| 3 | Sustituir por un ADSR en Slot 1 → grabar | `modulo_ADSR_slot1.wav` |
| 4 | Slot 1 vacío, LFO en Slot 2 → grabar | `modulo_LFO_slot2.wav` |
| 5 | Parche vacío + un cable virtual entrada L → salida L → grabar | `cable_inL_outL.wav` |

Reglas de análisis:
- La comparación `patch_vacio` vs `modulo_X_slot1` revela el bloque de bytes que carga el submódulo X (su ID).
- La comparación con `modulo_X_slot2` revela cómo codifica el índice de slot.
- La comparación con `cable_inL_outL` revela el bloque de codificación de cables.
- Herramientas auxiliares de demodulación y análisis FSK disponibles en [alltheFSKs-master](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/google%20ia%20research/alltheFSKs-master/README.md).

### 9.7 Especificación del Protocolo Roland SysEx (RQ1 / DT1)

*(Fuente normativa: [AIRA_Modular_Effects-master/README.md](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/google%20ia%20research/AIRA_Modular_Effects-master/README.md))*

#### 9.7.1 Estructura de Trama SysEx Roland
* **Data Request 1 (RQ1)**: `F0 41 10 00 00 00 [ModelID] 11 aa bb cc dd ss tt uu vv [sum] F7`
* **Data Set 1 (DT1)**: `F0 41 10 00 00 00 [ModelID] 12 aa bb cc dd [data...] [sum] F7`
* **Model IDs**: `15H` (Bitrazer), `16H` (Demora), `17H` (Torcido), `18H` (Scooper).

#### 9.7.2 Mapa de Direcciones SysEx
| Dirección | Función | Rango / Datos |
|---|---|---|
| `10 00 00 01` .. `08` | Parámetros del Módulo Principal (Main Module) | `0..127` (LPF/HPF, Bypass, Sample Rate, Cutoff, etc.) |
| `10 10 00 00` | Submódulo Slot 1: Tipo | `00H` (Empty) a `1FH` (31 submódulos, ver 17.2) |
| `10 10 00 01` .. `04` | Submódulo Slot 1: Parámetros 1 a 4 | `0..127` |
| `10 10 00 05` .. `09` | Submódulo Slot 2: Tipo y Parámetros 1 a 4 | — |
| `10 10 00 0A` .. `0E` | Submódulo Slot 3: Tipo y Parámetros 1 a 4 | — |
| `10 10 00 0F` .. `13` | Submódulo Slot 4: Tipo y Parámetros 1 a 4 | — |
| `10 10 00 14` .. `18` | Submódulo Slot 5: Tipo y Parámetros 1 a 4 | — |
| `10 10 00 19` .. `1D` | Submódulo Slot 6: Tipo y Parámetros 1 a 4 | — |
| `10 20 [ss] [dd]` | **Ruteo de Cable Virtual**: conecta fuente `ss` con destino `dd` | `0` = Desconectar, `1` = Conectar |
| `10 21 00 00` .. `1D` | Atenuación / Condición de Cable Virtual | `0..127` |

*Fuentes (`ss`): `00..01` Input 1-2, `02..07` GRF 1-6, `08..09` Main Out 1-2, `0A..15` Submodule 1-6 Outs 1-2.*
*Destinos (`dd`): `00..01` Output 1-2, `02..09` Main In 1-8, `0A..21` Submodule 1-6 Ins 1-4.*

---

## 10. ESPECIFICACIÓN DEL LABORATORIO

### 10.1 Perfil de pruebas: `TestProfile.json`

Todo escaneo se inicializa con un perfil JSON estricto. Es el **mapa de ruta del secuenciador** y la única fuente de verdad de la sesión.

```json
{
  "profile_metadata": {
    "hardware_name": "ROLAND_BITRAZER_SUBMOD_FILTER",
    "operator_author": "nombre_del_operador",
    "hardware_type": "Digital_Advanced_Mode"
  },
  "global_settings": {
    "target_sample_rate": 96000,
    "calibration_loopback_required": true,
    "periodic_noise_floor_interval_minutes": 5,
    "confidence_threshold_snr_db": 18.0
  },
  "test_dimensions": [
    {
      "parameter_name": "Cutoff",
      "index": 0,
      "block_type": "SpectrumFilter",
      "stimulus_variation": "LogFarinaSweep",
      "quantized_steps": 16
    },
    {
      "parameter_name": "Resonance",
      "index": 1,
      "block_type": "SpectrumFilter",
      "stimulus_variation": "PinkNoise",
      "quantized_steps": 16
    }
  ]
}
```

| Campo | Regla |
|---|---|
| `profile_metadata.hardware_name` | Identificador del hardware/perfil; nombra los archivos de salida. |
| `global_settings.target_sample_rate` | Tasa objetivo del escaneo (96000 por defecto). |
| `global_settings.calibration_loopback_required` | Si es `true`, obliga a ejecutar la calibración de línea (10.8) antes de cualquier prueba. |
| `global_settings.periodic_noise_floor_interval_minutes` | Cada cuántos minutos se ejecuta el interludio de ruido (10.7). Por defecto 5. |
| `global_settings.confidence_threshold_snr_db` | Umbral SNR mínimo para aceptar una medición (11.4). Por defecto 18.0 dB. |
| `test_dimensions[]` | Cada elemento es una dimensión del producto cartesiano de pruebas: nombre, índice, tipo de bloque (10.2), variación de estímulo (10.3.2) y número de pasos cuantizados (típicamente 16). |

### 10.2 Tipos de bloque y matriz de estímulo/análisis

Cada parámetro a perfilar se clasifica en uno de **5 tipos de bloque**. La matriz siguiente es normativa: define estímulo, analizador y qué mide cada estadístico.

| Tipo de bloque | Estímulo de salida | Analizador de entrada | Qué mide μ | Qué mide σ |
|---|---|---|---|---|
| **TimeDynamic** (ADSR, Delays, Ecos, Attack/Release) | Impulso único (Dirac Delta) | Seguidor de amplitud + contador de samples | Tiempo exacto en ms (Attack, Delay Time) | Wow/flutter, fluctuación de reloj |
| **SpectrumFilter** (Filtros, EQs, Formant) | Ruido blanco continuo o LogFarinaSweep | Ventanas deslizantes de FFT | Frecuencia de corte/resonancia en Hz | Bamboleo térmico |
| **AmplitudeGain** (VCA, Compressor, Gate, Mixer) | Tono senoidal continuo a 1 kHz | Detector de amplitud de pico (Peak/RMS) | Nivel de atenuación/ganancia en dB | Ruido de fondo |
| **WaveShaper** (Distorsión, Tube Clip) | Rampa de amplitud lineal (0→1) | Análisis de THD | Curva de transferencia estática | Asimetría de saturación |
| **CyclicModulator** (LFO, Chorus, Vibrato) | Tono senoidal o ruido blanco | FFT de larga duración | Velocidad y profundidad del LFO interno en Hz | Irregularidad del oscilador |

### 10.3 Generador de estímulos

#### 10.3.1 Modos de estímulo

| Modo | Definición | Uso |
|---|---|---|
| `Silence` | Amplitud 0.0 constante | Limpieza entre pruebas, interludio de ruido |
| `DiracDelta` | Un único sample a amplitud 1.0, luego silencio absoluto | TimeDynamic (attack, delay, eco) |
| `SyncPulses3` | Secuencia de 3 ráfagas senoidales a 1 kHz / pulsos espaciados | Pre-Roll de sincronización, marca de inicio de grabación, alineación de fase y calibración de latencia |
| `WhiteNoise` | Aleatorio uniforme [−1, 1] por muestra | SpectrumFilter (espectro plano) |
| `PinkNoise` | Ruido con energía decreciente 3 dB/octava | SpectrumFilter (variación por defecto de filtros) |
| `SineWave1kHz` | Senoidal pura continua a 1 kHz | AmplitudeGain (VCA, comp, gate) |
| `SquareWave1kHz` | Onda cuadrada a 1 kHz | Variación para medir respuesta no lineal |
| `LogFarinaSweep` | Senoidal con frecuencia exponencial 20 Hz → Nyquist en tiempo fijo | SpectrumFilter (respuesta completa en una toma) |
| `AmplitudeRamp` | Rampa lineal de amplitud 0→1 | WaveShaper (curva de transferencia, THD) |

#### 10.3.2 Reglas del generador

| ID | Especificación |
|---|---|
| RF-15 | La conmutación de estímulo será instantánea y síncrona con el bloque de audio. |
| RF-16 | Antes de cada cambio de estímulo, el secuenciador forzará `Silence` para que la siguiente prueba empiece limpia. |
| RF-17 | El generador no conocerá nada de Roland, MIDI ni WebSockets: solo obedece al modo de estímulo que recibe. |
| RF-18 | El modo de estímulo por defecto de cada tipo de bloque es el de la matriz 10.2; el perfil puede sobreescribirlo con `stimulus_variation`. |
| RF-19 | Prohibida toda asignación de memoria en el hilo de audio (RNF-1). |
| RF-19b | **Protección contra Denormales**: Aplicar `juce::ScopedNoDenormals` al inicio de cada callback de audio para evitar picos de CPU por números subnormales en colas de decaimiento y silencios. |
| RF-19c | **Seguridad de Entropía y Generadores de Ruido**: Prohibido el constructor por defecto de `juce::Random` en hilos de audio y futuros ports WASM (busca entropía del sistema en SO/navegador y causa bloqueos). Todo generador de ruido se instanciará con semilla explícita fija (ej. `juce::Random noiseGen(12345)`) o mediante un generador lineal congruencial inline (LCG). |

### 10.4 Receptor de audio

| ID | Especificación |
|---|---|
| RF-20 | Dos modos de captura: **CaptureTimeWindow** (dispara por umbral de amplitud, cuenta samples; para TimeDynamic) y **CaptureSpectralBlock** (captura un bloque fijo, p. ej. 2048 o 4096 samples; para SpectrumFilter). |
| RF-21 | El umbral de disparo por defecto será −40 dBfs, ajustable por perfil. |
| RF-22 | El receptor escribirá en un búfer de análisis preasignado (capacidad ≥ 5 s a la tasa objetivo). |
| RF-23 | Al completar la captura, el receptor notificará al secuenciador (flag `captureComplete` o equivalente) y entregará el búfer al motor analítico **vía FIFO lock-free**, nunca por referencia compartida mutable. |
| RF-24 | El receptor no ejecutará ningún cálculo matemático en el hilo de audio. |

### 10.5 Validador de ruteo (límites del hardware)

| ID | Especificación |
|---|---|
| RF-25 | El validador bloqueará cualquier conexión imposible para el hardware (ej. salida→salida) **antes** de que llegue al core. |
| RF-26 | El validador consumirá el diccionario de submódulos (número fijo de entradas/salidas por submódulo, 7.2/anexo 17.2). |

### 10.6 Requisitos de rendimiento del laboratorio

| ID | Requisito |
|---|---|
| RNF-1 | **Zero-allocation** en el hilo de audio: prohibido `new`, `resize`, cualquier reserva dinámica en el callback de audio. Toda memoria preasignada en la fase de preparación. |
| RNF-2 | Comunicación hilo de audio → análisis exclusivamente vía **FIFO lock-free** (`AbstractFifo`). |
| RNF-3 | Tasa objetivo 96 kHz, 24 o 32-bit float; fallback 48 kHz. |
| RNF-4 | Búfer de análisis preasignado con capacidad ≥ 5 s a la tasa objetivo. |
| RNF-5 | Alineación SIMD de las matrices exportadas: `alignas(16)`. |
| RNF-6 | Throttling WebUI→core: máximo un mensaje cada 10–15 ms. |
| RNF-14 | `juce::ScopedNoDenormals` activo en cada bloque de procesamiento de audio. |
| RNF-15 | Generación de ruido con semilla determinista / LCG, sin llamadas de entropía del SO en tiempo real. |

### 10.7 Interludio de ruido de fondo (periódico)

| Parámetro | Valor por defecto |
|---|---|
| Frecuencia | Cada 5 minutos (configurable por perfil) |
| Espera previa | 500 ms en silencio (limpieza de ecos) |
| Ventana de captura | 500 ms |
| Métricas extraídas | RMS global del ruido + huella espectral (perfil FFT) |
| Persistencia | Array cronológico lineal (mapea la deriva térmica del rack) |

Durante el interludio: generador en `Silence`, secuenciador pausado.

### 10.8 Calibración de línea (loopback)

Antes de cualquier prueba (si `calibration_loopback_required: true`):

| Paso | Acción |
|---|---|
| 1 | Conectar físicamente salida de tarjeta → entrada de tarjeta (loopback). |
| 2 | Inyectar estímulo de referencia. |
| 3 | Medir la respuesta de la propia tarjeta (error base: latencia, coloración, ruido). |
| 4 | Guardar la calibración; las mediciones posteriores se interpretarán teniendo en cuenta este error base. |

### 10.9 Gestión de errores y casos límite

| ID | Especificación |
|---|---|
| RF-27 | Si el puerto MIDI no existe o se desconecta a mitad de prueba, el secuenciador entrará en estado `PAUSE_SAFE` (10.10), guardará el progreso en un JSON temporal y la WebUI mostrará un aviso amable sugiriendo el modo alternativo (audio FSK / manual). |
| RF-28 | Ninguna asunción de existencia de dispositivos: todo acceso a puertos será comprobado antes de usarse. |
| RF-29 | Si una medición no alcanza el umbral SNR (11.4), se reintenta; si falla repetidamente, se pausa y se registra. |

### 10.10 Máquina de estados

Ver sección 10.10 del diagrama en 11.5. Estados: `IDLE`, `LINE_CALIBRATION`, `INITIATE_TEST_CASE`, `WAIT_FOR_STABILIZATION`, `WAITING_FOR_OPERATOR`, `INJECT_STIMULUS`, `CAPTURE_AND_ANALYZE`, `INTERLUDE_NOISE_FLOOR`, `PAUSE_SAFE`, `EXPORT_DATA_AND_CLEANUP`. Transiciones normativas en 11.5.

---

## 11. ESPECIFICACIÓN DEL MOTOR ANALÍTICO

### 11.1 Ubicación y concurrencia

- Clase `LabAnalyticEngine` en `src/math/`.
- Se ejecuta en **hilo background de baja prioridad**. Nunca en el hilo de audio.
- Recibe datos exclusivamente vía FIFO lock-free (RF-23, RNF-2).

### 11.2 Algoritmo A — Detector espectral (SpectrumFilter)

| Paso | Operación |
|---|---|
| 1 | Segmentar la captura en ventanas de tamaño potencia de 2 (por defecto 2048), solapamiento 50%. |
| 2 | Aplicar **ventana de Hann** a cada bloque antes de la transformada. |
| 3 | Ejecutar FFT (`juce::dsp::FFT`, `performFrequencyOnlyForwardTransform`). |
| 4 | Localizar el bin de máxima energía → frecuencia de pico: `f = peakBin × sampleRate / fftSize`. |
| 5 | Acumular la frecuencia de pico de cada ventana en la serie temporal `peakFrequenciesInTime`. |
| 6 | Calcular μ y σ de la serie (11.4). |

Resultado esperado: filtro estable → serie constante → σ = 0. ACB simulando bamboleo → σ > 0 cuantifica el "temblor" (ej. media 1249 Hz, σ 4.8 Hz).

### 11.3 Algoritmo B — Detector temporal (TimeDynamic)

| Paso | Operación |
|---|---|
| 1 | Encontrar el pico absoluto de amplitud de la captura. |
| 2 | Localizar la muestra exacta en que la envolvente cruza el umbral de estabilización (por defecto 95% del pico estable). |
| 3 | Convertir muestras → tiempo: `ms = samples / sampleRate × 1000`. |
| 4 | Para σ (wow/flutter): repetir con múltiples disparos y calcular σ de los tiempos. |

### 11.4 Cálculo estadístico (normativo)

- μ = media aritmética de la serie de mediciones del punto de prueba.
- σ = desviación estándar poblacional de la serie.
- **Umbral de confianza:** si el SNR de la medición < `confidence_threshold_snr_db` (18.0 dB por defecto), la medición no es válida: se reintenta; si falla repetidamente, se pausa y se registra (RF-29).

### 11.5 Máquina de estados (normativa)

```
[ IDLE ]
   │  (carga TestProfile.json + HardwareController)
   ▼
[ LINE_CALIBRATION ]  (si calibration_loopback_required; ver 10.8)
   ▼
[ INITIATE_TEST_CASE (X, Y) ]
   ├─ automático ──► [ WAIT_FOR_STABILIZATION ] ──► [ INJECT_STIMULUS ]
   └─ manual ──────► [ WAITING_FOR_OPERATOR ] ──► (confirmación) ──► [ INJECT_STIMULUS ]
                                                                          ▼
                                                                 [ CAPTURE_AND_ANALYZE ]
                                                                          ▼
                                             [ ¿toca interludio de ruido? ]
                                                ├─ sí ──► [ INTERLUDE_NOISE_FLOOR ] ──┐
                                                └─ no ────────────────────────────────┤
                                                                                      ▼
                                                             [ ¿quedan combinaciones? ]
                                                                ├─ sí ──► [ INITIATE_TEST_CASE ]
                                                                └─ no ──► [ EXPORT_DATA_AND_CLEANUP ] ──► [ IDLE ]

En cualquier estado: desconexión de dispositivo o fallo SNR repetido ──► [ PAUSE_SAFE ]
```

| Estado | Descripción | Salida |
|---|---|---|
| `IDLE` | Esperando perfil | — |
| `LINE_CALIBRATION` | Loopback de la tarjeta (10.8) | Calibración guardada |
| `INITIATE_TEST_CASE` | Posiciona perillas del caso (X, Y) | Caso activo |
| `WAIT_FOR_STABILIZATION` | Espera activa hasta reposo de la señal (11.6) | Señal en reposo |
| `WAITING_FOR_OPERATOR` | Modo manual: espera confirmación humana | Confirmación |
| `INJECT_STIMULUS` | Emite el estímulo del bloque | Estímulo emitido |
| `CAPTURE_AND_ANALYZE` | Receptor captura → FIFO → motor analítico | (μ, σ) |
| `INTERLUDE_NOISE_FLOOR` | Interludio de ruido (10.7) | Muestra de ruido registrada |
| `PAUSE_SAFE` | Pausa segura con guardado de progreso (RF-27) | Progreso en JSON temporal |
| `EXPORT_DATA_AND_CLEANUP` | Escritura de entregables (11.7) | Archivos generados |

### 11.6 Detección de estabilización

| ID | Especificación |
|---|---|
| RF-30 | La señal se considerará estable cuando la variación del valor medido entre bloques consecutivos sea < 0.5%. |
| RF-31 | Si el ACB introduce fluctuación constante (nunca < 0.5%), tras un timeout de seguridad se aplicará muestreo por promedio: captura continua de 500 ms y promedio de todas las lecturas de la ventana. |
| RF-32 | La captura de la ventana de promedio será de 500 ms continuos. |

### 11.7 Exportación de resultados

Al entrar en `EXPORT_DATA_AND_CLEANUP` se escriben los dos entregables (11.7.1 y 11.7.2) en `exported_luts/`, nombrados con `profile_metadata.hardware_name`.

#### 11.7.1 JSON de resultados

```json
{
  "hardware_metadata": { "name": "ROLAND_BITRAZER_FILTER", "samples_analyzed": 4194304 },
  "results_matrix": {
    "dimensions": ["Cutoff", "Resonance"],
    "steps": [16, 16],
    "data_points": [
      { "knobs": [0, 0],   "mu": 120.5, "sigma": 0.05 },
      { "knobs": [0, 8],   "mu": 122.1, "sigma": 0.08 }
    ]
  }
}
```

#### 11.7.2 Cabecera C++ (estructura de datos, no implementación)

Estructura de cada celda de la matriz exportada:

| Campo | Tipo | Significado |
|---|---|---|
| `stableValue` | float | Media (μ): valor estático del control en Hz o ms |
| `acbNoise` | float | Desviación estándar (σ): factor de inestabilidad |

Reglas: matriz estática `const`, alineada (`alignas(16)`, RNF-5), con cabecera autogenerada ("NO EDITAR MANUALMENTE"), nombrada con `hardware_name`. Debe compilar sin edición manual en un proyecto JUCE (O-4).

### 11.8 Matriz cronológica de ruido de fondo

Estructura de cada muestra del interludio (10.7):

| Campo | Significado |
|---|---|
| `totalRms` | Volumen RMS global del ruido en ese momento (dB) |
| `spectralProfile` | Huella espectral: energía del ruido en 32 bandas de frecuencia |
| `timestamp` | Minuto de sesión en que se tomó la muestra |

Uso posterior: detectar deriva térmica del rack; si el ruido de fondo sube (ej. de −85 dB a −70 dB), las mediciones posteriores se marcan como de menor confianza.

---

## 12. ESPECIFICACIÓN DE LA INTERFAZ DE USUARIO (PANEL DE CONTROL Y MONITORIZACIÓN DEL LAB)

> [!NOTE]
> La interfaz de **ABDAudioLab** es la **Consola de Operaciones del Laboratorio Científico**: su misión es configurar el audio ASIO, cargar perfiles de prueba `TestProfile.json`, monitorizar el progreso en tiempo real de las tomas, visualizar curvas de frecuencia/THD y controlar el robot.
> (Las especificaciones de cables visuales, catenarias y CAD de 6 slots descritas en 12.2–12.6 pertenecen al diseño del **Editor / Customizer Roland AIRA**, que se desarrollará en su propio repositorio independiente).

### 12.1 Principio rector

| ID | Especificación |
|---|---|
| RF-33 | La interfaz **solo renderiza y monitoriza**: la pantalla obedece y visualiza el estado del secuenciador y los datos del perfil; nunca altera la lógica del motor analítico. Un solo motor de renderizado limpio. |
| RF-34 | Los elementos visuales son puros: se dibujan leyendo un array plano de coordenadas y estados del secuenciador. Nunca guardan punteros ni estado propio. |
| RF-35 | Cada módulo de código será tan puro y pequeño que pueda explicarse y testearse de forma aislada. |

### 12.2 Editor de parches

- Lienzo fijo de **6 slots** con sus jacks de entrada/salida.
- Barra de herramientas con exactamente los 31 submódulos del catálogo (7.2); no hay inputs abiertos.
- Al arrastrar un cable, lo único que cambia es el array `connections` del JSON (8.2); al cambiar el JSON, todo el motor gráfico se redibuja en sincronía.
- Al guardar/exportar, el serializador lee **solo el JSON plano**, nunca objetos visuales.

### 12.3 Mapa de coordenadas de jacks

- Registro dinámico de las posiciones `X, Y` en pantalla de cada jack (`slot_0.out_wave` → (120, 250)).
- Se recalcula automáticamente en cada render o redimensionado.
- El renderizador de cables solo recibe pares de puntos; no sabe qué es un LFO ni un filtro.

### 12.4 Validador de ruteo

| Regla | Descripción |
|---|---|
| V-1 | Solo se conecta salida → entrada. Salida→salida y entrada→entrada están prohibidas. |
| V-2 | Se respetan el número fijo de entradas/salidas de cada submódulo (diccionario 7.2/17.2). |
| V-3 | Se respetan los límites de enrutamiento del DSP de Roland (índices de entrada/salida, no coordenadas de pantalla). |
| V-4 | Una conexión imposible se bloquea **antes** de llegar al core (RF-25). |
| V-5 | Las composiciones avanzadas legales (ej. doble ADSR para envolvente de 8 etapas, 7.3) **no** se bloquean. |

### 12.5 Cables: catenaria con evasión de perillas

- El cable es una curva catenaria (o Bezier equivalente) calculada desde dos puntos del mapa de jacks (12.3).
- **Precálculo de zonas prohibidas:** cajas de colisión de los potenciómetros (radio fijo por perilla).
- La física **no** se calcula frame a frame buscando colisiones complejas: los puntos de control de la curva se desvían solo si el vector del cable cruza el radio fijo de una perilla activa.
- El entorno es compacto (6 slots, máximo ~20–30 cables simultáneos): el rendimiento de SVG es suficiente (AD-2).

### 12.6 Skins híbridos

- Cuatro plantillas visuales que imitan la serigrafía real (colores: Torcido naranja `#ff5500`, Bitrazer verde `#00ff66`, Demora azul `#00aaff`, Scooper violeta `#cc00ff` — valores de referencia).
- Un único diccionario de configuración define, por módulo: id, nombre, color y etiquetas de controles. Al cambiar de pestaña, la estética muta pero la lógica de escucha de CC es idéntica (100% reutilizada).
- Las perillas virtuales GRF 1–6 cambian nombre, color de retroiluminación y rango operativo según el hardware activo y el parche cargado (`knob_map`, 8.2).

### 12.7 WebUI del laboratorio (esta versión)

- Panel de control: carga de `TestProfile.json`, inicio/pausa del escaneo, progreso del producto cartesiano, estado actual de la máquina (11.5).
- Modo captura asistida FSK (9.6).
- Avisos amigables ante fallos de hardware (RF-27): p. ej. "Hardware no detectado por USB. Conmutando a modo de control por Audio FSK."
- Throttling de eventos: máximo un mensaje cada 10–15 ms (RNF-6).

### 12.8 Ciclo de vida del Bridge WebUI y Contrato de Normalización

| ID | Especificación |
|---|---|
| RF-36 | **Echo Shield (Bloqueo de Bucles Bidireccionales)**: El core en C++ marcará el origen de los cambios. Si proviene de la WebUI, emite al hardware y bloquea la interpretación del eco entrante; si proviene del hardware, notifica a la WebUI y bloquea la reemisión hacia el hardware. |
| RF-37 | **Ciclo de vida dinámico del Bridge**: La detección de WebView en la WebUI será dinámica (`window.__JUCE__ !== undefined && window.__JUCE__.backend !== undefined`), con bucle de auto-reintento (`_initJuceBackend()`) para garantizar que el enlace de eventos no falle antes de la inyección de WebView2. |
| RF-38 | **Desbloqueo de Audio en Web / Standalone**: Ejecutar obligatoriamente `await this.audioCtx.resume()` ante la primera interacción del usuario (`pointerdown` o clic) para desbloquear el motor de audio web en navegadores modernos. |
| RF-39 | **Contrato de Normalización Estricta [0.0, 1.0]**: Todos los mensajes de sliders/knobs entre WebUI y C++ transportarán valores normalizados en el rango `[0.0, 1.0]`. La des-normalización a unidades físicas o rangos MIDI (`0..127`, `Hz`, `ms`) se realizará en el C++ Bridge utilizando escalado explícito y `std::lround()` para evitar truncados accidentales a cero. |

---

## 13. REQUISITOS NO FUNCIONALES

### 13.1 Hilo de audio

| ID | Requisito |
|---|---|
| RNF-1 | **Zero-allocation** en el hilo de audio (prohibido `new`, `resize`, reservas dinámicas en el callback). Toda memoria preasignada en la fase de preparación. |
| RNF-2 | Comunicación hilo de audio ⇄ análisis exclusivamente vía **FIFO lock-free** (`juce::AbstractFifo`). |
| RNF-7 | Prohibido cualquier cálculo matemático pesado (FFT, estadística) en el hilo de audio. |
| RNF-14 | `juce::ScopedNoDenormals` activo en cada bloque de procesamiento de audio. |
| RNF-15 | Generación de ruido con semilla determinista / LCG, sin llamadas de entropía del SO en tiempo real. |

### 13.2 Concurrencia

| ID | Requisito |
|---|---|
| RNF-8 | Motor analítico en **hilo background de baja prioridad**, despertado por la FIFO. |
| RNF-9 | Throttling WebUI→core: máximo un mensaje cada 10–15 ms. |

### 13.3 Audio

| ID | Requisito |
|---|---|
| RNF-3 | 96 kHz, 24/32-bit float, ASIO exclusivo (fallback DirectSound solo desarrollo). |
| RNF-4 | Búfer de análisis preasignado ≥ 5 s a la tasa objetivo. |
| RNF-16 | Cadena jerárquica de 3 pasos de inicialización de `AudioDeviceManager` con persistencia XML de estado (4.3). |
| RNF-17 | Inyector de tono de prueba atómico (440 Hz / 1 kHz) para diagnóstico instantáneo de la salida física DAC. |

### 13.4 Datos

| ID | Requisito |
|---|---|
| RNF-5 | Matrices exportadas alineadas (`alignas(16)`) para SIMD. |
| RNF-10 | LUTs versionadas en carpeta independiente del código; Git solo para código (15.3). |

### 13.5 Robustez

| ID | Requisito |
|---|---|
| RNF-11 | Desconexión de dispositivo a mitad de escaneo → `PAUSE_SAFE` + guardado de progreso en JSON temporal + aviso en WebUI. |
| RNF-12 | Gain staging automático antes de cada sesión (picos a ≈ −3 dBfs) para no adulterar σ. |
| RNF-13 | Throttling de eventos WebUI: máximo 1 mensaje cada 10–15 ms. |

---

## 14. CRITERIOS DE ACEPTACIÓN Y ESTADO POR FASE

### 14.1 Fase 1 — Laboratorio Autónomo (ESTADO: IMPLEMENTADA Y CONSTRUIDA ✓)

> [!NOTE]
> La Fase 1 ha sido implementada íntegramente en C++20 / JUCE 8.0.4.
> El código fuente compila limpiamente mediante `build.bat` generando el ejecutable `build/ABDAudioLab_artefacts/Release/ABDAudioLab.exe`.
> Para el detalle del mapa de ruta y del traspaso técnico entre fases, consultar [docs/ROADMAP.md](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/ROADMAP.md) y [docs/HANDOFF.md](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/HANDOFF.md).

| ID | Criterio de aceptación | Estado |
|---|---|---|
| CA-1 | El robot escanea 1 parámetro con 16 pasos en < 10 min sin intervención (O-1). | **Implementado** (Secuenciador configurable) |
| CA-2 | Cada punto de prueba registra μ y σ con ≥ 16 ventanas de análisis (O-2). | **Implementado** (`LabAnalyticEngine` con cálculo $(\mu, \sigma)$) |
| CA-3 | Un filtro estable produce σ = 0; un filtro con bamboleo produce σ > 0 coherente (ej. μ 1249 Hz, σ 4.8 Hz). | **Implementado** (Modelado en `MockHardwareController` y Farina) |
| CA-4 | Los `.h` generados compilan sin edición manual en un proyecto JUCE (O-4). | **Implementado** (`LutExporter` con `alignas(16)`) |
| CA-5 | Un escaneo sobrevive a una desconexión de MIDI a mitad de prueba: `PAUSE_SAFE` + progreso guardado (RF-27). | **Implementado** (`ProfilingSequencer::stopSession`) |
| CA-6 | Medición con SNR < 18 dB → reintento automático; fallo repetido → pausa registrada (RF-29). | **Implementado** (Control de SNR en analizador) |
| CA-7 | Cero allocations detectadas en el hilo de audio durante un escaneo completo (RNF-1). | **Implementado** (Búferes pre-asignados y FIFO *lock-free*) |
| CA-7b | Sin picos de CPU por denormales durante silencios (`juce::ScopedNoDenormals`, RNF-14). | **Implementado** (Protección en todos los callbacks de audio) |

### 14.2 Fase 2 — WebUI (parcial, solo lo necesario para el lab)

| ID | Criterio de aceptación |
|---|---|
| CA-8 | El validador bloquea salida→salida antes de que llegue al core (RF-25). |
| CA-9 | Redimensionar la ventana no desfasa cables ni perillas (un solo motor de render). |
| CA-10 | Guardar/exportar produce el JSON del parche leyendo solo texto plano, nunca objetos visuales. |

### 14.3 Fase 3 — Integración

| ID | Criterio de aceptación |
|---|---|
| CA-11 | Un parche cargado por FSK se refleja en el hardware y en la UI sin divergencias. |
| CA-12 | Mover perilla física mueve la virtual y viceversa, sin bucles ni temblores (RF-36 Echo Shield). |

---

## 15. FLUJO DE TRABAJO DEL EQUIPO

### 15.1 Spec-Driven Development

- Toda funcionalidad empieza como especificación en este documento (o PR que lo modifica) antes de escribir código.
- Los requisitos `RF-nn` / `RNF-nn` son la referencia para el code review.

### 15.2 Reglas de módulos (obligatorias)

| Regla |
|---|
| Cada módulo debe poder testearse y explicarse de forma aislada, sin leer otros módulos. |
| El calculador de catenaria recibe dos puntos y devuelve una curva: no sabe nada de Roland, JUCE ni WebSockets. |
| El gestor de conexiones solo procesa texto (`slot_2.out.audio_L` → `slot_5.in.cutoff_cv`): no sabe colores ni formas. |
| El generador de estímulos no sabe nada de Roland, MIDI ni WebSockets (RF-17). |
| `audio/` no depende de `math/` ni `network/` (sección 5). |

### 15.3 Git y datos

| Regla |
|---|
| Git solo para **código**; las LUTs/matrizes pesadas viven en carpeta independiente y no se versionan en el repo principal (RNF-10). |
| Un PR = una especificación implementada; el review verifica los `RF-nn` afectados. |
| `build/` y `exported_luts/` en `.gitignore`. |

### 15.4 Definition of Done (por tarea)

- [ ] Especificación (sección/RF) referenciada en la descripción del PR.
- [ ] Criterios de aceptación (14.x) verificados.
- [ ] Módulo testeable de forma aislada (15.2).
- [ ] Sin allocations en hilo de audio si toca el hilo de audio (RNF-1).
- [ ] Sin posiciones en píxeles en el modelo de datos si toca el parche (RF-10).

---

## 16. RIESGOS Y PREGUNTAS ABIERTAS

### 16.1 Preguntas abiertas y Resoluciones

| ID | Pregunta | Estado / Impacto |
|---|---|---|
| PQ-1 | ¿Existen comandos SysEx por USB MIDI que activen un modo dump y configuración de los módulos? | **RESUELTA (100%)**: Protocolo SysEx `RQ1` (Data Request) y `DT1` (Data Set) completamente documentado e identificado en [AIRA_Modular_Effects-master/README.md](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/google%20ia%20research/AIRA_Modular_Effects-master/README.md). Permite configuración y volcado bidireccional instantáneo por USB (sección 9.7). |
| PQ-2 | ¿Cuáles son los IDs y parámetros exactos de los 31 submódulos? | **RESUELTA (100%)**: Catálogo completo 31/31 con sus IDs hexadecimales (`00H`..`1FH`) y sus 4 parámetros internos por módulo extraídos de la ingeniería inversa (ver tabla normativa en 17.2). |
| PQ-3 | ¿Cuál es la estructura exacta de trama FSK (audio analógico)? | En desarrollo / Calibración: FSK se mantiene como protocolo analógico de respaldo para `REMOTE IN`. Herramientas de modulación/demodulación listas en [alltheFSKs-master](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/google%20ia%20research/alltheFSKs-master/README.md). |
| PQ-4 | ¿El hardware confirma la recepción de MIDI CC? | Ajustado con Echo Shield (`RF-36` en C++ Core). |

### 16.2 Riesgos

| ID | Riesgo | Mitigación |
|---|---|---|
| R-1 | Empezar a programar demasiado pronto y tener que tirar trabajo (lección del proyecto previo) | Spec-driven development estricto (15.1); prohibido codificar sin spec |
| R-2 | Acoplamiento que haga el proyecto ilegible para IAs y humanos | Reglas de módulos puros (15.2); responsabilidad única |
| R-3 | Ruido de fondo creciente (deriva térmica del rack) adulterando mediciones | Interludio de ruido periódico (10.7) + marcado de confianza (11.8) |
| R-4 | Dos tecnologías de renderizado compitiendo (lección del proyecto previo) | Un solo motor de render SVG (AD-2, RF-33) |
| R-5 | Glitches de audio por allocations en el hilo de audio | Zero-allocation (RNF-1) + FIFO lock-free (RNF-2) + NoDenormals (RNF-14) |
| R-6 | Hardware no disponible en el sistema del usuario | Fallbacks: audio FSK analógico siempre disponible (RF-27); comprobación previa de puertos (RF-28); Mock Hardware para tests (14.1). |

---

## 17. ANEXOS

### 17.1 Matriz resumen de tipos de bloque

Ver tabla normativa en 10.2. Resumen de los 5 tipos: `TimeDynamic`, `SpectrumFilter`, `AmplitudeGain`, `WaveShaper`, `CyclicModulator`.

### 17.2 Diccionario Normativo de los 31 Submódulos AIRA (IDs Hex y Parámetros)

*(Fuente: [AIRA_Modular_Effects-master/README.md](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/google%20ia%20research/AIRA_Modular_Effects-master/README.md))*

| ID Hex | Submódulo | Tipo de Bloque | Parameter 1 | Parameter 2 | Parameter 3 | Parameter 4 |
|---|---|---|---|---|---|---|
| `00H` | **EMPTY** | — | — | — | — | — |
| `01H` | **LFO** | CyclicModulator | WAVE | RATE | MOD IN LEVEL | OUTPUT LEVEL |
| `02H` | **ADSR** | TimeDynamic | ATTACK | DECAY | SUSTAIN | RELEASE |
| `03H` | **NOISE** | Generador | TYPE 1 | TYPE 2 | LEVEL 1 | LEVEL 2 |
| `04H` | **SAMPLE & HOLD** | CyclicModulator | RATE | MOD IN LEVEL | TRIGGER IN SW | — |
| `05H` | **RING MOD** | AmplitudeGain | MOD SW | — | — | — |
| `06H` | **FILTER 6dB** | SpectrumFilter | TYPE | CUTOFF | MOD IN | RESONANCE |
| `07H` | **FILTER 12dB** | SpectrumFilter | TYPE | CUTOFF | MOD IN | RESONANCE |
| `08H` | **TONE** | SpectrumFilter | TYPE | FREQUENCY | LEVEL | — |
| `09H` | **AMP** | AmplitudeGain | AMP 1 | AMP 2 | LEVEL 1 | LEVEL 2 |
| `0AH` | **MIXER** | AmplitudeGain | LEVEL 1 | LEVEL 2 | LEVEL 3 | LEVEL 4 |
| `0BH` | **STEREO MIXER** | AmplitudeGain | LEVEL 1 | LEVEL 2 | PAN 1 | PAN 2 |
| `0CH` | **CURVE CONV** | Utilidad Conversión | CURVE TYPE | REVERSE SW | — | — |
| `0DH` | **GATE DIVIDER** | Utilidad Conversión | MULTIPLE/DIVIDE | — | — | — |
| `0EH` | **TRIG TO CV DELAY TIME** | Conversor Control | MULTIPLE/DIVIDE | — | — | — |
| `11H` | **TUBE CLIP** | WaveShaper | GAIN | OUTPUT LEVEL | — | — |
| `12H` | **COMPRESSOR** | AmplitudeGain | THRESHOLD | RATIO | ATTACK | RELEASE |
| `13H` | **NOISE GATE** | AmplitudeGain | THRESHOLD | DECAY | — | — |
| `14H` | **3 BAND EQ** | SpectrumFilter | HI | MID | LOW | — |
| `15H` | **LOGIC OPERATION** | Utilidad Lógica | INPUT LEVEL 1 | INPUT LEVEL 2 | LOGIC TYPE | OUTPUT |
| `16H` | **CROSS FADER** | AmplitudeGain | CROSS FADE | CURVE TYPE | — | — |
| `17H` | **SWITCHER** | Utilidad Ruteo | ON OFF SW | SW TYPE | — | — |
| `18H` | **ENVELOPER** | TimeDynamic | THRESHOLD | RELEASE | OUTPUT LEVEL | — |
| `19H` | **TRIGGER TO LFO RATE CV** | Conversor Control | MULTIPLE/DIVIDE | — | — | — |
| `1AH` | **FILTER 18dB** | SpectrumFilter | TYPE | CUTOFF | MOD IN | RESONANCE |
| `1BH` | **FILTER 24dB** | SpectrumFilter | TYPE | CUTOFF | MOD IN | RESONANCE |
| `1CH` | **FORMANT FILTER** | SpectrumFilter | FORMANT 1 | FORMANT 2 | BALANCE | — |
| `1DH` | **SAW OSCILLATOR** | Generador | RANGE | FINE | COLOR | OUTPUT LEVEL |
| `1EH` | **SQR OSCILLATOR** | Generador | RANGE | FINE | COLOR | OUTPUT LEVEL |
| `1FH` | **MIDI NOTE TO CV/GATE** | Conversor Control | OCTAVE | TRANSPOSE | GATE POLARITY | — |

### 17.3 Especificación de análisis por efecto base

Cada efecto base se perfila con parche vacío (RF-08). Especificaciones por efecto:

**A. TORCIDO (distorsión / Tube Clip)**

| Aspecto | Especificación |
|---|---|
| Estímulo | Tono senoidal puro a 1 kHz con amplitud escalonada de 0.0 a 1.0 (fuerza saturación gradual) |
| Análisis | THD (distorsión armónica total) + curva de transferencia de amplitud (waveshaping curve) |
| Captura ACB | Detectar ruido de emulación de válvulas y fluctuaciones asimétricas en saturación profunda (vía σ) |

**B. BITRAZER (bitcrusher / sample rate reducer)**

| Aspecto | Especificación |
|---|---|
| Estímulo | Barrido de frecuencias continuo (sine sweep) + rampa de volumen lineal |
| Análisis | Ruido de cuantización (Bit Rate) y aliasing (Sample Rate); capturar frecuencias espejo |
| Captura ACB | Determinar si el reductor de tasa es interpolador lineal o Zero-Order Hold |

**C. DEMORA (multi-tap delay)**

| Aspecto | Especificación |
|---|---|
| Estímulo | Impulso único (Dirac Delta) + silencio posterior ≥ 5 s |
| Análisis | Tiempo exacto en muestras impulso→repeticiones (rango real de Delay Time); decaimiento exponencial (Feedback) |
| Captura ACB | Wow & flutter (micro-variaciones de tiempo en las repeticiones) + pérdida de agudos por eco |

**D. SCOOPER (scatter / looper / glitch)**

| Aspecto | Especificación |
|---|---|
| Estímulo | Búfer de audio rítmico o tono con variaciones drásticas de frecuencia a intervalos fijos |
| Análisis | Tamaño del búfer de grabación (Loop Time) y escala de tono (Pitch) |
| Captura ACB | σ intrínsecamente alta: mapear la distribución matemática del patrón pseudoaleatorio de Scatter (saltos/repeticiones de grano) |

### 17.4 Referencias académicas

| Autor/Estudio | Aporte al proyecto |
|---|---|
| Välimäki & Bilbao (DAFx) — Virtual Analog Modeling | Metodología de barrido por niveles de amplitud para filtros saturantes |
| Eichas & Zölzer (DAFx) — Wiener-Hammerstein | Estructura block-oriented: filtro entrada → saturador → filtro salida |
| Jatin Chowdhury (DAFx, chowdsp_utils) | Grey-box modeling; clases JUCE de interpolación bilineal/bicúbica de LUTs |
| Angelo Farina (2000) — Swept-sine technique | Barrido logarítmico: respuesta lineal + armónicos de distorsión en una sola toma |

### 17.5 Trazabilidad con el proyecto padre

| Este documento | Proyecto padre |
|---|---|
| Secciones 10–11 (laboratorio) | Fase 1 completa (subfases 1.1–1.4) |
| Sección 12 (WebUI) | Subconjunto de la Fase 2 necesario para operar el lab |
| Sección 9 (FSK y SysEx) | Subfases 1.4.1–1.4.2 y 3.3 |
| Fuera de alcance (2.3) | Fases 2 completa y 4 |

### 17.6 Directrices de Síntesis Virtual Analog y WASM para Fase 4

Para asegurar que los modelos DSP construidos en la Fase 4 a partir de las LUTs de este laboratorio cumplan con los estándares de producción de sintetizadores virtuales (`juce-audio-hybrid-plugin` skill):

1. **Osciladores y Anti-Aliasing (PolyBLEP)**:
   - Los submódulos osciladores (Saw, Square, Pulse) deben utilizar corrección de discontinuidades PolyBLEP para eliminar el aliasing por encima de Nyquist.
2. **Modelado de Filtros TPT/ZDF**:
   - Filtros de 4 polos (24 dB/oct) implementados mediante topología Zero-Delay Feedback (TPT) e integración trapezoidal.
   - Saturación no lineal (`std::tanh` o polinomios de saturación) en el lazo de realimentación de la resonancia para modelar la calidez analógica y evitar auto-oscilaciones explosivas.
   - Compensación de graves en la realimentación para evitar la caída de graves típica de filtros ladder a alta resonancia.
3. **Efectos de Modulación y Delay (BBD Chorus / Delay)**:
   - Líneas de retardo fraccionales con interpolación Lagrange o lineal para modulaciones suaves sin artefactos de cuantización temporal.
4. **Directrices de Portabilidad Web / WASM**:
   - Compilación Emscripten con `-s SINGLE_FILE=1` (Base64 embebido para zero-dependency en AudioWorklet).
   - Uso de `-s MALLOC=emmalloc` para latencia mínima en navegadores.
   - Exportación obligatoria de `HEAPF32` (`EXPORTED_RUNTIME_METHODS`) para transferencias de buffers de audio con cero copias.

### 17.7 Directorio de Recursos y Proyectos de Referencia

Guía de consulta de los archivos de investigación ubicados en `docs/google ia research/`:

| Recurso / Carpeta | Tipo | Utilidad y Rol en el Proyecto |
|---|---|---|
| [AIRA_Modular_Effects-master](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/google%20ia%20research/AIRA_Modular_Effects-master/README.md) | Documentación / Spec | **Ingeniería inversa MIDI/SysEx no oficial (Mugenkidou)**: Estructura de tramas `RQ1`/`DT1`, IDs de modelo (`15H`–`18H`), mapa de direcciones para los 6 slots, ruteo de cables virtuales (`10 20 ss dd`) y parámetros del módulo principal. |
| [alltheFSKs-master](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/google%20ia%20research/alltheFSKs-master/README.md) | Código Python | **Herramientas de Módem Audio FSK**: Scripts de modulación/demodulación (`MFSKModulator.py`, `MFSKDemodulator.py`, `crc16.py`) para decodificar grabaciones de patches `.wav` y servir de referencia para el modulador C++. |
| [audio-latency-examiner-main](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/google%20ia%20research/audio-latency-examiner-main/README.md) | Max / GenDSP | **Medición de Latencia Sample-Accurate**: Algoritmo de diferencia de tiempo y correlación de impulso (`at.calc_time_difference.gendsp`) aplicable a la calibración de línea loopback. |
| [NeuralAudio-main](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/google%20ia%20research/NeuralAudio-main/README.md) | Código C++ / CMake | **Motor de Inferencia Neuronal Real-Time (Mike Oliphant)**: Soporte para NAM (WaveNet/LSTM) y RTNeural (Keras) para modelado de no-linealidades complejas en la Fase 4. |
| [134-AES00.pdf](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/google%20ia%20research/134-AES00.pdf) | Paper AES (Farina) | **Fundamento del Farina Sweep**: Metodología del barrido senoidal logarítmico y de-convolución lineal para separar la respuesta lineal de los armónicos de distorsión no lineal con >60 dB de SNR. |
| [Wiener-Hammerstein model...pdf](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/google%20ia%20research/Wiener-Hammerstein%20model%20and%20its%20learning%20for%20nonlinear%20digital%20pre-distortion%20of%20optical%20transmitters-with-annotations.pdf) | Paper Científico | **Modelado Block-Oriented (LNL)**: Demostración de estructuras lineales-no lineales en cascada y optimización por gradiente. |
| [Especificaciones_Tecnicas_Laboratorio_Universal.pdf](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/google%20ia%20research/Especificaciones_Tecnicas_Laboratorio_Universal.pdf) | Plan Master | Resumen ejecutivo de arquitectura del laboratorio universal y matriz de autodeterminación. |
| [001.txt](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/google%20ia%20research/001.txt) | Registro de Diseño | Transcripción completa de las sesiones de planificación técnica y análisis de requisitos. |

---

## 18. ESPECIFICACIÓN TÉCNICA DE AMPLIACIÓN (FASE 1.5)

### 18.1 Requisitos Funcionales del Core y Motor DSP

| ID | Requisito | Descripción |
|---|---|---|
| `RF-27` | **Muestreo Periódico del Suelo de Ruido** | El secuenciador ejecutará un interludio de silencio automático cada $N$ minutos (500 ms silencio + 500 ms captura) y calculará $Noise_{RMS}$ y la huella FFT en 32 bandas, exportando `Analogue_Noise_Timeline.h` para modelar deriva térmica en el DAW. |
| `RF-28` | **Compensación de Error Loopback** | El motor registrará la latencia fija y la respuesta en frecuencia de la propia interfaz de audio en un test previo directo (salida $\rightarrow$ entrada) y la restará matemáticamente de las mediciones de hardware posterior. |
| `RF-29` | **Índice de Confianza y SNR Mínimo** | Cada punto de medición evaluará el SNR respecto al suelo de ruido. Si el SNR es $< 18\text{ dB}$, el robot reintentará la captura o emitirá una alerta de mal contacto. |
| `RF-30` | **Auto-Trim de Ganancia a $-3\text{ dBfs}$** | Fase previa de calibración de ganancia donde el motor inyecta un tono y autoajusta el escalado digital para que el pico roce $-3\text{ dBfs}$ sin saturar el ADC. |
| `RF-31` | **Bloque Funcional `CyclicModulator`** | 5º bloque funcional que analiza efectos de modulación cíclica (Chorus, Flanger, LFO) midiendo frecuencia en Hz, profundidad e irregularidad de onda. |
| `RF-32` | **Barrido Multinivel de Amplitud** | Inyección de estímulos a diferentes escalones de volumen ($-18, -12, -6, 0\text{ dBfs}$) para extraer curvas de transferencia no lineales dependientes del nivel de excitación. |
| `RF-33` | **Interpolador Multidimensional 2D** | Motor de interpolación bilineal/bicúbica para expandir matrices cuantizadas (ej. 16x16) a resolución completa 128x128 en las Look-Up Tables generadas. |
| `RF-34` | **Checkpoints de Sesión y Recuperación** | Guardado periódico del progreso en `session_checkpoint.json` para reanudar sesiones nocturnas si ocurre una desconexión accidental del hardware. |

### 18.2 Requisitos Funcionales de la Interfaz Gráfica (GUI)

| ID | Requisito | Descripción |
|---|---|---|
| `RF-35` | **Live Curve Plotter** | Gráfico interactivo 2D en pantalla que renderiza dinámicamente la curva de respuesta $(\mu)$ y la franja sombreada de dispersión $(\pm\sigma)$ en tiempo real. |
| `RF-36` | **Live FFT & 2D Heatmap** | Visualizador en vivo del espectro de frecuencias de retorno y mapa de calor interactivo para escaneos bidimensionales (ej. Cutoff vs Resonancia). |
| `RF-37` | **Vúmetros Estéreo y Calibración** | Medidores visuales de nivel RMS/Pico con LED de clipping e indicador visual de $-3\text{ dBfs}$. |
| `RF-38` | **Profile Builder & Batch Queue** | Diseñador interactivo de perfiles de prueba y gestor de cola de tareas por lotes desatendidas. |

---

*Fin del documento. Cualquier cambio de especificación se realiza mediante PR que modifique este archivo, referenciando los identificadores `RF-nn` / `RNF-nn` afectados.*
