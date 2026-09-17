# Release Notes — Fase 20.11.2: Núcleo Digital y Visor Multicontenedor FAIR/LNL

**Fecha de Certificación:** 17 de Septiembre de 2026  
**Commit:** `820aa2739ec163a67a13b76f59deb8a6e1309502`  
**Tag Oficial:** `phase-20.11.2-certified`  
**Estado de la Suite Global:** 373/373 test cases superados (223.515 aserciones, 0 fallos)  
**Entorno de Compilación:** MSVC 18.4.3 (Visual Studio 2026 Developer Command Prompt v18.4.3, Windows x64, Release)  

---

## 1. Resumen Ejecutivo

La **Fase 20.11.2** consolida y sella el **Núcleo de Medición Digital VST3 y el Visor Multivariante FAIR/LNL** de ABDAudioLab. Este hito culmina la transición desde la captura y alojamiento de plugins hacia una estación metrológica completa con capacidades de verificación criptográfica, comparación de equivalencia acústica, límites preventivos de memoria, virtualización segura de interfaz y accesibilidad nativa conforme a WCAG 2.4.7.

El sistema garantiza que ninguna comparación acústica, superposición gráfica o reproducción de audio se realice sobre datos no certificados, manipulados o metrológicamente incompatibles.

---

## 2. Identidad Metrológica y Entorno de Validación

| Parámetro | Valor de Referencia / Evidencia |
| :--- | :--- |
| **Plugin Target de Referencia** | `Dexed.vst3` (64-bit VST3 real) |
| **Ruta Canónica de Instalación** | `C:\Program Files\Common Files\VST3\Dexed.vst3` |
| **SHA-256 del Binario Target** | `e8b3b00a53bb0aa1ef082b0c1b5cb66bdf1af5eddf787b8f47397df6d2411a40` |
| **UID / CID del Componente VST3** | `VST3-Dexed-3f015740-d7709eec` |
| **Preset de Control Validado** | `Dexed_Controlled_Init` (8.340 bytes, SHA-256: `f2ef03f0c961f044fefc40ca1f744155374db84b9ae9b86390e21017946fbf7b`) |
| **Reproducibilidad Bit a Bit** | Verificada en render offline ($\Delta \text{audio} = 0$, `underruns` = 0, `overruns` = 0) |

---

## 3. Capacidades y Características de la Release

### 3.1. Visor y Coordinador Multicontenedor (`MeasurementComparisonSession`)
- **Gestión Asíncrona Concurrente:** Carga multi-hilo en `juce::ThreadPool` con desacoplo total del hilo de audio en tiempo real y del message thread de JUCE.
- **Máquina de Estados de Apagado (`SessionShutdownState`):**  
  `Running` $\to$ `CancellationRequested` $\to$ `Draining` $\to$ `Drained` / `DrainTimedOut` $\to$ `Destroyed`.
- **Prevención de *Use-After-Free*:** Estado interno administrado por `std::shared_ptr<SharedSessionState>`. Los hilos del pool y los callbacks de interfaz acceden exclusivamente a través de `std::weak_ptr`. En caso de timeout de drenado (`DrainTimedOut`), los recursos se preservan hasta la finalización cooperativa de los hilos sin publicar resultados huérfanos.
- **Generación Monotónica (`sessionGeneration`):** `std::atomic<uint64_t>` monotónico. Ningún callback tardío o perteneciente a una sesión cancelada puede alterar el estado de la UI:
  $$\text{generation} == \text{sessionGeneration} \;\land\; !\text{cancelToken} \;\land\; \text{shutdownState} == \text{Running}$$

### 3.2. Cuotas Preventivas de Recursos (`SessionResourceLimits`)
Evaluadas preventivamente mediante `checkResourceLimitsPreLoad(...)` **antes** de leer bytes, parsear JSON, asignar vectores o decodificar audio:
- **Máximo de Contenedores:** 64 contenedores por sesión.
- **Límite de Manifest:** 1.048.576 bytes (1 MB).
- **Límite por Archivo JSON:** 10.485.760 bytes (10 MB).
- **Límite de Puntos por Curva:** 100.000 puntos.
- **Límite por Archivo de Audio:** 104.857.600 bytes (100 MB).
- **Cargas Concurrentes Máximas:** 4 hilos de pool.
- **Respuesta ante Exceso:** Transición inmediata a `ContainerLoadState::Rejected` con diagnóstico trazable `resource_limit_exceeded: ...`.

### 3.3. Accesibilidad y Navegación por Teclado (WCAG 2.4.7)
- $\uparrow / \downarrow$: Navegación ordinal en la lista con auto-scroll (`scrollToEnsureRowIsOnscreen`).
- `Espacio`: Alternar inclusión en la comparación gráfica (restringido estrictamente a contenedores `Verified`).
- `Enter`: Conmutar contenedor seleccionado como fuente de audio activa (sólo si `isPlayable()`).
- `Delete`: Elimina **exclusivamente el contenedor seleccionado**. Si no hay selección, opera como *no-op*. El contenedor de audio activo no se elimina a menos que coincida con la selección.
- **Halo de Foco Cyan:** Indicador visual de 2px (`#00e5ff`) con contraste suficiente y persistencia dentro del viewport.

### 3.4. Virtualización Limpia de Filas
- `refreshComponentForRow(...)` sobrescribe íntegramente la configuración visual y reasigna los callbacks capturando el `containerId` actual de la fila.
- Cero listeners residuales, cero llamadas huérfanas y reseteo completo ante índices fuera de rango.

### 3.5. Exclusiones Reproducibles Deterministas
- Registro estructurado `ComparisonExclusionRecord` (`code`, `message`, `containerId`, `metric`, `comparisonBasisHash`, `rulesVersion`, `timestampIso`).
- Función `computeBasisHash(...)` simétrica y determinista (A vs B $\equiv$ B vs A) basada estrictamente en contenedores, métricas, unidades, dominios y versión de reglas. El campo `timestampIso` no participa en la decisión ni en el hash.

---

## 4. Clasificación de Estados e Integridad

### 4.1. Estados de Carga de Contenedor (`ContainerLoadState`)
- **`Pending`**: Contenedor encolado para inspección.
- **`Loading`**: Leyendo artefactos y calculando sumas de comprobación criptográficas SHA-256.
- **`Verified`**: Todos los artefactos coinciden bit a bit con `manifest.json`. Autorizado para comparación gráfica y audición.
- **`Corrupt`**: Discrepancia criptográfica detectada o archivo manipulado (*tampered*). Excluido de comparación; audio bloqueado de forma absoluta.
- **`Rejected`**: Cuotas preventivas excedidas (`resource_limit_exceeded`) o esquema inválido.
- **`Failed`**: Error de E/S o cancelación solicitada por el usuario.

### 4.2. Matriz de Equivalencia de Estado por Pares (`PairwiseStateEquivalence`)
- **`BitExact`**: Curvas idénticas ($\Delta = 0.0$), hashes de estado y binario de plugin coincidentes.
- **`SemanticallyEquivalent`**: Equivalencia numérica dentro de la tolerancia metrológica ($\Delta \le 10^{-4}$).
- **`NotEquivalent`**: Divergencia acústica observada.
- **`NotComparable`**: Dominios dispares, métricas incompatibles (e.g. Peak vs RMS), unidades no coincidentes o binarios distintos.

---

## 5. Limitaciones Conocidas y Alcance de Backlog

- **Fase 20.12 (Cadena Analógica Avanzada T4.3–T4.5):** Permanece intencionadamente en **backlog congelado**. No se contemplan compensaciones de latencia fraccional analógica en hardware ni modelos de tres capas descompuestos en esta release.
- **Confinamiento de Archivos:** Las rutas relativas dentro de los contenedores FAIR no pueden apuntar al exterior del directorio raíz del contenedor (verificación estricta anti-path traversal).

---

## 6. Procedimiento de Reproducción y Verificación

Para reproducir la certificación exacta de este commit:

```powershell
# 1. Comprobar limpieza del working tree y tag exacto
git status --short
git describe --tags --exact-match HEAD

# 2. Compilar la suite de tests en Release MSVC x64
.\build.bat tests

# 3. Ejecutar la sub-suite de la Fase 20.11.2
.\build\Release\ABDAudioLab_Tests.exe "[comparison_robustness]"

# 4. Ejecutar la suite global de 373 tests
.\build\Release\ABDAudioLab_Tests.exe
```

**Resultado Esperado:** 373/373 test cases pasados, 223.515 aserciones en verde, 0 fallos.
