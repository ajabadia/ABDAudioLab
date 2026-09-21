#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../gui/controllers/DiagnosticsTelemetryPoller.h"
#include <vector>
#include <limits>

using namespace abdaudiolab::gui;

namespace {

class MockTelemetryHost : public IDiagnosticsTelemetryHost
{
public:
    int snapshotCount { 0 };
    TelemetrySnapshot lastSnapshot;

    void applyTelemetrySnapshot(const TelemetrySnapshot& snapshot) override
    {
        snapshotCount++;
        lastSnapshot = snapshot;
    }
};

class MutableTelemetrySource : public IDiagnosticsTelemetrySource
{
public:
    TelemetryAudioLevels levels;
    bool spectrumIsReady { false };
    std::vector<float> spectrumData;
    TelemetryDeviceMetrics deviceMetrics;
    TelemetryCalibrationData calibrationData;
    TelemetrySessionProgress sessionProgress;

    TelemetryAudioLevels readAudioLevels() const override { return levels; }
    bool isSpectrumReady() const override { return spectrumIsReady; }

    std::size_t readSpectrumMagnitudes(float* destBuffer, std::size_t maxBins) const override
    {
        if (!destBuffer) return 0;
        std::size_t count = std::min(maxBins, spectrumData.size());
        for (std::size_t i = 0; i < count; ++i)
            destBuffer[i] = spectrumData[i];
        return spectrumData.size(); // returns total available
    }

    TelemetryDeviceMetrics readDeviceMetrics() const override { return deviceMetrics; }
    TelemetryCalibrationData readCalibrationData() const override { return calibrationData; }
    TelemetrySessionProgress readSessionProgress() const override { return sessionProgress; }
};

} // namespace

TEST_CASE("DiagnosticsTelemetryPoller Characterization Suite", "[DiagnosticsTelemetryPoller]")
{
    MockTelemetryHost host;
    MutableTelemetrySource source;

    SECTION("1. Pico maximo y RMS se copian correctamente")
    {
        source.levels.inPeakL  = 0.75f;
        source.levels.inPeakR  = 0.85f;
        source.levels.inRmsL   = 0.40f;
        source.levels.inRmsR   = 0.42f;
        source.levels.outPeakL = 0.90f;
        source.levels.outPeakR = 0.95f;
        source.levels.outRmsL  = 0.50f;
        source.levels.outRmsR  = 0.55f;

        DiagnosticsTelemetryPoller poller(source, host);
        poller.pollNow();

        REQUIRE(host.snapshotCount == 1);
        REQUIRE(host.lastSnapshot.inputPeakL == Catch::Approx(0.75f));
        REQUIRE(host.lastSnapshot.inputPeakR == Catch::Approx(0.85f));
        REQUIRE(host.lastSnapshot.inputRmsL  == Catch::Approx(0.40f));
        REQUIRE(host.lastSnapshot.inputRmsR  == Catch::Approx(0.42f));
        REQUIRE(host.lastSnapshot.outputPeakL == Catch::Approx(0.90f));
        REQUIRE(host.lastSnapshot.outputPeakR == Catch::Approx(0.95f));
        REQUIRE(host.lastSnapshot.outputRmsL  == Catch::Approx(0.50f));
        REQUIRE(host.lastSnapshot.outputRmsR  == Catch::Approx(0.55f));
    }

    SECTION("2. FFT conserva cantidad y orden de magnitudes")
    {
        source.spectrumIsReady = true;
        source.spectrumData = { 0.1f, 0.25f, 0.5f, 0.75f, 1.0f };

        DiagnosticsTelemetryPoller poller(source, host);
        poller.pollNow();

        REQUIRE(host.lastSnapshot.spectrumReady);
        REQUIRE(host.lastSnapshot.fftBinCount == 5);
        REQUIRE(host.lastSnapshot.fftMagnitudes[0] == Catch::Approx(0.1f));
        REQUIRE(host.lastSnapshot.fftMagnitudes[1] == Catch::Approx(0.25f));
        REQUIRE(host.lastSnapshot.fftMagnitudes[2] == Catch::Approx(0.5f));
        REQUIRE(host.lastSnapshot.fftMagnitudes[3] == Catch::Approx(0.75f));
        REQUIRE(host.lastSnapshot.fftMagnitudes[4] == Catch::Approx(1.0f));
    }

    SECTION("3. FFT vacia o no lista no produce error")
    {
        source.spectrumIsReady = false;
        source.spectrumData.clear();

        DiagnosticsTelemetryPoller poller(source, host);
        poller.pollNow();

        REQUIRE_FALSE(host.lastSnapshot.spectrumReady);
        REQUIRE(host.lastSnapshot.fftBinCount == 0);
    }

    SECTION("4. Formateo exhaustivo de notas MIDI")
    {
        DiagnosticsTelemetryPoller poller(source, host);

        // 60 -> C4
        source.sessionProgress.activeMidiNoteNumber = 60;
        poller.pollNow();
        REQUIRE(host.lastSnapshot.activeMidiNoteNumber == 60);
        REQUIRE(host.lastSnapshot.activeMidiNoteName == "C4");
        REQUIRE(host.lastSnapshot.stimulusDescription == "MIDI C4 (Note #60)");

        // 61 -> C#4
        source.sessionProgress.activeMidiNoteNumber = 61;
        poller.pollNow();
        REQUIRE(host.lastSnapshot.activeMidiNoteNumber == 61);
        REQUIRE(host.lastSnapshot.activeMidiNoteName == "C#4");
        REQUIRE(host.lastSnapshot.stimulusDescription == "MIDI C#4 (Note #61)");

        // 0 -> C-1
        source.sessionProgress.activeMidiNoteNumber = 0;
        poller.pollNow();
        REQUIRE(host.lastSnapshot.activeMidiNoteNumber == 0);
        REQUIRE(host.lastSnapshot.activeMidiNoteName == "C-1");
        REQUIRE(host.lastSnapshot.stimulusDescription == "MIDI C-1 (Note #0)");

        // 127 -> G9
        source.sessionProgress.activeMidiNoteNumber = 127;
        poller.pollNow();
        REQUIRE(host.lastSnapshot.activeMidiNoteNumber == 127);
        REQUIRE(host.lastSnapshot.activeMidiNoteName == "G9");
        REQUIRE(host.lastSnapshot.stimulusDescription == "MIDI G9 (Note #127)");

        // -1 -> "No MIDI note"
        source.sessionProgress.activeMidiNoteNumber = -1;
        poller.pollNow();
        REQUIRE(host.lastSnapshot.activeMidiNoteNumber == -1);
        REQUIRE(host.lastSnapshot.activeMidiNoteName == "No MIDI note");
        REQUIRE(host.lastSnapshot.stimulusDescription == "");

        // Out of bounds: -5 -> fallback seguro
        source.sessionProgress.activeMidiNoteNumber = -5;
        poller.pollNow();
        REQUIRE(host.lastSnapshot.activeMidiNoteNumber == -1);
        REQUIRE(host.lastSnapshot.activeMidiNoteName == "No MIDI note");

        // Out of bounds: 128 -> fallback seguro
        source.sessionProgress.activeMidiNoteNumber = 128;
        poller.pollNow();
        REQUIRE(host.lastSnapshot.activeMidiNoteNumber == -1);
        REQUIRE(host.lastSnapshot.activeMidiNoteName == "No MIDI note");
    }

    SECTION("5. CPU, sample rate y block size se propagan fielmente")
    {
        source.deviceMetrics.cpuUsagePercent = 23.5f;
        source.deviceMetrics.sampleRate = 96000.0;
        source.deviceMetrics.bufferSizeSamples = 256;

        DiagnosticsTelemetryPoller poller(source, host);
        poller.pollNow();

        REQUIRE(host.lastSnapshot.cpuUsagePercent == Catch::Approx(23.5f));
        REQUIRE(host.lastSnapshot.sampleRate == Catch::Approx(96000.0));
        REQUIRE(host.lastSnapshot.bufferSizeSamples == 256);
    }

    SECTION("6. Estado de calibracion y cadencia configurable")
    {
        TelemetryPollerConfig config;
        config.calibrationPeriodTicks = 3; // periodo reducido para test determinista

        source.calibrationData.isCalibrated = true;
        source.calibrationData.sampleRate = 48000.0;
        source.calibrationData.isSkipped = false;

        DiagnosticsTelemetryPoller poller(source, host, config);

        // Tick 0: isDue (0 % 3 == 0) -> lee calibracion
        poller.pollNow();
        REQUIRE(host.lastSnapshot.calibrationTickDue);
        REQUIRE(host.lastSnapshot.isCalibrated);
        REQUIRE(host.lastSnapshot.calibrationSampleRate == Catch::Approx(48000.0));

        // Tick 1: no due
        source.calibrationData.isCalibrated = false;
        poller.pollNow();
        REQUIRE_FALSE(host.lastSnapshot.calibrationTickDue);

        // Tick 2: no due
        poller.pollNow();
        REQUIRE_FALSE(host.lastSnapshot.calibrationTickDue);

        // Tick 3: isDue (3 % 3 == 0) -> actualiza
        poller.pollNow();
        REQUIRE(host.lastSnapshot.calibrationTickDue);
        REQUIRE_FALSE(host.lastSnapshot.isCalibrated);
    }

    SECTION("7. Progreso de sesion y limites")
    {
        DiagnosticsTelemetryPoller poller(source, host);

        // Caso normal: 5 / 10 = 50%
        source.sessionProgress.currentTrial = 5;
        source.sessionProgress.totalTrials = 10;
        poller.pollNow();
        REQUIRE(host.lastSnapshot.progressPercent == Catch::Approx(50.0f));

        // 0 totalTrials -> 0% sin division por cero
        source.sessionProgress.currentTrial = 0;
        source.sessionProgress.totalTrials = 0;
        poller.pollNow();
        REQUIRE(host.lastSnapshot.progressPercent == Catch::Approx(0.0f));

        // 100%
        source.sessionProgress.currentTrial = 10;
        source.sessionProgress.totalTrials = 10;
        poller.pollNow();
        REQUIRE(host.lastSnapshot.progressPercent == Catch::Approx(100.0f));

        // currentTrial negativo se satura a 0
        source.sessionProgress.currentTrial = -3;
        source.sessionProgress.totalTrials = 10;
        poller.pollNow();
        REQUIRE(host.lastSnapshot.currentTrial == 0);
        REQUIRE(host.lastSnapshot.progressPercent == Catch::Approx(0.0f));

        // currentTrial > totalTrials se clampa al 100%
        source.sessionProgress.currentTrial = 15;
        source.sessionProgress.totalTrials = 10;
        poller.pollNow();
        REQUIRE(host.lastSnapshot.progressPercent == Catch::Approx(100.0f));
    }

    SECTION("8. Snapshot determinista ante misma fuente")
    {
        source.levels.inPeakL = 0.5f;
        source.deviceMetrics.cpuUsagePercent = 12.0f;
        source.sessionProgress.activeMidiNoteNumber = 60;

        DiagnosticsTelemetryPoller poller(source, host);
        poller.pollNow();
        auto snap1 = host.lastSnapshot;

        poller.pollNow();
        auto snap2 = host.lastSnapshot;

        REQUIRE(snap1.inputPeakL == snap2.inputPeakL);
        REQUIRE(snap1.cpuUsagePercent == snap2.cpuUsagePercent);
        REQUIRE(snap1.activeMidiNoteNumber == snap2.activeMidiNoteNumber);
        REQUIRE(snap1.activeMidiNoteName == snap2.activeMidiNoteName);
    }

    SECTION("9. Proteccion contra NaN e Inf")
    {
        float nanVal = std::numeric_limits<float>::quiet_NaN();
        float infVal = std::numeric_limits<float>::infinity();

        source.levels.inPeakL = nanVal;
        source.levels.outRmsR = infVal;
        source.deviceMetrics.cpuUsagePercent = nanVal;
        source.deviceMetrics.sampleRate = infVal;
        source.sessionProgress.lastPluginOutputRmsDb = nanVal;

        DiagnosticsTelemetryPoller poller(source, host);
        poller.pollNow();

        REQUIRE(std::isfinite(host.lastSnapshot.inputPeakL));
        REQUIRE(host.lastSnapshot.inputPeakL == Catch::Approx(0.0f));

        REQUIRE(std::isfinite(host.lastSnapshot.outputRmsR));
        REQUIRE(host.lastSnapshot.outputRmsR == Catch::Approx(0.0f));

        REQUIRE(std::isfinite(host.lastSnapshot.cpuUsagePercent));
        REQUIRE(host.lastSnapshot.cpuUsagePercent == Catch::Approx(0.0f));

        REQUIRE(std::isfinite(host.lastSnapshot.sampleRate));
        REQUIRE(host.lastSnapshot.sampleRate == Catch::Approx(0.0));

        REQUIRE(std::isfinite(host.lastSnapshot.lastPluginOutputRmsDb));
        REQUIRE(host.lastSnapshot.lastPluginOutputRmsDb == Catch::Approx(-120.0f));
    }

    SECTION("10. fftBinCount mayor a 1024 se recorta de forma determinista")
    {
        source.spectrumIsReady = true;
        source.spectrumData.resize(1200, 0.42f);

        DiagnosticsTelemetryPoller poller(source, host);
        poller.pollNow();

        REQUIRE(host.lastSnapshot.spectrumReady);
        REQUIRE(host.lastSnapshot.fftBinCount == 1024);
        REQUIRE(host.lastSnapshot.fftMagnitudes[1023] == Catch::Approx(0.42f));
    }

    SECTION("11. pollNow no inicia ni detiene el Timer y entrega 1 snapshot exacto")
    {
        DiagnosticsTelemetryPoller poller(source, host);
        REQUIRE_FALSE(poller.isPolling());

        poller.pollNow();
        REQUIRE(host.snapshotCount == 1);
        REQUIRE_FALSE(poller.isPolling());

        poller.pollNow();
        REQUIRE(host.snapshotCount == 2);
        REQUIRE_FALSE(poller.isPolling());
    }

    SECTION("12. NullDiagnosticsTelemetrySource funciona con seguridad")
    {
        NullDiagnosticsTelemetrySource nullSource;
        DiagnosticsTelemetryPoller poller(nullSource, host);

        REQUIRE_NOTHROW(poller.pollNow());
        REQUIRE(host.snapshotCount == 1);
        REQUIRE(host.lastSnapshot.inputPeakL == Catch::Approx(0.0f));
        REQUIRE_FALSE(host.lastSnapshot.spectrumReady);
        REQUIRE(host.lastSnapshot.activeMidiNoteNumber == -1);
        REQUIRE(host.lastSnapshot.activeMidiNoteName == "No MIDI note");
    }

    SECTION("13. PERF-01: timer de telemetria configurado a 25 Hz por defecto")
    {
        DiagnosticsTelemetryPoller poller(source, host);
        poller.startPolling();
        REQUIRE(poller.isPolling());
        REQUIRE(poller.getTimerInterval() == 1000 / 25); // 40 ms
        poller.stopPolling();
        REQUIRE_FALSE(poller.isPolling());
    }
}
