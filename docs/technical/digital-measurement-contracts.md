# Especificación Técnica — Contratos de Medición Digital y Concurrencia

Este documento detalla la arquitectura de contratos, invariantes metrológicos y modelos de sincronización concurrentes implementados en **ABDAudioLab** para la medición e inspección de plugins VST3 y síntesis digital.

---

## 1. Contratos de Datos Metrológicos

### 1.1. Identidad de Plugin (`PluginIdentity`)
Representa la huella criptográfica e introspección formal del binario VST3:

```cpp
struct PluginIdentity
{
    juce::String canonicalPath;  /**< Ruta absoluta normalizada al binario en disco */
    juce::String binarySha256;   /**< Suma criptográfica SHA-256 del binario .vst3 */
    juce::String vendor;         /**< Fabricante según IPluginFactory */
    juce::String version;        /**< Versión formal declarada por el plugin */
    juce::String uid;            /**< Identificador único de clase (Class ID / CID) */
    juce::String architecture;   /**< Arquitectura de compilación ("x64", "arm64") */
};
```

### 1.2. Especificación de Medición (`MeasurementSpec`)
Describe las condiciones previas bajo las cuales se ejecuta el experimento:
- `measurementId`: Identificador UUID o canónico de la prueba.
- `measurementType`: Tipo de análisis ("dynamics", "modulation", "filter_response", "envelope").
- `executionDomain`: Dominio metrológico (`Vst3OfflineDigital`, `Vst3Realtime`, `DigitalHardwareRoundtrip`, `CombinedDutAndChain`).
- `stimulus`: Contrato del estímulo formal inyectado (nota MIDI, velocidad, barrido senoidal logarítmico o SysEx).
- `execution`: Frecuencia de muestreo (`sampleRateHz`) y tamaño de bloque (`blockSize`).

### 1.3. Resultado de Medición (`MeasurementResult`)
Contiene la telemetría observada tras la renderización:
- `status`: Estado del experimento (`completed`, `failed`, `aborted`).
- `curve`: Estructura vectorial `MeasurementCurve` con coordenadas $X$ e $Y$, nombres de métrica y unidades normalizadas (`xName`, `yName`, `xUnit`, `yUnit`).
- `artifacts`: Lista canónica de sumas de comprobación SHA-256 correspondientes a los artefactos generados (audio de referencia, audio de estímulo, respuesta al impulso, curva JSON y reporte HTML).

---

## 2. Semántica de Equivalencia de Estado por Pares (`PairwiseStateEquivalence`)

La comparación de dos contenedores verificados $A$ y $B$ produce un resultado formal de clasificación:

```cpp
enum class PairwiseStateEquivalence
{
    BitExact,
    SemanticallyEquivalent,
    NotEquivalent,
    NotComparable
};
```

### 2.1. `BitExact`
- **Condición:** Curvas de respuesta con valores idénticos muestra a muestra:
  $$\max_{i} |y_A[i] - y_B[i]| = 0.0$$
- **Requisito Criptográfico:** Mismo UID de plugin y mismo hash de estado binario.

### 2.2. `SemanticallyEquivalent`
- **Condición:** Divergencia numérica acotada dentro de la tolerancia de precisión de punto flotante metrológica:
  $$\max_{i} |y_A[i] - y_B[i]| \le 10^{-4}$$
- **Interpretación:** Comportamiento acústico indistinguible bajo estándares de modelado y tolerancia de truncamiento DSP.

### 2.3. `NotEquivalent`
- **Condición:** Divergencia acústica observable $\max_i |y_A[i] - y_B[i]| > 10^{-4}$ sobre la misma base de estímulo y configuración de plugin.

### 2.4. `NotComparable`
- Se asigna cuando los contenedores difieren en sus bases experimentales fundamentales:
  - Distintos dominios de ejecución (e.g. digital offline vs analógico no compensado).
  - Distintos binarios o CIDs de plugin.
  - Distinto número de puntos en las curvas o falta de datos requeridos.
  - Uno o ambos contenedores en estado distinto de `Verified`.

---

## 3. Reglas de Compatibilidad de Bases (`areMeasurementBasesCompatible`)

Antes de permitir la superposición gráfica de dos series o su comparación directa, se evalúan 5 reglas de validación estricta:

```cpp
bool MeasurementComparisonSession::areMeasurementBasesCompatible(const MeasurementViewModel& a,
                                                                 const MeasurementViewModel& b,
                                                                 juce::String& outReason);
```

1. **Incompatibilidad de Unidades:** `a.curve.yUnit == b.curve.yUnit`. (e.g., no se permite superponer dBFS con Hz).
2. **Exclusión Cruzada Peak vs RMS:** Si una serie representa nivel de pico (`Peak`) y la otra potencia cuadrática media (`RMS`), se rechaza la superposición: *"Cannot overlay Peak level with RMS power metric."*
3. **Exclusión Cruzada Centroid vs Rolloff:** No se permite cruzar centroide espectral con frecuencia de caída: *"Cannot overlay Spectral Centroid with Spectral Rolloff metric."*
4. **Segregación de Dominios:** Se prohíbe superponer el dominio puramente digital `Vst3OfflineDigital` con el dominio analógico `CombinedDutAndChain` sin un registro previo de compensación reversible: *"Cannot overlay pure Vst3OfflineDigital with uncompensated CombinedDutAndChain."*
5. **Coherencia de Frecuencia de Muestreo:** $|\Delta f_s| \le 1.0 \text{ Hz}$.

---

## 4. Concurrencia, Ciclo de Vida y Seguridad de Hilos

### 4.1. Máquina de Estados de Apagado (`SessionShutdownState`)
El ciclo de vida de la sesión está protegido contra carreras críticas y drenados incompletos:

```cpp
enum class SessionShutdownState
{
    Running,
    CancellationRequested,
    Draining,
    Drained,
    DrainTimedOut,
    Destroyed
};
```

### 4.2. Prevención de *Use-After-Free* Mediante Desacoplo de Estado
- El estado mutable de la sesión reside en un `std::shared_ptr<SharedSessionState>`.
- Cada tarea en segundo plano (`ContainerLoadJob`) conserva un `std::weak_ptr<SharedSessionState>`.
- En el destructor de `MeasurementComparisonSession`:
  1. Se establece `cancelToken = true` y se pasa a `SessionShutdownState::Draining`.
  2. Se ejecuta `threadPool_->removeAllJobs(true, 3000)`.
  3. Si la operación concluye en menos de 3.000 ms, el estado pasa a `Drained`.
  4. Si expira el tiempo límite, el estado pasa a `DrainTimedOut`. Los recursos compartidos **no se liberan** mientras los hilos huérfanos continúen ejecutándose, previniendo lecturas de memoria inválida.

### 4.3. Generación Monotónica (`sessionGeneration`)
Para evitar que un callback asíncrono tardío publique resultados en una sesión que fue reiniciada o cancelada:
- `std::atomic<uint64_t> sessionGeneration` se incrementa monotónicamente en cada nueva carga o cancelación.
- El callback asíncrono invocado vía `juce::MessageManager::callAsync` valida la triple guarda:
  ```cpp
  if (state->cancelToken.load() ||
      state->shutdownState.load() != SessionShutdownState::Running ||
      state->sessionGeneration.load() != capturedGeneration)
  {
      return; // Descarte determinista silencioso
  }
  ```

### 4.4. Confinamiento de Hilos (DSP Safety)
- **Cero Asignaciones en Audio:** Ninguna rutina de inspección o carga asigna memoria en el hilo de audio en tiempo real.
- **Confinamiento de `callAsync`:** `juce::MessageManager::callAsync` se invoca exclusivamente desde los hilos de trabajo del `ThreadPool` hacia el hilo de mensajes de la interfaz gráfica, nunca desde el procesamiento DSP.
