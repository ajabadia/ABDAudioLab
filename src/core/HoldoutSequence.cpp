/**
 * @file HoldoutSequence.cpp
 * @brief Implementation of formal HoldoutSequence.
 * @author ABDSynths
 * @date 2026
 */

#include "HoldoutSequence.h"
#include "../synth/Sha256.h"
#include <cmath>
#include <numbers>
#include <algorithm>

namespace abdaudiolab::core
{

AcousticModelParameters HoldoutSequence::getParametersAtTime(double timeSeconds) const noexcept
{
    if (trajectory.empty())
        return { 0.5f, 0.5f };

    if (timeSeconds <= trajectory.front().timeSeconds)
        return { trajectory.front().param1, trajectory.front().param2 };

    if (timeSeconds >= trajectory.back().timeSeconds)
        return { trajectory.back().param1, trajectory.back().param2 };

    // Find bounding trajectory interval
    for (size_t i = 0; i < trajectory.size() - 1; ++i)
    {
        const auto& p0 = trajectory[i];
        const auto& p1 = trajectory[i + 1];
        if (timeSeconds >= p0.timeSeconds && timeSeconds <= p1.timeSeconds)
        {
            double dt = p1.timeSeconds - p0.timeSeconds;
            if (dt <= 1e-9)
                return { p0.param1, p0.param2 };

            float alpha = static_cast<float>((timeSeconds - p0.timeSeconds) / dt);
            float p1Interp = p0.param1 + alpha * (p1.param1 - p0.param1);
            float p2Interp = p0.param2 + alpha * (p1.param2 - p0.param2);
            return { p1Interp, p2Interp };
        }
    }

    return { trajectory.back().param1, trajectory.back().param2 };
}

void HoldoutSequence::getStimulusAtTime(double timeSeconds, float& outFreqHz, float& outGain) const noexcept
{
    if (trajectory.empty())
    {
        outFreqHz = 440.0f;
        outGain = 0.5f;
        return;
    }

    if (timeSeconds <= trajectory.front().timeSeconds)
    {
        outFreqHz = trajectory.front().stimulusFreqHz;
        outGain = trajectory.front().stimulusGain;
        return;
    }

    if (timeSeconds >= trajectory.back().timeSeconds)
    {
        outFreqHz = trajectory.back().stimulusFreqHz;
        outGain = trajectory.back().stimulusGain;
        return;
    }

    for (size_t i = 0; i < trajectory.size() - 1; ++i)
    {
        const auto& p0 = trajectory[i];
        const auto& p1 = trajectory[i + 1];
        if (timeSeconds >= p0.timeSeconds && timeSeconds <= p1.timeSeconds)
        {
            double dt = p1.timeSeconds - p0.timeSeconds;
            if (dt <= 1e-9)
            {
                outFreqHz = p0.stimulusFreqHz;
                outGain = p0.stimulusGain;
                return;
            }

            float alpha = static_cast<float>((timeSeconds - p0.timeSeconds) / dt);
            outFreqHz = p0.stimulusFreqHz + alpha * (p1.stimulusFreqHz - p0.stimulusFreqHz);
            outGain = p0.stimulusGain + alpha * (p1.stimulusGain - p0.stimulusGain);
            return;
        }
    }

    outFreqHz = trajectory.back().stimulusFreqHz;
    outGain = trajectory.back().stimulusGain;
}

void HoldoutSequence::renderStimulus(float* outBuffer, int numSamples, double sr) const noexcept
{
    if (outBuffer == nullptr || numSamples <= 0 || sr <= 1000.0)
        return;

    const double twoPi = 2.0 * std::numbers::pi;
    double phase = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        double t = static_cast<double>(i) / sr;
        float freqHz = 440.0f;
        float gain = 0.5f;
        getStimulusAtTime(t, freqHz, gain);

        double phaseInc = (twoPi * freqHz) / sr;
        phase += phaseInc;
        if (phase >= twoPi)
            phase -= twoPi;

        // Band-limited 4-harmonic probe covering fundamental + upper spectral components
        float sample = static_cast<float>(
            0.60 * std::sin(phase) +
            0.25 * std::sin(2.0 * phase) +
            0.10 * std::sin(3.0 * phase) +
            0.05 * std::sin(4.0 * phase)
        ) * gain;

        outBuffer[i] = sample;
    }
}

bool HoldoutSequence::hasDataLeakage(const std::vector<std::pair<float, float>>& trainingPoints,
                                     float tolerance) const noexcept
{
    const float quantTol = std::max(tolerance, 1e-4f);

    for (const auto& holdoutCoord : unseenCoordinates)
    {
        // Domain validation: coordinates must reside within normalized [0, 1]
        if (holdoutCoord.first < 0.0f || holdoutCoord.first > 1.0f ||
            holdoutCoord.second < 0.0f || holdoutCoord.second > 1.0f)
            return true;

        for (const auto& trainCoord : trainingPoints)
        {
            // Coincidencia exacta o dentro de tolerancia de cuantización
            if (std::abs(holdoutCoord.first - trainCoord.first) < quantTol &&
                std::abs(holdoutCoord.second - trainCoord.second) < quantTol)
            {
                return true; // Coordenada idéntica/cuantizada: leakage detectado
            }

            float dist = std::hypot(holdoutCoord.first - trainCoord.first,
                                    holdoutCoord.second - trainCoord.second);
            if (dist < tolerance)
                return true; // Colisión de proximidad euclídea
        }
    }

    for (const auto& waypoint : trajectory)
    {
        for (const auto& trainCoord : trainingPoints)
        {
            if (std::abs(waypoint.param1 - trainCoord.first) < quantTol &&
                std::abs(waypoint.param2 - trainCoord.second) < quantTol)
            {
                return true; // Waypoint idéntico a punto de calibración
            }

            float dist = std::hypot(waypoint.param1 - trainCoord.first,
                                    waypoint.param2 - trainCoord.second);
            if (dist < tolerance)
                return true;
        }
    }

    return false;
}

void HoldoutSequence::updateHashes()
{
    // 1. Compute sequenceDefinitionHash from unseen coordinates and trajectory waypoints
    nlohmann::json seqJson;
    seqJson["sequenceId"] = sequenceId;
    seqJson["sampleRate"] = sampleRate;
    seqJson["totalDurationSeconds"] = totalDurationSeconds;
    seqJson["unseenCoordinates"] = unseenCoordinates;

    nlohmann::json trajArr = nlohmann::json::array();
    for (const auto& p : trajectory)
    {
        nlohmann::json item;
        item["timeSeconds"] = p.timeSeconds;
        item["param1"] = p.param1;
        item["param2"] = p.param2;
        item["stimulusFreqHz"] = p.stimulusFreqHz;
        item["stimulusGain"] = p.stimulusGain;
        trajArr.push_back(item);
    }
    seqJson["trajectory"] = trajArr;

    std::string seqDefStr = seqJson.dump();
    synth::Sha256 shaSeq;
    shaSeq.update(seqDefStr);
    sequenceDefinitionHash = shaSeq.finalHex();

    // 2. Compute holdoutPlanHash binding sequence definition and training plan
    nlohmann::json planJson;
    planJson["sequenceDefinitionHash"] = sequenceDefinitionHash;
    planJson["trainingPlanHash"] = trainingPlanHash;
    planJson["modelInputDomain"] = {
        { "param1Range", { 0.0, 1.0 } },
        { "param2Range", { 0.0, 1.0 } },
        { "sampleRate", sampleRate },
        { "totalDurationSeconds", totalDurationSeconds }
    };

    std::string planStr = planJson.dump();
    synth::Sha256 shaPlan;
    shaPlan.update(planStr);
    holdoutPlanHash = shaPlan.finalHex();
}

nlohmann::json HoldoutSequence::toJson() const
{
    nlohmann::json j;
    j["sequenceId"] = sequenceId;
    j["sampleRate"] = sampleRate;
    j["totalDurationSeconds"] = totalDurationSeconds;
    j["numChannels"] = numChannels;
    j["unseenCoordinates"] = unseenCoordinates;

    nlohmann::json trajArr = nlohmann::json::array();
    for (const auto& p : trajectory)
    {
        nlohmann::json item;
        item["timeSeconds"] = p.timeSeconds;
        item["param1"] = p.param1;
        item["param2"] = p.param2;
        item["stimulusFreqHz"] = p.stimulusFreqHz;
        item["stimulusGain"] = p.stimulusGain;
        trajArr.push_back(item);
    }
    j["trajectory"] = trajArr;
    j["trainingPlanHash"] = trainingPlanHash;
    j["holdoutPlanHash"] = holdoutPlanHash;
    j["sequenceDefinitionHash"] = sequenceDefinitionHash;
    return j;
}

HoldoutSequence HoldoutSequence::fromJson(const nlohmann::json& j)
{
    HoldoutSequence seq;
    seq.sequenceId = j.value("sequenceId", "holdout-dynamic-v1");
    seq.sampleRate = j.value("sampleRate", 48000.0);
    seq.totalDurationSeconds = j.value("totalDurationSeconds", 1.5);
    seq.numChannels = j.value("numChannels", 1);
    seq.trainingPlanHash = j.value("trainingPlanHash", "");
    seq.holdoutPlanHash = j.value("holdoutPlanHash", "");
    seq.sequenceDefinitionHash = j.value("sequenceDefinitionHash", "");

    if (j.contains("unseenCoordinates") && j["unseenCoordinates"].is_array())
    {
        for (const auto& item : j["unseenCoordinates"])
        {
            if (item.is_array() && item.size() >= 2)
                seq.unseenCoordinates.emplace_back(item[0].get<float>(), item[1].get<float>());
        }
    }

    if (j.contains("trajectory") && j["trajectory"].is_array())
    {
        for (const auto& item : j["trajectory"])
        {
            HoldoutTrajectoryPoint p;
            p.timeSeconds = item.value("timeSeconds", 0.0);
            p.param1 = item.value("param1", 0.5f);
            p.param2 = item.value("param2", 0.5f);
            p.stimulusFreqHz = item.value("stimulusFreqHz", 440.0f);
            p.stimulusGain = item.value("stimulusGain", 0.5f);
            seq.trajectory.push_back(p);
        }
    }

    return seq;
}

HoldoutSequence createCanonicalHoldoutSequence(double sampleRate,
                                               const std::string& trainingPlanHash)
{
    HoldoutSequence seq;
    seq.sequenceId = "holdout-dynamic-v1";
    seq.sampleRate = sampleRate;
    seq.totalDurationSeconds = 1.5;
    seq.numChannels = 1;
    seq.trainingPlanHash = trainingPlanHash;

    // Strict midpoints on 8x8 grid: k/7 in training are {0, 1/7, 2/7, 3/7, 4/7, 5/7, 6/7, 1}
    // Midpoints (unseen): { 0.0714, 0.2143, 0.3571, 0.5000, 0.6429, 0.7857, 0.9286 }
    seq.unseenCoordinates = {
        { 0.0714f, 0.2143f },
        { 0.2143f, 0.6429f },
        { 0.3571f, 0.0714f },
        { 0.5000f, 0.7857f },
        { 0.6429f, 0.3571f },
        { 0.7857f, 0.9286f },
        { 0.9286f, 0.5000f }
    };

    seq.trajectory = {
        { 0.00, 0.2143f, 0.3571f, 220.00f, 0.40f },
        { 0.30, 0.6429f, 0.2143f, 329.63f, 0.50f },
        { 0.60, 0.3571f, 0.7857f, 440.00f, 0.60f },
        { 0.90, 0.7857f, 0.5000f, 554.37f, 0.50f },
        { 1.20, 0.9286f, 0.6429f, 659.25f, 0.45f },
        { 1.50, 0.5000f, 0.0714f, 440.00f, 0.35f }
    };

    seq.updateHashes();
    return seq;
}

} // namespace abdaudiolab::core
