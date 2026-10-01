#pragma once

#include <string>
#include <utility>
#include <vector>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <juce_core/juce_core.h>
#include <nlohmann/json.hpp>
#include "HardwareContractQuarantine.h"

namespace abdaudiolab::core
{

struct HardwareControl
{
    int index { 1 };
    std::string name;
    std::string type; // "Slider", "Knob", "Switch", "RotarySwitch", "Button", "MidiCC", "Normalized"
    std::string controlMethod { "MANUAL" }; // "MANUAL", "MIDI_CC", "NRPN", "SYSEX"
    int ccNumber { -1 };
    int nrpnNumber { -1 };
    std::string sysexAddress; // e.g. "10 00 00 01"
    float minVal { 0.0f };
    float maxVal { 1.0f };
    float defaultVal { 0.5f };
    std::string unit;
    std::vector<std::string> options; // For switches / selectors
};

struct HardwareRoutingGuide
{
    std::string stimulusOutput; // e.g. "Audio Out 1 (L) -> GATE IN (+5V pulse)"
    std::string responseInput;  // e.g. "ENV 1 OUT -> Audio In 1 (L)"
    std::string notes;          // e.g. "Manual gate button can also be used"
};

enum class HardwareMethod
{
    MIDI_CC,
    NRPN,
    SYSEX_RAW,
    MANUAL_PROMPT
};

struct HardwareSetupAction
{
    std::string description;
    HardwareMethod method { HardwareMethod::MIDI_CC };
    int channel { 1 };          // MIDI channel [1..16]
    int controlNumber { -1 };   // CC or NRPN Parameter number
    float normalizedValue { 0.0f };
    std::string sysexHexPayload; // Hex string format: "F0 01 2F ... F7"
    int settlingDelayMs { 50 };  // Physical settling delay for circuits
};

/**
 * @brief Lee una lista JSON de `setupActions` en un vector de acciones.
 *
 * Vive aqui, y no dentro de ninguno de sus dos consumidores, porque hay DOS
 * puertas al catalogo y las dos necesitan exactamente la misma lectura: el
 * registro local y `SharedHardwareContractAdapter`. Estaba escrita dos veces,
 * byte a byte, como dos lambdas locales. Dos copias de una regla de parseo es
 * exactamente lo que se desincroniza sin ruido: una acepta un alias que la otra
 * ignora, y el contrato se comporta distinto segun por donde entre.
 *
 * El que la regla sobre `quarantine::evaluar` este compartida y esta no era
 * una contradiccion que se hubiera dejado a proposito: la primera se separo
 * porque un retenido decidirlo de dos maneras es un fallo de seguridad, y esta
 * se dejo porque aun no habia dado ningun problema.
 *
 * No es una API: es la lectura del formato, y por eso se declara al lado de los
 * tipos que devuelve en vez de dentro de la clase del registro.
 *
 * Lo que NO hace, a proposito: no reporta. Si el array no es un array o un
 * elemento no es un objeto, se los salta. Un contrato con un `lifecycle` roto
 * tiene que llegar a quien carga el contrato, que es donde si hay donde avisar;
 * aqui un aviso seria una segunda voz diciendo lo mismo con otra redaccion.
 */
void parseSetupActions (const nlohmann::json& arrJson,
                        std::vector<HardwareSetupAction>& actions);

struct NoteSequenceEvent
{
    int noteNumber { 60 };
    int velocity { 100 };
    int startDelayMs { 0 };
    int durationMs { 1000 };
    bool isLegato { false };
};

enum class ExcitationMode
{
    MidiNotes,      // Instrument plugins, hardware synths triggered by MIDI note-on/off
    AudioSweep,     // Audio effects, loopback, pedals, filter inputs
    ManualCapture,  // Manual hardware, Eurorack without auto-stimulus
    ExternalSignal  // Passive line-in recording
};

struct MeasurementPresetRecipe
{
    std::string recipeType; // e.g. "INTERNAL_NOISE_EXCITATION", "LEGATO_PITCH_SWEEP", "DIRECT_AUDIO_IN", "BULK_SYSEX_DUMP", "MANUAL_PATCH"
    std::string description;
    ExcitationMode excitationMode { ExcitationMode::AudioSweep };
    std::vector<HardwareSetupAction> setupActions;
    std::vector<NoteSequenceEvent> excitationNotes;
    int postSettlingDelayMs { 100 };
};

struct HardwareLifecycleContract
{
    std::vector<HardwareSetupAction> preCalibrationSetup;
    std::vector<HardwareSetupAction> preSessionSetup;
    std::vector<HardwareSetupAction> postSessionTeardown;
};

struct HardwareFunction
{
    std::string id;
    std::string name;
    std::string blockType; // "TimeDynamic", "SpectrumFilter", "WaveShaper", "CyclicModulator", "AmplitudeGain"
    std::string suggestedStimulus; // "GATE_PULSE", "LOG_SINE_SWEEP", "MULTILEVEL_RAMP", "SILENT_CAPTURE", "MULTI_CYCLE_MODULATION"
    ExcitationMode excitationMode { ExcitationMode::AudioSweep };
    std::string captureMode { "FIXED_TIME" }; // "FIXED_TIME", "ADAPTIVE_ENVELOPE", "INTEGRATED_TAIL"
    float defaultBurstDurationSec { 1.0f };
    float maxTimeoutSec { 60.0f };
    float silenceThresholdDb { -60.0f };
    HardwareRoutingGuide routingGuide;
    std::vector<HardwareControl> controls;
    MeasurementPresetRecipe measurementRecipe;
};

struct MidiIdentityContract
{
    std::string manufacturer;
    std::string manufacturerIdHex;         // e.g. "41", "42", "43", "44", "00 20 32"
    std::string model;
    std::string modelIdHex;                // e.g. "15", "58", "5A", "20", "2C", "09", "01"
    std::string familyIdHex;               // e.g. "00 00", "32 00"
    std::string sysexHeaderHex;            // e.g. "41 10 00 00 00 15" or "00 20 32 20"
    std::vector<std::string> portNameMatches; // Port substring keywords
};

struct HardwareContract
{
    std::string schemaVersion { "2.0" };
    std::string id;
    std::vector<std::string> aliases;
    std::string displayName;
    std::string description;
    std::string deviceType; // "MANUAL_EURORACK", "ANALOGUE_PEDAL", "AUTOMATED_SYSEX", "AUTOMATED_MIDI_CC", "VIRTUAL_LOOPBACK_ASIO", "MOCK_DSP", "SOFTWARE_PLUGIN"
    std::string brand;
    std::string brandLogo;
    std::string modelImage;
    std::string manufacturer;
    std::string model;
    std::string modelIdHex;
    std::string autoDetectSysEx;
    std::string theme { "audiolab-light" };

    /**
     * Estado editorial del contrato, leido del propio JSON.
     *
     * `quarantined` significa que se sabe dudoso y por eso el registro no lo
     * carga. Va DENTRO de la estructura, y no solo en una lista del registro,
     * para que un contrato retenido siga siendo identificable como tal en
     * cualquier consumidor que lo reciba por otra via: el adaptador compartido,
     * un snapshot de la sesion, un informe. Si el estado viviera solo en la
     * lista, cualquier copia del contrato pareceria un contrato normal.
     */
    std::string status;
    std::string statusReason;

    MidiIdentityContract midiIdentity;

    HardwareLifecycleContract lifecycle;
    std::vector<HardwareFunction> functions;
};

enum class CanonicalTargetProfileLoadOutcome
{
    Loaded,
    TargetDirectoryMissing,
    TargetProfileInvalid,
    CanonicalAliasCollision,
    CanonicalLegacyParityUnproven,
    RegistryUnchanged
};

struct CanonicalTargetProfileLoadResult
{
    CanonicalTargetProfileLoadOutcome outcome { CanonicalTargetProfileLoadOutcome::RegistryUnchanged };
    std::size_t profilesDiscovered { 0 };
    std::size_t profilesLoaded { 0 };
    std::vector<std::string> diagnosticCodes;
    std::vector<std::string> diagnosticMessages;
};

enum class HardwareContractResolutionSource
{
    CanonicalTargetProfileAdapted,
    NativeLegacyContract,
    CanonicalLegacyParityViolation,
    CanonicalProfileInvalid,
    LegacyProfileInvalid,
    NotFound
};

struct HardwareContractResolution
{
    std::optional<HardwareContract> contract;
    HardwareContractResolutionSource source { HardwareContractResolutionSource::NotFound };
    std::string diagnosticCode;
    std::string diagnosticMessage;
};

class HardwareContractRegistry
{
public:
    HardwareContractRegistry() = default;
    ~HardwareContractRegistry() = default;

    bool loadContractsFromDirectory(const juce::File& contractsDir);
    bool loadProfileResilient(const juce::File& jsonFile, HardwareContract& outContract, juce::String& outWarning);

    CanonicalTargetProfileLoadResult loadCanonicalTargetProfiles(const juce::File& targetsDir);

    std::function<void(const juce::String& warningMsg)> onProfileWarning;

    [[nodiscard]] const std::vector<HardwareContract>& getContracts() const noexcept { return effectiveContracts; }
    [[nodiscard]] bool hasContracts() const noexcept { return !effectiveContracts.empty(); }
    [[nodiscard]] const std::vector<juce::String>& getWarnings() const noexcept { return warnings; }

    /**
     * @brief Los contratos RETENIDOS por cuarentena, con su motivo.
     *
     * Deliberadamente aparte de `invalidLegacyProfiles`. Los dos son ficheros
     * que no se han cargado, pero no son lo mismo: uno esta roto y uno esta
     * withheld a proposito. Juntarlos seria esconder una decision editorial
     * dentro de un contador de errores, que es justo la confusion que hace
     * que alguien lo "arregle" reescribiendo el contrato.
     *
     * Cada uno lleva su `fichero`, que es lo que permite que el cajon abra el
     * JSON a editar. Un retenido sin path del que hablar, solo se puede nombrar.
     */
    [[nodiscard]] const std::vector<quarantine::Retenido>& getQuarantinedProfiles() const noexcept
    {
        return quarantinedProfiles;
    }

    [[nodiscard]] HardwareContractResolution resolveContractById(const std::string& id) const;
    [[nodiscard]] const HardwareContract* findContractById(const std::string& id) const noexcept;

    [[nodiscard]] const std::vector<HardwareContract>& getCanonicalAdaptedContracts() const noexcept { return canonicalAdaptedContracts; }
    [[nodiscard]] std::size_t getCanonicalAdaptedContractCount() const noexcept { return canonicalAdaptedContracts.size(); }
    [[nodiscard]] const std::string& getLastError() const noexcept { return lastErrorMessage; }

    void registerContract(const HardwareContract& contract)
    {
        for (auto& c : contracts)
        {
            if (c.id == contract.id)
            {
                c = contract;
                rebuildEffectiveContracts();
                return;
            }
        }
        contracts.push_back(contract);
        rebuildEffectiveContracts();
    }

    bool unregisterContract(const std::string& id)
    {
        auto it = std::remove_if(contracts.begin(), contracts.end(),
            [&](const HardwareContract& c) { return c.id == id; });
        if (it != contracts.end())
        {
            contracts.erase(it, contracts.end());
            rebuildEffectiveContracts();
            return true;
        }
        return false;
    }

private:
    void rebuildEffectiveContracts();

    std::vector<HardwareContract> contracts;
    std::vector<HardwareContract> canonicalAdaptedContracts;
    std::vector<HardwareContract> effectiveContracts;
    std::unordered_map<std::string, std::size_t> canonicalIdToIndex;
    std::unordered_map<std::string, std::string> aliasToCanonicalId;
    std::unordered_map<std::string, std::string> invalidCanonicalProfiles;
    std::unordered_set<std::string> invalidLegacyProfiles;
    std::vector<quarantine::Retenido> quarantinedProfiles;
    std::vector<juce::String> warnings;
    std::string lastErrorMessage;
};

} // namespace abdaudiolab::core
