# Guía de Diseño y Sistema Visual: Técnica Refinada (ABDSynths AudioLab)

Esta guía documenta la dirección de arte, sistema de diseño y especificación técnica de UI/UX para **ABDAudioLab** y los futuros módulos de la suite ABDSynths. Inspirada en la sobriedad, claridad y reducción de fatiga visual de **Sonarworks SoundID Reference** y **Audio Precision**, establece una identidad unificada basada en datos precisos, tipografía de alta legibilidad y jerarquía espacial limpia.

---

## 1. Filosofía de Diseño: Reducción de Carga Cognitiva

1. **Jerarquía Clara sobre Densidad Desordenada**: El software de medición técnica no debe intimidar ni fatigar. Cada dato debe tener un nivel de contraste y peso proporcional a su importancia en el flujo operativo.
2. **Cero Doble Modal**: Jamás se superpone un diálogo modal sobre otro. La configuración técnica, telemetría y créditos se integran en paneles laterales (*slide-in drawers*) o pestañas consolidadas.
3. **Semántica Estricta de Color**: Los colores son portadores de estado, no elementos decorativos.
   - **Esmeralda / Cian Técnico**: Acciones principales y estados nominales (OK / Running / Connected).
   - **Ámbar / Naranja Oscuro**: Advertencias y valores THD% (reemplaza al amarillo chillón).
   - **Rojo Carmesí Puro**: Saturación, clipping y desconexiones críticas.
   - **Gris Técnico Neutral**: Fondos, bordes de 1 px y textos secundarios.
4. **Paridad Multiplataforma**: La tipografía no depende de las fuentes instaladas en el sistema operativo cliente (Windows/macOS/Linux). Se incrustan las fuentes binarias oficiales.

---

## 2. Tokens de Diseño Global (`AppTheme`)

Todos los componentes deben consultar exclusivamente la estructura estática [`abdaudiolab::gui::AppTheme`](file:///d:/desarrollos/ABDSynths/ABDAudioLab/src/gui/AppTheme.h). Queda prohibido hardcodear códigos hexadecimales o instanciar `juce::Colour` ad-hoc en los métodos de renderizado.

### Paleta de Colores

| Token | Hex | Uso y Significado Semántico |
|---|---|---|
| `BackgroundApp` | `#F8F9FA` | Fondo base de la ventana principal. Reduce la fatiga ocular respecto al blanco puro. |
| `SurfaceCard` | `#FFFFFF` | Fondo de tarjetas centrales, contenedor de curvas, modales y listas. |
| `SurfaceSubtle` | `#F1F3F5` | Fondo de campos inactivos, chips de estado, celdas de tabla alternas. |
| `SurfaceHover` | `#E9ECEF` | Estado *hover* de filas, botones secundarios y elementos seleccionables. |
| `BorderSubtle` | `#E2E8F0` | Líneas divisorias de 1 px, bordes de contenedor y grillas sin sombras sucias. |
| `BorderCard` | `#CBD5E1` | Delimitación exterior de tarjetas y controles activos. |
| `TextPrimary` | `#1A1D20` | Texto principal, encabezados H1/H2 y valores de medición clave. |
| `TextSecondary` | `#6C757D` | Etiquetas de parámetros, metadatos y estados inactivos. |
| `TextMuted` | `#94A3B8` | Leyendas secundarias, notas de pie y atajos de teclado. |
| `AccentActive` | `#00A86B` / `#0F9D58` | Acción principal (Run Tests, Start Calibration), LEDs conectados (OK). |
| `AccentActiveHover` | `#008f5a` | Estado hover de acciones primarias. |
| `AccentWarning` | `#E65100` | THD%, avisos de señal baja, avisos de sobreescritura. |
| `AccentError` | `#D32F2F` | Clip analógico/digital, error de loopback, puerto desconectado. |

---

## 3. Tipografía Técnico-Modernista

Se estandariza el uso de dos familias tipográficas incrustadas mediante binarios en JUCE:

1. **Inter** (Google Fonts / Open Source):
   - Tipografía principal sans-serif de proporciones geométricas neutras.
   - **H1 (Títulos de Diálogos y Modales)**: `20px` - `24px`, SemiBold / Bold.
   - **H2 (Cabeceras de Secciones y Tarjetas)**: `14px` - `16px`, SemiBold.
   - **Body / Labels**: `12px` - `13px`, Regular / Medium.
   - **Micro Labels / Badges**: `10px`, Bold (con espaciado entre caracteres tracking +0.5px).

2. **Roboto Mono** o **JetBrains Mono**:
   - Tipografía monospaciada exclusiva para valores técnicos y telemetría:
   - Frecuencias (`Hz`, `kHz`), Niveles (`dBFS`, `dBu`), Latencias (`ms`, `samples`), Porcentajes THD (`%`) y código generado (`.h`).

---

## 4. Controles e Interactividad

### 4.1. Botones de Acción
- **Botón Primario (Hero Action)**:
  - Estilo píldora completa: radio de esquina = `height / 2.0f`.
  - Fondo `AccentActive` con texto en blanco bold.
  - Sin biseles pseudo-3D ni sombras difusas; iluminación plana con hover sutil (`darker(0.05f)` al pulsar).
- **Botones Secundarios (Secondary Action)**:
  - Borde sutil de 1 px (`BorderSubtle` o `AccentActive` según relevancia).
  - Fondo transparente o `SurfaceCard`, texto en `TextPrimary` o `AccentActive`.
  - Radio de esquina estándar: `6px` o pastilla según contexto.
- **Botones Terciarios / Enlaces**:
  - Sin borde ni fondo; solo texto en `TextSecondary` que pasa a `TextPrimary` en hover (ej: "Cancel").

### 4.2. Selectores Desplegables (ComboBox)
- Fondo plano `SurfaceCard`, borde de 1 px `BorderSubtle`.
- Radio de esquina: `6px`.
- Flecha desplegable: vector estilizado minimalista (triángulo equilátero hacia abajo de 6x4 px) dibujado en `juce::Path`, sin sombras duras.

### 4.3. Barras de Desplazamiento (Scrollbars)
- Ancho ultradelgado de `4px` a `6px`.
- Radio redondeado completo en el pulgar (*thumb*).
- Sin botones de flecha en los extremos para mantener pureza geométrica.

---

## 5. Distribución Espacial y Flujos Críticos

### 5.1. Barra Superior y Telemetría
- **Menú de Archivo**: Botón plano o icono menú accesible que abre un `juce::PopupMenu` estándar nativo (New, Open, Save, Save As, Export Report, Exit) de forma inmediata, sin cubrir toda la pantalla con una modal pesada.
- **Píldora de Telemetría IO (`IoStatusPill`)**: Contenedor único sobre fondo `SurfaceSubtle` con borde sutil. Cuatro indicadores LED (6 px) con etiquetas compactas: `A-IN`, `A-OUT`, `M-IN`, `M-OUT`. Tooltip flotante que expone el hardware exacto asignado.
- **Botón de Calibración**: Píldora plana e interactiva: `CALIBRATION: -3.0 dBFS`. Al hacer clic, despliega directamente el asistente interactivo.
- **Selector de Hardware**: Píldora elegante que muestra el dispositivo y módulo activo.

### 5.2. Zona Central: Gráficos y Conmutador Segmentado
- **Selector de Vistas**: Conmutador segmentado (*segmented control*) en la cabecera: `[ Curve (μ ± σ) | 2D Heatmap | Spectrum FFT ]`.
- **Paleta de Calor 2D**: Reemplazo de la rampa violeta/amarilla saturada por una rampa perceptual uniforme tipo **Viridis** (Azul marino profundo -> Verde azulado -> Esmeralda -> Amarillo oro).
- **Líneas de Grilla**: Grosor `0.5px`, trazo sutil con 20% de opacidad para que la curva de respuesta sea siempre el elemento protagónico.

### 5.3. Vúmetros de Precisión (`PrecisionAudioMeter`)
- Dos barras contiguas por canal:
  - Barra sólida inferior: Nivel RMS integrado.
  - Línea flotante superior (2 px): Pico máximo (*Peak Hold*) con caída balística exponencial a ~20 dB/s.
- Gradiente de color según normas acústicas:
  - Verde nominal hasta `-12 dBFS`.
  - Ámbar/Naranja de `-12 dBFS` a `-3 dBFS`.
  - Rojo puro de `-3 dBFS` a `0 dBFS` (indicador de clip).

### 5.4. Asistente de Calibración de Tarjeta (`LoopbackCalibrationModal`)
- Dimensiones: `550 x 380 px`, centrada sobre la ventana principal.
- Distribución en 2 columnas:
  - **Columna Izquierda (60%)**: Pasos interactivos numerados con chips circulares (`1`, `2`, `3`).
  - **Columna Derecha (40%)**: Mini-vúmetro horizontal de entrada en vivo que valida la presencia física del cable patch (alerta en rojo si la señal está por debajo de `-60 dBFS`).
- Acciones al pie: "Cancel" (texto plano a la izquierda) y "Start Loopback Measurement" (píldora verde dominante a la derecha).

### 5.5. Fusión de Telemetría y About (Eliminación de la Doble Modal)
- Se erradica la superposición de modales.
- Toda la información de build, versión, créditos y enlaces de soporte se consolida en una pestaña o sección dedicada dentro del panel lateral retráctil (*Settings / Telemetry Drawer*).

### 5.6. Pantalla de Inicio (Splash Screen)
- **Dimensiones y Contenedor**:
  - `560 x 320 px` centrado en pantalla con esquinas redondeadas (`cornerSize = 12.0f`).
  - Sin viñetas borrosas ni degradados difusos.
- **Panel Izquierdo (45% Ancho)**:
  - Imagen técnica/analógica recortada con un `juce::Path` de esquinas redondeadas únicamente en el lado izquierdo (superior e inferior).
  - Tratamiento monocromático o desaturación fría para casar con el blanco técnico de Sonarworks.
- **Panel Derecho (55% Ancho)**:
  - Overline en mayúsculas: `ABD SYNTHS • HARDWARE PROFILING LAB` (9 px, Bold, `AccentActive`).
  - Título: `ABDAudioLab` (24 px, SemiBold, `TextPrimary`).
  - Subtítulo y versión: `v1.1.0` en chip `SurfaceSubtle` + `Build 175` (`TextSecondary`, 11 px).
  - Pie descriptivo del motor DSP en una sola línea de texto (`10 px` `TextSecondary`):
    `«DSP Engine: Farina • Wiener-Hammerstein • NAM / RTNeural • SysEx»` (eliminando falsos botones grises).
  - Barra de carga minimalista: 2–3 px de alto, pista en `BorderSubtle`, relleno en `AccentActive`.

### 5.7. Modal y Editor de Parámetros de Prueba (Test & Parameter Config)
- **Dimensiones**:
  - Ancho ampliado a `680 px` con `juce::Viewport` interno para garantizar ergonomía en cualquier resolución.
- **Sección 1 (Formulario de Prueba)**:
  - Patrón *Label* encima del *Input*.
  - `Preset Configurations` a ancho completo.
  - `Test Name` (TextEditor de 32 px con padding de 8 px) y `Stimulus Type` (ComboBox estilizado).
  - Separador horizontal de 1 px (`BorderSubtle`).
- **Sección 2 (Duración y Captura)**:
  - Distribución en 2 columnas: `Signal Burst Duration` y `Custom Duration (s)` con sufijo «sec» integrado en el control.
  - `Adaptive Auto-Tail Silence Cutoff` implementado como un conmutador/toggle real (`juce::ToggleButton`).
- **Sección 3 (Matriz de Resolución con `juce::TableListBox`)**:
  - Sustitución de filas libres por una tabla estructurada [`juce::TableListBox`](file:///d:/desarrollos/ABDSynths/ABDAudioLab/src/gui/TestEditorPanel.h):
    - **Icono / Tipo (30 px)**: Icono vectorial (Knob / Fader).
    - **Parámetro (180 px)**: Nombre del control en SemiBold.
    - **Resolución (140 px)**: ComboBox embebido.
    - **Pasos (60 px)**: Valor numérico centrado en `Roboto Mono`.
    - **Rango Min-Max (110 px)**: Badge legible unificado: `0.0% → 100.0%`.
    - **Orden (50 px)**: Flechas vectoriales ▲/▼ compactas.
- **Sección 4 (Resumen y Botón Hero)**:
  - Tarjeta de estimación en `SurfaceSubtle` con tipografía mono (`ESTIMATED PLAN: 32 points total (~0m 32s)`).
  - Botón de acción principal: `Add to Session Plan` (40 px de altura, radio de 20 px píldora, fondo `AccentActive` o `TextPrimary`).

