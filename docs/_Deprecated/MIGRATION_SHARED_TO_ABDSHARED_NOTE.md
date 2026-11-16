# 13 Oct 2026 — Migración completada a ABDShared (Nota Operativa)

Este documento registra la finalización de la migración de **ABDAudioLab** desde los repositorios legados `ABDSharedCode` y `ABDSharedAssets` hacia la estructura consolidada de **`ABDShared`**.

---

## 1. Estado de la Migración

La migración se ha completado y verificado en compilación:
- `ABDAudioLab_Tests.exe` compila y enlaza limpiamente (`exit code 0`) contra los módulos canónicos de `ABDShared::*`.
- La resolución de rutas en tiempo de ejecución (`Assets` y `Contracts`) se ha actualizado con soporte nativo para el nuevo monorepo y compatibilidad hacia atrás.

---

## 2. Módulos Portados a `ABDShared/Code`

Antes de reorientar el build, se verificó que `ABDShared/Code` carecía de varios módulos críticos utilizados por `ABDAudioLab`. Se portaron íntegramente a `ABDShared/Code` (commit `8dfa12d` en `main` de `ABDShared`):

1. **`HardwareMidiDetect/`**:
   - Detección reactiva de puertos MIDI y estado de conexión de hardware.
   - Recursos WebUI incrustados mediante `juce_add_binary_data` (`ABDShared::HardwareMidiPickerAssets`).
2. **`StudioTopology/`**:
   - Visualizador interactivo de topología de estudio.
   - Recursos Web incrustados con `juce_add_binary_data` (`ABDShared::StudioTopologyAssets`).
3. **`MidiKeyboard/`**:
   - Backend C++ para puente con WebView2 (`ABDShared::MidiKeyboardCpp`).
4. **`AudioComparator/`**:
   - Motor de comparación de audio A/B, análisis espectral, alineamiento temporal y veredictos 5D (`ABDShared::AudioComparator`).
5. **`visualizers/`**:
   - Componentes 3D de cascada espectral (`Waterfall3DComponent`, `PreScanPoint`).
6. **`tests/`**:
   - Batería de tests de seguridad de endpoints MIDI y políticas de apertura de puertos.
7. **`Certification/`**:
   - Herramientas de auditoría (`CertificationAuditor.py`, `certification_manifest.schema.json`).

---

## 3. Cambios en `ABDAudioLab`

### A. Sistema de Compilación (`CMakeLists.txt`)
- Se reorientó la variable `ABDSHARED_CODE_DIR` para buscar primero `${ABDSHARED_DIR}/Code` (`../ABDShared/Code`), manteniendo un fallback ordenado a `../ABDSharedCode` solo como compatibilidad durante la migración.
- Se eliminaron 44 rutas relativas hardcodeadas (`${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode/...`) sustituyéndolas por `${ABDSHARED_CODE_DIR}` en fuentes de prueba y utilidades.
- Todos los targets consumen los alias canónicos: `ABDShared::DspCore`, `ABDShared::SynthCore`, `ABDShared::AudioComparator`, `ABDShared::HardwareMidiDetect`, etc.

### B. Scripts de Entorno (`build.bat`)
- Se añadió soporte para crear enlaces NTFS (junctions) hacia `..\ABDShared\Assets` (`models`, `brands`) con fallback a `..\ABDSharedAssets`.

### C. Resolución de Rutas en C++ (`src/core/LabResourcePaths.h` y `.cpp`)
- **`sharedAssetsDir()`**:
  1. Busca `<repo>/../ABDShared/Assets`.
  2. Busca subiendo directorios desde el binario (`.../ABDShared/Assets`).
  3. Mantiene fallback de compatibilidad a `ABDSharedAssets`.
- **`sharedContractsDir()` (Nueva función)**:
  - Resuelve el cuadrante formal de contratos en `ABDShared/Contracts`.
  - Mantiene fallback a `ABDSharedAssets/contracts` y a `sharedAssetsDir().getChildFile("contracts")`.

### D. Componentes GUI (`SoundIdResultsSummaryView.h` y `.cpp`)
- Se corrigieron desincronizaciones de tipos (`std::string` vs `juce::String`) y se restauraron las rutinas de visualización y apertura de informes HTML.

---

## 4. Política de Contratos y Fixtures
- El inventario versionado local en `ABDAudioLab/contracts/hardware` permanece intacto con sus 40 ficheros exactos (34 perfiles de hardware y 6 esquemas JSON), validado por `test_TargetProfileLegacyInventory.cpp`.
- Los contratos compartidos y especificaciones de parámetros adicionales (`*_parameter_spec.json`) residen en `ABDShared/Contracts`.
