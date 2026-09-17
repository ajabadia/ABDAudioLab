/**
 * @file test_FilterViewerPanelAndUI.cpp
 * @brief Catch2 unit tests for Filter Measurement UI, Frequency Curve, and Interactive Defenses (Phase 20.10.2 - T4).
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "gui/measurement/MeasurementViewModel.h"
#include "gui/measurement/MeasurementViewModelLoader.h"
#include "gui/measurement/MeasurementFrequencyCurveComponent.h"
#include "gui/measurement/MeasurementViewerPanel.h"
#include "measurement/MeasurementContainerExporter.h"
#include "measurement/MeasurementContracts.h"
#include <cmath>

using namespace abdaudiolab::gui::measurement;
using namespace abdaudiolab::measurement;

namespace
{

std::vector<float> makeSineBuffer(double sr, double freq, double dur, float amp = 0.5f)
{
    size_t count = static_cast<size_t>(std::lround(sr * dur));
    std::vector<float> b(count);
    double inc = 2.0 * 3.141592653589793 * freq / sr;
    double p = 0.0;
    for (size_t i = 0; i < count; ++i)
    {
        b[i] = static_cast<float>(std::sin(p) * amp);
        p += inc;
    }
    return b;
}

} // namespace

TEST_CASE("MeasurementFrequencyCurveComponent - Vector Rendering and Cutoff Rules", "[measurement][filter][ui]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    MeasurementFrequencyCurveComponent comp;
    comp.setBounds(0, 0, 760, 280);

    MeasurementCurve curve;
    curve.x = { 20.0, 100.0, 1000.0, 10000.0, 20000.0 };
    curve.y = { 0.0, -0.2, -3.0, -20.0, -40.0 };

    SECTION("Observable cutoff sets marker correctly")
    {
        SlopeFitMetadata sf;
        sf.frequencyStartHz = 1400.0;
        sf.frequencyEndHz = 20000.0;
        sf.rSquared = 0.998;
        sf.sampleCount = 80;

        comp.setCurve(curve, sf, 1000.0, true, true);
        CHECK(comp.isCutoffObservable() == true);
        CHECK(comp.getCutoffHz() == 1000.0);
    }

    SECTION("Pass-through or unobservable cutoff hides marker")
    {
        comp.setCurve(curve, std::nullopt, -1.0, false, true);
        CHECK(comp.isCutoffObservable() == false);
    }
}

TEST_CASE("MeasurementViewerPanel - Filter UI Presentation and Domain Rules", "[measurement][filter][viewer]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    MeasurementViewerPanel panel;
    panel.setBounds(0, 0, 900, 650);

    SECTION("Direct Transfer Function displays filter cards, slope fit and fc")
    {
        MeasurementViewModel vm;
        vm.measurementId = "meas-ui-filter-direct";
        vm.measurementType = "filter";
        vm.filterTopology = "lowPass";
        vm.measurementDomain = "directTransferFunction";
        vm.measurementStatus = MeasurementStatus::completed;
        vm.statusText = "COMPLETED";
        vm.statusIcon = "[OK]";
        vm.sampleRateHz = 48000.0;
        vm.blockSize = 512;

        vm.metrics.push_back({ "cutoffFrequency", 1000.0, "Hz", "observed", "detected_at_gpass_minus_3db" });
        vm.metrics.push_back({ "asymptoticSlope", -12.0, "dB/oct", "observed", "octave_fit" });
        vm.metrics.push_back({ "qFactor", 0.707, "dim", "observed", "two_crossings" });
        vm.metrics.push_back({ "resonance", 0.0, "dB", "observed", "flat_response" });

        SlopeFitMetadata sf;
        sf.frequencyStartHz = 1400.0;
        sf.frequencyEndHz = 18000.0;
        sf.rSquared = 0.9982;
        sf.sampleCount = 100;
        vm.slopeFit = sf;

        vm.curve.x = { 20.0, 1000.0, 20000.0 };
        vm.curve.y = { 0.0, -3.0, -36.0 };

        panel.setViewModel(vm);

        const auto& loadedVm = panel.getViewModel();
        CHECK(loadedVm.filterTopology == "lowPass");
        CHECK(loadedVm.measurementDomain == "directTransferFunction");
        CHECK(loadedVm.statusText == "COMPLETED");
        CHECK(loadedVm.statusIcon == "[OK]");
        REQUIRE(loadedVm.slopeFit.has_value());
        CHECK(loadedVm.slopeFit->rSquared >= 0.99);
    }

    SECTION("Synthesized Spectral Response (MIDI) hides isolated filter transfer cards")
    {
        MeasurementViewModel vm;
        vm.measurementId = "meas-ui-filter-midi";
        vm.measurementType = "filter";
        vm.filterTopology = "lowPass";
        vm.measurementDomain = "synthesizedSpectralResponse";
        vm.measurementStatus = MeasurementStatus::completed;
        vm.statusText = "COMPLETED";
        vm.statusIcon = "[OK]";

        // Composite proxy metrics
        vm.metrics.push_back({ "observedSpectralPeak", 500.0, "Hz", "observed", "composite_output" });
        vm.metrics.push_back({ "observedSpectralRolloff", -18.0, "dB/oct", "observed", "composite_slope" });
        // Direct transfer metrics marked not observable:
        vm.metrics.push_back({ "cutoffFrequency", 0.0, "Hz", "not_observable", "midi_composite_response_only" });
        vm.metrics.push_back({ "asymptoticSlope", 0.0, "dB/oct", "not_observable", "midi_composite_response_only" });

        panel.setViewModel(vm);

        const auto& loadedVm = panel.getViewModel();
        CHECK(loadedVm.measurementDomain == "synthesizedSpectralResponse");
        CHECK(loadedVm.statusText == "COMPLETED");
    }

    SECTION("Low R-squared slope fit displays as unreliable")
    {
        MeasurementViewModel vm;
        vm.measurementType = "filter";
        vm.filterTopology = "highPass";
        vm.measurementDomain = "directTransferFunction";
        vm.measurementStatus = MeasurementStatus::unreliable;
        vm.statusText = "UNRELIABLE";
        vm.statusIcon = "[!]";

        vm.metrics.push_back({ "asymptoticSlope", -8.5, "dB/oct", "unreliable", "low_r_squared" });

        SlopeFitMetadata sf;
        sf.frequencyStartHz = 20.0;
        sf.frequencyEndHz = 300.0;
        sf.rSquared = 0.72; // Low R^2 < 0.90
        sf.sampleCount = 20;
        vm.slopeFit = sf;

        panel.setViewModel(vm);

        const auto& loadedVm = panel.getViewModel();
        REQUIRE(loadedVm.slopeFit.has_value());
        CHECK(loadedVm.slopeFit->rSquared < 0.90);
        CHECK(loadedVm.statusText == "UNRELIABLE");
    }

    SECTION("Unobservable Q factor displays not_observable")
    {
        MeasurementViewModel vm;
        vm.measurementType = "filter";
        vm.filterTopology = "lowPass";
        vm.measurementDomain = "directTransferFunction";

        vm.metrics.push_back({ "qFactor", 0.0, "dim", "not_observable", "no_two_crossings_detected" });

        panel.setViewModel(vm);

        const auto& loadedVm = panel.getViewModel();
        auto itQ = std::find_if(loadedVm.metrics.begin(), loadedVm.metrics.end(),
            [](const MeasurementMetric& m) { return m.name == "qFactor"; });
        REQUIRE(itQ != loadedVm.metrics.end());
        CHECK(itQ->status == "not_observable");
    }
}

TEST_CASE("MeasurementViewerPanel - End-to-End Container Load, Audio Tracks, and On-Demand Tamper Defense", "[measurement][filter][e2e]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("ABDAudioLab_T4_ViewerE2E_" + juce::String(juce::Random::getSystemRandom().nextInt()));
    tempDir.createDirectory();

    const double sampleRate = 48000.0;
    const double duration = 0.05;

    MeasurementSpec spec;
    spec.measurementId = "meas-ui-e2e-filter";
    spec.measurementType = "filter";
    spec.filterTopology = "lowPass";
    spec.measurementDomain = "directTransferFunction";
    spec.execution.sampleRateHz = sampleRate;
    spec.execution.blockSize = 512;

    MeasurementResult res;
    res.measurementId = spec.measurementId;
    res.measurementType = "filter";
    res.filterTopology = "lowPass";
    res.measurementDomain = "directTransferFunction";
    res.status = MeasurementStatus::completed;
    res.metrics.push_back({ "cutoffFrequency", 1200.0, "Hz", "observed" });
    res.curve.xName = "frequency";
    res.curve.xUnit = "Hz";
    res.curve.yName = "magnitude";
    res.curve.yUnit = "dB";
    res.curve.x = { 100.0, 1200.0, 10000.0 };
    res.curve.y = { 0.0, -3.0, -18.0 };

    FilterExportArtifacts artifacts;
    artifacts.sampleRateHz = sampleRate;
    artifacts.stimulusAudio = makeSineBuffer(sampleRate, 100.0, duration, 0.5f);
    artifacts.capturedAudio = makeSineBuffer(sampleRate, 100.0, duration, 0.4f);
    artifacts.impulseResponse = makeSineBuffer(sampleRate, 1000.0, 0.01, 0.8f);

    juce::File containerDir = tempDir.getChildFile("container");
    juce::String err;
    REQUIRE(MeasurementContainerExporter::exportFilterMeasurement(containerDir, spec, res, artifacts, err));

    MeasurementViewerPanel panel;
    panel.setBounds(0, 0, 900, 650);

    juce::String loadErr;
    INFO("loadContainer error: " << loadErr.toStdString());
    REQUIRE(panel.loadContainer(containerDir, loadErr));

    const auto& vm = panel.getViewModel();
    CHECK(vm.integrityStatus == UiIntegrityStatus::Verified);
    CHECK(vm.isPlaybackAllowed() == true);
    CHECK(vm.isStimulusPlaybackAllowed() == true);
    CHECK(vm.isImpulseResponsePlaybackAllowed() == true);

    // Track switching check
    panel.selectAudioTrack(0); // Output
    panel.selectAudioTrack(1); // Stimulus
    panel.selectAudioTrack(2); // IR
    panel.selectAudioTrack(0); // Back to Output

    // Tampering test: modify audio_captured.wav
    juce::File capWav = containerDir.getChildFile("audio/audio_captured.wav");
    auto altered = makeSineBuffer(sampleRate, 999.0, duration * 2);
    REQUIRE(MeasurementContainerExporter::writeWavFile(capWav, altered, sampleRate));

    // Reload container
    juce::String reloadErr;
    bool reloaded = panel.loadContainer(containerDir, reloadErr);
    REQUIRE(reloaded); // Container opens, but integrity verification flags corruption

    const auto& tamperedVm = panel.getViewModel();
    CHECK(tamperedVm.integrityStatus == UiIntegrityStatus::Corrupt);
    CHECK(tamperedVm.statusText == "CORRUPT");
    CHECK(tamperedVm.statusIcon == "[X]");
    CHECK(tamperedVm.isPlaybackAllowed() == false);

    tempDir.deleteRecursively();
}
