# PLAN OPERATIVO: HITO-10 — TargetProfile Declarativo en JSON

**Estado:** 🟢 APROBADO CON AJUSTES NORMATIVOS PARA IMPLEMENTACIÓN  
**Fecha:** 2026-09-24  
**Proyecto:** ABDAudioLab  
**Base Certificada:** HITO-09D (Build #430 — 652 test cases | 644 PASS | 8 SKIPPED | 0 FAIL | 229.351 assertions)  
**Autoridad de Planificación:** Antigravity (Lead Architect & Metrologist) / ajabadia (Tándem Partner)

---

## 1. Frase Rectora y Ecuación Fundamental

> **«La receta expresa la intención científica; el perfil declara cómo ese target puede realizarla; el entorno confirma si puede hacerlo hoy; el motor común sigue siendo el único que ejecuta.»**

La resolución canónica desacopla de forma total la intención de medición respecto al target concreto:

$$\underbrace{\text{MeasurementRecipe}}_{\text{Qué se quiere medir (semanticId)}} \;+\; \underbrace{\text{TargetProfile}}_{\text{Cómo lo mapea el Target (transporte)}} \;+\; \underbrace{\text{ExecutionEnvironment}}_{\text{Dónde se ejecuta hoy}} \;\implies\; \underbrace{\text{ResolvedExecutionPlan}}_{\text{Secuencia física ejecutable}}$$

### Regla de Oro del Compilador y Cadena de Autoridad
```
TargetProfileService       --> Carga, valida y canonicaliza TargetProfile
MeasurementRecipeService   --> Carga, valida y canonicaliza MeasurementRecipe
ExperimentPlanCompiler     --> ÚNICA AUTORIDAD: combina Recipe + Profile + Environment
                               y produce ResolvedExecutionPlan
ProfilingSessionController --> Recibe la sesión preparada
ProfilingSequencer         --> Ejecuta la sesión en el hilo de audio en tiempo real
```

1. **El compilador conoce tipos de transporte:** `InternalParameter`, `VST3Parameter`, `MidiContinuousController`, `MidiSysEx`, `ManualOperator`.
2. **El perfil contiene identificadores técnicos del target:** estructuras tipadas por variante discriminada.
3. **La receta utiliza exclusivamente `semanticId`:** `filter_cutoff`, `filter_resonance`, `vca_attack`, etc.
4. **Cero decisiones por nombre de plugin dentro del compilador:**  
   Queda terminantemente prohibido cualquier patrón tipo `if (targetName == "Dexed") { param = 24; }`. La resolución es estrictamente declarativa por consulta al `TargetProfile`.

---

## 2. Separación de las Tres Identidades

| Nivel de Identidad | Campo | Ejemplo | Función Metrológica |
|---|---|---|---|
| **Contrato Declarativo** | `targetProfileId` | `org.abd.reference-synth.v1` | Identifica el esquema y contrato declarativo del perfil. |
| **Familia o Target Lógico** | `canonicalTargetId` | `reference-synth` o `vst3-dexed-3f015740-d7709eec` | Identifica la familia o dispositivo abstracto al que aplica el perfil. |
| **Instancia Concreta de Ejecución** | `ExecutionEnvironment` | SHA-256 binario, versión, path, buffer, driver | Identifica el binario o dispositivo físico cargado hoy en memoria. |

**Política de Fixity Binaria y Compatibilidad:**
- El `TargetProfile` declara reglas de compatibilidad (`acceptedUniqueIds`, `binaryIdentityPolicy`).
- El `ExecutionEnvironment` registra la evidencia del binario concreto.
- El preflight decide si el binario es aceptado, genera advertencia (`ApprovedWithWarnings`) o requiere auditoría de fixity (`BinaryChanged`). La actualización de un plugin no destruye el perfil declarativo.

---

## 3. Secuencia de Despliegue en Cinco Sub-Hitos

En estricta observancia del principio *«Caracterizar primero $\to$ comparar después $\to$ migrar después $\to$ retirar al final»*, se separan nítidamente las fases:

| Sub-Hito | Denominación | Alcance y Frontera |
|:---:|---|---|
| **HITO-10A** | **TargetProfile Contract & ReferenceSynth Pilot** | • Esquema JSON Schema Draft 2020-12.<br>• Modelo C++ inmutable con `TechnicalIdentifier` tipado en `std::variant`.<br>• `TargetProfileService` (carga, validación, hash RFC 8785).<br>• Sobrecarga de resolución en `ExperimentPlanCompiler`.<br>• Perfil piloto `reference_synth.target.json`.<br>• Resolución declarativa aislada y preflight (4 suites).<br>*Frontera:* NO reemplaza la resolución legacy ni exige paridad exacta aún. |
| **HITO-10B** | **Paridad Declarativa y Preflight contra Legacy** | • Congelar la ruta legacy como oráculo en `test_TargetProfileLegacyCharacterization.cpp`.<br>• Adaptador de comparación semántica de eventos, offsets, ventanas y políticas.<br>• Verificación de igualdad exacta de `ResolvedExecutionPlan` y `resolvedExecutionPlanHash` en `test_TargetProfileLegacyParity.cpp`.<br>• Preservar fallback legacy. |
| **HITO-10C** | **Migración Piloto de Plugin Real (Dexed VST3)** | • Perfil `dexed.target.json` con `Vst3ParameterIdentifier`.<br>• Mapeo de parámetros VST3 y auditoría de fixity binaria.<br>• Prueba A/B en render offline bit a bit. |
| **HITO-10D** | **Hardware Digital (CC/SysEx) y Analógico Manual** | • Perfiles para hardware outboard con `MidiCcIdentifier`, `MidiSysExIdentifier` y `ManualOperatorIdentifier` (tarjetas guiadas de operador con settling). |
| **HITO-10E** | **Retirada Segura de Duplicados Legacy** | • Deprecación controlada del cableado ad-hoc en `HardwareContractRegistry`, manteniendo fallbacks de compatibilidad y verificando cero regresiones en la suite global. |

---

## 4. Diseño Técnico de HITO-10A

### 4.1. Esquema Normativo (`docs/contracts/target-profile.schema.json`)
- JSON Schema Draft 2020-12 con `"additionalProperties": false`.
- Valida tipos de datos, rangos normalizados, enum de `targetKind` (`SyntheticFixture`, `PluginVST3`, `HardwareDigital`, `HardwareAnalogue`), transporte y capacidades de canal explícitas.

### 4.2. Perfil Piloto (`profiles/targets/reference_synth.target.json`)
```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "schemaVersion": "1.0",
  "kind": "abd.target-profile",

  "targetProfileId": "org.abd.reference-synth.v1",
  "displayName": "Reference Synth",
  "vendor": "ABD AudioLab",
  "targetKind": "SyntheticFixture",
  "revision": 1,

  "identity": {
    "canonicalTargetId": "reference-synth",
    "acceptedUniqueIds": [
      "reference-synth-v1"
    ],
    "binaryIdentityPolicy": "not-applicable"
  },

  "capabilities": {
    "midiInput": true,
    "supportsParameterAutomation": true,
    "controlTransports": [
      "InternalParameter"
    ],
    "audioOutput": {
      "supportedChannelCounts": [2],
      "requiredChannelCount": 2,
      "channelLayout": "stereo",
      "supportedObservationLayouts": ["stereo"]
    },
    "sampleRatesHz": [44100, 48000, 96000],
    "blockSizes": [64, 128, 256, 512],
    "supportsPolyphony": true,
    "midiChannels": [1],
    "midiNoteRange": [0, 127]
  },

  "parameters": [
    {
      "semanticId": "filter_cutoff",
      "displayName": "Filter Cutoff",
      "technicalIdentifier": {
        "kind": "InternalParameter",
        "parameterKey": "filter.cutoff"
      },
      "valueType": "continuous",
      "normalizedRange": [0.0, 1.0],
      "mappingCurve": {
        "kind": "linear"
      },
      "confirmationStatus": "Declared"
    },
    {
      "semanticId": "filter_resonance",
      "displayName": "Filter Resonance",
      "technicalIdentifier": {
        "kind": "InternalParameter",
        "parameterKey": "filter.resonance"
      },
      "valueType": "continuous",
      "normalizedRange": [0.0, 1.0],
      "mappingCurve": {
        "kind": "linear"
      },
      "confirmationStatus": "Declared"
    }
  ],

  "measurementPolicies": {
    "warmupTimeMs": 0,
    "defaultSettlingTimeMs": 50,
    "recommendedCalibrationPolicy": "None",
    "requiresResetBetweenTrials": false
  }
}
```

### 4.3. Estructuras C++ con Identificadores Tipados (`src/profiling/TargetProfile.h`)
```cpp
namespace abdaudiolab::profiling
{

enum class ControlTransportKind
{
    InternalParameter,
    VST3Parameter,
    MidiContinuousController,
    MidiSysEx,
    ManualOperator
};

// Identificadores técnicos fuertemente tipados por transporte (cero ambigüedad)
struct InternalParameterIdentifier
{
    std::string parameterKey;
};

struct Vst3ParameterIdentifier
{
    int32_t parameterIndex { -1 };
    std::string parameterId;
};

struct MidiCcIdentifier
{
    int channel { 1 };
    int controllerNumber { 1 };
};

struct MidiSysExIdentifier
{
    std::string messageTemplate;
    std::string valueEncoding { "7bit" };
};

struct ManualOperatorIdentifier
{
    std::string instructionId;
    std::string confirmationPrompt;
};

using TechnicalIdentifier = std::variant<
    InternalParameterIdentifier,
    Vst3ParameterIdentifier,
    MidiCcIdentifier,
    MidiSysExIdentifier,
    ManualOperatorIdentifier>;

struct TargetParameterMapping
{
    std::string semanticId;
    std::string displayName;
    TechnicalIdentifier technicalIdentifier;
    std::string valueType { "continuous" };
    std::pair<double, double> normalizedRange { 0.0, 1.0 };
    std::string mappingCurve { "linear" };
    std::string confirmationStatus { "Declared" };

    [[nodiscard]] ControlTransportKind getTransportKind() const noexcept
    {
        return static_cast<ControlTransportKind>(technicalIdentifier.index());
    }
};

struct TargetProfile
{
    std::string schemaVersion { "1.0" };
    std::string kind { "abd.target-profile" };
    std::string targetProfileId;
    std::string displayName;
    std::string vendor;
    std::string targetKind;
    int revision { 1 };

    struct Identity
    {
        std::string canonicalTargetId;
        std::vector<std::string> acceptedUniqueIds;
        std::string binaryIdentityPolicy { "not-applicable" };
    } identity;

    struct AudioOutputCapabilities
    {
        std::vector<int> supportedChannelCounts { 2 };
        int requiredChannelCount { 2 };
        std::string channelLayout { "stereo" };
        std::vector<std::string> supportedObservationLayouts { "stereo" };
    };

    struct Capabilities
    {
        bool midiInput { true };
        bool supportsParameterAutomation { true };
        std::vector<ControlTransportKind> controlTransports;
        AudioOutputCapabilities audioOutput;
        std::vector<int> sampleRatesHz { 44100, 48000, 96000 };
        std::vector<int> blockSizes { 64, 128, 256, 512 };
        bool supportsPolyphony { true };
        std::vector<int> midiChannels { 1 };
        std::pair<int, int> midiNoteRange { 0, 127 };
    } capabilities;

    std::vector<TargetParameterMapping> parameters;

    struct MeasurementPolicies
    {
        int warmupTimeMs { 0 };
        int defaultSettlingTimeMs { 50 };
        std::string recommendedCalibrationPolicy { "None" };
        bool requiresResetBetweenTrials { false };
    } measurementPolicies;

    [[nodiscard]] const TargetParameterMapping* findMappingForSemanticId(const std::string& semanticId) const noexcept;
};

} // namespace abdaudiolab::profiling
```

### 4.4. Servicio Desacoplado (`TargetProfileService.h` / `.cpp`)
El servicio se limita estrictamente a la autoridad de perfiles (cero compilación paralela):
```cpp
class TargetProfileService
{
public:
    TargetProfileService() = default;
    ~TargetProfileService() = default;

    [[nodiscard]] TargetProfileLoadResult loadAndValidateProfile(const juce::File& file) const;
    [[nodiscard]] TargetProfileLoadResult loadAndValidateProfileJson(const std::string& jsonString) const;
    [[nodiscard]] std::string computeCanonicalProfileHash(const TargetProfile& profile) const;
    [[nodiscard]] const TargetParameterMapping* findMapping(const TargetProfile& profile,
                                                            std::string_view semanticId) const noexcept;
};
```

### 4.5. Ampliación del Compilador Existente (`ExperimentPlanCompiler`)
La compilación se mantiene en su autoridad única:
```cpp
// Sobrecarga declarativa en ExperimentPlanCompiler.h
static ResolveExecutionPlanResult resolveExecutionPlan(
    const MeasurementRecipe& recipe,
    const TargetProfile& profile,
    const ExecutionEnvironment& environment,
    const CompilationDefaults& defaults = {});
```

---

## 5. Matriz de Pruebas Normativas para HITO-10A

Se implementan 4 suites de pruebas herméticas (las de caracterización y paridad legacy quedan diferidas a HITO-10B):

### Suite 1: Parsing Contractual (`test_TargetProfileParsing.cpp`)
1. Carga válida de `reference_synth.target.json`.
2. Extracción fiel de campos de identidad, capacidades, parámetros con `InternalParameterIdentifier` y políticas.
3. Deserialización estricta de `TechnicalIdentifier` como `std::variant`.

### Suite 2: Validación Estructural y Semántica (`test_TargetProfileValidation.cpp`)
4. Rechazo de JSON malformado o con sintaxis inválida.
5. Rechazo de campos desconocidos (`additionalProperties: false`).
6. Rechazo de `schemaVersion` incompatible.
7. Detección y rechazo de `semanticId` duplicado dentro del perfil.
8. Detección de `normalizedRange` inválido (`min > max` o fuera de $[0, 1]$).
9. Detección de `midiNoteRange` invertido o fuera de $[0, 127]$.
10. Determinismo de hash canónico RFC 8785: claves desordenadas producen exactamente el mismo `canonicalProfileHash`.

### Suite 3: Resolución Declarativa y Preflight Aislado (`test_TargetProfileResolution.cpp`)
11. Resolución válida: `MeasurementRecipe` (`filter_cutoff`) + `TargetProfile` (`ReferenceSynth`) + Entorno compatible (48 kHz / 2 canales) $\to$ `ResolvedExecutionPlan` válido.
12. Diagnóstico de parámetro no mapeado: receta pidiendo `filter_drive` ausente en el perfil emite diagnóstico RFC 6901 `ERR_TARGET_PROFILE_SEMANTIC_ID_UNMAPPED`.
13. Diagnóstico de entorno incompatible: sample rate 192 kHz no soportado por el perfil emite `ERR_CAPABILITY_SAMPLE_RATE_UNSUPPORTED`.
14. Diagnóstico de canales incompatibles: receta mono frente a target estéreo exclusivo emite `ERR_CAPABILITY_CHANNELS_UNSUPPORTED`.

### Suite 4: Invarianza y No-Bifurcación (`test_TargetProfileNoBypass.cpp`)
15. Invarianza: La carga o inspección de un `TargetProfile` NO crea `ProfilingSession`, no inicia audio y no despacha mensajes MIDI.
16. Jerarquía canónica: `TargetProfile` es un consumidor pasivo de metadatos para `ExperimentPlanCompiler`; la ejecución física pertenece con exclusividad a `ProfilingSessionController` $\to$ `ProfilingSequencer`.

---

## 6. Criterio de Salida de HITO-10A

1. Archivos normativos creados:
   - `docs/contracts/target-profile.schema.json`
   - `docs/contracts/TARGET_PROFILE_CONTRACT.md`
   - `profiles/targets/reference_synth.target.json`
   - `src/profiling/TargetProfile.h`
   - `src/profiling/TargetProfileService.h`
   - `src/profiling/TargetProfileService.cpp`
2. Modificaciones realizadas:
   - Sobrecarga declarativa en `src/profiling/ExperimentPlanCompiler.h` y `.cpp`.
   - Registro en `CMakeLists.txt`.
3. Batería de 4 suites aprobada al 100% (PASS, 0 FAIL).
4. Compilación limpia en `Release x64` (`build.bat Release`).
5. Cero modificaciones en `ProfilingSequencer` ni en el motor DSP.
6. Emisión del acta formal: `ACTA_HITO_10A_TARGET_PROFILE_CONTRACT.md`.
