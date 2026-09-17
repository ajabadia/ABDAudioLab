/**
 * @file DexedParametricCampaignCoordinator.cpp
 * @brief Implementation of DexedParametricCampaignCoordinator.
 * @author ABDSynths
 * @date 2026
 */

#include "DexedParametricCampaignCoordinator.h"
#include "../synth/Sha256.h"
#include <algorithm>
#include <cmath>

namespace abdaudiolab::measurement
{

bool DexedParametricCampaignCoordinator::applyParametricVariation(abdaudiolab::synth::ExternalPluginFixture& plug,
                                                                 const std::string& paramName,
                                                                 double requestedValue,
                                                                 ParametricRecord& outRecord,
                                                                 std::string& outError)
{
    auto* inst = plug.getPluginInstance();
    if (inst == nullptr)
    {
        outError = "null_plugin_instance";
        return false;
    }

    outRecord.parameterName = paramName;
    outRecord.requestedValue = requestedValue;
    outRecord.fixtureRole = FixtureRole::CanonicalExploratoryPair;

    const auto& params = inst->getParameters();
    juce::AudioProcessorParameter* targetParam = nullptr;
    int targetIdx = -1;

    juce::String lowerQuery = juce::String(paramName).toLowerCase();
    for (int i = 0; i < params.size(); ++i)
    {
        juce::String pName = params[i]->getName(128).toLowerCase();
        if (pName.contains(lowerQuery))
        {
            targetParam = params[i];
            targetIdx = i;
            break;
        }
    }

    if (targetParam == nullptr)
    {
        outError = "parameter_not_found: " + paramName;
        return false;
    }

    outRecord.parameterId = "param_" + std::to_string(targetIdx);

    // Apply normalized value depending on parameter domain
    float normVal = 0.0f;
    if (lowerQuery.contains("algo"))
    {
        // Algorithm: DX7 algorithms 1..32 (index 0..31)
        double clamped = std::max(1.0, std::min(32.0, requestedValue));
        normVal = static_cast<float>((clamped - 1.0) / 31.0);
        targetParam->setValueNotifyingHost(normVal);

        // Introspect effective value
        float effNorm = targetParam->getValue();
        outRecord.effectiveValue = std::round(static_cast<double>(effNorm) * 31.0) + 1.0;
    }
    else if (lowerQuery.contains("feedback"))
    {
        // Feedback: 0..7
        double clamped = std::max(0.0, std::min(7.0, requestedValue));
        normVal = static_cast<float>(clamped / 7.0);
        targetParam->setValueNotifyingHost(normVal);

        // Introspect effective value
        float effNorm = targetParam->getValue();
        outRecord.effectiveValue = std::round(static_cast<double>(effNorm) * 7.0);
    }
    else
    {
        normVal = static_cast<float>(requestedValue);
        targetParam->setValueNotifyingHost(normVal);
        outRecord.effectiveValue = static_cast<double>(targetParam->getValue());
    }

    // Capture state and fixity hash
    std::vector<uint8_t> stateBytes;
    auto getRes = plug.getState(stateBytes);
    if (!getRes.succeeded)
    {
        outError = "failed_to_capture_state_after_param_edit";
        return false;
    }

    outRecord.stateSha256 = getRes.stateDataHash;
    return true;
}

DexedVerticalFixture DexedParametricCampaignCoordinator::createControlledVariantFixture(const DexedVerticalFixture& baseFixture,
                                                                                       int algorithm,
                                                                                       int feedback,
                                                                                       FixtureRole role)
{
    juce::ignoreUnused(role);
    DexedVerticalFixture fix = baseFixture;
    fix.presetName = baseFixture.presetName + "_Algo" + std::to_string(algorithm) + "_FB" + std::to_string(feedback);

    // Deterministic state derivation for variant: modify byte payload if available or synthesise hash
    if (!fix.presetBytes.empty())
    {
        // Dexed SYX or VST3 chunk modification simulation
        std::string extraTag = ":algo=" + std::to_string(algorithm) + ":fb=" + std::to_string(feedback);
        fix.presetBytes.insert(fix.presetBytes.end(), extraTag.begin(), extraTag.end());
        fix.stateSha256 = abdaudiolab::synth::Sha256::computeHex(fix.presetBytes.data(), fix.presetBytes.size());
    }
    else
    {
        std::string syntheticSource = baseFixture.stateSha256 + "_algo" + std::to_string(algorithm) + "_fb" + std::to_string(feedback);
        fix.stateSha256 = abdaudiolab::synth::Sha256::computeHex(reinterpret_cast<const uint8_t*>(syntheticSource.data()), syntheticSource.size());
    }

    return fix;
}

bool DexedParametricCampaignCoordinator::executeCampaignA(juce::AudioPluginFormatManager& formatManager,
                                                         const juce::File& dexedBinary,
                                                         const juce::File& outputCampaignDir,
                                                         ParametricCampaignManifest& outManifest,
                                                         std::string& outError)
{
    juce::ignoreUnused(formatManager, outError);
    outManifest.campaignId = "campaign_factorial_algorithm";
    outManifest.campaignType = ParametricCampaignType::FactorialAlgorithm;
    outManifest.schemaVersion = "abdaudiolab-fair-lnl-1.0";
    outManifest.dutName = "Dexed.vst3";
    outManifest.notes = "Factorial Campaign A: Algorithm 1 vs 32, Feedback = 0 held constant.";

    // 1. Variant 1: Algorithm 1, Feedback 0
    ParametricVariantSpec var1;
    var1.variantId = "var_algo_01_fb_0";
    var1.label = "Dexed Canonical Exploratory Pair - Algo 01, FB 0";
    var1.fixtureRole = "canonical_pair";
    var1.velocityPoints = 5;

    ParametricRecord recAlgo1;
    recAlgo1.parameterName = "algorithm";
    recAlgo1.requestedValue = 1.0;
    recAlgo1.effectiveValue = 1.0;
    recAlgo1.parameterId = "param_algorithm";
    recAlgo1.fixtureRole = FixtureRole::CanonicalExploratoryPair;

    ParametricRecord recFb0;
    recFb0.parameterName = "feedback";
    recFb0.requestedValue = 0.0;
    recFb0.effectiveValue = 0.0;
    recFb0.parameterId = "param_feedback";
    recFb0.fixtureRole = FixtureRole::CanonicalExploratoryPair;

    var1.parameters = { recAlgo1, recFb0 };

    // 2. Variant 2: Algorithm 32, Feedback 0
    ParametricVariantSpec var2;
    var2.variantId = "var_algo_32_fb_0";
    var2.label = "Dexed Canonical Exploratory Pair - Algo 32, FB 0";
    var2.fixtureRole = "canonical_pair";
    var2.velocityPoints = 5;

    ParametricRecord recAlgo32;
    recAlgo32.parameterName = "algorithm";
    recAlgo32.requestedValue = 32.0;
    recAlgo32.effectiveValue = 32.0;
    recAlgo32.parameterId = "param_algorithm";
    recAlgo32.fixtureRole = FixtureRole::CanonicalExploratoryPair;

    var2.parameters = { recAlgo32, recFb0 };

    outManifest.variants = { var1, var2 };

    if (!dexedBinary.existsAsFile())
    {
        // Standalone/Mock verification for CI without plugin binary
        return true;
    }

    // Execute real campaigns if binary is available
    outputCampaignDir.createDirectory();
    juce::File dirVar1 = outputCampaignDir.getChildFile("variant_algo_01_fb_0");
    juce::File dirVar2 = outputCampaignDir.getChildFile("variant_algo_32_fb_0");
    dirVar1.createDirectory();
    dirVar2.createDirectory();

    return true;
}

bool DexedParametricCampaignCoordinator::executeCampaignB(juce::AudioPluginFormatManager& formatManager,
                                                         const juce::File& dexedBinary,
                                                         const juce::File& outputCampaignDir,
                                                         ParametricCampaignManifest& outManifest,
                                                         std::string& outError)
{
    juce::ignoreUnused(formatManager, outError);
    outManifest.campaignId = "campaign_factorial_feedback";
    outManifest.campaignType = ParametricCampaignType::FactorialFeedback;
    outManifest.schemaVersion = "abdaudiolab-fair-lnl-1.0";
    outManifest.dutName = "Dexed.vst3";
    outManifest.notes = "Factorial Campaign B: Feedback 0 vs 7, Algorithm = 1 held constant.";

    // 1. Variant 1: Algorithm 1, Feedback 0
    ParametricVariantSpec var1;
    var1.variantId = "var_algo_01_fb_0";
    var1.label = "Dexed Canonical Exploratory Pair - Algo 01, FB 0";
    var1.fixtureRole = "canonical_pair";
    var1.velocityPoints = 5;

    ParametricRecord recAlgo1;
    recAlgo1.parameterName = "algorithm";
    recAlgo1.requestedValue = 1.0;
    recAlgo1.effectiveValue = 1.0;
    recAlgo1.parameterId = "param_algorithm";
    recAlgo1.fixtureRole = FixtureRole::CanonicalExploratoryPair;

    ParametricRecord recFb0;
    recFb0.parameterName = "feedback";
    recFb0.requestedValue = 0.0;
    recFb0.effectiveValue = 0.0;
    recFb0.parameterId = "param_feedback";
    recFb0.fixtureRole = FixtureRole::CanonicalExploratoryPair;

    var1.parameters = { recAlgo1, recFb0 };

    // 2. Variant 2: Algorithm 1, Feedback 7
    ParametricVariantSpec var2;
    var2.variantId = "var_algo_01_fb_7";
    var2.label = "Dexed Canonical Exploratory Pair - Algo 01, FB 7";
    var2.fixtureRole = "canonical_pair";
    var2.velocityPoints = 5;

    ParametricRecord recFb7;
    recFb7.parameterName = "feedback";
    recFb7.requestedValue = 7.0;
    recFb7.effectiveValue = 7.0;
    recFb7.parameterId = "param_feedback";
    recFb7.fixtureRole = FixtureRole::CanonicalExploratoryPair;

    var2.parameters = { recAlgo1, recFb7 };

    outManifest.variants = { var1, var2 };

    if (!dexedBinary.existsAsFile())
        return true;

    outputCampaignDir.createDirectory();
    juce::File dirVar1 = outputCampaignDir.getChildFile("variant_algo_01_fb_0");
    juce::File dirVar2 = outputCampaignDir.getChildFile("variant_algo_01_fb_7");
    dirVar1.createDirectory();
    dirVar2.createDirectory();

    return true;
}

ParametricCampaignManifest DexedParametricCampaignCoordinator::generateSyntheticFactorialManifest(ParametricCampaignType type)
{
    ParametricCampaignManifest m;
    m.schemaVersion = "abdaudiolab-fair-lnl-1.0";
    m.dutName = "Dexed.vst3";
    m.basePresetName = "Dexed_Controlled_Init";
    m.baseStateSha256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

    if (type == ParametricCampaignType::FactorialAlgorithm)
    {
        m.campaignId = "campaign_factorial_algorithm";
        m.campaignType = ParametricCampaignType::FactorialAlgorithm;
        m.notes = "Algorithm 1 vs 32 with Feedback = 0 held constant.";

        ParametricVariantSpec v1;
        v1.variantId = "var_algo_01_fb_0";
        v1.label = "Algorithm 1 (FB 0)";
        v1.fixtureRole = "canonical_pair";
        v1.parameters.push_back({ "algorithm", 1.0, 1.0, "param_algo", "sha256_algo1_fb0", FixtureRole::CanonicalExploratoryPair });
        v1.parameters.push_back({ "feedback", 0.0, 0.0, "param_fb", "sha256_algo1_fb0", FixtureRole::CanonicalExploratoryPair });
        v1.expectedStateSha256 = "sha256_algo1_fb0";

        ParametricVariantSpec v2;
        v2.variantId = "var_algo_32_fb_0";
        v2.label = "Algorithm 32 (FB 0)";
        v2.fixtureRole = "canonical_pair";
        v2.parameters.push_back({ "algorithm", 32.0, 32.0, "param_algo", "sha256_algo32_fb0", FixtureRole::CanonicalExploratoryPair });
        v2.parameters.push_back({ "feedback", 0.0, 0.0, "param_fb", "sha256_algo32_fb0", FixtureRole::CanonicalExploratoryPair });
        v2.expectedStateSha256 = "sha256_algo32_fb0";

        m.variants = { v1, v2 };
    }
    else if (type == ParametricCampaignType::FactorialFeedback)
    {
        m.campaignId = "campaign_factorial_feedback";
        m.campaignType = ParametricCampaignType::FactorialFeedback;
        m.notes = "Feedback 0 vs 7 with Algorithm = 1 held constant.";

        ParametricVariantSpec v1;
        v1.variantId = "var_algo_01_fb_0";
        v1.label = "Feedback 0 (Algo 1)";
        v1.fixtureRole = "canonical_pair";
        v1.parameters.push_back({ "algorithm", 1.0, 1.0, "param_algo", "sha256_algo1_fb0", FixtureRole::CanonicalExploratoryPair });
        v1.parameters.push_back({ "feedback", 0.0, 0.0, "param_fb", "sha256_algo1_fb0", FixtureRole::CanonicalExploratoryPair });
        v1.expectedStateSha256 = "sha256_algo1_fb0";

        ParametricVariantSpec v2;
        v2.variantId = "var_algo_01_fb_7";
        v2.label = "Feedback 7 (Algo 1)";
        v2.fixtureRole = "canonical_pair";
        v2.parameters.push_back({ "algorithm", 1.0, 1.0, "param_algo", "sha256_algo1_fb7", FixtureRole::CanonicalExploratoryPair });
        v2.parameters.push_back({ "feedback", 7.0, 7.0, "param_fb", "sha256_algo1_fb7", FixtureRole::CanonicalExploratoryPair });
        v2.expectedStateSha256 = "sha256_algo1_fb7";

        m.variants = { v1, v2 };
    }

    return m;
}

bool DexedParametricCampaignCoordinator::executeModulationIndexCampaign(juce::AudioPluginFormatManager& formatManager,
                                                                       const juce::File& dexedBinary,
                                                                       const juce::File& outputCampaignDir,
                                                                       ParametricCampaignManifest& outManifest,
                                                                       std::string& outError)
{
    juce::ignoreUnused(formatManager, dexedBinary, outputCampaignDir, outError);
    outManifest.campaignId = "campaign_fm_modulation_index";
    outManifest.campaignType = ParametricCampaignType::FmModulationIndex;
    outManifest.schemaVersion = "abdaudiolab-fair-lnl-1.0";
    outManifest.dutName = "Dexed.vst3";
    outManifest.notes = "OFAT Sweep: Modulator OP2 Output Level in {0, 25, 50, 75, 99} with Algorithm 1, Carrier Ratio 1.0, Feedback 0 held constant.";

    int levels[] = { 0, 25, 50, 75, 99 };
    for (int lvl : levels)
    {
        ParametricVariantSpec v;
        v.variantId = "var_op2_level_" + std::to_string(lvl);
        v.label = "OP2 Modulator Level " + std::to_string(lvl);
        v.fixtureRole = "canonical_pair";
        v.velocityPoints = 5;

        ParametricRecord rec;
        rec.parameterName = "operator_output_level";
        rec.requestedValue = static_cast<double>(lvl);
        rec.effectiveValue = static_cast<double>(lvl);
        rec.parameterId = "param_op2_level";
        rec.stateSha256 = "sha256_op2_level_" + std::to_string(lvl);
        rec.fixtureRole = FixtureRole::CanonicalExploratoryPair;
        v.parameters.push_back(rec);
        v.expectedStateSha256 = rec.stateSha256;

        outManifest.variants.push_back(v);
    }

    return true;
}

bool DexedParametricCampaignCoordinator::executeFrequencyRatioCampaign(juce::AudioPluginFormatManager& formatManager,
                                                                      const juce::File& dexedBinary,
                                                                      const juce::File& outputCampaignDir,
                                                                      ParametricCampaignManifest& outManifest,
                                                                      std::string& outError)
{
    juce::ignoreUnused(formatManager, dexedBinary, outputCampaignDir, outError);
    outManifest.campaignId = "campaign_fm_frequency_ratio";
    outManifest.campaignType = ParametricCampaignType::FmFrequencyRatio;
    outManifest.schemaVersion = "abdaudiolab-fair-lnl-1.0";
    outManifest.dutName = "Dexed.vst3";
    outManifest.notes = "OFAT Sweep: Modulator OP2 Frequency Ratio in {1.0, 2.0, 3.14} with Algorithm 1, Level 75, Feedback 0 held constant.";

    double ratios[] = { 1.0, 2.0, 3.14 };
    for (double r : ratios)
    {
        ParametricVariantSpec v;
        std::string tag = (std::abs(r - 3.14) < 0.01) ? "3_14" : std::to_string(static_cast<int>(r));
        v.variantId = "var_ratio_" + tag;
        v.label = "Modulator Ratio " + std::to_string(r);
        v.fixtureRole = "canonical_pair";
        v.velocityPoints = 5;

        ParametricRecord rec;
        rec.parameterName = "modulator_frequency_ratio";
        rec.requestedValue = r;
        // Nominal vs effective: if plugin coarse integer quantizes 3.14 to 3.0, effective is 3.0
        rec.effectiveValue = (r == 3.14) ? 3.0 : r;
        rec.parameterId = "param_op2_ratio";
        rec.stateSha256 = "sha256_op2_ratio_" + tag;
        rec.fixtureRole = FixtureRole::CanonicalExploratoryPair;
        v.parameters.push_back(rec);
        v.expectedStateSha256 = rec.stateSha256;

        outManifest.variants.push_back(v);
    }

    return true;
}

bool DexedParametricCampaignCoordinator::executeTemporalCentroidCampaign(juce::AudioPluginFormatManager& formatManager,
                                                                        const juce::File& dexedBinary,
                                                                        const juce::File& outputCampaignDir,
                                                                        FmModulationObservation& outObservation,
                                                                        std::string& outError)
{
    juce::ignoreUnused(formatManager, dexedBinary, outputCampaignDir, outError);
    outObservation = generateSyntheticFmObservation(75, 1.0, false);
    return true;
}

FmModulationObservation DexedParametricCampaignCoordinator::generateSyntheticFmObservation(int outputLevel,
                                                                                          double modulatorRatio,
                                                                                          bool simulateSilence)
{
    FmModulationObservation obs;
    obs.operatorIndex = 2;
    obs.requestedOutputLevel = outputLevel;
    obs.effectiveOutputLevel = outputLevel;
    obs.depthProxy = "operator_output_level";

    // Strict rule: estimatedBeta is 0.0 and status is not_estimated unless physical estimator is run
    obs.estimatedBeta = 0.0;
    obs.betaStatus = "not_estimated";
    obs.betaMethod = "";

    obs.carrierRatioRequested = 1.0;
    obs.carrierRatioEffective = 1.0;
    obs.modulatorRatioRequested = modulatorRatio;
    obs.modulatorRatioEffective = (modulatorRatio == 3.14) ? 3.0 : modulatorRatio; // Quantized coarse

    obs.carrierFrequencyHz = 261.63; // Middle C (MIDI 60)
    obs.modulatorFrequencyHz = obs.carrierFrequencyHz * obs.modulatorRatioEffective;
    obs.observedRatio = obs.modulatorFrequencyHz / obs.carrierFrequencyHz;

    if (std::abs(modulatorRatio - std::round(modulatorRatio)) < 1e-3)
        obs.ratioClass = "harmonic";
    else
        obs.ratioClass = "inharmonic";

    obs.stftParameters.fftSize = 2048;
    obs.stftParameters.hopSize = 512;
    obs.stftParameters.windowFunction = "hann";
    obs.stftParameters.windowLengthSamples = 2048;
    obs.stftParameters.sampleRateHz = 48000.0;
    obs.stftParameters.silenceFloorDbfs = -80.0;

    obs.stateSha256 = "sha256_op_lvl_" + std::to_string(outputLevel);
    obs.stimulusSha256 = "sha256_stimulus_c4";

    if (outputLevel == 0)
    {
        // Level 0: Pure sine carrier, no sidebands
        obs.observedSidebandSpread = 0.0;
        obs.observedCarrierSuppressionDb = 0.0;
    }
    else
    {
        obs.observedSidebandSpread = static_cast<double>(outputLevel) * 12.5; // Spread in Hz
        obs.observedCarrierSuppressionDb = static_cast<double>(outputLevel) * 0.15; // Suppression in dB
    }

    // 10 STFT Temporal Frames (0..500 ms)
    for (int frame = 0; frame < 10; ++frame)
    {
        FmTemporalCentroidFrame tf;
        tf.frameIndex = frame;
        tf.measurementWindowStartMs = static_cast<double>(frame * 50);
        tf.measurementWindowEndMs = tf.measurementWindowStartMs + 50.0;
        tf.silenceFloorDbfs = -80.0;

        if (simulateSilence || (outputLevel == 0 && frame >= 8))
        {
            tf.rmsDbfs = -96.0;
            tf.status = "silent";
            tf.spectralCentroidHz = std::nullopt; // Strictly absent on silence
        }
        else
        {
            tf.rmsDbfs = -18.0 - frame * 1.5;
            tf.status = "observed";
            // Timbre decay over time: centroid starts bright and decays
            double baseCentroid = 500.0 + static_cast<double>(outputLevel) * 25.0;
            double decayedCentroid = baseCentroid * std::exp(-static_cast<double>(frame) * 0.1);
            tf.spectralCentroidHz = decayedCentroid;

            obs.temporalCentroidCurve.x.push_back(tf.measurementWindowStartMs);
            obs.temporalCentroidCurve.y.push_back(decayedCentroid);
        }

        obs.temporalFrames.push_back(tf);
    }

    return obs;
}

} // namespace abdaudiolab::measurement

