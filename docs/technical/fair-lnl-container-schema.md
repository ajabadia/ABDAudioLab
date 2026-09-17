# Especificación del Esquema — Contenedor FAIR / LNL (`abdaudiolab-fair-lnl-1.0`)

Este documento define la estructura en disco, los esquemas de metadatos JSON conforme a RFC 8785 y las reglas de verificación criptográfica para los contenedores de medición **FAIR / LNL** (Findable, Accessible, Interoperable, Reusable / Linear Non-Linear).

---

## 1. Topología del Árbol de Directorios

Cada contenedor FAIR/LNL se almacena como una carpeta autónoma e inmutable:

```
<container_directory>/
├── manifest.json                               # Manifiesto criptográfico y roles (RFC 8785)
├── specs/
│   └── measurement_spec.json                   # Especificación formal del experimento
├── results/
│   └── measurement_result.json                 # Telemetría y resultados escalares
├── curves/
│   ├── dynamics_velocity_level_curve.json      # Curva primaria (e.g. Velocidad vs dBFS)
│   └── dynamics_velocity_timbre_curve.json     # Curva secundaria opcional (e.g. Spectral Centroid)
├── audio/
│   ├── dexed_reference.wav                     # Audio de referencia capturado (16-bit/24-bit PCM)
│   ├── audio_stimulus.wav                      # Estímulo acústico inyectado
│   └── impulse_response.wav                    # Respuesta al impulso obtenida
└── reports/
    └── measurement_report.html                 # Informe auto-contenido con SVG integrados
```

---

## 2. Especificación de `manifest.json`

El archivo `manifest.json` constituye la raíz de confianza criptográfica del contenedor. Se serializa siguiendo las directrices de ordenación canónica de claves según **RFC 8785**:

```json
{
  "manifestFormat": "artifact-list-v1",
  "schemaVersion": "abdaudiolab-fair-lnl-1.0",
  "executionDomain": "Vst3OfflineDigital",
  "artifactCount": 5,
  "listedArtifactCount": 4,
  "manifestExcludedFromArtifacts": true,
  "artifacts": [
    {
      "path": "specs/measurement_spec.json",
      "sha256": "3a4b5c6d...",
      "sizeBytes": 1024,
      "role": "measurement_spec"
    },
    {
      "path": "audio/dexed_reference.wav",
      "sha256": "7e8f9a0b...",
      "sizeBytes": 96044,
      "role": "audio_reference"
    },
    {
      "path": "curves/dynamics_velocity_level_curve.json",
      "sha256": "1c2d3e4f...",
      "sizeBytes": 450,
      "role": "curve_data"
    },
    {
      "path": "results/measurement_result.json",
      "sha256": "5a6b7c8d...",
      "sizeBytes": 2048,
      "role": "result_data"
    }
  ]
}
```

### Campos Obligatorios del Manifiesto
- `schemaVersion`: Cadena estricta `"abdaudiolab-fair-lnl-1.0"`.
- `executionDomain`: Dominio metrológico correspondiente (`Vst3OfflineDigital`, `Vst3Realtime`, `DigitalHardwareRoundtrip`, `CombinedDutAndChain`).
- `artifacts`: Matriz de descriptores de artefactos.
  - `path`: Ruta relativa dentro del contenedor, usando siempre `/` como separador.
  - `sha256`: Hash criptográfico SHA-256 en hexadecimal en minúsculas (64 caracteres).
  - `sizeBytes`: Tamaño exacto del archivo en bytes.
  - `role`: Rol metrológico estandarizado.

---

## 3. Catálogo de Roles de Artefactos

| Rol Canónico | Descripción Metrológica | Requerido para `Verified` |
| :--- | :--- | :---: |
| `measurement_spec` | Parámetros del experimento y configuración DUT | Sí |
| `result_data` | Registro de ejecución y telemetría de salida | Sí |
| `curve_data` | Conjunto de puntos $(x, y)$ con unidades normalizadas | Sí |
| `audio_reference` | Archivo WAV resultante de la captura acústica | Sí (si `dutType == instrument`) |
| `measurement_stimulus_audio` | Estímulo acústico sintetizado (log-sweep, ruido, tono) | Condicional (Hardware) |
| `measurement_impulse_response` | Deconvolución calculada en dominio temporal | Condicional (Hardware) |
| `html_report` | Documento resumen de auditoría técnica | Opcional |

---

## 4. Política de Seguridad y Verificación Criptográfica

### 4.1. Confinamiento contra Path Traversal
El cargador (`MeasurementViewModelLoader`) implementa validación estricta de rutas relativas:
- Toda ruta declarada en `manifest.json` debe comenzar con la ruta canónica del contenedor y ser hija de la misma:
  ```cpp
  auto checkConfinement = [&](const juce::File& f) {
      return f.getFullPathName().startsWith(containerDir.getFullPathName()) &&
             f.isAChildOf(containerDir);
  };
  ```
- El uso de secuencias `../`, enlaces simbólicos hacia el exterior o rutas absolutas provoca el rechazo inmediato (`Security violation: Path traversal detected`).

### 4.2. Detección de Manipulación (*Tampering*)
Durante la verificación en segundo plano:
1. Se calcula el hash SHA-256 de cada archivo físico listado en `manifest.json`.
2. Si un archivo falta en disco o su suma calculada difiere de la declarada, el contenedor transita a:
   $$\text{loadState} = \text{ContainerLoadState::Corrupt}$$
3. Se genera un diagnóstico con el detalle del artefacto manipulado.
4. Cualquier intento de reproducción de audio o inclusión en comparadores queda bloqueado a nivel de arquitectura.

---

## 5. Trazabilidad de Exclusiones Reproducibles (`ComparisonExclusionRecord`)

Cuando una comparación entre dos contenedores es rechazada por incompatibilidad de bases (e.g. dominios o unidades), el sistema registra una exclusión estructurada:

```cpp
struct ComparisonExclusionRecord
{
    std::string code;                /**< Código estable: "metric_basis_mismatch" */
    std::string message;             /**< Explicación diagnóstica */
    std::string containerId;         /**< Contenedor excluido */
    std::string metric;              /**< Métrica evaluada */
    std::string comparisonBasisHash; /**< Hash simétrico reproducible */
    std::string rulesVersion;        /**< Versión de reglas ("exclusion-rules-1.0") */
    std::string timestampIso;        /**< Timestamp de sesión (excluido del hash) */
};
```

### Determinismo del Hash de Decisión (`computeBasisHash`)
Para asegurar la reproducibilidad de la exclusión a lo largo del tiempo:
1. Los identificadores de los contenedores se ordenan alfabéticamente ($A \le B$).
2. Se forma la cadena canónica:
   $$\text{raw} = \text{rulesVersion} + "|" + A + "|" + B + "|" + \text{metric} + "|" + \text{unit} + "|" + \text{domain}$$
3. Se calcula el SHA-256 de dicha cadena.
4. **Independencia del Timestamp:** El campo `timestampIso` no participa en el cálculo, garantizando que dos ejecuciones en momentos distintos con los mismos parámetros arrojen exactamente el mismo `comparisonBasisHash`.
