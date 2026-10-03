# ACTA DE CIERRE, SMOKE TEST Y CERTIFICACIÓN DE RELEASE — HITO-08
## ABDAudioLab v2.1.0 — Producción x64 & Gobernanza de Release

**Fecha de Cierre Local:** 4 de Octubre de 2026  
**Fecha de Publicación Remota:** 4 de Octubre de 2026  
**Hito Global:** HITO-08 (Release v2.1.0 & QA Operativo)  
**Documento Rector:** `docs/audits/ACTA_HITO_08_RELEASE_SMOKE_TEST_v2.1.0.md`  
**Compilador:** MSVC 18.10.3 (Visual Studio 2026 Developer Toolchain) · Release x64 (C++20)  
**Commit de Hardening Previo:** `f013037`  
**Commit Certificado de Release:** `0b76616a9a52a0f755e260ed0cf421db2de8f33b` (`main`)  
**Tag Anotado Publicado:** `v2.1.0` (Object `8df6db9a5c03f7ba27d8f9160341581c52dc1ed2`)  
**Commit Administrativo de Gobernanza:** `93a815967d6056b66e13fa7d62057398e4f16a04` (`main`)  
**Estado HITO-08:** 🟢 **CERRADO Y PUBLICADO REMOTAMENTE**  

---

## 1. Declaración Formal de Cierre y Alcance

Se declara formalmente completado, auditado y certificado el hito **HITO-08**, correspondiente a la publicación de la versión de producción **ABDAudioLab v2.1.0**.

El ciclo de validación garantiza:
1. Compilación `Release` limpia y reproducible con código de salida 0.
2. Suite global de pruebas Catch2 con 100% de aserciones en verde y cero fallos.
3. Smoke test operativo en entorno real con verificación de interfaz de usuario, arranque y cierre ordenado.
4. Cero procesos huérfanos tras la terminación.
5. Inviolabilidad de las barreras de seguridad física para el sintetizador DeepMind 12D.

---

## 2. Inventario de Artefactos de Release y Huellas Digitales SHA-256

Los binarios oficiales de producción compilados bajo configuración `Release` presentan las siguientes firmas criptográficas:

| Artefacto | Ruta de Salida | Tamaño (Bytes) | SHA-256 Checksum |
|---|---|---|---|
| **`ABDAudioLab.exe`** | `build\ABDAudioLab_artefacts\Release\ABDAudioLab.exe` | 9.326.080 | `C94A0BDB3A67CCE77D8459E5E103E9BF1CDA3321CF5C745FEA11379E7CF6C89E` |
| **`ABDAudioLab_PluginWorker.exe`** | `build\ABDAudioLab_artefacts\Release\ABDAudioLab_PluginWorker.exe` | 3.805.696 | `DDD2976C98442DBBCA2209AB67DD9A0412BC939783B15FFBD056425FF3B1913E` |
| **`ReferenceSynth.vst3`** | `build\ABDAudioLab_artefacts\Release\ReferenceSynth.vst3` | — | Bundle sincronizado en el árbol portable Release |

---

## 3. Baseline de Verificación y Calidad (Suite Global Catch2)

Ejecución completa del ejecutable de pruebas `build\Release\ABDAudioLab_Tests.exe "~[ves]"`:

```text
===============================================================================
test cases:    949 |    922 passed | 27 skipped
assertions: 211374 | 211374 passed |  0 skipped
fallos:          0
```

- **Casos Totales:** 949 test cases.
- **Superados:** 922 PASS.
- **Omitidos Legítimos:** 27 SKIPPED (exclusivamente fixtures de plugins VST3 externos no instalados en el entorno de build: Dexed / VES).
- **Aserciones Validadas:** 211.374 / 211.374 (100% de éxito).
- **Fallos:** 0 fallos reportados.

---

## 4. Acta de Smoke Test Operativo (Paso 2)

Se certifican las cuatro condiciones operativas en ejecución real:

1. **Arranque y Estabilidad:**
   - `ABDAudioLab.exe` arrancó en frío sin excepciones ni crashes. La aplicación alcanzó estado estable y mostró la ventana principal sin excepción, crash ni bloqueo observado durante el smoke test.
   - Registrado en telemetría de log de usuario: `ABDAudioLab_2026-10-04_00-36-23.log` (`%APPDATA%\ABDAudioLab\`).
2. **Transición Visual:**
   - La ventana de carga flotante (`SoundIdSplashWindow`) inicializó componentes y cedió el foco fluidamente a `LabMainWindow`.
   - Interfaz del Wizard (Paso 1) completamente despejada, sin recuadros de avisos de fondo invasivos ni solapamientos.
3. **Ausencia de Apertura MIDI Automática y Blindaje DeepMind:**
   - No se abrió ni armó ningún puerto MIDI en el arranque.
   - Píldora de estado superior: `A-IN` 🟢, `A-OUT` 🟢, `M-IN` 🔴, `M-OUT` 🔴.
   - Selector de hardware: `Select Target Hardware...` (círculo gris inactivo, sin dispositivo seleccionado).
   - Dispositivo activo por defecto: `Sintetizador Virtual de Prueba (Demo Snapshot) (ABDAudioLab)` (`SyntheticFixture`).
   - DeepMind 12D conectado por USB pero en **estricto reposo pasivo** (0 bytes TX/RX autorizados).
   - Servidor MCP `deepmind12` en `mcp_config.json`: `disabled: true`.
   - Flag de compilación: `ABDSYNTHS_ALLOW_PHYSICAL_SNAPSHOT=0`.
4. **Cierre Limpio y Ausencia de Procesos Huérfanos:**
   - Destructores de `MainComponent`, `PluginWindowController` y `PluginHost` invocados de manera ordenada.
   - Verificación PowerShell:
     ```powershell
     Get-Process ABDAudioLab, ABDAudioLab_PluginWorker -ErrorAction SilentlyContinue
     Get-Process | Where-Object { $_.ProcessName -like "*ABDAudioLab*" }
     ```
     **Resultado:** Cero procesos activos en memoria. Cero procesos huérfanos o hilos zombies.

---

## 5. Tag Governance Addendum — v2.1.0

> [!IMPORTANT]
> **Registro Histórico de Procedencia y Retag Forzado:**
> Durante la publicación remota se detectó que el repositorio remoto ya contenía un tag `v2.1.0` previo (objeto `c1bf1d6...`).
> 
> El tag fue actualizado forzadamente (*forced update*) desde el objeto previo:
> `c1bf1d6...`
> 
> al nuevo objeto de tag anotado:
> `8df6db9a5c03f7ba27d8f9160341581c52dc1ed2`
> 
> **Desreferenciación Unívoca:**
> El tag anotado actual `v2.1.0` desreferencia inequívocamente al commit certificado de release:
> `0b76616a9a52a0f755e260ed0cf421db2de8f33b` (`refs/tags/v2.1.0^{}`)
> 
> El acta completa de smoke test y el Addendum de Gobernanza del Retag fueron incorporados posteriormente en el commit administrativo `93a815967d6056b66e13fa7d62057398e4f16a04` de `main`. Dicho commit no forma parte del contenido versionado por `v2.1.0` y documenta la trazabilidad posterior de publicación.
> 
> **Motivo Técnico:**
> Alinear la referencia remota con el commit exacto que contiene las [RELEASE_NOTES_v2.1.0.md](../../RELEASE_NOTES_v2.1.0.md), el acta de Smoke Test completa, las correcciones de hardening de UI/notificaciones (`f013037`) y el inventario formal de hashes SHA-256.
> 
> **Acción para Clones y Consumidores con Tag Previo:**
> Aquellos entornos o clones locales que hubieran obtenido el tag anterior deben ejecutar:
> ```powershell
> git fetch --tags --force origin
> ```

---

## 6. Estado de Fronteras Hardware (DeepMind 12D)

| Parámetro | Estado |
|---|---|
| **DeepMind 12D Físico** | 🟡 **USB conectado, reposo pasivo** |
| **Puertos MIDI** | 🔒 **No abiertos / No armados** |
| **Servidor MCP `deepmind12`** | 🔒 **`disabled: true`** |
| **`ABDSYNTHS_ALLOW_PHYSICAL_SNAPSHOT`** | 🔒 **0** |
| **Adaptador Físico de Captura** | 🔒 **Excluido de Release** |
| **D2.7B Físico** | 🔒 **Bloqueada** |
| **Tráfico MIDI Físico** | 🔒 **0 bytes autorizados** |

---

## 7. Dictamen Final

**HITO-08 queda formalmente CERTIFICADO, SELLADO Y PUBLICADO REMOTAMENTE.**  
La versión de producto **ABDAudioLab v2.1.0** está consolidada en el repositorio oficial.
