/**
 * @file OscillatorShapingAnalytics.h
 * @brief Re-exports ABDSharedCode::AudioAnalytics::OscillatorShapingAnalytics for ABDAudioLab.
 *        Provides PWM Duty Cycle, FM Bessel Sideband Ratio, and Hard Sync / DWGS Harmonic Centroid probes.
 */

#pragma once

#include <AudioAnalytics/DspMetrologicalProbes.h>

namespace abdaudiolab::math::analytics
{

namespace OscillatorShapingAnalytics = abd::analytics::OscillatorShapingAnalytics;

} // namespace abdaudiolab::math::analytics
