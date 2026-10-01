# ACTA FINAL DE CIERRE Y CERTIFICACIÓN — HITO-AUDIO-AB-5D
## Acceptance Matrix, Canonical Preset Validation and Dedicated CI Baseline

**Fecha de cierre local:** 2026-09-29  
**Fecha de certificación CI remota:** pendiente — Run #8 en preparación (ver §7)  
**Hito Global:** HITO-AUDIO-AB-5D (Fases 5D.1 a 5D.9)  
**Documento Rector:** `docs/audits/ACTA_HITO_AUDIO_AB_5D.md`  
**Compilador:** MSVC 18.4.3 (Visual Studio 2026 Community) · Release x64  
**Build Identity:** `Release x64 - MSVC 18.4.3 - Build #514`  
**Policy de Tolerancias:** `audio-ab-5d-provisional-v1` (`sha256:7f45cbb662b66299b9cf2a70d9a6c924cfdd62479e0a0d6ee0bf0b1f83424d57`)  
**Baseline Canónica:** `audio-ab-5d-canonical-v1`  
**Estado local:** 🟢 **CERRADO Y SELLADO LOCALMENTE (886 PASS / 0 FAIL)**  
**Estado CI remota:** ⏳ **CERTIFICACIÓN PENDIENTE — Run #8 en preparación con SSOT inmutable (`9a99cbb`)**

---

## 1. Declaración Formal de Alcance y Cierre de Fases

Se declara formalmente completado y sellado **localmente** el hito **HITO-AUDIO-AB-5D** tras la ejecución y certificación local sucesiva de sus nueve tareas atómicas.

La certificación CI remota permanece pendiente del resultado exitoso del Run #6, conforme a los criterios definidos en la sección 5.

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
| **5D.9** | CI dedicado y congelación de baseline | `.github/workflows/audio-ab-5d-ci.yml` + manifest | 🟢 **CERTIFICADO LOCALMENTE** / ⏳ **CI REMOTA EN CURSO** |

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

**Fecha tag local:** 2026-09-29 18:49 CEST  
**Fecha fix Gate 2:** 2026-09-29 19:16 CEST  
**Estado CI remota:** ⏳ **PENDIENTE** — Run #6 en curso

| Elemento | Valor |
|---|---|
| **Tag cierre local** | `hito-audio-ab-5d-certified` → `23a5d20` |
| **Fix Gate 2 (POST-5D.4)** | `50900f6` — `ci(qa): fix Gate 2 -- add --allow-running-no-tests` |
| **Acta incidente** | `b4c3f6e` — `docs(acta): record Gate 2 CI incident` |
| **SHA ABDSharedCode pinnado** | `a0cdaf1` — `feat(hwid): integrate strict endpoint safety (HITO-SHARED-SYNC)` |
| **Workflow** | `.github/workflows/audio-ab-5d-ci.yml` — 6 gates configurados |
| **Repositorio** | `https://github.com/ajabadia/ABDAudioLab` |

> [!IMPORTANT]
> **Desfase tag / CI:** El tag `hito-audio-ab-5d-certified` apunta a `23a5d20`, anterior al fix `50900f6`.  
> Si Run #6 termina verde, la evidencia CI corresponde al commit `50900f6`, no al commit etiquetado.  
> Se creará un tag adicional `hito-audio-ab-5d-certified-ci` apuntando al commit exactamente validado por CI.  
> El tag original **no se mueve** — documenta el cierre local, no el cierre CI.

---

### Historial de runs de CI

| Run | SHA | Resultado | Causa |
|---|---|---|---|
| #1 | `d588fca` | ❌ failure | Generator MSVC no detectado |
| #2 | `dfc2c67` | ❌ failure | WebView2 no instalado |
| #3 | `8f63869` | ❌ failure | NuGet URL inválida |
| #4 | `004bc1b` | ❌ failure | Checkout multi-repo fallido |
| #5 | `657052a` | ❌ failure | Gate 2: `[diagnostics]` → 0 tests → exit code 1 |
| #6 | `50900f6` | ❌ failure | Gate 6: fallo en baseline `~[ves]` por dependencias de rutas absolutas codificadas (`D:/desarrollos/...`) |
| #7 | `6e8484c` | ❌ failure | Gates 1–5 verdes; Gate 6 falló en contratos/cuarentena por checkout remoto de `ABDSharedAssets` desfasado respecto a la verdad local (resuelto en POST-5D.6) |
| #8 | *(en preparación)* | ⏳ pendiente | Incidente POST-5D.6: Publicación de SSOT contractual y pin inmutable (`9a99cbb`) |

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

### Condición de cierre remoto

Para declarar **"cerrado y sellado local y remotamente"**:

```text
Run #7:     status=completed / conclusion=success
Gates 1-6:  todos success o skipped-por-diseño (nunca failure; Gate 6 100% verde sin bypass)
Gate 2:     success con ejecución autorizada vacía (documentado arriba)
Tag CI:     hito-audio-ab-5d-certified-ci → SHA validado por Run #7
ACTA:       actualizada con SHA, run_id y fecha de Run #7 exitoso
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
| `~[ves]` desde la raíz del repo | Sin fallos de resolución; falla `INTEGRATION-01` por una carrera preexistente |
| `docs/qa/` (evidencia congelada) | **0 diffs** — inmutable |
| Workflow CI | Gates 1–6, **0 supresores**, sin `continue-on-error` ni `\|\| true` |

### 6.3 Condición de cierre remoto (sin cambios)

La certificación remota de este acta **sigue pendiente** y requiere Run #7 verde en los Gates 1–6.
POST-5D.5 elimina la causa conocida del fallo del Run #6; no lo cierra por sí mismo, porque exige
ejecutar en un clon limpio. Hasta que ese Run exista, el estado remoto sigue siendo
**PENDIENTE** y así debe leerse el encabezado de esta acta.

### 6.4 Deuda que este addendum NO cierra

Registrada aquí para que no se confunda "hermético" con "sin fallos":

- `test_Integration01GuidedVsClassicAudio` compara con igualdad exacta valores que dependen del
  planificador del sistema. Preexistente, ajeno a las rutas, no modificado.
- Una de cada varias corridas completas de `~[ves]` muere en silencio (exit 3, sin resumen). No
  reproduce en aislamiento y sigue sin atribuirse a un test concreto (mitigado con TestTelemetry).
- `contracts/hardware/roland_aira_patch_spec.schema.json`: Causa aclarada y cerrada en POST-5D.6 (esquema canónico de SSOT que faltaba por publicar en el repositorio remoto).

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

---

## 8. Estado Global del Roadmap ([PLAN.md](../../PLAN.md))

```text
HITO-AUDIO-AB-5D:
  🟢 CERRADO LOCALMENTE (5D.1 a 5D.9 + 886 PASS / 0 FAIL).
  🏷️  Tag local: hito-audio-ab-5d-certified → 23a5d20
  ❌ CI Remota: Run #6 FALLÓ en Gate 6 — causa: rutas absolutas (cerrada por POST-5D.5).
  ❌ CI Remota: Run #7 FALLÓ en Gate 6 — causa: drift con SSOT remota desfasada (cerrada por POST-5D.6).
  ⏳ CI Remota: Run #8 PENDIENTE — SSOT fijada de forma inmutable a 9a99cbb.
  🔒 Tag CI:    pendiente resultado Run #8.

POST-5D.5 (hermeticidad de rutas):
  🟢 CERRADO. 0 rutas personales en src/. Guard [hygiene] verificado.

POST-5D.6 (SSOT contractual y pin inmutable):
  🟢 CERRADO. ABDSharedAssets publicado (9a99cbb); 40/40 contratos; pin en CI.

Bloqueados (inviolables):
  - D2.7B: Banco físico metrológico.
  - HITO-10V1 / VES: Boot headless no demostrado.
  - MIDI físico: 0 bytes autorizados.
  - ExportReadiness: Blocked permanente.
```


