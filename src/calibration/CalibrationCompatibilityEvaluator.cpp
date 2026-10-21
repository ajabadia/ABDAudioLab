/**
 * @file CalibrationCompatibilityEvaluator.cpp
 * @brief Implementation of CalibrationCompatibilityEvaluator.
 * @author ABDSynths
 * @date 2026
 */

#include "CalibrationCompatibilityEvaluator.h"
#include <juce_core/juce_core.h>
#include <cmath>

namespace abdaudiolab::calibration
{

namespace
{
    juce::String normalizeName(const std::string& s)
    {
        return juce::String(s).toLowerCase().removeCharacters(" :;()-_[]").trim();
    }

    bool matchNormalizedNames(const std::string& a, const std::string& b)
    {
        return normalizeName(a) == normalizeName(b);
    }
}

bool CalibrationCompatibilityEvaluator::isKnownProblematicAsioDriver(
    const std::string& deviceOrDriverName,
    std::string* reasonOut)
{
    if (deviceOrDriverName.empty())
        return false;

    const juce::String norm = normalizeName(deviceOrDriverName);

    if (norm.contains("asiodirectxfullduplex"))
    {
        if (reasonOut != nullptr)
        {
            *reasonOut = "El wrapper 'ASIO DirectX Full Duplex Driver' introduce buffers DirectSound "
                         "asimétricos no deterministas e inestabilidad de reloj full-duplex.";
        }
        return true;
    }

    if (norm.contains("novationusbasio"))
    {
        if (reasonOut != nullptr)
        {
            *reasonOut = "El controlador 'Novation USB ASIO Driver' presenta deriva de fase/latencia "
                         "en captura síncrona full-duplex de precisión.";
        }
        return true;
    }

    return false;
}

CompatibilityEvaluation CalibrationCompatibilityEvaluator::evaluate(
    const CalibrationSnapshot& snapshot,
    const CurrentAudioConfigurationSnapshot& current,
    bool strictBufferSize)
{
    CompatibilityEvaluation eval;

    // 1. Verify snapshot validity
    if (!snapshot.result.isValid() || snapshot.result.calibrationStatus != "Valid")
    {
        eval.verdict = CompatibilityVerdict::InvalidSnapshot;
        eval.isActionable = false;
        eval.summary = "El snapshot de calibración no es válido o presentó clipping/anomalías.";
        eval.mismatchDetails.push_back("Estado de calibración: " + snapshot.result.calibrationStatus);
        return eval;
    }

    if (!snapshot.integrity.snapshotHash.empty() && !snapshot.verifyIntegrity())
    {
        eval.verdict = CompatibilityVerdict::InvalidSnapshot;
        eval.isActionable = false;
        eval.summary = "Integridad del snapshot comprometida (hash mismatch).";
        eval.mismatchDetails.push_back("Hash no coincide con el contenido.");
        return eval;
    }

    // 2. Verify active audio stream
    if (!current.isValid())
    {
        eval.verdict = CompatibilityVerdict::NoActiveDevice;
        eval.isActionable = false;
        eval.summary = "No hay interfaz de audio activa configurada.";
        return eval;
    }

    // 2b. Check Known Problematic ASIO Full-Duplex Blacklist (HITO-DSP-24.6)
    std::string blacklistReason;
    if (isKnownProblematicAsioDriver(current.deviceName, &blacklistReason) ||
        isKnownProblematicAsioDriver(current.driverType, &blacklistReason))
    {
        eval.verdict = CompatibilityVerdict::BlacklistedDriver;
        eval.isActionable = false;
        eval.summary = "Controlador ASIO en lista negra por inestabilidad full-duplex conocida.";
        eval.mismatchDetails.push_back(blacklistReason);
        return eval;
    }

    // 3. Device & driver check
    bool deviceMatches = matchNormalizedNames(snapshot.compatibility.deviceStableId, current.deviceName);
    bool driverMatches = (snapshot.compatibility.driverType == "Unknown" ||
                          current.driverType == "Unknown" ||
                          matchNormalizedNames(snapshot.compatibility.driverType, current.driverType));

    if (!deviceMatches || !driverMatches)
    {
        eval.verdict = CompatibilityVerdict::DeviceOrDriverMismatch;
        eval.isActionable = false;
        eval.summary = "La calibración pertenece a otra interfaz de audio o controlador.";
        if (!deviceMatches)
        {
            eval.mismatchDetails.push_back("Dispositivo: " + snapshot.compatibility.deviceStableId +
                                           " (actual: " + current.deviceName + ")");
        }
        if (!driverMatches)
        {
            eval.mismatchDetails.push_back("Driver: " + snapshot.compatibility.driverType +
                                           " (actual: " + current.driverType + ")");
        }
        return eval;
    }

    // 4. Sample Rate check (strict, within 1.0 Hz)
    if (std::abs(snapshot.compatibility.sampleRateHz - current.sampleRate) > 1.0)
    {
        eval.verdict = CompatibilityVerdict::SampleRateMismatch;
        eval.isActionable = false;
        eval.summary = "La frecuencia de muestreo difiere de la calibración.";
        eval.mismatchDetails.push_back("Sample Rate: " +
                                       std::to_string(static_cast<int>(snapshot.compatibility.sampleRateHz)) +
                                       " Hz (actual: " +
                                       std::to_string(static_cast<int>(current.sampleRate)) + " Hz)");
        return eval;
    }

    // 5. Routing check
    if (snapshot.compatibility.inputChannelIndex != current.inputChannelIndex ||
        snapshot.compatibility.outputChannelIndex != current.outputChannelIndex)
    {
        eval.verdict = CompatibilityVerdict::RoutingMismatch;
        eval.isActionable = false;
        eval.summary = "El enrutamiento de canales (In/Out) difiere de la calibración de loopback.";
        if (snapshot.compatibility.inputChannelIndex != current.inputChannelIndex)
        {
            eval.mismatchDetails.push_back("Canal de Entrada: Ch " +
                                           std::to_string(snapshot.compatibility.inputChannelIndex + 1) +
                                           " (actual: Ch " +
                                           std::to_string(current.inputChannelIndex + 1) + ")");
        }
        if (snapshot.compatibility.outputChannelIndex != current.outputChannelIndex)
        {
            eval.mismatchDetails.push_back("Canal de Salida: Ch " +
                                           std::to_string(snapshot.compatibility.outputChannelIndex + 1) +
                                           " (actual: Ch " +
                                           std::to_string(current.outputChannelIndex + 1) + ")");
        }
        return eval;
    }

    // 6. Buffer size check
    if (snapshot.compatibility.bufferSamples != current.bufferSizeSamples)
    {
        eval.verdict = CompatibilityVerdict::BufferSizeMismatch;
        eval.isActionable = !strictBufferSize;
        eval.summary = "El tamaño de buffer difiere del calibrado. La latencia RTL puede variar.";
        eval.mismatchDetails.push_back("Buffer: " +
                                       std::to_string(snapshot.compatibility.bufferSamples) +
                                       " samples (actual: " +
                                       std::to_string(current.bufferSizeSamples) + " samples)");
        return eval;
    }

    // 7. Complete match
    eval.verdict = CompatibilityVerdict::Compatible;
    eval.isActionable = true;
    eval.summary = "Calibración 100% compatible con la configuración de hardware actual.";
    return eval;
}

} // namespace abdaudiolab::calibration
