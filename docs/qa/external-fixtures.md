# Directriz de Gobernanza de Fixtures Externas
## ABDAudioLab QA Framework — Fixtures de Prueba & Políticas de Ejecución

**Fecha:** 4 de Octubre de 2026  
**Documento:** `docs/qa/external-fixtures.md`  
**Fase:** R3 — Post-Release Stabilization & Backlog Triage (`v2.1.1`)  
**Estado:** 🟢 **NORMATIVA ACTIVA**  

---

## 1. Propósito y Taxonomía de Fixtures

Este documento establece la política oficial sobre fixtures de prueba (plugins VST3 e instrumentos emulados) en la suite de pruebas automatizadas Catch2 y en la telemetría de CI.

El objetivo es garantizar que la ausencia de plugins de terceros en un entorno limpio de compilación **no sea interpretada como un fallo del producto**, manteniendo una distinción inequívoca entre *fallo de código* (`FAIL`) y *omisión condicionada por entorno* (`SKIPPED`).

---

## 2. Clasificación Normativa de Fixtures

| Categoría | Definición | Requisito en Build/CI | Comportamiento en Suite |
|---|---|---|---|
| **1. Fixture Interna (Obligatoria)** | Componentes generados sintéticamente en memoria (`SyntheticFixture`, modelos LUT internos, generador de impulsos). | **Obligatoria**. Compilada y embebida en el proyecto. | **PASS Obligatorio**. Un fallo en esta categoría es un fallo bloqueante (`FAIL`). |
| **2. Fixture Externa (Opcional)** | Plugins de terceros independientes distribuidos en formato binario (.vst3) o ROMs propietarias (ej. Dexed VST3, VES). | **Opcional**. No se exige su preinstalación en la máquina de desarrollo o CI básica. | **SKIPPED Legítimo** si el binario no se encuentra en las rutas estándar. |
| **3. Fixture Ausente** | Plugin externo no detectado en el sistema durante la fase de descubrimiento. | Permitida en entornos estándar. | La prueba declara explícitamente la ausencia y retorna `SKIPPED` sin degradar el exit code de la suite. |
| **4. Fixture con Drift** | Plugin externo instalado cuyo hash SHA-256 o versión difiere de la referencia probada históricamente. | Requiere auditoría de compatibilidad. | Debe informar advertencia (*drift warning*) y documentar discrepancias sin quebrar la suite base. |

---

## 3. Matriz de Fixtures en la Suite de Referencia (949 Test Cases)

En la suite global de referencia (**949 test cases**), el desglose normativo es:

```text
===============================================================================
test cases:    949 |    922 passed | 27 skipped
assertions: 211374 | 211374 passed |  0 skipped
fallos:          0
```

| Fixture | Tipo | Tests Afectados | Estado en Entorno Base | Criterio de Aceptación |
|---|---|:---:|---|---|
| **SyntheticFixture** | Interna | 922 casos | Presente en memoria | 100% PASS (211.374 assertions) |
| **Dexed.vst3** | Externa | 19 casos | Ausente (no instalado) | `SKIPPED` legítimo declarado |
| **VES (Vintage Emulator Studio)** | Externa | 8 casos | Ausente (no instalado) | `SKIPPED` legítimo (`~[ves]`) |

> [!NOTE]
> Los **27 SKIPPED** son legítimos, esperados y certificados. No deben ser reportados como tests ejecutados ni sumados artificialmente a los tests aprobados.

---

## 4. Política sobre Hashes Criptográficos de Plugins Externos

1. **Prohibición de Hashes Teóricos:**
   - La base de código y la documentación **no inventarán ni declararán hashes SHA-256 de referencia para Dexed o VES** mientras no se realice una instalación formalizada bajo un entorno controlado de metrología.
2. **Registro de Procedencia:**
   - Cuando una fixture externa se instale formalmente en una estación de laboratorio, se registrará en un acta técnica con:
     - Versión exacta del instalador / release tag upstream.
     - SHA-256 del binario `.vst3`.
     - Sistema operativo y toolchain de origen.
     - Fecha de verificación.
3. **Resiliencia ante Drift:**
   - Un cambio de versión menor en Dexed upstream no debe romper las pruebas si la interfaz VST3 y los parámetros MIDI responden conforme al contrato canónico.

---

## 5. Resumen de Reglas para Desarrolladores y Agentes

1. **Nunca forzar la instalación de plugins externos en el ciclo estándar:** La compilación y ejecución de `build.bat` y `build.bat tests` debe ser 100% autónoma.
2. **Nunca convertir un `SKIPPED` legítimo en un fallo:** Si un test de Dexed o VES detecta que el plugin no está presente, debe utilizar la macro `SKIP()` de Catch2 con un mensaje informativo claro.
3. **No alterar el número de tests base:** La suite de referencia cuenta con 922 tests internos aprobados. Cualquier descenso es una regresión inaceptable.
