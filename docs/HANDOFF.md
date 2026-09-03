# Documento de Traspaso Técnico (HANDOFF) — ABDAudioLab

**Proyecto:** ABDAudioLab — Universal Black-Box Musical Hardware Profiler  
**Versión Actual:** 1.0.0 (Fase 1 Completada)  
**Fecha:** 2026-09-01  
**Autor:** Antigravity AI / ABDSynths  

---

## 1. Resumen del Proyecto y Objetivo

**ABDAudioLab** es una plataforma y banco de pruebas científico para ingeniería inversa y perfilado *Black-Box* y *Grey-Box* de hardware musical (sintetizadores, módulos Eurorack, efectos de guitarra). Inyecta estímulos de audio acústicamente calibrados (barridos exponenciales de Farina, impulsos Dirac Delta, rampa de ondas senoidales), captura la señal de respuesta en un búfer circular *lock-free* disparado por umbral, y calcula mediante análisis estadístico:
1. **La Media ($\mu$)**: Comportamiento de control estable (frecuencia de corte, tiempos ADSR, curvas de distorsión).
2. **La Desviación Estándar ($\sigma$)**: Variabilidad caótica orgánica, deriva térmica o inestabilidad analógica (ACB).
3. **Tablas de Búsqueda (LUTs)**: Archivos de cabecera `.h` con estructuras `alignas(16) static const AbdBatchedPoint` para emuladores en tiempo real (JUCE / `chowdsp_utils`).

---

## 2. Instrucciones de Compilación y Ejecución

### Requisitos del Sistema
- **Sistema Operativo**: Windows 10/11 (64-bit).
- **Compilador**: Microsoft Visual Studio 2022 o Visual Studio 2026 (MSVC con soporte C++20).
- **CMake**: Versión 3.22 o superior.
- **Tarjeta de Sonido**: Compatible con Windows Audio / WASAPI (modo exclusivo recomendado a 24-bit / 96 kHz o 48 kHz).

### Compilación Rápida (Build Script)
Para compilar en modo Release:
```cmd
./build.bat
```

Para limpiar y recompilar:
```cmd
./build.bat clean
./build.bat
```

Para compilar y lanzar la aplicación automáticamente:
```cmd
./build.bat run
```

### Ubicación del Ejecutable
El binario generado se ubica en:
```
build/ABDAudioLab_artefacts/Release/ABDAudioLab.exe
```

---

## 3. Estructura del Código Fuente

```
ABDAudioLab/
├── CMakeLists.txt              # Configuración CMake, JUCE 8.0.4 y dependencias
├── build.bat                   # Script de compilación automática MSVC
├── ESPECIFICACIONES_LABORATORIO.md # Especificación técnica normativa
├── docs/
│   ├── ROADMAP.md              # Mapa de ruta y fases del proyecto
│   ├── HANDOFF.md              # Este documento de traspaso técnico
│   └── google ia research/     # Papers (Farina, Wiener-Hammerstein) y proyectos de referencia
├── src/
│   ├── main.cpp                # GUI Standalone, consola de control y monitor de logs
│   ├── audio/                  # Motor de audio en tiempo real
│   │   ├── LabAudioEngine.h/.cpp       # Callback de audio, AudioDeviceManager y tono 1kHz
│   │   ├── LabStimulusGenerator.h/.cpp # Generador de estímulos (Farina, Dirac, Ruido)
│   │   └── LabAudioReceiver.h/.cpp     # Receptor lock-free (FIFO) y trigger por umbral
│   ├── hardware/               # Capa de abstracción de hardware
│   │   ├── HardwareController.h        # Interfaz abstracta pura IHardwareController
│   │   ├── MockHardwareController.h    # Simulación DSP en memoria para tests automáticos
│   │   ├── AiraSysExController.h       # Controlador Roland AIRA (SysEx RQ1/DT1 y CC)
│   │   ├── MidiCcController.h          # Controlador genérico MIDI CC
│   │   └── ManualAnalogueController.h  # Controlador interactivo para Eurorack manual
│   ├── math/                   # Motor matemático y estadístico
│   │   ├── FarinaDeconvolver.h/.cpp    # Deconvolución logarítmica y THD %
│   │   └── LabAnalyticEngine.h/.cpp    # Extracción de (µ, σ) para los 5 bloques funcionales
│   ├── core/                   # Secuenciador y perfiles
│   │   ├── ProfilingSession.h/.cpp     # Generador de matrices y parser de JSON
│   │   └── ProfilingSequencer.h/.cpp   # Máquina de estados en background thread
│   └── export/                 # Generación de código
│       └── LutExporter.h/.cpp          # Exportador de .h C++ (alignas 16) y .json
└── exported_luts/              # Carpeta de salida de LUTs y reportes generados
```

---

## 4. Principios y Reglas Críticas de Diseño

1. **Zero-Allocation en el Hilo de Audio**:
   - `LabStimulusGenerator::processBlock` y `LabAudioReceiver::processBlock` no reservan memoria dinámica en tiempo de ejecución. Los búferes están preasignados en `prepare()`.
2. **Protección contra Denormales**:
   - Todo callback de audio inicia con `juce::ScopedNoDenormals noDenormals;` (`RNF-14`).
3. **Entropía Segura y Generadores Deterministas**:
   - Prohibido el constructor por defecto de `juce::Random` en el hilo de audio. Se utiliza un generador lineal congruencial (LCG) determinista con semilla fija (`RNF-15`).
4. **Cadena de Inicialización de Audio de 3 Pasos**:
   - `AudioDeviceManager` intenta restaurar XML $\rightarrow$ prueba dispositivos por defecto $\rightarrow$ recurre a configuración básica (`RNF-16`).
5. **Aislamiento de Controladores**:
   - La máquina de estados (`ProfilingSequencer`) desconoce si interactúa con un sintetizador digital, una simulación o un operador humano; solo llama a `IHardwareController`.

---

## 5. Próximos Pasos y Roadmap Técnico

1. **Calibración de Línea Loopback**: Conectar interfaz con cable patch (DAC Out 1 -> ADC In 1) y ejecutar el asistente de auto-trim.
2. **Pruebas y Perfilado de Hardware Real**: Conectar Roland AIRA u otros sintetizadores para perfilado automático/asistido.
3. **Automatización MIDI (Ítem 1.7.13)**: Automatización de notas MIDI/CCs/Sysex para sintetizadores sin inyección de estímulo interno.
4. **Calidad de Código, Unit Tests & Real-Time Hardening (Ítem 1.7.14)**:
   - Reemplazar fallbacks estáticos en `LabAnalyticEngine` por métricas analíticas 100% reales.
   - Implementar suite de Unit Tests en `src/tests/` (Catch2/CTest).
   - Documentación Doxygen completa de encabezados públicos.
   - Auditoría estricta `Zero Heap Allocation` en el hilo de audio real-time.
