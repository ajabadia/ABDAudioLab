#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cmath>
#include "CalibrationRecord.h"

namespace juce
{
    class AudioIODevice;
    class String;
}

namespace abdaudiolab::calibration
{

/**
 * @brief Current observable snapshot of the audio interface configuration.
 * Captures only measurable and observable stream parameters.
 */
struct CurrentAudioConfigurationSnapshot
{
    std::string deviceName;
    std::string driverType { "Unknown" };
    double sampleRate { 0.0 };
    int bufferSizeSamples { 0 };
    int inputChannelIndex { -1 };
    std::string inputChannelLabel;
    int outputChannelIndex { -1 };
    std::string outputChannelLabel;

    [[nodiscard]] bool isValid() const noexcept
    {
        return !deviceName.empty() && sampleRate > 1000.0 && bufferSizeSamples > 0 &&
               inputChannelIndex >= 0 && outputChannelIndex >= 0;
    }

    /**
     * @brief Captures a snapshot from an active JUCE AudioIODevice and selected channels.
     */
    static CurrentAudioConfigurationSnapshot captureFrom(
        juce::AudioIODevice* device,
        int inChannelIdx, const juce::String& inLabel,
        int outChannelIdx, const juce::String& outLabel);
};

/**
 * @brief Observational compatibility status between saved calibration and current audio configuration.
 */
enum class CalibrationMatchStatus
{
    ConfigurationMatch,       // Coinciden todos los campos observables: nombre, driver, SR, buffer, routing
    ConfigurationMismatch,    // Mismo dispositivo/driver, pero difiere SR, buffer o routing
    DeviceOrDriverMismatch,   // Difiere nombre de dispositivo o tipo de driver
    NoActiveDevice,           // No hay dispositivo de audio activo
    InvalidOrCorruptProfile   // Perfil corrupto, incompleto o schema no soportado
};

/**
 * @brief Runtime alignment status of calibration in RAM.
 */
enum class ActiveCalibrationAlignment
{
    None,              // Ninguna calibración cargada o activa
    AlignedAndActive,  // Calibración activa y alineada con la configuración actual; trim aplicado
    Misaligned,        // Configuración desalineada (cambió SR/buffer/etc.); trim neutralizado
    Bypassed           // El usuario eligió expresamente continuar sin calibrar; trim neutralizado
};

/**
 * @brief Diagnostic field difference entry.
 */
struct CalibrationMatchDifference
{
    std::string fieldName;    // e.g. "Sample Rate", "Buffer Size", "Input Channel", etc.
    std::string profileValue; // e.g. "48000 Hz"
    std::string currentValue; // e.g. "96000 Hz"
};

/**
 * @brief Complete structured evaluation result.
 */
struct CalibrationMatchEvaluation
{
    CalibrationMatchStatus status { CalibrationMatchStatus::NoActiveDevice };
    bool isActionableMatch { false };
    std::string summaryMessage;
    std::string warningMessage;
    std::vector<CalibrationMatchDifference> differences;
};

/**
 * @brief Pure observational comparator service for calibration records.
 * Completely decoupled from file system, UI, and audio callbacks.
 */
class CalibrationMatchEvaluator
{
public:
    /**
     * @brief Evaluates an existing CalibrationRecord against current audio configuration snapshot.
     */
    [[nodiscard]] static CalibrationMatchEvaluation evaluate(
        const CalibrationRecord& record,
        const CurrentAudioConfigurationSnapshot& current);

    /**
     * @brief Finds the best matching profile from a candidate list.
     * Evaluates all candidates and filters those yielding ConfigurationMatch.
     * If multiple match, picks the most recent by createdAt.
     * If none match, returns std::nullopt and populates outEvaluation with
     * the evaluation of the most recent candidate (if any) to explain why it didn't match.
     */
    [[nodiscard]] static std::optional<CalibrationRecord> findBestMatchingProfile(
        const std::vector<CalibrationRecord>& candidates,
        const CurrentAudioConfigurationSnapshot& current,
        CalibrationMatchEvaluation* outEvaluation = nullptr);

    /**
     * @brief Returns true if an active calibration in RAM is still aligned with current configuration.
     */
    [[nodiscard]] static bool isStillAligned(
        const CalibrationRecord& activeRecord,
        const CurrentAudioConfigurationSnapshot& current);

    /**
     * @brief Returns the mandatory human-readable warning on physical/analog limits.
     */
    [[nodiscard]] static std::string getMandatoryAnalogWarning();
};

} // namespace abdaudiolab::calibration
