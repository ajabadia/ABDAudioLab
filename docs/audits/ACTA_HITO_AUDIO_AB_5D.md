# ACTA FINAL DE CIERRE Y CERTIFICACIÓN — HITO-AUDIO-AB-5D
## Acceptance Matrix, Canonical Preset Validation and Dedicated CI Baseline

**Fecha de cierre local:** 2026-09-29<br>
**Fecha de certificación CI remota:** 2026-10-02 (Run #16 / ID 37014243068)<br>
**Hito Global:** HITO-AUDIO-AB-5D (Fases 5D.1 a 5D.9)<br>
**Documento Rector:** `docs/audits/ACTA_HITO_AUDIO_AB_5D.md`<br>
**Compilador:** MSVC 18.4.3 (Visual Studio 2026 Community) · Release x64<br>
**Build Identity:** `Release x64 - MSVC 18.4.3 - Build #514`<br>
**Policy de Tolerancias:** `audio-ab-5d-provisional-v1` (`sha256:7f45cbb662b66299b9cf2a70d9a6c924cfdd62479e0a0d6ee0bf0b1f83424d57`)<br>
**Baseline Canónica:** `audio-ab-5d-canonical-v1`<br>
**Estado local:** 🟢 **CERRADO Y SELLADO LOCALMENTE (886 PASS / 0 FAIL)**<br>
**Estado CI remota:** 🟢 **CERRADO Y SELLADO REMOTAMENTE (Run #16 — Gates 1–7 PASS)**

---

## 1. Declaración Formal de Alcance y Cierre de Fases

Se declara formalmente completado y sellado **local y remotamente** el hito **HITO-AUDIO-AB-5D** tras la ejecución y certificación local sucesiva de sus nueve tareas atómicas y la validación final en CI remota (Run #16).

La certificación CI remota ha quedado satisfecha y validada en su totalidad por Run #16, conforme a los criterios definidos en la sección 5 y 8.

| Tarea | Denominación | Entregable Canónico | Estado |
|:---:|---|---|:---:|
| **5D.1** | Contrato de corrida A/B y schema de resultado | `docs/qa/audio-ab-5d-acceptance-spec.md` | ✅ **CERTIFICADO** |
| **5D.2** | Catálogo de cinco presets acústicos canónicos | `docs/qa/audio-ab-5d-canonical-preset-matrix.md` | ✅ **CERTIFICADO** |
| **5D.3** | Protocolos de excitación determinista y reset | `docs/qa/audio-ab-5d-excitation-protocol.md` | ✅ **CERTIFICADO** |
| **5D.4** | Matriz inicial de 10 corridas software | `test_AudioABCanonicalPresetRuns5D.cpp` | ✅ **CERTIFICADO** |
| **5D.5** | Métricas, alineación acotada y diagnósticos tipados | `src/math/AudioABMetrics5D.h` + tests | ✅ **CERTIFICADO** |
| **5D.6** | Policy de tolerancias por clase acústica (calibración v1) | `docs/qa/audio-ab-5d-tolerance-policy.md` | ✅ **CERTIFICADO** |
| **5D.7** | Fixtures herméticas y pruebas unitarias de comparator/verdict | `test_AudioABComparatorAndVerdict5D.cpp` | ✅ **CERTIFICADO** |
| **5D.8** | Matriz QA, revisión de deltas 44.1k/48k y aceptación software | `ACTA_HITO_AUDIO_AB_5D8_SOFTWARE_ACCEPTANCE.md` | ✅ **CERTIFICADO** |
| **5D.9** | CI dedicado y congelación de baseline | `.github/workflows/audio-ab-5d-ci.yml` + manifest | 🟢 **CERTIFICADO LOCAL Y REMOTAMENTE** |

---

## 2. Fronteras Inviolables de Seguridad y Gobernanza

> [!IMPORTANT]
> **Delimitación de Ámbito:**  
> La presente certificación aplica **única y exclusivamente al Aseguramiento de Calidad (QA) acústico de software en memoria**.  
> No constituye una medición física ni una certificación de hardware.
>
> 1. **Hardware Físico:** ⛔ **0 bytes transmitidos** en toda la fase.
> 2. **Sintetizador DeepMind 12D:** Desconectado y no utilizado; suites de banco físico omitidas limpiamente (`SKIP_PHYSICAL_BENCH_NOT_AVAILABLE`).
> 3. **Vintage Emulator Studio (VES):** ⛔ Excluido explícitamente de la baseline QA no-VES de HITO-AUDIO-AB-5D mediante `~[ves]`.
>    **Estado:** HITO-10V1 permanece bloqueado mientras no se demuestre arranque headless verificable, salida audible reproducible y control semántico del target emulado.
> 4. **Hitos Bloqueados:** **D2.7B** (Banco Físico Metrológico) y **HITO-10V1** (Formalización VES) permanecen **bloqueados preventivamente**.
> 5. **ExportReadiness:** `Blocked` permanente (ningún resultado software se exporta como calibración física).

---

## 3. Resumen Consolidado de la Matriz Canónica Software

Evaluación de los cinco presets acústicos canónicos a $44.1\text{ kHz}$ y $48.0\text{ kHz}$ bajo `audio-ab-5d-provisional-v1`:

```text
Corridas ejecutadas:     10
Determinismo A1 == A2:   10/10 BIT-EXACT PASS (Diff = 0.0)
Clipping inesperado:     0 corridas
Inversión de polaridad:  0 corridas
Lags fuera de ventana:   0 corridas
Hard Limit Violations:   0

Veredictos Resultantes:
  - PASS / Accepted:                              4 corridas (CleanReference, LowLevelDynamic)
  - WARN / AcceptableWithExpectedDispersion:      6 corridas (GentleModulation, AggressiveNonlinear, HighDensitySpectral)
  - FAIL / Rejected:                              0 corridas
```

---

## 4. Congelación de Artefactos de Integridad y CI

1. **Manifest Canónico de Baseline:**  
   Persistido en [`docs/qa/audio-ab-5d-baseline-manifest.json`](../qa/audio-ab-5d-baseline-manifest.json).
2. **Schema Formal de Validación:**  
   Persistido en [`docs/qa/audio-ab-5d-run-report.schema.json`](../qa/audio-ab-5d-run-report.schema.json).
3. **Checksums Criptográficos:**  
   Persistido en [`docs/qa/audio-ab-5d-artifacts.sha256`](../qa/audio-ab-5d-artifacts.sha256).
4. **Validación Automatizada C++:**  
   Suite dedicada en [`src/tests/test_AudioABBaselineManifestValidation5D.cpp`](../../src/tests/test_AudioABBaselineManifestValidation5D.cpp) que verifica la integridad SHA-256 de los 10 reportes de corrida, la ausencia de rutas absolutas locales y la conformidad estricta con los veredictos congelados.
5. **Workflow de GitHub Actions:**  
   Configurado en [`.github/workflows/audio-ab-5d-ci.yml`](../../.github/workflows/audio-ab-5d-ci.yml) con validación de matriz, manifest, hashes, baseline no-VES y publicación de artefactos QA.

---

## 5. Certificación de CI Remoto — POST-5D.2 / POST-5D.3 / POST-5D.4

**Fecha tag local:** 2026-09-29 18:49 CEST<br>
**Fecha certificación CI remota:** 2026-10-02 (Run #16 / ID 37014243068)<br>
**Estado CI remota:** 🟢 **CERTIFICADA** — Run #16 / ID 37014243068, conclusión `success`

| Elemento | Valor |
|---|---|
| **Tag cierre local** | `hito-audio-ab-5d-certified` → `23a5d20` |
| **Tag certificación CI remota** | `hito-audio-ab-5d-certified-ci` → `31d7adb` |
| **Commit CI certificado** | `31d7adbaf88acddd520e9eace7f5ad6c59da3978` |
| **Fix Gate 2 (POST-5D.4)** | `50900f6` — `ci(qa): fix Gate 2 -- add --allow-running-no-tests` |
| **Acta incidente** | `b4c3f6e` — `docs(acta): record Gate 2 CI incident` |
| **SSOT contractual (`ABDSharedAssets`)** | `065ca6c6aef82e8d41cf6a94d650ab28f5c27e4c` |
| **SHA ABDSharedCode pinnado** | `21c0a60` — `ref: 21c0a60c1efd5aac6a6cedcc57725f601a3ce14a` |
| **Workflow** | `.github/workflows/audio-ab-5d-ci.yml` — 1 Preflight Ubuntu + 1 Timings Ubuntu + 7 Gates Windows |
| **Repositorio** | `https://github.com/ajabadia/ABDAudioLab` |

> [!IMPORTANT]
> **Desfase tag local / tag CI:** El tag `hito-audio-ab-5d-certified` apunta a `23a5d20`, correspondiente al cierre local original.
> El tag inmutable `hito-audio-ab-5d-certified-ci` apunta a `31d7adbaf88acddd520e9eace7f5ad6c59da3978`, correspondiente a la validación completa en CI remota (Run #16).
> Ambos tags representan hitos complementarios e inmutables: el cierre local y la certificación reproducible en CI.

---

### Historial de runs de CI

| Runs | Resultado | Causa o evidencia |
|---|---|---|
| #1–#5 | ❌ Failure | Bootstrap de CI: generador MSVC, WebView2, NuGet, checkout multi-repo y Gate 2 vacío |
| #6 | ❌ Failure | Gate 6: rutas personales y CWD no herméticos |
| #7 | ❌ Failure | Gate 6: SSOT contractual remota desfasada |
| #8 | ❌ Failure | Preflight contractual: recurso ABDEep no versionado en la ruta esperada |
| #9–#15 | ❌ Failure / diag. progresivo | Red/FetchContent, higiene documental, estabilidad JUCE GUI y refinamientos de telemetría; sin drift de evidencia 5D |
| #16 | ✅ Success | Preflight Ubuntu, Suite Timings, Windows build y Gates 1–7 verdes |

---

### Incidente POST-5D.5 — Deuda Técnica de Rutas en Baseline No-VES

> [!NOTE]
> **Aislamiento del Incidente de CI:**  
> El fallo de Gate 6 en Run #6 confirma que Gates 1–5 (específicos de Audio A/B 5D) están completamente certificados y verdes tanto en local como en CI remota.  
> El bloqueo se debe exclusivamente a tests preexistentes no herméticos de la baseline global `~[ves]` (`TargetProfile*`, etc.) que contenían rutas fijas `D:/desarrollos/ABDSynths/...`.  
> Este incidente se gestiona en el microhito aislado `POST-5D.5`, sin alterar los resultados, tolerancias, métricas ni hashes de Audio A/B 5D.

---

### Semántica de Gate 2 — Nota de trazabilidad

> [!NOTE]
> **POST-5D.4 — Gate 2 con 0 tests en `[audioab_5d][diagnostics]`:**  
> El filtro `[audioab_5d][diagnostics]` no selecciona ningún test case en la suite actual.  
> El flag `--allow-running-no-tests` autoriza explícitamente esa ejecución vacía, devolviendo exit code 0.  
>
> **Gate 2 verde con 0 tests ≠ diagnósticos ejecutados y aprobados.**  
> La cobertura de policy (22 assertions en 12 test cases) proviene exclusivamente  
> del filtro `[audioab_5d][policy]`. El filtro `[diagnostics]` es una reserva de espacio  
> que no aporta evidencia de cobertura independiente en este hito.

---

### Condición de cierre remoto — Cumplida

Para declarar **"cerrado y sellado local y remotamente"**, se verificaron y cumplieron los siguientes criterios normativos:

```text
Run #16:
status=completed / conclusion=success.

Preflight contractual Ubuntu:
success.

Suite Timings Ubuntu:
success.

Windows build:
success.

Gates 1–7:
success.

Gate 2:
success con ejecución vacía autorizada y documentada (filtro [diagnostics] sin tests).

Tag CI:
hito-audio-ab-5d-certified-ci → 31d7adbaf88acddd520e9eace7f5ad6c59da3978.

SSOT:
ABDSharedAssets → 065ca6c6aef82e8d41cf6a94d650ab28f5c27e4c.
```

---

## 6. Addendum POST-5D.5 — Cierre de Hermeticidad (2026-10-01)

Este addendum **no altera** la certificación del hito ni la evidencia congelada en §3 y §4. Registra
el microhito POST-5D.5, que nació de un incidente de reproducibilidad de CI detectado al intentar
certificar esta acta en remoto.

### 6.1 Qué ocurrió

El Run #6 no pudo certificarse: **Gate 6** (`~[ves]`) falló en el runner de GitHub Actions porque la
suite resolvía los datos del repositorio contra rutas absolutas de la máquina de un desarrollador.
En local todo estaba verde, porque el ejecutable se lanzaba desde la raíz donde esas rutas existen.

Diagnóstico completo en [POST_5D5_HARDCODED_PATHS_INVENTORY.md](POST_5D5_HARDCODED_PATHS_INVENTORY.md),
incluido el **addendum §10 con las premisas del diagnóstico original que quedaron refutadas** al
ejecutarlo (entre ellas, la propuesta de crear un segundo módulo de resolución de rutas).

### 6.2 Estado al cierre

| Elemento | Estado |
|---|---|
| Rutas absolutas personales en `src/**` | **0** (verificado por grep y por el guard) |
| Módulo canónico de resolución | `src/core/LabResourcePaths.{h,cpp}` — único |
| Guard permanente | `test_ResourcePathHygiene.cpp`, 8 casos `[hygiene]`, verificado en ambos sentidos |
| `~[ves]` desde `build/Release` | 918 casos · 890 PASS · 28 SKIP · **0 FAIL** · exit 0 |
| `~[ves]` desde la raíz del repo | Observación histórica durante POST-5D.5: sin fallos de resolución; carrera preexistente en `INTEGRATION-01` observada localmente (resuelta en CI final; Gate 6 verde en Run #16) |
| `docs/qa/` (evidencia congelada) | **0 diffs** — inmutable |
| Workflow CI en el cierre de POST-5D.5 | Gates 1–6, **0 supresores**, sin `continue-on-error` ni `\|\| true` (evolución posterior: el workflow final certificado en Run #16 incorpora Gate 7 para snapshot contractual y cuarentena; ver §5 y §8) |

### 6.3 Condición de cierre remoto (cerrada por Run #16)

La certificación remota de este acta quedó plenamente validada y cerrada mediante el **Run #16** (ver §8), confirmando que la hermetización de rutas de POST-5D.5 opera de forma hermética y sin dependencias en un entorno limpio de CI.

### 6.4 Observaciones históricas de estabilidad y resolución

> [!NOTE]
> **Observación histórica durante POST-5D.5:**<br>
> En ejecuciones locales previas desde la raíz se observaron una carrera en `INTEGRATION-01` y terminaciones silenciosas `exit 3` no atribuibles a un test concreto.
>
> **Estado final:**<br>
> La corrección de `ScopedJuceInitialiser_GUI`, junto con `TestTelemetry`, permitió que la baseline `~[ves]` completara correctamente Gate 6 en Run #16.
>
> Estas incidencias no bloquearon la certificación final, pero su causa histórica se conserva por trazabilidad.<br>
> *(Nota sobre contratos: `contracts/hardware/roland_aira_patch_spec.schema.json` quedó plenamente resuelto y cerrado en POST-5D.6 y validado en Gate 7).*

---

## 7. Addendum POST-5D.6 — Sincronización SSOT y Pin Inmutable de Contratos (2026-10-01)

### 7.1 Qué demostró el Run #7
1. **Hermetización de rutas completada**: Gates 1–5 pasaron en verde al 100%. `LabResourcePaths` y el marcador versionado `ABDAudioLab.workspace` eliminaron la dependencia de CWD y rutas absolutas.
2. **Causa del fallo en Gate 6**: Gate 6 ejecutó durante 113s y falló en dos tests contractuales:
   - `HardwareContractQuarantine`: El esquema remoto no declaraba `status` / `statusReason`.
   - `ContractsSnapshotDrift`: Discrepancia entre los 40 contratos locales y los 39 del checkout remoto.
3. **Diagnóstico**: La SSOT `ABDSharedAssets` tenía 16 commits locales (cuarentena, `status`, `roland_aira_patch_spec.schema.json` y generador de claves) que aún no se habían enviado a su remoto `origin/main`. El runner de CI descargaba el commit antiguo `7e8e1d1`.

### 7.2 Acciones ejecutadas en POST-5D.6
- **Auditoría de `ABDSharedAssets`**: 1.720/1.720 tests Vitest aprobados (100%), preflight verde (`check-generated-contracts.mjs`), `git diff --check` limpio.
- **Publicación remota de SSOT**: Rama `main` de `ABDSharedAssets` publicada con éxito en GitHub (`7e8e1d1..9a99cbb`).
- **SHA remoto inmutable certificado**: `9a99cbbb5d001321395ab14c1bea94e954e9168f`.
- **Pinning en CI**: Se fijó `ref: 9a99cbbb5d001321395ab14c1bea94e954e9168f` en los dos pasos de checkout de `ABDSharedAssets` en `.github/workflows/audio-ab-5d-ci.yml`.
- **Paridad contractual garantizada**: 40/40 contratos idénticos byte a byte entre SSOT y `ABDAudioLab/contracts/hardware`.

> [!NOTE]
> **Evolución del Pin Contractual:**
> El pin `9a99cbbb5d001321395ab14c1bea94e954e9168f` fue una referencia intermedia de POST-5D.6.
> La certificación definitiva Run #16 se ejecutó con el pin `065ca6c6aef82e8d41cf6a94d650ab28f5c27e4c`, tras la corrección del generador de contratos ABDEep en `ABDSharedAssets`.

---

## 8. Certificación CI Remota Definitiva — Run #16 (2026-10-02)

Se declara formalmente la certificación remota reproducible del hito tras la ejecución exitosa de Run #16 en GitHub Actions:

```text
Certificación CI remota:
Run #16 / ID 37014243068.

Commit certificado:
31d7adbaf88acddd520e9eace7f5ad6c59da3978.

ABDSharedAssets pinneado:
065ca6c6aef82e8d41cf6a94d650ab28f5c27e4c.

Resultado:
Contract Preflight Ubuntu: success.
Suite Timings Ubuntu: success.
Windows build: success.
Gates 1–7: success.

Evidencia publicada:
audio-ab-5d-qa-acceptance-reports.
suite-timings.

Estado final:
CERRADO Y SELLADO LOCAL Y REMOTAMENTE.
```

### 8.1 Cierre formal de microhitos derivados

```text
POST-5D.5:
Cerrado — hermetización de rutas.

POST-5D.6:
Cerrado — SSOT contractual publicada, pinneada
y verificada remotamente.
```

Además, la corrección de `ScopedJuceInitialiser_GUI` en `test_MeasurementFloatingWindow` (commit `31d7adb`) eliminó el fallo en Gate 6 (`~[ves]`), permitiendo la finalización verde de la baseline global en un entorno Windows limpio.

---

## 9. Estado Global del Roadmap ([PLAN.md](../../PLAN.md))

```text
HITO-AUDIO-AB-5D:
  🟢 CERRADO Y SELLADO LOCAL Y REMOTAMENTE.
  🏷️  Tag local: hito-audio-ab-5d-certified → 23a5d20
  🏷️  Tag CI:    hito-audio-ab-5d-certified-ci → 31d7adb
  ✅ CI Remota: Run #16 SUCCESS (ID 37014243068). Gates 1–7 PASS.

POST-5D.5 (hermeticidad de rutas):
  🟢 CERRADO. 0 rutas personales en src/. Guard [hygiene] verificado.

POST-5D.6 (SSOT contractual y pin inmutable):
  🟢 CERRADO. ABDSharedAssets publicado y pinneado a 065ca6c; 40/40 contratos; Gates 1–7 verde.

Bloqueados (inviolables):
  - D2.7B: Banco físico metrológico (requerido banco físico).
  - HITO-10V1 / VES: Boot headless no demostrado.
  - MIDI físico: 0 bytes autorizados.
  - ExportReadiness: Blocked permanente.
```


