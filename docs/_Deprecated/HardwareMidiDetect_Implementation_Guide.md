# ABDAudioLab — HardwareMidiDetect Implementation Guide

> **Objective:** Integrate the shared `HardwareMidiDetect` module (contract-driven MIDI hardware detection, C++ pure layer + WebView2 picker) into ABDAudioLab in under 10 minutes with zero contract duplication.
>
> **Module:** `ABDShared::HardwareMidiDetect`  
> **Contracts Source:** `ABDSharedAssets/contracts/hardware/*.json` (single source of truth)

---

## 1. CMake Build Configuration

ABDAudioLab already adds JUCE + nlohmann_json before `add_subdirectory(ABDSharedCode)`. Only the link is needed:

```cmake
# In ABDAudioLab/CMakeLists.txt, after add_subdirectory(ABDSharedCode ...)
target_link_libraries(ABDAudioLab PRIVATE
    ABDShared::HardwareMidiDetect
)
```

The embedded WebUI assets (`HardwareMidiPickerAssets`) are generated automatically via `juce_add_binary_data` inside `ABDSharedCode/CMakeLists.txt` because JUCE is in scope. No extra CMake required.

---

## 2. Architecture Overview (ABDScope-Style)

```
┌─────────────────────────────────────────────────────────────────────┐
│                        ABDAudioLab (Host)                           │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ AudioLabMidiBackend  ← implements MidiHardwareBackend        │   │
│  │   • MidiOutput / MidiInput (JUCE)                            │   │
│  │   • sendBytes / startListening / handleIncomingMidiMessage   │   │
│  └──────────────────────────┬──────────────────────────────────┘   │
│                             │ injects backend                        │
│                             ▼                                        │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ JuceHardwareMidiPicker  (WebView2 Component)                │   │
│  │   • Embedded WebUI (index.html + JS) served from binary     │   │
│  │   • nativeEvent channel: hardware.send / listen / stop      │   │
│  │   • Callback: HardwarePickResult { cancelled, hardwareId,   │   │
│  │       displayName, manufacturer, model, firmwareVersion }   │   │
│  └──────────────────────────┬──────────────────────────────────┘   │
│                             │ result callback                        │
│                             ▼                                        │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ SharedHardwareContractAdapter  ← wraps shared registry       │   │
│  │   • Loads contracts via shared registry                      │   │
│  │   • Exposes LOCAL types with functions/controls populated    │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 3. Shared Contract Registry — Wire Protocol

The shared module loads only **identity fields** from each contract JSON. Product-specific sections (`functions`, `controls`, `routingGuide`) remain in raw JSON and are accessed via `getRawContractJson(id)`.

### 3.1 Shared Contract Structure (`abd::hwid::HardwareContract`)

| Field | Type | Source JSON Key | Description |
|-------|------|-----------------|-------------|
| `id` | string | `id` | Machine slug (e.g. `korg_ms2000`) |
| `displayName` | string | `displayName` | Human name |
| `description` | string | `description` | Free text |
| `deviceType` | string | `deviceType` | `MANUAL_EURORACK` / `AUTOMATED_SYSEX` / … |
| `brand` | string | `brand` | Brand name |
| `brandLogo` | string | `brandLogo` | Logo path |
| `modelImage` | string | `modelImage` | Image path |
| `manufacturer` | string | `midiIdentification.manufacturer` | e.g. `Korg` |
| `model` | string | `midiIdentification.model` | e.g. `MS2000` |
| `modelIdHex` | string | `midiIdentification.modelIdHex` | e.g. `58` |
| `autoDetectSysEx` | string | `midiIdentification.autoDetectSysEx` | SysEx query hex |
| `midiIdentity` | `MidiIdentityContract` | `midiIdentification` | Complete identity claim |

### 3.2 MidiIdentityContract Fields

| Field | Type | Example | Description |
|-------|------|---------|-------------|
| `manufacturer` | string | `Korg` | Human manufacturer |
| `manufacturerIdHex` | string | `42` | MMA manufacturer ID (space-separated hex bytes) |
| `model` | string | `MS2000` | Human model |
| `modelIdHex` | string | `58` | Family byte 2 in Universal Identity Reply |
| `familyIdHex` | string | `00 00` | Family ID (optional) |
| `sysexHeaderHex` | string | `42 30 58` | Proprietary SysEx header for this model |
| `portNameMatches` | string[] | `["MS2000", "Korg"]` | Port-name heuristic keywords |

### 3.3 Raw JSON Access (Domain Data)

```cpp
// In SharedHardwareContractAdapter::rebuildLocalCache()
if (auto rawJson = sharedRegistry.getRawContractJson(sharedC.id))
{
    // rawJson contains the FULL parsed contract including:
    // - "functions": [ { "id", "name", "blockType", "controls": [...] } ]
    // - "routingGuide": { "stimulusOutput", "responseInput", "notes" }
    // - "controls" (legacy v1 schema "parameters")
    parseFunctionsFromRawJson(*rawJson, localC); // your domain parser
}
```

---

## 4. Host MIDI Backend Implementation (`MidiHardwareBackend`)

The picker **requires** a concrete backend. Implement the interface over JUCE `MidiOutput`/`MidiInput`:

### 4.1 Interface Contract (`abd::hwid::MidiHardwareBackend`)

```cpp
class MidiHardwareBackend
{
public:
    virtual ~MidiHardwareBackend() = default;

    /** Return output port name for a given hardwareId (for WebUI display). */
    virtual std::string getOutputPortName(const std::string& hardwareId) const = 0;

    /** Send raw SysEx bytes to the device. */
    virtual void sendBytes(const std::string& hardwareId,
                           const std::vector<uint8_t>& bytes) = 0;

    /** Set callback for incoming MIDI bytes (SysEx Identity Reply, etc.). */
    virtual void setReceiveCallback(ReceiveCallback cb) = 0;

    /** Start listening on the input port for hardwareId. */
    virtual void startListening(const std::string& hardwareId) = 0;

    /** Stop listening. */
    virtual void stopListening() = 0;

    /** Refresh port list (no-op for JUCE). */
    virtual void refreshPorts() = 0;

    /** Optional: return cached identity if known. */
    virtual std::optional<HardwareIdentity> getKnownIdentity(
        const std::string& hardwareId) const { return std::nullopt; }
};
```

### 4.2 Complete Reference Implementation (`AudioLabMidiBackend.h/.cpp`)

```cpp
// AudioLabMidiBackend.h
#pragma once
#include <HardwareMidiDetect/MidiHardwareBackend.h>
#include <juce_audio_devices/juce_audio_devices.h>

namespace abdaudiolab::hardware
{
class AudioLabMidiBackend : public abd::hwid::MidiHardwareBackend,
                            private juce::MidiInputCallback
{
public:
    AudioLabMidiBackend() = default;
    ~AudioLabMidiBackend() override { stopListening(); }

    // MidiHardwareBackend
    std::string getOutputPortName(const std::string& hardwareId) const override;
    void sendBytes(const std::string& hardwareId, const std::vector<uint8_t>& bytes) override;
    void setReceiveCallback(ReceiveCallback cb) override { receiveCb_ = std::move(cb); }
    void startListening(const std::string& hardwareId) override;
    void stopListening() override;
    void refreshPorts() override;
    std::optional<abd::hwid::HardwareIdentity> getKnownIdentity(const std::string& hardwareId) const override;

private:
    // juce::MidiInputCallback
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;

    juce::MidiOutput* openOutputFor(const std::string& hardwareId);
    juce::MidiInput*  openInputFor(const std::string& hardwareId);

    ReceiveCallback receiveCb_;
    std::unique_ptr<juce::MidiInput> activeInput_;
    std::string activeHardwareId_;
};
}
```

```cpp
// AudioLabMidiBackend.cpp
#include "AudioLabMidiBackend.h"
#include <juce_core/juce_core.h>

namespace abdaudiolab::hardware
{

std::string AudioLabMidiBackend::getOutputPortName(const std::string& hardwareId) const
{
    auto outputs = juce::MidiOutput::getAvailableDevices();
    for (const auto& d : outputs)
        if (d.name.containsIgnoreCase(hardwareId) || d.identifier.containsIgnoreCase(hardwareId))
            return d.name.toStdString();
    return outputs.isEmpty() ? "" : outputs[0].name.toStdString();
}

void AudioLabMidiBackend::sendBytes(const std::string& hardwareId, const std::vector<uint8_t>& bytes)
{
    if (auto* out = openOutputFor(hardwareId))
    {
        auto msg = juce::MidiMessage::createSysExMessage(bytes.data(), (int)bytes.size());
        out->sendMessageNow(msg);
    }
}

void AudioLabMidiBackend::startListening(const std::string& hardwareId)
{
    stopListening();
    if (auto* in = openInputFor(hardwareId))
    {
        in->start();
        activeInput_.reset(in);
        activeHardwareId_ = hardwareId;
    }
}

void AudioLabMidiBackend::stopListening()
{
    if (activeInput_)
    {
        activeInput_->stop();
        activeInput_.reset();
    }
    activeHardwareId_.clear();
}

void AudioLabMidiBackend::refreshPorts() { /* JUCE auto-refreshes */ }

std::optional<abd::hwid::HardwareIdentity> AudioLabMidiBackend::getKnownIdentity(const std::string&) const
{
    return std::nullopt; // implement cache if needed
}

void AudioLabMidiBackend::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message)
{
    if (message.isSysEx() && receiveCb_)
    {
        const uint8_t* data = message.getSysExData();
        size_t size = message.getSysExDataSize();
        std::vector<uint8_t> bytes(data, data + size);
        receiveCb_(activeHardwareId_, std::move(bytes));
    }
}

juce::MidiOutput* AudioLabMidiBackend::openOutputFor(const std::string& hardwareId)
{
    auto outputs = juce::MidiOutput::getAvailableDevices();
    for (const auto& d : outputs)
        if (d.name.containsIgnoreCase(hardwareId) || d.identifier.containsIgnoreCase(hardwareId))
            return juce::MidiOutput::openDevice(d.identifier);
    return outputs.isEmpty() ? nullptr : juce::MidiOutput::openDevice(outputs[0].identifier);
}

juce::MidiInput* AudioLabMidiBackend::openInputFor(const std::string& hardwareId)
{
    auto inputs = juce::MidiInput::getAvailableDevices();
    for (const auto& d : inputs)
        if (d.name.containsIgnoreCase(hardwareId) || d.identifier.containsIgnoreCase(hardwareId))
            return juce::MidiInput::openDevice(d.identifier, this);
    return inputs.isEmpty() ? nullptr : juce::MidiInput::openDevice(inputs[0].identifier, this);
}

} // namespace abdaudiolab::hardware
```

---

## 5. Shared → Local Contract Adapter

The shared module **does not model** `functions`/`controls`/`routingGuide`. ABDAudioLab must read them from raw JSON.

### 5.1 Adapter Header (`SharedHardwareContractAdapter.h`)

```cpp
#pragma once
#include <HardwareMidiDetect/HardwareContractRegistry.h>
#include <HardwareMidiDetect/HardwareContract.h>
#include "HardwareContractRegistry.h" // your local domain types

namespace abdaudiolab::core
{
class SharedHardwareContractAdapter
{
public:
    explicit SharedHardwareContractAdapter(abd::hwid::HardwareContractRegistry& sharedRegistry)
        : sharedRegistry_(sharedRegistry) {}

    bool loadFromShared(const juce::File& contractsDir)
    {
        if (!sharedRegistry_.loadContractsFromDirectory(contractsDir))
        {
            lastError_ = sharedRegistry_.getLastError();
            return false;
        }
        rebuildLocalCache();
        return true;
    }

    [[nodiscard]] bool hasContracts() const noexcept { return !localCache_.empty(); }
    [[nodiscard]] const std::vector<HardwareContract>& getContracts() const noexcept { return localCache_; }
    [[nodiscard]] const HardwareContract* findContractById(const std::string& id) const noexcept;
    [[nodiscard]] const std::string& getLastError() const noexcept { return lastError_; }

private:
    void rebuildLocalCache();
    void parseFunctionsFromRawJson(const nlohmann::json&, HardwareContract&); // see §5.3

    abd::hwid::HardwareContractRegistry& sharedRegistry_;
    std::vector<HardwareContract> localCache_;
    std::string lastError_;
};
}
```

### 5.2 Adapter Implementation — Cache Rebuild

```cpp
// SharedHardwareContractAdapter.cpp
#include "SharedHardwareContractAdapter.h"

namespace abdaudiolab::core
{
void SharedHardwareContractAdapter::rebuildLocalCache()
{
    localCache_.clear();
    for (const auto& sharedC : sharedRegistry_.getContracts())
    {
        HardwareContract localC;
        // 1:1 base field copy
        localC.schemaVersion = sharedC.schemaVersion;
        localC.id = sharedC.id;
        localC.displayName = sharedC.displayName;
        localC.description = sharedC.description;
        localC.deviceType = sharedC.deviceType;
        localC.brand = sharedC.brand;
        localC.brandLogo = sharedC.brandLogo;
        localC.modelImage = sharedC.modelImage;
        localC.manufacturer = sharedC.manufacturer;
        localC.model = sharedC.model;
        localC.modelIdHex = sharedC.modelIdHex;
        localC.autoDetectSysEx = sharedC.autoDetectSysEx;
        localC.midiIdentity.manufacturer = sharedC.midiIdentity.manufacturer;
        localC.midiIdentity.manufacturerIdHex = sharedC.midiIdentity.manufacturerIdHex;
        localC.midiIdentity.model = sharedC.midiIdentity.model;
        localC.midiIdentity.modelIdHex = sharedC.midiIdentity.modelIdHex;
        localC.midiIdentity.familyIdHex = sharedC.midiIdentity.familyIdHex;
        localC.midiIdentity.sysexHeaderHex = sharedC.midiIdentity.sysexHeaderHex;
        localC.midiIdentity.portNameMatches = sharedC.midiIdentity.portNameMatches;

        // Domain-specific: read from raw JSON
        if (auto rawJson = sharedRegistry_.getRawContractJson(sharedC.id))
            parseFunctionsFromRawJson(*rawJson, localC);

        localCache_.push_back(std::move(localC));
    }
}

const HardwareContract* SharedHardwareContractAdapter::findContractById(const std::string& id) const noexcept
{
    for (const auto& c : localCache_)
        if (c.id == id) return &c;
    return nullptr;
}
} // namespace abdaudiolab::core
```

### 5.3 Domain Parser (Copy from your `HardwareContractRegistry.cpp` lines 71-146)

```cpp
void SharedHardwareContractAdapter::parseFunctionsFromRawJson(const nlohmann::json& j, HardwareContract& out)
{
    if (j.contains("functions") && j["functions"].is_array())
    {
        for (const auto& fJson : j["functions"])
        {
            HardwareFunction f;
            f.id = fJson.value("id", std::string("main"));
            f.name = fJson.value("name", std::string("Main Function"));
            f.blockType = fJson.value("blockType", std::string("SpectrumFilter"));
            f.suggestedStimulus = fJson.value("suggestedStimulus", std::string("LOG_SINE_SWEEP"));
            f.captureMode = fJson.value("captureMode", std::string("FIXED_TIME"));
            f.defaultBurstDurationSec = fJson.value("defaultBurstDurationSec", 1.0f);
            f.maxTimeoutSec = fJson.value("maxTimeoutSec", 60.0f);
            f.silenceThresholdDb = fJson.value("silenceThresholdDb", -60.0f);

            if (fJson.contains("routingGuide") && fJson["routingGuide"].is_object())
            {
                const auto& rg = fJson["routingGuide"];
                f.routingGuide.stimulusOutput = rg.value("stimulusOutput", std::string(""));
                f.routingGuide.responseInput = rg.value("responseInput", std::string(""));
                f.routingGuide.notes = rg.value("notes", std::string(""));
            }

            if (fJson.contains("controls") && fJson["controls"].is_array())
            {
                for (const auto& cJson : fJson["controls"])
                {
                    HardwareControl ctrl;
                    ctrl.index = cJson.value("index", static_cast<int>(f.controls.size() + 1));
                    ctrl.name = cJson.value("name", std::string("Control"));
                    ctrl.type = cJson.value("type", std::string("Knob"));
                    ctrl.controlMethod = cJson.value("controlMethod", std::string("MANUAL"));
                    ctrl.ccNumber = cJson.value("cc", cJson.value("midiCC", -1));
                    ctrl.nrpnNumber = cJson.value("nrpn", -1);
                    ctrl.sysexAddress = cJson.value("sysexAddress", std::string(""));
                    ctrl.minVal = cJson.value("min", 0.0f);
                    ctrl.maxVal = cJson.value("max", 1.0f);
                    ctrl.defaultVal = cJson.value("default", 0.5f);
                    ctrl.unit = cJson.value("unit", std::string(""));
                    if (cJson.contains("options") && cJson["options"].is_array())
                        for (const auto& opt : cJson["options"]) ctrl.options.push_back(opt.get<std::string>());
                    f.controls.push_back(std::move(ctrl));
                }
            }
            out.functions.push_back(std::move(f));
        }
    }
    else if (j.contains("parameters") && j["parameters"].is_array())
    {
        // Legacy v1 schema → single default function
        HardwareFunction f;
        f.id = "main_function";
        f.name = out.displayName;
        f.blockType = "SpectrumFilter";
        f.suggestedStimulus = "LOG_SINE_SWEEP";
        f.routingGuide.stimulusOutput = "Audio Out 1 (L) -> Hardware Input";
        f.routingGuide.responseInput = "Hardware Output -> Audio In 1 (L)";
        f.routingGuide.notes = "Standard audio loopback routing.";
        for (const auto& pJson : j["parameters"])
        {
            HardwareControl ctrl;
            ctrl.index = pJson.value("index", static_cast<int>(f.controls.size() + 1));
            ctrl.name = pJson.value("name", std::string("Param"));
            ctrl.type = pJson.value("type", std::string("Knob"));
            ctrl.defaultVal = pJson.value("default", 0.5f);
            f.controls.push_back(std::move(ctrl));
        }
        out.functions.push_back(std::move(f));
    }
}
```

---

## 6. Picker Integration in UI

In your drawer/component where the "Detect Hardware" button lives:

```cpp
#include <HardwareMidiDetect/JuceHardwareMidiPicker.h>
#include "AudioLabMidiBackend.h"
#include "SharedHardwareContractAdapter.h"

class MySlideInDrawer : public juce::Component
{
public:
    MySlideInDrawer(SharedHardwareContractAdapter& adapter)
        : contractAdapter_(adapter) {}

    void showHardwarePicker()
    {
        if (!midiBackend_)
            midiBackend_ = std::make_unique<abdaudiolab::hardware::AudioLabMidiBackend>();

        hwPicker_ = std::make_unique<abd::hwid::JuceHardwareMidiPicker>(
            *midiBackend_,
            [this](const abd::hwid::JuceHardwareMidiPicker::HardwarePickResult& result)
            {
                juce::MessageManager::callAsync([this, result] { onHardwarePicked(result); });
            });

        addAndMakeVisible(*hwPicker_);
        hwPicker_->setBounds(getLocalBounds());
        hwPicker_->startPick(); // launches detection UI
    }

private:
    void onHardwarePicked(const abd::hwid::JuceHardwareMidiPicker::HardwarePickResult& result)
    {
        if (result.cancelled)
        {
            hwPicker_.reset();
            return;
        }

        // result.hardwareId == "korg_ms2000", "roland_juno106", etc.
        if (auto* contract = contractAdapter_.findContractById(result.hardwareId))
        {
            applyContractToSession(*contract); // your existing logic
        }
        else
        {
            // Unknown device — fallback UI
        }
        hwPicker_.reset();
    }

    void applyContractToSession(const HardwareContract& c)
    {
        // Populate your session with c.functions, c.controls, c.midiIdentity, etc.
    }

    void resized() override
    {
        if (hwPicker_) hwPicker_->setBounds(getLocalBounds());
        // ... other children
    }

    std::unique_ptr<abdaudiolab::hardware::AudioLabMidiBackend> midiBackend_;
    std::unique_ptr<abd::hwid::JuceHardwareMidiPicker> hwPicker_;
    SharedHardwareContractAdapter& contractAdapter_;
};
```

---

## 7. Contract Loading at Startup

In `main.cpp` or your app initializer:

```cpp
#include <HardwareMidiDetect/HardwareContractRegistry.h>
#include "SharedHardwareContractAdapter.h"

// Global or App-owned instances
abd::hwid::HardwareContractRegistry sharedRegistry;
abdaudiolab::core::SharedHardwareContractAdapter contractAdapter(sharedRegistry);

void loadSharedContracts()
{
    // Reuse your existing searchRoots logic (from HardwareManager.cpp)
    auto contractsDir = findContractsDirectory(); // → ABDSharedAssets/contracts/hardware

    if (!contractAdapter.loadFromShared(contractsDir))
    {
        juce::Logger::writeToLog("[ABDAudioLab] Failed to load shared contracts: "
            + contractAdapter.getLastError());
    }
    else
    {
        juce::Logger::writeToLog("[ABDAudioLab] Loaded "
            + juce::String(contractAdapter.getContracts().size()) + " hardware contracts.");
    }
}
```

---

## 8. WebUI ↔ C++ Event Contract (`nativeEvent`)

The picker's embedded WebUI communicates with the C++ backend **exclusively** via `window.__JUCE__.backend.emitEvent('nativeEvent', ...)`. No other channel is used.

### 8.1 WebUI → Backend Events

| Event | Payload | Description |
|-------|---------|-------------|
| `hardware.send` | `{ payload: "<base64>" }` | Send raw SysEx bytes (base64 encoded) |
| `hardware.listen` | `{}` | Arm the input listener on current port |
| `hardware.stop` | `{}` | Disarm listener |
| `hardware.result` | `{ cancelled, hardwareId, displayName, manufacturer, model, firmwareVersion }` | Final result delivered to C++ callback |

### 8.2 Backend → WebUI Push

C++ pushes incoming MIDI bytes to WebUI via JS evaluation:

```cpp
// In JuceHardwareMidiPicker (internal)
webBrowser->evaluateJavascript(
    "if (window.__pushMidiBytes) __pushMidiBytes('" + base64Bytes + "')");
```

WebUI handler (in `index.html`):

```javascript
window.__pushMidiBytes = function(b64) {
    const bytes = base64ToBytes(b64);
    // Parse Identity Reply → match against contracts → emit hardware.result
};
```

### 8.3 Universal Identity Query (always sent)

```
F0 7E 7F 06 01 F7
```
(Non-Real-Time Universal Device Inquiry — works on all MIDI 1.0+ devices)

---

## 9. Verification Checklist

| Step | Validation | OK? |
|------|------------|-----|
| **CMake** | `target_link_libraries(... ABDShared::HardwareMidiDetect)` | ☐ |
| **Adapter** | `SharedHardwareContractAdapter` compiles, loads contracts, populates `functions`/`controls` | ☐ |
| **Backend** | `AudioLabMidiBackend` compiles, `sendBytes`/`startListening` work with real MIDI ports | ☐ |
| **Picker UI** | Button click → WebView2 loads `index.html` (dark theme, "Detecting..." spinner) | ☐ |
| **Detection** | Sends Universal Inquiry → receives Identity Reply → matches contract → callback fires | ☐ |
| **Result** | `onHardwarePicked` receives `result.hardwareId` matching a loaded contract | ☐ |
| **Domain Data** | `contract->functions` and `controls` available for session setup | ☐ |

---

## 10. Debugging Quick-Ref

| Symptom | Check |
|---------|-------|
| WebUI blank / 404 | `HardwareMidiPickerAssets.h` generated? (`build/ABDSharedCode/juce_binarydata_HardwareMidiPickerAssets/`) |
| `nativeEvent` not received | `JuceHardwareMidiPicker` constructor registers `withEventListener("nativeEvent", ...)`; WebView2 enabled in `juce_add_plugin/gui_app` (`NEEDS_WEBVIEW2 TRUE`) |
| SysEx not transmitted | `AudioLabMidiBackend::sendBytes` uses `createSysExMessage(data, size)` + `sendMessageNow`; output port opened with correct identifier |
| SysEx not received | `handleIncomingMidiMessage` checks `message.isSysEx()` + calls `receiveCb_(id, bytes)`; input port started with `this` as callback |
| Contracts empty | `contractsDir` path correct? Log shows `Contracts directory does not exist` or `No JSON contracts found` |
| Domain fields missing | `getRawContractJson(id)` called in adapter; `parseFunctionsFromRawJson` copies `functions`/`controls` |

---

## 11. Cross-References

- `ABDSharedCode/INTEGRATION_GUIDE.md` — § "Módulo: HardwareMidiDetect" (layer comparison, CMake, nativeEvent table, migration notes)
- `ABDSharedCode/HardwareMidiDetect/USAGE.md` — module usage guide (both layers)
- `ABDScope/docs/INTEGRATION_GUIDE.md` — ABDScope WebView2 integration pattern (binary assets, ResourceProvider, IPC)
- `ABDScope/Source/JUCE/ScopeResourceProvider.cpp` — embedded resource provider reference implementation
- `ABDSharedAssets/contracts/hardware/*.json` — single-source contract files (authority for `midiIdentification`, `autoDetectSysEx`)