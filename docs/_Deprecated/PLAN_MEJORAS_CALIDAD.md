# Plan de Mejoras de Calidad - Memory Ordering y Thread Safety

## Objetivo
Corregir las inconsistencias de ordenamiento de memoria (`memory_order`) identificadas en la revisión de código, alineando la implementación con las reglas establecidas en `.agents/rules/dsp_thread_safety.md` y `docs/GUIDE_ISSUES_TO_AVOID.md`.

## Estado Actual
Basado en la revisión de calidad realizada (Octubre 9, 2026) y la verificación in-situ del código en esta revisión:

- ✅ **MEJ-01 (ALTO)**: `LabStimulusGenerator` - ya se lee con `acquire` en `processBlock()`. Verificación: confirmado en `src/audio/LabStimulusGenerator.cpp`.
- ⚠️ **MEJ-02 (MEDIO)**: `LabAudioEngine` - inconsistencia menor en frecuencia/ganancia del tono de prueba. Estado actual: corregido en `src/audio/LabAudioEngine.h` y `src/audio/LabAudioEngine.cpp`.
- ⏳ **MEJ-03 (OBLIGATORIO)**: Compilar, ejecutar tests de audio y suite completa. Pendiente de ejecución.

## Plan de Implementación

### 1. MEJ-01 — LabStimulusGenerator (confirmada como aplicada)

**Archivo**: `src/audio/LabStimulusGenerator.cpp`  
**Estado**: el thread de audio ya usa `std::memory_order_acquire` para `playing`, `finished`, `currentSampleIndex` y `totalSamples`.  
**Detalles observados**:
- `processBlock()` lee con `acquire`.
- publicación de finalización usa `release`.
- `reset()` y `setStimulus()` siguen usando `relaxed` como inicialización; eso queda fuera del scope de MEJ-01 según la lectura actual del plan, porque el enfoque está en la lectura del thread de audio, no en los setters de entrada.

### 2. MEJ-02 — LabAudioEngine (corregido aquí)

**Archivos**:
- `src/audio/LabAudioEngine.h`
- `src/audio/LabAudioEngine.cpp`

**Estado anterior**: 
- `enableDiagnosticTestTone()` publicaba los parámetros con release.
- El callback de audio (`renderDiagnosticTone`) los leía con `relaxed`.
- `isDiagnosticTestToneActive()` y una lectura en `audioDeviceIOCallbackWithContext` usaban `relaxed` para el flag activo.

**Cambios aplicados**:
- Lectura de `diagnosticToneFreq` y `diagnosticToneLevel` en `renderDiagnosticTone` ahora usa `std::memory_order_acquire`.
- `isDiagnosticTestToneActive()` ahora usa `std::memory_order_acquire`.
- Lectura de `diagnosticToneActive` en `audioDeviceIOCallbackWithContext` ahora usa `std::memory_order_acquire`.

**Justificación**: el callback de audio puede ejecutarse en un hilo distinto del que publica los parámetros; la visibilidad de los valores publicados debe garantizarse antes de usarlos.

## Verificación (Obligatorio)

Seguir regla del proyecto: **Verify non-trivial changes by running the project's typecheck and relevant tests**.

### Compilación
- [ ] `build.bat`

### Tests relevantes
- [ ] tests de audio / motor (`[audio]`, `[AudioEngine]`, `[AudioReceiver]`, `[Stimulus]` si existen)
- [ ] suite completa para verificar 0 regresiones

### Criterio de aceptación
- [ ] 0 FAIL
- [ ] baseline verde mantenida (615 test cases PASS como mínimo según roadmap)

## Seguimiento (Atomic MVP Workflow)

1. **Inspección**: código actual leído en los archivos objetivo
2. **Cambio 1**: `src/audio/LabAudioEngine.h` — alinear lectura del flag y del getter
3. **Cambio 2**: `src/audio/LabAudioEngine.cpp` — alinear lectura de freq/level
4. **Test específico**: compilación
5. **Test suite**: tests de audio + suite completa
6. **Decisión**: commit solo si pasan

## Notas
- MEJ-01 ya estaba materializado en este punto del código.
- MEJ-02 se corrige en esta revisión.
- MEJ-03 no se ha ejecutado aún desde este cambio.

## Referencias
- `.agents/rules/dsp_thread_safety.md`
- `docs/GUIDE_ISSUES_TO_AVOID.md`
- Código real revisado en `src/audio/LabStimulusGenerator.cpp` y `src/audio/LabAudioEngine.*`
