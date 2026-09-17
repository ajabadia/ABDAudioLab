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

    /**
     * @brief Renders inline vector SVG chart for MIDI dynamics level response (velocity 0..127 vs dBFS).
     */
    static std::string generateDynamicsLevelSvg(const std::vector<double>& velocities,
                                                const std::vector<double>& levelsDb,
                                                const std::optional<CurveFitMetadata>& fit = std::nullopt,
                                                const DiscontinuityObservation& discontinuity = {},
                                                int width = 760,
                                                int height = 280);

    /**
     * @brief Renders inline vector SVG chart for MIDI dynamics timbre response (velocity 0..127 vs Hz).
     */
    static std::string generateDynamicsTimbreSvg(const std::vector<double>& velocities,
                                                 const std::vector<double>& centroidHz,
                                                 const std::vector<double>& rolloffHz = {},
                                                 const std::optional<CurveFitMetadata>& fit = std::nullopt,
                                                 int width = 760,
                                                 int height = 280);

    /**
     * @brief Renders inline vector SVG chart for demodulated modulation time trajectory.
     */
    static std::string generateModulationTimeSvg(const std::vector<double>& timeMs,
                                                 const std::vector<double>& values,
                                                 const std::string& yUnit = "dBFS",
                                                 const std::string& waveformShape = "sine",
                                                 int width = 760,
                                                 int height = 280);

    /**
     * @brief Renders inline vector SVG chart for modulation spectrum and observed sidebands.
     */
    static std::string generateModulationSpectrumSvg(const std::vector<double>& freqHz,
                                                     const std::vector<double>& magDb,
                                                     const std::vector<ModulationSideband>& sidebands = {},
                                                     double carrierHz = 0.0,
                                                     int width = 760,
                                                     int height = 280);

    /**
     * @brief Declarative series definition for multi-series SVG rendering.
     */
    struct SvgSeries
    {
        std::string id;
        std::string label;
        std::vector<std::pair<double, double>> points;
        int lineStyle { 0 };       /**< 0: Solid, 1: Dashed (6,3), 2: Dot-Dash (8,3,2,3), 3: Dotted (2,2) */
        int markerStyle { 0 };     /**< 0: Circle, 1: Rectangle, 2: Triangle, 3: Diamond */
        std::string strokeColor;   /**< Optional explicit hex color (e.g. "#00d4ff"). If empty, palette is used. */
        std::string unit;
    };

    /**
     * @brief Explicit plot specifications for multi-series SVG canvas.
     */
    struct SvgPlotSpec
    {
        std::string xLabel;
        std::string yLabel;
        std::string title;
        double xMin { 0.0 };
        double xMax { 127.0 };
        double yMin { -96.0 };
        double yMax { 0.0 };
        int width { 800 };
        int height { 320 };
        std::string yUnit { "dB" };
        std::vector<double> xTicks;
        std::vector<double> yTicks;
    };

    /**
     * @brief Unifying declarative SVG renderer for multi-series comparisons (DRY).
     *
     * Centralizes axis, grid lines, ticks, markers, strokes, and legend layout with zero business logic.
     * Guaranteed exact visual parity with Phase 20.11.1 / 20.11.2 reports.
     */
    static std::string generateMultiSeriesSvg(const std::vector<SvgSeries>& series,
                                              const SvgPlotSpec& spec = {});
};

} // namespace abdaudiolab::measurement
