# Protocolo de Excitación Determinista y Reset (Fase 5D)

**Documento:** `docs/qa/audio-ab-5d-excitation-protocol.md`  
**Hito Rector:** `HITO-AUDIO-AB-5D: Acceptance Matrix and Canonical Preset Validation`  
**Estado:** V1.0 — Congelado para Diseño y Especificación  
**Fecha:** 2026-09-29  

---

## 1. Principio de Repetibilidad Temporal y Reset de Estado

Para que un test A/B sea científica y formalmente válido, no puede existir contaminación de estado (*state carry-over*) entre corridas ni asimetría en las condiciones iniciales de los filtros analógicos modelados:

1. **Protocolo de Reset Obligatorio Pre-Render:**
   - Invocación explícita de `reset()` en todos los componentes del motor (osciladores, filtros ZDF/TPT, LFOs, líneas de retardo BBD y generadores estocásticos).
   - Limpieza de buffers acumuladores de fase ($\phi = 0.0$).
   - Inicialización determinista del generador de números pseudoaleatorios con la semilla declarada en el fixture.
2. **Período de Warm-up / Silencio Inicial:**
   - Warm-up silence: Los primeros 100 ms contienen magnitud lineal máxima 0.0f y RMS 0.0f. Para presentación logarítmica, el resultado se clasifica como DigitalSilence / −∞ dBFS, sin sustituirlo por 0 dBFS. Permite que filtros bloqueadores de continua (DC-blockers) y etapas con realimentación alcancen el reposo absoluto. Estas muestras iniciales se excluyen del cálculo de correlación de transitorio.

---

## 2. Vector Canónico de Excitación Musical

Cada corrida define una secuencia de eventos de control deterministas:

| Parámetro | Valor Canónico Inicial | Valor Ampliado (Fase Posterior) | Justificación |
|---|:---:|:---:|---|
| **Nota MIDI** | `60` (C4 / 261.63 Hz) | `36` (C2 / grave) y `84` (C6 / agudo) | Registro medio representativo de respuesta general |
| **Velocidad MIDI** | `100` (Forte nominal) | `64` (Mezzo-forte) | Respuesta a ganancia y compresión no lineal |
| **Tiempo NoteOn** | $100\text{ ms}$ tras inicio de render | Fijo | Permite estabilización previa |
| **Duración Note (Gate)** | $1.000\text{ ms}$ (1.0 s) | $250\text{ ms}$ (staccato) / $2.000\text{ ms}$ (legato) | Excitación sostenida para medir transitorio, cuerpo y sustain |
| **Tiempo NoteOff** | $1.100\text{ ms}$ tras inicio de render | En función del gate | Disparo exacto de envolvente de release |
| **Cola de Release (Tail)** | $500\text{ ms}$ | $1.000\text{ ms}$ en presets con delay/reverb | Medición de decaimiento natural, denormals y piso de ruido |
| **Duración Total** | $1.600\text{ ms}$ | $2.500\text{ ms}$ | Longitud total del buffer capturado en memoria |

---

## 3. Protocolo de Pasadas y Verificación Intra-Motor

Antes de contrastar el Motor A con el Motor B, el protocolo exige una verificación estricta de estabilidad intra-motor:

```text
       [Reset de Estado]
              │
              ▼
    ┌───────────────────┐
    │  Pasada 1 Motor A │ ──► Buffer A1
    └───────────────────┘
              │
       [Reset de Estado]
              │
              ▼
    ┌───────────────────┐
    │  Pasada 2 Motor A │ ──► Buffer A2
    └───────────────────┘
              │
    [Comprobación Bit a Bit] ──► A1 == A2 (Corr = 1.0, diff = 0.0)
              │
              ▼
       [Reset de Estado]
              │
              ▼
    ┌───────────────────┐
    │  Pasada 1 Motor B │ ──► Buffer B
    └───────────────────┘
              │
    [Comparación A/B Formal] ──► AudioABComparator(A1, B)
```

- **Invariante de Determinismo:** Si la Pasada A1 no coincide bit a bit con la Pasada A2 (con la misma semilla), el test falla inmediatamente con `DIAG_FAIL_NON_DETERMINISTIC_ENGINE` sin proceder a la comparación A/B.

---

## 4. Matriz Inicial de 10 Corridas Canónicas (Línea Base 5D)

Para validar la infraestructura sin crear una explosión combinatoria, la línea base se fija en **10 corridas canónicas exactas** (5 presets $\times$ 2 sample rates $\times$ 1 buffer size $\times$ 1 evento principal):

| Corrida ID | Preset Asignado | Clase Acústica | Sample Rate | Buffer Size | Evento MIDI | Semilla |
|---|---|---|:---:|:---:|:---:|:---:|
| `RUN_5D_01` | `PRESET_CLEAN_REF_01` | `CleanReference` | 44.100 Hz | 256 | C4, vel 100, 1.0s gate | `0x5D0001ULL` |
| `RUN_5D_02` | `PRESET_CLEAN_REF_01` | `CleanReference` | 48.000 Hz | 256 | C4, vel 100, 1.0s gate | `0x5D0001ULL` |
| `RUN_5D_03` | `PRESET_GENTLE_MOD_02` | `GentleModulation` | 44.100 Hz | 256 | C4, vel 100, 1.0s gate | `0x5D0002ULL` |
| `RUN_5D_04` | `PRESET_GENTLE_MOD_02` | `GentleModulation` | 48.000 Hz | 256 | C4, vel 100, 1.0s gate | `0x5D0002ULL` |
| `RUN_5D_05` | `PRESET_AGGR_NONLIN_03` | `AggressiveNonlinear` | 44.100 Hz | 256 | C4, vel 100, 1.0s gate | `0x5D0003ULL` |
| `RUN_5D_06` | `PRESET_AGGR_NONLIN_03` | `AggressiveNonlinear` | 48.000 Hz | 256 | C4, vel 100, 1.0s gate | `0x5D0003ULL` |
| `RUN_5D_07` | `PRESET_LOW_LEVEL_04` | `LowLevelDynamic` | 44.100 Hz | 256 | C4, vel 100, 1.0s gate | `0x5D0004ULL` |
| `RUN_5D_08` | `PRESET_LOW_LEVEL_04` | `LowLevelDynamic` | 48.000 Hz | 256 | C4, vel 100, 1.0s gate | `0x5D0004ULL` |
| `RUN_5D_09` | `PRESET_HIGH_DENSITY_05` | `HighDensitySpectral` | 44.100 Hz | 256 | C4, vel 100, 1.0s gate | `0x5D0005ULL` |
| `RUN_5D_10` | `PRESET_HIGH_DENSITY_05` | `HighDensitySpectral` | 48.000 Hz | 256 | C4, vel 100, 1.0s gate | `0x5D0005ULL` |

---

## 5. Criterios de Aceptación Global de la Matriz

Para que la matriz de 10 corridas sea certificada en la Fase 5D:
1. **10/10 Corridas Ejecutadas Limpiamente:** 0 cuelgues, 0 excepciones, 0 fallos de memoria, terminación normal.
2. **Determinismo Bit a Bit Demostrado:** Las 10 pruebas intra-motor deben resultar idénticas al 100%.
3. **0 Veredictos `FAIL` Inesperados:** Todas las corridas deben resolver en `PASS` o en `WARN` documentado formalmente por la clase de dispersión acústica.
4. **Hashes Criptográficos Emitidos:** Generación de un digest SHA-256 inmutable para cada buffer de audio generado y para el informe consolidado.
