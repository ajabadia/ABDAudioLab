#pragma once

#include "hardware/transport/IMonotonicClock.h"

namespace abdaudiolab::tests
{

/**
 * @brief Deterministic mock clock for advancing and controlling monotonic time in unit tests.
 */
class MockMonotonicClock final : public hardware::IMonotonicClock
{
public:
    explicit MockMonotonicClock(uint64_t initialTimeMs = 0) noexcept
        : currentTimeMs_(initialTimeMs)
    {
    }

    [[nodiscard]] uint64_t nowMs() const noexcept override
    {
        return currentTimeMs_;
    }

    void advanceMs(uint64_t deltaMs) noexcept
    {
        currentTimeMs_ += deltaMs;
    }

    void setTimeMs(uint64_t newTimeMs) noexcept
    {
        currentTimeMs_ = newTimeMs;
    }

private:
    uint64_t currentTimeMs_ { 0 };
};

} // namespace abdaudiolab::tests
