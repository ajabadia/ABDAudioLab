#include "NoiseFloorTracker.h"
#include <fstream>
#include <iomanip>
#include <numbers>

namespace abdaudiolab::math
{

NoiseFloorTracker::NoiseFloorTracker()
{
    reset();
}

void NoiseFloorTracker::reset()
{
    snapshots.clear();
    fftData.fill(0.0f);
    windowBuffer.fill(0.0f);
}

void NoiseFloorTracker::recordNoiseSnapshot(double timestampSec, const float* buffer, int numSamples, double sampleRate)
{
    if (buffer == nullptr || numSamples <= 0 || sampleRate <= 0.0)
        return;

    NoiseSnapshot snap;
    snap.timestampSec = timestampSec;

    // 1. Calculate overall RMS and Peak
    double sumSq = 0.0;
    float peakVal = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float absVal = std::abs(buffer[i]);
        if (absVal > peakVal)
            peakVal = absVal;
        sumSq += absVal * absVal;
    }

    float rmsLinear = static_cast<float>(std::sqrt(sumSq / static_cast<double>(numSamples)));
    snap.totalRmsDb = (rmsLinear > 1e-6f) ? (20.0f * std::log10(rmsLinear)) : -120.0f;
    snap.peakDb = (peakVal > 1e-6f) ? (20.0f * std::log10(peakVal)) : -120.0f;

    // 2. Perform 2048-point FFT with Hann window to extract 32 frequency bands
    int samplesToCopy = std::min(numSamples, fftSize);
    fftData.fill(0.0f);

    for (int i = 0; i < samplesToCopy; ++i)
    {
        // Apply Hann window
        float hann = static_cast<float>(0.5 * (1.0 - std::cos(2.0 * std::numbers::pi * i / static_cast<double>(fftSize))));
        fftData[static_cast<size_t>(i)] = buffer[i] * hann;
    }

    fft.performFrequencyOnlyForwardTransform(fftData.data());

    // 3. Aggregate FFT bins into 32 log-spaced frequency bands (20 Hz to 20 kHz)
    int numBins = fftSize / 2;
    double binHz = (sampleRate * 0.5) / static_cast<double>(numBins);

    double minFreq = 20.0;
    double maxFreq = std::min(20000.0, sampleRate * 0.49);
    double logRatio = std::pow(maxFreq / minFreq, 1.0 / 32.0);

    double bandStartHz = minFreq;
    for (int b = 0; b < 32; ++b)
    {
        double bandEndHz = bandStartHz * logRatio;
        int startBin = std::clamp(static_cast<int>(bandStartHz / binHz), 0, numBins - 1);
        int endBin = std::clamp(static_cast<int>(bandEndHz / binHz), startBin, numBins - 1);

        float bandEnergy = 0.0f;
        int count = 0;
        for (int k = startBin; k <= endBin; ++k)
        {
            bandEnergy += fftData[static_cast<size_t>(k)];
            count++;
        }

        float avgEnergy = (count > 0) ? (bandEnergy / static_cast<float>(count)) : 0.0f;
        float bandDb = (avgEnergy > 1e-6f) ? (20.0f * std::log10(avgEnergy)) : -120.0f;
        snap.spectralBands[static_cast<size_t>(b)] = bandDb;

        bandStartHz = bandEndHz;
    }

    snapshots.push_back(snap);
}

bool NoiseFloorTracker::exportNoiseTimelineHeader(const juce::File& outputFile, const juce::String& hardwareName) const
{
    std::ofstream out(outputFile.getFullPathName().toStdString());
    if (!out.is_open())
        return false;

    out << "// ==============================================================================\n";
    out << "// ABDAudioLab - Analogue Noise Floor & Thermal Drift Timeline\n";
    out << "// Hardware: " << hardwareName.toStdString() << "\n";
    out << "// Generated automatically by ABDAudioLab. DO NOT EDIT MANUALLY.\n";
    out << "// ==============================================================================\n\n";
    out << "#pragma once\n\n";
    out << "#include <cstddef>\n\n";
    out << "namespace abdaudiolab::models\n{\n\n";
    out << "struct NoiseSnapshotEntry\n{\n";
    out << "    float timestampSec;\n";
    out << "    float totalRmsDb;\n";
    out << "    float peakDb;\n";
    out << "    float spectralBands[32]; // 32 log-spaced bands (20Hz-20kHz)\n";
    out << "};\n\n";

    out << "static constexpr size_t k" << hardwareName.toStdString() << "NoiseSnapshotCount = "
        << snapshots.size() << ";\n\n";
    out << "alignas(16) static const NoiseSnapshotEntry " << hardwareName.toStdString() << "NoiseTimeline["
        << (snapshots.empty() ? 1 : snapshots.size()) << "] = {\n";

    if (snapshots.empty())
    {
        out << "    { 0.0f, -96.0f, -90.0f, { -100.0f } }\n";
    }
    else
    {
        for (size_t i = 0; i < snapshots.size(); ++i)
        {
            const auto& s = snapshots[i];
            out << "    { " << std::fixed << std::setprecision(2) << s.timestampSec << "f, "
                << std::setprecision(1) << s.totalRmsDb << "f, " << s.peakDb << "f, { ";
            for (size_t b = 0; b < 32; ++b)
            {
                out << std::setprecision(1) << s.spectralBands[b] << "f" << (b < 31 ? ", " : "");
            }
            out << " } }" << (i + 1 < snapshots.size() ? ",\n" : "\n");
        }
    }

    out << "};\n\n";
    out << "} // namespace abdaudiolab::models\n";
    return true;
}

} // namespace abdaudiolab::math
