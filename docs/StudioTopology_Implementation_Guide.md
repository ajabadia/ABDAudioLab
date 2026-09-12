# Guía de Implementación: Módulo StudioTopology (ABDSharedCode)

El módulo **`StudioTopology`** es un componente compartido para visualización interactiva de interconexiones de audio, MIDI y hardware en el ecosistema ABDSynths (utilizado en `ABDAudioLab`, hosts y plugins).

---

## 1. Filosofía de Diseño: Visor Aséptico vs. Llamante Inteligente

El principio fundamental del módulo es la **separación estricta de responsabilidades**:

```
 ┌─────────────────────────────────────────────────────────────┐
 │                    Llamante (Host / App)                   │
 │  - Lee drivers y puertos físicos/virtuales del SO (JUCE/OS) │
 │  - Determina asignación activa (Audio In/Out, MIDI In/Out)   │
 │  - Conoce las capacidades del Target (DUT: Pedal, Sintetizador)│
 │  - Construye el payload JSON normalizado                   │
 └──────────────────────────────┬──────────────────────────────┘
                                │ updateTopology(payload)
                                ▼
 ┌─────────────────────────────────────────────────────────────┐
 │                Visor Aséptico (StudioTopology)              │
 │  - Motor puramente gráfico (HTML5 + SVG + WebGL/Canvas)     │
 │  - No consulta el sistema ni asume reglas de hardware       │
 │  - Posicionamiento orbital con relajación anti-colisiones   │
 │  - Conecta cables según 'connections' recibidos             │
 │  - Oculta jacks si el nodo especifica `hasMidi: false`     │
 │  - Muestra nodos en color (assigned) o gris (unassigned)    │
 └─────────────────────────────────────────────────────────────┘
```

---

## 2. Estructura de Archivos del Módulo

Dentro del repositorio `ABDSharedCode`:

```
ABDSharedCode/
└── StudioTopology/
    ├── CMakeLists.txt                      # Target ABDShared::StudioTopologyAssets
    ├── JuceStudioTopologyComponent.h       # Componente JUCE que aloja el WebBrowserComponent (WebView2)
    ├── StudioTopologyFloatingWindow.h      # Ventana flotante desacoplada con gestión de temas
    ├── StudioTopologyResourceProvider.h    # Bridge de recursos empaquetados en BinaryData
    ├── StudioTopologyResourceProvider.cpp
    ├── README.md                           # Especificación y guía rápida
    └── resources/                          # Assets Web (compilados a binario con juce_add_binary_data)
        ├── index.html                      # Layout base (header, canvas SVG, capa de nodos)
        ├── style.css                       # Estilos temáticos claro/oscuro, tarjetas con contraste
        ├── topology.js                     # Motor SVG dinámico y layout físico anti-colisión
        ├── interfaces/                     # Imágenes de interfaces de audio/MIDI
        │   ├── generic-audio-midi-interface.png
        │   ├── presonus-audiobox-usb.png
        │   └── roland-mx-1.png
        └── models/                         # Imágenes estándar de sintetizadores/pedales/racks
```

---

## 3. Contrato de Datos (JSON Payload)

El llamante debe suministrar un objeto JSON a través de `updateTopology(const nlohmann::json& payload)` o `setTopologyJson(juce::String)`:

```json
{
  "target": {
    "name": "BOSS DS-1 Distortion",
    "category": "ANALOGUE_PEDAL",
    "details": "Submódulo: Clipper Diodes",
    "image": "models/boss-ds1.png",
    "hasAudio": true,
    "hasMidi": false
  },
  "devices": [
    {
      "id": "dev_0",
      "name": "PreSonus AudioBox USB",
      "details": "Detectada en Windows",
      "image": "interfaces/presonus-audiobox-usb.png",
      "assigned": false,
      "hasAudio": true,
      "hasMidi": true
    },
    {
      "id": "dev_1",
      "name": "Realtek High Definition Audio",
      "details": "Audio Asignado",
      "image": "interfaces/generic-audio-midi-interface.png",
      "assigned": true,
      "hasAudio": true,
      "hasMidi": false
    },
    {
      "id": "dev_2",
      "name": "LoopBe1 - Internal Midi Port",
      "details": "Driver Virtual MIDI",
      "image": "interfaces/generic-audio-midi-interface.png",
      "assigned": false,
      "hasAudio": false,
      "hasMidi": true
    }
  ],
  "connections": [
    { "type": "audioOut", "from": "dev_1", "to": "target" },
    { "type": "audioIn",  "from": "target", "to": "dev_1" }
  ]
}
```

### Reglas del Contrato:
1. **`assigned`**:
   - `true`: El nodo se dibuja a **pleno color** con borde acentuado.
   - `false`: El nodo se dibuja en **escala de grises** con opacidad reducida (`opacity: 0.45`), indicando que está conectado al PC pero no activo en la sesión.
2. **`hasMidi`**:
   - Si es `false`, el visor no genera los conectores MIDI Out / MIDI In en dicho nodo.
3. **`connections`**:
   - Tipos soportados: `"audioOut"`, `"audioIn"`, `"midiOut"`, `"midiIn"`.
   - Si el target no posee MIDI, el llamante **nunca** debe emitir cables MIDI, aun cuando haya interfaces con puertos MIDI asignados.

---

## 4. Algoritmo de Distribución y Anti-Colisión (`topology.js`)

El visor ejecuta una distribución radial de dos niveles y un paso de relajación física:
1. **Target Hero**: Se ubica en el centro exacto `(centerX, centerY)`.
2. **Dispositivos Asignados (`assigned: true`)**: Se ordenan en un arco superior de radio $R_1 \approx 230\text{ px}$.
3. **Dispositivos No Asignados (`assigned: false`)**: Se colocan en un arco inferior/exterior de radio $R_2 \approx 320\text{ px}$.
4. **Paso de Relajación (Anti-colisión)**:
   - Para evitar que dispositivos con textos largos o dimensiones variables se solapen, se ejecuta un bucle iterativo de distancias:
   - Si $\Delta x < 220\text{ px}$ y $\Delta y < 170\text{ px}$, ambos nodos se separan proporcionalmente. El nodo central (`target`) permanece fijo.

---

## 5. Diseño Visual y Contraste (`style.css`)

Para asegurar visibilidad óptima independientemente del color del hardware o del tema:
- Cada imagen está encapsulada en `.node-image-container` con fondo neutro translúcido:
  - **Modo Oscuro**: `rgba(255, 255, 255, 0.08)` con borde sutil `rgba(255, 255, 255, 0.07)`. Evita que interfaces o pedales de color negro se fundan con el fondo oscuro.
  - **Modo Claro**: `rgba(0, 0, 0, 0.04)` con borde `rgba(0, 0, 0, 0.08)`.
- Se admiten los selectores de tema `[data-theme="audiolab"]` y `[data-theme="audiolab-light"]`.

---

## 6. Integración en Proyectos CMake

En el `CMakeLists.txt` de la aplicación consumidora:

```cmake
# 1. Vincular librería de assets compartidos
target_link_libraries(TuApp
    PRIVATE
        ABDShared::StudioTopologyAssets
)

# 2. Habilitar WebView2 en Windows
target_compile_definitions(TuApp
    PRIVATE
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
)

# 3. Incluir fuentes del módulo
target_sources(TuApp
    PRIVATE
        ${ABDSHARED_CODE_DIR}/StudioTopology/StudioTopologyResourceProvider.h
        ${ABDSHARED_CODE_DIR}/StudioTopology/StudioTopologyResourceProvider.cpp
        ${ABDSHARED_CODE_DIR}/StudioTopology/JuceStudioTopologyComponent.h
        ${ABDSHARED_CODE_DIR}/StudioTopology/StudioTopologyFloatingWindow.h
)
```

---

## 7. Ejemplo de Uso en C++

```cpp
#include <StudioTopology/StudioTopologyFloatingWindow.h>

// En la clase controladora:
std::unique_ptr<abd::topology::StudioTopologyFloatingWindow> topologyWindow;

void showTopology()
{
    if (!topologyWindow)
        topologyWindow = std::make_unique<abd::topology::StudioTopologyFloatingWindow>();

    nlohmann::json root = buildStudioTopologyJson(); // Función del llamante con datos reales del SO

    topologyWindow->setTheme(isDark ? "audiolab" : "audiolab-light", juce::Colours::black);
    topologyWindow->updateTopology(root);
    topologyWindow->setVisible(true);
    topologyWindow->toFront(true);
}
```
