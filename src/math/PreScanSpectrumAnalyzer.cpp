#include "PreScanSpectrumAnalyzer.h"

namespace abdaudiolab::math
{

PreScanResult PreScanSpectrumAnalyzer::analyzeFilterNoiseSweep (const std::vector<float>& buffer, 
                                                                double sampleRate, 
                                                                float startMidiVal, 
                                                                float endMidiVal, 
                                                                int fftOrder, 
                                                                float windowHopMs)
{
    PreScanResult result;
    const int fftSize = 1 << fftOrder;
    juce::dsp::FFT fftProcessor (fftOrder);
    
    const int totalSamples = static_cast<int> (buffer.size());
    const int hopSamples = std::max(1, static_cast<int> (sampleRate * (windowHopMs / 1000.0f)));
    
    if (totalSamples < fftSize) return result;

    // Precalcular ventana de Hann para evitar asignaciones dinámicas en el bucle
    std::vector<float> hannWindow (fftSize);
    for (int i = 0; i < fftSize; ++i)
        hannWindow[i] = 0.5f * (1.0f - std::cos (2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1)));

    std::vector<float> fftData (fftSize * 2, 0.0f);

    for (int pos = 0; pos + fftSize <= totalSamples; pos += hopSamples)
    {
        float progress = (totalSamples > fftSize) ? (static_cast<float> (pos) / static_cast<float> (totalSamples - fftSize)) : 0.0f;
        float currentMidiCtrl = startMidiVal + progress * (endMidiVal - startMidiVal);
        float currentTimeSec = static_cast<float> (pos) / static_cast<float> (sampleRate);

        // Aplicar enventanado
        for (int i = 0; i < fftSize; ++i)
            fftData[i] = buffer[static_cast<size_t> (pos + i)] * hannWindow[i];
            
        std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);

        fftProcessor.performFrequencyOnlyForwardTransform (fftData.data());

        // Peak Detection en la mitad positiva del espectro
        float maxMag = -1.0f;
        int maxBin = 0;
        const int halfFftSize = fftSize / 2;

        for (int bin = 1; bin < halfFftSize; ++bin) // Omitir DC bin 0
        {
            if (fftData[bin] > maxMag)
            {
                maxMag = fftData[bin];
                maxBin = bin;
            }
        }

        float peakHz = static_cast<float> (maxBin) * (static_cast<float> (sampleRate) / static_cast<float> (fftSize));

        PreScanPoint pt;
        pt.controlValue = currentMidiCtrl;
        pt.timeSec = currentTimeSec;
        pt.primaryMetric = peakHz;
        pt.thdPercent = 0.0f;
        
        result.trajectory.push_back (pt);
    }

    return result;
}

PreScanResult PreScanSpectrumAnalyzer::analyzeSaturationToneSweep (const std::vector<float>& buffer, 
                                                                   double sampleRate, 
                                                                   float fundamentalHz, 
                                                                   float startMidiVal, 
                                                                   float endMidiVal, 
                                                                   int fftOrder, 
                                                                   float windowHopMs)
{
    PreScanResult result;
    const int fftSize = 1 << fftOrder;
    const int totalSamples = static_cast<int> (buffer.size());
    const int hopSamples = std::max(1, static_cast<int> (sampleRate * (windowHopMs / 1000.0f)));
    
    if (totalSamples < fftSize) return result;

    // Precalcular ventana de Hann
    std::vector<float> hannWindow (fftSize);
    for (int i = 0; i < fftSize; ++i)
        hannWindow[i] = 0.5f * (1.0f - std::cos (2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1)));

    std::vector<float> windowBuffer (fftSize);

    for (int pos = 0; pos + fftSize <= totalSamples; pos += hopSamples)
    {
        float progress = (totalSamples > fftSize) ? (static_cast<float> (pos) / static_cast<float> (totalSamples - fftSize)) : 0.0f;
        float currentMidiCtrl = startMidiVal + progress * (endMidiVal - startMidiVal);
        float currentTimeSec = static_cast<float> (pos) / static_cast<float> (sampleRate);

        for (int i = 0; i < fftSize; ++i)
            windowBuffer[i] = buffer[static_cast<size_t> (pos + i)] * hannWindow[i];

        float thd = calculateInstantaneousThd (windowBuffer.data(), fftSize, sampleRate, fundamentalHz);

        PreScanPoint pt;
        pt.controlValue = currentMidiCtrl;
        pt.timeSec = currentTimeSec;
        pt.primaryMetric = thd; // En saturación la métrica principal es la distorsión
        pt.thdPercent = thd;
        
        result.trajectory.push_back (pt);
    }

    return result;
}

float PreScanSpectrumAnalyzer::calculateInstantaneousThd (const float* windowSamples, 
                                                          int fftSize, 
                                                          double sampleRate, 
                                                          float fundamentalHz)
{
    int fftOrder = static_cast<int> (std::round(std::log2 (fftSize)));
    juce::dsp::FFT fftProcessor (fftOrder);
    
    std::vector<float> fftData (fftSize * 2, 0.0f);
    std::copy (windowSamples, windowSamples + fftSize, fftData.begin());
    
    fftProcessor.performFrequencyOnlyForwardTransform (fftData.data());

    auto getMagnitudeAtHz = [&] (float hz) -> float 
    {
        float bin = hz * (static_cast<float> (fftSize) / static_cast<float> (sampleRate));
        int centerIdx = static_cast<int> (std::round (bin));
        int startIdx = std::max (1, centerIdx - 2);
        int endIdx = std::min (fftSize / 2, centerIdx + 2);
        
        float maxPeak = 0.0f;
        for (int idx = startIdx; idx <= endIdx; ++idx)
        {
            maxPeak = std::max (maxPeak, fftData[idx]);
        }
        return maxPeak;
    };

    float v1 = getMagnitudeAtHz (fundamentalHz);
    if (v1 < 0.00001f) return 0.0f; // Evitar división por cero si no hay señal

    float sumHarmonicsSq = 0.0f;
    for (int h = 2; h <= 5; ++h)
    {
        float v_h = getMagnitudeAtHz (fundamentalHz * static_cast<float> (h));
        sumHarmonicsSq += v_h * v_h;
    }

    return (std::sqrt (sumHarmonicsSq) / v1) * 100.0f;
}

} // namespace abdaudiolab::math
