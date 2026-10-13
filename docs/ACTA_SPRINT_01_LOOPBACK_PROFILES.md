# ACTA DE CERTIFICACIÓN FORMAL: SPRINT 1 — PERFILES DE INFRAESTRUCTURA (.abdloop)

**Fecha:** 2026-10-13  
**Proyecto:** ABDAudioLab v2.x  
**Compilación:** Build Release x64 (MSVC 19.44 / JUCE 8.0.6)  
**Autoridad de Certificación:** Antigravity (Lead Architect & Planner) / ajabadia (Tándem Partner)  
**Estado:** CERTIFICADO Y VALIDADO (PASS — 178 assertions, 14 test cases)  

---

## 1. Resumen Ejecutivo y Alcance Certificado

Siguiendo el roadmap de modernización guiada de ABDAudioLab orientado a reducir la fricción operativa ("habla en músico, piensa en ingeniero") y compitiendo en estándares de usabilidad con Sonarworks SoundID Reference, se ha completado e implementado formalmente el **Sprint 1: Perfiles de Infraestructura Persistentes (.abdloop) y Autodetección Asíncrona**.

El objetivo cumplido elimina la fatiga de recablear obligatoriamente la tarjeta de sonido en cada sesión: una vez que el usuario calibra su cadena de loopback para una interfaz y frecuencia de muestreo determinadas, el sistema almacena de forma inmutable y hermética los datos de calibración, cargándolos en segundo plano en futuras sesiones y otorgando un bypass transparente del Paso 2.

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
  * Ubicación canónica desacoplada: `%APPDATA%/ABDAudioLab/loopback_profiles/`.
  * Escritura atómica a disco en dos fases (`.tmp` $\to$ `.abdloop`) para prevenir archivos corruptos ante cortes de alimentación o cierres inesperados.
  * Búsqueda insensible a mayúsculas/minúsculas y tolerancia de frecuencia de muestreo ($\pm 1.0\text{ Hz}$) para absorber derivas de reloj de hardware.
  * Selección determinista por timestamp más reciente cuando existen múltiples candidatos.

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

---

## 3. Matriz de Evidencias de Verificación (Catch2)

Ejecución de suite en el binario de Release:
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

### Casos de Prueba Verificados
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

---

## 4. Estado de Archivos y Trazabilidad

| Archivo | Estado | Rol |
|---|---|---|
| `src/calibration/LoopbackProfile.{h,cpp}` | Creado | Modelo y hashing de calibración de loopback |
| `src/calibration/LoopbackProfileStore.{h,cpp}` | Creado | Almacén persistente seguro y motor de matching |
| `src/calibration/AsyncLoopbackProfileLoader.{h,cpp}` | Creado | Worker no bloqueante para detección en arranque |
| `src/gui/soundid/SoundIdSidebarStepper.{h,cpp}` | Modificado | Renderizado visual del estado `LoadedFromHistory` |
| `src/gui/MainContentComponent.{h,cpp}` | Modificado | Hooking de autodetección y autoguardado tras calibrar |
| `src/gui/MainContentComponentHardware.cpp` | Modificado | Lógica de bypass de navegación |
| `src/tests/test_LoopbackProfileStore.cpp` | Creado | Suite Catch2 con 14 test cases y 178 aserciones |
| `CMakeLists.txt` | Modificado | Corrección de enlaces y compilación de nuevos módulos |
| `PRD.md` | Creado | Product Requirements Document v2.x |
| `TRD.md` | Creado | Technical Requirements Document v2.x |
| `docs/ROADMAP.md` | Modificado | Actualización del plan maestro con el Sprint 1 completado |

---

## 5. Dictamen

El Sprint 1 queda formalmente **CERRADO Y CERTIFICADO**. La base de código se encuentra limpia, sin regresiones en la suite general y lista para iniciar el Sprint 2 (Presets de Medición y Edición Quirúrgica de Outliers).
