#pragma once

#include <cstdint>
#include <chrono>

namespace abdaudiolab::hardware
{

/**
 * @brief Pure interface for querying monotonic wall-clock time in milliseconds.
 * Injected into HardwareDispatchScheduler to ensure 100% deterministic testing without sleep().
 */
class IMonotonicClock
{
public:
    virtual ~IMonotonicClock() = default;

    [[nodiscard]] virtual uint64_t nowMs() const noexcept = 0;
};

/**
 * @brief Real production wall-clock monotonic implementation using std::chrono::steady_clock.
 */
class RealMonotonicClock final : public IMonotonicClock
{
public:
    [[nodiscard]] uint64_t nowMs() const noexcept override
    {
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
    }
};

} // namespace abdaudiolab::hardware
