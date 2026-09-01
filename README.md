# ABDAudioLab — Universal Black-Box Hardware Profiler & Measurement Robot

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![JUCE](https://img.shields.io/badge/JUCE-8.0.4-orange.svg)](https://juce.com/)
[![CMake](https://img.shields.io/badge/CMake-3.22%2B-green.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-Proprietary-red.svg)]()

**ABDAudioLab** es una plataforma y banco de pruebas científico para ingeniería inversa y perfilado *Black-Box* y *Grey-Box* de hardware musical (sintetizadores, módulos Eurorack y efectos). 

Automatiza la inyección de estímulos acústicamente calibrados (barridos exponenciales de Farina, impulsos Dirac Delta, rampa lineal y secuencias de sincronización multi-tono), captura la respuesta en un búfer circular *lock-free* disparado por umbral, y calcula mediante análisis estadístico:
1. **La Media ($\mu$)**: Curva de respuesta y comportamiento de control nominal estable (Hz, ms, dB).
2. **La Desviación Estándar ($\sigma$)**: Factor de deriva térmica, ruido analógico o inestabilidad caótica (*Analog Circuit Behavior / ACB*).
3. **Tablas de Búsqueda (LUTs)**: Archivos `.h` con estructuras `alignas(16) static const AbdBatchedPoint` optimizadas para SIMD y compatibles con emuladores en tiempo real (`chowdsp_utils`).

---

## 🚀 Características Principales

* **Capa de Abstracción de Hardware (`IHardwareController`)**:
  * **Mock Virtual-Analog DSP**: Modelo interno de filtro resonante de 4 polos con saturación $\tanh$ y ruido térmico simulado para auto-tests y CI/CD.
  * **Roland AIRA Modular (SysEx/CC)**: Soporte completo de tramas `RQ1`/`DT1`, los 31 submódulos internos y matriz de ruteo virtual (`10 20 ss dd`).
  * **Generic MIDI CC**: Controlador parametrizable para sintetizadores hardware estándar.
  * **Manual Eurorack (Operator-Assisted)**: Modo guiado para módulos analógicos puros con confirmación por teclado (**Barra Espaciadora**).
* **Generador de Estímulos en Tiempo Real (`LabStimulusGenerator`)**:
  * Silencio de limpieza, Dirac Delta, Ruido Blanco determinista (LCG), Ruido Rosa (Voss-McCartney), Tono puro 1 kHz, Onda Cuadrada 1 kHz, Rampa Lineal y Barrido Logarítmico de **Farina (2000)**.
  * Secuencia de **Pre-Roll de 3 Tonos (`SyncPulses3`)** para alineación *sample-accurate*, medición de latencia y comprobación de *gain staging*.
* **Receptor de Audio Lock-Free (`LabAudioReceiver`)**:
  * Búfer circular preasignado (5s @ 96 kHz) gobernado por `juce::AbstractFifo` con disparo por umbral ($-40\text{ dBfs}$) para neutralizar retardos de la tarjeta de sonido.
* **Motor Analítico & Desconvolución (`LabAnalyticEngine` & `FarinaDeconvolver`)**:
  * Filtro inverso a $-6\text{ dB/oct}$, FFT con ventana de Hann, separación de armónicos no lineales ($\Delta t_N = T \frac{\ln N}{\ln(\omega_2/\omega_1)}$) y cálculo de THD %.
* **Consola de Control Standalone (`src/main.cpp`)**:
  * Configuración completa de dispositivos de **Audio y Puertos MIDI (In/Out)**.
  * Monitor de logs en tiempo real con marcas de tiempo y barra de progreso.

---

## 🛠️ Requisitos del Sistema y Compilación

### Requisitos
* **Sistema Operativo**: Windows 10 / 11 (64-bit).
* **Compilador**: Microsoft Visual Studio 2022 o Visual Studio 2026 (MSVC con soporte C++20).
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
├── ESPECIFICACIONES_LABORATORIO.md # Especificación técnica normativa
├── README.md                   # Este documento
├── docs/                       # Suite completa de documentación técnica
│   ├── ROADMAP.md              # Mapa de ruta y fases del proyecto
│   ├── HANDOFF.md              # Documento de traspaso técnico para desarrolladores
│   ├── ARCHITECTURE.md         # Arquitectura detallada, capas y reglas de tiempo real
│   ├── HARDWARE_PROTOCOLS.md   # Especificaciones SysEx Roland, MIDI CC y FSK
│   ├── MATHEMATICAL_MODELS.md  # Fórmulas Farina, Wiener-Hammerstein y estadísticas
│   ├── QA_TEST_PLAN.md         # Matriz de casos de prueba y aseguramiento de calidad
│   └── google ia research/     # Papers científicos y proyectos de referencia
├── src/
│   ├── main.cpp                # Consola GUI, configuración Audio/MIDI y secuenciador
│   ├── audio/                  # Motor de audio en tiempo real (zero-allocation)
│   ├── hardware/               # Abstracción de hardware y controladores
│   ├── math/                   # Desconvolución FFT y motor estadístico (µ, σ)
│   ├── core/                   # Secuenciador y gestión de perfiles de prueba
│   └── export/                 # Exportador de LUTs C++ alignas(16) y reportes JSON
└── exported_luts/              # Carpeta de salida de LUTs y reportes generados
```

---

## 📚 Documentación Técnica Detallada

* [Especificaciones Normativas del Laboratorio](file:///ESPECIFICACIONES_LABORATORIO.md)
* [Roadmap de Desarrollo](file:///docs/ROADMAP.md)
* [Arquitectura del Sistema](file:///docs/ARCHITECTURE.md)
* [Protocolos de Hardware (Roland SysEx / MIDI CC)](file:///docs/HARDWARE_PROTOCOLS.md)
* [Fundamentos Matemáticos y Modelos DSP](file:///docs/MATHEMATICAL_MODELS.md)
* [Plan de Calidad y Pruebas (QA)](file:///docs/QA_TEST_PLAN.md)
* [Documento de Traspaso Técnico (Handoff)](file:///docs/HANDOFF.md)
