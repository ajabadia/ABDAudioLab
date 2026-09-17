/**
 * @file PhysicalLoopbackAdapter.h
 * @brief Adaptador y coordinador de medición de loopback físico puro (Fase 20.11 T4.2).
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "MeasurementContracts.h"
#include "LoopbackCalibrator.h"
#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <string>
#include <vector>
#include <memory>

namespace abdaudiolab::measurement
{

/**
 * @struct PhysicalLoopbackSpec
 * @brief Especificación formal para la medición y calibración de loopback analógico.
 */
struct PhysicalLoopbackSpec
{
    std::string deviceName { "default_audio_device" };
    double sampleRateHz { 48000.0 };
    int blockSize { 512 };
    int outputChannel { 0 };
    int inputChannel { 0 };
    double sweepDurationSec { 1.0 };
    double leadInSilenceSec { 0.05 };
    float levelDbfs { -6.0f };

    // Criterios metrológicos de aceptación
    double snrDbMin { 18.0 };
    double peakDbfsMax { -0.5 };
    double dcOffsetDbMax { -60.0 };
    double maxClockDriftPpm { 50.0 };

    [[nodiscard]] std::string toJsonString(int indent = 2) const;
    static PhysicalLoopbackSpec fromJsonString(const std::string& jsonStr);
};

/**
 * @struct LoopbackCalibrationArtifacts
 * @brief Artefactos crudos y evaluados de la sesión de calibración de loopback (Nivel loopback_reference).
 */
struct LoopbackCalibrationArtifacts
{
    std::vector<float> stimulusAudio;
    std::vector<float> responseAudio;
    LoopbackCalibrationRecord record;
    AnalogChainArtifactTier tier { AnalogChainArtifactTier::LoopbackReference };
    PhysicalLoopbackSpec spec;
};

/**
 * @class ILoopbackAudioTransport
 * @brief Interfaz abstracta para la transmisión y captura física del bucle analógico.
 */
class ILoopbackAudioTransport
{
public:
    virtual ~ILoopbackAudioTransport() = default;

    virtual bool open(const std::string& deviceName,
                      double sampleRate,
                      int blockSize,
                      int outputChannel,
                      int inputChannel,
                      std::string& error) = 0;

    virtual void close() = 0;

    virtual bool transmitAndCapture(const std::vector<float>& stimulus,
                                    std::vector<float>& response,
                                    std::string& error) = 0;
};

/**
 * @class PhysicalLoopbackCoordinator
 * @brief Orquesta la adquisición física de la señal de referencia de loopback sin DUT.
 */
class PhysicalLoopbackCoordinator
{
public:
    /**
     * @brief Ejecuta el ciclo metrológico completo de calibración:
     * validación -> estímulo -> reproducción/captura -> análisis -> emisión pass/fail.
     */
    static bool executeCalibration(ILoopbackAudioTransport* transport,
                                   const PhysicalLoopbackSpec& spec,
                                   LoopbackCalibrationArtifacts& outArtifacts,
                                   std::string& outError);
};

/**
 * @class LoopbackContainerExporter
 * @brief Empaqueta y persiste el contenedor FAIR de calibración de loopback en disco.
 */
class LoopbackContainerExporter
{
public:
    /**
     * @brief Genera specs/, audio/, results/, reports/ y manifest.json.
     */
    static bool exportLoopbackPackage(const juce::File& outputDir,
                                      const LoopbackCalibrationArtifacts& artifacts,
                                      std::string& outError);
};

} // namespace abdaudiolab::measurement
