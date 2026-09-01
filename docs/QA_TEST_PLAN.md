# Plan de Calidad y Pruebas (QA Test Plan) — ABDAudioLab

**Proyecto:** ABDAudioLab  
**Versión:** 1.0.0  
**Fecha:** 2026-09-01  

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
