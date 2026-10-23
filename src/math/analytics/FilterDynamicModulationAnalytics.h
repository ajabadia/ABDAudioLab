/**
 * @file FilterDynamicModulationAnalytics.h
 * @brief Re-exports ABDSharedCode::AudioAnalytics::FilterDynamicModulationAnalytics for ABDAudioLab.
 *        Provides Key Tracking Slope (+-200%), Bipolar Envelope Excursion, and Filter Self-Oscillation probes.
 */

#pragma once

#include <AudioAnalytics/DspMetrologicalProbes.h>

namespace abdaudiolab::math::analytics
{

namespace FilterDynamicModulationAnalytics = abd::analytics::FilterDynamicModulationAnalytics;

} // namespace abdaudiolab::math::analytics
