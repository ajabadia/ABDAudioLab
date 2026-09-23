# ADR-001 — Convergencia de Flujos: Modelo de Presets y Plan Canónico

**Proyecto:** ABDAudioLab  
**Estado:** ACEPTADO — caracterización unitaria completada (PARITY-01, Build #411); pendiente paridad E2E (INTEGRATION-01)  
**Fecha:** 2026-09-23  
**Autores:** Usuario (decisión de producto) + Antigravity (formalización arquitectónica)  
**Hito asociado:** PARITY-01 (PARITY-01A / PARITY-01B) / HITO-CONVERGENCIA-01 / INTEGRATION-01 / HITO-09  

---

## 1. Contexto y Diagnóstico Factual

ABDAudioLab v2.1.0 consolida el pipeline de ejecución y exportación: un único `ProfilingSequencer`, un único `ProfilingSessionController` y una única cadena `ReportExportService → ProductionPackage`.

La inspección estática del código revela que **no existen dos motores científicos completos**, sino un único motor de medición (`ProfilingSequencer`) sobre el cual la interfaz construye experiencias artificialmente separadas.

### 1.1. Las Tres Rutas Identificadas en el Código

En lugar de dos modos, la arquitectura actual alberga realmente tres rutas operativas:

| Ruta | Mecanismo en Código | Naturaleza Metodológica |
|---|---|---|
| **Guiado Sistemático** | `SoundIdProfilingRunView` $\to$ `onStartClicked` $\to$ `startProfilingSession(false)` | Medición sistemática formal (UI simplificada con preflight y ocultamiento de cola/gráficos). |
| **Libre Sistemático** | `suiteList` $\to$ `onToggleSessionRunClicked` $\to$ `startProfilingSession(false)` | Medición sistemática formal (UI de banco de trabajo con inspección completa de cola y curvas). |
| **Toma Libre** | `btnFreeCapture` $\to$ NoteOn/NoteOff $\to$ `sessionCoordinator.triggerFreeCapture()` | Exploración ad-hoc no metrológica (`confirmationStatus = "unknown"`, sin plan declarado). |

### 1.2. Principio Metodológico de No Regresión
> **Regla de oro:** *Caracterizar primero; comparar después; migrar después; retirar al final.*
> No se eliminará ningún componente (`btnModeToggle`, `SoundIdProfilingRunView`, `btnFreeCapture`, `triggerFreeCapture`, `UiWorkflowMode`) hasta que los tests de caracterización hayan fijado su comportamiento y trazado la ruta de migración sin riesgo de pérdida funcional.

---

## 2. Decisión Arquitectónica

**Se adopta como dirección definitiva:**

> Un único banco de trabajo con una única ruta de ejecución canónica,  
> diferenciada únicamente en la forma de generar o editar el `ExperimentPlan`.

La dualidad conceptual Guiado/Exploración **desaparece como modo o producto independiente**.  
Se establece un modelo de **presets declarativos + tres niveles de asistencia en la UI**:

```
Target seleccionado
        ↓
Preset / Macro tarea / Edición manual
        ↓
ExperimentPlan canónico
        ↓
Preflight / guardas metrológicas
        ↓
ProfilingSequencer (única autoridad de ejecución)
        ↓
Captura y análisis canónicos
        ↓
EvaluationSnapshot
        ↓
Resultados / Informe / Exportación
        ↓
ReportExportService → ProductionPackage
```

---

## 3. Tres Niveles de Asistencia para el Usuario (HITO-09)

El usuario **nunca cambia de motor**. Solo cambia el grado de control y detalle que desea visualizar:

| Nivel | Qué ve el usuario | Qué ocurre internamente |
|---|---|---|
| **Rápido (Presets)** | “Perfil rápido”, “Respuesta a velocidad”, “Medir envolvente ADSR” | Carga un preset declarativo JSON en el `ExperimentPlan`. |
| **Configurable** | Puede ajustar notas, velocidades, compuertas (`gateMs`), repeticiones y análisis | Modifica los campos del mismo `ExperimentPlan` canónico. |
| **Avanzado** | Inspecciona todas las dimensiones, políticas DSP, trazas de eventos y tolerancias | Audita el mismo plan y los resultados con acceso total. |

---

## 4. Modelo de Presets Declarativos

Los presets son **datos (ficheros JSON), no ramas de código**.

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

Estructura canónica de un preset:
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

## 5. Delimitación de Auditoría: PARITY-01A y PARITY-01B

PARITY-01 no compara "Guiado" contra "Toma Libre" (que no son equivalentes), sino que se divide formalmente en dos tareas:

### PARITY-01A — Guiado Sistemático vs. Libre Sistemático
* **Caso controlado:** `ReferenceSynth`, 48 kHz, buffer 256, preset sine, C4, vel 64, gate 250ms, settling 50ms, 3 repeticiones.
* **Objetivo:** Demostrar si `SoundIdProfilingRunView` y `suiteList` ejecutan idéntico `ProfilingSequencer` y generan idéntico `EvaluationSnapshot` y paquete de exportación.
* **Resultado esperado:** Convergencia en ejecución y análisis, con identificación de divergencias en metadatos (`r.kind`) o copia local de `guided/`.

### PARITY-01B — Clasificación Formal de la Toma Libre
* **Objetivo:** Auditar y blindar el contrato de `btnFreeCapture` / `triggerFreeCapture()`.
* **Criterios de gobernanza:**
  1. No puede clasificarse como `Measurement` ni producir veredicto `Accepted`.
  2. No puede habilitar exportación a paquete de producción (`ProductionPackage`).
  3. Debe registrar explícitamente procedencia como exploración no certificable.
  4. Debe poder convertirse en receta declarativa si el operador desea formalizarla.

---

## 6. Correcciones Futuras Identificadas (No Aplicar Antes de los Tests)

1. **Bifurcación de Metadatos (`ProfilingSessionController.cpp:1019`):**
   * *Actual:* `r.kind = (workflowMode == Guided) ? Measurement : Exploration;`
   * *Objetivo:* `r.kind` dependerá exclusivamente de la presencia de un `ExperimentPlan` válido y el cumplimiento de las guardas preflight, nunca de la vista de origen.
2. **Directorio Local de Evidencias (`ProfilingSessionController.cpp:761`):**
   * *Actual:* Copia condicional desde `guided/session.json`.
   * *Objetivo:* Estructura unificada de sesión y evidencias para todos los flujos.

---

## 7. Referencias

- `docs/ROADMAP.md` v2.4.0 — §3b (PARITY-01 e HITO-09)
- `PLAN_PARITY_01.md` — Plan de auditoría y caracterización
- `MATRIX_PARITY_01.md` — Matriz de las 8 dimensiones y 3 rutas
