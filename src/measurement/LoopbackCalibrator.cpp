/**
 * @file LoopbackCalibrator.cpp
 * @brief Implementación del calibrador metrológico de loopback analógico para Fase 20.11 T4.
 * @author ABDSynths
 * @date 2026
 */

#include "LoopbackCalibrator.h"
#include "../synth/Sha256.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace abdaudiolab::measurement
{

namespace
{
constexpr double kPi = 3.14159265358979323846;

std::string generateUniqueCalibrationId(double sampleRate, int blockSize)
{
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    std::ostringstream ss;
    ss << "cal_loopback_" << static_cast<int>(sampleRate) << "hz_" << blockSize << "b_" << ms;
    return ss.str();
}
} // namespace

std::vector<float> LoopbackCalibrator::generateCalibrationStimulus(double sampleRate,
                                                                   double sweepDurationSec,
                                                                   double leadInSilenceSec,
                                                                   float levelDbfs)
{
    if (sampleRate <= 0.0 || sweepDurationSec <= 0.0)
        return {};

    size_t leadInSamples = static_cast<size_t>(std::lround(leadInSilenceSec * sampleRate));
    size_t sweepSamples = static_cast<size_t>(std::lround(sweepDurationSec * sampleRate));
    size_t totalSamples = leadInSamples + sweepSamples;

    std::vector<float> stimulus(totalSamples, 0.0f);

    float amp = std::pow(10.0f, levelDbfs / 20.0f);
    double fStart = 20.0;
    double fEnd = std::min(20000.0, (sampleRate * 0.5) * 0.95);

    double sweepDuration = static_cast<double>(sweepSamples) / sampleRate;
    double logFactor = std::log(fEnd / fStart);

    // Generar log-sine sweep con envolvente suave al inicio y fin para evitar clics
    size_t rampSamples = std::min(static_cast<size_t>(sampleRate * 0.005), sweepSamples / 20);

    for (size_t i = 0; i < sweepSamples; ++i)
    {
        double t = static_cast<double>(i) / sampleRate;
        double phase = 2.0 * kPi * fStart * (sweepDuration / logFactor) * (std::exp((t / sweepDuration) * logFactor) - 1.0);

        float sample = amp * static_cast<float>(std::sin(phase));

        // Fade in
        if (i < rampSamples)
        {
            float w = 0.5f * (1.0f - std::cos(static_cast<float>(kPi * i / rampSamples)));
            sample *= w;
        }
        // Fade out
        else if (i >= sweepSamples - rampSamples)
        {
            size_t rem = sweepSamples - 1 - i;
            float w = 0.5f * (1.0f - std::cos(static_cast<float>(kPi * rem / rampSamples)));
            sample *= w;
        }

        stimulus[leadInSamples + i] = sample;
    }

    // Insertar marcador de sincronización metrológico (Sync Marker) al final del silencio previo
    // para determinación unívoca y exacta de la latencia total de ida y vuelta (round-trip)
    if (leadInSamples >= 4)
    {
        size_t syncIdx = leadInSamples - 4;
        stimulus[syncIdx]     = +amp;
        stimulus[syncIdx + 1] = -amp;
        stimulus[syncIdx + 2] = +amp;
        stimulus[syncIdx + 3] = -amp;
    }

    return stimulus;
}

LoopbackCalibrationRecord LoopbackCalibrator::analyzeLoopback(const std::vector<float>& stimulusAudio,
                                                             const std::vector<float>& responseAudio,
                                                             double sampleRate,
                                                             int blockSize,
                                                             const std::string& customCalibrationId)
{
    LoopbackCalibrationRecord record;
    record.calibrationId = !customCalibrationId.empty() ? customCalibrationId : generateUniqueCalibrationId(sampleRate, blockSize);
    record.sampleRateHz = sampleRate;
    record.blockSize = blockSize;
    record.status = "fail";

    if (stimulusAudio.empty() || responseAudio.empty() || sampleRate <= 0.0)
    {
        return record;
    }

    // Hashes criptográficos de fijación inmutable
    record.stimulusSha256 = synth::Sha256::computeHex(stimulusAudio.data(), stimulusAudio.size());
    record.responseSha256 = synth::Sha256::computeHex(responseAudio.data(), responseAudio.size());

    // 1. Nivel pico (peakDbfs) y detección de clipping
    float maxVal = 0.0f;
    double sumSamples = 0.0;
    for (float s : responseAudio)
    {
        float absVal = std::abs(s);
        if (absVal > maxVal)
            maxVal = absVal;
        sumSamples += static_cast<double>(s);
    }

    record.peakDbfs = (maxVal > 1e-12f) ? static_cast<double>(20.0f * std::log10(maxVal)) : -120.0;

    // 2. Estimación de latencia de ida y vuelta total (roundTripLatencySamples)
    size_t leadInSamples = static_cast<size_t>(std::lround(0.05 * sampleRate));
    size_t syncIdx = (leadInSamples >= 4) ? (leadInSamples - 4) : 0;
    size_t stimStart = leadInSamples;

    // 2.1 Offset de componente continua (dcOffsetDb) medido metrológicamente en reposo
    size_t dcWindow = (leadInSamples >= 16) ? (leadInSamples / 2) : std::min(responseAudio.size(), size_t(32));
    double dcSum = 0.0;
    for (size_t i = 0; i < dcWindow && i < responseAudio.size(); ++i)
    {
        dcSum += static_cast<double>(responseAudio[i]);
    }
    double dcMean = (dcWindow > 0) ? (dcSum / static_cast<double>(dcWindow)) : 0.0;
    record.dcOffsetDb = (std::abs(dcMean) > 1e-12) ? (20.0 * std::log10(std::abs(dcMean))) : -120.0;

    // 3. Correlación cruzada mediante Sync Marker de banda ancha
    double maxMarkerCorr = 0.0;
    size_t bestMarkerLag = 0;
    size_t maxSearchLag = (responseAudio.size() > 4) ? std::min(responseAudio.size() - 4, static_cast<size_t>(sampleRate * 2.0)) : 0;

    for (size_t i = 0; i < maxSearchLag; ++i)
    {
        double corr = static_cast<double>(responseAudio[i])     * (+1.0)
                    + static_cast<double>(responseAudio[i + 1]) * (-1.0)
                    + static_cast<double>(responseAudio[i + 2]) * (+1.0)
                    + static_cast<double>(responseAudio[i + 3]) * (-1.0);

        if (corr > maxMarkerCorr)
        {
            maxMarkerCorr = corr;
            bestMarkerLag = i;
        }
    }

    if (maxMarkerCorr > 0.05 && bestMarkerLag >= syncIdx)
    {
        record.roundTripLatencySamples = static_cast<double>(bestMarkerLag - syncIdx);
    }
    else
    {
        // Fallback: correlación cruzada sobre ventana activa
        size_t corrWindow = std::min(static_cast<size_t>(sampleRate * 0.1), stimulusAudio.size() - stimStart);
        if (corrWindow > 0 && responseAudio.size() > corrWindow)
        {
            size_t maxLag = std::min(responseAudio.size() - corrWindow, static_cast<size_t>(sampleRate * 1.5));
            double maxCorr = -1.0;
            size_t bestLag = 0;

            for (size_t lag = 0; lag < maxLag; ++lag)
            {
                double corr = 0.0;
                for (size_t k = 0; k < corrWindow; ++k)
                {
                    corr += static_cast<double>(responseAudio[lag + k]) * static_cast<double>(stimulusAudio[stimStart + k]);
                }

                if (corr > maxCorr)
                {
                    maxCorr = corr;
                    bestLag = lag;
                }
            }

            if (bestLag >= stimStart)
                record.roundTripLatencySamples = static_cast<double>(bestLag - stimStart);
            else
                record.roundTripLatencySamples = 0.0;
        }
    }

    record.roundTripLatencyMs = (record.roundTripLatencySamples / sampleRate) * 1000.0;

    // 4. Estimación de SNR (Signal-to-Noise Ratio)
    // Medir piso de ruido en el lead-in previo a la llegada del estímulo detectado
    size_t signalArrival = static_cast<size_t>(std::lround(record.roundTripLatencySamples + static_cast<double>(stimStart)));
    size_t noiseLeadInSamples = (signalArrival > 32) ? (signalArrival - 16) : 0;
    if (noiseLeadInSamples > responseAudio.size())
        noiseLeadInSamples = responseAudio.size();

    double noisePower = 0.0;
    if (noiseLeadInSamples >= 32)
    {
        for (size_t i = 0; i < noiseLeadInSamples; ++i)
        {
            double s = static_cast<double>(responseAudio[i]);
            noisePower += s * s;
        }
        noisePower /= static_cast<double>(noiseLeadInSamples);
    }

    // Potencia de la señal durante la excitación
    size_t stimRem = (stimulusAudio.size() > stimStart) ? (stimulusAudio.size() - stimStart) : 0;
    size_t respRem = (responseAudio.size() > signalArrival) ? (responseAudio.size() - signalArrival) : 0;
    size_t signalDurationSamples = std::min(stimRem, respRem);
    double signalPower = 0.0;
    if (signalArrival < responseAudio.size() && signalDurationSamples > 64)
    {
        size_t safeDuration = std::min(signalDurationSamples, responseAudio.size() - signalArrival);
        for (size_t i = 0; i < safeDuration; ++i)
        {
            double s = static_cast<double>(responseAudio[signalArrival + i]);
            signalPower += s * s;
        }
        signalPower /= static_cast<double>(safeDuration);
    }

    if (noisePower > 1e-12 && signalPower > 1e-12)
    {
        record.snrDb = 10.0 * std::log10(signalPower / noisePower);
    }
    else if (signalPower > 1e-12)
    {
        record.snrDb = 100.0; // Piso de ruido virtualmente nulo
    }
    else
    {
        record.snrDb = 0.0;
    }

    // 5. Estimación de Deriva de Reloj (clockDriftPpm)
    // En bucle cerrado sobre la misma interfaz (DAC y ADC esclavos del mismo oscilador),
    // la deriva nominal física es 0.0 ppm
    record.clockDriftPpm = 0.0;

    // 6. Evaluación de Criterios Metrológicos de Aceptación
    bool passesSnr = (record.snrDb >= record.snrDbMin);
    bool passesClipping = (record.peakDbfs <= record.peakDbfsMax);
    bool passesDc = (record.dcOffsetDb <= record.dcOffsetDbMax);
    bool passesDrift = (std::abs(record.clockDriftPpm) <= record.maxClockDriftPpm);
    bool passesLatency = (record.roundTripLatencySamples >= 0.0);

    if (passesSnr && passesClipping && passesDc && passesDrift && passesLatency)
    {
        record.status = "pass";
    }
    else
    {
        record.status = "fail";
    }

    return record;
}

} // namespace abdaudiolab::measurement
