# Plan Maestro: Transformación UX SoundID (Flujo Guiado y Reducción Cognitiva)

## Diagnóstico y Principio de Seguridad

> **Lección de la refactorización anterior:** NUNCA se destruye o reescribe código en vivo antes de tener su reemplazo probado y enlazado.  
> Toda la maquinaria interna (`ProfilingSequencer`, `LabAnalyticEngine`, `HardwareManager`, `LutExporter`, etc.) está blindada por 115 tests pasando al 100%.  
> Esta transformación es **estrictamente de presentación y flujo de usuario (UX)**: construimos los nuevos componentes al lado de los actuales, los probamos de forma aislada e incremental, y solo los conmutamos en la vista principal cuando están 100% operativos.

---

## Estrategia de Implementación Segura (Side-by-Side)

En lugar de demoler `MainContentComponent.cpp`, adoptamos una **estrategia aditiva y reversible**:

1. **Mantener el ejecutable y los tests siempre verdes:** Cada paso termina con compilación en Release y paso de la suite de 115 tests.
2. **Creación de componentes atómicos nuevos** bajo `src/gui/soundid/`:
   - `SoundIdSidebarStepper`: Barra vertical colapsable (expandida ~240px / colapsada ~56px) con números/iconos, checks verdes y tooltips flotantes.
   - `SoundIdHardwareCatalogSelector`: Selector en cascada (`Tipo ➔ Marca ➔ Modelo`) con lectura directa de contratos JSON.
   - `SoundIdActiveProfilingView`: Pantalla de medición inspirada en *SoundID Measure* (indicador visual grande, cuenta atrás de puntos, barra de progreso limpia y tarjetas de perillas manuales).
   - `SoundIdResultsDashboard`: Ficha de resultados final limpia con el botón verde de 1 clic `Export Production Package`.
3. **Conmutador de Shell en `MainContentComponent`**:
   - Se mantiene el arnés de eventos y controladores (`sessionCoordinator`, `sessionManager`, `audioEngine`).
   - Se introduce un flag o switch de layout para conectar los nuevos paneles paso a paso, asegurando que ninguna llamada o callback existente se rompa.

---

## Fases Secuenciales de Ejecución

### Fase 1: Limpieza Rápida de Tipografía y Caracteres (Quick Wins)
- [ ] **Codificación UTF-8 en todo el proyecto:** Reparar de raíz las cadenas con tildes y caracteres rotos (`CalibraciÃ³n`, `SEÃ‘AL`, `âž¢`, etc.) en `LoopbackCalibrationModal.cpp`, `AudioABVerificationModal.cpp` y paneles de estado.
- [ ] **Despeje de Cabecera en `PlotterFrequencyCurveRenderer`:** Asegurar que los badges `Mean (μ)`, `±1σ` y `THD %` se posicionen a la derecha o con márgenes amplios, eliminando por completo el solapamiento sobre el texto `Parameter & Response Curves`.
- [ ] **Prevención de Pantalla Negra:** Configurar que la vista inicial por defecto del visor sea la Curva 2D (`ViewMode::FrequencyCurve`) con rejilla limpia de frecuencias, en lugar del modo `3D Mountains` vacío.
- [ ] *Verificación Fase 1:* Compilación Release de `ABDAudioLab` y `ABDAudioLab_Tests` (115/115 pasando).

---

### Fase 2: Componente `SoundIdSidebarStepper` (Barra Vertical Colapsable)
- [ ] **Diseño y Maquetación:**
  - Modo Expandido (240px): 4 pasos verticales con círculos numerados ①..④, etiquetas limpias en tipografía moderna, línea conectora vertical y badge de estado (`Completado ✓`, `Activo ●`, `Pendiente ○`).
  - Modo Colapsado (56px): Carril minimalista solo con los 4 círculos e iconos. Botón de toggle `◀` / `▶`.
  - Tooltips flotantes contextuales al hacer hover sobre los círculos colapsados (estilo *Glassmorphism* / tarjeta oscura SoundID).
  - En la parte inferior: **Ficha fija de resumen de sesión** (Hardware, perfil, sample rate, estado).
- [ ] **Test Unitario:** Crear `src/tests/test_SoundIdSidebarStepper.cpp` verificando transiciones de colapso, callbacks de selección de paso y persistencia de estado.
- [ ] *Verificación Fase 2:* CTest en verde.

---

### Fase 3: Selector de Hardware en Cascada (Tipo ➔ Marca ➔ Modelo)
- [ ] **Extracción y Clasificación desde Contratos:**
  - Leer dinámicamente del `HardwareContractRegistry`:
    1. **Categoría / Tipo**: *Sintetizador*, *Pedal de Efecto*, *Módulo Eurorack / VCF*, *Procesador de Rack*, *Recinto / Altavoz*.
    2. **Marca**: *Roland, Korg, Behringer, BOSS, Yamaha, Casio...*
    3. **Modelo**: *MS2000, Juno-106, DS-1, CZ-101...*
- [ ] **Construcción de `SoundIdHardwareCatalogSelector`:**
  - Componente autónomo con 3 combos o botones de navegación limpios.
  - Elimina los textos técnicos (`(AUTOMATED_MIDI_CC)`, `(MOCK_DSP)`).
  - Muestra la fotografía oficial del modelo, la descripción funcional y el esquema de conexión en una única tarjeta elegante sin popups intermedias.
  - El botón "Auto-Detect Device" realiza la detección silenciosa y selecciona el modelo correspondiente sin abrir ventanas modales.
- [ ] **Selector de Función / Objetivo de Medición:**
  - Una vez elegido el modelo, lista sus presets/recetas (ej: *Filtro VCF*, *Curva de Distorsión Tone*, *Envolvente ADSR* o *Modo Libre / Avanzado*).
- [ ] **Test Unitario:** Crear `src/tests/test_SoundIdHardwareCatalogSelector.cpp`.

---

### Fase 4: Vista de Medición Guiada y Ficha Congelada (`SoundIdActiveProfilingView`)
- [ ] **Ficha Congelada (Solo Lectura):**
  - Una vez confirmado el hardware y objetivo, los selectores se bloquean. La barra lateral muestra la ficha fija inmutable.
- [ ] **Vista Central de Medición:**
  - Para hardware MIDI automatizado:
    - Rejilla de curva limpia + Indicador SoundID grande ("Measuring in progress...").
    - Contador claro: *"Punto 14 de 32 | Restantes: 18"* + Barra de progreso esmeralda.
    - Botones limpios: `[ PAUSAR ]` y `[ CANCELAR ]`.
  - Para hardware manual (Pedales / Eurorack):
    - El **Inspector de Perillas Virtuales** pasa al frente como protagonista (indicando los ángulos de *TONE*, *LEVEL*, *DIST* para el punto actual).
    - Botón verde principal: **`[ MEDIR PUNTO (Espacio) ]`**.
- [ ] *Verificación Fase 4:* CTest en verde y compilación en Release.

---

### Fase 5: Pantalla de Resultados y Exportación de 1-Clic (`SoundIdResultsDashboard`)
- [ ] **Panel de Resultados Estilo SoundID:**
  - Título: *"Resultados: Curva de respuesta y caracterización completada"*.
  - Curva de respuesta en frecuencia / función de transferencia limpia en alta definición.
  - Tarjeta de métricas acústicas (`SNR`, `Piso de Ruido`, `THD % Medio`).
  - Botón verde protagonista: **`[ EXPORT PRODUCTION PACKAGE (1-CLICK) ]`** y botón secundario **`[ Ver Informe HTML ]`**.
  - Ocultar la llamada a la API Cloud o moverla a un menú discreto de ajustes para evitar advertencias en rojo.
- [ ] *Verificación Fase 5:* CTest en verde y compilación en Release.

---

### Fase 6: Enlace Maestro, Validación Visual y Retirada Segura de Elementos Antiguos
- [ ] **Conexión en `MainContentComponent`:** Conectar el `SoundIdSidebarStepper` y los nuevos paneles guiados al orquestador `SessionExecutionCoordinator`.
- [ ] **Prueba de Fuego:** Probar el flujo completo de principio a fin (desde la selección de un sinte o pedal hasta la generación del paquete C++ / JSON).
- [ ] **Limpieza de Código Muerto:** Solo una vez que el nuevo flujo esté completamente verificado y probado por el usuario, retirar los modales antiguos redundantes.

---

## Verificación Continua

| Métrica | Condición de Aprobación |
|---|---|
| **Compilación** | Cero advertencias críticas en MSVC v144 Release |
| **Suites CTest** | 115/115 suites pasando (134.221+ aserciones) |
| **Estabilidad de Audio** | Cero heap allocations en hilo de audio tiempo real |
| **Ergonomía UI** | Cero modales anidados (efecto matrioshka), cero solapamientos de texto |
