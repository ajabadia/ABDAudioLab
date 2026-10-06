/**
 * @file ActiveCalibrationContext.h
 * @brief Runtime application and projection of an active calibration snapshot.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <optional>
#include <string>
#include <cmath>
#include "CalibrationSnapshot.h"
#include "CalibrationCompatibilityEvaluator.h"

namespace abdaudiolab::calibration
{

/**
 * @struct InputGainPlan
 * @brief Structured composite gain plan decoupling interface calibration from session instrument trim.
 */
struct InputGainPlan
{
    float interfaceCalibrationTrimDb { 0.0f };
    float sessionTargetTrimDb { 0.0f };
    float effectiveTrimDb { 0.0f };

    [[nodiscard]] float getEffectiveLinearGain() const noexcept
    {
        return std::pow(10.0f, effectiveTrimDb / 20.0f);
    }

    void recalculate() noexcept
    {
        effectiveTrimDb = interfaceCalibrationTrimDb + sessionTargetTrimDb;
    }

    [[nodiscard]] bool operator==(const InputGainPlan& other) const noexcept
    {
        return std::abs(interfaceCalibrationTrimDb - other.interfaceCalibrationTrimDb) < 1e-4f &&
               std::abs(sessionTargetTrimDb - other.sessionTargetTrimDb) < 1e-4f &&
               std::abs(effectiveTrimDb - other.effectiveTrimDb) < 1e-4f;
    }
};

/**
 * @class ActiveCalibrationContext
 * @brief Runtime context managing the active calibration for audio receiver and sequencer.
 */
class ActiveCalibrationContext
{
public:
    ActiveCalibrationContext() = default;

    /**
     * @brief Attempts to activate a CalibrationSnapshot against the current audio stream configuration.
     * @return true if compatible and activated, false otherwise.
     */
    bool activate(const CalibrationSnapshot& snapshot,
                  const CurrentAudioConfigurationSnapshot& currentConfig,
                  bool strictBufferSize = true);

    /**
     * @brief Deactivates and clears the active calibration context.
     * Resets latency compensation to 0 and neutralizes interface trim.
     */
    void deactivate() noexcept;

    /**
     * @brief Re-evaluates compatibility when audio device or stream settings change.
     * If the current configuration is no longer compatible, deactivates.
     */
    void checkAlignment(const CurrentAudioConfigurationSnapshot& currentConfig);

    [[nodiscard]] bool isActive() const noexcept { return isActive_; }
    [[nodiscard]] const std::optional<CalibrationSnapshot>& getSnapshot() const noexcept { return activeSnapshot_; }

    /**
     * @brief Returns latency compensation in samples to inject into LabAudioReceiver.
     * Returns 0 if inactive, incompatible, or invalid.
     */
    [[nodiscard]] int getLatencyCompensationSamples() const noexcept;

    /**
     * @brief Updates the session-specific target trim measured from the instrument/synthesizer.
     * Composes effectiveTrimDb without modifying or overwriting interfaceCalibrationTrimDb.
     */
    void setSessionTargetTrimDb(float sessionTrimDb) noexcept;

    /**
     * @brief Returns the full structured gain plan.
     */
    [[nodiscard]] InputGainPlan getGainPlan() const noexcept { return gainPlan_; }

    /**
     * @brief Returns effective gain in dB.
     */
    [[nodiscard]] float getEffectiveTrimDb() const noexcept { return gainPlan_.effectiveTrimDb; }

    /**
     * @brief Returns effective linear gain multiplier.
     */
    [[nodiscard]] float getEffectiveLinearGain() const noexcept { return gainPlan_.getEffectiveLinearGain(); }

    // De-coloring policy queries
    [[nodiscard]] bool isInverseCompensationEnabled() const noexcept;
    [[nodiscard]] float getInverseCompensationMaxBoostDb() const noexcept;

private:
    std::optional<CalibrationSnapshot> activeSnapshot_;
    InputGainPlan gainPlan_;
    bool isActive_ { false };
    bool strictBufferSize_ { true };
};

} // namespace abdaudiolab::calibration
