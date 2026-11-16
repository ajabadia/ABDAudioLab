# Guía de Migración v1.x → v2.x — ABDAudioLab

**Versión:** 2.1.0
**Fecha:** 2026-10-13
**Audiencia:** Usuarios de ABDAudioLab v1.x, desarrolladores de plugins, integradores

---

## 1. Resumen de Cambios Arquitectónicos

| Aspecto | v1.x (Legacy) | v2.x (Actual) | Impacto Migración |
|---------|---------------|---------------|-------------------|
| **Motor de ejecución** | Múltiples secuenciadores por modo | **Único**: `ProfilingSequencer` | Transparente — mismo API de alto nivel |
| **Modos de trabajo** | Guiado / Clásico bifurcados | **Unificado**: `ExperimentPlan` canónico | Presets de rol compilan al mismo plan |
| **Perfiles hardware** | JSON sueltos en `contracts/hardware/` | **TargetProfile** declarativo (`.target.json`) | Migración automática vía `TargetProfileLegacyAdapter` |
| **Calibración loopback** | Manual cada sesión | **`.abdloop`** persistente + autodetección | Cero acción si hardware ya calibrado |
| **Recetas medición** | Hardcodeadas en C++ | **MeasurementRecipe** declarativo (JSON) | Catálogo extensible sin recompilar |
| **Exportación** | Múltiples exportadores paralelos | **Único**: `ReportExportService` (ADR-13) | Un solo reporte, metadatos diferencian modo |
| **Plugins VST3** | In-process | **Out-of-process** (`PluginWorker`) | Aislamiento crashes, misma API |
| **Formatos persistencia** | `.abdlabtest` v1 | `.abdlabtest` v2 (schemaVersion: 2) | Round-trip automático v1→v2 |
| **Clonación/Perfiles** | Formatos ad-hoc | **`.abdprofile`** (ZIP estándar) | Portabilidad total entre máquinas |

---

## 2. Migración de Datos de Usuario

### 2.1 Sesiones Anteriores (`.abdlabtest`)

**Automática:** Al abrir una sesión v1.x en v2.x:
1. `SessionSerializer` detecta `schemaVersion: 1` (o ausencia)
2. Migra internamente a v2 (`schemaVersion: 2`)
3. Guarda como v2 al cerrar/guardar
4. **Backup automático**: `.abdlabtest.v1.bak` antes de sobrescribir

```cpp
// Comportamiento interno (SessionSerializer.cpp)
bool loadSessionFromPackage(File package, SessionManifest& manifest, ...)
{
    int schemaVersion = manifest.getProperty("schemaVersion", 1); // default v1
    if (schemaVersion == 1) {
        migrateV1ToV2(manifest, measuredPoints); // transparente
        manifest.setProperty("schemaVersion", 2);
    }
    return true;
}
```

**Verificación:** `test_SessionSerializer.cpp` — casos `RoundTripV1ToV2`, `RoundTripV2ToV2`.

### 2.2 Perfiles de Hardware Legacy (JSON en `contracts/hardware/`)

**Estado:** **3 perfiles migrados y retirados** (HITO-10E certificado):
- `behringer_pro800.json` → `profiles/targets/behringer_pro800.target.json`
- `yamaha_dx7.json` → `profiles/targets/yamaha_dx7.target.json`
- `boss_ds1_distortion.json` → `profiles/targets/boss_ds1_distortion.target.json`

**28 perfiles legacy restantes** — mantenidos como fallback (no migrados aún):
- Se cargan vía `HardwareContractRegistry` con precedencia canónica sobre legacy
- `HardwareContractResolutionSource` = `Legacy` vs `Canonical`
- **No acción requerida** — funcionan transparente

**Para migrar un perfil legacy manualmente:**
```bash
# 1. Copiar legacy a profiles/targets/
cp contracts/hardware/mi_synth.json profiles/targets/mi_synth.target.json

# 2. Editar: añadir campos TargetProfile obligatorios
#    - targetKind: "HardwareMidi" | "PluginVST3" | "ManualOperator"
#    - canonicalTargetId: "mi-synth"
#    - transport: "MidiCC" | "MidiSysEx" | "InProcessVirtualMidi"
#    - audio: "PhysicalInterface" | "InProcessPluginAudio"
#    - schemaVersion: 1

# 3. Validar
.\build\Release\ABDAudioLab_Tests.exe "[targetprofile]" -- "Validation"
```

### 2.3 Calibraciones Loopback (`.abdloop`)

**Nuevo en v2.x:** Formato `.abdloop` con SHA-256, autodetección por `hardware_key`.

**Migración:** Automática en primera ejecución v2.x con hardware ya calibrado:
1. Sistema calcula `hardware_key = sanitizedName + "_" + sampleRate + "_" + bufferSize`
2. Busca en `%APPDATA%/ABDAudioLab/loopback_profiles/<key>.abdloop`
3. Si existe y hash íntegro → **Paso 2 marcado `LoadedFromHistory`**, bypass automático
4. Si no existe → guía calibración normal + guarda `.abdloop` al final

**Ubicación legacy:** v1.x guardaba en `exports/loopback_calibration.json` — **ya no se lee**.

### 2.4 Exportaciones Anteriores (LUT, NAM, HTML)

| Formato v1.x | Formato v2.x | Compatibilidad |
|--------------|--------------|----------------|
| `exported_luts/<Nombre>_LUT.h` | Idéntico + `alignas(16)` | ✅ Compatible — mismo include |
| `exports/nam/<Nombre>/` | Idéntico + `manifest.json` v2 | ✅ Compatible — NAM standard |
| `exports/reports/<Nombre>.html` | HTML5 autocontenido + SVG | ✅ Compatible — abre en navegador |

**No requiere migración** — formatos de salida estables.

---

## 3. Cambios en API para Desarrolladores/Integradores

### 3.1 ExperimentPlan (Nuevo Tipo Canónico)

```cpp
// v1.x: Múltiples structs según modo
// v2.x: Un solo tipo
struct ExperimentPlan
{
    String workflowMode;           // "Guided" | "Classic" | "Free"
    String targetProfileId;        // UUID TargetProfile
    MeasurementRecipe recipe;      // Receta declarativa
    CalibrationRequirements calib; // audio/midi/digital requirements
    ExportSpecification exportSpec; // LUT, NAM, HTML, etc.
};
```

**Migración código cliente:**
```cpp
// ANTES (v1.x)
if (isGuidedMode) {
    auto plan = buildGuidedPlan(target, recipe);
} else {
    auto plan = buildClassicPlan(target, recipe);
}

// DESPUÉS (v2.x) — un solo camino
auto plan = ExperimentPlanCompiler::compile(targetProfile, recipe, assistanceLevel);
// assistanceLevel: "Rapido" | "Configurable" | "Avanzado"
```

### 3.2 TargetProfile API

```cpp
// Cargar perfil (nuevo)
auto profile = TargetProfile::loadFromFile(profilePath); // retorna std::optional<TargetProfile>

// Validar contra schema
auto result = TargetProfileValidator::validate(profile);
if (!result.isValid) {
    for (auto& err : result.errors) log(err.jsonPointer, err.message);
}

// Resolver plan de ejecución
auto resolvedPlan = TargetProfileResolver::resolve(profile, recipe);
// resolvedPlan.resolvedExecutionPlanHash — hash canónico para reproducibilidad
```

### 3.3 Hardware Controller (Unificado)

```cpp
// v1.x: Múltiples factories según tipo
// v2.x: Factory único + contrato
std::unique_ptr<IHardwareController> controller = 
    HardwareControllerFactory::create(profile.transport, profile.audio);

// Capacidades desde contrato (no hardcodeadas)
bool supportsMidi = profile.capabilities.midi;
bool supportsSysEx = profile.capabilities.sysex;
bool requiresPhysicalPort = profile.requiresPhysicalMidiPort;
```

### 3.4 Plugin Worker (Out-of-Process)

```cpp
// v1.x: Directo en proceso principal
// v2.x: A través de WorkerProcessHost (IPC Named Pipes)

WorkerProcessHost worker;
worker.launch(); // Inicia ABDAudioLab_PluginWorker.exe

// Cargar plugin en worker
auto result = worker.loadPlugin(pluginPath); // Async, retorna Future<PluginLoadResult>

// Usar plugin (transparentemente)
auto processor = worker.createProcessor(); // Retorna proxy que marshalleaa IPC

// Crash del plugin → worker muere, host sigue vivo
worker.onCrash = [] { log("Worker crashed, session recovered"); };
```

---

## 4. Migración de Presets y Recetas

### 4.1 Presets de Rol → MeasurementRecipe

| Preset v1.x (Hardcodeado) | MeasurementRecipe v2.x (Declarativo) |
|---------------------------|--------------------------------------|
| "SpectrumFilter" (15 pts Farina) | `FilterSweepRecipe` — `excitationType: "LogFarinaSweep"`, `points: 15` |
| "TimeDynamic ADSR" | `AdsrPulseRecipe` — `excitationType: "SyncPulses3"`, `gateMs: 250` |
| "WaveShaper Saturation" | `WaveShaperRampRecipe` — `excitationType: "LinearRamp"`, `range: [0,1]` |
| "Gain/VCA" | `GainToneRecipe` — `excitationType: "ToneBurst"`, `frequencies: [100,1k,10k]` |
| "Chorus/Modulator" | `ModulationPRBSRecipe` — `excitationType: "PRBS"`, `rate: 5Hz` |

**Catálogo v2.x:** `presets/recipes/*.recipe.json` (3 canónicas + extensibles)

### 4.2 Niveles de Asistencia (HITO-09C)

| Nivel | Descripción | Uso |
|-------|-------------|-----|
| **Rápido** | Preset completo, 0 decisiones | Músico — "un clic" |
| **Configurable** | Preset base + edición parámetros clave | Productor — ajuste fino |
| **Avanzado** | Receta completa editable + matriz 4x8 | Ingeniero — control quirúrgico |

**Todos comparten** el mismo `ExperimentPlan` compilado.

---

## 5. Breaking Changes (Requieren Acción)

| Cambio | Acción Requerida | Afecta |
|--------|------------------|--------|
| `SessionSerializer::loadSessionFromPackage` firma cambiada (4 output params → struct) | Actualizar llamadas | Código cliente que carga sesiones |
| `HardwareContractRegistry::findContractById` retorna `std::optional<>` en lugar de raw ptr | Check `.has_value()` | Código que busca contratos |
| `ProfilingSequencer::setHardwareController` toma `std::shared_ptr<>` en lugar de raw ptr | Pasar `shared_ptr` | Código que inyecta controlador |
| `LabAnalyticEngine` funciones de análisis renombradas + signatures | Actualizar llamadas | Código que usa análisis directo |
| `MeasurementRecipe` reemplaza `MidiRecipe` / `ManualOperatorRecipe` | Migrar a `MeasurementRecipe` | Código que construye recetas |

---

## 6. Deprecations (Funcionan pero No Usar en Código Nuevo)

| API Legado | Reemplazo v2.x | Fecha Retirada Estimada |
|------------|----------------|-------------------------|
| `MidiRecipe` / `ManualOperatorRecipe` | `MeasurementRecipe` | v3.0 |
| `LoopbackCalibrationModal` (GUI) | `NativeCalibrationPanel` + `.abdloop` auto | v2.5 |
| `stepperBar` (Stepper legacy) | `SoundIdSidebarStepper` + `WorkflowNavigationController` | v2.5 (HITO-06) |
| `loopbackModal` (Calibración legacy) | `CalibrationSnapshot` + autodetección | v2.5 (HITO-06) |
| `RoutingValidator` (hardware AIRA) | `TargetProfile.transportPolicy` + `HardwareDispatchScheduler` | v3.0 |
| `AiraRoutingValidator` | **Eliminar** — clon dead code | Inmediato (v2.1.1) |
| `StereoVuMeter`, `LiveCurvePlotter`, `InfoDrawer` | **Eliminar** — dead code | Inmediato (v2.1.1) |
| `SplineInterpolator2D` | **Eliminar** — dead code | Inmediato (v2.1.1) |

---

## 7. Scripts de Migración Asistida

### 7.1 Migrar Sesiones v1.x en Lote

```powershell
# scripts/migrate_sessions_v1_to_v2.ps1
param(
    [string]$SourceDir = "$env:APPDATA\ABDAudioLab\sessions",
    [string]$BackupDir = "$env:APPDATA\ABDAudioLab\sessions_v1_backup_$(Get-Date -Format 'yyyyMMdd')"
)

# 1. Crear backup
Copy-Item $SourceDir\*.abdlabtest $BackupDir -Force

# 2. Ejecutar migración vía app (headless)
# Requiere build con flag -DMIGRATION_TOOL=ON
.\build\Release\ABDAudioLab_MigrationTool.exe --migrate-sessions --source $SourceDir

# 3. Verificar
.\build\Release\ABDAudioLab_Tests.exe "[session]" -- "RoundTripV1ToV2"
```

### 7.2 Validar Perfiles Legacy

```powershell
# scripts/validate_legacy_profiles.ps1
.\build\Release\ABDAudioLab_Tests.exe "[targetprofile]" -- "LegacyInventory"
# Output: docs/audits/TARGETPROFILE_LEGACY_INVENTORY.md actualizado
```

### 7.3 Verificar Hermeticidad Rutas (POST-5D.5)

```powershell
# Desde raíz repo
.\build\Release\ABDAudioLab_Tests.exe "[hygiene]"

# Desde build/Release
cd build\Release
.\ABDAudioLab_Tests.exe "[hygiene]"
# Ambos deben dar 8 casos PASS, 0 fallos
```

---

## 8. Troubleshooting Común

| Síntoma | Causa Probable | Solución |
|---------|----------------|----------|
| **Paso 2 (Calibración) no se salta** | `.abdloop` no encontrado o hash inválido | Verificar `%APPDATA%/ABDAudioLab/loopback_profiles/` — recalibrar |
| **Target no aparece en catálogo** | Perfil legacy no migrado / TargetProfile inválido | `test_TargetProfileValidation` — revisar errores schema |
| **Plugin VST3 crashea la app** | Worker no lanzado / IPC fallado | Verificar `ABDAudioLab_PluginWorker.exe` en mismo dir que `.exe` |
| **Tests fallan en CI pero pasan local** | Dependencia CWD (resuelto POST-5D.5) | Ejecutar desde `build/Release` también |
| **Exportación LUT no compila en plugin destino** | Sanitización nombre tabla | Verificar `LutExporter` genera `lut_` prefix si empieza con dígito |
| **Audio glitches / xruns** | Data races DSP (deuda conocida) | Workaround: aumentar buffer size; Fix: P0 thread safety |

---

## 9. Rollback v2.x → v1.x (Si Necesario)

```powershell
# 1. Restaurar sesiones desde backup
Copy-Item $env:APPDATA\ABDAudioLab\sessions_v1_backup_*\*.abdlabtest $env:APPDATA\ABDAudioLab\sessions\ -Force

# 2. Eliminar perfiles .abdloop (v1.x no los usa)
Remove-Item $env:APPDATA\ABDAudioLab\loopback_profiles\*.abdloop -Force

# 3. Reinstalar v1.x (installer previo)
# 4. Verificar: abrir sesión, calibrar manualmente, exportar
```

> **Nota:** v2.x escribe `schemaVersion: 2` en sesiones. v1.x **no puede leer** sesiones v2.x. El backup previo a migración es crítico.

---

## 10. Referencias y Documentación Vinculada

| Documento | Ubicación | Relevancia Migración |
|-----------|-----------|---------------------|
| **ARCHITECTURE.md** | `docs/ARCHITECTURE.md` | Arquitectura v2.x completa |
| **PLAN.md** | `docs/PLAN.md` | Hitos certificados, secuencia vinculante |
| **HANDOFF.md** | `docs/HANDOFF.md` | Estado ejecutivo, autoridades, riesgos |
| **CODE_QUALITY_REPORT.md** | `docs/CODE_QUALITY_REPORT.md` | Deuda técnica conocida (15 CRITICAL) |
| **TARGETPROFILE_LEGACY_INVENTORY.md** | `docs/audits/` | Inventario 31 perfiles legacy |
| **TARGETPROFILE_EQUIVALENCE_MATRIX.md** | `docs/audits/` | Matriz equivalencia legacy↔canónico |
| **ACTA_HITO_10E_LEGACY_REMOVAL.md** | `docs/audits/` | Evidencia retirada 3 JSON legacy |
| **POST_5D5_HARDCODED_PATHS_INVENTORY.md** | `docs/audits/` | Resolución rutas hermética |

---

## 11. Soporte Migración

| Canal | Contacto |
|-------|----------|
| **Issues migración** | GitHub: `ABDSynths/ABDAudioLab/issues` (label `migration`) |
| **Email técnico** | `migration@abdsynths.com` |
| **Discord** | `#abdaudiolab-migration` |

---

*Guía actualizada por release. v2.1.0 → v2.2.0: añadir migración `.abdprofile` v1→v2 si aplica.*