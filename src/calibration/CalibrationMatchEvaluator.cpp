#include "CalibrationMatchEvaluator.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <algorithm>
#include <sstream>

namespace abdaudiolab::calibration
{

CurrentAudioConfigurationSnapshot CurrentAudioConfigurationSnapshot::captureFrom(
    juce::AudioIODevice* device,
    int inChannelIdx, const juce::String& inLabel,
    int outChannelIdx, const juce::String& outLabel)
{
    CurrentAudioConfigurationSnapshot snap;
    if (device == nullptr)
        return snap;

    snap.deviceName = device->getName().toStdString();
    snap.driverType = device->getTypeName().toStdString();
    snap.sampleRate = device->getCurrentSampleRate();
    snap.bufferSizeSamples = device->getCurrentBufferSizeSamples();
    snap.inputChannelIndex = inChannelIdx;
    snap.inputChannelLabel = inLabel.toStdString();
    snap.outputChannelIndex = outChannelIdx;
    snap.outputChannelLabel = outLabel.toStdString();

    return snap;
}

std::string CalibrationMatchEvaluator::getMandatoryAnalogWarning()
{
    return "La interfaz y la configuración actual coinciden con los datos guardados.\n"
           "No se pueden detectar cambios físicos en cables, ganancia analógica o una segunda unidad idéntica.";
}

CalibrationMatchEvaluation CalibrationMatchEvaluator::evaluate(
    const CalibrationRecord& record,
    const CurrentAudioConfigurationSnapshot& current)
{
    CalibrationMatchEvaluation eval;

    // 1. Verificación de interfaz activa
    if (current.deviceName.empty() || current.sampleRate <= 0.0 || current.bufferSizeSamples <= 0)
    {
        eval.status = CalibrationMatchStatus::NoActiveDevice;
        eval.isActionableMatch = false;
        eval.summaryMessage = "Sin interfaz de audio disponible.";
        return eval;
    }

    // 2. Verificación de integridad del registro guardado
    if (record.schemaVersion != 1 || !record.calibrationResult.isCalibrated || record.calibrationResult.clippingDetected)
    {
        eval.status = CalibrationMatchStatus::InvalidOrCorruptProfile;
        eval.isActionableMatch = false;
        eval.summaryMessage = "Perfil de calibración no disponible (corrupto o no válido).";
        return eval;
    }

    // 3. Verificación de identidad de interfaz y driver
    if (record.deviceSnapshot.deviceName != current.deviceName ||
        record.deviceSnapshot.driverType != current.driverType)
    {
        eval.status = CalibrationMatchStatus::DeviceOrDriverMismatch;
        eval.isActionableMatch = false;
        eval.summaryMessage = "Esta calibración pertenece a otra interfaz o driver.";

        if (record.deviceSnapshot.deviceName != current.deviceName)
        {
            eval.differences.push_back({
                "Dispositivo",
                record.deviceSnapshot.deviceName,
                current.deviceName
            });
        }

        if (record.deviceSnapshot.driverType != current.driverType)
        {
            eval.differences.push_back({
                "Tipo de Driver",
                record.deviceSnapshot.driverType,
                current.driverType
            });
        }

        return eval;
    }

    // 4. Verificación de parámetros medibles del stream y routing
    if (std::abs(record.deviceSnapshot.sampleRate - current.sampleRate) > 0.1)
    {
        std::ostringstream profSr, curSr;
        profSr << static_cast<long long>(record.deviceSnapshot.sampleRate) << " Hz";
        curSr << static_cast<long long>(current.sampleRate) << " Hz";
        eval.differences.push_back({ "Frecuencia de muestreo", profSr.str(), curSr.str() });
    }

    if (record.deviceSnapshot.bufferSizeSamples != current.bufferSizeSamples)
    {
        eval.differences.push_back({
            "Tamaño de buffer",
            std::to_string(record.deviceSnapshot.bufferSizeSamples) + " muestras",
            std::to_string(current.bufferSizeSamples) + " muestras"
        });
    }

    if (record.routingSnapshot.inputChannelIndex != current.inputChannelIndex)
    {
        eval.differences.push_back({
            "Canal de entrada (índice)",
            "Canal " + std::to_string(record.routingSnapshot.inputChannelIndex + 1),
            "Canal " + std::to_string(current.inputChannelIndex + 1)
        });
    }

    if (!record.routingSnapshot.inputChannelLabel.empty() && !current.inputChannelLabel.empty() &&
        record.routingSnapshot.inputChannelLabel != current.inputChannelLabel)
    {
        eval.differences.push_back({
            "Canal de entrada (etiqueta)",
            record.routingSnapshot.inputChannelLabel,
            current.inputChannelLabel
        });
    }

    if (record.routingSnapshot.outputChannelIndex != current.outputChannelIndex)
    {
        eval.differences.push_back({
            "Canal de salida (índice)",
            "Canal " + std::to_string(record.routingSnapshot.outputChannelIndex + 1),
            "Canal " + std::to_string(current.outputChannelIndex + 1)
        });
    }

    if (!record.routingSnapshot.outputChannelLabel.empty() && !current.outputChannelLabel.empty() &&
        record.routingSnapshot.outputChannelLabel != current.outputChannelLabel)
    {
        eval.differences.push_back({
            "Canal de salida (etiqueta)",
            record.routingSnapshot.outputChannelLabel,
            current.outputChannelLabel
        });
    }

    // 5. Dictamen final
    if (!eval.differences.empty())
    {
        eval.status = CalibrationMatchStatus::ConfigurationMismatch;
        eval.isActionableMatch = false;
        eval.summaryMessage = "La calibración guardada no coincide con la configuración actual.";
    }
    else
    {
        eval.status = CalibrationMatchStatus::ConfigurationMatch;
        eval.isActionableMatch = true;
        eval.summaryMessage = "Se encontró una calibración con la misma configuración de audio.";
        eval.warningMessage = getMandatoryAnalogWarning();
    }

    return eval;
}

std::optional<CalibrationRecord> CalibrationMatchEvaluator::findBestMatchingProfile(
    const std::vector<CalibrationRecord>& candidates,
    const CurrentAudioConfigurationSnapshot& current,
    CalibrationMatchEvaluation* outEvaluation)
{
    if (candidates.empty())
    {
        if (outEvaluation != nullptr)
        {
            outEvaluation->status = CalibrationMatchStatus::NoActiveDevice;
            outEvaluation->isActionableMatch = false;
            outEvaluation->summaryMessage = "No hay perfiles de calibración guardados.";
        }
        return std::nullopt;
    }

    // Evaluar todos los candidatos y buscar aquellos con ConfigurationMatch
    std::vector<std::pair<CalibrationRecord, CalibrationMatchEvaluation>> matches;

    for (const auto& rec : candidates)
    {
        auto eval = evaluate(rec, current);
        if (eval.status == CalibrationMatchStatus::ConfigurationMatch)
        {
            matches.emplace_back(rec, std::move(eval));
        }
    }

    if (!matches.empty())
    {
        // Ordenar por fecha de creación descendente (el más reciente primero)
        std::sort(matches.begin(), matches.end(), [](const auto& a, const auto& b) {
            return a.first.createdAt > b.first.createdAt;
        });

        if (outEvaluation != nullptr)
            *outEvaluation = matches.front().second;

        return matches.front().first;
    }

    // Si ninguno coincide, evaluar el más reciente para explicar por qué no coincide
    if (outEvaluation != nullptr)
    {
        *outEvaluation = evaluate(candidates.front(), current);
    }

    return std::nullopt;
}

bool CalibrationMatchEvaluator::isStillAligned(
    const CalibrationRecord& activeRecord,
    const CurrentAudioConfigurationSnapshot& current)
{
    auto eval = evaluate(activeRecord, current);
    return eval.status == CalibrationMatchStatus::ConfigurationMatch;
}

} // namespace abdaudiolab::calibration
