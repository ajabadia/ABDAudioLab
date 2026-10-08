# Especificación Técnica: R5-AUDIO2 - Ciclo de Vida y Seguridad de Captura en Loopback

**Hito:** R5 - Audio Interface Calibration  
**Microhito:** R5-AUDIO2  
**Estado:** Aprobado para Implementación  
**Fecha:** 5 de Octubre de 2026  
**Rama:** `main`  
**Baseline previo:** `57b3f68` (R5-AUDIO1: 976 test cases, 949 PASS, 27 SKIPPED, 0 FAIL, 211,261 assertions PASS)

---

## 1. Diagnóstico del Defecto y Análisis de Causalidad

En **R5-AUDIO1** se certificó matemáticamente la adaptación de sweep y banda de planitud a frecuencias de muestreo reales ($f_s = 44.100\text{ Hz}$). Sin embargo, la prueba física con la interfaz PreSonus AudioBox USB demostró que la calibración se detiene en `State::Failed` (`[ CHECK RETURN SIGNAL ]`), mostrando el mensaje genérico *"Insufficient or invalid return signal detected."* a pesar de que el retorno físico está conectado y los vúmetros registran señal ($\approx -20\text{ a } -12\text{ dBFS}$).

La auditoría de `src/audio/LabAudioReceiver.cpp` y `src/gui/NativeCalibrationPanel.cpp` reveló una fragilidad estructural:

### 1.1 Fragilidad "Todo o Nada" en `retrieveRecordedData()`
1. El panel arma el receptor con:
   $$\text{armCapture}(55,125\text{ samples}, 0.005\text{f})$$
   A $44.1\text{ kHz}$, $55,125\text{ samples}$ corresponden a $1.25\text{ s} = 1250\text{ ms}$.  
   El umbral lineal $0.005\text{f}$ equivale a $\approx -46.02\text{ dBFS}$.
2. El temporizador de la interfaz gráfica corre a intervalos de $50\text{ ms}$, disparando la lectura en el paso 29:
   $$29 \text{ pasos} \times 50\text{ ms} = 1.45\text{ s} = 1450\text{ ms}$$
3. Mientras ningún sample supere $-46.02\text{ dBFS}$, el receptor permanece en `ReceiverState::WaitingForTrigger`.
4. Si el disparo se retrasa (por latencia de buffers ASIO o arranque gradual a 20 Hz), al llegar a los $1450\text{ ms}$ de la UI el receptor ha acumulado menos de los $55,125\text{ samples}$ configurados.
5. `retrieveRecordedData()` comprueba de forma binaria:
   ```cpp
   if (state.load(std::memory_order_acquire) != ReceiverState::Finished)
       return false;
   ```
   Al no haber alcanzado `Finished`, devuelve `false` y deja el vector `destination` **completamente vacío (0 samples)**.
6. `analyzeLoopback()` recibe un vector vacío y falla de inmediato con el mensaje genérico, ocultando al operador la causa real.

---

## 2. Principio Metrológico: La Cantidad de Muestras No Prueba la Integridad del Sweep

Una conclusión fundamental de la auditoría es que **contar suficientes muestras no demuestra por sí solo que se haya capturado el sweep completo desde su inicio**:

> **Demostración de Falso Positivo por Conteo Ciego:**  
> Si el sweep se emite en $t = [0.0\text{ s} \dots 1.0\text{ s}]$, pero el trigger se activa tarde en $t = 0.3\text{ s}$ y el receptor graba durante $1.0\text{ s}$ (hasta $t = 1.3\text{ s}$), el buffer contendrá exactamente $44,100\text{ samples}$ a $44.1\text{ kHz}$ ($1.0\text{ s}$).  
> Sin embargo, **se habrán perdido los primeros 300 ms del sweep** (la rampa de graves de 20 Hz a 100 Hz).  
> Alimentar este fragmento mutilado a la deconvolución Farina generará una respuesta impulsional distorsionada, una estimación errónea de latencia y un **falso PASS corrupto**, lo cual es inaceptable.

Por tanto:
$$\text{Cantidad de samples suficiente } \neq \text{ Sweep íntegro garantizado}$$

---

## 3. Política de Captura en Loopback: Captura Continua Sincronizada con Ventana Completa

Para la calibración de loopback controlada, se adopta formalmente la **Captura Continua Sincronizada**:

1. **Innecesaridad de Trigger en Farina:** El método de Angelo Farina (2000) no requiere sincronización por umbral de amplitud. La deconvolución por transformada de Fourier produce la respuesta impulsional lineal completa en el dominio temporal, donde **la posición del pico de la IR determina de forma exacta y unívoca la latencia de ida y vuelta (RTL)**.
2. **Preservación Integral de Graves:** Al capturar de forma continua desde el frame 0, se garantiza que el inicio del sweep (20 Hz) no se trunca ni sufre atenuación por un umbral de disparo.
3. **Inicio Síncrono:** La captura continua se activa y el generador de sweep arranca en el mismo frame de audio. La latencia de ida y vuelta de la interfaz física (RTL típicamente entre 10 ms y 100 ms) constituye el tiempo de vuelo natural antes de la llegada del sweep, garantizando que el inicio de la señal queda íntegramente contenido en el buffer.

### 3.1 Fórmula General Adaptativa según Sample Rate ($f_s$)
La ventana temporal requerida no es fija en muestras, sino que se calcula matemáticamente para cualquier frecuencia de muestreo soportada:

$$N_{\text{required}} = N_{\text{sweep}} + N_{\text{latency-margin}} + N_{\text{decay-tail}}$$

Donde:
- $N_{\text{sweep}} = \text{round}(f_s \times T_{\text{sweep}})$ con $T_{\text{sweep}} = 1.00\text{ s} = 1000\text{ ms}$.
- $N_{\text{latency-margin}} = \text{round}(f_s \times T_{\text{latency}})$ con $T_{\text{latency}} = 0.20\text{ s} = 200\text{ ms}$.
- $N_{\text{decay-tail}} = \text{round}(f_s \times T_{\text{decay}})$ con $T_{\text{decay}} = 0.10\text{ s} = 100\text{ ms}$.

#### Tabla de Valores por Frecuencia de Muestreo
| $f_s$ | Sweep (1.00 s) | Margen RTL (0.20 s) | Cola (0.10 s) | Total Requerido ($N_{\text{required}}$, 1.30 s) |
| :--- | :--- | :--- | :--- | :--- |
| **44.1 kHz** | 44,100 | 8,820 | 4,410 | **57,330 samples** |
| **48.0 kHz** | 48,000 | 9,600 | 4,800 | **62,400 samples** |
| **96.0 kHz** | 96,000 | 19,200 | 9,600 | **124,800 samples** |

### 3.2 Deadline Dinámico de la Interfaz de Usuario
El temporizador de la UI **no utiliza constantes fijas** ni números mágicos de pasos. El tiempo límite de espera se deriva formalmente de la duración requerida más un margen de gracia de planificación:

$$T_{\text{timeout, ms}} = \left\lceil \frac{N_{\text{required}}}{f_s} \times 1000.0 \right\rceil + T_{\text{scheduler-grace, ms}}$$

Con $T_{\text{scheduler-grace, ms}} = 250\text{ ms}$:
$$T_{\text{timeout, ms}} = 1300\text{ ms} + 250\text{ ms} = 1550\text{ ms}$$
Para un intervalo de tick de $50\text{ ms}$:
$$\text{maxSteps} = \left\lceil \frac{1550\text{ ms}}{50\text{ ms}} \right\rceil = 31\text{ pasos}$$

---

## 4. Concurrencia y Snapshot Seguro (Acknowledge del Hilo de Audio)

Para evitar condiciones de carrera (TOCTOU) donde la UI copie datos de la FIFO mientras el hilo de audio en tiempo real continúa escribiendo, se establece un protocolo de reconocimiento explícito:

### 4.1 Máquina de Estados del Ciclo de Finalización
$$\text{Recording} \longrightarrow \text{FinalizeRequested} \longrightarrow \text{Finalized} \longrightarrow \text{SnapshotRetrieved}$$

### 4.2 Protocolo de Handshake Seguro
1. **Petición desde la UI:** Al vencer el deadline, la UI invoca:
   ```cpp
   receiver.requestFinalizeCapture();
   ```
   Esta función escribe atómicamente `finalizeRequested.store(true, std::memory_order_release)`.
2. **Reconocimiento en el Hilo de Audio:** En el siguiente bloque de `processBlock()`:
   - Detecta `finalizeRequested == true`.
   - **Deja de insertar muestras** en la FIFO circular.
   - Fija de forma inmutable el contador final `recordedCount`.
   - Cambia el estado a `ReceiverState::Finalized` y activa `snapshotReady.store(true, std::memory_order_release)`.
3. **Extracción en la UI:**
   - La UI comprueba `isSnapshotReady()`.
   - Invoca `retrieveFinalizedSnapshot(destination, requirements)`.
   - Como el hilo de audio ya no escribe en la FIFO, la lectura y copia al vector `destination` son libres de carreras y deterministas.

---

## 5. Diseño de API y Contratos de Datos

Ubicación: `src/audio/LabAudioReceiver.h`

```cpp
namespace abdaudiolab::audio
{

enum class CaptureResult
{
    Complete,                   ///< Ventana objetivo completa alcanzada (N >= requiredSamples).
    TimedOutWaitingForTrigger,  ///< Exclusivo de modo trigger: expiró sin señal sobre el umbral.
    TimedOutIncomplete,         ///< Venció el plazo con N < requiredSamples (fallo explícito, vector vacío).
    Aborted,                    ///< Captura abortada (ver abortReason).
    Invalid                     ///< Error de configuración o buffers.
};

enum class CaptureAbortReason
{
    None,
    SustainedClipping,          ///< Sobrecarga por clipping sostenido (> 700 samples durante sweep).
    PossibleFeedbackLoop,       ///< Posible bucle de feedback o señal peligrosa (> -6 dBFS o clipping instantáneo durante silencio).
    DeviceStopped,              ///< El dispositivo de audio se detuvo.
    UserCancelled,              ///< Cancelado por la UI o el usuario.
    BufferOverrun               ///< Desbordamiento de la FIFO circular.
};

struct CaptureRequirements
{
    int requiredSamples { 0 };      ///< Total requerido (sweep + margen latencia + cola).
    int sweepSamples { 0 };         ///< Duración nominal del sweep.
    int latencyMarginSamples { 0 }; ///< Margen de latencia máxima esperable.
    int decayTailSamples { 0 };     ///< Margen de cola de decaimiento.
    bool requireTrigger { false };  ///< false: Loopback continuo; true: Modo trigger.

    /**
     * @brief Función fábrica pura para calcular requisitos matemáticos exactos según el sample rate.
     */
    [[nodiscard]] static CaptureRequirements makeLoopbackRequirements(
        double sampleRate,
        double sweepDurationSeconds = 1.0,
        double latencyMarginSeconds = 0.2,
        double decayTailSeconds = 0.1) noexcept
    {
        CaptureRequirements req;
        if (sampleRate <= 0.0)
            return req;

        req.sweepSamples = static_cast<int>(std::lround(sampleRate * sweepDurationSeconds));
        req.latencyMarginSamples = static_cast<int>(std::lround(sampleRate * latencyMarginSeconds));
        req.decayTailSamples = static_cast<int>(std::lround(sampleRate * decayTailSeconds));
        req.requiredSamples = req.sweepSamples + req.latencyMarginSamples + req.decayTailSamples;
        req.requireTrigger = false;
        return req;
    }
};

struct CaptureStatus
{
    CaptureResult result { CaptureResult::Invalid };
    CaptureAbortReason abortReason { CaptureAbortReason::None };
    int samplesCaptured { 0 };
    int requiredSamples { 0 };
    float peakDetectedLinear { 0.0f };
    bool triggerReached { false };
};

} // namespace abdaudiolab::audio
```

### 5.1 Regla de Exclusión de Estados en Calibración Loopback
- En modo **Loopback Calibration** (`requireTrigger = false`), el receptor captura continuamente desde $t = 0$.
- **Invariante:** En este modo es imposible que se produzca `CaptureResult::TimedOutWaitingForTrigger`. La UI de calibración solo puede recibir `Complete`, `TimedOutIncomplete` o `Aborted`.
- El estado `TimedOutWaitingForTrigger` queda reservado exclusivamente para subsistemas que activen `requireTrigger = true`.

---

## 6. Plan de Pruebas Unitarias Herméticas

Ubicación: `src/tests/test_AudioReceiverLifecycle.cpp`

Batería de pruebas a implementar:
1. **Fábrica de Requisitos Adaptativa:** Verificar que `makeLoopbackRequirements` genera los valores exactos a 44.1 kHz (57,330), 48 kHz (62,400) y 96 kHz (124,800).
2. **Rechazo por Desbordamiento de Capacidad:** Si `requiredSamples` supera el tamaño máximo del ring buffer asignado, el receptor debe rechazar la configuración retornando `CaptureResult::Invalid`.
3. **Cálculo Determinista de Timeout UI:** Verificar que la fórmula de timeout produce el deadline correcto según sample rate y margen de scheduler.
4. **Configuración Continua Inmediata:** `armWithRequirements(req, 0.0f)` arranca en `Recording` en el frame 0, vaciando residuos previos.
5. **Captura Completa Natural:** Inyectar bloques hasta $N = \text{requiredSamples}$; verificar transición a `Finalized` y `CaptureResult::Complete`.
6. **Handshake de Finalización Segura:** Enviar `requestFinalizeCapture()`, procesar un bloque de audio, comprobar que `isSnapshotReady() == true` y que bloques posteriores no alteran `recordedCount`.
7. **Rechazo Categórico de Sweep Incompleto ($N < N_{\text{required}}$):** Inyectar $N < N_{\text{required}}$ (ej. 44,100 samples cuando se exigen 57,330); verificar `CaptureResult::TimedOutIncomplete` y vector `destination` estrictamente vacío.
8. **Parada por Sobrecarga con Motivo Tipado:** Inyectar señal con clipping sostenido (> 700 samples); verificar auto-abort con `CaptureResult::Aborted` y `CaptureAbortReason::SustainedClipping`.
9. **Modo Trigger - Espera en Silencio:** Con `requireTrigger = true`, inyectar señal sub-umbral; verificar `WaitingForTrigger` y `TimedOutWaitingForTrigger` al solicitar finalización.
10. **Modo Trigger - Activación por Rampa:** Con `requireTrigger = true`, inyectar rampa que cruce el umbral; verificar transición a `Recording` y conservación de muestras a partir del punto de cruce.
11. **Reseteo Hermético:** Comprobar que `reset()` deja el receptor en `Idle` con punteros de FIFO en cero sin fugas de memoria ni residuos de ejecuciones previas.

---

## 7. Criterios de Aceptación Física

Una vez que la suite hermética pase en verde:
1. Compilar `ABDAudioLab.exe`.
2. Ejecutar la calibración con la PreSonus AudioBox USB conectada físicamente.
3. El panel debe:
   - Mantener el vúmetro y la lectura de dBFS activos durante la reproducción del sweep.
   - En caso de desconexión física o ganancia nula, mostrar la causa veraz según `CaptureStatus`.
   - En operación normal con loopback conectado, capturar la ventana continua íntegra de 1300 ms, calcular la RTL (~25 ms) y auto-trim de forma estable, alcanzando `State::Success`.

---

## 8. Protocolo de Seguridad y Baseline de Ruido Físico (Fase 2.5)

Para garantizar que el cable loopback físico no inicie un lazo analógico incontrolado (feedback por Direct Monitor o knob Mix hacia Input) y certificar la relación señal/ruido real (SNR):

### 8.1 Secuencia Segura de Ejecución
1. **Instrucción Física Previa:**
   Exigir al usuario que desactive el monitor directo analógico o coloque el potenciómetro Mix 100% hacia Playback antes de armar la medición.
2. **Confirmación de Silencio Digital en Audio Thread:**
   El motor de audio (`LabAudioEngine`) confirma al menos 2 bloques consecutivos de cero digital absoluto en la salida física antes de armar el baseline (`isOutputConfirmedSilent()`).
3. **Medición Aislada de Baseline (400 ms):**
   Se miden ~400 ms de silencio real del ADC pre-trim con la salida del generador en cero digital absoluto ($0.0\text{f}$ por muestra, Digital Mute, $-\infty\text{ dBFS}$). Este baseline **NO** forma parte de los 57.330 samples de la ventana contractual de loopback ni altera la compensación RTL.
4. **Protección en Dos Niveles:**
   - **Aborto de Emergencia por Bloque (Feedback Inmediato):** Si en cualquier bloque del baseline una muestra alcanza clipping ($\ge 0\text{ dBFS}$) o nivel cercano a clipping ($> -6\text{ dBFS}$), el receptor aborta inmediatamente en el audio thread con `CaptureAbortReason::PossibleFeedbackLoop`. El sweep nunca se inicia y la salida permanece en cero digital absoluto.
   - **Detección de Contaminación de Fondo ($> -45\text{ dBFS}$ RMS):** Si el nivel RMS del baseline supera $-45\text{ dBFS}$, se clasifica como `NoiseBaselineStatus::Contaminated`. El sweep se bloquea y la UI instruye al usuario a revisar el Direct Monitor, ganancia de entrada o retirar fuentes externas conectadas.
5. **Lanzamiento Condicional del Sweep:**
   Solo si el baseline resulta `Valid` o `BelowMeasurementFloor`, se procede a armar la captura contractual de 1.300 ms ($N_{\text{required}} = 57.330$ a 44,1 kHz) y disparar el sweep Farina a $-3\text{ dBFS}$.
6. **Cálculo Real de SNR:**
   $\text{SNR}_{\text{dB}} = L_{\text{signal, RMS, dBFS}} - L_{\text{noise, RMS, dBFS}}$ utilizando el ruido medido en el baseline en lugar del valor fijo arbitrario de $-96\text{ dBFS}$.

### 8.2 Separación Formal en Dos Subvistas y Máquinas de Estado Independientes (2A y 2B)

El Paso 2 desacopla estrictamente **Navegación** de **Ejecución de Audio**.
- Las tarjetas superiores actúan como **Pestañas de Navegación Pura** (View Switcher): hacer clic sobre ellas cambia la vista activa para inspeccionar instrucciones o resultados, pero **NUNCA** dispara audio ni altera el hardware.
- La ejecución de audio ocurre exclusivamente mediante **Botones de Acción Explícitos** dentro de cada vista.
- **Invariante de Invalidación Inmediata:** Al pulsar `[ Check Input Noise Baseline ]` en 2A, el estado de 2B pasa inmediatamente a `Stale`, se neutraliza `ActiveCalibrationContext` y se cancela la compensación de latencia y trim hasta que se ejecute y apruebe un nuevo sweep 2B.

```
[ PASO 2: Audio Interface Calibration ]
  │
  ├── [ Pestaña 2A: Input Noise Baseline ] (View: CalibrationSubView::NoiseBaseline_2A)
  │     - Condición Física: Cable de loopback DESCONECTADO (entrada aislada/sin fuentes).
  │     - Salida digital: Cero digital absoluto (0.0f por muestra, Digital Mute).
  │     - Entrada física: ADC seleccionado exclusivamente.
  │     - Botón de Acción: [ Check Input Noise Baseline ]
  │     - ScopedPhysicalLoopbackCapture: activo (MockHardware nulo, plugins bypass).
  │     - Duración: 400 ms.
  │     - Diagnóstico Persistente: RMS, pico, 32 bandas, estado de contaminación.
  │
  └── [ Pestaña 2B: Physical Loopback Measurement ] (View: CalibrationSubView::PhysicalLoopback_2B)
        - Condición Física: Cable Main Out 1 ➔ Input 1 CONECTADO. Direct Monitor OFF.
        - Estado inicial: [ LOCKED ] (requiere 2A Passed).
        - Botón de Acción: [ Run Physical Loopback Calibration ] (habilitado solo tras 2A OK).
        - Preflight de Seguridad: 200 ms con salida muteada con cable conectado (aborta ante feedback).
        - Estímulo: Log Farina Sweep (-3 dBFS).
        - Captura: Ventana continua contractual de 1.300 ms (57.330 samples @ 44.1 kHz).
        - Análisis: RTL (ms y samples), planitud espectral H(f), pico, auto-trim y SNR real.
        - Diagnóstico Persistente: Métricas completas, accesibles incluso al volver de 2A.
```

### 8.3 Garantía Explícita de Ruta Física y Prevención de Filtros Fantasma

Para evitar que una ruta virtual interna (como el filtro ladder de 24 dB/octava detectado en auditorías previas) o una instancia activa de plugin secuestren la calibración:
1. `ScopedPhysicalLoopbackCapture` se instancia al arrancar el Subpaso 2A y **permanece vivo continuamente** a través del estado de espera `NoiseBaselinePassed` y durante todo el Subpaso 2B.
2. Antes de reproducir el sweep en 2B, la aplicación verifica:
   - `engine.isPhysicalLoopbackIsolationActive() == true`
   - `engine.isCaptureSourcePhysicalAdc() == true`
   Si no se cumple, el sweep no se reproduce y se muestra un error de pre-vuelo:
   `"Physical loopback calibration unavailable. Reason: Calibration input is not the physical ADC path. No sweep was played."`
3. El `CalibrationSnapshot` sella los campos booleanos `physicalAdcVerified = true`, `mockHardwareIsolated = true`, `pluginPathBypassed = true`, garantizando procedencia auditable.

