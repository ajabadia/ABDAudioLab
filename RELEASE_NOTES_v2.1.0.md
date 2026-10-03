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

## 3. Baseline de Verificación y Calidad (Suite Global Catch2)

La suite de pruebas automatizadas garantiza la estabilidad funcional, la ausencia de memory leaks y la seguridad en tiempo real:

- **Casos de prueba totales ejecutados:** 949 test cases
- **Casos superados:** 922 PASS
- **Casos omitidos (legítimos):** 27 SKIPPED (correspondientes a fixtures de plugins VST3 externos no instalados en la máquina de build: Dexed / VES)
- **Aserciones validadas:** 211.374 / 211.374 assertions PASS (100% de éxito)
- **Fallos reportados:** 0 fallos

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

## 5. Estado de Seguridad y Aislamiento Hardware (DeepMind 12D)

Durante todo el ciclo de empaquetado y smoke testing de Release v2.1.0:
- **Barrera de Hardware Física:** El sintetizador DeepMind 12D permanece en **reposo pasivo** por USB.
- **Transmisión MIDI:** **0 bytes TX / 0 bytes RX** transmitidos.
- **Puertos MIDI:** No se abre ni arma automáticamente ningún puerto MIDI en el arranque (`M-IN` y `M-OUT` en estado cerrado).
- **Servidor MCP:** Servidor `deepmind12` configurado como `disabled: true` en `mcp_config.json`.
- **Definiciones de Compilación:** `ABDSYNTHS_ALLOW_PHYSICAL_SNAPSHOT=0`. La adquisición física queda excluida y estrictamente bloqueada en la compilación estándar.
- **Infraestructura Validada:** Únicamente los contratos de datos y la decodificación inmutable en memoria (D2.7B software-core) forman parte del ecosistema validado.

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
