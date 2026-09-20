# ACTA DE CIERRE: HITO-03.1-STEPPER-COHERENCE

**Fecha**: 2026-09-20  
**Proyecto**: ABDAudioLab  
**Responsable Arquitectura**: Antigravity (Lead Architect & Planner)  
**Entorno de Ejecución**: Windows 11 / Visual Studio 2022 (MSVC 19.4) / CMake / Release x64  
**Área de Impacto**: Navegación de Stepper, Selector Único, Calibración Condicionada e Invalidación de Sesión  

---

## 1. Declaración de Alcance y Correcciones Incorporadas

El presente corte técnico resuelve la coherencia de interacción y flujo entre el Paso 1 (Target & Routing), el Paso 2 (Calibration & Setup) y el Paso 3 (Run Session), preservando estrictamente la cadena de ejecución certificada en los Hitos 1–3 y aplicando las dos correcciones normativas obligatorias:

1. **Corrección 1 (Verificación Digital ≠ Bypass Automático)**:
   - Para plugins VST3, el loopback físico no es aplicable (`audio.requirement = NotApplicable`).
   - La ruta digital es requerida (`digital.requirement = Required`), pero **no** se declara verificada de forma automática (`digital.verified = false`).
   - La verificación se efectúa a través de una comprobación activa de preparación de audio/latencia (`verifyDigitalCalibration()`). `bypassed` no se confunde con `verified`.
2. **Corrección 2 (Capacidades Reales de Plugin VST3 ≠ Asunción Universal de MIDI)**:
   - Los plugins VST3 no asumen universalmente excitación MIDI.
   - El modo `ExcitationMode::AutomatedMidi` se habilita **únicamente** si el contrato del plugin declara `supportsMidiInput = true`.
   - Si no dispone de MIDI pero admite automatización de parámetros, adopta `AutomatedVstParameter`. En ausencia de ambos, recurre a `ManualOperator`.
3. **Selector Único y Autoridad Centralizada**:
   - `catalogSelector` (`SoundIdHardwareCatalogSelector`) es la **única autoridad** interactiva para seleccionar targets.
   - `SoundIdTargetView` es una ficha **pasiva y viva** (el botón emergente de selección ha sido desactivado y retirado del layout).
   - `HardwareSelectorPill` en el header superior es un indicador pasivo que navega directamente a Paso 1 (`HardwareRouting`) sin generar selección divergente.
4. **Reordenación Coherente del Stepper**:
   - Secuencia visual:
     - `0. Studio Environment`
     - `1. Target & Routing` (`Step::HardwareRouting`)
     - `2. Calibration & Setup` (`Step::CalibrateLoopback`)
     - `3. Run Session` (`Step::RunSession`)
     - `4. Export & Report` (`Step::ExportReport`)
   - **Compatibilidad total preservada**: Los valores de los enums internos se conservaron intactos (`Step::HardwareRouting = 2`, `Step::CalibrateLoopback = 1`, etc.). La reordenación se gobierna mediante mapeo desacoplado de orden visual (`visualOrder`).
5. **Regla de Invalidación de Sesión y Receta**:
   - El cambio de target invalida recetas previas (`recipe.status = RecipeStatus::IncompatibleWithTarget;`), cancela sesiones en curso, reinicia el estado ortogonal de calibración e incrementa el contador de generación del controlador.

---

## 2. Resultados de la Suite Automatizada

Compilación limpia en configuración `Release` tanto del ejecutable de pruebas como de la aplicación de usuario:
- `build\Release\ABDAudioLab_Tests.exe`
- `build\ABDAudioLab_artefacts\Release\ABDAudioLab.exe`

```
Filtros Específicos Ejecutados:
  [coherence]                    -->  65 assertions in 18 test cases  (100% PASS)
  [stepper][excitation]          -->  58 assertions in 14 test cases  (100% PASS)
====================================================================================
SUITE GLOBAL COMPLETA          --> 228.169 assertions in 564 test cases (100% PASS - 0 FALLOS)
  - 564 casos totales PASS
  - 546 casos preexistentes conservados y PASS
  - 18 casos nuevos de coherencia PASS
```

---

## 3. Matriz de Cobertura de Pruebas: ST-47 a ST-68

| Test ID | Área / Requisito | Propósito y Verificación | Resultado |
|---|---|---|---|
| **ST-47** | Selector Único | Invariante: `catalogSelector` es la única autoridad; TargetView es pasivo | **PASS** |
| **ST-48** | Stepper Reordenado | Títulos y mapeo visual: Paso 1 = Target & Routing, Paso 2 = Calibration & Setup | **PASS** |
| **ST-49** | Compatibilidad Enum | Los valores numéricos del enum `Step` son idénticos a las sesiones anteriores | **PASS** |
| **ST-50** | Calibración VST3 | VST3 establece `audio = NotApplicable` y `digital = Required` | **PASS** |
| **ST-51** | Calibración Hardware | Analógico establece `audio = Required`, `digital = NotApplicable` | **PASS** |
| **ST-52** | Calibración MIDI HW | Hardware MIDI establece `audio = Required`, `midi = Required` | **PASS** |
| **ST-53** | Verificación Digital | `digital.verified` inicia en `false` y requiere `verifyDigitalCalibration()` | **PASS** |
| **ST-54** | Compuerta de Avance | `isReadyForProfiling()` requiere verificación o completitud según el target | **PASS** |
| **ST-55** | Invalidación Target | Cambio de target invalida receta (`IncompatibleWithTarget`) y resetea calibración | **PASS** |
| **ST-56** | Generación Monotónica | Cambio de target incrementa `controllerGeneration` y renueva `sessionId` | **PASS** |
| **ST-57** | Selector Pill | `HardwareSelectorPill` solo navega a `HardwareRouting` (Paso 1) | **PASS** |
| **ST-58** | Panel Calibración | `NativeCalibrationPanel` adapta interfaz entre modo analógico y digital | **PASS** |
| **ST-59** | VST3 sin MIDI | VST3 sin MIDI no ofrece `AutomatedMidi` y pasa a parámetros o manual | **PASS** |
| **ST-60** | VST3 Automatización | VST3 con parámetros ofrece `AutomatedVstParameter` | **PASS** |
| **ST-61** | Verificación vs Bypass | `digital.verified` no se enciende automáticamente por bypass | **PASS** |
| **ST-62** | Cancelación Activa | Cambio de target durante sesión activa solicita cancelación segura | **PASS** |
| **ST-63** | Calibración Huérfana | Calibración huérfana de target anterior bloquea `isReadyForProfiling()` | **PASS** |
| **ST-64** | Compatibilidad Sesión | Deserialización de sesión heredada conserva el significado de sus pasos | **PASS** |
| **ST-65** | Hash de Receta | El hash de receta vincula la identidad del target para evitar reutilización | **PASS** |
| **ST-66** | TargetView Pasivo | `SoundIdTargetView` no dispara eventos de cambio de target | **PASS** |
| **ST-67** | Navegación Pill | Pill no altera el target activo, únicamente el paso activo | **PASS** |
| **ST-68** | Idempotencia Eventos | Selección única de plugin no genera eventos duplicados en el controlador | **PASS** |

---

## 4. Estado de la Base de Código y Archivos Modificados

1. **`src/gui/session/ProfilingSessionContracts.h`**:
   - Tipos ortogonales: `RecipeStatus`, `CalibrationRequirement`, `AudioCalibrationState`, `MidiCalibrationState`, `DigitalPathCalibrationState`, `CalibrationStatus`.
   - Capacidades contractuales en `TargetSelectionState` (`supportsMidiInput`, `supportsParameterAutomation`, `supportsMidiCc`, `supportsSysEx`).
   - Métodos de comando en `IProfilingSessionCommands`.
2. **`src/gui/session/ProfilingSessionController.h` / `.cpp`**:
   - Gestión de calibración ortogonal, invalidación de receta ante cambio de target, incremento monotónico de generación.
3. **`src/gui/soundid/SoundIdTargetView.h` / `.cpp`**:
   - `selectPluginButton_` desactivado y retirado del layout; vista exclusivamente de telemetría y especificaciones de target.
4. **`src/gui/HardwareSelectorPill.h`**:
   - Indicador pasivo con navegación directa a Paso 1 (`HardwareRouting`).
5. **`src/gui/soundid/SoundIdSidebarStepper.h` / `.cpp`**:
   - Reordenación visual conservando valores de enum, mapeo de orden `visualOrder`.
6. **`src/gui/WorkflowStepperBar.h`**:
   - Reordenación visual coherente con los nodos del workflow.
7. **`src/gui/NativeCalibrationPanel.h` / `.cpp`**:
   - Adaptación visual a modo digital vs loopback analógico y botón para verificación digital de latencia.
8. **`src/gui/soundid/SoundIdExcitationConfigPanel.cpp`**:
   - Verificación de `supportsMidiInput` previo a ofrecer `AutomatedMidi`.
9. **`src/gui/MainContentComponent.cpp`**:
   - Conexión de `catalogSelector` a `profilingSessionController.selectTarget` con lectura de capacidades de plugins y contratos hardware.
10. **`src/tests/test_StepperCoherence_ST47_ST68.cpp`**:
    - Suite de regresión y certificación ST-47 a ST-68.

---

## 5. Veredicto Final

**HITO-03.1-STEPPER-COHERENCE: CERTIFICADO Y CERRADO FORMALMENTE**.
- Selector único operativo y validado.
- Stepper reordenado coherentemente sin romper compatibilidad.
- Calibración condicionada implementada con verificación real para VST3.
- Reglas de invalidación de recetas y sesiones activadas.
- 564 casos de prueba verdes (0 fallos).
