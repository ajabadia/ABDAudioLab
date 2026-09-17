/**
 * @file MeasurementSvgGenerator.h
 * @brief Standalone vector SVG chart generator for response measurements.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "MeasurementContracts.h"
#include <string>
#include <vector>
#include <optional>

namespace abdaudiolab::measurement
{

/**
 * @class MeasurementSvgGenerator
 * @brief Renders inline SVG charts for temporal envelope and filter frequency responses.
 */
class MeasurementSvgGenerator
{
public:
    /**
     * @brief Renders inline vector SVG line chart for temporal envelope response (time ms vs amplitude dBFS).
     * 
     * @param timeMs Vector of time stamps in ms.
     * @param amplitudeDbfs Vector of amplitude values in dBFS.
     * @param width SVG width in pixels.
     * @param height SVG height in pixels.
     * @return std::string Inline SVG string.
     */
    static std::string generateTemporalCurveSvg(const std::vector<double>& timeMs,
                                                const std::vector<double>& amplitudeDbfs,
                                                int width = 760,
                                                int height = 280);

    /**
     * @brief Renders inline vector SVG chart for frequency response (log frequency Hz vs magnitude dB).
     * 
     * @param frequenciesHz Vector of frequency points in Hz.
     * @param magnitudesDb Vector of magnitude values in dB.
     * @param slopeFit Optional metadata describing the asymptotic slope fit region and R^2.
     * @param cutoffHz Optional cutoff frequency marker in Hz.
     * @param width SVG width in pixels.
     * @param height SVG height in pixels.
     * @return std::string Inline SVG string.
     */
    static std::string generateFilterCurveSvg(const std::vector<double>& frequenciesHz,
                                              const std::vector<double>& magnitudesDb,
                                              const std::optional<SlopeFitMetadata>& slopeFit = std::nullopt,
                                              double cutoffHz = -1.0,
                                              int width = 760,
                                              int height = 280);
};

} // namespace abdaudiolab::measurement
