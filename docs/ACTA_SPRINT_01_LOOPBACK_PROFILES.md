# ACTA DE CERTIFICACIÓN FORMAL: SPRINT 1 — PERFILES DE INFRAESTRUCTURA (.abdloop) Y DIRECTORIO DE TRABAJO UNIFICADO

**Fecha:** 2026-10-13  
**Proyecto:** ABDAudioLab v2.x  
**Compilación:** Build Release x64 (MSVC 19.44 / JUCE 8.0.6)  
**Autoridad de Certificación:** Antigravity (Lead Architect & Planner) / ajabadia (Tándem Partner)  
**Estado:** CERTIFICADO Y VALIDADO (PASS — 178 assertions en Loopback + Suite de Directorios en verde)  

---

## 1. Resumen Ejecutivo y Alcance Certificado

Siguiendo el roadmap de modernización guiada de ABDAudioLab orientado a reducir la fricción operativa ("habla en músico, piensa en ingeniero") y compitiendo en estándares de usabilidad con Sonarworks SoundID Reference, se ha completado e implementado formalmente el **Sprint 1: Perfiles de Infraestructura Persistentes (.abdloop), Autodetección Asíncrona y Directorio de Trabajo Unificado**.

El objetivo cumplido:
1. Elimina la fatiga de recablear obligatoriamente la tarjeta de sonido en cada sesión: una vez que el usuario calibra su cadena de loopback para una interfaz y frecuencia de muestreo determinadas, el sistema almacena de forma inmutable y hermética los datos de calibración, cargándolos en segundo plano en futuras sesiones y otorgando un bypass transparente del Paso 2.
2. Unifica y centraliza todo el almacenamiento del laboratorio en un **Directorio de Trabajo General** configurable por el usuario, estructurado automáticamente en 5 subdirectorios funcionales y persistido entre sesiones.

---

## 2. Componentes e Implementación Técnica

### 2.1. Contrato de Perfil y Clave Determinista (`LoopbackProfile`)
* **Archivo:** `src/calibration/LoopbackProfile.h` y `src/calibration/LoopbackProfile.cpp`
* **Especificación:**
  * Estructura de datos inmutable para metrología de loopback: SNR (dB), THD (%), latencia de ida y vuelta (ms), vector de frecuencia (Hz) y respuesta en magnitud (dB).
  * Generación determinista de clave de hardware (`generateHardwareKey`): sanitización alfanumérica de `deviceName` ligada al `sampleRate` en Hz.
  * Hashing FIPS 180-4 SHA-256 (`computeSha256` / `verifyHash`) para detectar corrupción o manipulación manual del fichero JSON.
  * Serialización/Deserialización robusta JSON conforme a esquema canónico versionado (`schemaVersion: 1`).

### 2.2. Almacén Atómico en Dos Fases (`LoopbackProfileStore`)
* **Archivo:** `src/calibration/LoopbackProfileStore.h` y `src/calibration/LoopbackProfileStore.cpp`
* **Especificación:**
  * Ubicación canónica por defecto en el directorio de trabajo unificado: `<DirectorioDeTrabajo>/loopback_profiles/` (con fallback de emergencia en `%APPDATA%/ABDAudioLab/loopback_profiles/`).
  * Escritura atómica a disco en dos fases (`.tmp` $\to$ `.abdloop`) para prevenir archivos corruptos ante cortes de alimentación o cierres inesperados.
  * Búsqueda insensible a mayúsculas/minúsculas y tolerancia de frecuencia de muestreo ($\pm 1.0\text{ Hz}$) para absorber derivas de reloj de hardware.
  * Selección determinista por timestamp más reciente cuando existen múltiples candidatos.
  * Método `setStorageDirectory(const juce::File&)` para re-apuntar el almacén en caliente cuando el usuario modifica su directorio de trabajo.

### 2.3. Carga Asíncrona Desacoplada (`AsyncLoopbackProfileLoader`)
* **Archivo:** `src/calibration/AsyncLoopbackProfileLoader.h` y `src/calibration/AsyncLoopbackProfileLoader.cpp`
* **Especificación:**
  * Ejecución en segundo plano mediante `juce::ThreadPoolJob`, protegiendo de cualquier bloqueo tanto a la interfaz gráfica como al hilo de procesamiento de audio en tiempo real (`zero heap allocation` / cero I/O).
  * Notificación segura de finalización en el hilo de mensaje mediante `juce::MessageManager::callAsync`.

### 2.4. Integración y Usabilidad SoundID Reference
* **Archivos:**
  * `src/gui/soundid/SoundIdSidebarStepper.h` y `.cpp`
  * `src/gui/MainContentComponent.h` y `.cpp`
  * `src/gui/MainContentComponentHardware.cpp`
* **Especificación:**
  * Nuevo estado `CalibrationState::LoadedFromHistory`.
  * Insignia distintiva SoundID en color cian (`#06b6d4`) con icono de verificación blanco.
  * Subtítulo dinámico `"Cargado de historial"` bajo el Paso 2 de la barra lateral.
  * Detección automática al inicializar la aplicación o al cambiar de interfaz de audio en el Paso 1 (`HardwareRouting`).
  * Bypass inteligente: cuando existe un perfil cargado de historial, la confirmación de la interfaz de audio no fuerza la navegación al Paso 2, permitiendo comenzar la sesión inmediatamente.

### 2.5. Unificación del Directorio de Trabajo y Normalización I/O (`LabDataDirectories`)
* **Archivos:**
  * `src/core/LabDataDirectories.h` y `src/core/LabDataDirectories.cpp`
  * `src/gui/MainHeaderController.h` y `src/gui/MainHeaderController.cpp`
  * `src/gui/MainContentComponentHardware.cpp`
  * `src/gui/SessionReportManager.cpp`
  * `src/gui/controllers/SessionIoController.cpp`
  * `src/gui/measurement/MeasurementViewerPanel.cpp`
  * `src/gui/measurement/MeasurementContainerListPanel.cpp`
  * `src/gui/measurement/MeasurementComparisonPanel.cpp`
* **Especificación:**
  * **Estructura unificada de subdirectorios:**
    ```text
    <Directorio de Trabajo>/
    ├── experiments/          -> Sesiones (.abdlabtest) y capturas FAIR/LNL
    ├── exports/              -> Modelos DSP exportados, LUTs SIMD C++20
    ├── loopback_profiles/    -> Perfiles de infraestructura física .abdloop
    ├── presets/              -> Presets canónicos de medición y calibración
    └── reports/              -> Certificaciones e informes HTML / PDF
    ```
  * **Persistencia Atómica de Preferencias:** Implementación de `persistDataRootPreference` y `loadPersistedDataRootPreference` en `%APPDATA%/ABDAudioLab/lab_settings.json` mediante escritura en dos fases (`.tmp` $\to$ `.json`).
  * **Nuevo Origen Formal:** `DataRootOrigin::UserPersistedPreference` (Prioridad 3 en la cadena formal de resolución).
  * **Controles en el Menú File:**
    * `Set Working Directory...`: Abre `juce::FileChooser` para seleccionar la raíz y reconfigura los almacenes en caliente.
    * `Open Working Directory Folder`: Abre la carpeta raíz en el Explorador de Windows.
    * `Open Reports Folder`: Abre la carpeta de informes y certificados.
  * **Normalización de Puntos de Importación/Exportación:** Erradicación de accesos no canónicos a directorios genéricos de usuario (`userDocumentsDirectory`, `userHomeDirectory`) en todos los paneles de visualización, comparación e I/O de sesiones.

---

## 3. Matriz de Evidencias de Verificación (Catch2)

### 3.1. Suite de Perfiles de Loopback (`[abdloop],[store],[async]`)
```powershell
.\build\Release\ABDAudioLab_Tests.exe "[abdloop],[store],[async]"
```
**Resultado:**
```text
Filters: [abdloop],[store],[async]
Randomness seeded to: 186993582
===============================================================================
All tests passed (178 assertions in 14 test cases)
```

| Caso de Prueba | Cobertura / Invariante Verificada | Estado |
|---|---|:---:|
| `LoopbackProfile: JSON roundtrip serialization` | Serialización y deserialización sin pérdida de precisión en vectores ni metrología. | PASS |
| `LoopbackProfile: generateHardwareKey determinism` | Clave determinista sanitizada basada en nombre de interfaz y sample rate. | PASS |
| `LoopbackProfile: verifyHash detects corruption` | Detección estricta de manipulaciones de datos o hashes incorrectos. | PASS |
| `LoopbackProfileStore: save and load profile roundtrip` | Guardado en dos fases (`.tmp` $\to$ `.abdloop`) y lectura fidedigna. | PASS |
| `LoopbackProfileStore: findMatchingProfile exact match` | Localización de perfil exacto por nombre y sample rate. | PASS |
| `LoopbackProfileStore: findMatchingProfile tolerance` | Tolerancia a jitter de clock ($\pm 1.0\text{ Hz}$). | PASS |
| `LoopbackProfileStore: case insensitive matching` | Búsqueda agnóstica de mayúsculas/minúsculas en nombres de dispositivos. | PASS |
| `LoopbackProfileStore: select most recent profile` | Resolución al perfil más reciente cuando hay duplicados temporales. | PASS |
| `LoopbackProfileStore: reject corrupted hash file` | Descarte silencioso y seguro de archivos manipulados o dañados. | PASS |
| `AsyncLoopbackProfileLoader: loads profile on thread pool` | Carga asíncrona no bloqueante con callback exitoso. | PASS |
| `AsyncLoopbackProfileLoader: notifies when profile not found` | Notificación segura de ausencia de perfil sin fallos ni bloqueos. | PASS |
| `AsyncLoopbackProfileLoader: cancelJob stops execution` | Cancelación limpia de tareas pendientes en el pool. | PASS |
| `UI: LoadedFromHistory state visual contract` | Color cian SoundID (`#06b6d4`) y texto de historial en el stepper lateral. | PASS |
| `UI: Navigation bypass on LoadedFromHistory` | No redirección obligada al Paso 2 si la calibración fue recuperada. | PASS |

### 3.2. Suite de Directorios de Datos (`[directories]`)
```powershell
.\build\Release\ABDAudioLab_Tests.exe "[directories]"
```
* Valida la jerarquía de prioridad estricta: `ExplicitOverride` > `EnvironmentVariable` > `UserPersistedPreference` > `WorkspaceMarker` > `UserDocumentsFallback`.
* Valida la creación automática de los 5 subdirectorios funcionales (`experiments`, `exports`, `loopback_profiles`, `presets`, `reports`).
* Valida la persistencia atómica y recuperación de preferencias en `lab_settings.json`.

---

---

## 4. Estado de Archivos y Trazabilidad

| Archivo | Estado | Rol |
|---|---|---|
| `src/audio/LabAudioEngine.{h,cpp}` | Modificado | Autodetección de interfaces de audio y auto-enlace de dispositivos MIDI |
| `src/core/LabDataDirectories.{h,cpp}` | Modificado | Servicio centralizado de datos, 5 subcarpetas y persistencia en disco |
| `src/calibration/LoopbackProfile.{h,cpp}` | Creado | Modelo y hashing de calibración de loopback |
| `src/calibration/LoopbackProfileStore.{h,cpp}` | Creado / Modificado | Almacén persistente seguro vinculado a `loopback_profiles/`, búsqueda por dispositivo |
| `src/calibration/AsyncLoopbackProfileLoader.{h,cpp}` | Creado | Worker no bloqueante para detección en arranque |
| `src/gui/NativeCalibrationPanel.{h,cpp}` | Modificado | Inyección de `applyLoadedLoopbackProfile`, hooks preflight e historial |
| `src/gui/NativeCalibrationPanelWorkflow.cpp` | Modificado | Preflight check antes de medir ruido/loopback y soporte de perfiles cargados |
| `src/gui/NativeCalibrationPanelUiState.cpp` | Modificado | Limpieza y reset de estado ante re-medición |
| `src/gui/calibration/CalibrationPanelViewState.h` | Modificado | Soporte de `loadedProfile` para renderizado de metadatos |
| `src/gui/calibration/CalibrationPanelPainterReports.cpp` | Modificado | Tarjeta visual SoundID Reference con fecha, interface, resolución y métricas |
| `src/gui/soundid/SoundIdSidebarStepper.{h,cpp}` | Modificado | Renderizado visual del estado `LoadedFromHistory` |
| `src/gui/MainHeaderController.{h,cpp}` | Modificado | Menú File con selector de Directorio de Trabajo y accesos directos |
| `src/gui/MainContentComponent.{h,cpp}` | Modificado | Preflight modal, guardado interactivo/inteligente y menú popup de historial |
| `src/gui/MainContentComponentHardware.cpp` | Modificado | Métodos de interfaz para selección y navegación a directorios |
| `src/gui/SessionReportManager.cpp` | Modificado | Normalización de fallbacks de guardado y carga a `LabDataDirectories` |
| `src/gui/controllers/SessionIoController.cpp` | Modificado | Inicialización canónica de exportaciones a `labDirs.exports` |
| `src/gui/measurement/MeasurementViewerPanel.cpp` | Modificado | Selector de contenedor FAIR/LNL vinculado a `labDirs.experiments` |
| `src/gui/measurement/MeasurementContainerListPanel.cpp` | Modificado | Selector de contenedor múltiple vinculado a `labDirs.experiments` |
| `src/gui/measurement/MeasurementComparisonPanel.cpp` | Modificado | Exportación de informes HTML vinculada a `labDirs.reports` |
| `src/tests/test_LoopbackProfileStore.cpp` | Creado / Modificado | Tests de serialización, tolerancia a fallos, matching y `findProfilesForDevice` |
| `src/tests/test_LabDataDirectories.cpp` | Modificado | Tests de los 5 subdirectorios y persistencia de settings |
| `CMakeLists.txt` | Modificado | Corrección de enlaces y compilación de nuevos módulos |
| `PRD.md` | Creado | Product Requirements Document v2.x |
| `TRD.md` | Creado | Technical Requirements Document v2.x |
| `docs/ROADMAP.md` | Modificado | Actualización del plan maestro con el Sprint 1 completado |

---

## 5. Dictamen

El Sprint 1, la infraestructura de almacenamiento unificado y el flujo de calibración inteligente quedan formalmente **CERRADOS Y CERTIFICADOS**:
1. **Autodetección al entrar**: Detección automática de interfaces de audio y auto-enlace de dispositivos MIDI.
2. **Preflight Guard**: Alerta modal interactiva antes de medir si el directorio de trabajo no ha sido configurado expresamente.
3. **Guardado Inteligente**: Auto-guardado en el directorio de trabajo si es la primera medición, o diálogo modal de confirmación si ya existía una medición para la interfaz/sample rate.
4. **Histórico y Estética SoundID Reference**: Carga por defecto del más reciente, selector popup interactivo para cargar mediciones anteriores y tarjeta visual detallada (fecha, interface, resolución, búfer, RTL, SNR, THD, trim).
