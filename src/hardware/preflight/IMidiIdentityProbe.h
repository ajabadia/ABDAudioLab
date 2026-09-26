#pragma once

#include "HardwareTransportPreflightTypes.h"
#include "profiling/TargetProfile.h"
#include "hardware/transport/MidiTransportTypes.h"

namespace abdaudiolab::hardware
{

/**
 * @brief Pure interface for hardware identity probing (input/bidirectional handshake).
 * Strictly decoupled from output transport (IMidiTransport).
 */
class IMidiIdentityProbe
{
public:
    virtual ~IMidiIdentityProbe() = default;

    /**
     * @brief Probes connected device on the given port to interrogate identity.
     */
    virtual HardwareIdentityProbeResult probe(
        const abdaudiolab::profiling::TargetProfile& profile,
        const MidiPortSelection& outputPort) = 0;
};

} // namespace abdaudiolab::hardware
