/**
 * @file ActiveCalibrationContext.cpp
 * @brief Implementation of ActiveCalibrationContext.
 * @author ABDSynths
 * @date 2026
 */

#include "ActiveCalibrationContext.h"
#include <algorithm>

namespace abdaudiolab::calibration
{

bool ActiveCalibrationContext::activate(
    const CalibrationSnapshot& snapshot,
    const CurrentAudioConfigurationSnapshot& currentConfig,
    bool strictBufferSize)
{
    strictBufferSize_ = strictBufferSize;

    auto eval = CalibrationCompatibilityEvaluator::evaluate(snapshot, currentConfig, strictBufferSize_);
    if (!eval.isCompatible())
    {
        deactivate();
        return false;
    }

    activeSnapshot_ = snapshot;
    isActive_ = true;

    // Set interface calibration trim from snapshot
    gainPlan_.interfaceCalibrationTrimDb = snapshot.result.interfaceTrimDb;
    gainPlan_.recalculate();

    return true;
}

void ActiveCalibrationContext::deactivate() noexcept
{
    activeSnapshot_ = std::nullopt;
    isActive_ = false;
    gainPlan_.interfaceCalibrationTrimDb = 0.0f;
    gainPlan_.recalculate();
}

void ActiveCalibrationContext::checkAlignment(const CurrentAudioConfigurationSnapshot& currentConfig)
{
    if (!isActive_ || !activeSnapshot_.has_value())
        return;

    auto eval = CalibrationCompatibilityEvaluator::evaluate(*activeSnapshot_, currentConfig, strictBufferSize_);
    if (!eval.isCompatible())
    {
        deactivate();
    }
}

int ActiveCalibrationContext::getLatencyCompensationSamples() const noexcept
{
    if (!isActive_ || !activeSnapshot_.has_value())
        return 0;

    const auto& snap = *activeSnapshot_;
    if (!snap.processingPolicy.latencyCompensationEnabled)
        return 0;

    if (!snap.result.isValid() || snap.result.rtlSamples < 0)
        return 0;

    return snap.result.rtlSamples;
}

void ActiveCalibrationContext::setSessionTargetTrimDb(float sessionTrimDb) noexcept
{
    gainPlan_.sessionTargetTrimDb = sessionTrimDb;
    gainPlan_.recalculate();
}

bool ActiveCalibrationContext::isInverseCompensationEnabled() const noexcept
{
    if (!isActive_ || !activeSnapshot_.has_value())
        return false;

    return activeSnapshot_->processingPolicy.inverseCompensationEnabled;
}

float ActiveCalibrationContext::getInverseCompensationMaxBoostDb() const noexcept
{
    if (!isActive_ || !activeSnapshot_.has_value())
        return 0.0f;

    return activeSnapshot_->processingPolicy.inverseCompensationMaxBoostDb;
}

} // namespace abdaudiolab::calibration
