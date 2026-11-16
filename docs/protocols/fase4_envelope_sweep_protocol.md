# Fase 4 — Protocolo Operacional para el Barrido Físico de la Curva de Envolvente del Filtro

Estado del Documento: Manual Técnico Sincronizado y Auditado. Confirmada la existencia física de los módulos analíticos y de persistencia en disco.

Este manual define el procedimiento de instrumentación para resolver de forma empírica el Defecto 5 (desvío del byte neutro 128 en las curvas variables de las envolventes). El sistema opera bajo la arquitectura nativa de excitación y orquestación del laboratorio, actuando de forma aislada sobre el sintetizador objetivo (Behringer DeepMind).

Objetivo medido: respuesta en frecuencia del filtro bajo control de un parámetro de curva de envolvente, no la validación completa de un ADSR general.

## Resumen de Arquitectura de la Fase 4

- **Fidelidad de Datos**: Los resultados se analizan mediante una regresión lineal por mínimos cuadrados para determinar el coeficiente de determinación \(R^{2}\). Un valor de \(R^2 \approx 1.0\) en el byte 128 puede usarse como indicador de linealidad pura de la etapa medida, siempre que el diseño experimental controle latencia, excitación y calibración.
- **Aislamiento de Entornos**: ABDAudioLab gobierna las mediciones y exporta los reportes estructurados localmente a su propio directorio de artefactos (`build/ABDAudioLab_artefacts/CapturedSweeps/`). ABDeep actúa únicamente como consumidor pasivo de copias estáticas (fixtures), erradicando dependencias cruzadas en tiempo de compilación.

## Modo 1: Modo Matemático Offline (Validación Sintética)

Este modo se ejecuta sin hardware físico conectado y sirve para auditar la estabilidad matemática del motor de regresión y del exportador de reportes en el entorno de C++20 / JUCE 8.

Nota de estado: en este repositorio existe `src/math/LabEnvelopeCurveAnalyzer` y el exportador de barrido `src/export/CurveReportExporter`, y ambos están declarados en el target de tests junto con `src/math/UnitTests/EnvelopeAnalysisTests.cpp`. Esos nombres ya no son propuesta de diseño: son los componentes reales del flujo offline de la Fase 4 y se prueban antes de tocar el hardware.

### Flujo de Ejecución

1. **Generación de Datos**: La suite de Catch2 genera vectores de prueba sintéticos (`EnvelopePoint`) que simulan trayectorias de Hercios ideales (rampas perfectas, líneas planas y caídas con saturación).
2. **Cálculo de Regresión**: El módulo activo `LabEnvelopeCurveAnalyzer::analyzeTrajectory` procesa el dataset calculando la pendiente (\(m\)), intercepto (\(c\)), residuos muestra a muestra (\(Y_{medido} - Y_{teorico}\)) y el coeficiente \(R^{2}\).
3. **Detección de Breakpoint**: El analizador identifica la saturación analógica evaluando la desaceleración de la pendiente local (\(\Delta Hz / \Delta t \to 0\)) combinada con un signo persistente y sostenido en los residuos, marcando el tiempo exacto en segundos.
4. **Emisión del Reporte**: Se genera un informe estructural compatible con las aserciones de integración.

### Comando de Validación Local

Para compilar y correr de forma aislada las aserciones sintéticas antes de tocar el hardware, ejecuta en tu terminal de PowerShell:

```powershell
cmake --build build --target ABDAudioLab_Tests --config Release
.\build\ABDAudioLab_artefacts\Release\ABDAudioLab_Tests.exe "[math][analysis][envelope]"
```

## Modo 2: Modo Físico (Medición Real sobre Hardware)

Este modo se activa cuando el sintetizador físico está conectado a la interfaz de sonido, delegando la captura al framework de medición y la generación al sintetizador.

### Inicialización y Preparación (ISynthTarget::prepare)

Antes del disparo de la secuencia, el método `DeepMindHardwareTarget::prepare` recibe el objeto `ProcessingSpec` e inicializa el circuito analógico del filtro mediante ráfagas MIDI NRPN de 14 bits para convertirlo en un oscilador puro libre de modulaciones cíclicas:

- **VCF Cutoff** → 0 (Filtro totalmente cerrado).
- **VCF Resonance** → 16383 (Máximo fader NRPN) (Auto-oscilación sinusoidal pura).
- **VCF Env Depth** → 16383 (Máximo) (Apertura total de la rampa).
- **Env 2 Attack** → 0 (Ataque instantáneo).
- **Env 2 Sustain** → 0 (Caída total hasta el suelo del filtro).
- La transferencia de estado binario inicial se opera mediante los métodos nativos de vector de bytes `getState` y `setState` cumpliendo el contrato de `StateTransferResult`.

Nota de verificación: el ID de NRPN del parámetro de curva tiene que confirmarse en el sintetizador objetivo antes de usarlo. El documento usa como referencia ilustrativa el índice 58 (`ENV2_ATTACK_CURVE`), pero ese número debe validarse con medición o documentación del hardware real.

Nota de artefacto de salida: en este repositorio el exportador de barrido vive en `src/export/CurveReportExporter` y la suite de aceptación sintética en `src/math/UnitTests/EnvelopeAnalysisTests`. Ambos ya están declarados en el CMakeLists del target de tests y enlazan con `src/math/LabEnvelopeCurveAnalyzer`. Por eso el flujo offline usa `CurveReportExporter`, no `LutExporter` ni `ExportSessionManifest` con `MeasuredPoint`.

### Ejecución de la Secuencia de Excitación (ISynthTarget::render)

El flujo de estímulos musicales no se opera de forma manual por pasos artesanales, sino que se encapsula en una `MidiExcitationSequence` canónica con sus parámetros de temporización reales (`gateDurationSec`, `preSilenceSec`, `postSilenceSec`, `totalDurationSec`) y hashes de consistencia (`sequenceHash`).

Nota de implementación: la forma de variar el byte de curva entre tomas puede hacerse interpretando el `repetitionIndex` o los `parameterEvents` de la secuencia activa, pero esa decisión es de diseño del adaptador, no un contrato garantizado por el framework. Para reproducibilidad, elige una construcción de `MidiExcitationSequence` y ajústala antes de lanzar capturas; por ejemplo, usa `createNoteTrial`, `createCanonicalNoteTrial`, o una secuencia con `parameterEvents`, según lo que quieras reproducir.

1. **Inyección del Parámetro**: El adaptador determina el byte de curva actual (0, 64, 128, 192, 255) para la toma activa y envía los 4 bytes de control MIDI NRPN correspondientes al parámetro de hardware de curva de envolvente.
2. **Secuenciación de Audio**: El framework de medición orquesta el ciclo de vida síncrono invocando a `ComplexEnvelopeOrchestrator::orchestrate` pasando la configuración de captura (`OrchestratorConfig`).
3. **Captura de Audio**: El receptor `LabAudioReceiver` se arma mediante `armCapture()` o `armWithRequirements()`, reacciona por umbral de amplitud, gestiona los estados de la FIFO (`AbstractFifo`) y compensa de manera exacta la latencia en muestras de los drivers de Windows (WASAPI/DirectSound).
4. **Extracción de Trayectoria**: El flujo de audio capturado se analiza mediante `PreScanSpectrumAnalyzer` (ejecutando `analyzeSaturationToneSweep()`), rastreando el pico espectral en Hz muestra a muestra para poblar los puntos de la trayectoria de la envolvente real.

### Salida y Consumo del Artefacto

- El analizador procesa la trayectoria analógica real extraída, calcula su métrica \(R^{2}\) y delega en `CurveReportExporter` la escritura del reporte JSON del barrido.
- Para un JSON de puntos con metadatos de barrido, usa el contrato del exportador que escribe campos como `schemaVersion`, `sweepMeta`, `regression`, `residuals`, `fittedHz`, `rawPoints` y `breakpoint`. El nombre del archivo JSON debe variar por byte medido y por run. Antes de automatizar capturas, decide la nomenclatura y los campos que distinguen un run de otro; por ejemplo: byte medido, tasa de muestreo, id o hash de la secuencia MIDI, modo de calibración y timestamp determinista o de run.
- El directorio de salida (`build/ABDAudioLab_artefacts/CapturedSweeps/`) puede crearse manualmente si no existe.
- **El Gancho de Entrega (The Hook)**: Para actualizar las pruebas de regresión de la interfaz web, el desarrollador toma una copia estática de ese JSON y la deposita manualmente en la carpeta de fixtures del frontend (`ABDEep/WebUI/tests/fixtures/hardware_snapshots/`), manteniendo ambos repositorios 100% independientes en tiempo de compilación.

Nota de uso del fixture: el archivo copiado a ABDeep debe tratarse como dato de prueba read-only y como resultado de medición, no como especificación de diseño.

Nota de datos medida: el formato `EnvelopeObservationPoint` del laboratorio tiene campos como valor, estado, confianza, unidad, metros de incertidumbre y referencias de normalización. Si el objetivo del barrido es una curva de frecuencia vs tiempo, habrá que decidir una representación o adaptador propio para esos puntos de trayectoria antes de empaquetarlos.



<details>
<summary>⚠️ Advertencia de Coherencia del Entorno (Verificación Local del Target de Tests)</summary>

Antes de iniciar cualquier flujo de compilación o pruebas físicas en el laboratorio, el operador debe tener en cuenta el estado de integración de las fuentes en el sistema de construcción de CMake:

• **Infraestructura Verificada en Disco**: Se confirma la existencia real y el sincronismo de los módulos base en el repositorio:
    • **Módulo Analítico**: `src/math/LabEnvelopeCurveAnalyzer.cpp` y su correspondiente cabecera `.h`.
    • **Módulo de Persistencia**: `src/export/CurveReportExporter.cpp` y su correspondiente cabecera `.h`.

• **Alineación con CMake**: El archivo `CMakeLists.txt` raíz ya declara e integra de forma oficial estos componentes dentro de las fuentes del proyecto.

• **Nota de Pre-Vuelo para el Target de Tests**: El script de CMake (target `ABDAudioLab_Tests`) incluye `src/math/LabEnvelopeCurveAnalyzer.cpp` y `src/math/UnitTests/EnvelopeAnalysisTests.cpp` como fuentes del test. Dado que la ruta original que figuraba en versiones anteriores del protocolo no existía (`src/tests/test_LabEnvelopeCurveAnalyzer.cpp` y un supuesto subdirectorio `src/tests/UnitTests/`), el operador debe verificar localmente la ubicación exacta del test antes de lanzar la compilación, para evitar trabas de dependencias insatisfechas o referencias a rutas obsoletas.

</details>
