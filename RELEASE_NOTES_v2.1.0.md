# ABDAudioLab v2.1.0 — Release Notes

**Versión del Producto:** `v2.1.0`  
**Estado:** Release Publicada y Sellada  
**Fecha:** 4 de Octubre de 2026  
**Arquitectura:** Windows x64 (MSVC 2026, C++20)  
**Commit de Hardening Previo:** `f013037`  
**Commit Certificado de Release:** `0b76616a9a52a0f755e260ed0cf421db2de8f33b`  
**Tag Anotado:** `v2.1.0` (Objeto: `8df6db9a5c03f7ba27d8f9160341581c52dc1ed2`)  

---

## 1. Resumen Ejecutivo

ABDAudioLab v2.1.0 consolida el ciclo de maduración técnica, operativa y de seguridad arquitectónica desarrollado a lo largo de los hitos **HITO-01 a HITO-08**. Esta versión establece una base de producción certificada y determinista con hosting VST3 nativo, pipeline de pruebas de audio determinista (Suite Audio A/B 5D.8), empaquetado y exportación unificada de modelos, sistema de diseño dual (Claro/Oscuro), sistema no invasivo de notificaciones del sistema con campana en cabecera y modal centrado, y aislamiento estricto de seguridad offline.

---

## 2. Inventario de Artefactos de Release y Huellas Digitales SHA-256

Los binarios oficiales de producción han sido compilados bajo configuración `Release` con MSVC v18.10 (Visual Studio 2026 Developer Toolchain) y C++20:

| Artefacto | Tamaño (Bytes) | SHA-256 Checksum | Función |
|---|---|---|---|
| **`ABDAudioLab.exe`** | 9.326.080 | `C94A0BDB3A67CCE77D8459E5E103E9BF1CDA3321CF5C745FEA11379E7CF6C89E` | Aplicación principal de laboratorio y GUI |
| **`ABDAudioLab_PluginWorker.exe`** | 3.805.696 | `DDD2976C98442DBBCA2209AB67DD9A0412BC939783B15FFBD056425FF3B1913E` | Proceso auxiliar de hosting para plugins |
| **`ReferenceSynth.vst3`** | — | — | Bundle VST3 de referencia sincronizado en el árbol portable |

---

## 3. Baseline de Verificación y Calidad (Suite Canónica en Git)

La suite de pruebas automatizadas sobre el commit inmutable de release (`0b76616`) garantiza la estabilidad funcional, la ausencia de memory leaks y la seguridad en tiempo real:

- **Casos de prueba totales:** 945 test cases
- **Casos superados:** 918 PASS
- **Casos omitidos (legítimos):** 27 SKIPPED (fixtures de plugins VST3 externos no instalados en el entorno de build: Dexed / VES)
- **Aserciones validadas:** 211.036 / 211.036 assertions PASS (100% de éxito)
- **Fallos reportados:** 0 fallos

*(Nota técnica: Véase el Addendum 8 para la reconciliación histórica frente a valores locales previos de 949 / 211.374).*

---

## 4. Correcciones Críticas de Hardening Integradas (Commit `f013037`)

1. **Sistema de Notificaciones del Sistema No Invasivo**:
   - Integración de `NotificationBellButton` en la cabecera superior junto al conmutador de tema, con indicador numérico animado (*badge*).
   - Conversión de `StartupWarningsPanel` en una ventana modal centrada y flotante con fondo atenuado (*dark scrim*), viewport scrolleable y descarte seguro por clic fuera o botón «Entendido».
   - Eliminación del desplazamiento o corrupción de tarjetas en el flujo del Wizard (Paso 1).
2. **Filtrado Estricto de Esquemas en `HardwareContractRegistry`**:
   - Omisión automática de metaschemas y ficheros de validación `*.schema.json` o con `$schema` no compatible, eliminando 9 falsos positivos en el inicio y dejando únicamente avisos editoriales legítimos de cuarentena.
3. **Seguridad Headless en `MeasurementFloatingWindow`**:
   - Inclusión de parámetro explícito `addToDesktop = false` para pruebas automatizadas y protección de `centreWithSize` ante displays inexistentes, erradicando fallos de acceso a puntero nulo en ejecución sin monitor.
4. **Resiliencia en Scripts de Build**:
   - Corrección de sintaxis en `build.bat` para garantizar la ejecución correcta de builds rápidas (`.\build.bat tests`).

---

## 5. Estado de Seguridad y Aislamiento Hardware (DeepMind 12D & D2.7B)

Durante todo el ciclo de empaquetado y smoke testing de Release v2.1.0:
- **Barrera de Hardware Física:** El sintetizador DeepMind 12D permanece en **reposo pasivo** por USB.
- **Transmisión MIDI:** **0 bytes TX / 0 bytes RX** transmitidos.
- **Puertos MIDI:** No se abre ni arma automáticamente ningún puerto MIDI en el arranque (`M-IN` y `M-OUT` en estado cerrado).
- **Servidor MCP:** Servidor `deepmind12` configurado como `disabled: true` en `mcp_config.json`.
- **Definiciones de Compilación:** `ABDSYNTHS_ALLOW_PHYSICAL_SNAPSHOT=0`. La adquisición física queda excluida y estrictamente bloqueada en la compilación estándar.
- **Estatus D2.7B Software-Core:** El desarrollo preliminar D2.7B (contratos inmutables, decodificador 242 y 4 tests Catch2 con 330 assertions) constituyó trabajo local experimental no versionado en el commit `0b76616`, por lo que **no formó parte del binario oficial publicado ni de la suite certificada de release**. Dicho trabajo se encuentra formalmente aislado y preservado en la rama local `archive/d2-7b-offline-snapshot`.

---

## 6. Acta de Smoke Test Operativo (Paso 2 Certificado)

Se ha verificado empíricamente la ejecución del binario Release `ABDAudioLab.exe` en entorno real:
1. **Arranque y Estabilidad:** Inicialización limpia. La aplicación alcanzó estado estable y mostró la ventana principal sin excepción, crash ni bloqueo observado durante el smoke test (registrado en log de usuario `ABDAudioLab_2026-10-04_00-36-23.log`).
2. **Splash y Ventana Principal:** La ventana de presentación (`SoundIdSplashWindow`) cedió el foco correctamente a la ventana principal (`LabMainWindow`).
3. **Ausencia de Apertura MIDI:** No se seleccionó ningún dispositivo hardware por defecto. Dispositivo activo inicializado: fixture virtual de prueba (`SyntheticFixture`).
4. **Cierre Ordenado:** Destructores de `MainComponent`, `PluginWindowController` y `PluginHost` invocados secuencialmente.
5. **Cero Procesos Huérfanos:** Verificación post-cierre con PowerShell:
   ```powershell
   Get-Process | Where-Object { $_.ProcessName -like "*ABDAudioLab*" }
   ```
   Retornó 0 procesos activos en memoria.

---

## 7. Tag Governance Addendum — v2.1.0

Durante la publicación remota se detectó que el repositorio contenía un tag `v2.1.0` previo (`c1bf1d6...`). 

El tag fue actualizado forzadamente (*forced update*) al nuevo objeto de tag anotado:
`8df6db9a5c03f7ba27d8f9160341581c52dc1ed2`

El tag anotado actual desreferencia inequívocamente al commit certificado de release:
`0b76616a9a52a0f755e260ed0cf421db2de8f33b` (`refs/tags/v2.1.0^{}`)

El presente addendum y el acta de smoke test completa fueron incorporados posteriormente en el commit administrativo de gobernanza en `main` para documentar la trazabilidad del retag; dicho commit posterior no altera el contenido inmutable versionado por `v2.1.0`.

**Motivo:**
Alinear la referencia remota con el commit certificado de release `0b76616a9a52a0f755e260ed0cf421db2de8f33b`, que contiene el código de producción validado, las notas de release originales, las correcciones de hardening de UI/notificaciones (`f013037`) y el inventario SHA-256 de los artefactos certificados.

**Acción para consumidores que hubieran obtenido el tag previo:**
```powershell
git fetch --tags --force origin
```

---

## 8. Post-Release Addendum — Conciliación de Métricas de Suite y Aislamiento D2.7B

> [!NOTE]
> **Auditoría Forense de Métricas (2026-10-04):**  
> Durante la estabilización post-release se identificó que las métricas reportadas inicialmente en la documentación histórica (**949 test cases, 922 PASS, 211.374 assertions**) correspondieron a una ejecución sobre un working tree local extendido y no reflejaban el commit canónico inmutable `0b76616`.
>
> **Desglose de los Tres Estados de Verificación:**
> 1. **Baseline Canónico de Release (Commit `0b76616` / Tag `v2.1.0` inmutable):**
>    - Test cases: **945** (918 PASS, 27 SKIPPED legítimos, 0 FAIL)
>    - Aserciones: **211.036 / 211.036 PASS** (100% de éxito)
> 2. **Estado Local Extendido (con trabajo D2.7B no versionado):**
>    - Añadía 4 test cases (`test_SnapshotCore_IsolationAndInvariants.cpp`) y 330 assertions.
>    - Resultado local: **949 test cases, 922 PASS, 27 SKIPPED, 211.366 assertions PASS**.
> 3. **Estado Histórico Previo al Hardening `f013037`:**
>    - Incorporaba además 8 aserciones dinámicas asociadas a advertencias de esquema descartadas durante el hardening.
>    - **Ecuación de Conciliación:**  
>      `211.036 (Canónico 0b76616) + 330 (D2.7B local) + 8 (Warnings pre-hardening) = 211.374 assertions`.
>
> **Destino de D2.7B Software-Core:**  
> La totalidad de artefactos de D2.7B (29 archivos entre fuentes C++, tests y herramientas de loopback/harness) ha sido preservada y aislada de forma permanente e inmutable en la rama local `archive/d2-7b-offline-snapshot` (commit `c7fde7d`). Dicho código no forma parte de `main` ni de los binarios de `v2.1.0`, confirmando que el aislamiento del hardware DeepMind 12D y la exclusión de módulos preliminares en el producto final fueron totales.

