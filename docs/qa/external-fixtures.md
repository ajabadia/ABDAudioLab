# Directriz de Gobernanza de Fixtures Externas y Artefactos de Prueba
## ABDAudioLab QA Framework — Catálogo de Fixtures, Procedencia y Políticas de Ejecución

**Fecha:** 6 de Octubre de 2026
**Documento:** `docs/qa/external-fixtures.md`
**Fase:** R3 — Post-Release Stabilization & Backlog Triage (`v2.1.1`)
**Estado:** 🟢 **NORMATIVA ACTIVA Y AUDITADA**

---

## 1. Propósito y Taxonomía de Fixtures

Este documento establece la política oficial de gobernanza para todas las fixtures de prueba (plugins VST3 de terceros, archivos JSON de evaluación predefinida, snapshots de interfaz de usuario y modelos generados sintéticamente) utilizadas en la suite de pruebas automatizadas Catch2 y en los procedimientos de verificación de integridad de release de **ABDAudioLab**.

El marco distingue formalmente entre:
1. **Fallo de código / Regresión (`FAIL`):** Incumplimiento de una aserción, discrepancia de hash SHA-256 o ausencia de un componente obligatorio.
2. **Omisión condicionada por entorno (`SKIPPED`):** Ausencia documentada de una herramienta o plugin de terceros en un entorno limpio de integración continua o máquina de compilación local.
3. **Prohibición de Falso PASS:** Un test condicionado por una dependencia externa ausente **nunca debe retornar PASS**. Debe invocar la macro `SKIP()` de Catch2 con un mensaje explicativo explícito.

---

## 2. Clasificación Normativa y Comportamiento ante Fallos

| Categoría | Definición | Requisito en Build / CI | Comportamiento en Suite | Comportamiento en Verificador de Integridad |
|---|---|---|---|---|
| **1. Fixture Interna Crítica** | Embebida en código C++ (`SyntheticFixture`, generador armónico, LUT 2D sintético). | **Obligatoria**. Compilada y residente en memoria. | **PASS Obligatorio**. Un fallo detiene el pipeline (`FAIL`). | N/A (Embebida en binario). |
| **2. Fixture de Datos Versionada** | Archivos estáticos en repositorio (`fixtures/evaluations/*.json`, snapshots de UI). | **Obligatoria**. Versionada en git y ratificada por SHA-256. | **PASS Obligatorio**. Si falta o cambia el hash -> `FAIL`. | **[FAIL] Inmediato** si el hash o tamaño difiere. |
| **3. Fixture Externa Opcional** | Plugins de terceros (.vst3) o emuladores de hardware externo (ej. Dexed, VES). | **Opcional**. No se exige preinstalación en entornos estándar. | **SKIPPED Documentado**. La prueba invoca `SKIP()` con motivo visible. | **[SKIP] Controlado** si el manifiesto la marca como `"optional": true`. |
| **4. Fixture con Drift / Sabotaje** | Archivo o plugin presente cuyo contenido, tamaño o hash SHA-256 difiere del esperado. | Requiere auditoría de compatibilidad o rechazo de seguridad. | **FAIL Obligatorio** (detectado por auditoría de integridad criptográfica). | **[FAIL] Inmediato** con reporte detallado de `expected` vs `actual`. |

---

## 3. Catálogo e Inventario Detallado de Fixtures

### 3.1. Fixtures Internas (En Memoria / Obligatorias)

#### `SyntheticFixture`
- **Nombre y Propósito:** Sintetizador analógico virtual en memoria para pruebas de ciclo cerrado y calibración metrológica.
- **Proveedor / Origen:** Código fuente nativo de ABDAudioLab (`src/synth/`, `src/tests/`).
- **Versión:** Interna (sincronizada con el commit actual).
- **Fecha de Incorporación:** Fase 1 (Diseño inicial de arquitectura DSP).
- **Ruta:** Embebida en `ABDAudioLab_Tests.exe`.
- **Tipo de Contenido:** Algoritmos C++ y buffers PCM generados deterministamente.
- **Tests que la Consumen:** `test_SoundIdViews.cpp`, `test_Step3Step4Transitions.cpp`, `test_SoundIdWorkflow.cpp`, suites de `ABDSharedCode`.
- **Acción si Falta / Falla:** `FAIL` crítico inmediato.

---

### 3.2. Fixtures de Evaluación Predefinidas (Archivos JSON Versionados / Obligatorias)

Ubicadas en `fixtures/evaluations/`. Utilizadas para validar la proyección visual en Step 4 (`SoundIdResultsSummaryView`), el bloqueo estricto de exportación y la auditoría de integridad.

| Archivo | Propósito | Proveedor / Origen | Versión / Fecha | SHA-256 Canónico | Tamaño (Bytes) | Acción si Falta / Difiere |
|---|---|---|---|---|---|---|
| **`fixture_approved.json`** | Evaluación aprobada con modelo `LUT_SIMD_2D` y métricas óptimas. | ABDAudioLab QA Pipeline | v1.0 (2026-09-15) | `FE3635ACFCC04E041C621BEBEDEFAA0463FEFBAC8668FCB63476207A7D288F38` | 1.157 | **FAIL Inmediato** |
| **`dexed_warnings.json`** | Evaluación de Dexed VST3 con advertencias de settling prolongado y reset necesario. | ABDAudioLab QA Pipeline | v1.0 (2026-09-15) | `BFD1771FE08048A8AC97A5601A52E8A8013FD8B2B5D45F1FE6C593ACABEA4001` | 1.961 | **FAIL Inmediato** |
| **`inconclusive.json`** | Evaluación con datos insuficientes; bloquea la exportación de C++. | ABDAudioLab QA Pipeline | v1.0 (2026-09-15) | `C824EFE381092C920B77291C18298607AC405F15770392887B665293E0DFC185` | 1.170 | **FAIL Inmediato** |
| **`rejected.json`** | Evaluación con ESR inaceptable; rechazo de modelo y bloqueo de exportación. | ABDAudioLab QA Pipeline | v1.0 (2026-09-15) | `F603C7472F7E792E810F41344B2A41E067853F49723CDAF30546D960B26FF8E0` | 1.274 | **FAIL Inmediato** |
| **`tampered_hash_mismatch.json`** | Fixture de control con hash canónico manipulado deliberadamente. | ABDAudioLab Security QA | v1.0 (2026-09-15) | `E0697FCB2551F573855896381602E9DC6959D8D54F5CD8FD435889C5F64899AC` | 1.156 | **FAIL Inmediato** |

- **Licencia / Restricciones:** Propiedad del proyecto ABDAudioLab. Licencia interna / abierta del repositorio.
- **Tipo de Contenido:** JSON estructurado según esquema `ModelEvaluationSummary` y `ValidationUiSummary`.
- **Tests que las Consumen:** `test_SoundIdViews.cpp` (Tests de carga externa y discrepancia de hash), `test_SmokeStep4UI.cpp`.

---

### 3.3. Fixtures Externas de Terceros (Binarios VST3 / Opcionales)

#### `Dexed.vst3`
- **Nombre y Propósito:** Sintetizador FM de 6 operadores multiplataforma y emulador bit-accurate de Yamaha DX7. Se utiliza como DUT (Device Under Test) real para evaluar introspección VST3, captura SysEx/MIDI y modelado automatizado.
- **Proveedor / Origen:** Digital Suburban / Pascal Gauthier ([asb2m10/dexed](https://github.com/asb2m10/dexed)).
- **Versión de Referencia de Laboratorio:** `0.9.6` o superior (64-bit VST3).
- **Licencia:** GPL v3.
- **Ruta Esperada en Windows:** `C:\Program Files\Common Files\VST3\Dexed.vst3` (Fallback: `%LOCALAPPDATA%\Programs\Common\VST3\Dexed.vst3`).
- **Tipo de Contenido:** Plugin VST3 binario compilado (bundle con `moduleinfo.json` y binarios DLL/x86_64).
- **Tests que lo Consumen:**
  - `test_DexedValidation.cpp`
  - `test_GuidedPluginLoad_Dexed.cpp`
  - `test_DexedEnvelopeMeasurement_T5.cpp`
  - `test_Phase20_11_VerticalDexedAndFair_T5.cpp`
  - `test_Vst3DexedRealHosting_T2.cpp`
  - Total: 19 casos de prueba etiquetados con `[target_profile][external][dexed]`.
- **Acción si Falta:** `SKIPPED` legítimo documentado mediante `SKIP("External fixture unavailable: Dexed.vst3 not found at configured path")`. **Bajo ninguna circunstancia emitirá PASS falso ni provocará fallo del build.**
- **Acción si está Presente:** Ejecución completa con validación de hash de contrato de parámetros.

#### `VES (Vintage Emulator Studio)`
- **Nombre y Propósito:** Entorno de emulación de sintetizadores vintage para validación de latencia y no-linealidades complejas.
- **Proveedor / Origen:** Entorno propietario de laboratorio / emulación de hardware.
- **Versión de Referencia:** v1.2.
- **Licencia:** Propietaria de laboratorio.
- **Ruta Esperada:** Ruta configurada por variable de entorno o registro de sistema.
- **Tipo de Contenido:** Emulador de plugin / interfaz IPC.
- **Tests que lo Consumen:** 8 casos de prueba etiquetados con `~[ves]`.
- **Acción si Falta:** `SKIPPED` legítimo (`~[ves]`).

---

## 4. Política de Hashes Criptográficos y Prevención de Falsos Positivos

1. **Hashes Verificados, No Teóricos:**
   - La suite y el verificador de release nunca inventarán ni hardcodearán hashes de plugins externos no instalados formalmente en una estación de calibración.
   - Las fixtures versionadas en el repositorio (`fixtures/evaluations/*.json`) están auditadas con su SHA-256 inmutable exacto.
2. **Tratamiento del Estado `SKIPPED` en la Suite Canónica (945 Test Cases):**
   - En una máquina sin `Dexed.vst3` ni `VES`:
     ```text
     test cases:    945 |    918 passed | 27 skipped
     assertions: 211036 | 211036 passed |  0 skipped
     fallos:          0
     ```
   - Los **27 `SKIPPED`** representan exactamente las pruebas dependientes de plugins de terceros ausentes.
   - Si en el futuro se añaden pruebas condicionales y el número asciende a 36 `SKIPPED`, cada una de las omisiones debe estar tipificada en este catálogo.
3. **Regeneración de Hashes del Manifiesto:**
   - El script `tools/verify-release-hashes.ps1` opera en modo **SOLO LECTURA** de manera predeterminada.
   - La actualización de hashes del manifiesto (`-UpdateManifest`) es una operación reservada para el Lead de Release y requiere revisión minuciosa del `git diff` antes de su commit.

---

## 5. Procedimiento de Verificación en el Proceso de Release

Para certificar la integridad de una distribución o build de release antes de empaquetar:

```powershell
# 1. Ejecutar verificación de integridad de artefactos y fixtures
powershell -File .\tools\verify-release-hashes.ps1

# 2. Comprobar código de retorno
if ($LASTEXITCODE -ne 0) {
    Write-Error "La verificación de integridad de release ha fallado. Detener pipeline."
}
```

- **Exit Code 0:** Todos los artefactos obligatorios existen y su SHA-256 coincide exactamente.
- **Exit Code 1:** Discrepancia de hash, fichero corrupto o ausencia de fixture obligatoria.
- **Exit Code 2:** Error sintáctico en el manifiesto o argumentos inválidos.
