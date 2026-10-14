# Acta de Handoff Técnico — PR #1 y Estabilización CI Gates 1–7

**Fecha:** 2026-10-08  
**Rama activa:** `feature/abdscope-sharedcode-scope`  
**PR:** #1 (`feature/abdscope-sharedcode-scope` -> `main`)  
**Base Tag:** `v2.1.1-build570-stepper-buttontokens-rc1` (Build 570 congelado en `src/BuildVersion.h`)  
**Autores del tándem:** Antigravity (Lead Architect & Planner) & ajabadia (Lead Engineer)

---

## 1. Estado de Aceptación y Resumen Ejecutivo

El objetivo de esta fase de trabajo es desbloquear y certificar la PR #1, que implementa el desacoplamiento canónico de **ABDScope** hacia el repositorio hermano `ABDSharedCode/Scope` (en la ruta compartida del equipo, sin reproducir la ruta absoluta del autor aquí porque el guard de higiene no lo permite). y garantiza que el pipeline de CI (`audio-ab-5d-ci.yml`) ejecute de forma 100% hermética y en verde todos sus controles.

### Estado Actual del Pipeline y de la PR

- **Preflight hermético de contratos de hardware (40/40):** 🟢 **PASS** (30s en runner Ubuntu).
- **Suite local focalizada (ejecutada en terminal interactiva del usuario):** 🟢 **PASS**
  - Test cases: `1031 | 1004 passed | 27 skipped | 0 failed`
  - Assertions: `228436 | 228436 passed | 0 failed`
  - Exit code: `0`
- **Compilación en CI (MSVC / CMake):** 🟢 **PASS** (`ABDAudioLab_Tests` compilado con éxito en 22m 40s).
- **Gates 1–5 en CI:** 🟢 **PASS** (Fixtures, Boundaries, Matriz canónica, Cryptographic artifacts, No-regresión 5D).
- **Gate 6 en CI (Global Non-VES Suite):** 🔴 **FAIL** (Run `37760330695`)
  ```text
  Test cases: 935 | 888 passed | 2 failed | 45 skipped
  Assertions: 208885 | 208868 passed | 17 failed
  Exit code: 1
  ```
  Los dos fallos identificados son:
  1. `HardwareContractQuarantine`: El contrato exige que el literal C salga del esquema, pero `hardwareprofile.schema.json` no contiene el campo `status`/`quarantine` esperado.
  2. `ContractsSnapshotDrift`: Se compararon 39 archivos y existe drift entre `ABDSharedAssets` y el snapshot local de `ABDAudioLab`, además de detectarse un archivo extra no esperado: `rolandairapatchspec.schema.json`.

  ### Nota de ejecución local en esta sesión
  En la ejecución local más reciente de la suite completa en la terminal del usuario, el único caso fallido fue:
  - `src/tests/test_ContractsSnapshotDrift.cpp(258)`: "El snapshot `<repo_root>/contracts/hardware` no coincide con el origen `<shared_assets>/contracts`" (con los mensajes adicionales `ficheros comparados: 40` y `falta en la copia: korg_ms2000_parameter_spec.json`).

  Eso representa 1 test case fallido y 3 aserciones fallidas, y **no es un fallo de la cadena de audio ni del cambio de `memory_order`** que se valida en este documento.
- **Gate 7 en CI (Contract Snapshot & Quarantine Gate):** 🟡 **PENDIENTE** (no certificado hasta que Gate 6 pase y el pipeline se complete íntegro).
- **Resolución de la PR #1:** ⛔ **NO lista para merge**.

---

## 2. Diagnósticos de Causa Raíz y Soluciones Aplicadas

### A. Teardown Seguro de `ProfilingSessionCoordinator` (Worker Thread Lifetime)
- **Diagnóstico:** Anteriormente, la destrucción de `ProfilingSessionCoordinator` durante tests o salidas abruptas provocaba violaciones del contrato de lifetime de `juce::Thread`, derivando en cuelgues o abortos del proceso.
- **Implementación real en la rama:**
  - El coordinador solicita cancelación explícita (`requestCancel()`), despierta el worker (`notify()`) y espera explícitamente a que termine antes de destruirlo mediante `waitForWorkerToStop(2000)`, seguido de `jassert(stopped)`.
  - El teardown usa `waitForWorkerToStop` con timeout finito y no pretende resolver el problema abandonando un hilo activo.
  - *Nota de certificación:* El lifetime no representa ya el bloqueo principal, pero no se considerará formalmente certificado de forma definitiva hasta que el run Windows completo pase de extremo a extremo sin anomalías.

### B. Preflight Hermético de Contratos de Hardware
- **Diagnóstico:** El job `contracts-preflight` fallaba al invocar `pnpm run preflight` debido a la dependencia de monorepo `@abd/eep`, ausente en el CI aislado.
- **Implementación real:** Se sustituyó por una comprobación hermética inline en Node.js que valida la paridad exacta byte por byte de los 40 contratos de hardware compartidos.
- **Distinción de alcances:**
  > [!IMPORTANT]
  > La paridad 40/40 certifica únicamente el subconjunto de hardware consumido por `ABDAudioLab`; **no sustituye** la validación global de snapshots de `ABDSharedAssets`.

### C. Alineación del Test Guard C++ `test_ContractsCiContract.cpp`
- **Diagnóstico:** `test_ContractsCiContract.cpp:167` buscaba cadenas obsoletas en el YAML (`pnpm run preflight`), fallando localmente contra el nuevo flujo hermético.
- **Implementación:** Se actualizaron las aserciones del test para validar el contrato actual del step de paridad de 40 contratos, verificando ausencia de dependencias externas.

### D. Optimización de I/O en Gate 6
- **Diagnóstico:** El flag `-d yes` en Catch2 inundaba la consola con 228.436 duraciones, provocando timeout del runner.
- **Implementación:** Se retiró `-d yes` y se fijó `timeout-minutes: 20` para Gate 6.

---

## 3. Trazabilidad de Commits en `feature/abdscope-sharedcode-scope`

Historial de commits en la rama:

| Commit | Atribución y Alcance |
|---|---|
| `c71d17f` | Primera implementación del teardown y del preflight delimitado. |
| `f8c6949` | Corrección de la ruta de origen de contratos. |
| `d5d75d5` | Telemetría de Gate 6 hacia `runner.temp` en caso de fallo. |
| `0a11eb6` | Actualización del guard C++, retirada de `-d yes` y ampliación del timeout. |

---

## 4. Diferenciación de Contratos: Subconjunto Hardware vs Snapshot Global

Existen dos contratos conceptual y técnicamente distintos en el repositorio:

1. **Contrato de hardware de ABDAudioLab:**
   - 40 archivos JSON ubicados en `contracts/hardware/`.
   - Validados por el job rápido `contracts-preflight` en Ubuntu.
   - Certifica únicamente que los perfiles hardware consumidos por el laboratorio son idénticos a los del origen.
2. **Contrato global de snapshot (`ContractsSnapshotDrift`):**
   - Validado dentro de la suite de tests (`Gate 6`).
   - Compara un conjunto de 39 archivos contra `ABDSharedAssets`, incluyendo esquemas y especificaciones generales.
   - Detectó desalineación y el archivo no esperado `rolandairapatchspec.schema.json`.

---

## 5. Tareas Pendientes Reales (Próxima Sesión)

Para reanudar el trabajo de forma efectiva, se deben abordar los siguientes puntos en orden:

### Pendiente A: `HardwareContractQuarantine`
Hay que aclarar la gobernanza del campo `quarantine` antes de aplicar cambios (no elegir una opción solo para silenciar el test):
- **Opción 1:** Añadir `status`/`quarantine` al esquema y regenerar los contratos correspondientes.
- **Opción 2:** Cambiar el test de Catch2 para que no exija un campo que el esquema formal no declara.
- **Opción 3:** Eliminar el literal manual del header C++ y leer únicamente el artefacto generado.

### Pendiente B: `ContractsSnapshotDrift`
Reconciliar el snapshot global de contratos:
- Copiar o regenerar los archivos que realmente pertenecen a la Single Source of Truth (SSOT).
- Eliminar el archivo extra `rolandairapatchspec.schema.json` si ya no pertenece al contrato.
- Actualizar los recuentos y hashes esperados.
- Re-ejecutar el test de drift.
*(Nota: Este fallo ocurre dentro de la suite global en Gate 6 y es independiente del falso positivo de ABDEep en el job de preflight).*

### Pendiente C: Gate 7
- Permanece en estado **PENDIENTE** / no certificado hasta que Gate 6 pase de forma limpia y el job ejecute Gate 7 sin saltarse pasos.

---

## 6. Secuencia para Retomar y Directrices Operativas

### Secuencia Paso a Paso para la Próxima Sesión
1. **Inspeccionar el contrato real del campo `quarantine`** y decidir qué artefacto es la SSOT.
2. **Reconciliar el snapshot global y el archivo extra** `rolandairapatchspec.schema.json`.
3. **Ejecutar localmente los filtros de ambos tests** en la terminal del usuario para confirmar la corrección.
4. **Compilar y ejecutar la suite completa** en la terminal del usuario (verificando 0 fallos).
5. **Pushear los cambios** a `feature/abdscope-sharedcode-scope`.
6. **Esperar a que Gates 1–7 terminen 100% en verde** en GitHub Actions.
7. **Solo entonces hacer merge de la PR #1** hacia `main`.
8. **Iniciar la Fase 4** (curvas y barridos físicos) a partir de `main` actualizado.

### Restricciones Operativas
- **⛔ Regla de Oro (AGENTS.md):** Antigravity NUNCA debe ejecutar compilaciones (`cmake --build`, `build.bat`, MSBuild) ni lanzar ejecutables de tests (`ABDAudioLab_Tests.exe`) desde la terminal. El usuario compila y ejecuta en su terminal interactiva.
- **Inmutabilidad de Versión:** Base tag `v2.1.1-build570-stepper-buttontokens-rc1` y Build 570 congelados en `src/BuildVersion.h`.
- **Git Hygiene:** No comitear archivos borradores de Fase 4 (`CurveReportExporter.*`, `LabEnvelopeCurveAnalyzer.*`).

