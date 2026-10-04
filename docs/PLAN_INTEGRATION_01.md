# Plan Operativo — INTEGRATION-01: Paridad E2E de Audio, Eventos y Métricas DSP

**Proyecto:** ABDAudioLab (Universal Black-Box Musical Hardware & Synth Profiler)  
**Documento:** PLAN_INTEGRATION_01.md  
**Hito:** INTEGRATION-01  
**Versión:** 1.0.0  
**Fecha:** 2026-09-23  
**Estado:** AUTORIZADO PARA PLANIFICACIÓN  
**Prerequisito:** [HITO-CONVERGENCIA-01 CERTIFICADO](docs/ADR-001_CONVERGENCIA_GUIADO_EXPLORACION.md)  
**Dependiente:** HITO-09 (bloqueado hasta el cierre exitoso de INTEGRATION-01)  

---

## 1. Misión del Hito

Demostrar de forma empírica y reproducible que **Guiado Sistemático (GS)** y **Libre Sistemático (LS)** no solo comparten arquitectura y contratos, sino que al recibir idéntico target, estado inicial y receta metrológica, producen **idéntica ejecución científica**:
- Mismos eventos MIDI efectivos y orden de despacho (D5-eventos).
- Mismos buffers de audio y huella criptográfica canónica (D6-audio).
- Mismas métricas DSP y veredictos en el `EvaluationSnapshot` (D7-DSP).
- Idéntica estructura de manifest RFC 8785 y paquete `ProductionPackage` (D8-exportación).

---

## 2. Caso Testigo Inicial Único (Benchmark Canónico)

Para garantizar aislamiento y reproducibilidad sin dependencias de plugins externos (Dexed) ni jitter de red:

* **Target:** `SyntheticAudioFixture` o `ReferenceSynth` determinista interno.
* **Frecuencia de Muestreo:** 48.000 Hz.
* **Tamaño de Bloque (Buffer):** 256 muestras.
* **Preset:** Seno puro (sin modulación, efectos ni deriva analógica simulada).
* **Estímulo:**
  - Nota MIDI: C4 (Note Number 60).
  - Velocidad MIDI: 64.
  - Compuerta (`gateMs`): 250 ms.
  - Estabilización (`settlingMs`): 50 ms.
  - Repeticiones: 3 repeticiones.
* **Semilla / Pseudoaleatoriedad:** Fija y determinista.

---

## 3. Arquitectura del Harness Determinista (MockAudioEngine)

Para ejecutar pruebas headless en CI y entornos de test sin depender de drivers de audio del sistema operativo (WASAPI, ASIO, DirectSound):

* **Ubicación:** `src/tests/support/MockAudioEngine.h` (infraestructura exclusiva de tests; cero intrusión en código de producción).
* **Mecanismo:** 
  - Conecta en bucle cerrado con `audio::LabAudioEngine`.
  - Impulsa el ciclo `audioDeviceIOCallbackWithContext` bombeando bloques de 256 muestras tan rápido como el hilo de cálculo lo requiera.
  - Permite que `ProfilingSequencer` y `ProfilingAudioCapture` completen su ciclo síncrono hasta alcanzar `SequencerState::Finished`.
  - Incluye guardas de seguridad contra bloqueos indefinidos (timeout máximo controlado).

```
[ MockAudioEngine (Harness Headless) ]
         │
         ▼ (audioDeviceIOCallbackWithContext - bloques de 256 muestras)
[ LabAudioEngine ] ───► [ ReferenceSynth / SyntheticAudioFixture ]
         ▲                                   │
         │ (NoteOn / NoteOff)                ▼ (muestras generadas)
[ ProfilingHardwareDispatcher ]    [ LabAudioReceiver::processBlock ]
         ▲                                   │
         │ (orquestación síncrona)           ▼ (buffer capturado)
[ ProfilingSequencer ] ───────────► [ ProfilingAudioCapture ]
                                             │
                                             ▼
                                     [ MeasuredPoint / DSP ]
                                             │
                                             ▼
                                    [ EvaluationSnapshot ]
```

---

## 4. Comparación de Paridad en Tres Niveles

### Nivel 1: Control Exacto (Entrada y Despacho)
Comparación estricta (igualdad 100%):
- `TargetSelectionState` (identidad, nombre, flags deterministas).
- `ExperimentPlan` / items de ensayo.
- `sequenceHash` y semilla de secuencia.
- Estado inicial y políticas preflight de calibración.
- Lista cronológica de eventos MIDI (`NoteOn`, `NoteOff`, velocidad, canal).
- Orden de despacho y conteo de bloques procesados.

### Nivel 2: Audio Canónico (Fidelidad de Señal)
Comparación física sobre buffers numéricos:
- Número de canales idéntico (estéreo / 2 canales).
- Longitud exacta de muestras.
- Diferencia absoluta punto a punto: $\max |s_{\text{GS}}[i] - s_{\text{LS}}[i]| \le 10^{-7}$.
- Error cuadrático medio: $\text{RMSE} \le 10^{-7}$.
- **SHA-256 Canónico:** Calculado sobre una serialización determinista (`float32` little-endian, sin cabeceras WAV con timestamps, sin metadatos volátiles).

### Nivel 3: Salida Científica y Exportación
Comparación de metrología y artefactos:
- Métricas acústicas en `MeasuredPoint`: RMS (dBFS), Pico (dBFS), SNR (dB), THD (%), f0 (Hz).
- Resumen espectral y correlación $\rho$.
- `EvaluationSnapshot`: `selectionStatus`, veredicto, warnings.
- Manifest canónico RFC 8785 (idéntico salvo `sessionId` y `timestamp`).
- `ProductionPackage` / reporte HTML generado.

---

## 5. Localización Precisa del Primer Punto de Divergencia

Si se detecta cualquier discrepancia entre Guiado y Libre Sistemático, el test reportará el eslabón exacto de la cadena:

$$\text{Plan} \longrightarrow \text{MIDI} \longrightarrow \text{Audio} \longrightarrow \text{DSP} \longrightarrow \text{Evaluación} \longrightarrow \text{Manifest} \longrightarrow \text{Exportación}$$

---

## 6. Fases de Ejecución

1. **Fase 1 (Harness):** Diseñar e implementar `src/tests/support/MockAudioEngine.h`.
2. **Fase 2 (Suite de Test):** Implementar `src/tests/test_Integration01GuidedVsClassicAudio.cpp` con las secciones de los 3 niveles de paridad.
3. **Fase 3 (Verificación y Cierre):** Compilar, ejecutar suite `[integration]`, suite `[parity]`, suite `[convergence]` y suite global (618+ casos).
4. **Fase 4 (Acta):** Emitir `ACTA_INTEGRATION_01_AUDIO_PARITY.md` y actualizar `MATRIX_PARITY_01.md`.
