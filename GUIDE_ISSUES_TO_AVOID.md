# Guía de problemas a evitar — ABDAudioLab

Lecciones aprendidas del audit de calidad del código. Ordenada por impacto real observado.

---

## 1. Thread Safety en audio DSP

**El problema:** Variables escritas desde el thread de control (GUI) y leídas desde el thread de audio sin sincronización. Causa UB, glitches, crashes intermitentes.

**Patrones que los causan:**

```cpp
// MAL — bool plano, data race
bool playing { false };
void setPlaying(bool v) { playing = v; }  // GUI thread
bool isPlaying() { return playing; }       // audio thread

// BIEN — atómico
std::atomic<bool> playing { false };
void setPlaying(bool v) { playing.store(v, std::memory_order_release); }
bool isPlaying() { return playing.load(std::memory_order_acquire); }
```

**Regla:** Si un miembro se escribe en un thread y se lee en otro, **debe** ser `std::atomic` o estar protegido por un lock. No hay excepciones.

**Casos reales encontrados:**
- `LabStimulusGenerator`: `playing`, `finished`, `currentSampleIndex`, `totalSamples` — todos plain
- `LabAudioEngine`: `diagnosticToneFreq`, `diagnosticToneLevel` — plain float
- `LabAudioReceiver`: `ringBufferSize`, `triggerThreshold` — plain int/float
- `MockHardwareController`: `cutoffNormalized`, `resonanceNormalized`, `driveNormalized` — plain float

---

## 2. Memory ordering en atómicos

**El problema:** Usar `memory_order_relaxed` cuando se necesita `acquire`/`release` para garantizar visibilidad de escrituras previas.

```cpp
// MAL — relaxed no garantiza que el reader vea las escrituras previas del writer
spectrumDataReady.store(true, std::memory_order_release);  // writer
if (spectrumDataReady.load(std::memory_order_relaxed))     // reader — puede ver true pero datos viejos
    std::copy(spectrumMagnitudesDb.begin(), ...);

// BIEN — acquire en el reader empareja con release en el writer
if (spectrumDataReady.load(std::memory_order_acquire))     // reader ve todas las escrituras previas
    std::copy(spectrumMagnitudesDb.begin(), ...);
```

**Regla de memoria:**
| Operación | Memory order |
|-----------|-------------|
| Solo importa el valor, no la sincronización | `relaxed` |
| Publicar datos para otro thread | `release` |
| Leer datos publicados por otro thread | `acquire` |
| Inicialización de un objeto | `release` (writer) / `acquire` (reader) |

---

## 3. TOCTOU (Time-of-Check-Time-of-Use)

**El problema:** Verificar un estado y luego actuar sobre él, pero el estado cambia entre la verificación y la acción.

```cpp
// MAL — midiOut puede ser reseteado entre isConnected() y sendMessageNow()
bool setParameterRaw(int idx, int val) {
    if (!isConnected())      // CHECK — midiOut != nullptr
        return false;
    // ← disconnect() puede ejecutarse aquí en otro thread
    midiOut->sendMessageNow(msg);  // USE — crash si midiOut es nullptr
}

// BIEN — capturar el puntero una vez
bool setParameterRaw(int idx, int val) {
    auto* out = midiOut.get();  // snapshot atómico del puntero
    if (out == nullptr) return false;
    out->sendMessageNow(msg);   // seguro — el unique_ptr no se libera
}
```

**Regla:** Nunca desreferenciar un puntero/iterador después de verificar su validez si otro thread puede invalidarlo. Captura el valor una vez.

---

## 4. `unique_ptr` no es thread-safe

**El problema:** `std::unique_ptr::reset()`, `operator=`, y el destructor no son atómicos. Si un thread llama `reset()` mientras otro lee el puntero → data race = UB.

**Regla:** Si un `unique_ptr` se comparte entre threads, necesitas un `std::mutex` o `std::shared_mutex` para protegerlo. No hay forma de hacerlo lock-free de forma segura con `unique_ptr`.

**Caso real:** `AiraSysExController` — `midiOut` y `midiIn` son `unique_ptr` escritos en `connect()`/`disconnect()` y leídos en `setParameterRaw()`/`handleIncomingMidiMessage()`.

---

## 5. Heap allocation en audio callbacks

**El problema:** `new`, `malloc`, `vector::resize`, `string` en el thread de audio causan glitches o crashes por prioridad.

```cpp
// MAL — resize en cada callback
void audioDeviceIOCallback(...) {
    buffer.resize(numSamples);  // ← heap alloc!
}

// BIEN — pre-asignar en prepare()
void audioDeviceAboutToStart(AudioIODevice* device) {
    buffer.assign(device->getCurrentBufferSizeSamples(), 0.0f);
}
void audioDeviceIOCallback(...) {
    // buffer ya tiene tamaño suficiente
}
```

**Regla:** El audio callback solo puede usar memoria pre-asignada. Todo `new`/`resize`/`reserve` debe estar en `prepare()` o `aboutToStart()`.

---

## 6. DRY violations que causan bugs de mantenimiento

**El problema:** Lógica duplicada en dos archivos que evoluciona independientemente.

**Caso real:** `TestConfigModal` y `SlideInDrawer` tenían UI de stimulus/duration/matrix casi idéntica (~300 líneas). Cuando se actualizó uno, el otro quedó desincronizado.

**Regla:** Si una lógica aparece en más de un archivo, extráela a un componente compartido. No confíes en "ya lo actualizo después" — nunca se hace.

---

## 7. Código muerto que complica el audit

**El problema:** Archivos/clases que nadie usa pero están en el build system. Consumen tiempo de compilación y confunden a quien audita.

**Caso real:** `StereoVuMeter`, `LiveCurvePlotter` — en CMakeLists.txt pero nunca instanciados. `SessionManager` y `HardwareManager` — clases completas pero nunca integradas en `main.cpp`.

**Regla:** Si un archivo no es referenciado por nadie, bórralo. Si creaste una refactorización pero no la integraste, no la subas — es deuda técnica encubierta.

---

## 8. Tests que fallan pre-existente = deuda invisible

**El problema:** Tests que fallan desde hace tiempo se ignoran y ocultan regresiones reales.

**Caso real:** `FarinaDeconvolver` y `SessionSerializer` fallan pero nadie los arregla porque "ya fallaban antes".

**Regla:** Si un test falla, bórralo o arréglalo en el mismo PR. Un test roto es peor que no tener test — crea falsa sensación de cobertura.

---

## 9. Datos hardcodeados en análisis

**El problema:** Valores de análisis que devuelven constantes en vez de computar resultados reales.

**Caso real:** `LabAnalyticEngine.cpp:412` — asimetría LFO siempre `0.02f`, nunca calculada de la señal.

**Regla:** Si una función de análisis devuelve un valor constante, o es un stub que debe implementarse, o es un test helper que debe estar en código de test, no en producción.

---

## 10. `#include` muertos crean dependencias fantasma

**El problema:** Incluir un header que defines clases que nunca usas. Si esas clases cambian de API, tu archivo compila pero el linking falla o el comportamiento cambia silenciosamente.

**Caso real:** `main.cpp` incluía `SessionManager.h` y `HardwareManager.h` pero nunca instanciaba ninguna clase.

**Regla:** Si no usas nada de un header, quita el `#include`. Los headers muertos crean acoplamiento fantasma que dificulta refactoring.
