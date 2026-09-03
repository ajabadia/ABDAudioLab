/**
 * @file CertificationReportExporter.h
 * @brief Standalone publication-grade HTML/PDF certification report generator with embedded vector SVG charts.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <string>
#include <vector>
#include "LutExporter.h"

namespace abdaudiolab::exporting
{

/**
 * @class CertificationReportExporter
 * @brief Generates self-contained HTML/PDF profiling certification reports with inline SVG vector plots.
 */
class CertificationReportExporter
{
public:
    CertificationReportExporter() = default;
    ~CertificationReportExporter() = default;

    /**
     * @brief Exports session measurement data to a print-ready self-contained HTML report.
     * @param targetPath Target filesystem path for HTML document (.html).
     * @param manifest Session manifest metadata structure.
     * @param points Vector of measured data points.
     * @return true on success, false on write error.
     */
    static bool exportReportToHtml(const std::string& targetPath,
                                   const SessionManifestData& manifest,
                                   const std::vector<MeasuredPoint>& points);

    /**
     * @brief Renders an inline SVG vector line chart for log frequency magnitude response (20 Hz - 20 kHz).
     * @param freqsHz Vector of frequency bins in Hz.
     * @param magsDb Vector of magnitude values in dBFS.
     * @param width Width in SVG pixels.
     * @param height Height in SVG pixels.
     * @return std::string SVG XML string element.
     */
    static std::string generateFrequencyCurveSvg(const std::vector<float>& freqsHz,
                                                 const std::vector<float>& magsDb,
                                                 int width = 760,
                                                 int height = 280);

    /**
     * @brief Renders an inline SVG vector 2D heatmap matrix for parameter sweeps.
     * @param points Vector of measured data points.
     * @param rows Matrix row count (dimension 1).
     * @param cols Matrix column count (dimension 2).
     * @param width Width in SVG pixels.
     * @param height Height in SVG pixels.
     * @return std::string SVG XML string element.
     */
    static std::string generateHeatmapSvg(const std::vector<MeasuredPoint>& points,
                                          int rows = 8,
                                          int cols = 8,
                                          int width = 500,
                                          int height = 300);

    /**
     * @brief Generates HTML table element summarizing THD %, SNR dB, and noise metrics.
     * @param points Vector of measured data points.
     * @return std::string HTML table snippet string.
     */
    static std::string generateThdTableHtml(const std::vector<MeasuredPoint>& points);
};

} // namespace abdaudiolab::exporting
