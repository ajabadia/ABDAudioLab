# Guía del Operador — Visor Multicontenedor FAIR/LNL

Esta guía describe los procedimientos operativos estándar para cargar, verificar, comparar, auditar y exportar mediciones acústicas y de síntesis utilizando el Visor de Comparación Multicontenedor de **ABDAudioLab**.

---

## 1. Flujo Práctico de Trabajo

El flujo de trabajo metrológico consta de siete etapas secuenciales obligatorias:

```
[1. Cargar Contenedor] 
       │
       ▼
[2. Esperar Verificación Criptográfica (Verified)]
       │
       ▼
[3. Seleccionar Series Compatibles]
       │
       ▼
[4. Interpretar Exclusiones Metrológicas]
       │
       ▼
[5. Comparar Equivalencias de Estado por Pares]
       │
       ▼
[6. Audición Reproducible (Solo Audio Verificado)]
       │
       ▼
[7. Exportación de Informe Auto-Contenido HTML]
```

### Paso 1: Carga de Contenedores
1. Haz clic en el botón superior **`+ Cargar Contenedor...`**.
2. Selecciona la carpeta raíz de un contenedor estructurado conforme al estándar `abdaudiolab-fair-lnl-1.0`.
3. El contenedor ingresará en estado `Loading` (insignia azul), procesándose en segundo plano sin congelar la interfaz.

### Paso 2: Verificación de Integridad
- El sistema inspecciona preventivamente las cuotas de recursos.
- A continuación, calcula el hash SHA-256 de cada archivo físico en disco y lo contrasta contra `manifest.json`.
- **Resultado `Verified` (Insignia Verde):** El contenedor está certificado, inmutable y disponible para superposición gráfica y audición.
- **Resultado `Corrupt` (Insignia Roja):** Se detectó manipulación de archivos o suma de comprobación no coincidente. El contenedor se excluye automáticamente de cualquier cálculo y su audio queda bloqueado.
- **Resultado `Rejected` (Insignia Ámbar):** El contenedor superó alguna cuota de memoria o tamaño máximo (`resource_limit_exceeded`).

### Paso 3: Selección de Series para Comparación
- Mediante el checkbox izquierdo de cada fila, activa o desactiva la curva en el gráfico central.
- Los contenedores corruptos o rechazados tienen el control de selección deshabilitado permanentemente.
- Cada serie cuenta con un color accesible de alto contraste, un patrón de trazo distintivo (sólido, rayado, raya-punto, punteado) y marcadores geométricos (círculo, cuadrado, triángulo, rombo) para lectura accesible independiente del color.

### Paso 4: Interpretación de Exclusiones
Si dos o más contenedores presentan bases incompatibles, el sistema los excluye de la superposición gráfica para evitar falsas conclusiones:
- **Discrepancia de Unidades:** No es posible superponer curvas con ejes en dBFS contra curvas en Hz o ms.
- **Diferencia de Métrica:** Está estrictamente prohibido superponer niveles de pico (*Peak*) con potencia promediada (*RMS*).
- **Segregación de Dominios:** No se permite superponer mediciones digitales puras (`Vst3OfflineDigital`) con mediciones analógicas no compensadas (`CombinedDutAndChain`).

### Paso 5: Evaluación de la Matriz de Equivalencia por Pares
En la tarjeta de equivalencia (panel derecho), se evalúa la correspondencia acústica par a par entre los contenedores seleccionados:
- **`BitExact` (Insignia Verde Neón):** Curvas idénticas ($\Delta = 0.0$), mismo UID de plugin y concordancia de estado.
- **`SemanticallyEquivalent` (Insignia Ámbar):** Diferencia acústica acotada dentro de la tolerancia metrológica ($\Delta \le 10^{-4}$).
- **`NotEquivalent` (Insignia Roja):** Divergencia acústica observable.
- **`NotComparable` (Insignia Gris):** Dominios, estímulos o binarios incompatibles.

### Paso 6: Audición Segura
- Selecciona cualquier contenedor verificado mediante el botón **`[Audio]`**.
- La insignia pasará a **`[ACTIVO]`** y el reproductor inferior cargará la referencia verificada.
- Si un contenedor entra en estado corrupto, el reproductor silencia automáticamente la salida y deniega la reproducción.

### Paso 7: Exportación de Informe
- Haz clic en **`Exportar Informe HTML`**.
- El generador compila un informe HTML auto-contenido que incluye el gráfico multiserie en formato SVG accesible, la tabla de exclusiones con sus códigos reproducibles y la matriz de equivalencia.

---

## 2. Accesibilidad de Teclado y Foco Visible (WCAG 2.4.7)

El visor de contenedores implementa controles nativos por teclado para operadores sin ratón o con herramientas de apoyo:

| Tecla / Combinación | Acción Operativa | Condición de Seguridad |
| :--- | :--- | :--- |
| **Flecha Arriba ($\uparrow$)** | Mueve el foco y la selección a la fila anterior. | El viewport realiza auto-scroll para mantener la fila visible. |
| **Flecha Abajo ($\downarrow$)** | Mueve el foco y la selección a la fila siguiente. | El viewport realiza auto-scroll para mantener la fila visible. |
| **Espacio (`Space`)** | Alterna la casilla de superposición comparativa. | Solo opera si el contenedor está en estado `Verified`. |
| **Intro (`Enter`)** | Activa el contenedor seleccionado como fuente de audio. | Solo opera si el contenedor verificado cuenta con audio reproducible. |
| **Supr (`Delete`)** | Elimina de la sesión el contenedor seleccionado. | **Condición estricta:** Opera sobre la fila con selección de foco, **nunca** sobre el contenedor que tiene el audio activo (a menos que coincidan). Si no hay fila seleccionada, es un *no-op*. |

### Indicador de Foco Visible
La fila que posee el foco de teclado se resalta de inmediato con un halo perimetral cyan (`#00e5ff`) de 2 píxeles de grosor y alto contraste, garantizando el cumplimiento estricto del criterio de conformidad **WCAG 2.4.7 (Focus Visible)**.

---

## 3. Límites de Recursos y Diagnósticos (`SessionResourceLimits`)

Para proteger la estabilidad del sistema contra archivos masivos, archivos manipulados o ataques de denegación de servicio por memoria, la sesión aplica cuotas preventivas:

| Recurso | Límite Máximo | Diagnóstico en Caso de Exceso |
| :--- | :--- | :--- |
| **Contenedores por Sesión** | 64 | `resource_limit_exceeded: maximum container count reached (64)` |
| **Tamaño de `manifest.json`** | 1 MB (1.048.576 B) | `resource_limit_exceeded: manifest.json size (...) exceeds limit` |
| **Tamaño de Archivos JSON** | 10 MB (10.485.760 B) | `resource_limit_exceeded: JSON file ... exceeds limit` |
| **Puntos por Curva** | 100.000 puntos | `resource_limit_exceeded: curve points (...) exceeds maxCurvePoints` |
| **Tamaño de Archivos WAV** | 100 MB (104.857.600 B) | `resource_limit_exceeded: audio file ... exceeds limit` |
| **Hilos Concurrentes** | 4 cargas | Encolamiento seguro en `juce::ThreadPool` |

---

## 4. Política de Tampering y Preservación de Evidencia

Si un operador o proceso externo modifica un archivo dentro de un contenedor ya verificado:
1. Cualquier re-verificación o intento de audición detectará la discrepancia del hash SHA-256.
2. El contenedor se degradará a `ContainerLoadState::Corrupt`.
3. El reproductor de audio se detendrá de inmediato.
4. El diagnóstico registrará: `Integrity check failed: ... hash mismatch`.
