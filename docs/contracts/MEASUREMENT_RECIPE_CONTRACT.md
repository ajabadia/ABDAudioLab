# Contrato Normativo: MeasurementRecipe y Cadena de Ejecución Metrológica

**Versión del Esquema:** 1.0  
**Fecha:** 2026-09-23  
**Estado:** VIGENTE / NORMATIVO  
**Referencia:** [RFC 8785 (JSON Canonicalization Scheme)](https://www.rfc-editor.org/info/rfc8785/) | [RFC 6901 (JSON Pointer)](https://www.rfc-editor.org/info/rfc6901/)

---

## 1. Principios Rectores

1. **Separación de Responsabilidades:**
   - `MeasurementRecipe` especifica **qué** experimento científico realizar de manera puramente declarativa y reproducible.
   - `ExecutionRequest` especifica **cómo y cuándo** se solicita la corrida (target, modo, operador, destino).
   - `ExecutionEnvironment` describe el **entorno físico** (tanto el `rawEnvironment` observado como el `canonicalEnvironment` normalizado) resuelto en tiempo de ejecución (driver, buffers, latencia).
   - `ExecutionRecord` captura el **resultado inmutable** con la traza de hashes criptográficos.
2. **Inmutabilidad y Trazabilidad:**
   - Una receta nunca almacena resultados de ejecución.
   - Una ejecución nunca altera el archivo de receta original.
3. **Rechazo Estricto de Propiedades Desconocidas (`additionalProperties: false`):**
   - Cualquier clave no definida explícitamente en el esquema provoca el rechazo inmediato del documento con diagnóstico JSON Pointer. Queda terminantemente prohibido ignorar propiedades desconocidas en documentos ejecutables.

---

## 2. La Cadena Canónica de Hashes de Cuatro Niveles

```
MeasurementRecipe
  → recipeDocumentHash
        ↓
ExperimentPlan (Receta + defaults normativos)
  → experimentPlanHash
        ↓
ResolvedExecutionPlan (ExperimentPlan + TargetCapabilities + ExecutionEnvironment)
  → resolvedExecutionPlanHash
        ↓
ExecutionRecord (Audio capturado)
  → audioEvidenceHash
```

| Hash | Qué identifica | Algoritmo y Dependencias | Propósito Metrológico |
|---|---|---|---|
| **`recipeDocumentHash`** | El documento exacto archivado | SHA-256 sobre JSON normalizado RFC 8785 (invariante ante el orden de claves). | Integridad documental y archivo versionado. |
| **`experimentPlanHash`** | El experimento científico puro | SHA-256 binario canónico del `ExperimentPlan` (con defaults normativos, agnóstico al hardware). | Responde: *«¿Es el mismo experimento científico?»* |
| **`resolvedExecutionPlanHash`** | La realización física ejecutable | SHA-256 de `ResolvedExecutionPlan` (`ExperimentPlan` + target resuelto + driver canonicalizado + sample rate + block size + canales + capabilities ordenadas). | Responde: *«¿Se ejecutó con la misma configuración concreta de entorno?»* |
| **`audioEvidenceHash`** | La evidencia acústica observada | SHA-256 sobre muestras de audio crudas Float32 IEEE-754 Little-Endian en `ExecutionRecord`. | Certificación bit-exacta de señales acústicas. |

---

## 3. Política de Versionado y Migración

1. **Versiones Soportadas:**
   - La implementación actual soporta **exclusivamente `schemaVersion = "1.0"`**.
   - Cualquier versión distinta de `"1.0"`, incluidas versiones futuras `1.x`, se rechaza hasta que una versión posterior del software declare de forma explícita su compatibilidad.
   - Nunca se realiza inferencia por aproximación ni interpretación parcial.
2. **Política de Compatibilidad Futura:**
   - Una futura versión `1.x` podrá añadir únicamente campos opcionales, sin alterar semántica ni defaults normativos existentes.
   - Versiones mayores (`2.0`) requerirán migración explícita.
3. **Migración No Destructiva:**
   - La migración siempre crea un documento nuevo; nunca sobreescribe el original.
   - Todo documento migrado debe poblar la sección `provenance` con:
     - `migratedFromSchemaVersion`
     - `migrationToolVersion`
     - `sourceRecipeDocumentHash`

---

## 4. Diagnóstico de Errores mediante JSON Pointer (RFC 6901)

Todos los diagnósticos emitidos por el validador estructurado reportan:
- `severity`: `Error` | `Warning`
- `jsonPointer`: Ruta exacta de la anomalía (ej. `/excitation/notes/0/gateMs`)
- `code`: Código canónico de error (ej. `ERR_SCHEMA_UNKNOWN_FIELD`, `ERR_SEMANTICS_INVALID_RANGE`, `ERR_CAPABILITY_SAMPLE_RATE_UNSUPPORTED`)
- `message`: Explicación concisa y accionable para humanos y asistentes LLM.

---

## 5. Reglas Normativas de Canonicalización del Entorno (`ExecutionEnvironment`)

Para que `resolvedExecutionPlanHash` no sufra variaciones incidentales derivadas de diferencias de formato, espacios, rutas o mayúsculas entre plataformas, y para preservar la evidencia forense completa en `ExecutionRecord`:

1. **Separación de Entorno Observado y Canonicalizado:**
   - `rawEnvironment`: almacena fielmente lo que informó el sistema operativo, host, driver o plugin (ej. `driver = "ASIO4ALL v2"`).
   - `canonicalEnvironment`: almacena la representación normalizada y cerrada utilizada para el cálculo de `resolvedExecutionPlanHash` (ej. `driver = "ASIO"`).
   - **Exclusión de Rutas Personales/Temporales:** Queda terminantemente prohibido incorporar rutas absolutas personales (`C:\Users\...`) o directorios temporales en el blob canonicalizado. Solo se admiten identificadores lógicos o stable-IDs independientes de la ubicación del archivo.
2. **Codificación de Cadenas:**
   - Todas las cadenas de texto se codifican en **UTF-8 sin BOM**.
   - Se realiza *trim* de espacios en los extremos y colapso de espacios en blanco consecutivos.
3. **Enumerados Cerrados de Driver:**
   - El campo canonicalizado `driver` debe pertenecer al conjunto cerrado normativo:
     `"MockAudioEngine"`, `"WASAPI"`, `"ASIO"`, `"DirectSound"`, `"CoreAudio"`, `"ALSA"`, `"JACK"`.
   - Cualquier variación o alias se mapea a este conjunto antes de calcular el hash.
4. **Ordenamiento de Capacidades (`discoveredCapabilities`):**
   - La lista de capacidades descubiertas se ordena lexicográficamente según el código ASCII de sus bytes antes de serializar:
     `std::sort(caps.begin(), caps.end())`.
5. **Identificador Estable de Plugin / Target:**
   - Si el target es un plugin VST3, se utiliza su identificador lógico o su UID canonicalizado en minúsculas (ej. `vst3-dexed-3f015740-d7709eec`).
   - Las rutas a binarios se normalizan con barras inclinadas (`/`), sin trailing slashes y en minúsculas en Windows.
6. **Versión de Plugin y Arquitectura:**
   - La versión de plugin se normaliza en formato semántico (`X.Y.Z`).
   - La arquitectura binaria se normaliza a etiquetas canónicas: `"x86_64"`, `"arm64"`, `"x86"`.
7. **Política para Campos Ausentes o Nulos:**
   - Se distingue estrictamente entre:
     - `"unknown"`: Propiedad aplicable pero no detectada por el sistema.
     - `"not-applicable"`: Propiedad que no aplica al target (ej. latencia analógica en fixture sintético).
     - `""` (cadena vacía): Ausencia de valor no permitida en campos normativos.
8. **Formato Canónico del Blob para `resolvedExecutionPlanHash`:**
   ```
   <experimentPlanHash>\n
   <canonicalDriver>\n
   <sampleRate>\n
   <blockSize>\n
   <channels>\n
   <deviceName>\n
   <pluginBinary>\n
   <pluginVersion>\n
   <latencySamples>\n
   CAP:<capability_1>\n
   CAP:<capability_2>\n
   ...
   ```

---

## 6. Firmas Normativas de Compilación y Ejecución (C++20)

La transformación de recetas a planes de ejecución es estrictamente **unidireccional** y determinista:

```cpp
namespace abdaudiolab::profiling
{

// 1. Compilación determinista de receta a plan científico puro (agnóstico al hardware)
synth::ExperimentPlan ExperimentPlanCompiler::compileToExperimentPlan(
    const MeasurementRecipe& recipe,
    const CompilationDefaults& defaults = {});

// 2. Realización ejecutable ligando el plan al entorno físico y capacidades
ResolveExecutionPlanResult ExperimentPlanCompiler::resolveExecutionPlan(
    const synth::ExperimentPlan& plan,
    const ExecutionEnvironment& env,
    const MeasurementRecipe* sourceRecipe = nullptr);

// 3. Puente compatible hacia el pipeline existente de ejecución
core::ProfilingSession ExperimentPlanCompiler::createProfilingSession(
    const ResolvedExecutionPlan& resolvedPlan,
    const gui::session::TargetSelectionState& targetState);

} // namespace abdaudiolab::profiling
```

*Regla inviolable:* No existe transformación inversa implícita `ExperimentPlan -> MeasurementRecipe`.
