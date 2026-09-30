#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <numeric>

#include "synth/ExternalPluginFixture.h"
#include "profiling/TargetProfileService.h"
#include "core/LabResourcePaths.h"

using namespace abdaudiolab::synth;
using namespace abdaudiolab::profiling;

namespace
{

juce::File resolveDexedBinary()
{
    auto envPath = juce::SystemStats::getEnvironmentVariable("DEXED_VST3_PATH", "");
    if (envPath.isNotEmpty())
    {
        juce::File f(envPath);
        if (f.exists())
            return f;
    }

    juce::File defaultWin("C:\\Program Files\\Common Files\\VST3\\Dexed.vst3");
    if (defaultWin.exists())
        return defaultWin;

    juce::File localVst3 = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("../Local/Programs/Common/VST3/Dexed.vst3");
    if (localVst3.exists())
        return localVst3;

    return {};
}

double computeSpectralCentroid(const std::vector<float>& samples, double sampleRate)
{
    if (samples.empty()) return 0.0;
    // Estimación de cruces por cero como aproximación temporal determinista robusta del centroide espectral
    int zeroCrossings = 0;
    for (size_t i = 1; i < samples.size(); ++i)
    {
        if ((samples[i - 1] < 0.0f && samples[i] >= 0.0f) ||
            (samples[i - 1] >= 0.0f && samples[i] < 0.0f))
        {
            zeroCrossings++;
        }
    }
    double durationSec = static_cast<double>(samples.size()) / sampleRate;
    if (durationSec <= 0.0) return 0.0;
    return (static_cast<double>(zeroCrossings) / (2.0 * durationSec));
}

double computeRms(const std::vector<float>& samples)
{
    if (samples.empty()) return 0.0;
    double sumSq = 0.0;
    for (float s : samples)
        sumSq += (s * s);
    return std::sqrt(sumSq / static_cast<double>(samples.size()));
}

} // namespace

TEST_CASE("HITO-10C: TargetProfile Dexed Observable Acoustic Behavior", "[target_profile][external][dexed]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::File dexedFile = resolveDexedBinary();
    if (!dexedFile.exists())
    {
        SKIP("External fixture unavailable: Dexed.vst3 not found at configured path");
    }

    TargetProfileService profileService;
    const auto profileFile = abdaudiolab::core::repoResource("profiles/targets/dexed.target.json");

    REQUIRE(profileFile.existsAsFile());
    auto profileRes = profileService.loadAndValidateProfile(profileFile);
    REQUIRE(profileRes.isSuccess());
    const auto& targetProfile = profileRes.profile;

    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    ExternalPluginFixture fixture(formatManager);
    std::string loadErr;
    const double sampleRate = 48000.0;
    const int blockSize = 512;
    bool loaded = fixture.loadPluginFromDisk(dexedFile, sampleRate, blockSize, loadErr);
    REQUIRE(loaded);

    ProcessingSpec spec{ sampleRate, blockSize, 2 };
    fixture.prepare(spec);

    // Capturar estado base inmutable para pruebas reproducibles
    std::vector<uint8_t> baseState;
    auto getRes = fixture.getState(baseState);
    REQUIRE(getRes.succeeded);
    REQUIRE(!baseState.empty());

    // Localizar parámetro de cutoff confirmado en Dexed
    const auto* cutoffMapping = targetProfile.findMappingForSemanticId("filter_cutoff");
    REQUIRE(cutoffMapping != nullptr);
    REQUIRE(std::holds_alternative<Vst3ParameterIdentifier>(cutoffMapping->technicalIdentifier));

    auto* instance = fixture.getPluginInstance();
    REQUIRE(instance != nullptr);
    auto params = instance->getParameters();

    juce::AudioProcessorParameter* cutoffParam = nullptr;
    for (int i = 0; i < params.size(); ++i)
    {
        if (params[i] != nullptr && params[i]->getName(128).equalsIgnoreCase("Cutoff"))
        {
            cutoffParam = params[i];
            break;
        }
    }
    REQUIRE(cutoffParam != nullptr);

    // Secuencia de excitación MIDI: NoteOn C4 (60), vel 0.8, gate 250ms, duración 400ms
    MidiExcitationSequence seq;
    seq.totalDurationSec = 0.40;
    seq.gateDurationSec = 0.25;
    seq.events.push_back(TimedMidiEvent{ TimedMidiType::NoteOn, 1, 60, 0.8f, 0, 0.0 });
    seq.events.push_back(TimedMidiEvent{ TimedMidiType::NoteOff, 1, 60, 0.0f, static_cast<int>(0.25 * sampleRate), 0.25 });

    SECTION("1. Variacion observable de Cutoff (0.20 vs 0.80)")
    {
        // Trial A: Cutoff bajo = 0.20
        fixture.setState(baseState);
        fixture.resetState();
        cutoffParam->setValueNotifyingHost(0.20f);

        std::vector<float> audioLow;
        fixture.render(seq, audioLow);
        REQUIRE(!audioLow.empty());

        double rmsLow = computeRms(audioLow);
        double centroidLow = computeSpectralCentroid(audioLow, sampleRate);

        // Trial B: Cutoff alto = 0.80
        fixture.setState(baseState);
        fixture.resetState();
        cutoffParam->setValueNotifyingHost(0.80f);

        std::vector<float> audioHigh;
        fixture.render(seq, audioHigh);
        REQUIRE(!audioHigh.empty());

        double rmsHigh = computeRms(audioHigh);
        double centroidHigh = computeSpectralCentroid(audioHigh, sampleRate);

        CHECK(rmsLow > 0.0);
        CHECK(rmsHigh > 0.0);
        CHECK(centroidLow >= 0.0);
        CHECK(centroidHigh >= 0.0);

        // Demostración de comportamiento acústico observable
        REQUIRE(audioLow.size() == audioHigh.size());
        double diffSumSq = 0.0;
        float maxAbsDiff = 0.0f;
        for (size_t i = 0; i < audioLow.size(); ++i)
        {
            float d = std::abs(audioHigh[i] - audioLow[i]);
            if (d > maxAbsDiff) maxAbsDiff = d;
            diffSumSq += (d * d);
        }
        double diffRms = std::sqrt(diffSumSq / static_cast<double>(audioLow.size()));

        // El audio DEBE diferir de forma medible (> 1e-3)
        CHECK(diffRms > 0.001);
        CHECK(maxAbsDiff > 0.005f);

        // Sin clipping masivo (> 1.5)
        for (float s : audioLow) CHECK(std::abs(s) <= 1.5f);
        for (float s : audioHigh) CHECK(std::abs(s) <= 1.5f);
    }

    SECTION("2. Repetibilidad estricta tras reset (ausencia de estado residual)")
    {
        // Pase 1 con Cutoff = 0.50
        fixture.setState(baseState);
        fixture.resetState();
        cutoffParam->setValueNotifyingHost(0.50f);

        std::vector<float> audioPass1;
        fixture.render(seq, audioPass1);

        // Pase 2 con idéntico reset y estado restaurado
        fixture.setState(baseState);
        fixture.resetState();
        cutoffParam->setValueNotifyingHost(0.50f);

        std::vector<float> audioPass2;
        fixture.render(seq, audioPass2);

        REQUIRE(audioPass1.size() == audioPass2.size());
        float maxPassDiff = 0.0f;
        for (size_t i = 0; i < audioPass1.size(); ++i)
        {
            float d = std::abs(audioPass1[i] - audioPass2[i]);
            if (d > maxPassDiff) maxPassDiff = d;
        }

        // La diferencia entre pases idénticos debe ser prácticamente cero (< 1e-4)
        CHECK(maxPassDiff < 1e-4f);
    }
}
