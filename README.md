# ABDAudioLab — Universal Black-Box Hardware Profiler & Measurement Robot

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![JUCE](https://img.shields.io/badge/JUCE-8.0.4-orange.svg)](https://juce.com/)
[![CMake](https://img.shields.io/badge/CMake-3.22%2B-green.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-Proprietary-red.svg)]()

**ABDAudioLab** es una plataforma y banco de pruebas científico para ingeniería inversa y perfilado *Black-Box* y *Grey-Box* de hardware musical (sintetizadores, módulos Eurorack y procesadores de señal).

Automatiza la inyección de estímulos acústicamente calibrados (barridos exponenciales de Farina, impulsos Dirac Delta, rampa lineal y secuencias de sincronización multi-tono), captura la respuesta en un búfer circular *lock-free* y graba en tiempo real archivos `.wav` de 24-bit en disco, calculando mediante análisis estadístico:
1. **La Media ($\mu$)**: Curva de respuesta y comportamiento nominal de control (Hz, ms, dB).
2. **La Desviación Estándar ($\sigma$)**: Factor de deriva térmica, ruido analógico y dispersión (*Analog Circuit Behavior / ACB*).
3. **Tablas de Búsqueda (LUTs)**: Archivos C++ `.h` con estructuras `alignas(16) static const AbdBatchedPoint` optimizadas para SIMD y compatibles con emuladores en tiempo real (`chowdsp_utils`).
4. **Contenedor `.abdlabtest`**: Paquete ZIP autónomo y portable que integra el manifiesto de sesión, matrices binarias, grabaciones raw WAV y sumas de verificación.

---

## 🚀 Características Principales

* **Interfaz Gráfica Moderna (*SoundID Architecture*)**:
  * **Pantalla Splash Flotante Inmediata (< 50ms)**: Telemetría de inicio con sondeo de interfaces de audio y carga de contratos.
  * **Trazador de Curvas Interactivo (`SoundIdCurvePlotter`)**: Renderizado de curvas paramétricas en tiempo real con rejilla logarítmica Hz/dB.
  * **Plan de Sesión y Cola de Pruebas (`SoundIdSuiteList`)**: Gestión interactiva de tests con activación/bypass individual, creación de tests personalizados y visualización de progreso dinámico (`RUNNING X/N` $\rightarrow$ `DONE [OK]`).
  * **Cajón de Hardware & Módulos (`SlideInDrawer`)**: Selección de catálogo basada en contratos JSON dinámicos con fichas visuales de hardware y soporte para los 31 submódulos Roland AIRA.
  * **Tira de Vúmetros Estéreo (`SoundIdMeterStrip`)**: Monitorización de nivel RMS/Pico con calibración de ganancia y botón de inicio/parada maestro.
  * **Diálogo de Calibración de Bucle (`LoopbackCalibrationModal`)**: Medición de latencia de ida y vuelta (RTL) y respuesta en frecuencia del interfaz de audio.
* **Capa de Abstracción de Hardware (`IHardwareController`)**:
  * **Mock Virtual-Analog DSP**: Modelo interno de filtro resonante de 4 polos con saturación $\tanh$ y ruido térmico simulado para auto-tests y CI/CD.
  * **Roland AIRA Modular (SysEx/CC)**: Soporte completo de tramas `RQ1`/`DT1`, los 31 submódulos internos y matriz de ruteo virtual.
  * **Generic MIDI CC**: Controlador parametrizable para sintetizadores hardware estándar.
  * **Manual Eurorack (Operator-Assisted)**: Modo guiado para módulos analógicos puros con confirmación interactiva por teclado (**Barra Espaciadora**).
* **Generador y Grabador de Audio en Tiempo Real**:
  * Silencio de calibración de ruido térmico, Dirac Delta, Ruido Blanco determinista (LCG), Ruido Rosa (Voss-McCartney), Tono 1 kHz, Onda Cuadrada 1 kHz, Rampa Lineal y Barrido Logarítmico de **Farina (2000)**.
  * **Grabación Directa a Disco**: Guardado automático de archivos de audio `.wav` PCM de 24-bit sin pérdida por cada punto medido en la carpeta de sesión.
* **Persistencia y Seguridad de Datos**:
  * Empaquetador `.abdlabtest` ([SessionSerializer](src/core/SessionSerializer.h)) con compresión ZIP.
  * Flujos de `Save`, `Save As...` y autoguardado periódico en segundo plano.
  * Diálogos de salvaguarda de 3 opciones (*Purgar* vs *Invalidar* vs *Cancelar*) al editar o borrar pruebas con datos grabados.

---

## 🛠️ Requisitos del Sistema y Compilación

### Requisitos
* **Sistema Operativo**: Windows 10 / 11 (64-bit).
* **Compilador**: Microsoft Visual Studio 2022 o 2026 (MSVC con soporte C++20).
* **CMake**: 3.22 o superior.

### Compilación Rápida (Script Automatizado)
```cmd
./build.bat
```

Para compilar y ejecutar inmediatamente:
```cmd
./build.bat run
```

El ejecutable resultante se genera en:
```
build/ABDAudioLab_artefacts/Release/ABDAudioLab.exe
```

---

## 📁 Estructura del Repositorio

```
ABDAudioLab/
├── CMakeLists.txt              # Configuración CMake, JUCE 8.0.4 y C++20
├── build.bat                   # Script de compilación automática MSVC
├── contracts/                  # Especificaciones JSON de hardware
│   └── hardware/               # Contratos por modelo (Mock, AIRA, MIDI CC, Eurorack...)
├── assets/                     # Recursos gráficos, logotipos e imágenes de hardware
├── docs/                       # Documentación técnica y arquitectura
│   ├── ROADMAP.md              # Mapa de ruta y fases del proyecto
│   ├── ARCHITECTURE.md         # Arquitectura detallada y reglas de tiempo real
│   ├── HARDWARE_PROTOCOLS.md   # Especificaciones SysEx Roland, MIDI CC y FSK
│   ├── MATHEMATICAL_MODELS.md  # Fórmulas Farina, Wiener-Hammerstein y estadísticas
│   └── QA_TEST_PLAN.md         # Plan de calidad y pruebas de laboratorio
├── src/
│   ├── main.cpp                # Ventana principal, navegación SoundID y secuenciador
│   ├── audio/                  # Motor de audio, generador y receptor lock-free
│   ├── core/                   # Secuenciador, sesiones, registro de contratos y serializador .abdlabtest
│   ├── gui/                    # Componentes GUI estilo SoundID Reference (Gráficos, Cajones, Vúmetros, Modales)
│   ├── hardware/               # Controladores de hardware (Mock, SysEx, CC, Manual)
│   ├── math/                   # Motor analítico, desconvolución Farina, interpolador 2D y calibrador loopback
│   └── export/                 # Exportador de LUTs C++ alignas(16) y reportes JSON
└── exported_luts/              # Carpeta de salida de sesiones, audios WAV y paquetes .abdlabtest
```

---

## 📚 Documentación Técnica Detallada

* [Roadmap de Desarrollo](docs/ROADMAP.md)
* [Arquitectura del Sistema](docs/ARCHITECTURE.md)
* [Protocolos de Hardware](docs/HARDWARE_PROTOCOLS.md)
* [Fundamentos Matemáticos y Modelos DSP](docs/MATHEMATICAL_MODELS.md)
* [Plan de Calidad y Pruebas (QA)](docs/QA_TEST_PLAN.md)
* [Documento de Traspaso Técnico (Handoff)](docs/HANDOFF.md)
