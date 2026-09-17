/**
 * @file MeasurementSvgGenerator.cpp
 * @brief Implementation of MeasurementSvgGenerator.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementSvgGenerator.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

namespace abdaudiolab::measurement
{

std::string MeasurementSvgGenerator::generateTemporalCurveSvg(const std::vector<double>& timeMs,
                                                             const std::vector<double>& amplitudeDbfs,
                                                             int width,
                                                             int height)
{
    if (timeMs.empty() || amplitudeDbfs.empty() || timeMs.size() != amplitudeDbfs.size())
    {
        std::ostringstream ss;
        ss << "<svg width=\"" << width << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height
           << "\" xmlns=\"http://www.w3.org/2000/svg\">\n"
           << "  <rect width=\"100%\" height=\"100%\" fill=\"#0f172a\" rx=\"6\"/>\n"
           << "  <text x=\"" << (width / 2) << "\" y=\"" << (height / 2)
           << "\" fill=\"#64748b\" font-family=\"sans-serif\" font-size=\"12\" text-anchor=\"middle\">No curve data available</text>\n"
           << "</svg>\n";
        return ss.str();
    }

    double minTime = timeMs.front();
    double maxTime = timeMs.back();
    if (std::abs(maxTime - minTime) < 1e-4)
        maxTime = minTime + 100.0;

    const double minDb = -96.0;
    const double maxDb = 0.0;

    const float padLeft = 60.0f;
    const float padRight = 20.0f;
    const float padTop = 20.0f;
    const float padBottom = 35.0f;

    float plotW = static_cast<float>(width) - padLeft - padRight;
    float plotH = static_cast<float>(height) - padTop - padBottom;

    std::ostringstream ss;
    ss << "<svg width=\"" << width << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height
       << "\" xmlns=\"http://www.w3.org/2000/svg\">\n"
       << "  <defs>\n"
       << "    <linearGradient id=\"envGrad\" x1=\"0%\" y1=\"0%\" x2=\"0%\" y2=\"100%\">\n"
       << "      <stop offset=\"0%\" stop-color=\"#0284c7\" stop-opacity=\"0.35\"/>\n"
       << "      <stop offset=\"100%\" stop-color=\"#0284c7\" stop-opacity=\"0.0\"/>\n"
       << "    </linearGradient>\n"
       << "  </defs>\n"
       << "  <rect width=\"100%\" height=\"100%\" fill=\"#0f172a\" rx=\"6\"/>\n";

    // Grid lines (dBFS)
    double dbSteps[] = { 0.0, -20.0, -40.0, -60.0, -80.0, -96.0 };
    for (double db : dbSteps)
    {
        float y = padTop + static_cast<float>((maxDb - db) / (maxDb - minDb)) * plotH;
        ss << "  <line x1=\"" << padLeft << "\" y1=\"" << y << "\" x2=\"" << (padLeft + plotW) << "\" y2=\"" << y
           << "\" stroke=\"#1e293b\" stroke-width=\"1\"/>\n";
        ss << "  <text x=\"" << (padLeft - 8) << "\" y=\"" << (y + 4)
           << "\" fill=\"#64748b\" font-family=\"sans-serif\" font-size=\"10\" text-anchor=\"end\">"
           << static_cast<int>(db) << " dB</text>\n";
    }

    // Grid lines (time ms)
    int numTimeGrid = 5;
    for (int i = 0; i <= numTimeGrid; ++i)
    {
        double t = minTime + (maxTime - minTime) * (static_cast<double>(i) / numTimeGrid);
        float x = padLeft + static_cast<float>(static_cast<double>(i) / numTimeGrid) * plotW;
        ss << "  <line x1=\"" << x << "\" y1=\"" << padTop << "\" x2=\"" << x << "\" y2=\"" << (padTop + plotH)
           << "\" stroke=\"#1e293b\" stroke-width=\"1\" stroke-dasharray=\"3,3\"/>\n";
        ss << "  <text x=\"" << x << "\" y=\"" << (padTop + plotH + 18)
           << "\" fill=\"#64748b\" font-family=\"sans-serif\" font-size=\"10\" text-anchor=\"middle\">"
           << static_cast<int>(std::round(t)) << " ms</text>\n";
    }

    // Build curve path
    std::ostringstream pathD;
    std::ostringstream areaD;

    for (size_t i = 0; i < timeMs.size(); ++i)
    {
        double t = std::clamp(timeMs[i], minTime, maxTime);
        double db = std::clamp(amplitudeDbfs[i], minDb, maxDb);

        float px = padLeft + static_cast<float>((t - minTime) / (maxTime - minTime)) * plotW;
        float py = padTop + static_cast<float>((maxDb - db) / (maxDb - minDb)) * plotH;

        if (i == 0)
        {
            pathD << "M " << px << " " << py;
            areaD << "M " << px << " " << (padTop + plotH) << " L " << px << " " << py;
        }
        else
        {
            pathD << " L " << px << " " << py;
            areaD << " L " << px << " " << py;
        }
    }

    if (!timeMs.empty())
    {
        double lastT = std::clamp(timeMs.back(), minTime, maxTime);
        float lastPx = padLeft + static_cast<float>((lastT - minTime) / (maxTime - minTime)) * plotW;
        areaD << " L " << lastPx << " " << (padTop + plotH) << " Z";

        ss << "  <path d=\"" << areaD.str() << "\" fill=\"url(#envGrad)\"/>\n";
        ss << "  <path d=\"" << pathD.str() << "\" fill=\"none\" stroke=\"#38bdf8\" stroke-width=\"2\" stroke-linejoin=\"round\"/>\n";
    }

    ss << "</svg>\n";
    return ss.str();
}

std::string MeasurementSvgGenerator::generateFilterCurveSvg(const std::vector<double>& frequenciesHz,
                                                           const std::vector<double>& magnitudesDb,
                                                           const std::optional<SlopeFitMetadata>& slopeFit,
                                                           double cutoffHz,
                                                           int width,
                                                           int height)
{
    if (frequenciesHz.empty() || magnitudesDb.empty() || frequenciesHz.size() != magnitudesDb.size())
    {
        std::ostringstream ss;
        ss << "<svg width=\"" << width << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height
           << "\" xmlns=\"http://www.w3.org/2000/svg\">\n"
           << "  <rect width=\"100%\" height=\"100%\" fill=\"#0f172a\" rx=\"6\"/>\n"
           << "  <text x=\"" << (width / 2) << "\" y=\"" << (height / 2)
           << "\" fill=\"#64748b\" font-family=\"sans-serif\" font-size=\"12\" text-anchor=\"middle\">No frequency curve data available</text>\n"
           << "</svg>\n";
        return ss.str();
    }

    const double minFreq = 20.0;
    const double maxFreq = 20000.0;
    const double logMinF = std::log10(minFreq);
    const double logMaxF = std::log10(maxFreq);

    const double minDb = -72.0;
    const double maxDb = 12.0;

    const float padLeft = 60.0f;
    const float padRight = 30.0f;
    const float padTop = 25.0f;
    const float padBottom = 35.0f;

    float plotW = static_cast<float>(width) - padLeft - padRight;
    float plotH = static_cast<float>(height) - padTop - padBottom;

    auto freqToX = [&](double f) -> float {
        double clampedF = std::clamp(f, minFreq, maxFreq);
        double norm = (std::log10(clampedF) - logMinF) / (logMaxF - logMinF);
        return padLeft + static_cast<float>(norm) * plotW;
    };

    auto dbToY = [&](double db) -> float {
        double clampedDb = std::clamp(db, minDb, maxDb);
        double norm = (maxDb - clampedDb) / (maxDb - minDb);
        return padTop + static_cast<float>(norm) * plotH;
    };

    std::ostringstream ss;
    ss << "<svg width=\"" << width << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height
       << "\" xmlns=\"http://www.w3.org/2000/svg\">\n"
       << "  <defs>\n"
       << "    <linearGradient id=\"filterGrad\" x1=\"0%\" y1=\"0%\" x2=\"0%\" y2=\"100%\">\n"
       << "      <stop offset=\"0%\" stop-color=\"#38bdf8\" stop-opacity=\"0.35\"/>\n"
       << "      <stop offset=\"100%\" stop-color=\"#38bdf8\" stop-opacity=\"0.0\"/>\n"
       << "    </linearGradient>\n"
       << "  </defs>\n"
       << "  <rect width=\"100%\" height=\"100%\" fill=\"#0f172a\" rx=\"6\"/>\n";

    // Horizontal grid lines (dB)
    double dbSteps[] = { 12.0, 6.0, 0.0, -6.0, -12.0, -24.0, -36.0, -48.0, -60.0, -72.0 };
    for (double db : dbSteps)
    {
        float y = dbToY(db);
        std::string strokeColor = (std::abs(db) < 1e-4) ? "#475569" : "#1e293b";
        float strokeW = (std::abs(db) < 1e-4) ? 1.5f : 1.0f;
        ss << "  <line x1=\"" << padLeft << "\" y1=\"" << y << "\" x2=\"" << (padLeft + plotW) << "\" y2=\"" << y
           << "\" stroke=\"" << strokeColor << "\" stroke-width=\"" << strokeW << "\"/>\n";
        ss << "  <text x=\"" << (padLeft - 8) << "\" y=\"" << (y + 4)
           << "\" fill=\"#64748b\" font-family=\"sans-serif\" font-size=\"10\" text-anchor=\"end\">"
           << static_cast<int>(db) << " dB</text>\n";
    }

    // Vertical grid lines (Log Frequencies)
    double fSteps[] = { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 };
    for (double f : fSteps)
    {
        float x = freqToX(f);
        ss << "  <line x1=\"" << x << "\" y1=\"" << padTop << "\" x2=\"" << x << "\" y2=\"" << (padTop + plotH)
           << "\" stroke=\"#1e293b\" stroke-width=\"1\" stroke-dasharray=\"2,2\"/>\n";
        std::string label = (f >= 1000.0) ? (std::to_string(static_cast<int>(f / 1000.0)) + "k") : std::to_string(static_cast<int>(f));
        ss << "  <text x=\"" << x << "\" y=\"" << (padTop + plotH + 18)
           << "\" fill=\"#64748b\" font-family=\"sans-serif\" font-size=\"10\" text-anchor=\"middle\">"
           << label << "</text>\n";
    }

    // Build curve path
    std::ostringstream pathD;
    std::ostringstream areaD;

    bool first = true;
    float baselineY = dbToY(minDb);
    float lastX = padLeft;

    for (size_t i = 0; i < frequenciesHz.size(); ++i)
    {
        double f = frequenciesHz[i];
        if (f < minFreq || f > maxFreq)
            continue;

        float x = freqToX(f);
        float y = dbToY(magnitudesDb[i]);

        if (first)
        {
            pathD << "M " << x << " " << y;
            areaD << "M " << x << " " << baselineY << " L " << x << " " << y;
            first = false;
        }
        else
        {
            pathD << " L " << x << " " << y;
            areaD << " L " << x << " " << y;
        }
        lastX = x;
    }

    if (!first)
    {
        areaD << " L " << lastX << " " << baselineY << " Z";
        ss << "  <path d=\"" << areaD.str() << "\" fill=\"url(#filterGrad)\"/>\n";
        ss << "  <path d=\"" << pathD.str() << "\" fill=\"none\" stroke=\"#38bdf8\" stroke-width=\"2\"/>\n";
    }

    // Cutoff frequency marker
    if (cutoffHz >= minFreq && cutoffHz <= maxFreq)
    {
        float cx = freqToX(cutoffHz);
        ss << "  <line x1=\"" << cx << "\" y1=\"" << padTop << "\" x2=\"" << cx << "\" y2=\"" << (padTop + plotH)
           << "\" stroke=\"#f59e0b\" stroke-width=\"1.5\" stroke-dasharray=\"4,3\"/>\n";
        ss << "  <text x=\"" << (cx + 4) << "\" y=\"" << (padTop + 14)
           << "\" fill=\"#f59e0b\" font-family=\"sans-serif\" font-size=\"10\" font-weight=\"600\">fc: "
           << static_cast<int>(std::round(cutoffHz)) << " Hz</text>\n";
    }

    // Slope fit line
    if (slopeFit.has_value() && slopeFit->sampleCount >= 2 && slopeFit->frequencyStartHz < slopeFit->frequencyEndHz)
    {
        float sx1 = freqToX(slopeFit->frequencyStartHz);
        float sx2 = freqToX(slopeFit->frequencyEndHz);
        ss << "  <line x1=\"" << sx1 << "\" y1=\"" << (padTop + plotH - 3) << "\" x2=\"" << sx2 << "\" y2=\"" << (padTop + plotH - 3)
           << "\" stroke=\"#a855f7\" stroke-width=\"3\" stroke-linecap=\"round\"/>\n";
        ss << "  <text x=\"" << ((sx1 + sx2) * 0.5f) << "\" y=\"" << (padTop + plotH - 8)
           << "\" fill=\"#a855f7\" font-family=\"sans-serif\" font-size=\"9\" font-weight=\"600\" text-anchor=\"middle\">Fit R²="
           << std::fixed << std::setprecision(3) << slopeFit->rSquared << "</text>\n";
    }

    ss << "</svg>\n";
    return ss.str();
}

} // namespace abdaudiolab::measurement
