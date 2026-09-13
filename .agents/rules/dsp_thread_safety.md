# Reglas de Calidad, Concurrencia y Seguridad en Tiempo Real (DSP)

Directrices de ingeniería derivadas de `GUIDE_ISSUES_TO_AVOID.md` que deben cumplirse estrictamente en todo el código C++20 / JUCE 8 de `ABDAudioLab`:

---

## 1. Thread Safety en Audio DSP
- Si un miembro se escribe en un thread (p. ej. GUI) y se lee en otro (p. ej. Audio thread), **DEBE ser `std::atomic`** o estar protegido por un mecanismo lock-free. No se permiten tipos primitivos planos (`bool`, `int`, `float`) compartidos entre hilos sin sincronización.

---

## 2. Memory Ordering en Atómicos
Usar `memory_order_relaxed` únicamente cuando solo importe el valor atómico aislado y no la sincronización de escrituras asociadas.

| Operación | Memory Order | Justificación |
| :--- | :--- | :--- |
| **Solo importa el valor** | `relaxed` | Contadores o métricas donde no se publican buffers. |
| **Publicar datos para otro hilo** | `release` (writer) | Garantiza que las escrituras en memoria previas sean visibles por el lector. |
| **Leer datos publicados por otro hilo** | `acquire` (reader) | Empareja con `release`. Garantiza ver todos los datos escritos antes del store. |
| **Inicialización / Handshake de objetos** | `release` (writer) / `acquire` (reader) | Evita lectura de punteros o datos a medio construir. |

```cpp
// CORRECTO:
// Escritor (hilo que produce datos, ej. DSP o background worker):
spectrumDataReady.store(true, std::memory_order_release);

// Lector (hilo consumidor, ej. GUI paint / timerCallback):
if (spectrumDataReady.load(std::memory_order_acquire))
{
    std::copy(spectrumMagnitudesDb.begin(), spectrumMagnitudesDb.end(), localBuffer.begin());
}
```

---

## 3. Prevención de TOCTOU (Time-of-Check-Time-of-Use)
- **Regla**: Nunca desreferenciar un puntero o recurso tras verificar su validez si otro hilo puede invalidarlo entre la comprobación y el uso.
- **Patrón seguro**: Capturar una copia local del puntero antes de comprobar y usar (`auto* ptr = sharedPtr.get(); if (ptr) ptr->doWork();`).

---

## 4. `std::unique_ptr` NO es Thread-Safe
- `std::unique_ptr::reset()`, `operator=` y su destructor provocan data races y undefined behavior si otro hilo está leyendo el recurso.
- Si un recurso de audio/MIDI se conmuta en caliente, debe sincronizarse mediante `std::mutex` o intercambiarse de forma atómica segura.

---

## 5. Cero Asignaciones Dinámicas (Zero Heap Allocation) en Hilo de Audio
- Prohibido terminantemente llamar a `new`, `malloc`, `std::vector::resize`, `std::string`, `juce::String` o cualquier método que reserve memoria dinámica dentro de `audioDeviceIOCallbackWithContext` o funciones invocadas en tiempo real.
- Toda la memoria y buffers deben pre-asignarse en `prepare()` o `audioDeviceAboutToStart()`.

---

## 6. Mantenimiento y Buenas Prácticas
- **DRY**: Si una lógica de UI o cálculo analítico se repite, extraer a un componente o función compartida.
- **Sin código muerto ni includes huérfanos**: Eliminar `#include` no utilizados y clases en desuso del `CMakeLists.txt`.
- **Tests siempre en verde**: Nunca dejar un test fallando; las regresiones deben subsanarse inmediatamente.
