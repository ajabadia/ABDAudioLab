#pragma once

#include <juce_core/juce_core.h>
#include <cstdint>

namespace abdaudiolab::hardware
{

/**
 * @brief Pure abstract contract for musical hardware control.
 * 
 * Guarantees aseptic separation between the measurement sequencer
 * and the physical device (Digital MIDI CC, Roland SysEx, Eurorack Manual Operator, or Mock).
 */
class IHardwareController
{
public:
    virtual ~IHardwareController() = default;

    /**
     * @brief Returns true if this controller is automatic (MIDI/SysEx), 
     * or false if it requires human operator intervention.
     */
    [[nodiscard]] virtual bool isAutomatic() const noexcept = 0;

    /**
     * @brief Returns a friendly name of the hardware under test.
     */
    [[nodiscard]] virtual juce::String getHardwareName() const = 0;

    /**
     * @brief Connects to the hardware port / interface.
     */
    virtual bool connect() = 0;

    /**
     * @brief Disconnects from the hardware.
     */
    virtual void disconnect() = 0;

    /**
     * @brief Checks if connection is active.
     */
    [[nodiscard]] virtual bool isConnected() const noexcept = 0;

    /**
     * @brief Sets a parameter value using normalized range [0.0, 1.0].
     */
    virtual bool setParameter(int paramIndex, float normalizedValue) = 0;

    /**
     * @brief Sets a parameter value using raw integer range [0, 127].
     */
    virtual bool setParameterRaw(int paramIndex, int rawValue) = 0;

    /**
     * @brief Configures a submodule in a specific slot (for modular/digital synths).
     */
    virtual bool setupSubmodule(int slotIndex, uint8_t typeId) = 0;

    /**
     * @brief Connects or disconnects a virtual patch cable between source and destination.
     */
    virtual bool setPatchCable(uint8_t sourceId, uint8_t destId, bool isConnected) = 0;

    /**
     * @brief Requests a full state dump from hardware (if supported via SysEx RQ1).
     */
    virtual void requestStateDump() = 0;
};

} // namespace abdaudiolab::hardware
