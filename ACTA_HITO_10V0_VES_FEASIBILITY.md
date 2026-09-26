# ACTA DE CERTIFICACIÓN METROLÓGICA Y FACTIBILIDAD TÉCNICA: HITO-10V0

**Fecha:** 2026-09-25  
**Proyecto:** ABDAudioLab  
**Compilación de Prueba:** Build #459 (Release x64)  
**Autoridad Metrológica:** Antigravity (Lead Architect & Metrologist) / ajabadia (Tándem Partner)  
**Test Suite:** `[ves][external][firmware_emulated][feasibility]` (22 assertions, 5 test cases, 0 failures)  
**Estado:** CERTIFICADO Y AUDITADO (PASS)  

---

## 1. Axioma Rector y Alcance Metrológico

> *«Un emulador puede reproducir una máquina vintage, pero solo una prueba de capacidad puede demostrar qué interfaz de control ofrece realmente al host y qué transporte declarativo puede usar ABDAudioLab sin inventar capacidades.»*
>
> *«Que un mensaje atraviese un MidiBuffer sin crash demuestra transporte de host; solo un cambio reproducible de estado o audio demuestra control real de la máquina emulada.»*

En cumplimiento de las directivas para la integración de emuladores basados en firmware/ROM y en estricta deferencia al principio de no presuposición técnica, se ha ejecutado el spike de caracterización **HITO-10V0**. 

Este hito tuvo como objetivo someter a prueba empírica directa el plugin binario VST3 **Vintage Emulator Studio (VES)** sobre la máquina **Casio CZ-101**, resolviendo rigurosamente las incógnitas iniciales de hosting, legalidad, rutas y estabilidad básica antes de abordar el control semántico en **HITO-10V0.1** y los perfiles de target en **HITO-10V1**.

---

## 2. Telemetría de Inspección del Binario VST3 (VES)

A través del arnés hermético de introspección [`ExternalPluginFixture`](src/synth/ExternalPluginFixture.cpp) y [`test_VesCz101Feasibility.cpp`](src/tests/test_VesCz101Feasibility.cpp), se obtuvieron los siguientes datos canónicos:

| Parámetro Metrológico | Valor Observado | Verificación / Cumplimiento |
|---|---|:---:|
| **Ubicación Canónica** | `C:\Program Files\Common Files\VST3\Vintage Emulator Studio.vst3` | ✅ PASS |
| **Fabricante / Vendor** | `Autodafe` | ✅ PASS |
| **Identificador Canónico (UID)** | `VST3-Vintage Emulator Studio-3c51d60f-91c58935` | ✅ PASS |
| **Hash SHA-256 del Binario** | `c100b10b6bef0b23eb7bab23925f185213cfdded72e01c50fdc00c98329de9d8` | ✅ FIXITY CERTIFICADA |
| **Tamaño en Disco** | 48.480.256 bytes (~46.2 MB) | ✅ AUDITADO |
| **Buses de Audio Expuestos** | 1 bus de salida estéreo (2 canales) | ✅ PASS |
| **Arquitectura de Ejecución** | InProcess External Binary x64 | ✅ PASS |

---

## 3. Descubrimiento de ROM, Fijación Criptográfica y Límites Legales

Se auditó la cadena de resolución de configuración de VES y el directorio de ROMs del usuario:

1. **Resolución de Ajustes de VES:**
   - Ubicación: `%APPDATA%\VintageEmulatorStudio\settings\rom-directory.txt`
   - Ruta configurada: `C:\Users\ajaba\Downloads\ves-windows\ROMS`
2. **Archivo ROM CZ-101 Localizado:**
   - Nombre: `cz101.zip` (y binario plano `Casio CZ-101_HN613256P_5F3_S40.BIN`)
   - Tamaño: 30.223 bytes (zip) / 32.768 bytes (binario plano de 32 KB)
   - Hash SHA-256: `3a7889267c6f3594d49b3c4a81cc827a0df61c9b4760995fd1fb97efb00f3a91`
3. **Límite Legal y Normativo (Compliance Estricto):**
   - **Regla de Oro:** ABDAudioLab jamás almacena, redistribuye, empaqueta ni descarga ROMs protegidas por propiedad intelectual.
   - El contrato declarativo de TargetProfile solo almacenará metadatos de validación técnica: `romHash` (SHA-256), `romSizeBytes` (30223 / 32768) y `machineId` (`cz101`), garantizando que la ROM sea provista estrictamente por el usuario final.

---

## 4. Introspección del Catálogo de Parámetros y Mecanismo de Selección de Máquina

La inspección del catálogo VST3 arrojó un hallazgo fundamental de arquitectura:

```text
[HITO-10V0 Parameter Catalog & Machine Selection Interface]
  totalParametersExposed:       2081
  hasMachineSelectionParameter: NO
  hasCzSpecificParameter:       NO
```

### Detalle de los 2.081 Parámetros VST3
- **Parámetro 0:** `Bypass`
- **Parámetros 1 a 2080:** Mapeo lineal exhaustivo de 16 canales MIDI × 130 números de control MIDI CC (`MIDI CC 0|0` hasta `MIDI CC 15|129`).
- **Parámetros de Síntesis Específica:** 0 parámetros de síntesis expuestos nativamente como automatización VST3.

### Núcleo de Emulación Subyacente y Advertencias de Trazas
Durante la instanciación e inicialización de VES se registraron trazas directas del motor:
```text
hd44780_a00.bin ROM NEEDS REDUMP
WARNING: the machine might not run correctly.
Error parsing XML layout: unable to find element cpanel
Initializing debugger module windows failed, falling back to imgui
```
**Conclusión técnica y advertencia metrológica:**
1. Vintage Emulator Studio opera internamente como un contenedor host del núcleo **MAME / MESS**. La máquina emulada (Casio CZ-101) no se selecciona mediante un parámetro VST3 continuo o discreto, sino mediante la configuración interna de la GUI de VES o el estado binario serializado (`getStateInformation` / `setStateInformation`).
2. Se registra el warning formal: **`WARN_VES_ROM_DUMP_QUALITY_UNVERIFIED`** debido a la traza `hd44780_a00.bin ROM NEEDS REDUMP`. La emulación puede requerir redump de pantalla LCD o ajustes adicionales.

---

## 5. Factibilidad de Transporte de Control y Telemetría de Audio

Se sometió a VES a un ciclo de inyección directa de eventos en el bloque de procesamiento de audio en tiempo real:

1. **Eventos NoteOn / NoteOff:** Aceptados por el host sin excepciones ni rechazos (`HostAccepted`).
2. **Eventos MIDI CC:** Aceptados por el host sin excepciones ni fallos (`HostAccepted`).
3. **Inyección Virtual de SysEx:** Se inyectó una trama de prueba Casio CZ (Manufacturer ID `0x44`, 9 bytes: `F0 44 00 00 70 10 00 3F F7`). El plugin procesó el bloque exitosamente sin excepciones de memoria ni crashes (`sysexProcessedWithoutCrash == true`).
   - **Delimitación metrológica:** Esto certifica **`virtualSysExTransport: NoCrash / HostAccepted`**. NO demuestra aún que el mensaje haya llegado a la CPU del CZ-101 ni que haya modificado parámetros de síntesis (`virtualSysExSemanticControl: PENDIENTE`).
4. **Ciclo de Vida de Estado:** `prepareToPlay(48000, 256)` y `releaseResources()` operan de forma limpia y recuperable.
5. **Telemetría de Audio en Arranque Frío:**
   - Amplitud máxima de buffer: `audioBufferMaxMagnitude = 0.000000` (silencio en arranque frío headless sin snapshot de máquina inicializada).
6. **Determinismo y Repetibilidad del Ciclo de Vida del Host:**
   - 2 pasadas consecutivas idénticas de 5.120 muestras a 48 kHz.
   - Diferencia muestral máxima absoluta: `0.00000000`.
   - Clasificación metrológica estricta: **`SilentBufferByteIdentical`** (o `HostLifecycleDeterministicBeforeAudibleOutput`).
   - **Delimitación metrológica:** Dos buffers de silencio iguales demuestran estabilidad del ciclo de vida del host, pero **NO** demuestran repetibilidad de audio musical audible. La repetibilidad audible queda diferida a una prueba con sonido real.

---

## 6. Respuestas Concluyentes a las 14 Preguntas de Factibilidad

| # | Pregunta de Factibilidad | Respuesta Metrológica / Evidencia |
|---|---|---|
| **1** | ¿VES se carga de forma estable como VST3? | **SÍ.** Carga limpia mediante `AudioPluginFormatManager` y `ExternalPluginFixture`. |
| **2** | ¿Cuáles son sus UID, versión y hash binario? | **UID:** `VST3-Vintage Emulator Studio-3c51d60f-91c58935`, **Vendor:** `Autodafe`, **SHA-256:** `c100b10b6bef0b23eb7bab23925f185213cfdded72e01c50fdc00c98329de9d8`. |
| **3** | ¿Se puede elegir CZ-101 desde el host vía parámetros VST3? | **NO.** No existe parámetro VST3 para cambiar de máquina. La selección es interna a VES (GUI o bloque de estado serializado). |
| **4** | ¿Cómo se detecta que CZ-101 está activo? | Mediante inspección del estado binario (`getStateInformation`) o pre-verificación de la presencia de la ROM `cz101.zip`. |
| **5** | ¿Dónde busca VES las ROMs y cómo se comprueban? | En la ruta leída de `%APPDATA%\VintageEmulatorStudio\settings\rom-directory.txt`. Se comprueba SHA-256 (`3a788926...`) y tamaño (30.223 B) sin copiar la ROM al repositorio. |
| **6** | ¿Se puede leer y restaurar el estado completo de VES? | **SÍ.** `getStateInformation` y `setStateInformation` son soportados por el wrapper VST3. |
| **7** | ¿VES recibe eventos NoteOn/NoteOff del host? | **SÍ (HostAccepted).** Aceptados e inyectados en `juce::MidiBuffer` sin errores de host. |
| **8** | ¿VES recibe eventos MIDI CC del host? | **SÍ (HostAccepted).** Dispone además de 2.080 parámetros espejo dedicados a CCs. |
| **9** | ¿VES procesa SysEx virtual inyectado por el host? | **TRANSPORTE SÍ (NoCrash / HostAccepted); CONTROL SEMÁNTICO PENDIENTE.** La trama atraviesa el buffer sin crash, pero no se ha demostrado aún que altere el sonido ni el estado del CZ-101. |
| **10** | ¿VES expone parámetros VST3 que sustituyan al SysEx original? | **NO.** Solo expone Bypass y MIDI CCs genéricos. No hay automatización directa de parámetros CZ-101. |
| **11** | ¿Produce audio en el buffer interno del host? | **SILENCIO EN ARRANQUE FRÍO (0.000000).** Requiere estado inicial válido / inicialización de máquina para producir audio audible. |
| **12** | ¿Se puede reiniciar el estado entre ejecuciones? | **SÍ.** Reinicializable vía `prepareToPlay` / `releaseResources` o restauración de snapshot de memoria. |
| **13** | ¿Cómo se clasifica la repetibilidad de VES? | **`SilentBufferByteIdentical`** (Determinismo de ciclo de vida del host sobre buffers silenciosos; repetibilidad musical audible pendiente). |
| **14** | ¿La evidencia debe etiquetarse como virtual? | **SÍ.** Obligatoriamente `targetKind = "FirmwareEmulatedPlugin"`, diferenciada terminantemente de `HardwarePhysical`. |

---

## 7. Dictamen Metrológico y Estado de Hitos

```text
HITO-10V0:
CERTIFICADO EXCLUSIVAMENTE COMO SPIKE DE DISCOVERY, HOSTING Y ESTABILIDAD BÁSICA.

HITO-10V1:
NO APROBADO AÚN COMO INTEGRACIÓN COMPLETA. BLOQUEADO HASTA V0.1.

HITO-10V0.1:
APROBADO Y REQUERIDO: "VES CZ-101 Semantic Control & Audible Output Probe".
```

### Justificación Normativa
El spike HITO-10V0 cumplió su cometido: caracterizó el binario, resolvió las rutas persistidas, verificó la ROM sin violar licencias y confirmó que VES no crashea con MIDI/SysEx. Sin embargo, no autoriza la redacción del perfil final `casio_cz101_ves.target.json` con `controlTransport = "InProcessVirtualMidi"` porque aún no se ha demostrado que el SysEx virtual modifique la síntesis ni que el CZ-101 emulado produzca audio audible en ABDAudioLab.

---

## 8. Especificación de Requisitos para HITO-10V0.1

Antes de autorizar **HITO-10V1**, se debe ejecutar una prueba atómica decisiva:

- **Identificador:** `HITO-10V0.1: VES CZ-101 Semantic Control & Audible Output Probe`
- **Suite:** `src/tests/test_VesCz101SemanticControl.cpp`
- **Tags:** `[ves][external][firmware_emulated][semantic_control]`
- **Condiciones Vinculantes de Certificación:**
  1. **Estado Inicial Verificable:** Snapshot de estado de VES configurado con CZ-101, validado e inspeccionado (garantizando **cero bytes de ROM incrustados** en artefactos del repositorio).
  2. **Audio Audible:** NoteOn MIDI 60 produce audio no silencioso (`RMS > -80 dBFS` o `peak > 1e-5`).
  3. **Control Semántico Observable:** Un mensaje SysEx CZ documentado (o CC asignado) produce una alteración demostrable (`audio A != audio B` o cambio en `stateHash`).
  4. **Repetibilidad Audible:** Dos pasadas idénticas sobre el estado restaurado producen audio no silencioso reproducible (`ByteIdentical` o `FunctionallyEquivalent` con tolerancia estricta).
  5. **Trazas de Diagnóstico:** Etiquetado de advertencia `WARN_VES_ROM_DUMP_QUALITY_UNVERIFIED` en la evidencia metrológica.
