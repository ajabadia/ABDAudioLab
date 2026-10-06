#pragma once

#include "LabAudioEngine.h"
#include "../hardware/MockHardwareController.h"

namespace abdaudiolab::audio
{

/**
 * @class ScopedPhysicalLoopbackCapture
 * @brief RAII guard that temporarily isolates the audio engine from internal mock hardware
 *        during physical loopback calibration, guaranteeing restore upon completion, timeout,
 *        abort, or destruction.
 */
class ScopedPhysicalLoopbackCapture
{
public:
    explicit ScopedPhysicalLoopbackCapture(LabAudioEngine& engine)
        : engine_(engine),
          previousMock_(engine.getMockHardware())
    {
        engine_.setMockHardware(nullptr);
        engine_.setPhysicalLoopbackIsolation(true);
    }

    ~ScopedPhysicalLoopbackCapture()
    {
        restore();
    }

    void restore() noexcept
    {
        if (active_)
        {
            engine_.setPhysicalLoopbackIsolation(false);
            engine_.setMockHardware(previousMock_);
            active_ = false;
        }
    }

    [[nodiscard]] hardware::MockHardwareController* getPreviousMock() const noexcept
    {
        return previousMock_;
    }

    [[nodiscard]] bool isActive() const noexcept
    {
        return active_;
    }

    ScopedPhysicalLoopbackCapture(const ScopedPhysicalLoopbackCapture&) = delete;
    ScopedPhysicalLoopbackCapture& operator=(const ScopedPhysicalLoopbackCapture&) = delete;
    ScopedPhysicalLoopbackCapture(ScopedPhysicalLoopbackCapture&&) = delete;
    ScopedPhysicalLoopbackCapture& operator=(ScopedPhysicalLoopbackCapture&&) = delete;

private:
    LabAudioEngine& engine_;
    hardware::MockHardwareController* previousMock_ { nullptr };
    bool active_ { true };
};

} // namespace abdaudiolab::audio
