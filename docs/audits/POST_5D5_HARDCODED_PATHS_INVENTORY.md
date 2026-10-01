# INVENTARIO EXHAUSTIVO DE REFERENCIAS ABSOLUTAS Y RUTAS PERSONALES
## Microhito POST-5D.5 — Incidente de Reproducibilidad CI: Hermetización de Rutas y Baseline No-VES

**Documento:** `docs/audits/POST_5D5_HARDCODED_PATHS_INVENTORY.md`  
**Fecha de emisión:** 2026-09-29  
**Fase:** POST-5D.5.1  
**Autor:** Antigravity (Lead Architect & Supervisor)  
**Estado:** 🟢 **COMPLETO Y CERTIFICADO PARA DISEÑO (POST-5D.5.2)**  
**Addendum:** §10 documenta la resolución real y **qué premisas de este documento quedaron refutadas** al ejecutar el plan. Leer §10 antes de usar §3–§9 como guía.  

---

## 1. Declaración de Propósito y Fronteras de Seguridad

El presente inventario audita de forma exhaustiva, determinista y reproducible cualquier referencia a rutas absolutas del entorno personal de desarrollo (`D:/desarrollos/`, `d:/desarrollos/`, `ABDSynths/`, `ABDSharedAssets/`, `C:/Users/`, rutas UNC personales) presentes en el repositorio `ABDAudioLab`.

### Fronteras Inviolables:
1. **Aislamiento Técnico:** Este inventario pertenece a un **incidente de reproducibilidad de CI** independiente del alcance funcional de Audio A/B 5D.
2. **Cero Alteraciones Canónicas 5D:** Se certifica que los reportes de corridas (`docs/qa/runs/*.json`), el manifest canónico (`docs/qa/audio-ab-5d-baseline-manifest.json`) y la huella criptográfica (`docs/qa/audio-ab-5d-artifacts.sha256`) se mantienen **100% inmutables y sin cambios**.
3. **Cero Hacks en CI:** El objetivo es la hermeticidad de la suite; queda prohibido alterar Gate 6 con supresores de error (`--allow-running-no-tests`, `|| true`, `continue-on-error`).
4. **Gobernanza de Hardware:** 0 bytes físicos autorizados. D2.7B, VES y ExportReadiness permanecen bloqueados.

---

## 2. Metodología Exhaustiva de Escaneo

El escaneo se ejecutó sobre todo el árbol del repositorio excluyendo directorios volátiles no versionados (`build/`, `.git/`), mediante los siguientes vectores de búsqueda:

1. **Patrón de unidad de desarrollo personal:** `(?i)[dD]:[/\\]desarrollos`
2. **Patrón de ecosistema y carpetas compartidas:** `ABDSynths`, `ABDSharedAssets`, `ABDAudioLab[/\\]build`
3. **Patrón de perfil de usuario Windows:** `(?i)C:[/\\]Users[/\\]`, `(?i)[a-zA-Z]:[/\\]Users[/\\]`
4. **Patrones UNC y servidores locales:** `\\\\` (excluyendo tuberías kernel IPC `\\\\.\\pipe\\`)
5. **Inspección de construcción y configuración:** `CMakeLists.txt`, `*.cmake`, scripts (`build.bat`, `run-plan.bat`), workflows de GitHub Actions (`.github/workflows/`), JSON fixtures (`fixtures/`, `presets/`, `profiles/`, `contracts/`).
6. **Inspección de construcción por concatenación y macros:** Búsqueda de `getChildFile` con rutas fijas o variables de preprocesador.

---

## 3. Matriz Exhaustiva de Coincidencias en Código Activo

| ID | Archivo | Línea o símbolo | Ruta / recurso actual | Clase | Participa en Gate 6 (`~[ves]`) | Sustitución prevista | Decisión inequívoca |
|:---:|---|---|---|---|:---:|---|---|
| **R01** | `src/tests/test_TargetProfileTransportSafety.cpp` | L18 (`pro800File`) | `D:/desarrollos/ABDSynths/ABDAudioLab/profiles/targets/behringer_pro800.target.json` | Test | **SÍ (BLOQUEANTE)** | `core::repoResource("profiles/targets/behringer_pro800.target.json")` | ✅ **MIGRADO:** `core::repoResource()`. |
| **R02** | `src/tests/test_TargetProfileTransportSafety.cpp` | L19 (`dx7File`) | `D:/desarrollos/ABDSynths/ABDAudioLab/profiles/targets/yamaha_dx7.target.json` | Test | **SÍ (BLOQUEANTE)** | `core::repoResource("profiles/targets/yamaha_dx7.target.json")` | ✅ **MIGRADO:** `core::repoResource()`. |
| **R03** | `src/tests/test_TargetProfileTransportSafety.cpp` | L20 (`ds1File`) | `D:/desarrollos/ABDSynths/ABDAudioLab/profiles/targets/boss_ds1_distortion.target.json` | Test | **SÍ (BLOQUEANTE)** | `core::repoResource("profiles/targets/boss_ds1_distortion.target.json")` | ✅ **MIGRADO:** `core::repoResource()`. |
| **R04** | `src/tests/test_TargetProfileTransportSafety.cpp` | L43 (`contractsDir`) | `D:/desarrollos/ABDSynths/ABDAudioLab/contracts/hardware` | Test | **SÍ (BLOQUEANTE)** | `core::contractsHardwareDir()` | ✅ **MIGRADO:** `core::repoResource()`. |
| **R05** | `src/tests/test_TargetProfileTransportSafety.cpp` | L50 (`targetsDir`) | `D:/desarrollos/ABDSynths/ABDAudioLab/profiles/targets` | Test | **SÍ (BLOQUEANTE)** | `core::repoResource("profiles/targets")` | ✅ **MIGRADO:** `core::repoResource()`. |
| **R06** | `src/tests/test_TargetProfileTransportSafety.cpp` | L72 (`ds1File`) | `D:/desarrollos/ABDSynths/ABDAudioLab/profiles/targets/boss_ds1_distortion.target.json` | Test | **SÍ (BLOQUEANTE)** | `core::repoResource("profiles/targets/boss_ds1_distortion.target.json")` | ✅ **MIGRADO:** `core::repoResource()`. |
| **R07** | `src/tests/test_TargetProfileTransportPolicy.cpp` | L309 (`targetsDir`) | `D:/desarrollos/ABDSynths/ABDAudioLab/profiles/targets` | Test | **SÍ** | `core::repoResource("profiles/targets")` | ✅ **MIGRADO:** `core::repoResource()`. |
| **R08** | `src/tests/test_TargetProfileMidiSysEx.cpp` | L16 (`profileFile`) | `D:/desarrollos/ABDSynths/ABDAudioLab/profiles/targets/yamaha_dx7.target.json` | Test | **SÍ** | `core::repoResource("profiles/targets/yamaha_dx7.target.json")` | ✅ **MIGRADO:** `core::repoResource()`. |
| **R09** | `src/tests/test_TargetProfileMidiCc.cpp` | L16 (`profileFile`) | `D:/desarrollos/ABDSynths/ABDAudioLab/profiles/targets/behringer_pro800.target.json` | Test | **SÍ** | `core::repoResource("profiles/targets/behringer_pro800.target.json")` | ✅ **MIGRADO:** `core::repoResource()`. |
| **R10** | `src/tests/test_TargetProfileMidiCc.cpp` | L155 (`recipeFile`) | `D:/desarrollos/ABDSynths/ABDAudioLab/presets/profiling/quick_vcf_3pts.json` | Test | **SÍ** | `core::repoResource("presets/profiling/quick_vcf_3pts.json")` | ✅ **MIGRADO:** `core::repoResource()`. |
| **R11** | `src/tests/test_TargetProfileManualOperator.cpp` | L16 (`profileFile`) | `D:/desarrollos/ABDSynths/ABDAudioLab/profiles/targets/boss_ds1_distortion.target.json` | Test | **SÍ** | `core::repoResource("profiles/targets/boss_ds1_distortion.target.json")` | ✅ **MIGRADO:** `core::repoResource()`. |
| **R12** | `src/tests/test_TargetProfileDexedProfile.cpp` | L18 (`profileFile`) | `D:/desarrollos/ABDSynths/ABDAudioLab/profiles/targets/dexed.target.json` | Test | **SÍ** | `core::repoResource("profiles/targets/dexed.target.json")` | ✅ **MIGRADO:** `core::repoResource()`. |
| **R13** | `src/tests/test_TargetProfileDexedProfile.cpp` | L127 (`recipeFile`) | `D:/desarrollos/ABDSynths/ABDAudioLab/presets/profiling/quick_vcf_3pts.json` | Test | **SÍ** | `core::repoResource("presets/profiling/quick_vcf_3pts.json")` | ✅ **MIGRADO:** `core::repoResource()`. |
| **R14** | `src/tests/test_TargetProfileDexedHosting.cpp` | L57 (`profileFile`) | `D:/desarrollos/ABDSynths/ABDAudioLab/profiles/targets/dexed.target.json` | Test | **SÍ** | `core::repoResource("profiles/targets/dexed.target.json")` | ✅ **MIGRADO:** `core::repoResource()`. |
| **R15** | `src/tests/test_TargetProfileDexedHosting.cpp` | L119 (`recipeFile`) | `D:/desarrollos/ABDSynths/ABDAudioLab/presets/profiling/quick_vcf_3pts.json` | Test | **SÍ** | `core::repoResource("presets/profiling/quick_vcf_3pts.json")` | ✅ **MIGRADO:** `core::repoResource()`. |
| **R16** | `src/tests/test_TargetProfileDexedBehavior.cpp` | L82 (`profileFile`) | `D:/desarrollos/ABDSynths/ABDAudioLab/profiles/targets/dexed.target.json` | Test | **SÍ** | `core::repoResource("profiles/targets/dexed.target.json")` | ✅ **MIGRADO:** `core::repoResource()`. |
| **R17** | `src/tests/test_SynthTargetLifecycleAdapter.cpp` | L138, L151 (`fp1/fp2.pluginPath`) | `D:/desarrollos/ABDSynths/ABDAudioLab/build/ReferenceSynth.vst3` y variante minúscula | Test (Normalización de path) | **SÍ** | Sustituir por rutas sintéticas mock puras independientes del entorno de host (`mock_build/plugins/ReferenceSynth.vst3`) | ✅ **MIGRADO:** rutas sinteticas `mock_build/...`; el test no toca disco. |
| **R18** | `src/tests/test_OfficialLutGeneration.cpp` | L99 (`exportDir`) | `D:/desarrollos/ABDSynths/ABDAudioLab/exported_luts` | Test / Fixture | **SÍ** | `core::repoResource("exported_luts")` o directorio temporal de test | ✅ **MIGRADO:** `core::repoResource()`. |
| **R19** | `src/tests/test_MidiIdentityDetector.cpp` | L15, L16 (`searchRoots`) | `d:/desarrollos/ABDSynths/ABDAudioLab` y `D:/desarrollos/ABDSynths/ABDSharedAssets` | Dependencia externa / Fixture | **SÍ** | `core::requireRepoRoot()` y resolución prioritaria de `contracts/hardware/` local versionado | ✅ **HECHO:** `contracts/hardware/` versionado en el repo. |
| **R20** | `src/tests/test_MidiDeviceHotplugMonitor.cpp` | L15 (`contractsDir`) | `D:/desarrollos/ABDSynths/ABDSharedAssets/contracts` | Dependencia externa / Fixture | **SÍ** | `core::contractsHardwareDir()` apuntando a `contracts/hardware/` local | ✅ **HECHO:** `contracts/hardware/` versionado en el repo. |
| **R21** | `src/tests/test_VesCz101SemanticControl.cpp` | L115 (`downloadsRom`) | `C:\Users\ajaba\Downloads\ves-windows\ROMS` | Test (VES) | **NO** (excluido por `~[ves]`) | Lectura de variable de entorno **`VES_ROM_DIR`** con fallback seguro vacio | ✅ **MIGRADO:** variable de entorno. Ver nota en R21. |
| **R22** | `src/tests/test_VesCz101Feasibility.cpp` | L125 (`defaultDownloads`) | `C:\Users\ajaba\Downloads\ves-windows\ROMS` | Test (VES) | **NO** (excluido por `~[ves]`) | Lectura de variable de entorno **`VES_ROM_DIR`** con fallback seguro vacio | ✅ **MIGRADO:** variable de entorno. Ver nota en R21. |
| **R23** | `src/synth/SynthTargetLifecycleAdapters.cpp` | L153 (`demoBuild`) | `D:\desarrollos\ABDSynths\_RESOURCES\DemoSynthPlugin-main\...` | Runtime Productivo (Residuo) | Potencial | Ascenso de niveles desde el ejecutable; la variable `ABDAUDIOLAB_VST3_PATH` ya cubre el caso arbitrario | ✅ **MIGRADO:** se eliminó el literal y se sube desde el ejecutable. **No** se borró la rama: vea §10.4. |
| **R24** | `src/gui/SoundIdSplashScreen.h` | L110 (`f`) | `d:/desarrollos/ABDSynths/ABDAudioLab/assets/splash_art.jpg` | Runtime GUI | No (test headless) | Sustituir los 5 fallbacks por `core::optionalRepoResource("assets")` | ✅ **MIGRADO.** Ver §10.4. |
| **R25** | `src/gui/session/ProfilingSessionController.cpp` | L1289 (`repoDir`) | `d:/desarrollos/ABDSynths/ABDAudioLab/fixtures/evaluations` | Runtime Productivo | No directamente en Gate 6 | `core::optionalRepoResource("fixtures/evaluations")`; las 2 ocurrencias de CWD restantes se justifican | ✅ **MIGRADO:** `core::repoResource()`. |
| **R26** | `src/gui/MainContentComponent.cpp` | L115 (`roots`) | `D:/desarrollos/ABDSynths/ABDSharedAssets/contracts` | Runtime GUI / Orquestación | No (test headless) | Vector de raices sin ruta absoluta: `sharedAssetsDir()` + `optionalRepoResource` | ✅ **MIGRADO.** Ver §10.4. |

---

## 4. Análisis Detallado por Tipología de Recurso y Decisión de Migración

> **Cómo leer esta sección.** Registra la decisión que se tomaró en su momento y **cómo
> resultó al ejecutarla**. El título original decía "Decisión inequívoca" y eso ya no es
> cierto: varias de estas decisiones resultaron equivocadas al aplicarlas. No se reescriben para que
> el diagnóstico parezca acertado desde el principio — se dejan con su resultado real, porque un
> análisis que se corrige a posteriori para parecer infalible no sirve para aprender. Vea §10.

### 4.1. Recursos de Test y Fixtures Propios de ABDAudioLab (R01–R16, R18, R25)
- **Qué representan:** Archivos `.target.json` de perfiles declarativos (`profiles/targets/`), recetas de medición (`presets/profiling/quick_vcf_3pts.json`), carpetas de exportación de LUTs (`exported_luts/`) y fixtures de evaluaciones (`fixtures/evaluations/`).
- **Problema actual:** En `test_TargetProfileTransportSafety.cpp`, los paths son 100% fijos sin fallback. En las demás suites, existe un fallback a `getCurrentWorkingDirectory()`, pero si la prueba se ejecuta desde `build/` o un directorio de salida de CMake, el fallback colapsa inmediatamente a la ruta de máquina personal `D:/desarrollos/...`.
- **Decisión original:** **Migrar al resolver de ABDAudioLab.** — *Ejecutada tal cual.* Se sustituyen por llamadas a `abdaudiolab::core::repoResource(...)` del módulo canónico `src/core/LabResourcePaths.h`. **Ver §10.1:** la premisa de crear un `TestPathResolver` propio quedó refutada.

### 4.2. Contratos y Assets de Dependencias Compartidas (R19, R20)
- **Qué representan:** La especificación de contratos de dispositivos MIDI y hardware. Históricamente residían en `ABDSharedAssets/contracts`.
- **Análisis de arquitectura:** `ABDAudioLab` ya contiene en su propio árbol de fuentes un snapshot versionado y canónico en `contracts/hardware/` (con 31 especificaciones contractuales). Depender en tests de un repo externo `ABDSharedAssets` no clonado en CI produce fallos silenciosos o regresiones.
- **Decisión original:** **Vendorizar un fixture mínimo, con versión y hash.** — *Ejecutada;  quedó versionado en el repo.* Los tests de `ABDAudioLab` deben validar prioritariamente el contrato versionado dentro del repositorio en `contracts/hardware/` mediante `core::contractsHardwareDir()`, eliminando toda dependencia obligatoria de una carpeta externa fuera del árbol de Git.

### 4.3. Normalización y Mocks de Build/Runtime (R17)
- **Qué representan:** `test_SynthTargetLifecycleAdapter.cpp` valida que el método `TargetFingerprint::computeNormalizedFingerprint()` es invariante ante diferencias de mayúsculas/minúsculas y separadores de barra (`/` vs `\`).
- **Problema actual:** El autor del test utilizó cadenas con el path de su máquina local (`D:/desarrollos/ABDSynths/ABDAudioLab/build/ReferenceSynth.vst3`). No accede al disco, pero dispara falsos positivos en herramientas de auditoría y guardrails.
- **Decisión original:** **Migrar al resolver de build.** — *Ejecutada.* Reemplazar las cadenas personales por rutas sintéticas mock (ej. `mock_build/plugins/ReferenceSynth.vst3` vs `mock_build\\plugins\\referencesynth.vst3`).

### 4.4. Fixtures de Emuladores Externos no Distribuidos (R21, R22)
- **Qué representan:** Pruebas de integración para Vintage Emulator Studio (VES) y ROMs del Casio CZ-101 (`C:\Users\ajaba\Downloads\ves-windows\ROMS`).
- **Participación en Gate 6:** **NO participan** porque están etiquetadas con `[ves]` y Gate 6 ejecuta explícitamente `~[ves]`.
- **Decisión original:** **Inyectar mediante entorno/configuración de test.** — *Ejecutada, pero con el nombre de variable equivocado.* Leer la variable `VES_ROM_DIR` —no `ABDAUDIOLAB_VES_ROM_PATH`, nombre que este documento propuso y que **no** es la convención del repositorio: `test_VesCz101Feasibility.cpp` ya usaba `VES_ROM_DIR`, y duplicar el nombre habría creado dos fuentes de verdad para lo mismo. Si no está configurada, el test omite la sección mediante `SKIP()` de Catch2 de manera limpia y determinista.

### 4.5. Fallbacks Personales en Runtime (R23, R24, R26)
- **Qué representan:**
  - `R23` (`SynthTargetLifecycleAdapters.cpp:153`): Ruta personal a un proyecto de demostración fuera del repositorio (`_RESOURCES/DemoSynthPlugin-main`).
  - `R24` (`SoundIdSplashScreen.h:110`): Fallback número 5 a `d:/...` tras 4 intentos relativos limpios.
  - `R26` (`MainContentComponent.cpp:115`): Primera entrada fija en un vector de búsqueda de contratos.
- **Decisión ORIGINAL (REFUTADA):** "Eliminar como residuo muerto". **No se sostiene.** Una rama que resuelve un recurso de terceros no es código muerto: es una dependencia externa legítima que ademas estaba mal expresada. Eliminar la rama habria roto la funcionalidad sin resolver la ruta personal.
- **Decisión CORRECTA:** sustituir la referencia a la máquina por la API canónica (`sharedAssetsDir()`, `optionalRepoResource()`, o ascenso acotado desde el ejecutable para distribución portable). La distinción que faltaba es *entrada del repo* frente a *salida del host / artefacto externo*: unas van contra la raíz del repo, las otras son relativas al entorno de ejecución por naturaleza. Ver §10.4.

---

## 5. Examen de Concatenación, Macros de Compilación y Variables de CMake

### 5.1. Construcción de Rutas por Concatenación
- Se auditó el uso de `.getChildFile(...)` y operadores `/` en todo el directorio `src/`.
- La gran mayoría de los componentes de runtime (`AssetLocator.h`, `HardwareDeviceDisplayCardComponent.cpp`) utilizan traversal ascendente relativo desde `juce::File::currentExecutableFile` subiendo de 1 a 7 niveles.
- **Hallazgo ORIGINAL (REFUTADO):** "Este patrón es seguro y portable". **No lo es.** El ascenso de N niveles desde el ejecutable hace que el resultado dependa de *cuántos niveles separen el `.exe` de la raíz*, y eso varía entre un checkout, un `build/` de Ninja y un `build/` multi-config de Visual Studio. El caso real lo confirma: `HardwareDeviceDisplayCardComponent.cpp` subía 7 niveles a ciegas y por eso, pese a migrar todo lo demás, seguía siendo una reimplementación de la resolución y quedaba fuera de todo barrido. Ver §10.7.
- **Hallazgo CORRECTO:** consultar el directorio del ejecutable es legítimo **solo** para distribución portable (assets al lado del binario), y **un solo nivel**, nunca como forma de localizar la raíz del repositorio. Para el repositorio existe una única API: `core::repoResource()` / `core::requireRepoRoot()` / `core::optionalRepoResource()` / `core::sharedAssetsDir()`. El guard `[hygiene]` prohíbe `currentExecutableFile` en producción fuera de una allowlist con motivo escrito.

### 5.2. Macros de Compilación y CMakeLists.txt
- En `CMakeLists.txt`, `ABDAudioLab_Tests` define:
  ```cmake
  target_compile_definitions(ABDAudioLab_Tests PRIVATE
      _CRT_SECURE_NO_WARNINGS
      NOMINMAX
      JUCE_PLUGINHOST_VST3=1
      JUCE_PLUGINHOST_AU=$<IF:$<PLATFORM_ID:Darwin>,1,0>
      ABD_TESTING=1
  )
  ```
- No existen macros que inyecten rutas absolutas de host (`CMAKE_SOURCE_DIR` no se pasa como `-D` en C++).
- Las dependencias `ABDSharedCode` y `ABDScope` se resuelven en CMake de forma condicional: si existen localmente como hermanas (`${CMAKE_CURRENT_SOURCE_DIR}/../...`) las incluye, y si no, las descarga vía `FetchContent` con SHA pinneado e inmutable.

### 5.3. JSON de Fixtures y Presets
- Se revisaron todos los archivos en `profiles/targets/*.json`, `presets/profiling/*.json`, `fixtures/*.json` y `contracts/**/*.json`.
- **Resultado:** Cero rutas absolutas personales en archivos de datos. Todos los identificadores son relativos o URIs de esquema abstracto (`https://json-schema.org/draft/2020-12/schema`).

---

## 6. Análisis de Rutas Falsas / Mocks / Aserciones Negativas (No Deuda)

El escaneo detectó las siguientes cadenas con formato de ruta o letra de unidad que **NO** constituyen deuda técnica ni rutas personales, sino datos de prueba legítimos:

1. `src/tests/test_WorkerProcessHost.cpp:116`: `"C:/Ruta/Ficticia/Inexistente.exe"`  
   *Justificación:* Test de robustez ante ejecutable inexistente (fallo deliberado controlado).
2. `src/tests/test_SessionIoController.cpp:86`: `"Z:/non_existent_drive_9999/session.abdlabtest"`  
   *Justificación:* Test de fallo deliberado ante unidad no mapeada.
3. `src/tests/test_PluginHostManager.cpp:175`: `"C:/path/to/non_existent_abdaudio_test.vst3"`  
   *Justificación:* Test de error ante plugin no encontrado.
4. `src/tests/test_HardwareProfileValidation.cpp:101`: `"d:/non_existent_folder_xyz/invalid_hw.json"`  
   *Justificación:* Test de validación ante archivo JSON no existente.
5. `src/tests/test_UiCoordinatorGovernance.cpp:441`: `"d:/plugins/MockDexed.vst3"`  
   *Justificación:* Identificador sintético de plugin mock en memoria.
6. `src/tests/test_OutOfProcessVst3LifecycleAdapter.cpp:64, 74, 294`: `"C:/Plugins/TestSynth.vst3"`, `"C:/Ruta/Completamente/Falsa/NoExiste.vst3"`  
   *Justificación:* Identificadores de solicitud IPC en memoria sin acceso a disco.
7. `src/tests/test_ExperimentStorage.cpp:207`: `"C:\autoexec.bat"`  
   *Justificación:* Aserción de seguridad contra path traversal (`CHECK_FALSE(isSafeRelativePath(...))`).
8. `src/tests/test_AudioABBaselineManifestValidation5D.cpp:103-105`: Verificación metrológica de que los reportes de Audio A/B 5D no contienen `"C:\"`, `"D:\"` ni `"/Users/"`.
9. `src/ipc/Win32NamedPipe.cpp:17`: `"\\\\.\\pipe\\abdaudiolab_..."`  
   *Justificación:* Sintaxis nativa Win32 para tuberías nombradas locales en memoria kernel (no es ruta de red UNC).
10. Rutas estándar del sistema VST3 en Windows: `"C:\Program Files\Common Files\VST3\..."` presentes en varios tests de plugins opcionales. Están protegidas con guardas `if (!file.exists()) SKIP(...)` y representan la convención estándar del sistema operativo, no rutas de desarrollo personal.

---

## 7. Diagnóstico Causa-Raíz del Fallo de Gate 6 en CI (Run #6)

En el Run #6, Gate 6 ejecutó:
```powershell
.\build\Release\ABDAudioLab_Tests.exe "~[ves]" -r console
```
En el runner de GitHub Actions:
1. El repositorio está clonado en `D:\a\ABDAudioLab\ABDAudioLab` o `C:\actions-runner\...`.
2. La ruta fija `D:/desarrollos/ABDSynths/ABDAudioLab` **no existe**.
3. `test_TargetProfileTransportSafety.cpp` intentó cargar directamente `pro800File`, `dx7File`, `ds1File`, `contractsDir` y `targetsDir` sin resolución relativa ni de ejecutable.
4. Las aserciones `REQUIRE(pro800File.existsAsFile())` fallaron inmediatamente, abortando la ejecución de Catch2 con exit code != 0.
5. Los tests `test_TargetProfileMidiSysEx`, `test_TargetProfileMidiCc`, `test_TargetProfileManualOperator`, `test_TargetProfileDexedProfile` dependían de fallbacks a `D:/desarrollos/...` si el CWD del runner variaba.
6. `test_MidiDeviceHotplugMonitor` y `test_MidiIdentityDetector` dependían de `ABDSharedAssets/contracts` en `D:/desarrollos/...`, ignorando que ABDAudioLab dispone de su propio snapshot versionado en `contracts/hardware/`.

---

## 8. Especificación de Diseño para POST-5D.5.2 (`TestPathResolver`)

> ⚠️ **SECCIÓN HISTÓRICA, SUPERADA.** Esta especificación **no se ejecutó**: el resolver ya existía
> y crear un segundo fue una duplicación que hubo que revertir. Lo realmente implementado está
> en `src/core/LabResourcePaths.h` y su desviación está razonada en **§10.1**. Se conserva por
> registro de auditoría, no como guía de implementación.

**TEXTO HISTÓRICO NO EJECUTADO.** Lo que sigue es el diseño que este documento propuso en su
momento. Se conserva por registro. **No es lo que se construyó** y sus nombres de función no
existen: el módulo real es `src/core/LabResourcePaths.h` con `core::repoResource()`,
`core::requireRepoRoot()` y `core::contractsHardwareDir()`. Vea §10.1.

A partir de la evidencia objetiva recolectada, el diseño del resolver debe satisfacer exactamente:

1. **Ubicación canónica:** `src/tests/TestPathResolver.h` (namespace `test_support`).
2. **Firmas obligatorias:**
   ```cpp
   namespace test_support
   {
       // Devuelve la raíz comprobada del repositorio ABDAudioLab
       juce::File resolveRepoRootForTests();

       // Resuelve un recurso relativo a la raíz del repositorio (ej. "profiles/targets/dexed.target.json")
       juce::File resolveRepoResource(const juce::String& relativePath);

       // Resuelve la raíz de contratos de hardware (priorizando contracts/hardware dentro de ABDAudioLab)
       juce::File resolveContractsDirectory();
   }
   ```
3. **Jerarquía estricta de 4 niveles:**
   - **Nivel 1:** Variable de entorno `ABDAUDIOLAB_REPO_ROOT` (solo para test o CI explícitamente configurado, validando marcadores).
   - **Nivel 2:** Ubicación del ejecutable (`juce::File::getSpecialLocation(juce::File::currentExecutableFile)`) + ascenso por ancestros (hasta 6 niveles) validando marcadores reales del repositorio.
   - **Nivel 3:** Working directory (`juce::File::getCurrentWorkingDirectory()`) únicamente como fallback de desarrollo local si contiene los marcadores.
   - **Nivel 4:** Error determinista (`std::runtime_error`) informativo y sin ocultar la causa, detallando todas las rutas intentadas y los marcadores requeridos ausentes.
4. **Marcadores requeridos de raíz:**
   - `CMakeLists.txt`
   - `profiles/targets`
   - `docs`
   - `contracts/hardware`

---

## 9. Lista de Comprobación y Criterios de Cierre de POST-5D.5.1

- [x] **Búsqueda de D:/desarrollos/ y d:/desarrollos/:** 26 referencias activas identificadas. *(Recuento de la detección original. Estado verificable actual: **0** en `src/**`; ver §10.5.)*
- [x] **Búsqueda de ABDSynths, ABDAudioLab/build y ABDSharedAssets:** Completa y categorizada.
- [x] **Búsqueda de C:/Users/ y rutas UNC personales:** Identificados 2 casos en VES (inyección por entorno) y 1 caso de named pipe kernel (no deuda).
- [x] **Clasificación de cada aparición:** Identificadas en la tabla con sus clases formales.
- [x] **Identificación explicita de archivos alcanzados por Gate 6:** R01 a R20 identificados como participantes directos de `~[ves]`. *(Recuento de la detección original; ver §10.5 para el estado verificable actual.)*
- [x] **Decisión de migración por cada referencia activa:** Asignada una de las 6 decisiones inequívocas a cada fila. *(Varias de esas decisiones quedaron refutadas al ejecutarlas; ver §10.4.)*
- [x] **Separación clara:** Diferenciado entre test, fixture, runtime productivo, documentación y código muerto.
- [x] **Confirmación de que no se han modificado artifacts canónicos 5D:** `docs/qa/runs/*.json`, `docs/qa/audio-ab-5d-baseline-manifest.json` y `docs/qa/audio-ab-5d-artifacts.sha256` se mantienen idénticos (0 diffs en Git).
- [x] **Inspección de construcción por concatenación, macros de CMake y fixtures JSON:** Completada sin hallazgos adicionales ocultos.

**Conclusión:** La tarea **POST-5D.5.1 queda formalmente CERRADA y CERTIFICADA**. Se autoriza el paso a **POST-5D.5.2**.

> ⚠️ **Corrección posterior (ver §10.1).** Este cierre autorizaba "Diseño e implementación de
> `TestPathResolver` y su test unitario". Eso **no** ocurrió: se confirmó que
> `src/core/LabResourcePaths` ya cubría el diseño, el helper nuevo se descartó por duplicarlo, y
> POST-5D.5.2 se cerró **extendiendo el módulo existente**. La autorización original queda anulada.

---

## 10. Addendum de Resolución (2026-09-30) — Premisas Refutadas y Estado Real

Este addendum se emite tras **ejecutar** el plan de las secciones 1–9. Un inventario que solo
diagnostica y no se contrasta con la realidad se convierte en deuda documentary: el lector
implementa literalmente un diseño que ya se demostró equivocado. Aquí queda el contraste.

### 10.1 REFUTADA — "Hay que crear `src/tests/TestPathResolver.h`"

**Lo que decía §8:** crear un helper nuevo en `test_support` con `resolveRepoRootForTests()`,
`resolveRepoResource()` y `resolveContractsDirectory()`.

**Lo que se hizo:** `TestPathResolver.h` se escribió, se registró en `CMakeLists.txt` y se
**eliminó**. Motivo: `src/core/LabResourcePaths.{h,cpp}` ya implementaba exactamente esa
jerarquía (`ABDAUDIOLAB_REPO_ROOT` → ejecutable con marcadores → CWD → error explícito) y ya
tenía su test unitario (`test_LabResourcePaths.cpp`). El helper nuevo era una **copia** de esa
jerarquía: mantener dos resolutores es precisamente la deuda que este microhito vino a cerrar.

**Consecuencia:** el módulo canónico y único es `abdaudiolab::core::repoResource(...)`.
Los nombres de §8 (`test_support::*`) **no existen**; usarlos compila solo si alguien
reintroduce el duplicado.

### 10.2 REFUTADA — "`repoResource()` es la API para producción"

`repoResource()` **lanza excepción** si el recurso no existe. Usarla en el arranque de la app
haría que la ausencia de un asset abortase el proceso. Se añadió para producción:

- `optionalRepoResource(rel)` — devuelve `File{}` inválida en vez de lanzar.
- `sharedAssetsDir()` — resuelve el repo hermano `ABDSharedAssets` (no versionado aquí) y no lanza.

**Consecuencia:** una migración de producción que use `repoResource()` donde debía usar
`optionalRepoResource()` reintroduce un fallo de arranque, no de CI.

### 10.3 REFUTADA — "El guard permanente debe buscar rutas absolutas"

**Lo que decía POST-5D.5.7 en `PLAN.md`:** un guard que falle ante `D:/desarrollos/` o `C:/Users/`.

**Lo que se construyó primero (mal):** `test_ResourcePathHygiene.cpp` buscando
`getCurrentWorkingDirectory()`. Ese barrido **no ve la causa real del Run #6**: R01–R06 eran
literales absolutos sin fallback, que no mencionan el CWD. El guard era verde sobre exactamente
el código que había roto CI.

**Lo que hay ahora:** dos barridos independientes, ambos deterministas y con invariantes
anti-vacuidad (`REQUIRE(scanned > 0)`):

| Barrido | Patrón | Cubre |
|:---:|---|---|
| `[hygiene][resourcepaths][hermetic]` | `getCurrentWorkingDirectory` | dependencia del CWD |
| `[hygiene][resourcepaths][production]` | `getCurrentWorkingDirectory` en `src/` | lo mismo, en producción |
| `[hygiene][resourcepaths][hermetic]` | `D:/desarrollos`, `C:/Users/` | **literales absolutos** (tests) |
| `[hygiene][resourcepaths][production]` | `D:/desarrollos`, `C:/Users/` | **literales absolutos** (producción) |

El patrón de ruta es deliberadamente estrecho: **no** se busca cualquier `X:/`, porque las rutas
ficticias de §6 (`C:/Ruta/Ficticia/Inexistente.exe`, `Z:/non_existent_drive_9999/...`,
`C:\autoexec.bat`) son aserciones negativas legítimas y deben seguir siendo válidas.

### 10.4 REFUTADA — "Eliminar como residuo muerto" (R23, R24, R26, y el par CWD de R25)

Varias filas de §3 **no** se eliminaron: se migraron a `LabResourcePaths`. La
distinción que faltaba en el inventario era entre *"referencia a la ruta de mi máquina"* (mal) y
*"referencia a un recurso que no vive en el repo"* (correcto, pero debe ir por una API explícita):

- **R24** (`SoundIdSplashScreen.h`) y **R26** (`MainContentComponent.cpp`): migrados a
  `optionalRepoResource` / `sharedAssetsDir`. La GUI sigue funcionando, sin literales personales.
- **R25** (`ProfilingSessionController.cpp`): `getEvaluationsDirectory()` migrado. Las otras dos
  ocurrencias de CWD se **mantienen justificadas**: destino de exportación elegido por el usuario
  y `guided/evidence` producido por el worker.
- **R23** (`SynthTargetLifecycleAdapters.cpp`) y `OutOfProcessVst3LifecycleAdapter.cpp`: **no se
  pueden migrar**. Apuntan a VST3 de terceros y al ejecutable del plugin worker, que no están en
  el repo. Van a la allowlist `allowedProductionCwdConsumers()` con su motivo escrito.

**Consecuencia:** "residuo muerto" era una etiqueta prematura. La regla correcta es la que sostiene
la allowlist: *entradas del repo → contra la raíz del repo; salidas del usuario y artefactos
externos → relativas al entorno de ejecución, por naturaleza.*

### 10.5 CORREGIDO — Conteos y números de línea de §3

`§9` declara "26 referencias activas" y "R01 a R20 identificados como participantes directos de
`~[ves]`". Tras la ejecución, el recuento **verificable hoy** es:

- **0** rutas absolutas personales en `src/**` (excluyendo el propio guard, que nombra el patrón).
- **0** rutas de perfil de usuario `C:/Users/` en `src/**`.
- Las 6 filas VES (R21, R22 y las de `[ves]`) ya no aparecen porque `C:\Users\ajaba\...` se
  sustituyó por la variable de entorno **`VES_ROM_DIR`** (mismo nombre que ya usaba
  `test_VesCz101Feasibility`). Sin ella el test se omite limpio.
- Los **números de línea de §3 están obsoletos**: el fichero se migró y las líneas se movieron.
  La tabla se conserva como registro de la detección original, no como localización actual.

### 10.6 Lo que NO se resolvió (y no debe darse por cerrado)

- 1 de 7 corridas completas de `~[ves]` murió en silencio (exit 3) tras el render offline de
  Dexed T2.4 con `bit_exact: NO`. No reproduce en aislamiento (6/6) ni aislando `[dexed]` (4/4).
- `contracts/hardware/roland_aira_patch_spec.schema.json` reaparece en cada corrida completa
  (2/2) y no aparece en aislamiento. Descartados proceso externo, generador en `src/` y copia
  desde `ABDSharedAssets`. **Sin atribuir a ningún test concreto.**

Ambos son independientes de las rutas y siguen abiertos.

### 10.7 Hallazgos surgidos al ejecutar el plan (2026-10-01)

Estos tres no eran predicciones del inventario: los produjo la propia ejecución, y los tres
muestran por qué el guard necesitaba ampliarse en vez de darse por bueno.

**a) R23 seguía vivo.** `SynthTargetLifecycleAdapters.cpp:153` conservaba la ruta absoluta
`D:\desarrollos\ABDSynths\_RESOURCES\DemoSynthPlugin-main\...`. Estaba en la allowlist de
producción, pero **solo por su uso de CWD**: la allowlist de CWD no prohibitaba el literal
absoluto, así que la ruta personal siguió ahí unnoticed durante toda la fase de tests. El
barrido de literales absolutos la encontró en la primera corrida. Sustituida por resolución
ascendente desde el ejecutable; la variable `ABDAUDIOLAB_VST3_PATH` ya cubría el caso arbitrario.

**b) La allowlist de CWD estaba caducada en 11 de 14 entradas.** Los ficheros se habían migrado
en fases anteriores pero nadie había retirado sus permisos. Una allowlist con permisos caducados
es peor que no tenerla: el fichero queda libre y el guard deja de vigilarlo. Invariante añadida
al propio fichero del guard.

**c) Un needle con barra simple no encuentra una ruta escrita con barra invertida.** En C++ la
misma ruta aparece como `"D:/desarrollos/..."`, como `R"(D:\desarrollos\...)"` y como
`"D:\desarrollos\..."`. El primer needle solo encontraba la primera forma. Probado con un
sonda que contenía las cinco variantes: **fallaba tres de cinco**. Corregido normalizando la
línea (doble barra → simple, barra invertida → barra normal) antes de comparar.

**Verificación de no-vacuidad del guard (resultado, no afirmación):** con una sonda temporal
en `src/tests/` se detectaron las cinco formas —`getCurrentWorkingDirectory`, `D:/desarrollos`,
`D:\desarrollos\`, `C:\Users\ajaba\` y `R"(C:\Users\ajaba\...)"`— nombrando fichero y línea
en los barridos de test y de producción. Sonda retirada después. Con el código limpio:
8 casos y 57 aserciones en verde.

**Nota sobre el barrido de producción:** `REQUIRE(result.unreadable.empty())` y
`REQUIRE(result.scanned > 0)` son la red anti-vacuidad. Un guard que lee 0 ficheros pasa en
verde; por eso se afirma el número de ficheros leídos, no solo que no haya infracciones.

---

## 11. Cierre de la hermeticidad de ESCRITURA (2026-10-01)

§10 cerró la hermeticidad de *lectura*: que un test resuelva sus rutas sin depender del
directorio de trabajo. Quedaba la otra mitad, igual de silenciosa: **los tests escribían
dentro del árbol del repositorio**.

### 11.1 Qué se audtaba

De 348 escrituras brutas en `src/tests/*.cpp`, solo cuatro escribían en el árbol del repo:

| Fichero | Destino | Ficheros versionados tocados |
|---|---|---|
| `test_AudioABCanonicalPresetRuns5D.cpp` | `docs/qa/runs/RUN_5D_NN_acceptance_report.json` | **10** |
| `test_ModelEvaluation.cpp` | `fixtures/evaluations/*.json` | **5** |
| `test_CasioCzLiveScanSuite.cpp` | `assets/presets/casio_cz101_mame_ves_session.json` | 1 |
| `test_OfficialLutGeneration.cpp` | `exported_luts/` (no versionado, pero creaba el directorio dentro del repo) | 0 |

El resto ya usaba `%TEMP%`. El daño real no es el fichero sucio: es que un gate de «el árbol
está limpio» se vuelve imposible de evaluar cuando el propio test ensucia el árbol, y un
artefacto regenerado sin querer se confunde con un cambio de código en el commit siguiente.

### 11.2 El módulo `src/tests/support/LabTestScratch`

Dos salidas, y solo dos:

- **`scratchDir(caseName)`** — `%TEMP%/abdaudiolab-tests/<nombre>`, vaciado recursivamente al
  entrar. Resuelve los tres fallos del patrón anterior (`getSpecialLocation(tempDirectory)
  .getChildFile("nombre_fijo")`): nombre fijo compartido entre tests, restos que se acumulan
  sin límite, y ausencia de negación. Aquí `scratchDir()` **lanza** si el directorio cae
  dentro de la raíz del repo, y `ScratchDir` es la variante RAII con limpieza al salir.
- **`artifactDir(repoRelativeDir, caseName)`** — el destino de los artefactos canónicos. Por
  defecto devuelve el scratch (correr la suite no toca el repo); solo con
  `ABD_REGENERATE_ARTIFACTS=1` devuelve el directorio real. Es la **única función del
  proyecto** que devuelve una ruta del repo con fines de escritura.

### 11.3 Migración aplicada

- **4 sitios** de escritura al repo → `artifactDir()` (opt-in por variable de entorno).
- **57 sitios** de temporal crudo → `scratchDir()`, en dos pasadas: 25 verificados por
  compilación en la primera, 32 (31 enunciados + 1 expresión en línea en `test_PauseResume.cpp`)
  en la segunda. Se unificaron también los que escribían un fichero suelto con nombre fijo
  directamente en la raíz de `%TEMP%` (`test_JsLutExport`, `test_LnlManifestExport`,
  `test_LoopbackDiagnostics`, `test_SysexPresetGenerator`): compartían nombre con cualquier
  otra corrida.
- **8 sitios** de `test_SessionExecutionCoordinator.cpp` y los 4 sitios de `%TEMP%` directo de
  `test_PauseResume.cpp` y `test_CasioCzLiveScanSuite.cpp` migrados a mano: eran `%TEMP%` sin
  subdirectorio, no el patrón mecánico que el script reconocía.

Quedan fuera, por pertenecer a una workstream paralela sin cerrar y no ser modificables aquí:
`test_ContractsSnapshotDrift.cpp`, `test_HardwareContractQuarantine.cpp`,
`test_HardwareContractRangeFieldNames.cpp` y `test_StartupWarningsPanel.cpp` (6 sitios con
`getSpecialLocation(tempDirectory)`). Figuran en la allowlist del guard, con el motivo.

### 11.4 El guard pasa de convención a invariante

Dos `TEST_CASE` nuevos en `src/tests/test_ResourcePathHygiene.cpp`:

1. **`[writes]` — ningún test pide una ruta del repo fuera de `artifactDir`.** Needles:
   `docsQaRunsDir`, `fixturesEvaluationsDir`, `exportedLutsDir`, `assetsDir`, `repoResource`
   (que ingiere `optionalRepoResource`). Allowlist de 9 lectores legítimos, cada uno con su
   motivo escrito. Los lectores habituales (`contractsHardwareDir`, `canonicalTargetsDir`,
   `profilingPresetsDir`, `profilesDir`, `docsQaDir`, `fixturesDir`) quedan **fuera del
   needle a propósito**: leer el repo es legítimo e inevitable, y prohibirlos obligaría a
   falsejar el código en lugar de a protegerlo.
2. **`[writes]` — las fixtures de un test viven en su scratch, no en `%TEMP%` crudo.**
   Needle en las dos grafías (`getSpecialLocation(` y `getSpecialLocation (`) porque el código
   del repo usa las dos y un needle con una sola deja pasar la otra. Segundo barrido sobre
   `src/tests/support/*.cpp`, donde la única autorizada a citar `tempDirectory` es
   `LabTestScratch.cpp`: es el punto donde se decide el scratch raíz.

El needle es el accessor, no una cadena de ruta, precisamente porque los casos que ensuciaban
el árbol no aparecían en ninguno de los barridos anteriores: escribían a través de un
accesor que devuelve una ruta del repo, y ningún patrón de texto delata un accessor.

3. **`[writes][scratch]` — `LabTestScratch` probado a sí mismo.** Que el scratch se cree, se
   vacíe al repedirlo y se borre con `clearScratchDir()`; que la raíz esté en el temporal del
   sistema; que un nombre de `TEST_CASE` con acentos, comas y corchetes se vuelva un nombre de
   directorio válido; y la invariante central vía `isInsideRepo()`: `true` para la raíz del
   repo y para un directorio real suyo, `false` para el scratch raíz y para la temporal del
   sistema. La rama de `artifactDir()` se elige con el valor **real** del proceso, así que
   correr la suite con `ABD_REGENERATE_ARTIFACTS=1` ejercita la otra mitad sin duplicar el test.

Red anti-vacuidad en los tres: `REQUIRE(result.scanned > 0)`.

### 11.5 Verificación ejecutada

El ejecutable de la suite **no existe en este árbol** (`build/Release/ABDAudioLab_Tests.exe`)
y no hay forma de recuperarlo sin cerrar la workstream de cuarentena, así que la verificación
es de compilación, no de ejecución:

- **Compilación unitaria** con los flags exactos de `Release|x64` del `.vcxproj`
  (`build/compile_one.cmd`, `/permissive- /Zc:__cplusplus /utf-8 …`): **30 ficheros** de test
  tocados + `LabTestScratch.cpp` + `test_ResourcePathHygiene.cpp`, todos en verde.
- **Simulación de los guards** (`build/sim_guard.py`, copia línea por línea de
  `scanForForbiddenPatterns()`): GUARD A lee 185 ficheros y da 0 infracciones; GUARD B lee 190
  y da 0; el barrido de `support/` lee 2 y da 0.
- **Comprobación negativa** (el guard ve el fallo que motiva el guard): sobre `HEAD`, GUARD A
  señala exactamente los cuatro testes de la tabla 11.1, y GUARD B cuenta 8 sitios en
  `test_SessionExecutionCoordinator.cpp` y 1 en `test_PauseResume.cpp` solo.

`build/Release/ABDAudioLab_Tests.exe` debe relanzarse en cuanto la workstream paralela cierre;
los tres `TEST_CASE` nuevos están escritos para no necesitar nada más que el árbol del repo.

### 11.6 Pendiente

- Los 6 sitios de los 4 ficheros de la workstream de cuarentena, y sus 4 entradas de la
  allowlist, se migran con ella.
- `E2ETempDirectory`, `TestTempDirectory`, `IntegrationTempDirectory` y `SmokeTempDirectory`
  (en `test_E2E_HermeticWorkflows.cpp`, `test_ExportIO.cpp`, `test_ModeToExportIntegration.cpp`
  y `test_SmokeStep4UI.cpp`) ya cumplen el contrato —temporal propio, vaciado y borrado al
  salir— por su cuenta, pero son cuatro copias de lo que ya hace `ScratchDir`. Convergerlas es
  limpieza, no bug: no se han tocado.
