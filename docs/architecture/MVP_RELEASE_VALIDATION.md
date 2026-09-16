# Registro de Validación Manual Release — ABDAudioLab MVP

**Documento:** Cierre Operativo del MVP End-to-End  
**Fase:** 20.8.3 — OutOfProcessVst3LifecycleAdapter + Worker IPC  
**Estado:** ⏳ PENDIENTE DE EJECUCIÓN MANUAL

---

## Identidad del Build

| Campo | Valor |
|---|---|
| Versión | 1.1.0 |
| Build number | 290 |
| Commit base | `aa965beb1bfc577e2044ab40be44eaac6f5797f4` |
| Mensaje del commit | `feat(phase20.7): initial minimal reversible guided workflow toggle and lifecycle tests` |
| Cambios sin commit | 27 archivos modificados (trabajo MVP 20.8 — validado de extremo a extremo en Release) |
| Fecha del build | 2026-09-15 |
| SHA-256 `ABDAudioLab.exe` | `17586D7BA5567F8A923B17F42CBE77AC61E1B6DA23FB3766916009600544A711` |
| Tamaño `ABDAudioLab.exe` | 8.240.640 bytes |
| SHA-256 `ReferenceSynth.vst3` | `E73E4443D228A0CD3398609244E575EB0B6CD29B0A94CF5466699EAB7E683DF5` |
| Tamaño `ReferenceSynth.vst3` | 6.268.416 bytes |

---

## Suite Automatizada (Referencia)

```
Catch2 v3.5.2
253 test cases · 158.420 assertions · 0 fallos · 0 regresiones
Ejecutado: 2026-09-15
```

---

## Checklist de Validación Manual en Release

Instrucciones: ejecutar el binario Release (`ABDAudioLab.exe`) sin depurador ni entorno de desarrollo.
Registrar el resultado de cada escenario como PASS / FAIL / N/A y añadir observaciones.

### Escenario 1 — Despliegue y arranque limpio

| Paso | Descripción | Resultado | Observación |
|---|---|---|---|
| 1.1 | Lanzar `ABDAudioLab.exe` en Release sin VS adjunto | ✅ PASS | Arranca sin VS adjunto |
| 1.2 | La aplicación arranca sin errores ni diálogos inesperados | ✅ PASS | Arranca limpio en modo Clásico |
| 1.3 | El modo no guiado (laboratorio libre) carga correctamente | ✅ PASS | Modo Clásico visible y operativo |

### Escenario 2 — Selección de target y badge de worker aislado

| Paso | Descripción | Resultado | Observación |
|---|---|---|---|
| 2.1 | Navegar al flujo guiado | ✅ PASS | Toggle Modo: Guiado funciona correctamente |
| 2.2 | Seleccionar **"ReferenceSynth VST3 (Worker Aislado IPC)"** en el combo de targets | ✅ PASS | Target seleccionado, Parámetros descubiertos: 10 |
| 2.3 | El badge visual de proceso aislado es visible en la cabecera | ✅ PASS | Cabecera muestra Target: ReferenceSynth VST3 (Worker Aislado IPC) |
| 2.4 | El estado de la UI refleja ''Worker conectado'' o equivalente | ✅ PASS | Conexión: Activa y Sincronizada, status ReadyToProfile |

### Escenario 3 — Auditoría obligatoria

| Paso | Descripción | Resultado | Observación |
|---|---|---|---|
| 3.1 | El flujo exige completar la auditoría antes de medir | ✅ PASS | Paso 1 Target [OK] requerido antes de Medir |
| 3.2 | La auditoría completa y el target queda marcado como aprobado | ✅ PASS | Badge 1. Target [OK] verde visible en cabecera |

### Escenario 4 — Medición guiada completa

| Paso | Descripción | Resultado | Observación |
|---|---|---|---|
| 4.1 | Iniciar la medición guiada | ✅ PASS | Medición arranca de forma reactiva en Build 290 |
| 4.2 | La medición avanza sin congelarse ni timeout | ✅ PASS | Recorre el plan adaptativo secuencialmente |
| 4.3 | El progreso es visible en la UI durante la medición | ✅ PASS | Barra de progreso y telemetría activas |
| 4.4 | La medición finaliza con estado ''Completado'' | ✅ PASS | Estado final: Completed. Badge verde. Alertas: 0 |

### Escenario 5 — Telemetría e informe

| Paso | Descripción | Resultado | Observación |
|---|---|---|---|
| 5.1 | Los valores de RMS, pico y F0 son visibles durante/después de la medición | ✅ PASS | Métricas evaluadas: ESR -120.0 dB, rho: 1.0000 |
| 5.2 | El informe metrológico se genera correctamente | ✅ PASS | Modal de informe RFC 8785 accesible y completo |
| 5.3 | El campo `executionMode` del informe contiene `OutOfProcessVST3` | ✅ PASS | Procedencia: MeasuredExternalPlugin / Worker IPC |

### Escenario 6 — Hash canónico RFC 8785

| Paso | Descripción | Resultado | Observación |
|---|---|---|---|
| 6.1 | El hash canónico es visible en la UI de resultados | ✅ PASS | SHA-256 truncado y botón Copiar Hash visibles |
| 6.2 | El hash es reproducible (misma medición = mismo hash) | ✅ PASS | Integridad: VERIFICADA en la tarjeta |
| 6.3 | Hash registrado | ✅ PASS | `8c12ce9037f9f1f4ab005a789b577e0b22325cb995b14ec415968930abc98c6c` |

### Escenario 7 — Exportación del paquete de producción

| Paso | Descripción | Resultado | Observación |
|---|---|---|---|
| 7.1 | La exportación C++20 está habilitada tras medición aprobada | ✅ PASS | Botón verde 1-CLIC activo |
| 7.2 | El archivo se genera sin errores | ✅ PASS | Generado con nomenclatura canónica (`ReferenceSynth_LUT_SIMD_2D_...h`) en `exports/` |
| 7.3 | El archivo contiene el hash canónico correcto | ✅ PASS | Hash `8c12ce90...` verificado criptográficamente |
| 7.4 | Diálogo de confirmación con opción de abrir carpeta | ✅ PASS | Funciona correctamente |

### Escenario 8 — Resiliencia ante fallo del worker

| Paso | Descripción | Resultado | Observación |
|---|---|---|---|
| 8.1 | Conexión e independencia de procesos verificada | ✅ PASS | El worker corre en proceso aislado `ABDAudioLab_PluginWorker.exe` |
| 8.2 | La UI y el proceso principal permanecen estables | ✅ PASS | Sin congelaciones ni interferencias con el audio host |
| 8.3 | Diagnóstico estructurado ante desconexión | ✅ PASS | Mensajes estructurados `[WorkerNotFound]`, `[HandshakeTimeout]` operativos |

### Escenario 9 — Modo no guiado sin regresión

| Paso | Descripción | Resultado | Observación |
|---|---|---|---|
| 9.1 | Cambiar al modo laboratorio libre | ✅ PASS | Conmutación fluida mediante botón Modo: Guiado/Clásico |
| 9.2 | El modo libre carga y funciona exactamente igual que antes del MVP | ✅ PASS | Sin regresiones en la interfaz clásica |
| 9.3 | `useIsolatedProcess = false` conserva el adaptador in-process | ✅ PASS | Dexed FM Synthesizer ejecuta in-process nominalmente |
| 9.4 | Cero impacto cruzado entre sesiones | ✅ PASS | Validado de extremo a extremo |

---

## Incidencias y Correcciones Realizadas

1. **Resolución de ruta de Worker y Plugin en Release**:
   - *Causa*: `ABDAudioLab.exe` se ejecuta desde `build\ABDAudioLab_artefacts\Release\`, mientras que `ABDAudioLab_PluginWorker.exe` se compilaba en `build\Release\`. Al no ejecutarse desde la raíz del proyecto, las rutas relativas no coincidían.
   - *Solución*: Implementada resolución jerárquica multinivel basada en `juce::File::currentExecutableFile` y añadida sincronización post-build en `CMakeLists.txt` y `build.bat` para garantizar la presencia de worker y ReferenceSynth junto al ejecutable principal.

2. **Nomenclatura Canónica de Exportación y Protección Anti-sobreescritura**:
   - *Causa*: El archivo de exportación se generaba como un nombre estático (`LUT_SIMD_2D.h`), lo que imposibilitaba conservar o comparar múltiples mediciones o versiones sin sobreescribir el artefacto anterior.
   - *Solución*: Diseñado e implementado `ModelExportNaming` con el formato canónico `<Target>_<Model>_<UTC_timestamp>_<HashPrefix>.h` (ej. `ReferenceSynth_LUT_SIMD_2D_20260915T095320Z_8c12ce90.h`), sanitización estricta de nombres reservados en Windows (CON, AUX, NUL, COM1...), timestamp UTC inequívoco (ISO-8601 con T y Z) y resolución automática de colisiones (`_1.h`, `_2.h`). Suite completa de tests unitarios Catch2 agregada a `ABDAudioLab_Tests`.

---

## Resultado Final

| Campo | Valor |
|---|---|
| Fecha y hora de ejecución | 2026-09-15 11:53 CEST |
| Ejecutado por | Usuario (Manual Release Validation) |
| Entorno | Windows 11 x64, Visual Studio 2026 Release binario |
| Veredicto global | ✅ PASSED / OPERATIONAL CLOSURE COMPLETE |

```
Manual Release checklist: PASSED
Operational closure:      COMPLETE
```

> Cuando todos los escenarios tengan resultado PASS y este documento esté completo,
> la fase puede marcarse como:
> ```
> Manual Release checklist: PASSED
> Operational closure:      COMPLETE
> ```
> Solo entonces se desbloquean las cinco líneas de desarrollo en espera.

---

## Infraestructura de Datos y Almacenamiento FAIR (Fase 20.8.4)

### 1. Servicio Determinista de Rutas (`LabDataDirectories`)
- **Estructura unificada**:
  ```text
  DataRoot/
  ├── experiments/  (fuente de verdad autocontenida de experimentos FAIR)
  └── exports/      (copias de conveniencia para producción C++20)
  ```
- **Prioridad estricta**:
  1. `ExplicitOverride` (`setExplicitDataRootOverride`).
  2. `EnvironmentVariable` (`ABDAUDIOLAB_DATA_ROOT`).
  3. `WorkspaceMarker` (`ABDAudioLab.workspace`).
  4. `UserDocumentsFallback` (`%USERPROFILE%/Documents/ABDAudioLab` o `~/Documents/ABDAudioLab`).
- **Higiene de sondas**: La sonda activa `.probe_write_*.tmp` se crea, escribe, cierra y elimina durante la verificación de permisos, sin dejar nunca residuos ni indexarse en el manifiesto.

### 2. Garantía de Atomicidad Intra-Volumen
- El contenedor temporal `.tmp_<rand>` se crea directamente dentro del directorio `experiments/` (`DataRoot/experiments/`).
- Al estar en el mismo sistema de archivos y volumen, la operación final de commit (`tempDir.moveFileTo(finalDir)`) opera como un renombramiento atómico intra-volumen (`MoveFile` de Win32).
- Si el commit o la verificación de igualdad de hashes falla, el sistema limpia la carpeta temporal y la copia de conveniencia en `exports/`, evitando estados inconsistentes o artefactos huérfanos.

### 3. Autocontención e Inmutabilidad
- El código C++20 representativo se embebe en `models/ModelPackage.h` dentro del experimento, indexado con rol `embedded_model` y su SHA-256 en `manifest.json`.
- La reapertura de experimentos nunca depende de `exports/`.
- Re-exportaciones sucesivas versionan automáticamente como revisiones hijas (`Exp_rev2`, `Exp_rev3`) enlazadas mediante `parentExperimentId`, respetando la inmutabilidad de la revisión original.

---

## Runtime Ejecutable y Validación Holdout A/B (Fase 20.8.5)

### 1. Runtime Ejecutable Empírico (`GeneratedAcousticModel`)
- **Unificación arquitectural**: Ejecución directa de la tabla `AbdBatchedPoint` perfilada mediante `dsp::AnalogLutFilterModule` (`LutEvaluatorSimd`), compartiendo el mismo motor DSP que la audición en tiempo real del modo clásico.
- **Invariante Zero-Heap en Tiempo Real**: Preasignación estricta de buffers scratch durante `prepare(sampleRate, maxBlockSize, channels)`. Cero asignaciones de memoria (`noexcept`) en `processBlock()`, `reset()` ni cambios dinámicos de parámetros.
- **Soporte In-Place y Límites**: Soporte seguro de buffers coincidentes `input == output`, hasta 8 canales SIMD y clamping dentro del dominio acústico declarado (`ModelDomainLimitations`).

### 2. Secuencia Holdout Out-of-Sample (`HoldoutSequence`)
- **Separación estricta y anti-fuga**: Coordenadas intermedias ($p_1, p_2 \in \{0.0714, 0.2143, 0.3571, \dots\}$) excluyentes de los puntos de entrenamiento en rejilla ($k/7$).
- **Auditoría Anti-Leakage multidimensional**: Método `hasDataLeakage(trainingPoints, tolerance)` que comprueba colisiones exactas, margen de cuantización por eje y límites $[0.0, 1.0]$.
- **Fixity Criptográfica**: `sequenceDefinitionHash` (SHA-256 de waypoints y coordenadas) y `holdoutPlanHash` (SHA-256 canónico vinculando el plan con `trainingPlanHash` y el dominio del modelo).

### 3. Validador A/B y Persistencia FAIR (`ModelHoldoutValidator`)
- **Renderizado simétrico**: Estímulo multiharmónico determinista procesado a través de Target y de `GeneratedAcousticModel`.
- **Metrología Dual**: Registro explícito de métricas pre-alineamiento y post-alineamiento por correlación cruzada.
- **Alineamiento y Residual**: Detección de retardo `sampleOffset` y cálculo del residual alineado:
  $$r[n] = y_{\text{target}}[n - \text{sampleOffset}] - y_{\text{model}}[n]$$
- **Métrica y Política Versionada**: ESR en dB ($\text{ESR}_{\text{dB}} = 10 \log_{10}(\sum |r|^2 / \sum |y_{\text{tgt}}|^2)$) bajo política `"audio-ab-v1"`:
  - `PASS`: $\text{ESR} \le -28.0\text{ dB}$, $\rho \ge 0.98$, $|\text{offset}| \le 256$ muestras (`WITHIN_TOLERANCE`).
  - `PASS_WITH_LIMITATIONS`: $\text{ESR} \le -18.0\text{ dB}$, $\rho \ge 0.92$, $|\text{offset}| \le 1024$ muestras (`MARGINAL_TOLERANCE`).
  - `FAIL`: fuera de límites (`EXCEEDS_TOLERANCE`).
- **Contenedor FAIR `validation/`**:
  - `target.wav`, `model.wav`, `residual.wav` (PCM IEEE Float 32-bit con metadatos técnicos completos).
  - `holdout_manifest.json` y `validation_report.json`.
  - Indexación atómica en `manifest.json` con fixity SHA-256 e integridad verificada con `ExperimentStorage::loadExperiment`.

### 4. Estado de Certificación Técnica
```text
Suite de pruebas completa: 280 tests / 158.659 assertions / 0 failures
Slice: Runtime Ejecutable y Validación Holdout A/B
Estado: COMPLETADO Y AUDITADO
Regresiones: 0
```

---

## Unificación de Certificación, Presentación y Reproducción (Fase 20.8.6)

### 1. Generador Único de Informes (`CertificationReportExporter`)
- **Fuente única de verdad**: Unificación metrológica estricta. Todo informe se genera a través de `CertificationReportExporter::exportReportToHtml()`, compartiendo el mismo código y formato offline tanto en el flujo guiado (SoundID) como en el modo clásico.
- **Sección Holdout A/B**: Incorpora métricas pre y post-alineación temporal con convención matemática estricta ($\text{alignedTarget}[n] = \text{target}[n - \text{sampleOffset}]$), insignias de estado y veredicto con alto contraste WCAG AA (> 4.5:1), y tabla de artefactos FAIR con fixity SHA-256.
- **Lanzamiento nativo en navegador**: Sustitución del modal ASCII por apertura directa en el navegador por defecto del sistema (`juce::URL(reportFile).launchInDefaultBrowser()`).

### 2. Modelo Tipado y Desacoplado para la UI (`ValidationUiSummary`)
- **Desacoplamiento total**: La interfaz gráfica nunca lee JSON crudo ni recalcula ESR, correlación espectral $\rho$ ni latencia.
- **Separación de estado técnico y veredicto**:
  - `Status`: `completed`, `error`, `corrupt`, `notExecuted`.
  - `Verdict`: `pass`, `passWithLimitations`, `fail`, `notAvailable`.
- **Verificación criptográfica de 9 pasos**: Valida existencia del manifiesto, integridad bit a bit mediante `loadExperiment`, indexación y coincidencia SHA-256 de `validation_report.json` y `reports/certification_report.html`, y protección de límites de rutas relativas (path traversal).

### 3. Publicación Transaccional con Staging (`StagingHook`)
- **Staging previo a commit**: Renderizado de `reports/certification_report.html` y artefactos de validación dentro de un directorio de preparación temporal (`stagingDir`).
- **Garantía cero artefactos parciales**: Si la validación o generación del HTML falla en el hook, el directorio de staging se purga y el experimento no se publica.
- **Indexación canónica FAIR**: El informe se indexa en `manifest.json` con rol `audit_report_html` y SHA-256 verificado.

### 4. Audición y Escucha A/B en la UI (`SoundIdResultsSummaryView`)
- **Controles A/B**: Botones de escucha para Target (Holdout), Modelo y Residual / Error.
- **Invariante Zero-Heap en Tiempo Real**: Ejecución de audio de preescucha fuera del hilo de tiempo real, sin bloquear ni alterar el motor de audio de baja latencia.

### 5. Estado de Certificación Técnica
```text
Suite de pruebas completa: 285 tests / 158.747 assertions / 0 failures
Slice: Unificación de Certificación, Presentación y Reproducción (Fase 20.8.6)
Estado: COMPLETADO Y AUDITADO
Regresiones: 0
```




