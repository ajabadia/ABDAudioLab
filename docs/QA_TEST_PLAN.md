# Plan de Calidad y Pruebas (QA Test Plan) — ABDAudioLab

**Proyecto:** ABDAudioLab  
**Versión:** 1.1.0  
**Fecha:** 2026-09-01  
**Actualizado:** 2026-10-01 (alta de QA-13 a QA-16, gates de hermeticidad)  

---

## 1. Estrategia de Calidad y Objetivos de Prueba

El plan de calidad tiene como objetivo asegurar la precisión acústica, la estabilidad en tiempo real y la compatibilidad de exportación de los modelos numéricos extraídos.

---

## 2. Matriz de Casos de Prueba (Test Cases)

| ID | Área | Descripción de la Prueba | Criterio de Aprobación | Estado |
|---|---|---|---|---|
| **QA-01** | Compilación | Ejecutar `./build.bat` en Windows 11 con MSVC Visual Studio 18 (2026). | Compilación Release limpia (código de salida 0) y generación de `ABDAudioLab.exe`. | **PASADO** ✓ |
| **QA-02** | Audio & MIDI Setup | Abrir diálogo `Audio & MIDI Setup...` y verificar detección de interfaces. | Visualización de entradas/salidas de audio WASAPI y puertos MIDI In/Out activos. | **PASADO** ✓ |
| **QA-03** | Tono Diagnóstico | Activar botón `Diagnostic Tone (1 kHz)`. | Emisión de onda senoidal pura a 1 kHz hacia el DAC/altavoces sin chasquidos. | **PASADO** ✓ |
| **QA-04** | Auto-Test Mock Filter | Seleccionar modo *Mock DSP* y suite *SpectrumFilter* (15 puntos Farina). | Secuenciador completa la sesión, calcula $(\mu, \sigma)$ y genera archivos `.h` y `.json` en `exported_luts/`. | **PASADO** ✓ |
| **QA-05** | Suite ADSR | Seleccionar suite *TimeDynamic (ADSR)*. | Inyección de pulsos trigger y cálculo coherente de tiempos de ataque y decaimiento. | **PASADO** ✓ |
| **QA-06** | Suite Delay | Seleccionar suite *TimeDynamic (Delay)*. | Disparo de Dirac Delta y medición precisa de retardos en milisegundos. | **PASADO** ✓ |
| **QA-07** | Suite WaveShaper | Seleccionar suite *WaveShaper (Saturation)*. | Rampa lineal de amplitud de 0.0 a 1.0 y extracción de curva de transferencia y THD %. | **PASADO** ✓ |
| **QA-08** | Modo Manual Eurorack | Seleccionar modo *Manual Analog / Eurorack*. | El robot despliega el cartel de ajuste para el operador y se reanuda al pulsar la **Barra Espaciadora**. | **PASADO** ✓ |
| **QA-09** | Conexión Roland AIRA | Conectar módulo AIRA USB y conmutar a *Roland AIRA Modular*. | Detección de dispositivo USB SysEx y envío de tramas `DT1`/`RQ1`. | **PASADO** ✓ |
| **QA-10** | Zero-Allocation DSP | Ejecución de escaneo continuo de audio. | Cero llamadas a `malloc`/`new` dentro de `processBlock` y protección `ScopedNoDenormals`. | **PASADO** ✓ |
| **QA-11** | Validador de Ruteo | Conectar cables ilegales en matriz AIRA (`RF-25`, `RF-26`). | Bloqueo previo por `RoutingValidator` e informe de error sin envío de tramas erróneas al hardware. | **PASADO** ✓ |
| **QA-12** | Pre-Roll de 3 Tonos | Activar estímulo `SyncPulses3` / Calibración de sesión. | Emisión de 3 ráfagas a 1 kHz a -3 dBfs con envolvente Hann para alineación sample-accurate. | **PASADO** ✓ |
| **QA-13** | Hermeticidad de rutas (guard) | Ejecutar `ABDAudioLab_Tests.exe "[hygiene]"`. | 8 casos y 57 aserciones en verde. Cero dependencia de `getCurrentWorkingDirectory` y cero literales de ruta personal en `src/**`, sin excepciones sin justificar. | **PASADO** ✓ |
| **QA-14** | Hermeticidad de rutas (ejecución) | Ejecutar `ABDAudioLab_Tests.exe "~[ves]"` **desde la raíz del repo y desde `build/Release`**. | **Ningún fallo de resolución de recursos** en ninguno de los dos directorios: los 918 casos se descubren y resuelven igual desde cualquiera de los dos. | **PARCIAL** ⚠️ |
| **QA-15** | No-vacuidad del guard | Inyectar una regresión de ruta (CWD o literal absoluto) y ejecutar `[hygiene]`. | El guard **falla** nombrando fichero y línea. Un guard que nunca se ha visto fallar no está verificado. | **PASADO** ✓ |
| **QA-16** | Inmutabilidad de evidencia 5D | `git diff --name-only -- docs/qa/` antes y después de la suite. | **0 ficheros.** La suite no puede alterar reportes, manifest ni hashes canónicos. | **PASADO** ✓ |

### Nota sobre QA-14 (resultado parcial, 2026-10-01)

La hermeticidad de rutas —el objetivo de POST-5D.5— **se cumple**: cero fallos de resolución de
recursos desde cualquiera de los dos directorios, y los 918 casos se descubren igual en ambos.
Desde `build/Release` la suite cerró en verde (890 PASS, 0 FAIL, exit 0).

Lo que impide marcar QA-14 como **PASADO** es un defecto **preexistente y ajeno a las rutas**:
`test_Integration01GuidedVsClassicAudio` compara con igualdad exacta `pumpedBlocks`,
`processedSamples` y `midiTrace` entre dos rutas que compiten con un hilo worker real. Esos valores
dependen del planificador, así que bajo carga de máquina las dos rutas bombean distinto
(observado: 525 vs 638 bloques). Pasó en aislamiento y falló en dos corridas completas desde la raíz.

**No es una regresión de POST-5D.5** y no se ha modificado: cambiar la semántica de una aserción de
aceptación es decisión del responsable del hito. Ver `docs/HANDOFF.md` §1, "Abiertos conocidos".

---

## 3. Procedimiento de Verificación de Entregables (.h / .json)

1. Verificar que el archivo generado en `exported_luts/<Nombre>_LUT.h` contiene la estructura alineada:
   ```cpp
   struct alignas(16) AbdBatchedPoint
   {
       float p1;
       float p2;
       float mu;
       float sigma;
       float sec_mu;
       float sec_sigma;
       float thd_percent;
       float reserved;
   };
   ```
2. Verificar que compila directamente al incluirse en un proyecto JUCE de síntesis virtual.
3. Verificar que el archivo `<Nombre>_Report.json` es un JSON sintácticamente válido que se abre en cualquier visualizador estándar.
