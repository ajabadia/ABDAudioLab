# Guía de Diseño y Sistema Visual: Técnica Refinada (ABDSynths AudioLab)

Esta guía documenta la dirección de arte, sistema de diseño y especificación técnica de UI/UX para **ABDAudioLab**. Se rige por el sistema de diseño transversal de la suite definido en [ABDShared/Docs/ui-ux/DESIGN_SYSTEM_GUIDE.md](../../ABDShared/Docs/ui-ux/DESIGN_SYSTEM_GUIDE.md), aplicando sus directrices al laboratorio de metrología, perfilado acústico y análisis diferencial A/B. Inspirada en la sobriedad, claridad y reducción de fatiga visual de **Sonarworks SoundID Reference**, **Audio Precision** y estaciones de metrología analítica, establece una identidad unificada basada en datos precisos, tipografía de alta legibilidad, jerarquía espacial limpia y paridad multiplataforma.

---

## 1. Filosofía de Diseño: Reducción de Carga Cognitiva

1. **Jerarquía Clara sobre Densidad Desordenada**: El software de medición técnica no debe intimidar ni fatigar. Cada dato posee un nivel de contraste y peso visual proporcional a su importancia en el flujo operativo.
2. **Cero Doble Modal**: Jamás se superpone un diálogo modal sobre otro. Toda configuración secundaria, alertas de arranque y menús se consolidan en paneles dedicados, cajones retráctiles (*drawers*) o modales únicas centradas y bloqueantes con z-index controlado.
3. **Semántica Estricta de Color**: Los colores son portadores de estado metrológico, nunca adornos arbitrarios:
   - **Esmeralda / Cian Técnico (`AccentActive` / `Success`)**: Acciones principales, estados nominales (OK / Running / Connected / Calibrated).
   - **Ámbar / Naranja Cálido (`AccentWarning`)**: Avisos de tolerancia, advertencias de señal, mediciones con THD% elevado o hardware en modo manual.
   - **Rojo Carmesí Puro (`AccentError` / `Danger`)**: Saturación, clipping analógico/digital (-0.5 dBFS), fallos de loopback o desconexión crítica.
   - **Azul Técnico (`Info`)**: Rutas de señal, curvas de fase / retardo de grupo, telemetría de solo lectura y metadatos RFC 8785.
   - **Gris Técnico Neutral**: Fondos de chasis, bordes de 1 px, rejillas y textos secundarios.
4. **Arquitectura UI Dual con Paridad Multiplataforma**:
   - **WebUI (Objetivo Principal de Producción `ABDAudioLab_Web`)**: Basado en HTML5/CSS3/Vanilla JS empaquetado con Vite y ejecutado sobre Microsoft Edge WebView2 (Windows) / WebKit (macOS). Consumo zero-copy de tokens y componentes universales desde `ABDShared`.
   - **JUCE C++ Native (`ABDAudioLab`)**: Renderizado vectorial acelerado por hardware para entornos embebidos o legacy mediante [`abdaudiolab::gui::AppTheme`](../src/gui/AppTheme.h) y [`SoundIdTheme`](../src/gui/SoundIdTheme.h).

---

## 2. Sistema de Tokens Globales (Doble Tema: Oscuro y Claro)

El sistema soporta conmutación en tiempo real entre el tema oscuro nórdico (*Scientific Dark*) y el tema claro técnico (*Precision Light / Técnica Refinada*).

### 2.1. Tokens CSS de WebUI (`ABDShared/Assets/styles/`)

Los componentes de `WebUI/` consumen variables CSS estándar de `tokens.css`, `audiolab.css` y `audiolab-light.css`:

| Token CSS | Tema Oscuro (`audiolab`) | Tema Claro (`audiolab-light`) | Propósito Semántico |
|---|---|---|---|
| `--color-bg-base` | `#070A0E` | `#F8F9FA` | Fondo base del chasis principal. |
| `--color-panel-bg` | `#0B1118` | `#FFFFFF` | Fondo de sidebars, módulos y tarjetas. |
| `--color-panel-surface` | `#121A24` | `#F8FAFC` | Fondo de tarjetas internas y ranuras inactivas. |
| `--color-panel-surface-elevated` | `#1A2636` | `#FFFFFF` | Superficies elevadas, botones secundarios y popups. |
| `--color-panel-border` | `rgba(255,255,255,0.09)` | `rgba(15,23,42,0.12)` | Delimitaciones de 1 px de módulos y tarjetas. |
| `--color-panel-border-dim` | `rgba(255,255,255,0.05)` | `rgba(15,23,42,0.06)` | Rejillas de gráficos, guías y separadores sutiles. |
| `--color-text-main` | `#E8FFF0` | `#1A1D20` | Lectura principal, H1/H2 y valores de medición. |
| `--color-text-muted` | `#7EB5A0` | `#475569` | Etiquetas de parámetros, unidades y estados. |
| `--color-text-dim` | `#4A8A6A` | `#475569` | Leyendas secundarias, badges y notas al pie. |
| `--color-accent` | `#00E676` | `#007A4D` | Acciones principales, botones primarios y estado activo. |
| `--color-warning` | `#FFB142` | `#B45309` | Alertas, umbrales de aviso y advertencias. |
| `--color-danger` | `#FF5252` | `#C5221F` | Saturación / Clipping (-0.5 dBFS), error de hardware. |
| `--color-info` | `#00C3FF` | `#0284C7` | Rutas de audio secundarias, telemetría y curvas FFT. |

### 2.2. Tokens C++ JUCE (`AppTheme.h` / `SoundIdTheme.h`)

Para componentes C++ nativos, se prohíbe hardcodear colores hexadecimales en llamadas `paint()`. Se consulta la estructura estática correspondiente:

```cpp
// AppTheme (abdaudiolab::gui::AppTheme)
juce::Colour background = AppTheme::BackgroundApp;   // 0xfff8f9fa (Light)
juce::Colour card       = AppTheme::SurfaceCard;     // 0xffffffff
juce::Colour accent     = AppTheme::AccentActive;    // 0xff00a86b
juce::Colour alert      = AppTheme::AccentWarning;   // 0xffe65100
juce::Colour clip       = AppTheme::AccentError;     // 0xffd32f2f

// SoundIdTheme (Soporte Dual C++ y Modo Oscuro)
juce::Colour bgMain     = SoundIdTheme::bgDark;      // 0xff0b1118
juce::Colour border     = SoundIdTheme::borderSubtle;// 1px separator
```

---

## 3. Tipografía Técnico-Modernista

1. **Inter** (Google Fonts / Incrustada binariamente en JUCE):
   - Tipografía principal sans-serif neutra y de alta legibilidad en pantallas HiDPI.
   - **H1 (Títulos de Modales y Cabeceras de Módulo)**: `18px` – `22px`, SemiBold / Bold.
   - **H2 (Subtítulos de Sección y Tarjetas)**: `13px` – `15px`, SemiBold.
   - **Body / Etiquetas de Controles**: `11.5px` – `13px`, Regular / Medium.
   - **Micro Labels / Badges**: `9px` – `10.5px`, Bold (con espaciado `letter-spacing: +0.6px` a `+1.2px`).
2. **JetBrains Mono / Roboto Mono / Consolas**:
   - Tipografía monospaciada con `tabular-nums` obligatoria para magnitudes físicas y telemetría:
     - Frecuencias (`20.0 Hz`, `1.0 kHz`), Niveles (`-18.2 dBFS`, `+0.4 dBr`), Latencias (`5.3 ms`, `256 samples`), Porcentajes (`0.012% THD`) y hashes criptográficos SHA-256.

---

## 4. Estándares de Controles e Interactividad

### 4.1. Botones de Acción
- **Botón Primario (Hero / Primary Action)**:
  - Estilo píldora o radio de esquina de `6px` a `8px`.
  - Fondo `var(--color-accent)` con texto de alto contraste (`#FFFFFF` o `#070A0E`).
  - Sin biseles pseudo-3D; iluminación plana con hover sutil (`filter: brightness(1.08)` o `darker(0.05f)`).
- **Botones Secundarios y de Utilidad**:
  - Borde de 1 px `var(--color-panel-border)`, fondo `var(--color-panel-surface-elevated)`.
  - Texto en `var(--color-text-main)` con hover en `var(--color-accent)`.
- **Botones de Estado / Conmutadores (Toggles / Pills)**:
  - Indicadores activos mediante anillo de acento o relleno sutil (`rgba(accent, 0.15)`).

### 4.2. Iconografía Anti-Slop Estricta
- **Cero Emojis como Iconos**: Queda terminantemente prohibido el uso de caracteres emoji (ej: 📊, ⚡, 🔍, ⚙️) en la interfaz.
- **Iconos SVG Vectoriales Limpios**: Todos los iconos deben ser elementos `<svg>` puros con trazo `stroke="currentColor"`, `stroke-width="2"` o `fill="currentColor"`, integrados semánticamente en botones y badges.

### 4.3. Selectores Desplegables y Modales
- Desplegables con borde sutil, fondo consistente con el panel y flechas vectoriales estilizadas.
- Modales centradas con capa de fondo (*backdrop blur*), cierre mediante botón accesible, tecla `Escape` y clic exterior controlado.

---

## 5. Distribución Espacial: El Flujo Guiado en 5 Pasos

La arquitectura visual organiza el espacio en tres zonas principales:
1. **Barra Superior (`SynthNavBar`)**: Identidad, menús, telemetría I/O, selector de tema y accesos rápidos.
2. **Sidebar Izquierdo (Workflow Stepper)**: Navegador guiado colapsable con los 5 pasos canónicos y ficha de hardware.
3. **Área Central Dinámica**: Lienzo de trabajo específico según el paso activo.
4. **Sidebar Derecho (Meters & Telemetry)**: Vúmetros de precisión, pico numérico y estado del motor.

```
+---------------------------------------------------------------------------------------------------------+
| [LOGO] ABDAudioLab | Archivo Edición Ver Herramientas Ayuda | [A-IN][A-OUT][M-IN][M-OUT] | [A/B] [🔔] [🌓] |
+------------------+--------------------------------------------------------------------+-----------------+
| WORKFLOW         | MAIN STAGE CONTENT                                                 | TELEMETRÍA I/O  |
| 1. Target        |                                                                    |                 |
| 2. Calibration   |                                                                    |   -18.2 dBFS    |
| 3. Preflight     |                                                                    |   [IN]   [OUT]  |
| 4. Acquisition   |  [ ÁREA DINÁMICA SEGÚN PASO ACTIVO ]                               |   |..|   |..|   |
| 5. Export        |                                                                    |   |..|   |..|   |
|                  |                                                                    |   |..|   |..|   |
| [AUDIO CARD]     |                                                                    |   ● READY       |
+------------------+--------------------------------------------------------------------+-----------------+
| [ VIRTUAL MIDI KEYBOARD DRAWER - Desplegable con Ctrl+K ]                                               |
+---------------------------------------------------------------------------------------------------------+
```

---

## 6. Especificación Detallada de los 5 Pasos

### Paso 1: Target & Routing (`stage1_target.js`)
- **Catálogo de Hardware y Plugins**: Malla de tarjetas estilizadas que diferencian:
  - Sintetizadores Hardware con control MIDI/SysEx o potenciómetros analógicos.
  - Pedales de efectos y procesadores analógicos.
  - Plugins Virtuales VST3 escaneados con carátula gráfica automática y ventana nativa de edición.
- **Enrutamiento I/O**: Asignación de canales físicos estéreo o dual-mono y puertos MIDI IN/OUT.

### Paso 2: Interface Calibration (`stage2_calibration.js`)
- **Loopback de Tarjeta de Sonido**:
  - Medición automatizada de latencia física de ida y vuelta (*round-trip latency* en muestras y ms).
  - Verificación de respuesta en frecuencia del convertidor D/A y A/D (compensación de tarjeta).
  - Medición de SNR y nivel de ruido de fondo en dBr.
  - **Bypass Automático**: Si el objetivo es un plugin VST3, este paso se desactiva e indica *«Bypassed for digital VST»*.

### Paso 3: Hookup & Preflight (`stage3_preflight.js`)
- **Diagrama de Cableado Interactivo**: Esquema visual con conexiones codificadas por color (Audio In, Audio Out, MIDI In, MIDI Out).
- **Alineación de Potenciómetros y Parches**:
  - Plugins VST3: Sincronización automática de parámetros neutrales.
  - Sintetizadores MIDI: Transmisión de SysEx de inicialización a estado base.
  - Hardware Analógico: Confirmación manual de potenciómetros físicos en posición de referencia.
- **Ping de Señal Acústica**:
  - Disparo de pulso de excitación de 800 Hz (-18 dBFS) con retorno visual instantáneo en los vúmetros y detección de saturación.

### Paso 4: Run Acquisition & Visualizador Multi-Pestaña (`stage3_classic.js`)
- **Cola de Medición (`SuiteQueue`)**: Lista de puntos de prueba con estado (Pending, Measuring, Completed, Error) y progreso.
- **Conmutador Segmentado de 8 Pestañas**:
  1. **Freq Curves (μ ± σ)**: Curvas de respuesta en magnitud con banda de dispersión estadística $\pm 1\sigma$.
  2. **2D Heatmap**: Matriz espectral 2D calculada con la rampa perceptual uniforme **Viridis** (Azul marino profundo `#0A194B` -> Verde azulado `#00A5D2` -> Esmeralda `#00E678` -> Amarillo oro `#F5BE00`).
  3. **Spectrum FFT**: Espectro de Fourier de alta resolución con picos armónicos.
  4. **Phase / GD**: Curvas de fase desplegada (rad) y retardo de grupo (*group delay* en ms).
  5. **3D Mountains**: Gráfico tridimensional tipo cascada (*waterfall*) con paletas seleccionables (`Emerald`, `Fire`, `Frost`).
  6. **Mod Matrix**: Tabla de modulación de parámetros y comportamiento dinámico.
  7. **±σ Band**: Visualización dedicada de la envolvente de tolerancia e incertidumbre.
  8. **THD %**: Nodos de distorsión armónica total sobre el espectro de excitación.
- **Líneas de Grilla**: Trazos delgados (`0.5px` a `1px`) con opacidad sutil (`20%` a `30%`) y color `--color-panel-border-dim`.

### Paso 5: Export & Report (`stage4_export.js`)
- **Sellado Metrológico**: Generación de paquete de producción C++20 con tabla SIMD Look-Up Table (`.h`).
- **Telemetría y Manifiesto RFC 8785**: Exportación de curvas en formato JSON canónico con hash SHA-256 sellado.
- **Informe de Certificación**: Ficha técnica metrológica que valida correlación espectral $\rho \ge 0.999$, ESR $\le -40\text{ dB}$ y ENOB de medición.

---

## 7. Componentes Especializados

### 7.1. Vúmetros de Precisión (`PrecisionAudioMeter` / `VuMeter`)
- **Ubicación**: Sidebar derecho persistente (`.meters-sidebar`).
- **Canales**: Medición dual independiente `IN` y `OUT` calibrada de `-60 dBFS` a `0 dBFS`.
- **Comportamiento Balístico**:
  - Barra sólida con gradiente acústico: Verde nominal hasta `-12 dBFS`, Ámbar de `-12` a `-3 dBFS`, Rojo de `-3` a `0 dBFS`.
  - LED de Clipping superior a `-0.5 dBFS` con retención visible.
  - Display de lectura numérica superior en `monospace` (`#telemetryPeakReadout`) con indicación en tiempo real y alerta de saturación.
  - Indicador de estado al pie: `● IDLE / READY`, `ACQUIRING...`, `INPUT CLIPPING`.

### 7.2. Comparador Universal A/B Diferencial y Null-Test (`UniversalABComparatorModal`)
- **Dimensiones y Ergonomía**: Modal amplia de `1260 px` centrada sobre el espacio de trabajo.
- **Topología de Comparación**:
  - Comparación cruzada flexible: Hardware real A vs Hardware real B, Hardware vs Plugin VST3, Plugin vs Plugin, o Grabación de Audio.
  - Enrutamiento físico real: Soporte para tarjetas estéreo con *Dual-Mono Split* (CH1 para Referencia A, CH2 para Target B) o tarjetas multi-canal.
  - Catálogo de VST3 dinámico con auto-carga en el motor de hosting aislado y botón para abrir la ventana GUI nativa.
- **Motor Metrológico**:
  - Alineación temporal fraccional por interpolación sinc sub-sample ($\tau < 0.1 \text{ samples}$).
  - Invarianza de fase para osciladores analógicos *free-running*.
  - Descomposición espectral en 24 bandas de 1/3 de octava con histograma de error diferencial.
  - Métrica de error de señal residual (ESR en dB): Idéntico $\le -60\text{ dB}$, Excelente $\le -40\text{ dB}$, Desviación audible $> -24\text{ dB}$.
  - Conmutador de audición instantánea A/B sin clics acústicos (crossfade de 5 ms).

### 7.3. Cajón de Teclado MIDI Virtual (`VirtualKeyboardDrawer`)
- Teclado desplegable en la parte inferior de la ventana (atajo global `Ctrl+K`).
- Rango configurable de 61 u 88 teclas de piano con respuesta a la velocidad y pitch bend.
- Conexión directa al motor de audio C++ para disparar notas de prueba sin necesidad de teclado físico externo.

---

## 8. Matriz de Cumplimiento Técnico

| Requisito / Especificación | Estado Actual | Implementación / Ubicación |
|---|---|---|
| **Cero Doble Modal** | Cumple | Gestor centralizado en `SoundIdModal.js` y `app.js`. |
| **Paleta Viridis en Heatmap 2D** | Cumple | `stage3_dsp_curves.js` (`dbToHeatmapColor`) y `PlotterHeatmapRenderer.cpp`. |
| **Grilla de 0.5px / baja opacidad** | Cumple | CSS `--color-panel-border-dim` y `SoundIdTheme::borderSubtle`. |
| **Vúmetros con balística y gradiente acústico** | Cumple | C++ `SoundIdMeterStrip.cpp` y WebUI `VuMeter` (`@components/vuMeter.js`). |
| **Iconos SVG sin emojis** | Cumple | Prohibición estricta en todo el código y assets SVG dedicados. |
| **Tipografía Inter + Mono** | Cumple | Fuentes oficiales incrustadas en C++ y cargadas en CSS `@styles/tokens.css`. |
| **Doble Tema (Claro / Oscuro)** | Cumple | Selector en navbar, `audiolab.css` y `audiolab-light.css`. |
| **Conmutador de Vistas en Medición** | Cumple (Expandido) | 8 pestañas operativas en `stage3_classic.js`. |
| **Comparador A/B Null-Test** | Cumple | `UniversalABComparatorModal.js` + `UniversalABComparatorEngine.h`. |
| **Teclado MIDI Virtual** | Cumple | `VirtualKeyboardDrawer.js` (atajo `Ctrl+K`). |
