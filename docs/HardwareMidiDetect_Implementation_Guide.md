# ABDAudioLab — Implementación del módulo HardwareMidiDetect (Shared)

Este documento describe **cómo migrar ABDAudioLab** de su registry local `abdaudiolab::core::HardwareContractRegistry` al módulo compartido `abd::hwid::HardwareContractRegistry` y cómo integrar el picker WebView2 (`JuceHardwareMidiPicker`) usando el patrón ABDScope.

---

## 1. Cambios en CMakeLists.txt

ABDAudioLab ya añade JUCE y nlohmann_json antes de `add_subdirectory(ABDSharedCode)`. **No hay cambios de dependencias** (el módulo usa `juce_audio_devices`, `juce_gui_extra`, `juce_core` y `nlohmann_json::nlohmann_json`, todos ya en scope).

Añade el link del nuevo target:

```cmake
# ... tras add_subdirectory("${ABDSHARED_CODE_DIR}" ...)
target_link_libraries(ABDAudioLab PRIVATE ABDShared::HardwareMidiDetect)
```

> El WebUI embebido se genera vía `juce_add_binary_data(HardwareMidiPickerAssets ...)` dentro del CMake de ABDSharedCode (activado porque JUCE está en scope). El target alias es `ABDShared::HardwareMidiPickerAssets` — **no hace falta linkearlo explícitamente**, `HardwareMidiDetect` ya lo hace internamente.

---

## 2. Migración del Registry local → Shared

### 2.1 Diferencias clave

| Campo | Local (`abdaudiolab::core::HardwareContract`) | Shared (`abd::hwid::HardwareContract`) |
|-------|-----------------------------------------------|----------------------------------------|
| `functions` (vector<HardwareFunction>) | **Sí** — completo | **No** — no modelado |
| `controls` (vector<HardwareControl>) | **Sí** — completo | **No** — no modelado |
| `routingGuide` | **Sí** | **No** |
| `midiIdentity` | Parcial (mismo esquema) | Completo |
| `getRawContractJson(id)` | **No existe** | **Sí** — expone el JSON parseado completo |

**Regla**: el módulo shared **no conoce** `functions`/`controls`/`routingGuide`. ABDAudioLab los lee del JSON crudo via `getRawContractJson()`.

### 2.2 Adaptador recomendado (header-only)

Crea `src/core/SharedHardwareContractAdapter.h`:

```cpp
#pragma once

#include <HardwareMidiDetect/HardwareContractRegistry.h>
#include <HardwareMidiDetect/HardwareContract.h>
#include "HardwareContractRegistry.h" // tu registry local (para tipos de dominio)

namespace abdaudiolab::core
{

/**
 * @brief Carga contratos del módulo shared y expone una vista compatible
 *        con el registry local de ABDAudioLab (incluyendo functions/controls).
 */
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
    void rebuildLocalCache()
    {
        localCache_.clear();
        for (const auto& sharedC : sharedRegistry_.getContracts())
        {
            HardwareContract localC;
            // Copia campos base 1:1
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

            // --- Domain-specific: leer de JSON crudo ---
            if (auto rawJson = sharedRegistry_.getRawContractJson(sharedC.id))
            {
                parseFunctionsFromRawJson(*rawJson, localC);
            }

            localCache_.push_back(std::move(localC));
        }
    }

    void parseFunctionsFromRawJson(const nlohmann::json& j, HardwareContract& out)
    {
        // Reutiliza tu lógica existente (líneas 71-146 de HardwareContractRegistry.cpp)
        // Copia aquí el bloque que parsea "functions" y "controls" del JSON.
        // ... (ver apéndice A)
    }

    abd::hwid::HardwareContractRegistry& sharedRegistry_;
    std::vector<HardwareContract> localCache_;
    std::string lastError_;
};

} // namespace abdaudiolab::core
```

> **Por qué un adapter**: mantiene tu código de dominio (`HardwareFunction`, `HardwareControl`, `HardwareRoutingGuide`) sin cambios y evita acoplar el módulo shared a tu esquema de producto.

---

## 3. Backend MIDI concreto para el Picker (`MidiHardwareBackend`)

El picker WebView2 requiere que el host implemente `abd::hwid::MidiHardwareBackend`. Crea `src/hardware/AudioLabMidiBackend.h`:

```cpp
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

    // --- MidiHardwareBackend ---
    std::string getOutputPortName(const std::string& hardwareId) const override;
    void sendBytes(const std::string& hardwareId, const std::vector<uint8_t>& bytes) override;
    void setReceiveCallback(ReceiveCallback cb) override { receiveCb_ = std::move(cb); }
    void startListening(const std::string& hardwareId) override;
    void stopListening() override;
    void refreshPorts() override;
    std::optional<abd::hwid::HardwareIdentity> getKnownIdentity(const std::string& hardwareId) const override;

private:
    // MidiInputCallback
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;

    juce::MidiOutput* openOutputFor(const std::string& hardwareId);
    juce::MidiInput* openInputFor(const std::string& hardwareId);

    ReceiveCallback receiveCb_;
    std::unique_ptr<juce::MidiInput> activeInput_;
    std::string activeHardwareId_;
};
```

Implementación (`AudioLabMidiBackend.cpp`):

```cpp
#include "AudioLabMidiBackend.h"
#include <juce_core/juce_core.h>

namespace abdaudiolab::hardware
{

std::string AudioLabMidiBackend::getOutputPortName(const std::string& hardwareId) const
{
    // Busca en device list y devuelve el nombre de salida que coincide con hardwareId
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

void AudioLabMidiBackend::refreshPorts()
{
    // Nada especial; JUCE refresca automáticamente en getAvailableDevices()
}

std::optional<abd::hwid::HardwareIdentity> AudioLabMidiBackend::getKnownIdentity(const std::string& hardwareId) const
{
    // Opcional: si ya tienes cache de identidad de sesiones previas, devuélvela.
    return std::nullopt;
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

## 4. Integración del Picker en la UI (SlideInDrawer / botón)

En tu componente que abre el drawer (p. ej. `SlideInDrawer.cpp` o donde esté el botón "Detect Hardware"):

```cpp
#include <HardwareMidiDetect/JuceHardwareMidiPicker.h>
#include "AudioLabMidiBackend.h"
#include "SharedHardwareContractAdapter.h"

// ...

class MySlideInDrawer : public juce::Component
{
    // ...
    std::unique_ptr<abd::hwid::JuceHardwareMidiPicker> hwPicker_;
    std::unique_ptr<abdaudiolab::hardware::AudioLabMidiBackend> midiBackend_;
    SharedHardwareContractAdapter contractAdapter_; // tu adapter

    void showHardwarePicker()
    {
        if (!midiBackend_)
            midiBackend_ = std::make_unique<abdaudiolab::hardware::AudioLabMidiBackend>();

        hwPicker_ = std::make_unique<abd::hwid::JuceHardwareMidiPicker>(
            *midiBackend_,
            [this](const abd::hwid::JuceHardwareMidiPicker::HardwarePickResult& result)
            {
                juce::MessageManager::callAsync([this, result] {
                    onHardwarePicked(result);
                });
            });

        addAndMakeVisible(*hwPicker_);
        hwPicker_->setBounds(getLocalBounds());
        hwPicker_->startPick(); // dispara la UI de detección
    }

    void onHardwarePicked(const abd::hwid::JuceHardwareMidiPicker::HardwarePickResult& result)
    {
        if (result.cancelled)
        {
            // Usuario cerró sin seleccionar
            hwPicker_.reset();
            return;
        }

        // result.hardwareId == "korg_ms2000", etc.
        auto* contract = contractAdapter_.findContractById(result.hardwareId);
        if (contract)
        {
            // Tienes el contrato completo con functions/controls listos para tu dominio
            applyContractToSession(*contract);
        }
        else
        {
            // Fallback: dispositivo desconocido
        }

        hwPicker_.reset(); // limpia el picker
    }

    void applyContractToSession(const HardwareContract& c)
    {
        // Tu lógica existente: poblar functions, controls, routing, etc.
    }

    void resized() override
    {
        if (hwPicker_)
            hwPicker_->setBounds(getLocalBounds());
        // ...
    }
};
```

---

## 5. Cargar contratos al arranque (main.cpp o AppInitializer)

```cpp
#include <HardwareMidiDetect/HardwareContractRegistry.h>
#include "SharedHardwareContractAdapter.h"

// ...

abd::hwid::HardwareContractRegistry sharedRegistry;
abdaudiolab::core::SharedHardwareContractAdapter contractAdapter(sharedRegistry);

auto contractsDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                        .getParentDirectory()
                        .getParentDirectory()
                        .getChildFile("ABDSharedAssets/contracts/hardware");

if (!contractAdapter.loadFromShared(contractsDir))
{
    juce::Logger::writeToLog("[ABDAudioLab] Failed to load shared contracts: " + contractAdapter.getLastError());
}
// Ahora contractAdapter.getContracts() tiene TUS tipos locales con functions/controls poblados.
```

> **Ruta de contratos**: en ABDAudioLab ya usas `searchRoots` en `HardwareManager`. Reutiliza esa lógica para encontrar `ABDSharedAssets/contracts/hardware` (igual que en `INTEGRATION_GUIDE.md`).

---

## 6. Apéndice A — `parseFunctionsFromRawJson` (copia de tu parser local)

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
                    {
                        for (const auto& opt : cJson["options"])
                            ctrl.options.push_back(opt.get<std::string>());
                    }
                    f.controls.push_back(std::move(ctrl));
                }
            }
            out.functions.push_back(std::move(f));
        }
    }
    else if (j.contains("parameters") && j["parameters"].is_array())
    {
        // Backward compat v1 -> función única (mismo bloque que en tu .cpp líneas 125-146)
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

## 7. Checklist de verificación

| Paso | Comando / Acción | OK? |
|------|-------------------|-----|
| 1. CMake link añadido | `target_link_libraries(ABDAudioLab PRIVATE ABDShared::HardwareMidiDetect)` | ☐ |
| 2. Adapter compila | `SharedHardwareContractAdapter.h/.cpp` | ☐ |
| 3. Backend MIDI compila | `AudioLabMidiBackend.h/.cpp` | ☐ |
| 4. Picker se instancia y muestra UI | Botón "Detect Hardware" → WebView2 carga `index.html` | ☐ |
| 5. Detección real funciona | Envía `F0 7E 7F 06 01 F7` → recibe Identity Reply → match por contrato | ☐ |
| 6. Callback `hardware.result` trae `hardwareId` correcto | `onHardwarePicked` recibe `result.hardwareId` | ☐ |
| 7. Contrato local poblado con `functions/controls` | `applyContractToSession` usa `contract->functions` | ☐ |

---

## 8. Notas de depuración

- **WebUI no carga**: verifica que `HardwareMidiPickerAssets.h` se generó (en `build/ABDSharedCode/juce_binarydata_HardwareMidiPickerAssets/`). Si no, asegúrate de que `juce_add_binary_data` esté en scope **antes** de `add_subdirectory(ABDSharedCode)`.
- **`nativeEvent` no llega al backend**: el picker registra el listener en `JuceHardwareMidiPicker.h` constructor. Asegúrate de que `WebBrowserComponent` tiene `withBackend(juce::WebBrowserComponent::Options::Backend::webview2)`.
- **SysEx no sale/entra**: revisa `AudioLabMidiBackend::sendBytes` / `handleIncomingMidiMessage` — usa `createSysExMessage` y `receiveCb_` exactamente como en el ejemplo.
- **Contratos no cargan**: `contractsDir` debe apuntar a la carpeta que contiene `*.json` (p. ej. `korg_ms2000.json`, `roland_juno106.json`). Log del adapter te dirá la ruta exacta.

---

## 9. Referencias cruzadas

- `ABDSharedCode/INTEGRATION_GUIDE.md` — sección completa "Módulo: HardwareMidiDetect" (dos capas, contrato `nativeEvent`, migración ABDAudioLab).
- `ABDSharedCode/HardwareMidiDetect/USAGE.md` — guía de uso general del módulo.
- `ABDScope/Source/JUCE/ScopeResourceProvider.cpp` — patrón ResourceProvider embebido (referencia de implementación).