/**
 * @file FineLatencyAnalyzer.cpp
 * @brief Implementation of fine sub-sample latency and clock drift analyzer.
 * @author ABDSynths
 * @date 2026
 */

#include "FineLatencyAnalyzer.h"
#include "synth/Sha256.h"
#include "MeasurementDspUtils.h"
#include <juce_core/juce_core.h>
#include <algorithm>
#include <numeric>
#include <cmath>

namespace abdaudiolab::measurement
{

FineLatencyAnalyzer::SubsampleRefinementResult FineLatencyAnalyzer::refineParabolicThreePoint(
    double rPrev,
    double rPeak,
    double rNext,
    double snrDbfs) noexcept
{
    SubsampleRefinementResult res;

    // Check for local maximum condition
    if (rPeak < rPrev || rPeak < rNext)
    {
        res.valid = false;
        res.delta = 0.0;
        res.interpolatedPeak = rPeak;
        res.curvature = 0.0;
        res.estimatedErrorBound = 0.5;
        return res;
    }

    const double denom = 2.0 * (rPrev - 2.0 * rPeak + rNext);
    const double curvature = std::abs(rPrev - 2.0 * rPeak + rNext);
    res.curvature = curvature;

    // Degenerate flat peak or near-zero curvature
    if (std::abs(denom) < 1e-12 || curvature < 1e-9)
    {
        res.valid = false;
        res.delta = 0.0;
        res.interpolatedPeak = rPeak;
        res.estimatedErrorBound = 0.5;
        return res;
    }

    const double delta = (rPrev - rNext) / denom;

    // Parabolic vertex must lie within the [-0.5, 0.5] interval around the integer peak
    if (delta < -0.5 || delta > 0.5 || std::isnan(delta))
    {
        res.valid = false;
        res.delta = 0.0;
        res.interpolatedPeak = rPeak;
        res.estimatedErrorBound = 0.5;
        return res;
    }

    res.delta = delta;
    res.interpolatedPeak = rPeak - ((rPrev - rNext) * (rPrev - rNext)) / (4.0 * denom);
    res.valid = true;

    // Bounded uncertainty based on SNR and peak curvature:
    // Flatter peaks (low curvature) or low SNR widen the uncertainty bound.
    const double linearSnr = std::max(1.0, std::pow(10.0, snrDbfs / 20.0));
    const double rawBound = (1.0 / (linearSnr * std::max(0.01, curvature)));
    res.estimatedErrorBound = std::clamp(rawBound, 0.001, 0.5);

    return res;
}

std::string FineLatencyAnalyzer::computeAudioSha256(std::span<const float> audio)
{
    if (audio.empty())
    {
        return abdaudiolab::synth::Sha256::computeHex(nullptr, 0);
    }
    return abdaudiolab::synth::Sha256::computeHex(reinterpret_cast<const void*>(audio.data()), audio.size_bytes());
}

namespace
{
    // Helper to compute normalized cross-correlation for a given lag
    double computeNormalizedCorrelationAtLag(
        std::span<const float> x,
        std::span<const float> y,
        int lag) noexcept
    {
        const int nX = static_cast<int>(x.size());
        const int nY = static_cast<int>(y.size());
        if (lag < 0 || lag >= nY) return 0.0;

        const int len = std::min(nX, nY - lag);
        if (len <= 0) return 0.0;

        double sumXY = 0.0;
        double sumX2 = 0.0;
        double sumY2 = 0.0;

        for (int i = 0; i < len; ++i)
        {
            const double sX = static_cast<double>(x[static_cast<size_t>(i)]);
            const double sY = static_cast<double>(y[static_cast<size_t>(lag + i)]);
            sumXY += sX * sY;
            sumX2 += sX * sX;
            sumY2 += sY * sY;
        }

        const double denom = std::sqrt(sumX2 * sumY2);
        if (denom <= 1e-12) return 0.0;
        return sumXY / denom;
    }
}

FineLatencyCalibrationRecord FineLatencyAnalyzer::analyzeFineLatencyAndDrift(
    std::span<const float> stimulus,
    std::span<const float> capture,
    double sampleRate,
    const FineLatencyConfig& config,
    const std::string& calibrationId)
{
    FineLatencyCalibrationRecord record;
    record.calibrationId = calibrationId.empty() ? ("calib_" + std::to_string(static_cast<int64_t>(sampleRate)) + "_" + std::to_string(capture.size())) : calibrationId;
    record.nominalSampleRateHz = sampleRate;
    record.clockTopology = config.clockTopology;
    record.refinementMethod = config.refinementMethod;
    record.rawReferenceSha256 = computeAudioSha256(stimulus);
    record.rawCaptureSha256 = computeAudioSha256(capture);

    // 1. Validate signal presence and SNR
    const double stimulusRms = computeRmsDbfs(stimulus);
    const double captureRms = computeRmsDbfs(capture);

    if (stimulus.empty() || capture.empty() || captureRms < config.minSnrDbfs || stimulusRms < config.minSnrDbfs)
    {
        record.status = "insufficient_signal";
        record.fitStatus = LinearFitStatus::Invalid;
        record.driftInterpretation = DriftInterpretation::NotIdentifiable;
        return record;
    }

    const double estimatedSnr = std::clamp(captureRms - config.minSnrDbfs, 10.0, 90.0);

    // 2. Initial global cross-correlation to find fixed latency and check for ambiguity
    const int maxLag = std::min(config.maxSearchLagSamples, static_cast<int>(capture.size()));
    std::vector<double> rValues(static_cast<size_t>(maxLag), 0.0);

    double maxR = -1.0;
    int bestLag = 0;

    for (int lag = 0; lag < maxLag; ++lag)
    {
        const double r = computeNormalizedCorrelationAtLag(stimulus, capture, lag);
        rValues[static_cast<size_t>(lag)] = r;
        if (r > maxR)
        {
            maxR = r;
            bestLag = lag;
        }
    }

    // Check second peak for ambiguity
    double secondPeakR = 0.0;
    for (int lag = 0; lag < maxLag; ++lag)
    {
        if (std::abs(lag - bestLag) > 16 && rValues[static_cast<size_t>(lag)] > secondPeakR)
        {
            secondPeakR = rValues[static_cast<size_t>(lag)];
        }
    }

    record.globalConfidence = std::clamp(maxR, 0.0, 1.0);
    record.peakRatio = (maxR > 1e-6) ? (secondPeakR / maxR) : 1.0;
    record.ambiguityMargin = (maxR > 1e-6) ? ((maxR - secondPeakR) / maxR) : 0.0;

    if (maxR < config.minCorrelationConfidence)
    {
        record.status = "insufficient_signal";
        record.fitStatus = LinearFitStatus::Invalid;
        record.driftInterpretation = DriftInterpretation::NotIdentifiable;
        return record;
    }

    if (record.peakRatio >= config.maxPeakRatio || record.ambiguityMargin <= config.ambiguityMarginThreshold)
    {
        record.status = "ambiguous";
        // Continue but mark ambiguity in status
    }

    // 3. Sub-sample refinement on the global peak
    double globalFractionalDelta = 0.0;
    if (config.refinementMethod == SubsampleRefinementMethod::ParabolicThreePoint && bestLag > 0 && bestLag < maxLag - 1)
    {
        const auto refResult = refineParabolicThreePoint(
            rValues[static_cast<size_t>(bestLag - 1)],
            rValues[static_cast<size_t>(bestLag)],
            rValues[static_cast<size_t>(bestLag + 1)],
            estimatedSnr);

        if (refResult.valid)
        {
            globalFractionalDelta = refResult.delta;
            record.estimatedErrorBoundSamples = refResult.estimatedErrorBound;
        }
        else
        {
            record.estimatedErrorBoundSamples = 0.5;
        }
    }

    record.integerLatencySamples = bestLag;
    record.fractionalLatencySamples = globalFractionalDelta;
    record.totalLatencySamples = static_cast<double>(bestLag) + globalFractionalDelta;
    record.fixedLatencySeconds = record.totalLatencySamples / sampleRate;

    // 4. Multi-window analysis along the capture timeline for clock drift & discontinuities
    const int winSize = std::max(256, config.analysisWindowSizeSamples);
    const int hopSize = std::max(128, config.analysisHopSizeSamples);
    const int totalCaptureSamples = static_cast<int>(capture.size());

    std::vector<ClockDriftObservation> observations;

    // Ensure we have enough signal to perform multi-window extraction
    if (totalCaptureSamples >= bestLag + winSize)
    {
        for (int startY = bestLag; startY + winSize <= totalCaptureSamples; startY += hopSize)
        {
            const int startX = startY - bestLag;
            if (startX + winSize > static_cast<int>(stimulus.size())) break;

            const double elapsedTimeSec = static_cast<double>(startY) / sampleRate;

            // Search in a local lag window [-32, +32] around expected offset
            const int localSearchRange = 32;
            double localMaxR = -1.0;
            int localBestOffset = bestLag;

            for (int dLag = -localSearchRange; dLag <= localSearchRange; ++dLag)
            {
                const int testLag = bestLag + dLag;
                const int capStart = startX + testLag;
                if (testLag < 0 || capStart < 0 || capStart + winSize > totalCaptureSamples)
                    continue;

                // Window correlation
                std::span<const float> winX(&stimulus[static_cast<size_t>(startX)], static_cast<size_t>(winSize));
                std::span<const float> winY(&capture[static_cast<size_t>(capStart)], static_cast<size_t>(winSize));

                const double r = computeNormalizedCorrelationAtLag(winX, winY, 0);
                if (r > localMaxR)
                {
                    localMaxR = r;
                    localBestOffset = testLag;
                }
            }

            ClockDriftObservation obs;
            obs.elapsedTimeSeconds = elapsedTimeSec;
            obs.integerOffsetSamples = localBestOffset;
            obs.correlationConfidence = std::clamp(localMaxR, 0.0, 1.0);

            // Refine local offset sub-sample
            const int capBestStart = startX + localBestOffset;
            if (config.refinementMethod == SubsampleRefinementMethod::ParabolicThreePoint && capBestStart > 0 && capBestStart + winSize + 1 <= totalCaptureSamples)
            {
                std::span<const float> winX(&stimulus[static_cast<size_t>(startX)], static_cast<size_t>(winSize));
                std::span<const float> winYPrev(&capture[static_cast<size_t>(capBestStart - 1)], static_cast<size_t>(winSize));
                std::span<const float> winYPeak(&capture[static_cast<size_t>(capBestStart)], static_cast<size_t>(winSize));
                std::span<const float> winYNext(&capture[static_cast<size_t>(capBestStart + 1)], static_cast<size_t>(winSize));

                const double rPrev = computeNormalizedCorrelationAtLag(winX, winYPrev, 0);
                const double rPeak = computeNormalizedCorrelationAtLag(winX, winYPeak, 0);
                const double rNext = computeNormalizedCorrelationAtLag(winX, winYNext, 0);

                const auto sub = refineParabolicThreePoint(rPrev, rPeak, rNext, estimatedSnr);
                if (sub.valid)
                {
                    obs.localOffsetFractionalSamples = sub.delta;
                    obs.estimatedErrorBoundSamples = sub.estimatedErrorBound;
                }
            }

            obs.localOffsetSamples = static_cast<double>(obs.integerOffsetSamples) + obs.localOffsetFractionalSamples;
            observations.push_back(obs);
        }
    }

    // 5. Linear drift regression and anomaly diagnostics
    fitLinearDrift(observations, sampleRate, config.clockTopology, config.maxAcceptableDropoutJumpSamples, record);
    record.observations = observations;

    return record;
}

void FineLatencyAnalyzer::fitLinearDrift(
    std::vector<ClockDriftObservation>& observations,
    double sampleRate,
    ClockTopology topology,
    double maxJumpThreshold,
    FineLatencyCalibrationRecord& outRecord)
{
    outRecord.discontinuities.clear();

    if (observations.empty())
    {
        outRecord.fitStatus = LinearFitStatus::InsufficientSpan;
        outRecord.driftInterpretation = DriftInterpretation::NotIdentifiable;
        return;
    }

    // Check for discontinuities / dropouts between consecutive windows
    for (size_t i = 1; i < observations.size(); ++i)
    {
        const double jump = observations[i].localOffsetSamples - observations[i - 1].localOffsetSamples;
        if (std::abs(jump) > maxJumpThreshold)
        {
            TimingDiscontinuity disc;
            disc.elapsedTimeSeconds = observations[i].elapsedTimeSeconds;
            disc.jumpSamples = jump;
            disc.type = "dropout";
            outRecord.discontinuities.push_back(disc);
        }
    }

    const double timeSpan = observations.back().elapsedTimeSeconds - observations.front().elapsedTimeSeconds;
    if (observations.size() < 2 || timeSpan < 0.05)
    {
        outRecord.fitStatus = LinearFitStatus::InsufficientSpan;
        outRecord.driftInterpretation = DriftInterpretation::NotIdentifiable;
        outRecord.inlierCount = static_cast<int>(observations.size());
        return;
    }

    // Initial least-squares regression: offset = b + m * t
    const size_t n = observations.size();
    double sumT = 0.0;
    double sumO = 0.0;
    double sumT2 = 0.0;
    double sumTO = 0.0;

    for (const auto& obs : observations)
    {
        sumT += obs.elapsedTimeSeconds;
        sumO += obs.localOffsetSamples;
        sumT2 += obs.elapsedTimeSeconds * obs.elapsedTimeSeconds;
        sumTO += obs.elapsedTimeSeconds * obs.localOffsetSamples;
    }

    const double denom = (static_cast<double>(n) * sumT2 - sumT * sumT);
    if (std::abs(denom) < 1e-12)
    {
        outRecord.fitStatus = LinearFitStatus::Invalid;
        outRecord.driftInterpretation = DriftInterpretation::NotIdentifiable;
        return;
    }

    double m = (static_cast<double>(n) * sumTO - sumT * sumO) / denom;
    double b = (sumO - m * sumT) / static_cast<double>(n);

    // Compute residuals and MAD (Median Absolute Deviation)
    std::vector<double> residuals(n);
    for (size_t i = 0; i < n; ++i)
    {
        residuals[i] = observations[i].localOffsetSamples - (b + m * observations[i].elapsedTimeSeconds);
    }

    std::vector<double> sortedRes = residuals;
    std::sort(sortedRes.begin(), sortedRes.end());
    const double medianRes = sortedRes[n / 2];

    std::vector<double> absDev(n);
    for (size_t i = 0; i < n; ++i)
        absDev[i] = std::abs(residuals[i] - medianRes);
    std::sort(absDev.begin(), absDev.end());
    const double mad = absDev[n / 2];
    outRecord.residualMedianAbsoluteDeviation = mad;

    // Outlier filtering threshold (3.0 * 1.4826 * MAD)
    const double outlierThresh = std::max(0.1, 3.0 * 1.4826 * mad);

    int inliers = 0;
    int outliers = 0;
    double sumT_in = 0.0;
    double sumO_in = 0.0;
    double sumT2_in = 0.0;
    double sumTO_in = 0.0;

    for (size_t i = 0; i < n; ++i)
    {
        if (std::abs(residuals[i]) > outlierThresh)
        {
            observations[i].isOutlier = true;
            ++outliers;
        }
        else
        {
            observations[i].isOutlier = false;
            ++inliers;
            sumT_in += observations[i].elapsedTimeSeconds;
            sumO_in += observations[i].localOffsetSamples;
            sumT2_in += observations[i].elapsedTimeSeconds * observations[i].elapsedTimeSeconds;
            sumTO_in += observations[i].elapsedTimeSeconds * observations[i].localOffsetSamples;
        }
    }

    outRecord.inlierCount = inliers;
    outRecord.outlierCount = outliers;

    // Refit using inliers if sufficient
    if (inliers >= 2)
    {
        const double denom_in = (static_cast<double>(inliers) * sumT2_in - sumT_in * sumT_in);
        if (std::abs(denom_in) > 1e-12)
        {
            m = (static_cast<double>(inliers) * sumTO_in - sumT_in * sumO_in) / denom_in;
            b = (sumO_in - m * sumT_in) / static_cast<double>(inliers);
        }
    }

    // Compute R^2, max residual, and jitter (standard deviation of inlier residuals)
    double ssTot = 0.0;
    double ssRes = 0.0;
    double maxRes = 0.0;
    const double meanO_in = (inliers > 0) ? (sumO_in / static_cast<double>(inliers)) : 0.0;

    for (size_t i = 0; i < n; ++i)
    {
        const double fitVal = b + m * observations[i].elapsedTimeSeconds;
        const double res = observations[i].localOffsetSamples - fitVal;
        if (std::abs(res) > maxRes) maxRes = std::abs(res);

        if (!observations[i].isOutlier)
        {
            ssTot += (observations[i].localOffsetSamples - meanO_in) * (observations[i].localOffsetSamples - meanO_in);
            ssRes += res * res;
        }
    }

    outRecord.maxResidualSamples = maxRes;
    outRecord.shortTermJitterSamples = (inliers > 1) ? std::sqrt(ssRes / static_cast<double>(inliers - 1)) : 0.0;
    outRecord.driftRSquared = (ssTot > 1e-9) ? std::clamp(1.0 - (ssRes / ssTot), 0.0, 1.0) : 1.0;

    outRecord.slopeSamplesPerSecond = m;
    outRecord.fittedDriftRatePpm = (m / sampleRate) * 1e6;

    // Clock topology interpretation
    if (topology == ClockTopology::SharedClock)
    {
        outRecord.driftInterpretation = DriftInterpretation::ResidualJitter;
    }
    else if (topology == ClockTopology::IndependentClocks)
    {
        outRecord.driftInterpretation = DriftInterpretation::RelativeClockDrift;
    }
    else
    {
        outRecord.driftInterpretation = (std::abs(outRecord.fittedDriftRatePpm) > 2.0)
            ? DriftInterpretation::RelativeClockDrift
            : DriftInterpretation::ResidualJitter;
    }

    // Status classification
    if (!outRecord.discontinuities.empty())
    {
        outRecord.fitStatus = (inliers >= 4 && outliers <= 2) ? LinearFitStatus::OutlierContaminated : LinearFitStatus::Invalid;
        outRecord.status = (outRecord.fitStatus == LinearFitStatus::Invalid) ? "calibration_invalid" : "degraded";
    }
    else if (outliers > 0)
    {
        outRecord.fitStatus = LinearFitStatus::OutlierContaminated;
        if (outRecord.status == "resolved") outRecord.status = "degraded";
    }
    else if (outRecord.driftRSquared < 0.85 && std::abs(outRecord.fittedDriftRatePpm) > 2.0)
    {
        outRecord.fitStatus = LinearFitStatus::NonlinearDrift;
        if (outRecord.status == "resolved") outRecord.status = "degraded";
    }
    else
    {
        outRecord.fitStatus = LinearFitStatus::LinearFit;
    }
}

std::vector<ChannelSkewObservation> FineLatencyAnalyzer::analyzeMultichannelSkew(
    const std::vector<std::span<const float>>& multichannelCapture,
    double sampleRate,
    const FineLatencyConfig& config)
{
    std::vector<ChannelSkewObservation> skews;
    if (multichannelCapture.size() < 2)
    {
        return skews;
    }

    const auto& refChan = multichannelCapture[0];
    const int maxLag = std::min(4096, static_cast<int>(refChan.size()));

    for (size_t c = 1; c < multichannelCapture.size(); ++c)
    {
        const auto& testChan = multichannelCapture[c];
        ChannelSkewObservation obs;
        obs.channelIndex = static_cast<int>(c);
        obs.referenceChannelIndex = 0;
        obs.phaseReferenceFrequencyHz = config.phaseReferenceFrequencyHz;

        if (testChan.empty() || refChan.empty())
        {
            obs.status = "low_coherence";
            skews.push_back(obs);
            continue;
        }

        // Cross-correlation between Channel 0 and Channel C for lag search in [-search, +search]
        const int searchWindow = std::min(1024, maxLag / 2);
        double maxPosR = -1.0;
        double minNegR = 1.0;
        int bestPosLag = 0;
        int bestNegLag = 0;

        std::vector<double> rLags(static_cast<size_t>(searchWindow * 2 + 1), 0.0);

        for (int lag = -searchWindow; lag <= searchWindow; ++lag)
        {
            double r = 0.0;
            if (lag >= 0)
                r = computeNormalizedCorrelationAtLag(refChan, testChan, lag);
            else
                r = computeNormalizedCorrelationAtLag(testChan, refChan, -lag);

            const size_t idx = static_cast<size_t>(lag + searchWindow);
            rLags[idx] = r;

            if (r > maxPosR)
            {
                maxPosR = r;
                bestPosLag = lag;
            }
            if (r < minNegR)
            {
                minNegR = r;
                bestNegLag = lag;
            }
        }

        // Detect polarity inversion if negative correlation is stronger
        if (std::abs(minNegR) > maxPosR && std::abs(minNegR) > 0.30)
        {
            obs.polarityInverted = true;
            obs.confidence = std::abs(minNegR);

            // Refine around bestNegLag
            const size_t negIdx = static_cast<size_t>(bestNegLag + searchWindow);
            double delta = 0.0;
            if (negIdx > 0 && negIdx < rLags.size() - 1)
            {
                // Invert values to use standard parabolic peak finder
                const auto sub = refineParabolicThreePoint(-rLags[negIdx - 1], -rLags[negIdx], -rLags[negIdx + 1], 60.0);
                if (sub.valid) delta = sub.delta;
            }
            obs.skewSamples = static_cast<double>(bestNegLag) + delta;
            obs.status = "polarity_inverted";
        }
        else
        {
            obs.polarityInverted = false;
            obs.confidence = std::clamp(maxPosR, 0.0, 1.0);

            // Refine around bestPosLag
            const size_t posIdx = static_cast<size_t>(bestPosLag + searchWindow);
            double delta = 0.0;
            if (posIdx > 0 && posIdx < rLags.size() - 1)
            {
                const auto sub = refineParabolicThreePoint(rLags[posIdx - 1], rLags[posIdx], rLags[posIdx + 1], 60.0);
                if (sub.valid) delta = sub.delta;
            }
            obs.skewSamples = static_cast<double>(bestPosLag) + delta;
            obs.status = (obs.confidence >= config.minCorrelationConfidence) ? "resolved" : "low_coherence";
        }

        obs.skewNanoseconds = (obs.skewSamples / sampleRate) * 1e9;

        // Compute phase angle at reference frequency: phi = 2 * pi * f * delta_t
        const double timeDelaySec = obs.skewSamples / sampleRate;
        const double phaseRad = 2.0 * 3.14159265358979323846 * config.phaseReferenceFrequencyHz * timeDelaySec;
        obs.phaseAngleRad = phaseRad;

        skews.push_back(obs);
    }

    return skews;
}

LatencyCompensationView FineLatencyAnalyzer::createCompensationView(
    std::span<const float> rawCapture,
    CompensationTransformationType type,
    const FineLatencyCalibrationRecord& latencyRecord)
{
    if (type == CompensationTransformationType::TimeAxisOnly)
    {
        return LatencyCompensationView(
            rawCapture,
            CompensationTransformationType::TimeAxisOnly,
            latencyRecord.fixedLatencySeconds,
            latencyRecord.nominalSampleRateHz);
    }

    if (type == CompensationTransformationType::IntegerShift)
    {
        const int shift = latencyRecord.integerLatencySamples;
        std::vector<float> shifted(rawCapture.size(), 0.0f);
        if (shift >= 0 && static_cast<size_t>(shift) < rawCapture.size())
        {
            std::copy(rawCapture.begin() + shift, rawCapture.end(), shifted.begin());
        }
        return LatencyCompensationView(
            rawCapture,
            CompensationTransformationType::IntegerShift,
            latencyRecord.fixedLatencySeconds,
            latencyRecord.nominalSampleRateHz,
            std::move(shifted));
    }

    // Default fallback to TimeAxisOnly
    return LatencyCompensationView(
        rawCapture,
        CompensationTransformationType::TimeAxisOnly,
        latencyRecord.fixedLatencySeconds,
        latencyRecord.nominalSampleRateHz);
}

} // namespace abdaudiolab::measurement
