/**
 * @file FilterAnalytics.cpp
 * @brief Implementation of FilterAnalytics.
 * @author ABDSynths
 * @date 2026
 */

#include "FilterAnalytics.h"
#include <numeric>
#include <algorithm>
#include <cmath>

namespace abdaudiolab::math::analytics
{

FilterStatisticalPair FilterAnalytics::calculateStatistics(const std::vector<float>& dataset)
{
    FilterStatisticalPair res;
    if (dataset.empty())
        return res;

    if (dataset.size() == 1)
    {
        res.mean = dataset[0];
        res.stdDev = 0.0f;
        return res;
    }

    double sum = 0.0;
    for (float v : dataset)
        sum += v;

    double mean = sum / static_cast<double>(dataset.size());
    res.mean = static_cast<float>(mean);

    double varSum = 0.0;
    for (float v : dataset)
    {
        double diff = static_cast<double>(v) - mean;
        varSum += diff * diff;
    }

    double variance = varSum / static_cast<double>(dataset.size() - 1);
    res.stdDev = static_cast<float>(std::sqrt(std::max(0.0, variance)));

    return res;
}

FilterAnalysisData FilterAnalytics::analyzeFilterPasses(const std::vector<std::vector<float>>& recordedPasses,
                                                       const std::vector<float>& inverseFilter,
                                                       double sampleRate,
                                                       double durationSec,
                                                       float startFreqHz,
                                                       float endFreqHz)
{
    FilterAnalysisData result;
    if (recordedPasses.empty())
        return result;

    std::vector<float> cutoffs;
    std::vector<float> resonances;
    std::vector<float> thds;

    cutoffs.reserve(recordedPasses.size());
    resonances.reserve(recordedPasses.size());
    thds.reserve(recordedPasses.size());

    for (const auto& pass : recordedPasses)
    {
        auto deco = FarinaDeconvolver::deconvolve(pass, inverseFilter, sampleRate, durationSec, startFreqHz, endFreqHz);
        cutoffs.push_back(deco.peakFrequencyHz);
        resonances.push_back(deco.resonancePeakDb);
        thds.push_back(deco.thdPercent);

        if (result.frequencyCurveHz.empty())
        {
            result.frequencyCurveHz = deco.frequenciesHz;
            result.magnitudeCurveDb = deco.frequencyResponseMagnitudeDb;
        }
    }

    result.cutoffHz = calculateStatistics(cutoffs);
    result.resonanceDb = calculateStatistics(resonances);
    result.thdPercent = calculateStatistics(thds);

    return result;
}

double FilterAnalytics::detectPassbandGain(const std::vector<float>& freqs,
                                          const std::vector<float>& magsDb,
                                          measurement::FilterTopology topology,
                                          bool& outObservable)
{
    outObservable = false;
    if (freqs.empty() || magsDb.empty() || freqs.size() != magsDb.size())
        return 0.0;

    std::vector<float> passbandSamples;

    if (topology == measurement::FilterTopology::highPass)
    {
        for (size_t i = 0; i < freqs.size(); ++i)
        {
            if (freqs[i] >= 5000.0f && freqs[i] <= 18000.0f)
                passbandSamples.push_back(magsDb[i]);
        }
    }
    else if (topology == measurement::FilterTopology::bandPass)
    {
        float maxMag = -150.0f;
        for (size_t i = 0; i < freqs.size(); ++i)
        {
            if (freqs[i] >= 20.0f && freqs[i] <= 20000.0f)
                maxMag = std::max(maxMag, magsDb[i]);
        }
        outObservable = (maxMag > -100.0f);
        return static_cast<double>(maxMag);
    }
    else
    {
        for (size_t i = 0; i < freqs.size(); ++i)
        {
            if (freqs[i] >= 20.0f && freqs[i] <= 200.0f)
                passbandSamples.push_back(magsDb[i]);
        }

        if (passbandSamples.size() < 3)
        {
            size_t count = std::min(freqs.size(), std::max(size_t(3), freqs.size() / 20));
            passbandSamples.clear();
            for (size_t i = 0; i < count; ++i)
                passbandSamples.push_back(magsDb[i]);
        }
    }

    if (passbandSamples.empty())
        return 0.0;

    std::sort(passbandSamples.begin(), passbandSamples.end());
    double median = passbandSamples[passbandSamples.size() / 2];

    float minVal = passbandSamples.front();
    float maxVal = passbandSamples.back();
    outObservable = ((maxVal - minVal) < 12.0f && median > -100.0);

    return median;
}

measurement::CutoffMetrics FilterAnalytics::extractCutoffMetrics(
    const std::vector<float>& freqs,
    const std::vector<float>& magsDb,
    double passbandGainDb,
    measurement::FilterTopology topology)
{
    measurement::CutoffMetrics m;
    m.lowerCutoffHz.name = "lowerCutoff";
    m.lowerCutoffHz.unit = "Hz";
    m.upperCutoffHz.name = "upperCutoff";
    m.upperCutoffHz.unit = "Hz";
    m.bandwidthHz.name = "bandwidth";
    m.bandwidthHz.unit = "Hz";

    if (freqs.empty() || magsDb.empty() || freqs.size() != magsDb.size())
    {
        m.lowerCutoffHz.status = "not_observable";
        m.lowerCutoffHz.reason = "empty_or_mismatched_curve";
        m.upperCutoffHz.status = "not_observable";
        m.upperCutoffHz.reason = "empty_or_mismatched_curve";
        m.bandwidthHz.status = "not_observable";
        m.bandwidthHz.reason = "empty_or_mismatched_curve";
        return m;
    }

    const double target3Db = passbandGainDb - 3.0103;

    if (topology == measurement::FilterTopology::allPass ||
        topology == measurement::FilterTopology::comb)
    {
        m.lowerCutoffHz.status = "not_applicable";
        m.lowerCutoffHz.reason = "topology_has_no_cutoff_transition";
        m.upperCutoffHz.status = "not_applicable";
        m.upperCutoffHz.reason = "topology_has_no_cutoff_transition";
        m.bandwidthHz.status = "not_applicable";
        m.bandwidthHz.reason = "topology_has_no_cutoff_transition";
        return m;
    }

    if (topology == measurement::FilterTopology::highPass)
    {
        m.upperCutoffHz.status = "not_applicable";
        m.bandwidthHz.status = "not_applicable";

        bool found = false;
        double lowerFc = 0.0;
        for (int i = static_cast<int>(freqs.size()) - 1; i > 0; --i)
        {
            if (magsDb[i] >= target3Db && magsDb[i - 1] < target3Db)
            {
                double frac = (target3Db - magsDb[i - 1]) / (magsDb[i] - magsDb[i - 1]);
                lowerFc = freqs[i - 1] + frac * (freqs[i] - freqs[i - 1]);
                found = true;
                break;
            }
        }

        if (found && lowerFc >= 10.0 && lowerFc <= 22000.0)
        {
            m.lowerCutoffHz.value = lowerFc;
            m.lowerCutoffHz.status = "observed";
        }
        else
        {
            m.lowerCutoffHz.status = "not_observable";
            m.lowerCutoffHz.reason = "no_3db_crossing_found";
        }
        return m;
    }

    if (topology == measurement::FilterTopology::bandPass)
    {
        size_t peakIdx = 0;
        float maxVal = -150.0f;
        for (size_t i = 0; i < freqs.size(); ++i)
        {
            if (freqs[i] >= 20.0f && freqs[i] <= 20000.0f && magsDb[i] > maxVal)
            {
                maxVal = magsDb[i];
                peakIdx = i;
            }
        }

        double peakTarget = maxVal - 3.0103;
        bool foundLower = false;
        double lowerFc = 0.0;
        for (int i = static_cast<int>(peakIdx); i > 0; --i)
        {
            if (magsDb[i] >= peakTarget && magsDb[i - 1] < peakTarget)
            {
                double frac = (peakTarget - magsDb[i - 1]) / (magsDb[i] - magsDb[i - 1]);
                lowerFc = freqs[i - 1] + frac * (freqs[i] - freqs[i - 1]);
                foundLower = true;
                break;
            }
        }

        bool foundUpper = false;
        double upperFc = 0.0;
        for (size_t i = peakIdx; i + 1 < freqs.size(); ++i)
        {
            if (magsDb[i] >= peakTarget && magsDb[i + 1] < peakTarget)
            {
                double frac = (peakTarget - magsDb[i]) / (magsDb[i + 1] - magsDb[i]);
                upperFc = freqs[i] + frac * (freqs[i + 1] - freqs[i]);
                foundUpper = true;
                break;
            }
        }

        if (foundLower)
        {
            m.lowerCutoffHz.value = lowerFc;
            m.lowerCutoffHz.status = "observed";
        }
        else
        {
            m.lowerCutoffHz.status = "unreliable";
            m.lowerCutoffHz.reason = "lower_3db_crossing_not_observable";
        }

        if (foundUpper)
        {
            m.upperCutoffHz.value = upperFc;
            m.upperCutoffHz.status = "observed";
        }
        else
        {
            m.upperCutoffHz.status = "unreliable";
            m.upperCutoffHz.reason = "upper_3db_crossing_not_observable";
        }

        if (foundLower && foundUpper && upperFc > lowerFc)
        {
            m.bandwidthHz.value = upperFc - lowerFc;
            m.bandwidthHz.status = "observed";
        }
        else
        {
            m.bandwidthHz.status = "unreliable";
            m.bandwidthHz.reason = "bandwidth_crossings_not_observable";
        }
        return m;
    }

    // Default: lowPass or unknown
    m.lowerCutoffHz.status = "not_applicable";
    m.bandwidthHz.status = "not_applicable";

    bool found = false;
    double upperFc = 0.0;
    for (size_t i = 1; i < freqs.size(); ++i)
    {
        if (freqs[i] >= 20.0f && magsDb[i - 1] >= target3Db && magsDb[i] < target3Db)
        {
            double frac = (target3Db - magsDb[i - 1]) / (magsDb[i] - magsDb[i - 1]);
            upperFc = freqs[i - 1] + frac * (freqs[i] - freqs[i - 1]);
            found = true;
            break;
        }
    }

    if (found && upperFc >= 19000.0)
    {
        // Guard against sweep termination edge: check if audio band 100Hz - 18kHz is essentially flat
        float minVal = 100.0f, maxVal = -100.0f;
        int count = 0;
        for (size_t i = 0; i < freqs.size(); ++i)
        {
            if (freqs[i] >= 100.0f && freqs[i] <= 18000.0f)
            {
                minVal = std::min(minVal, magsDb[i]);
                maxVal = std::max(maxVal, magsDb[i]);
                count++;
            }
        }
        if (count > 20 && (maxVal - minVal) < 3.5f)
        {
            found = false;
        }
    }

    if (found && upperFc >= 20.0 && upperFc <= 22000.0)
    {
        m.upperCutoffHz.value = upperFc;
        m.upperCutoffHz.status = "observed";
    }
    else
    {
        m.upperCutoffHz.value = 0.0;
        m.upperCutoffHz.status = "not_observable";
        m.upperCutoffHz.reason = "no_3db_crossing_found";
    }

    return m;
}

void FilterAnalytics::extractResonancePeak(const std::vector<float>& freqs,
                                          const std::vector<float>& magsDb,
                                          double passbandGainDb,
                                          measurement::MeasurementMetric& outPeakHz,
                                          measurement::MeasurementMetric& outGainDb)
{
    outPeakHz.name = "resonanceFrequency";
    outPeakHz.unit = "Hz";
    outPeakHz.status = "observed";

    outGainDb.name = "resonanceGain";
    outGainDb.unit = "dB";
    outGainDb.status = "observed";

    float maxMag = -150.0f;
    float peakFreq = 0.0f;

    for (size_t i = 0; i < freqs.size(); ++i)
    {
        if (freqs[i] >= 20.0f && freqs[i] <= 20000.0f && magsDb[i] > maxMag)
        {
            maxMag = magsDb[i];
            peakFreq = freqs[i];
        }
    }

    outPeakHz.value = peakFreq;

    double boost = maxMag - passbandGainDb;
    if (boost > 0.5)
    {
        outGainDb.value = boost;
        outGainDb.status = "observed";
    }
    else
    {
        outGainDb.value = 0.0;
        outGainDb.status = "observed";
        outGainDb.reason = "critically_damped_or_flat";
    }
}

measurement::MeasurementMetric FilterAnalytics::calculateQFactor(
    const std::vector<float>& freqs,
    const std::vector<float>& magsDb,
    double resonancePeakHz,
    double resonancePeakDb)
{
    measurement::MeasurementMetric q;
    q.name = "qFactor";
    q.unit = "";
    q.value = 0.0;

    if (resonancePeakDb <= 0.5 || resonancePeakHz < 20.0)
    {
        q.status = "not_applicable";
        q.reason = "no_resonance_peak_for_q";
        return q;
    }

    size_t peakIdx = 0;
    float minDist = 1e9f;
    for (size_t i = 0; i < freqs.size(); ++i)
    {
        float d = std::abs(freqs[i] - static_cast<float>(resonancePeakHz));
        if (d < minDist)
        {
            minDist = d;
            peakIdx = i;
        }
    }

    double peakLevel = magsDb[peakIdx];
    double targetLevel = peakLevel - 3.0103;

    bool foundLeft = false;
    double fLeft = 0.0;
    for (int i = static_cast<int>(peakIdx); i > 0; --i)
    {
        if (magsDb[i] >= targetLevel && magsDb[i - 1] < targetLevel)
        {
            double frac = (targetLevel - magsDb[i - 1]) / (magsDb[i] - magsDb[i - 1]);
            fLeft = freqs[i - 1] + frac * (freqs[i] - freqs[i - 1]);
            foundLeft = true;
            break;
        }
    }

    bool foundRight = false;
    double fRight = 0.0;
    for (size_t i = peakIdx; i + 1 < freqs.size(); ++i)
    {
        if (magsDb[i] >= targetLevel && magsDb[i + 1] < targetLevel)
        {
            double frac = (targetLevel - magsDb[i]) / (magsDb[i + 1] - magsDb[i]);
            fRight = freqs[i] + frac * (freqs[i + 1] - freqs[i]);
            foundRight = true;
            break;
        }
    }

    if (foundLeft && foundRight && fRight > fLeft)
    {
        double bw = fRight - fLeft;
        q.value = resonancePeakHz / bw;
        q.status = "observed";
    }
    else
    {
        q.status = "unreliable";
        q.reason = "bandwidth_crossings_not_observable";
    }

    return q;
}

measurement::MeasurementMetric FilterAnalytics::calculateAsymptoticSlope(
    const std::vector<float>& freqs,
    const std::vector<float>& magsDb,
    double cutoffHz,
    double resonancePeakHz,
    double sampleRate,
    measurement::FilterTopology /*topology*/,
    measurement::SlopeFitMetadata& outFit)
{
    measurement::MeasurementMetric slope;
    slope.name = "rollOffSlope";
    slope.unit = "dB/oct";
    slope.value = 0.0;

    outFit = measurement::SlopeFitMetadata();

    if (freqs.empty() || magsDb.empty() || freqs.size() != magsDb.size())
    {
        slope.status = "unreliable";
        slope.reason = "empty_curve";
        outFit.selectionReason = "empty_curve";
        return slope;
    }

    // Flat / Pass-through check when cutoff is not observable
    if (cutoffHz <= 0.0)
    {
        float minM = 100.0f, maxM = -100.0f;
        int count = 0;
        for (size_t i = 0; i < freqs.size(); ++i)
        {
            if (freqs[i] >= 100.0f && freqs[i] <= 10000.0f)
            {
                minM = std::min(minM, magsDb[i]);
                maxM = std::max(maxM, magsDb[i]);
                count++;
            }
        }
        if (count > 10 && (maxM - minM) < 3.0f)
        {
            slope.value = 0.0;
            slope.status = "observed";
            outFit.frequencyStartHz = 100.0;
            outFit.frequencyEndHz = 10000.0;
            outFit.rSquared = 1.0;
            outFit.sampleCount = count;
            outFit.selectionReason = "flat_pass_through";
            return slope;
        }

        slope.status = "not_applicable";
        slope.reason = "no_cutoff_to_anchor_stopband";
        outFit.selectionReason = "no_cutoff_anchor";
        return slope;
    }

    double fStart = std::max(1.4 * cutoffHz, resonancePeakHz * 1.25);
    double fEnd = std::min(4.0 * cutoffHz, sampleRate * 0.42);

    if (fEnd <= fStart * 1.15)
    {
        fStart = 1.2 * cutoffHz;
        fEnd = std::min(3.0 * cutoffHz, sampleRate * 0.45);
    }

    std::vector<double> xLog2;
    std::vector<double> yDb;

    for (size_t i = 0; i < freqs.size(); ++i)
    {
        double f = freqs[i];
        if (f >= fStart && f <= fEnd && magsDb[i] > -90.0f)
        {
            xLog2.push_back(std::log2(f));
            yDb.push_back(static_cast<double>(magsDb[i]));
        }
    }

    if (xLog2.size() < 5)
    {
        slope.status = "unreliable";
        slope.reason = "insufficient_asymptotic_stopband_samples";
        outFit.selectionReason = "insufficient_samples";
        return slope;
    }

    double sumX = 0.0, sumY = 0.0;
    size_t n = xLog2.size();
    for (size_t i = 0; i < n; ++i)
    {
        sumX += xLog2[i];
        sumY += yDb[i];
    }
    double meanX = sumX / static_cast<double>(n);
    double meanY = sumY / static_cast<double>(n);

    double sxx = 0.0, sxy = 0.0, syy = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        double dx = xLog2[i] - meanX;
        double dy = yDb[i] - meanY;
        sxx += dx * dx;
        sxy += dx * dy;
        syy += dy * dy;
    }

    if (sxx < 1e-6)
    {
        slope.status = "unreliable";
        slope.reason = "singular_regression_window";
        return slope;
    }

    double slopeVal = sxy / sxx;
    double r2 = (syy > 1e-6) ? ((sxy * sxy) / (sxx * syy)) : 0.0;

    outFit.frequencyStartHz = std::pow(2.0, xLog2.front());
    outFit.frequencyEndHz = std::pow(2.0, xLog2.back());
    outFit.rSquared = r2;
    outFit.sampleCount = static_cast<int>(n);
    outFit.selectionReason = "asymptotic_stopband";

    slope.value = slopeVal;
    if (r2 >= 0.80)
    {
        slope.status = "observed";
    }
    else
    {
        slope.status = "unreliable";
        slope.reason = "non_linear_stopband_fit";
    }

    return slope;
}

FilterDetailedAnalysis FilterAnalytics::analyzeDeconvolvedFilter(
    const math::DeconvolutionResult& deco,
    double sampleRate,
    measurement::FilterTopology declaredTopology,
    measurement::MeasurementDomain domain)
{
    FilterDetailedAnalysis a;
    a.topology = declaredTopology;
    a.domain = domain;

    // 1. Passband gain
    bool passbandObs = false;
    a.passbandGainDbfs = detectPassbandGain(deco.frequenciesHz, deco.frequencyResponseMagnitudeDb, declaredTopology, passbandObs);
    a.passbandObservable = passbandObs;

    // 2. Cutoff metrics
    a.cutoff = extractCutoffMetrics(deco.frequenciesHz, deco.frequencyResponseMagnitudeDb, a.passbandGainDbfs, declaredTopology);
    
    // Compatibility alias: upperCutoffHz for lowpass, lowerCutoffHz for highpass
    if (declaredTopology == measurement::FilterTopology::highPass)
    {
        a.cutoffFrequencyHz = a.cutoff.lowerCutoffHz;
    }
    else
    {
        a.cutoffFrequencyHz = a.cutoff.upperCutoffHz;
    }
    a.cutoffFrequencyHz.name = "cutoffFrequency";

    // 3. Resonance peak
    extractResonancePeak(deco.frequenciesHz, deco.frequencyResponseMagnitudeDb, a.passbandGainDbfs, a.resonancePeakHz, a.resonanceGainDb);

    // 4. Q factor
    a.qFactor = calculateQFactor(deco.frequenciesHz, deco.frequencyResponseMagnitudeDb, a.resonancePeakHz.value, a.resonanceGainDb.value);

    // 5. Asymptotic slope
    double fc = (a.cutoffFrequencyHz.status == "observed") ? a.cutoffFrequencyHz.value : 0.0;
    a.rollOffSlopeDbPerOct = calculateAsymptoticSlope(
        deco.frequenciesHz, deco.frequencyResponseMagnitudeDb, fc, a.resonancePeakHz.value, sampleRate, declaredTopology, a.slopeFit);

    // 6. Distortion harmonics
    a.thdPercent.name = "thd";
    a.thdPercent.value = deco.thdPercent;
    a.thdPercent.unit = "%";
    a.thdPercent.status = "observed";

    a.h2Percent.name = "h2Distortion";
    a.h2Percent.value = deco.h2Percent;
    a.h2Percent.unit = "%";
    a.h2Percent.status = "observed";

    a.h3Percent.name = "h3Distortion";
    a.h3Percent.value = deco.h3Percent;
    a.h3Percent.unit = "%";
    a.h3Percent.status = "observed";

    // 7. Latency from passband group delay
    double sumDelaySamples = 0.0;
    int delayCount = 0;
    for (size_t i = 0; i < deco.frequenciesHz.size(); ++i)
    {
        if (deco.frequenciesHz[i] >= 50.0f && deco.frequenciesHz[i] <= 500.0f && i < deco.groupDelaySamples.size())
        {
            sumDelaySamples += deco.groupDelaySamples[i];
            delayCount++;
        }
    }
    double meanDelaySamples = (delayCount > 0) ? (sumDelaySamples / delayCount) : 0.0;
    a.passbandLatencyMs.name = "latency";
    a.passbandLatencyMs.value = (sampleRate > 0.0) ? (meanDelaySamples / sampleRate) * 1000.0 : 0.0;
    a.passbandLatencyMs.unit = "ms";
    a.passbandLatencyMs.status = "observed";

    // 8. Curves
    size_t numBins = deco.frequenciesHz.size();
    a.frequencyBinsHz.resize(numBins);
    a.magnitudeDb.resize(numBins);
    a.phaseRad.resize(numBins);
    a.groupDelayMs.resize(numBins);

    for (size_t i = 0; i < numBins; ++i)
    {
        a.frequencyBinsHz[i] = deco.frequenciesHz[i];
        a.magnitudeDb[i] = (i < deco.frequencyResponseMagnitudeDb.size()) ? deco.frequencyResponseMagnitudeDb[i] : -100.0;
        a.phaseRad[i] = (i < deco.phaseResponseRad.size()) ? deco.phaseResponseRad[i] : 0.0;
        double gdSamples = (i < deco.groupDelaySamples.size()) ? deco.groupDelaySamples[i] : 0.0;
        a.groupDelayMs[i] = (sampleRate > 0.0) ? (gdSamples / sampleRate) * 1000.0 : 0.0;
    }

    return a;
}

} // namespace abdaudiolab::math::analytics
