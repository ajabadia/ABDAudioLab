# Auditoría y Registro de Evolución: Wizard de Inicio y Calibración de Interfaz (R4)

**Fecha de Actualización:** 4 de Octubre de 2026  
**Rama:** `main`  
**Documento de Gobernanza:** `docs/audits/AUDIT_CALIBRATION_LAB_AND_STARTUP_WIZARD.md`  
**Estado:**
- 🟢 **Prioridad 1 cerrada** (`855991f`) — Sincronización del arranque del Wizard en Tarea 1 (Target & Routing)
- 🟢 **Prioridad 2 cerrada** (`ab30e62`) — Claridad operativa y UX copy de la calibración de interfaz
- 🟢 **P3A cerrada** (`4a12fe6`) — Rechazo estricto de clipping en calibración loopback
- 🟢 **P3B cerrada** (`c38f39b`) — Persistencia hermética de perfiles de calibración en AppData
- 🟢 **P3C cerrada** (`6598b42`) — Coincidencia observable y reutilización explícita de perfiles

---

## 1. Registro Canónico de Baselines de Pruebas

Para garantizar la estricta trazabilidad de no-regresión y justificar la variación en el cómputo de aserciones de la suite Catch2, se certifican los siguientes puntos de control:

| Baseline / Hito | Commit SHA | Tests Totales | PASS | SKIPPED (Legítimos) | FAIL | Assertions PASS | Delta Aserciones |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Release Histórica Inmutable v2.1.0** | `0b76616` | 945 | 918 | 27 | 0 | **211.036** | Baseline de partida |
| **Post-Prioridad 1 (Startup Sync)** | `855991f` | 946 | 919 | 27 | 0 | **211.042** | +6 (+1 test case ST-69) |
| **Post-Prioridad 2 (Claridad UX/Copy)** | `ab30e62` | 946 | 919 | 27 | 0 | **211.044** | +2 (Aserciones de copy del Stepper) |
| **Post-Prioridad 3A (Criterio Seguro Clipping)** | `4a12fe6` | 946 | 919 | 27 | 0 | **211.044** | 0 (Reemplazo de aserción en test Farina existente) |
| **Post-Prioridad 3B (Perfil Persistente)** | `c38f39b` | 955 | 928 | 27 | 0 | **211.160** | +116 (+9 test cases herméticos) |
| **Post-Prioridad 3C (Coincidencia Observable y Reutilización)** | `6598b42` | 973 | 946 | 27 | 0 | **211.213** | +53 (+18 test cases herméticos) |

> [!NOTE]
> Las 27 pruebas en estado `SKIPPED` corresponden exclusivamente a la ausencia de plugins VST3 externos de prueba (Dexed / VES) en el entorno de desarrollo local, de acuerdo con la clasificación normativa `KI-01`.

---

## 2. Estado de Cierre de Microhitos

### Prioridad 1: Sincronización de Arranque del Wizard (`855991f`)
- **Problema resuelto:** Incoherencia visual al abrir la aplicación (la barra lateral indicaba Tarea 0 pero el panel central presentaba Tarea 1).
- **Solución implementada:** Se fijó `Step::HardwareRouting` (Tarea 1 — Target & Routing) como único estado inicial sincronizado tanto en `SoundIdSidebarStepper` como en `WorkflowNavigationController`, garantizando la invocación canónica de `resetToNewSession()`.
- **Integridad:** Se preservaron intactos los valores ordinales del enum class `Step`. Se añadió el test de contrato `ST-69` en `test_StepperCoherence_ST47_ST68.cpp`.

### Prioridad 2: Claridad Operativa de Calibración de Interfaz (`ab30e62`)
- **Problema resuelto:** Ambigüedad en la Tarea 2 respecto a qué se calibra (el usuario creía calibrar su sintetizador en lugar de la tarjeta), instrucción incorrecta de ajuste manual a -3 dBFS, detalles técnicos irrelevantes en modo digital y códigos de error crípticos.
- **Solución implementada:**
  1. Título canónico: `2. Calibración de Interfaz de Audio` (subtítulo: `Latencia y Nivel de Tarjeta`).
  2. Aclaración explícita: La prueba mide la ruta de audio de la interfaz (DAC/ADC), no el instrumento bajo prueba.
  3. Desacoplamiento de ganancia: Se eliminó el requerimiento manual de -3 dBFS; la UI explica que el cómputo de compensación es automático (Auto-Trim).
  4. Mensajes accionables ante fallos: Instrucción específica de comprobar cables y retornos o reducir ganancia ante saturación, bajo el badge `✕ REVISAR RETORNO`.
  5. Modo digital depurado: Se eliminó la promesa de *“32/64-bit float”*, explicando con claridad que la calibración analógica no aplica a plugins/sintetizadores virtuales y se asumen 0 ms y nivel nominal.
  6. Presentación explícita de canales: Se exhibe visualmente la ruta activa `Output 1 ➔ Input 1`.

#### Salvedad Técnica Registrada (Gobernanza)
En `NativeCalibrationPanel`, los métodos `setCalibrationChannels()`, `getCalibrationOutputChannel()` y `getCalibrationInputChannel()` constituyen únicamente preparación técnica privada e inactiva.  
**Norma para Prioridad 3:** La Prioridad 3 **no debe asumir** que existe routing configurable de usuario por la mera presencia de estos métodos. Su uso real requerirá auditoría y diseño específico si se decide exponerlo como funcionalidad de usuario.

---

## 3. Plan de Trabajo Segmentado: Prioridad 3 (Robustez Técnica y Persistencia)

La Prioridad 3 aborda cambios de comportamiento matemático, criterios de aceptación estricta y persistencia en disco. Para mitigar riesgos y facilitar la validación granular, el trabajo se divide en **tres microhitos secuenciales independientes**:

```mermaid
graph TD
    P3A["<b>P3A: Criterio Seguro de Resultado</b><br>Clipping detectado ➔ isCalibrated = false<br>(Rechazo estricto de señal saturada)"] --> P3B["<b>P3B: Perfil Persistente de Calibración</b><br>Guardar y recuperar calibración en AppData<br>(Desacoplado del ciclo de vida del Wizard)"]
    P3B --> P3C["<b>P3C: Compatibilidad e Invalidación</b><br>Identidad de interfaz, driver, SR, buffer<br>(Reutilización inteligente o recalibración)"]
```

### Secuencia de Ejecución

1. **P3A — Criterio Seguro de Resultado (Fallo Estricto por Clipping)**:
   - **Objetivo:** Garantizar que ninguna calibración con clipping sea aceptada como válida (`isCalibrated = false`).
   - **Alcance:** Modificación en la regla de decisión de `LoopbackCalibrator::analyzeLoopback()` y ajuste de los tests unitarios correspondientes.
   - **Estado:** ✅ **Completado y Certificado (`4a12fe6`)**.
     - Implementación: `result.isCalibrated = (!result.clippingDetected && result.peakInDbfs > -40.0f && result.frequencyFlatnessDb < 6.0f);` en `src/math/LoopbackCalibrator.cpp` (línea 155).
     - Test unitario: Caso dual en `src/tests/test_LoopbackDiagnostics.cpp` (barrido Farina sin saturar ➔ `isCalibrated = true`; con saturación inyectada ➔ `isCalibrated = false`).
     - Verificación: `ABDAudioLab_Tests.exe "[diagnostics]"` (17 assertions en 3 test cases PASS) y `ABDAudioLab_Tests.exe "[hygiene]"` (152 assertions en 22 test cases PASS).

2. **P3B — Perfil Persistente de Calibración**:
   - **Objetivo:** Almacenar de forma desacoplada la calibración exitosa en un archivo JSON en `AppData` (directorio del usuario) para evitar obligar a recalibrar en cada sesión.
   - **Alcance:**
     - Modelo de datos estructurado en `src/calibration/CalibrationRecord.h`.
     - Almacén de perfiles `src/calibration/CalibrationProfileStore.h` y `src/calibration/CalibrationProfileStore.cpp` con escritura atómica (`.json.tmp` ➔ `.json`), validación estricta de `profileId`, orden descendente por fecha (`createdAt`) y resiliencia ante JSON corrupto o esquema desconocido.
     - Preservación de calibración válida en RAM durante la navegación en `src/gui/MainContentComponent.cpp` y reinicio solo al iniciar nueva sesión explícita en `src/gui/MainContentComponentPersistence.cpp`.
     - UI no bloqueante en `src/gui/NativeCalibrationPanel.h` y `src/gui/NativeCalibrationPanel.cpp`: botón `[Guardar Calibración]`, sección compacta de perfiles guardados con `[Ver Detalles]` y `[Eliminar]`. Estrictamente sin botones de "Usar perfil" ni autoaplicación (reservado para P3C).
   - **Estado:** 🟢 **Cerrada y certificada** (`feat(calibration): persist valid loopback calibration profiles`).
   - **Validación técnica observada:**
     - Suite hermética P3B: `ABDAudioLab_Tests.exe "[calibration][store][hermetic]"` (9 test cases, 108 assertions, 100% PASS).
     - Suite de higiene: `ABDAudioLab_Tests.exe "[hygiene]"` (22 test cases, 152 assertions, 100% PASS).
     - Suite canónica no-VES: `ABDAudioLab_Tests.exe "~[ves]"` (955 test cases | 928 passed | 27 skipped | 211,160 assertions, 0 failed).

3. **P3C — Coincidencia de Configuración Observable y Reutilización Explícita**:
   - **Objetivo:** Determinar cuándo un perfil guardado coincide con la configuración observable de audio del sistema y permitir su reutilización explícita por el operador, garantizando que nunca se aplique de forma silenciosa ni se reutilice si cambian los parámetros medibles.
   - **Alcance:**
     - Modelo de snapshot observable: `CurrentAudioConfigurationSnapshot` en `src/calibration/CalibrationMatchEvaluator.h`.
     - Comparador puro y desacoplado: `CalibrationMatchEvaluator` en `src/calibration/CalibrationMatchEvaluator.cpp`.
     - 5 estados precisos: `ConfigurationMatch`, `ConfigurationMismatch`, `DeviceOrDriverMismatch`, `NoActiveDevice` e `InvalidOrCorruptProfile`.
     - Selección multianálisis: `findBestMatchingProfile` analiza todos los perfiles y propone el coincidente más reciente (un perfil nuevo no coincidente no bloquea uno anterior coincidente).
     - Neutralización de trim activo: si una calibración activa en RAM pasa a `Misaligned`, o el usuario elige `[Continuar sin calibrar (Bypass)]`, o se inicia una nueva sesión (`performNewSessionReset`), el `inputAutoTrim` se neutraliza de inmediato a 1.0f (0.0 dB).
     - Integración en UI: `NativeCalibrationPanel` añade botón `[Reutilizar calibración guardada]`, renombra `[Continuar sin calibrar (Bypass)]` y muestra advertencia explícita sobre los límites de detección analógica/física.
   - **Estado:** 🟢 **Cerrada y certificada** (`feat(calibration): evaluate configuration matching and explicit profile reuse`).
   - **Validación técnica observada:**
     - Suite hermética P3C: `ABDAudioLab_Tests.exe "[calibration][compatibility][hermetic]"` (18 test cases, 53 assertions, 100% PASS).
     - Suite de higiene: `ABDAudioLab_Tests.exe "[hygiene]"` (22 test cases, 152 assertions, 100% PASS).
     - Suite canónica no-VES: `ABDAudioLab_Tests.exe "~[ves]"` (973 test cases | 946 passed | 27 skipped | 211,213 assertions, 0 failed).
