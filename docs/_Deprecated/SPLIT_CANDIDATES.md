# Archivos candidatos a división — ABDAudioLab

Criterio: archivos grandes (>300 líneas), responsabilidades mezcladas, y oportunidades DRY.

> **Actualizado:** Verificación de splits existentes vs pendientes.

---

## Estado general

| Prioridad | Archivo | Líneas actuales | Estado | Siguiente paso |
|-----------|---------|-----------------|--------|----------------|
| 🔴 P1 | `main.cpp` | 1,594 | ⚠️ `SessionManager` y `HardwareManager` existen como clases completas pero `main.cpp` solo los incluye sin usarlos (includes muertos) | Integrar managers → eliminar ~200 líneas inline. **NO hacer durante fix de errores** — alto riesgo de regresión. |
| 🔴 P2 | `SlideInDrawer.cpp` | 1,151 (.cpp) | ⚠️ `TestEditorPanel` extraído (DRY resuelto). Falta `AssetLocator` y 4 drawer views | Extraer `locateAssetFile()` y crear drawer views |
| 🟠 P3 | `SoundIdSuiteList.cpp` | 671 (.cpp) | 🔴 Sin cambios | Extraer `QueueItemModel` + `QueueRowRenderer` |
| 🟠 P4 | `TestConfigModal.cpp` | 126 (.cpp) | ✅ **RESUELTO** — `TestEditorPanel` extraído, archivo reducido a 126 líneas | Solo mover structs a header compartido |
| 🟡 P5 | `OperatorStepModalDialog.h` | 270 | ✅ **RESUELTO** — `HardwareControlRenderer` extraído, `drawTargetValueBadge()` DRY | Ninguno |
| 🟡 P6 | `ProfilingSequencer.cpp` | 442 (.cpp) | 🟡 Sin cambios | Extraer WAV export + export orchestrator |
| 🟢 P7 | `SoundIdCurvePlotter.cpp` | 436 (.cpp) | 🟡 Sin cambios | Extraer ViridisColormap + renderers |
| 🟢 P8 | `LabAnalyticEngine.cpp` | 421 (.cpp) | 🟡 Sin cambios | Extraer analyzers por dominio |

---

## 🔴 P1 — `main.cpp` (1,594 líneas) — AÚN MEZCLADO

### Estado: `SessionManager` y `HardwareManager` existen pero son huérfanos

| Archivo sugerido | Existe? | Integrado en main.cpp? |
|---|---|---|
| `core/SessionManager.h/.cpp` | ✅ Sí (56 líneas) | ❌ **NO** — `main.cpp` aún posee `sessionSerializer`, `buildCurrentSessionManifest()`, `applyLoadedSession()`, dirty tracking, todo save/load/open |
| `core/HardwareManager.h/.cpp` | ✅ Sí (57 líneas) | ❌ **NO** — `main.cpp` aún posee `mockController`, `airaController`, `midiCcController`, `manualController`, `contractRegistry`, `onHardwareSelected()` |
| `core/ProfilingSessionBuilder.h/.cpp` | ❌ No | `buildProfilingSessionFromQueue()` sigue en `main.cpp:1005-1130` (~125 líneas) |
| `gui/MonochromeInfoButton.h` | ❌ No | Componente inline en `main.cpp:51-83` |
| `gui/MainContentComponent.h/.cpp` | ❌ No | Clase gigante sin archivo propio |

### Acción requerida
1. **Conectar** `main.cpp` para usar `SessionManager` y `HardwareManager` que ya existen
2. **Crear** `ProfilingSessionBuilder` extrayendo `buildProfilingSessionFromQueue()`
3. **Extraer** `MainContentComponent` a su propio archivo

### DRY pendiente
- `mapBadgeToBlockType()` duplicado en `main.cpp` ↔ `SlideInDrawer.cpp`
- `mapHardwareIdToAiraModel()` repetido en 3 sitios de `main.cpp`
- Badge color mapping (FLT/ENV/SAT/MOD) duplicado en `main.cpp:349-352` y `1197-1200`

---

## 🔴 P2 — `SlideInDrawer.cpp` (1,151 líneas) — PARCIALMENTE RESUELTO

### Lo que ya se hizo
- ✅ `TestEditorPanel` extraído y compartido con `TestConfigModal` — elimina ~300 líneas de duplicación

### Lo que falta

| Archivo sugerido | Existe? |
|---|---|
| `utils/AssetLocator.h/.cpp` | ❌ — `locateAssetFile()` sigue en `SlideInDrawer.cpp:7-75` (68 líneas) |
| `gui/drawer/FileDrawerView.h/.cpp` | ❌ |
| `gui/drawer/HardwareDrawerView.h/.cpp` | ❌ |
| `gui/drawer/TestEditorDrawerView.h/.cpp` | ❌ |
| `gui/drawer/SetupDrawerView.h/.cpp` | ❌ |

### DRY resuelto
- ✅ Stimulus type combo (compartido vía `TestEditorPanel`)
- ✅ Duration preset combo (compartido vía `TestEditorPanel`)
- ✅ `updateEstimatedTime()` (compartido vía `TestEditorPanel`)
- ✅ Control row widget construction (compartido vía `TestEditorPanel`)

### DRY pendiente
- `locateAssetFile()` — extraer a utilidad compartida

---

## 🟠 P3 — `SoundIdSuiteList.cpp` (671 líneas) — SIN CAMBIOS

### División sugerida

| Nuevo archivo | Responsabilidad | ~Líneas |
|---|---|---|
| `gui/QueueItemModel.h/.cpp` | `QueueItem`, `QueueItemStatus`, CRUD, dedup, pin, move | ~150 |
| `gui/QueueRowRenderer.h/.cpp` | `paintRow()`, `drawBadge()`, `drawStatusPill()`, sub-row rendering | ~250 |
| `gui/SoundIdSuiteList.h/.cpp` | Orquestador delgado: modelo + renderer + callbacks | ~200 |

### DRY pendiente
- Layout magic numbers (36px, 38px, 290px, 60px) hardcodeados independientemente en `paint()` y `mouseDown()`

---

## 🟠 P4 — `TestConfigModal.cpp` (126 líneas) — ✅ RESUELTO

`TestEditorPanel` extraído. `TestConfigModal` ahora es un wrapper modal delgado.

**Pendiente menor:** Mover `ControlStepConfig` y `TestConfiguration` structs a un header compartido (son usados por `SlideInDrawer`, `SoundIdSuiteList`, `main.cpp`).

---

## 🟡 P5 — `OperatorStepModalDialog.h` (270 líneas) — ✅ RESUELTO

`HardwareControlRenderer` extraído. `drawTargetValueBadge()` consolidado (DRY). Sin pendientes.

---

## 🟡 P6 — `ProfilingSequencer.cpp` (442 líneas) — SIN CAMBIOS

### División sugerida

| Nuevo archivo | Responsabilidad | ~Líneas |
|---|---|---|
| `core/RawAudioExporter.h/.cpp` | WAV writing, adaptive trimming | ~80 |
| `core/SequencerExportOrchestrator.h/.cpp` | LUT, JSON, manifest, noise timeline export | ~60 |
| `core/ProfilingSequencer.h/.cpp` | Thread loop, operator prompts, analytic dispatch | ~300 |

---

## 🟢 P7 — `SoundIdCurvePlotter.cpp` (436 líneas) — SIN CAMBIOS

### División sugerida

| Nuevo archivo | Responsabilidad | ~Líneas |
|---|---|---|
| `gui/ViridisColormap.h/.cpp` | Reusable Viridis color lookup | ~50 |
| `gui/FrequencyPlotRenderer.h/.cpp` | `drawFrequencyPlot()` + dB/freq grid | ~150 |
| `gui/HeatmapPlotRenderer.h/.cpp` | `drawHeatmap2D()` + color bar | ~80 |
| `gui/SoundIdCurvePlotter.h/.cpp` | Tab switching, legend, delega a renderers | ~150 |

---

## 🟢 P8 — `LabAnalyticEngine.cpp` (421 líneas) — SIN CAMBIOS

### División sugerida

| Nuevo archivo | Responsabilidad | ~Líneas |
|---|---|---|
| `math/Statistics.h` | `calculateStatistics()`, `calculateSNR()` | ~50 |
| `math/FilterAnalyzer.h/.cpp` | `analyzeFilterPasses()` | ~40 |
| `math/EnvelopeAnalyzer.h/.cpp` | `analyzeAdsrEnvelopes()`, `analyzeDelayImpulses()` | ~120 |
| `math/WaveShaperAnalyzer.h/.cpp` | `analyzeWaveShaperRamps()` | ~70 |
| `math/GainAnalyzer.h/.cpp` | `analyzeGainTones()` | ~30 |
| `math/CyclicModulatorAnalyzer.h/.cpp` | `analyzeCyclicModulator()` | ~70 |

---

## Resumen DRY cross-file

| Patrón | Estado |
|--------|--------|
| Stimulus type combo | ✅ Resuelto (TestEditorPanel) |
| Duration preset combo | ✅ Resuelto (TestEditorPanel) |
| `updateEstimatedTime()` | ✅ Resuelto (TestEditorPanel) |
| Control row widget construction | ✅ Resuelto (TestEditorPanel) |
| `mapBadgeToBlockType()` | ❌ Pendiente — duplicado en main.cpp ↔ SlideInDrawer |
| `mapHardwareIdToAiraModel()` | ❌ Pendiente — 3 sitios en main.cpp |
| Badge color mapping | ❌ Pendiente — duplicado en main.cpp |
| Layout magic numbers | ❌ Pendiente — SoundIdSuiteList paint vs mouseDown |
| **Total eliminado** | **~400 líneas** |
| **Total pendiente** | **~100 líneas** |

---

## Quick Win actualizado

| # | Acción | Impacto | Esfuerzo |
|---|--------|---------|----------|
| 1 | Conectar `main.cpp` a `SessionManager` + `HardwareManager` existentes | 🔴 Alta | Medio |
| 2 | Extraer `ProfilingSessionBuilder` de `main.cpp` | 🔴 Alta | Bajo |
| 3 | Mover `ControlStepConfig`/`TestConfiguration` a header compartido | 🟠 Media | Bajo |
| 4 | Extraer `locateAssetFile()` a `utils/AssetLocator.h` | 🟠 Media | Bajo |
| 5 | Extraer `QueueItemModel` + `QueueRowRenderer` de `SoundIdSuiteList` | 🟠 Media | Medio |
| 6 | Unificar layout magic numbers en `SoundIdSuiteList` | 🟡 Baja | Bajo |
| 7 | Consolidar `mapBadgeToBlockType()` y `mapHardwareIdToAiraModel()` | 🟡 Baja | Bajo |
| 8 | Extraer `ViridisColormap` de `SoundIdCurvePlotter` | 🟢 Baja | Bajo |
