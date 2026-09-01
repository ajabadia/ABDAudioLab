#pragma once

#include "HardwareController.h"
#include <functional>
#include <atomic>

namespace abdaudiolab::hardware
{

/**
 * @brief Controller for purely manual analog hardware (Eurorack, custom circuits).
 * 
 * Pauses the sequencer and triggers interactive operator guidance.
 */
class ManualAnalogueController : public IHardwareController
{
public:
    using PromptCallback = std::function<void(const juce::String& paramName, float targetValNormalized, int targetValRaw)>;

    explicit ManualAnalogueController(juce::String customName = "Analog Modular Rack")
        : hardwareName(std::move(customName))
    {
    }

    void setPromptCallback(PromptCallback cb)
    {
        promptCallback = std::move(cb);
    }

    [[nodiscard]] bool isAutomatic() const noexcept override { return false; }

    [[nodiscard]] juce::String getHardwareName() const override
    {
        return hardwareName;
    }

    bool connect() override
    {
        connected = true;
        return true;
    }

    void disconnect() override
    {
        connected = false;
    }

    [[nodiscard]] bool isConnected() const noexcept override
    {
        return connected;
    }

    bool setParameter(int paramIndex, float normalizedValue) override
    {
        int raw = std::clamp(static_cast<int>(std::lround(normalizedValue * 127.0f)), 0, 127);
        return setParameterRaw(paramIndex, raw);
    }

    bool setParameterRaw(int paramIndex, int rawValue) override
    {
        if (promptCallback)
        {
            float norm = static_cast<float>(rawValue) / 127.0f;
            juce::String pName = "Parameter #" + juce::String(paramIndex);
            promptCallback(pName, norm, rawValue);
        }
        return true;
    }

    bool setupSubmodule(int /*slotIndex*/, uint8_t /*typeId*/) override
    {
        return true;
    }

    bool setPatchCable(uint8_t /*sourceId*/, uint8_t /*destId*/, bool /*isConnected*/) override
    {
        return true;
    }

    void requestStateDump() override
    {
    }

private:
    juce::String hardwareName;
    std::atomic<bool> connected { false };
    PromptCallback promptCallback;
};

} // namespace abdaudiolab::hardware
