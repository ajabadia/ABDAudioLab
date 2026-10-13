# Product Requirements Document (PRD): ABDAudioLab v2.x

**Versión:** 2.1.0  
**Fecha:** Octubre 2026  
**Estado:** Aprobado / En Ejecución  
**Lead Architect:** Antigravity  

---

## 1. Visión y Propósito del Producto

**ABDAudioLab** es una estación de trabajo de laboratorio y clonación acústico-electrónica para instrumentos y procesadores musicales. Su misión es capturar el comportamiento dinámico, la distorsión armónica, la respuesta al impulso y las curvas de modulación de hardware analógico y sintetizadores digitales, produciendo modelos computacionales (NAM, LUTs aceleradas por SIMD, perfiles `.abdprofile`) e informes de certificación técnica.

### La Transformación hacia Usabilidad Comercial (Paradigma SoundID Reference)
Tradicionalmente, las suites de clonación y análisis acústico han sufrido de una extrema complejidad de laboratorio, exigiendo recableados constantes, configuración manual de barridos FFT y una interfaz orientada exclusivamente a ingenieros de DSP. 

Inspirado en el paradigma de diseño sustractivo de **Sonarworks SoundID Reference**, ABDAudioLab v2.x evoluciona bajo el principio rector: **"Habla en músico, piensa en ingeniero"**. La herramienta ofrece un flujo de trabajo vertical de 5 pasos asistido que oculta la fricción matemática y el recableado repetitivo, manteniendo al mismo tiempo un acceso de precisión total para ingenieros a través de presets canónicos unificados.

---

## 2. Audiencia Objetivo y Arquetipos de Usuario

### 2.1. Músico / Productor de Sonido ("El Músico")
- **Perfil:** Dueño de sintetizadores hardware (Roland Juno, Casio CZ, sintetizadores modernos analógicos/FM), pedales de guitarra y cajas de preamplificación.
- **Necesidad:** Desea capturar la huella de su hardware en un clic sin desconectar cables cada vez que abre la sesión. Requiere visualizaciones claras e intuitivas ("calidez", "claridad", "fase correcta") y exportar modelos listos para usar en su DAW o modelos de emulación.
- **Fricción a eliminar:** No quiere ver complejas matrices de calibración de bucle cerrado cada vez que arranca el software.

### 2.2. Ingeniero de Grabación / Desarrollador de DSP ("El Ingeniero")
- **Perfil:** Ingeniero de mastering, diseñador de emulaciones analógicas o investigador acústico.
- **Necesidad:** Demanda acceso quirúrgico a la matriz de parámetros (32 coeficientes 4x8), corrección de outliers, repetibilidad determinista en decibelios (dBfs) y muestras (samples RTL), y reportes reproducibles de intermodulación y THD+N.
- **Fricción a eliminar:** No tolera parpadeos ni bloqueos en la interfaz cuando se realizan operaciones de disco o mediciones puntuales.

---

## 3. Dominios de Hardware Soportados

A diferencia de herramientas de calibración de salas/altavoces, ABDAudioLab está diseñado para la caracterización de 5 dominios de audio instrumental:

1. **Sintetizadores Hardware Analógicos y Digitales:**
   - Inyección y grabación de secuencias de control (MIDI CC, Pitch Bend, compuertas de velocidad).
   - Control automatizado vía protocolos específicos: Roland SysEx, Casio CZ Nibble Codec, Audio Tape FSK Modem (Juno-60).
2. **Pedales de Distorsión, Saturación y Fuzz:**
   - Caracterización armónica H2-H5 mediante deconvolución Farina.
   - Análisis de respuesta dinámica no lineal y compresión de simetría de onda.
3. **Preamplificadores y Canales de Consola:**
   - Medición de curva de transferencia de frecuencia $H(f)$, SNR con piso de ruido físico silenciado y respuesta de transitorios.
4. **Procesadores Dinámicos (Compresores / Limitadores):**
   - Curvas de transferencia entrada/salida (I/O), estimación de umbral (Threshold), ratio, y tiempos de ataque/relajación.
5. **Plugins de Software (VST3 / AU / LV2 / ARA):**
   - Hosting autónomo out-of-process para contrastar el comportamiento del plugin digital frente a la unidad física gemela.

---

## 4. Objetivos Estratégicos y Roadmap de Sprints

### 4.1. Sprint 1: Perfiles de Infraestructura (`.abdloop`) y Autodetección
- **Objetivo:** Eliminar por completo el paso repetitivo de Loopback Calibration si la tarjeta de sonido, frecuencia de muestreo y tamaño de buffer ya fueron caracterizados previamente.
- **Entregables:**
  - Formato abierto `.abdloop` (JSON estructurado con firma criptográfica SHA-256).
  - Almacén local atómico en `%APPDATA%/ABDAudioLab/loopback_profiles/`.
  - Carga asíncrona mediante `juce::ThreadPoolJob` sin bloqueo del hilo de mensajería (Message Thread).
  - Estado `LoadedFromHistory` en el Stepper vertical con badge cian SoundID y bypass automático del Paso 2.

### 4.2. Sprint 2: Presets de Medición Asistida y Edición Quirúrgica
- **Objetivo:** Sustituir la configuración manual de laboratorio por presets según el rol del equipo e implementar edición directa en la matriz de coeficientes.
- **Entregables:**
  - Presets canónicos de medición: "Pedal de Saturación", "Sintetizador Dinámico", "Preamplificador Lineal", "Compresor de Transitorios".
  - Menú contextual 2D en el mapa de calor: re-medición de punto singular, interpolación bilateral de vecinos y marcado de outlier.
  - Bloqueo visual controlado durante la re-medición puntual sin afección al motor de audio.

### 4.3. Sprint 3: Contenedor Universal `.abdprofile` e Importación Portátil
- **Objetivo:** Empaquetar el perfil de clonación completo en un formato portátil y universal.
- **Entregables:**
  - Archivo nativo `.abdprofile` (contenedor ZIP estructurado: metadatos, curvas de corrección, matriz binaria `model_matrix.dat` de 32 coeficientes float y muestras de validación).
  - Importación inteligente: detección de discrepancia de hardware. Si el equipo coincide, activación en caliente; si difiere, importación offline con aviso de advertencia amigable y modo de solo lectura.

### 4.4. Sprint 4: Traductores Semánticos y UI Unificada en WebView2
- **Objetivo:** Acercar las métricas de laboratorio al vocabulario musical y pulir la experiencia visual.
- **Entregables:**
  - Traductores visuales: "Calidez / Grano" (distorsión armónica H2/H3), "Brillo / Apertura" (respuesta > 8 kHz), semáforo de coherencia de fase.
  - Integración fluida del osciloscopio interactivo en tiempo real vía WebView2 (ABDScope).
  - Selector de asistencia (Guiado, Clásico, Libre) sin bifurcación de motor.

---

## 5. Requisitos Funcionales Detallados

### RF-01: Flujo de Trabajo Guiado (Guided Workflow Stepper)
- La interfaz principal presentará una barra lateral vertical inspirada en SoundID Reference con 5 etapas secuenciales:
  - **Paso 0 (Info):** Identificación del sistema, reloj y estado del entorno.
  - **Paso 1 (Hardware / Ruteo):** Selección del dispositivo analógico o plugin virtual, asignación de canales I/O y carga de preset de rol.
  - **Paso 2 (Calibración Loopback):** Si existe `.abdloop`, se auto-marca como `LoadedFromHistory` y se habilita el botón "Recalibrar"; si no existe, guía al usuario en el barrido con Farina.
  - **Paso 3 (Ejecución de Sesión):** Disparo de la receta de medición, monitoreo de vúmetros en tiempo real y progreso porcentual.
  - **Paso 4 (Reporte y Exportación):** Visualización de curvas comparativas, informe técnico de certificación y exportación de modelos.

### RF-02: Persistencia y Autodetección de Hardware (`.abdloop`)
- El sistema calculará una clave hash normalizada derivada de `(deviceName, sampleRate, bufferSize)`.
- Si el perfil existe y el hash es íntegro, el motor cargará la curva de compensación inversa $H_{inv}(f)$ y el trim automático de ganancia sin interacción del usuario.

### RF-03: Paridad Absoluta de Modos (ADR-001 / PARITY-01)
- Las modalidades "Modo Guiado" y "Modo Clásico" compartirán de forma estricta la misma entidad `ExperimentPlan`.
- Cambiar de modo en cualquier momento de la sesión no reinicializará los buffers ni descartará las mediciones ya capturadas.

### RF-04: Exportación de Modelos de Clonación
- Generación de datasets preparados para Neural Amp Modeler (NAM) en formato WAV Float32 a 48 kHz.
- Generación de tablas LUT (Look-Up Tables) optimizadas para evaluación SIMD AVX2 en plugins de emulación en tiempo real.
- Exportación de informes técnicos de certificación en HTML interactivo autónomo (sin dependencias web externas) y CSV analítico.

---

## 6. Requisitos No Funcionales (Calidad e Ingeniería)

- **RNF-01 (DSP Real-Time Safety):** El callback de audio de JUCE (`audioDeviceIOCallbackWithContext`) tiene prohibición estricta de asignaciones en el heap (`malloc`, `new`, `std::vector::push_back`) y bloqueos de sincronización (`std::mutex`, semáforos o llamadas al sistema de archivos).
- **RNF-02 (Sincronización Atómica):** Todo intercambio de banderas y estado entre el hilo de audio y los hilos de control debe utilizar `std::atomic` con ordenamiento explícito de memoria (`memory_order_acquire` / `release`).
- **RNF-03 (UI Responsiva y Libre de Bloqueos):** Ninguna operación de lectura/escritura en disco puede ejecutarse en el Message Thread de JUCE. Toda persistencia debe delegarse a `juce::ThreadPoolJob`.
- **RNF-04 (Tolerancia a Corrupción):** Cualquier archivo `.abdloop` o `.abdprofile` dañado, incompleto o manipulado debe ser descartado silenciosamente o reportado vía diálogo no bloqueante, sin comprometer la estabilidad del proceso.
- **RNF-05 (Integridad de Compilación y Suite de Tests):** Compilación limpia bajo MSVC x64 sin advertencias críticas ni dependencias externas rotas; 100% de la suite de tests unitarios (Catch2) en verde.

---

## 7. Criterios de Aceptación y Éxito Comercial

| Criterio | Métrica de Éxito | Método de Validación |
| :--- | :--- | :--- |
| **Tiempo de Setup** | Reducción del 80% en tiempo previo a la medición cuando la tarjeta ya fue calibrada. | Paso 2 marcado como `LoadedFromHistory` en < 200 ms tras iniciar. |
| **Estabilidad de Audio** | 0 xruns o glitches causados por la UI o por operaciones de disco. | Test de verificación de prioridad de hilos y `zero-allocation`. |
| **Portabilidad de Perfiles** | Carga exitosa de perfiles `.abdprofile` entre diferentes máquinas. | Validación en `test_LoopbackProfileStore.cpp` y suites de integración. |
| **Aprobación de la Suite** | 0 regresiones en los tests existentes del proyecto (> 600 tests). | Ejecución completa de `ABDAudioLab_Tests.exe`. |
