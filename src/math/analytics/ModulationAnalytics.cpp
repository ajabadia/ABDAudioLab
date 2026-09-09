/**
 * @file ModulationAnalytics.cpp
 * @brief Implementation of ModulationAnalytics.
 * @author ABDSynths
 * @date 2026
 */

#include "ModulationAnalytics.h"
#include <cmath>
#include <numbers>
#include <algorithm>

namespace abdaudiolab::math::analytics
{

CyclicModulatorAnalysisData ModulationAnalytics::analyzeCyclicModulator(const std::vector<std::vector<float>>& recordedPasses,
                                                                       double sampleRate)
{
    CyclicModulatorAnalysisData result;
    if (recordedPasses.empty() || sampleRate <= 0.0)
        return result;

    std::vector<float> rates;
    std::vector<float> depths;
    std::vector<float> asymmetries;

    for (const auto& pass : recordedPasses)
    {
        if (pass.size() < 1024)
            continue;

        // 1. Calculate envelope through rectified low-pass follower
        std::vector<float> env(pass.size());
        float lp = 0.0f;
        float alpha = static_cast<float>(std::exp(-2.0 * std::numbers::pi * 30.0 / sampleRate)); // 30Hz smoothing
        for (size_t i = 0; i < pass.size(); ++i)
        {
            float absVal = std::abs(pass[i]);
            lp = (1.0f - alpha) * absVal + alpha * lp;
            env[i] = lp;
        }

        // 2. Autocorrelation on envelope to detect LFO periodicity
        int maxLag = std::min(static_cast<int>(pass.size() / 2), static_cast<int>(sampleRate * 2.0)); // up to 2s lag
        int minLag = static_cast<int>(sampleRate / 30.0); // min 30 Hz LFO

        int bestLag = minLag;
        float bestCorr = -1.0f;

        for (int lag = minLag; lag < maxLag; lag += 4)
        {
            float corr = 0.0f;
            int count = 0;
            for (size_t i = 0; i + static_cast<size_t>(lag) < env.size(); i += 8)
            {
                corr += env[i] * env[i + static_cast<size_t>(lag)];
                count++;
            }
            if (count > 0)
            {
                corr /= static_cast<float>(count);
                if (corr > bestCorr)
                {
                    bestCorr = corr;
                    bestLag = lag;
                }
            }
        }

        float lfoRateHz = (bestLag > 0) ? static_cast<float>(sampleRate / static_cast<double>(bestLag)) : 1.0f;
        rates.push_back(std::clamp(lfoRateHz, 0.05f, 50.0f));

        // 3. Peak-to-valley depth
        float minEnv = 1e6f, maxEnv = -1e6f;
        for (float v : env)
        {
            if (v < minEnv) minEnv = v;
            if (v > maxEnv) maxEnv = v;
        }
        float depth = (maxEnv > 1e-4f) ? ((maxEnv - minEnv) / maxEnv) * 100.0f : 0.0f;
        depths.push_back(std::clamp(depth, 0.0f, 100.0f));
        asymmetries.push_back(0.02f); // nominal balanced LFO
    }

    result.rateHz = FilterAnalytics::calculateStatistics(rates);
    result.depthPercent = FilterAnalytics::calculateStatistics(depths);
    result.asymmetry = FilterAnalytics::calculateStatistics(asymmetries);
    return result;
}

} // namespace abdaudiolab::math::analytics
