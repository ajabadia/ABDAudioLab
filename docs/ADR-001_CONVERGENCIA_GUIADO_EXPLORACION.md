# ADR-001 — Convergencia de Flujos: Modelo de Presets y Plan Canónico

**Proyecto:** ABDAudioLab  
**Estado:** PROPUESTO — pendiente de validación en PARITY-01  
**Fecha:** 2026-09-23  
**Autores:** Usuario (decisión de producto) + Antigravity (formalización arquitectónica)  
**Hito asociado:** PARITY-01 / HITO-09  

---

## 1. Contexto y Problema

ABDAudioLab v2.1.0 cierra el ciclo HITO-01 a HITO-08 con una arquitectura canónica consolidada:
un único `ProfilingSequencer`, un único `ProfilingSessionController`, un único
`ReportExportService → ProductionPackage`. El código de ejecución y exportación está unificado.

Sin embargo, la capa de presentación e iniciación de sesión todavía mantiene una dualidad
conceptual entre **Modo Guiado** y **Modo Exploración** que:

1. Puede estar ocultando información relevante al usuario experto en el modo guiado.
2. Puede estar generando rutas de receta divergentes que alimentan al mismo motor con entradas distintas.
3. Introduce una segunda forma de navegar sin aportar una ventaja funcional clara respecto al modo exploración.
4. Obliga al usuario a elegir un "modo" antes de entender qué quiere medir.

La pregunta central que PARITY-01 debe responder:

> ¿Las rutas guiada y exploración son actualmente dos presentaciones del mismo `ExperimentPlan`
> canónico, o producen planes, capturas o análisis materialmente distintos?

---

## 2. Decisión

**Se adopta como dirección arquitectónica:**

> Un único banco de trabajo con una única ruta de ejecución,  
> diferenciada únicamente en la forma de generar el `ExperimentPlan`.

La dualidad Guiado/Exploración **desaparece como concepto de producto**.  
En su lugar se establece un modelo de **presets declarativos + edición avanzada**:

| Entrada del usuario | Resultado |
|---|---|
| Selecciona un preset/macro | Se carga un `ExperimentPlan` canónico preconfigurado, revisable y editable |
| Edita libremente | Construye el mismo `ExperimentPlan` desde cero |
| En ambos casos | `ProfilingSequencer` ejecuta, `EvaluationSnapshot` captura, `ReportExportService` exporta |

---

## 3. Arquitectura Objetivo

```
Target seleccionado
        ↓
Preset / Macro tarea / Edición manual
        ↓
ExperimentPlan canónico
        ↓
Preflight / guardas metrológicas
        ↓
ProfilingSequencer
        ↓
Captura y análisis canónicos
        ↓
EvaluationSnapshot
        ↓
Resultados / Informe / Exportación
        ↓
ReportExportService → ProductionPackage
```

**Lo que desaparece:** bifurcaciones de motor, máquinas de estado paralelas,
presentaciones de resultado separadas, exportadores alternativos.

**Lo que permanece idéntico independientemente del origen del plan:**
receta, ejecución, captura, análisis, evaluación, informe, exportación.

---

## 4. Modelo de Presets Declarativos

Los presets son **datos, no ramas de código**. Cada preset es un fichero JSON
que define un `ExperimentPlan` predefinido:

```
presets/
  QuickSynthProfile.json
  VelocityResponse.json
  FilterSweep.json
  EnvelopeProfile.json
  ManualAnalogueBasic.json
  CalibrationDiagnostic.json
  ADSREnvelopeProfile.json
  SaturationProfile.json
```

Estructura mínima de un preset:

```json
{
  "id": "QuickSynthProfile",
  "displayName": "Perfil rápido del instrumento",
  "description": "C4 · 3 velocidades · 3 repeticiones · análisis de envolvente y espectro",
  "targetType": ["VST3_Instrument", "MIDI_Hardware"],
  "plan": {
    "notes": ["C4"],
    "velocities": [40, 64, 100],
    "gateMs": 250,
    "settlingMs": 50,
    "repetitions": 3,
    "analysis": ["envelope", "spectrum", "rms", "peak", "f0"]
  },
  "acceptanceCriteria": {
    "minSNR_dB": 20.0,
    "maxTHD_pct": 5.0
  }
}
```

---

## 5. Experiencia de Usuario Objetivo

### Flujo simplificado

```
1. Elegir target
2. Elegir qué quiero averiguar
3. Elegir una plantilla o crear mi propio plan
4. Revisar el plan generado
5. Ejecutar
6. Ver y exportar el resultado
```

### Pantalla de selección (ejemplo con DemoSynth)

```
Target: DemoSynth

¿Qué quieres hacer?

[ Perfil rápido del instrumento ]
  C4 · 3 velocidades · 3 repeticiones · análisis de envolvente y espectro

[ Medir respuesta a velocidad ]
  5 velocidades · notas fijas · RMS, pico y centroide espectral

[ Explorar libremente ]
  Editar notas, parámetros, tiempos, repeticiones y análisis
```

Al seleccionar un preset, el plan generado es inmediatamente visible e inspeccionable
antes de ejecutar. No existe "maquinaria oculta de modo guiado".

---

## 6. Qué Desaparece y Qué Se Conserva

| Concepto actual | Destino |
|---|---|
| Modo Guiado | Se convierte en colección de presets operativos |
| Modo Exploración | Se convierte en editor avanzado del `ExperimentPlan` |
| Motor de perfilado | Se conserva íntegro y sin cambios |
| `EvaluationSnapshot` | Se conserva íntegro y sin cambios |
| `ReportExportService → ProductionPackage` | Se conserva íntegro y sin cambios |
| Pantallas de resultados | Se unifica en una sola vista |
| `SoundIdGuidedWorkflowContainer` | Ya eliminado en HITO-07 |

---

## 7. Condiciones y Dependencias

Esta decisión está **condicionada** a los resultados de PARITY-01:

| Resultado de PARITY-01 | Implicación para HITO-09 |
|---|---|
| Convergen completamente | Simplificar UI directamente hacia presets/macros |
| Convergen parcialmente | Migrar partes divergentes al pipeline canónico antes de rediseñar |
| Divergen materialmente | No fusionar UI hasta resolver la divergencia funcional |

---

## 8. Fuera de Alcance de Esta Decisión

- Corrección de contraste de modo oscuro (KI-01 — sesión futura independiente)
- Hosting out-of-process
- Soporte de AU, CLAP, LV2
- Pruebas de rendimiento DSP

---

## 9. Referencias

- `docs/ROADMAP.md` — Principios No Negociables §2 (especialmente puntos 3, 4, 9)
- `ACTA_RELEASE_CANDIDATE_v2.1.0.md` — §5 Delimitación Metodológica
- `MANIFEST_RELEASE_v2.1.0.json` — `scopeExclusions`
- PARITY-01 — Auditoría de convergencia Guiado/Exploración
- HITO-09 — Unificación de flujo operativo
