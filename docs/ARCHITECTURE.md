# Arquitectura de Software — ABDAudioLab

**Proyecto:** ABDAudioLab — perfilador de hardware musical de caja negra
**Versión del documento:** 1.1.0
**Actualizado:** 2026-09-03
**Tecnología:** C++20, JUCE 8, CMake, ABDScope y WebView2 (Windows)

---

## 1. Alcance y estado de esta arquitectura

Este documento describe la arquitectura **actualmente integrada en el ejecutable**. Distingue entre componentes implementados, integración verificada mediante pruebas simuladas y capacidades pendientes de comprobar con hardware físico.

No describe propuestas futuras como WASM, ni considera una capacidad terminada solo porque exista una clase o un borrador de UI en el repositorio.

### Estado de verificación

| Estado | Significado |
|---|---|
| Implementado | El código forma parte del producto o de sus dependencias de build. |
| Verificado en simulación | Hay prueba automatizada sin dispositivo físico. |
| Pendiente de banco | Requiere interfaz de audio, cableado o hardware real. |

---

## 2. Capas actuales

```text
+------------------------------------------------------------------------------+
| Presentación JUCE (main.cpp)                                                  |
| SoundID UI, cola de ensayos, drawer, diálogos, calibración y ventanas Scope |
+-----------------------------------+------------------------------------------+
                                    |
                                    v
+------------------------------- Core ----------------------------------------+
| ProfilingSequencer | ProfilingSession | contratos JSON | serialización       |
| SessionSerializer  | análisis y exportación                                  |
+---------------------+-----------------------------+--------------------------+
                      |                             |
                      v                             v
+-----------------------------+   +-------------------------------------------+
| Hardware                    |   | Audio/DSP                                 |
| IHardwareController          |   | LabAudioEngine                            |
| Mock, MIDI CC, SysEx, manual |   | estímulo, captura, trim, FFT y métricas  |
+-----------------------------+   +--------------------+----------------------+
                                                       |
                                                       v
+------------------------- ABDScope ------------------------------------------+
| ScopeDataCollector: Hardware In (DUT) | Stimulus Generator | Diagnostic 1kHz|
| ScopeTap SPSC -> serializador JSON -> JuceWebScopeComponent -> WebView2     |
+------------------------------------------------------------------------------+
```

### 2.1 Presentación

`MainContentComponent` compone la UI y actualmente sigue coordinando parte de la selección de hardware, el ciclo de sesión, persistencia y scopes. Es el principal foco de acoplamiento pendiente de extraer.

Componentes principales:

- `SoundIdSuiteList`, `TestEditorPanel` y `SlideInDrawer`: configuración de ensayos y hardware.
- `LoopbackCalibrationModal`: calibración DAC -> ADC y ajuste de ganancia de entrada.
- `ScopeFloatingWindow`: scope C++ nativo heredado, pendiente de retirada cuando ABDScope Web cubra el flujo requerido.
- `ScopeWebFloatingWindow`: contenedor del scope WebView2 de ABDScope.

### 2.2 Core, sesión y hardware

`ProfilingSequencer` se ejecuta en un hilo de JUCE y usa `IHardwareController` para desacoplar la automatización de los protocolos concretos. Coordina estímulo, captura, análisis y exportación.

Los contratos JSON se cargan mediante `HardwareContractRegistry`; los controladores disponibles incluyen Mock DSP, MIDI CC, Roland AIRA SysEx y operación analógica manual.

`SessionSerializer` está integrado para paquetes de sesión, recuperación y persistencia. `SessionManager` y `HardwareManager` existen como clases de extracción, pero no son aún los orquestadores efectivos de `main.cpp`; deben tratarse como trabajo en curso, no como límites arquitectónicos consolidados.

### 2.3 Motor de audio y tiempo real

`LabAudioEngine` implementa `juce::AudioIODeviceCallback` y es el único punto de I/O en tiempo real.

- Inicializa el dispositivo con una cadena de recuperación de tres pasos.
- Preasigna buffers L/R por canal antes de procesar audio.
- Genera estímulos en el DAC mediante `LabStimulusGenerator`.
- Procesa ADC físico o el loopback del Mock DSP con `LabAudioReceiver`.
- Publica picos/RMS y FFT para la UI.
- Aplica `juce::ScopedNoDenormals` al inicio del callback.

Regla: no se permiten reservas de memoria, I/O de disco, logs ni bloqueos en el callback. La ausencia de estas operaciones se revisa en código; la garantía de latencia debe medirse en banco y no se declara aún como una cifra contractual.

### 2.4 Telemetría ABDScope

ABDScope recibe `float` PCM estéreo por taps SPSC. Solo el tap activo recibe muestras, por lo que los scopes nativo y web no deben abrirse simultáneamente sobre el mismo tap.

| Tap | Fuente | Uso y estado |
|---|---|---|
| `Hardware In (DUT)` | ADC físico tras trim, o salida del Mock DSP | Verificado en simulación unitaria (L/R + trim + JSON); pendiente de loopback físico con hardware real. |
| `Stimulus Generator` | Señal enviada al DAC | Verificado en simulación unitaria (generación + tap + JSON). |
| `Diagnostic 1kHz` | Referencia virtual para el scope; tono físico cuando se activa el modo diagnóstico | Verificado en simulación unitaria; valida renderizado y telemetría, no prueba el ADC por sí solo. |

El camino web es:

```text
ScopeTap -> ScopeFrameSerializer -> JSON (timeDataL/timeDataR, RMS, peak)
         -> JuceWebScopeComponent (30 Hz) -> window.__pushScopeFrame()
         -> ABDScope WebUI
```

La prueba `test_AudioEngineBounds.cpp` verifica en simulación unitaria que los tres taps (`Hardware In`, `Stimulus Generator` y `Diagnostic 1kHz`) reciben sus señales respectivas, que el trim simétrico se aplica a L/R y que el serializador produce el esquema JSON esperado. No sustituye una prueba de WebView2 real en ventana ni una prueba de audio físico.

### 2.5 Exportación

La salida no se limita a LUT y JSON. El sistema incluye:

- `LutExporter` para tablas y reportes estructurados.
- `CertificationReportExporter` para reportes de medición.
- `NamDatasetExporter` para datasets de calibración NAM/RTNeural.
- `SessionSerializer` para paquetes de sesión y recuperación.

---

## 3. Flujo de una medida

```text
Usuario/configuración
  -> ProfilingSequencer (hilo worker)
  -> IHardwareController: aplica parámetros
  -> LabStimulusGenerator: genera estímulo en el callback de audio
  -> DAC -> DUT físico -> ADC                 [pendiente de banco]
  -> LabAudioReceiver: captura y dispara
  -> LabAnalyticEngine: análisis
  -> exportadores / sesión

En paralelo: ADC o Mock -> tap ABDScope -> JSON -> Scope Web
```

En modo Mock, `Hardware In` representa la salida del DSP simulado. No debe usarse como evidencia de que el ADC físico, el cableado o el DUT real funcionan.

---

## 4. Verificación pendiente

Estas tareas no se consideran completadas hasta contar con resultado reproducible:

1. **Loopback físico DAC -> ADC**: seno conocido, comparación de `Stimulus Generator` y `Hardware In`, comprobación de ganancia, latencia y ambos canales en interfaz física real.
2. **Smoke test de WebView2**: validación en ejecución del componente gráfico real, temporizador de 30 fps, comunicación IPC y dibujo en canvas web.
3. **Medición de carga y latencia**: evaluación de tiempo de CPU y jitter del callback bajo tamaños de bloque estándar (64 a 512 muestras) en banco de pruebas.

---

## 5. Decisiones y deuda técnica

- **ABDScope Web es el destino estratégico.** El scope C++ nativo es temporal; no se añadirán nuevas funciones a él salvo correcciones necesarias.
- **MainContentComponent necesita extracción gradual.** Los candidatos son control de hardware, flujo de sesión y composición de UI.
- **SessionManager y HardwareManager requieren adopción o eliminación.** No deben permanecer como una segunda arquitectura sin uso.
- **WASM no forma parte de la arquitectura vigente.** Cuando se decida, deberá registrarse en un ADR o en el roadmap con sus restricciones propias.
