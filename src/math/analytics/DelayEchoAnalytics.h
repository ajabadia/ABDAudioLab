/**
 * @file DelayEchoAnalytics.h
 * @brief Re-exports ABDSharedCode::AudioAnalytics::DelayEchoAnalytics for ABDAudioLab.
 *        Provides Impulse Echo Tap Delay and Stereo Phase Quadrature / Decorrelation probes.
 */

#pragma once

#include <AudioAnalytics/DspMetrologicalProbes.h>

namespace abdaudiolab::math::analytics
{

namespace DelayEchoAnalytics = abd::analytics::DelayEchoAnalytics;

} // namespace abdaudiolab::math::analytics
