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

// ==============================================================================
// MIDI Dynamics SVG Charts
// ==============================================================================

std::string MeasurementSvgGenerator::generateDynamicsLevelSvg(const std::vector<double>& velocities,
                                                              const std::vector<double>& levelsDb,
                                                              const std::optional<CurveFitMetadata>& fit,
                                                              const DiscontinuityObservation& discontinuity,
                                                              int width,
                                                              int height)
{
    if (velocities.empty() || levelsDb.empty() || velocities.size() != levelsDb.size())
    {
        std::ostringstream ss;
        ss << "<svg width=\"" << width << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height
           << "\" xmlns=\"http://www.w3.org/2000/svg\">\n"
           << "  <rect width=\"100%\" height=\"100%\" fill=\"#0f172a\" rx=\"6\"/>\n"
           << "  <text x=\"" << (width / 2) << "\" y=\"" << (height / 2)
           << "\" fill=\"#64748b\" font-family=\"sans-serif\" font-size=\"12\" text-anchor=\"middle\">No dynamics curve data</text>\n"
           << "</svg>\n";
        return ss.str();
    }

    const float padLeft = 60.0f;
    const float padRight = 20.0f;
    const float padTop = 25.0f;
    const float padBottom = 35.0f;

    float plotW = static_cast<float>(width) - padLeft - padRight;
    float plotH = static_cast<float>(height) - padTop - padBottom;

    const double minDb = -96.0;
    const double maxDb = 0.0;
    const double minVel = 0.0;
    const double maxVel = 127.0;

    auto velToX = [&](double v) -> float {
        return padLeft + static_cast<float>((std::clamp(v, minVel, maxVel) - minVel) / (maxVel - minVel)) * plotW;
    };
    auto dbToY = [&](double db) -> float {
        return padTop + static_cast<float>((maxDb - std::clamp(db, minDb, maxDb)) / (maxDb - minDb)) * plotH;
    };

    std::ostringstream ss;
    ss << "<svg width=\"" << width << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height
       << "\" xmlns=\"http://www.w3.org/2000/svg\">\n"
       << "  <defs>\n"
       << "    <linearGradient id=\"dynGrad\" x1=\"0%\" y1=\"0%\" x2=\"0%\" y2=\"100%\">\n"
       << "      <stop offset=\"0%\" stop-color=\"#0284c7\" stop-opacity=\"0.35\"/>\n"
       << "      <stop offset=\"100%\" stop-color=\"#0284c7\" stop-opacity=\"0.0\"/>\n"
       << "    </linearGradient>\n"
       << "  </defs>\n"
       << "  <rect width=\"100%\" height=\"100%\" fill=\"#0f172a\" rx=\"6\"/>\n";

    // Grid (dBFS)
    double dbSteps[] = { 0.0, -20.0, -40.0, -60.0, -80.0, -96.0 };
    for (double db : dbSteps)
    {
        float y = dbToY(db);
        ss << "  <line x1=\"" << padLeft << "\" y1=\"" << y << "\" x2=\"" << (padLeft + plotW) << "\" y2=\"" << y
           << "\" stroke=\"#1e293b\" stroke-width=\"1\"/>\n";
        ss << "  <text x=\"" << (padLeft - 8) << "\" y=\"" << (y + 4)
           << "\" fill=\"#64748b\" font-family=\"sans-serif\" font-size=\"10\" text-anchor=\"end\">"
           << static_cast<int>(db) << " dB</text>\n";
    }

    // Grid (Velocity)
    int velSteps[] = { 0, 32, 64, 96, 127 };
    for (int v : velSteps)
    {
        float x = velToX(static_cast<double>(v));
        ss << "  <line x1=\"" << x << "\" y1=\"" << padTop << "\" x2=\"" << x << "\" y2=\"" << (padTop + plotH)
           << "\" stroke=\"#1e293b\" stroke-width=\"1\" stroke-dasharray=\"3,3\"/>\n";
        ss << "  <text x=\"" << x << "\" y=\"" << (padTop + plotH + 18)
           << "\" fill=\"#64748b\" font-family=\"sans-serif\" font-size=\"10\" text-anchor=\"middle\">"
           << v << "</text>\n";
    }

    // Discontinuity observation highlight band
    if (discontinuity.detected && discontinuity.lowerVelocity < discontinuity.upperVelocity)
    {
        float x1 = velToX(static_cast<double>(discontinuity.lowerVelocity));
        float x2 = velToX(static_cast<double>(discontinuity.upperVelocity));
        float bandW = std::max(4.0f, x2 - x1);
        ss << "  <rect x=\"" << x1 << "\" y=\"" << padTop << "\" width=\"" << bandW << "\" height=\"" << plotH
           << "\" fill=\"#ef4444\" fill-opacity=\"0.15\" stroke=\"#ef4444\" stroke-width=\"1\" stroke-dasharray=\"3,3\"/>\n";
        ss << "  <text x=\"" << ((x1 + x2) * 0.5f) << "\" y=\"" << (padTop + 14)
           << "\" fill=\"#f87171\" font-family=\"sans-serif\" font-size=\"10\" font-weight=\"600\" text-anchor=\"middle\">Jump: "
           << std::fixed << std::setprecision(1) << discontinuity.jumpDb << " dB</text>\n";
    }

    // Build path & dots
    std::ostringstream pathD;
    std::ostringstream areaD;
    float baselineY = dbToY(minDb);

    for (size_t i = 0; i < velocities.size(); ++i)
    {
        float px = velToX(velocities[i]);
        float py = dbToY(levelsDb[i]);
        if (i == 0)
        {
            pathD << "M " << px << " " << py;
            areaD << "M " << px << " " << baselineY << " L " << px << " " << py;
        }
        else
        {
            pathD << " L " << px << " " << py;
            areaD << " L " << px << " " << py;
        }
    }
    float lastX = velToX(velocities.back());
    areaD << " L " << lastX << " " << baselineY << " Z";

    ss << "  <path d=\"" << areaD.str() << "\" fill=\"url(#dynGrad)\"/>\n";
    ss << "  <path d=\"" << pathD.str() << "\" fill=\"none\" stroke=\"#38bdf8\" stroke-width=\"2\"/>\n";

    for (size_t i = 0; i < velocities.size(); ++i)
    {
        float px = velToX(velocities[i]);
        float py = dbToY(levelsDb[i]);
        ss << "  <circle cx=\"" << px << "\" cy=\"" << py << "\" r=\"3.5\" fill=\"#38bdf8\" stroke=\"#0f172a\" stroke-width=\"1.5\"/>\n";
    }

    // Model Fit Badge
    if (fit.has_value())
    {
        ss << "  <text x=\"" << (padLeft + plotW - 6) << "\" y=\"" << (padTop + 14)
           << "\" fill=\"#94a3b8\" font-family=\"sans-serif\" font-size=\"10\" text-anchor=\"end\">Model: "
           << fit->model << " (R²=" << std::fixed << std::setprecision(3) << fit->rSquared << ")</text>\n";
    }

    ss << "</svg>\n";
    return ss.str();
}

std::string MeasurementSvgGenerator::generateDynamicsTimbreSvg(const std::vector<double>& velocities,
                                                              const std::vector<double>& centroidHz,
                                                              const std::vector<double>& rolloffHz,
                                                              const std::optional<CurveFitMetadata>& fit,
                                                              int width,
                                                              int height)
{
    if (velocities.empty() || centroidHz.empty())
    {
        std::ostringstream ss;
        ss << "<svg width=\"" << width << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height
           << "\" xmlns=\"http://www.w3.org/2000/svg\">\n"
           << "  <rect width=\"100%\" height=\"100%\" fill=\"#0f172a\" rx=\"6\"/>\n"
           << "  <text x=\"" << (width / 2) << "\" y=\"" << (height / 2)
           << "\" fill=\"#64748b\" font-family=\"sans-serif\" font-size=\"12\" text-anchor=\"middle\">No timbre curve data</text>\n"
           << "</svg>\n";
        return ss.str();
    }

    const float padLeft = 65.0f;
    const float padRight = 20.0f;
    const float padTop = 25.0f;
    const float padBottom = 35.0f;

    float plotW = static_cast<float>(width) - padLeft - padRight;
    float plotH = static_cast<float>(height) - padTop - padBottom;

    double maxFreq = 10000.0;
    for (double c : centroidHz)
        maxFreq = std::max(maxFreq, c * 1.2);
    for (double r : rolloffHz)
        maxFreq = std::max(maxFreq, r * 1.1);
    maxFreq = std::min(24000.0, maxFreq);

    auto velToX = [&](double v) -> float {
        return padLeft + static_cast<float>(std::clamp(v, 0.0, 127.0) / 127.0) * plotW;
    };
    auto hzToY = [&](double hz) -> float {
        return padTop + static_cast<float>((maxFreq - std::clamp(hz, 0.0, maxFreq)) / maxFreq) * plotH;
    };

    std::ostringstream ss;
    ss << "<svg width=\"" << width << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height
       << "\" xmlns=\"http://www.w3.org/2000/svg\">\n"
       << "  <rect width=\"100%\" height=\"100%\" fill=\"#0f172a\" rx=\"6\"/>\n";

    // Grid (Frequency)
    int freqSteps = 5;
    for (int i = 0; i <= freqSteps; ++i)
    {
        double hz = maxFreq * (static_cast<double>(i) / freqSteps);
        float y = hzToY(hz);
        ss << "  <line x1=\"" << padLeft << "\" y1=\"" << y << "\" x2=\"" << (padLeft + plotW) << "\" y2=\"" << y
           << "\" stroke=\"#1e293b\" stroke-width=\"1\"/>\n";
        ss << "  <text x=\"" << (padLeft - 8) << "\" y=\"" << (y + 4)
           << "\" fill=\"#64748b\" font-family=\"sans-serif\" font-size=\"10\" text-anchor=\"end\">"
           << static_cast<int>(hz) << " Hz</text>\n";
    }

    // Grid (Velocity)
    int velSteps[] = { 0, 32, 64, 96, 127 };
    for (int v : velSteps)
    {
        float x = velToX(static_cast<double>(v));
        ss << "  <line x1=\"" << x << "\" y1=\"" << padTop << "\" x2=\"" << x << "\" y2=\"" << (padTop + plotH)
           << "\" stroke=\"#1e293b\" stroke-width=\"1\" stroke-dasharray=\"3,3\"/>\n";
        ss << "  <text x=\"" << x << "\" y=\"" << (padTop + plotH + 18)
           << "\" fill=\"#64748b\" font-family=\"sans-serif\" font-size=\"10\" text-anchor=\"middle\">"
           << v << "</text>\n";
    }

    // Rolloff line if available
    if (!rolloffHz.empty() && rolloffHz.size() == velocities.size())
    {
        std::ostringstream rollD;
        for (size_t i = 0; i < velocities.size(); ++i)
        {
            float px = velToX(velocities[i]);
            float py = hzToY(rolloffHz[i]);
            if (i == 0) rollD << "M " << px << " " << py;
            else rollD << " L " << px << " " << py;
        }
        ss << "  <path d=\"" << rollD.str() << "\" fill=\"none\" stroke=\"#10b981\" stroke-width=\"1.5\" stroke-dasharray=\"4,3\"/>\n";
    }

    // Centroid line and dots
    std::ostringstream centD;
    for (size_t i = 0; i < velocities.size(); ++i)
    {
        float px = velToX(velocities[i]);
        float py = hzToY(centroidHz[i]);
        if (i == 0) centD << "M " << px << " " << py;
        else centD << " L " << px << " " << py;
    }
    ss << "  <path d=\"" << centD.str() << "\" fill=\"none\" stroke=\"#f59e0b\" stroke-width=\"2\"/>\n";

    for (size_t i = 0; i < velocities.size(); ++i)
    {
        float px = velToX(velocities[i]);
        float py = hzToY(centroidHz[i]);
        ss << "  <circle cx=\"" << px << "\" cy=\"" << py << "\" r=\"3.5\" fill=\"#f59e0b\" stroke=\"#0f172a\" stroke-width=\"1.5\"/>\n";
    }

    // Legend
    ss << "  <circle cx=\"" << (padLeft + 10) << "\" cy=\"" << (padTop + 12) << "\" r=\"4\" fill=\"#f59e0b\"/>\n";
    ss << "  <text x=\"" << (padLeft + 20) << "\" y=\"" << (padTop + 15) << "\" fill=\"#f59e0b\" font-family=\"sans-serif\" font-size=\"10\">Centroid</text>\n";
    if (!rolloffHz.empty())
    {
        ss << "  <line x1=\"" << (padLeft + 80) << "\" y1=\"" << (padTop + 12) << "\" x2=\"" << (padLeft + 95) << "\" y2=\"" << (padTop + 12)
           << "\" stroke=\"#10b981\" stroke-width=\"2\" stroke-dasharray=\"3,2\"/>\n";
        ss << "  <text x=\"" << (padLeft + 102) << "\" y=\"" << (padTop + 15) << "\" fill=\"#10b981\" font-family=\"sans-serif\" font-size=\"10\">Rolloff (85%)</text>\n";
    }

    if (fit.has_value())
    {
        ss << "  <text x=\"" << (padLeft + plotW - 6) << "\" y=\"" << (padTop + 14)
           << "\" fill=\"#94a3b8\" font-family=\"sans-serif\" font-size=\"10\" text-anchor=\"end\">Fit R²="
           << std::fixed << std::setprecision(3) << fit->rSquared << "</text>\n";
    }

    ss << "</svg>\n";
    return ss.str();
}

// ==============================================================================
// LFO Cyclic Modulation SVG Charts
// ==============================================================================

std::string MeasurementSvgGenerator::generateModulationTimeSvg(const std::vector<double>& timeMs,
                                                               const std::vector<double>& values,
                                                               const std::string& yUnit,
                                                               const std::string& waveformShape,
                                                               int width,
                                                               int height)
{
    if (timeMs.empty() || values.empty() || timeMs.size() != values.size())
    {
        std::ostringstream ss;
        ss << "<svg width=\"" << width << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height
           << "\" xmlns=\"http://www.w3.org/2000/svg\">\n"
           << "  <rect width=\"100%\" height=\"100%\" fill=\"#0f172a\" rx=\"6\"/>\n"
           << "  <text x=\"" << (width / 2) << "\" y=\"" << (height / 2)
           << "\" fill=\"#64748b\" font-family=\"sans-serif\" font-size=\"12\" text-anchor=\"middle\">No modulation trajectory data</text>\n"
           << "</svg>\n";
        return ss.str();
    }

    const float padLeft = 60.0f;
    const float padRight = 20.0f;
    const float padTop = 25.0f;
    const float padBottom = 35.0f;

    float plotW = static_cast<float>(width) - padLeft - padRight;
    float plotH = static_cast<float>(height) - padTop - padBottom;

    double minTime = timeMs.front();
    double maxTime = timeMs.back();
    if (std::abs(maxTime - minTime) < 1e-4) maxTime = minTime + 1000.0;

    double minVal = *std::min_element(values.begin(), values.end());
    double maxVal = *std::max_element(values.begin(), values.end());
    if (std::abs(maxVal - minVal) < 1e-3)
    {
        minVal -= 1.0;
        maxVal += 1.0;
    }
    else
    {
        double margin = (maxVal - minVal) * 0.15;
        minVal -= margin;
        maxVal += margin;
    }

    auto tToX = [&](double t) -> float {
        return padLeft + static_cast<float>((std::clamp(t, minTime, maxTime) - minTime) / (maxTime - minTime)) * plotW;
    };
    auto valToY = [&](double v) -> float {
        return padTop + static_cast<float>((maxVal - std::clamp(v, minVal, maxVal)) / (maxVal - minVal)) * plotH;
    };

    std::ostringstream ss;
    ss << "<svg width=\"" << width << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height
       << "\" xmlns=\"http://www.w3.org/2000/svg\">\n"
       << "  <rect width=\"100%\" height=\"100%\" fill=\"#0f172a\" rx=\"6\"/>\n";

    // Y Grid
    int ySteps = 4;
    for (int i = 0; i <= ySteps; ++i)
    {
        double v = minVal + (maxVal - minVal) * (static_cast<double>(i) / ySteps);
        float y = valToY(v);
        ss << "  <line x1=\"" << padLeft << "\" y1=\"" << y << "\" x2=\"" << (padLeft + plotW) << "\" y2=\"" << y
           << "\" stroke=\"#1e293b\" stroke-width=\"1\"/>\n";
        ss << "  <text x=\"" << (padLeft - 8) << "\" y=\"" << (y + 4)
           << "\" fill=\"#64748b\" font-family=\"sans-serif\" font-size=\"10\" text-anchor=\"end\">"
           << std::fixed << std::setprecision(1) << v << " " << yUnit << "</text>\n";
    }

    // X Grid (Time ms)
    int xSteps = 5;
    for (int i = 0; i <= xSteps; ++i)
    {
        double t = minTime + (maxTime - minTime) * (static_cast<double>(i) / xSteps);
        float x = tToX(t);
        ss << "  <line x1=\"" << x << "\" y1=\"" << padTop << "\" x2=\"" << x << "\" y2=\"" << (padTop + plotH)
           << "\" stroke=\"#1e293b\" stroke-width=\"1\" stroke-dasharray=\"3,3\"/>\n";
        ss << "  <text x=\"" << x << "\" y=\"" << (padTop + plotH + 18)
           << "\" fill=\"#64748b\" font-family=\"sans-serif\" font-size=\"10\" text-anchor=\"middle\">"
           << static_cast<int>(std::round(t)) << " ms</text>\n";
    }

    // Trajectory path
    std::ostringstream pathD;
    for (size_t i = 0; i < timeMs.size(); ++i)
    {
        float px = tToX(timeMs[i]);
        float py = valToY(values[i]);
        if (i == 0) pathD << "M " << px << " " << py;
        else pathD << " L " << px << " " << py;
    }
    ss << "  <path d=\"" << pathD.str() << "\" fill=\"none\" stroke=\"#06b6d4\" stroke-width=\"2\"/>\n";

    // Waveform badge
    if (!waveformShape.empty())
    {
        ss << "  <text x=\"" << (padLeft + plotW - 6) << "\" y=\"" << (padTop + 14)
           << "\" fill=\"#38bdf8\" font-family=\"sans-serif\" font-size=\"10\" font-weight=\"600\" text-anchor=\"end\">Waveform: "
           << waveformShape << "</text>\n";
    }

    ss << "</svg>\n";
    return ss.str();
}

std::string MeasurementSvgGenerator::generateModulationSpectrumSvg(const std::vector<double>& freqHz,
                                                                   const std::vector<double>& magDb,
                                                                   const std::vector<ModulationSideband>& sidebands,
                                                                   double carrierHz,
                                                                   int width,
                                                                   int height)
{
    if (freqHz.empty() || magDb.empty())
    {
        std::ostringstream ss;
        ss << "<svg width=\"" << width << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height
           << "\" xmlns=\"http://www.w3.org/2000/svg\">\n"
           << "  <rect width=\"100%\" height=\"100%\" fill=\"#0f172a\" rx=\"6\"/>\n"
           << "  <text x=\"" << (width / 2) << "\" y=\"" << (height / 2)
           << "\" fill=\"#64748b\" font-family=\"sans-serif\" font-size=\"12\" text-anchor=\"middle\">No spectrum data</text>\n"
           << "</svg>\n";
        return ss.str();
    }

    const float padLeft = 60.0f;
    const float padRight = 20.0f;
    const float padTop = 25.0f;
    const float padBottom = 35.0f;

    float plotW = static_cast<float>(width) - padLeft - padRight;
    float plotH = static_cast<float>(height) - padTop - padBottom;

    double minFreq = freqHz.front();
    double maxFreq = freqHz.back();
    if (std::abs(maxFreq - minFreq) < 1.0) maxFreq = minFreq + 1000.0;

    const double minDb = -96.0;
    const double maxDb = 0.0;

    auto fToX = [&](double f) -> float {
        return padLeft + static_cast<float>((std::clamp(f, minFreq, maxFreq) - minFreq) / (maxFreq - minFreq)) * plotW;
    };
    auto dbToY = [&](double db) -> float {
        return padTop + static_cast<float>((maxDb - std::clamp(db, minDb, maxDb)) / (maxDb - minDb)) * plotH;
    };

    std::ostringstream ss;
    ss << "<svg width=\"" << width << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height
       << "\" xmlns=\"http://www.w3.org/2000/svg\">\n"
       << "  <rect width=\"100%\" height=\"100%\" fill=\"#0f172a\" rx=\"6\"/>\n";

    // Grid (dBFS)
    double dbSteps[] = { 0.0, -20.0, -40.0, -60.0, -80.0, -96.0 };
    for (double db : dbSteps)
    {
        float y = dbToY(db);
        ss << "  <line x1=\"" << padLeft << "\" y1=\"" << y << "\" x2=\"" << (padLeft + plotW) << "\" y2=\"" << y
           << "\" stroke=\"#1e293b\" stroke-width=\"1\"/>\n";
        ss << "  <text x=\"" << (padLeft - 8) << "\" y=\"" << (y + 4)
           << "\" fill=\"#64748b\" font-family=\"sans-serif\" font-size=\"10\" text-anchor=\"end\">"
           << static_cast<int>(db) << " dB</text>\n";
    }

    // Grid (Frequency)
    int fSteps = 5;
    for (int i = 0; i <= fSteps; ++i)
    {
        double f = minFreq + (maxFreq - minFreq) * (static_cast<double>(i) / fSteps);
        float x = fToX(f);
        ss << "  <line x1=\"" << x << "\" y1=\"" << padTop << "\" x2=\"" << x << "\" y2=\"" << (padTop + plotH)
           << "\" stroke=\"#1e293b\" stroke-width=\"1\" stroke-dasharray=\"3,3\"/>\n";
        ss << "  <text x=\"" << x << "\" y=\"" << (padTop + plotH + 18)
           << "\" fill=\"#64748b\" font-family=\"sans-serif\" font-size=\"10\" text-anchor=\"middle\">"
           << static_cast<int>(std::round(f)) << " Hz</text>\n";
    }

    // Spectrum curve
    std::ostringstream pathD;
    for (size_t i = 0; i < freqHz.size(); ++i)
    {
        float px = fToX(freqHz[i]);
        float py = dbToY(magDb[i]);
        if (i == 0) pathD << "M " << px << " " << py;
        else pathD << " L " << px << " " << py;
    }
    ss << "  <path d=\"" << pathD.str() << "\" fill=\"none\" stroke=\"#818cf8\" stroke-width=\"1.5\"/>\n";

    // Carrier Marker
    if (carrierHz >= minFreq && carrierHz <= maxFreq)
    {
        float cx = fToX(carrierHz);
        ss << "  <line x1=\"" << cx << "\" y1=\"" << padTop << "\" x2=\"" << cx << "\" y2=\"" << (padTop + plotH)
           << "\" stroke=\"#fbbf24\" stroke-width=\"1.5\" stroke-dasharray=\"4,3\"/>\n";
        ss << "  <text x=\"" << cx << "\" y=\"" << (padTop + 14)
           << "\" fill=\"#fbbf24\" font-family=\"sans-serif\" font-size=\"10\" font-weight=\"600\" text-anchor=\"middle\">Carrier "
           << static_cast<int>(std::round(carrierHz)) << " Hz</text>\n";
    }

    // Sideband Markers
    for (const auto& sb : sidebands)
    {
        if (sb.sidebandFrequencyHz >= minFreq && sb.sidebandFrequencyHz <= maxFreq)
        {
            float sx = fToX(sb.sidebandFrequencyHz);
            ss << "  <circle cx=\"" << sx << "\" cy=\"" << (padTop + plotH * 0.4f)
               << "\" r=\"3\" fill=\"#34d399\" stroke=\"#0f172a\" stroke-width=\"1\"/>\n";
            ss << "  <text x=\"" << sx << "\" y=\"" << (padTop + plotH * 0.4f - 6)
               << "\" fill=\"#34d399\" font-family=\"sans-serif\" font-size=\"9\" text-anchor=\"middle\">"
               << (sb.order > 0 ? "+" : "") << sb.order << " (" << std::fixed << std::setprecision(1)
               << sb.levelRelativeToCarrierDb << "dB)</text>\n";
        }
    }

    ss << "</svg>\n";
    return ss.str();
}

std::string MeasurementSvgGenerator::generateMultiSeriesSvg(const std::vector<SvgSeries>& series,
                                                            const SvgPlotSpec& spec)
{
    std::ostringstream ss;
    ss << "<svg viewBox=\"0 0 " << spec.width << " " << spec.height << "\" xmlns=\"http://www.w3.org/2000/svg\">\n";

    // Background and plotting area
    ss << "<rect x=\"50\" y=\"20\" width=\"550\" height=\"260\" fill=\"#161620\" stroke=\"#2e2e40\"/>\n";

    const double xMin = spec.xMin;
    const double xMax = (std::abs(spec.xMax - spec.xMin) > 1e-6) ? spec.xMax : (xMin + 1.0);
    const double yMin = spec.yMin;
    const double yMax = (std::abs(spec.yMax - spec.yMin) > 1e-6) ? spec.yMax : (yMin + 1.0);

    // X Ticks & Grid
    std::vector<double> xTicks = spec.xTicks;
    if (xTicks.empty())
        xTicks = { 0.0, 32.0, 64.0, 96.0, 127.0 };

    for (double v : xTicks)
    {
        const float x = 50.0f + static_cast<float>((v - xMin) / (xMax - xMin)) * 550.0f;
        ss << "<line x1=\"" << x << "\" y1=\"20\" x2=\"" << x << "\" y2=\"280\" stroke=\"#262636\"/>\n";
        ss << "<text x=\"" << x << "\" y=\"295\" fill=\"#8e8ea0\" font-size=\"10\" text-anchor=\"middle\">"
           << static_cast<int>(std::round(v)) << "</text>\n";
    }

    // Y Ticks & Grid
    std::vector<double> yTicks = spec.yTicks;
    if (yTicks.empty())
        yTicks = { 0.0, -24.0, -48.0, -72.0, -96.0 };

    for (double db : yTicks)
    {
        const float frac = static_cast<float>((db - yMin) / (yMax - yMin));
        const float y = 280.0f - frac * 260.0f;
        ss << "<line x1=\"50\" y1=\"" << y << "\" x2=\"600\" y2=\"" << y << "\" stroke=\"#262636\"/>\n";
        ss << "<text x=\"42\" y=\"" << (y + 3.0f) << "\" fill=\"#8e8ea0\" font-size=\"10\" text-anchor=\"end\">"
           << static_cast<int>(std::round(db)) << " " << spec.yUnit << "</text>\n";
    }

    // Palette & Rendering
    const char* kColours[] = { "#00d4ff", "#ffa726", "#e040fb", "#76ff03", "#ff5252" };
    int seriesIdx = 0;

    for (const auto& s : series)
    {
        if (s.points.empty())
            continue;

        const std::string col = !s.strokeColor.empty() ? s.strokeColor : kColours[seriesIdx % 5];
        std::string dashAttr = "";
        if (s.lineStyle == 1) dashAttr = "stroke-dasharray=\"6,3\" ";
        else if (s.lineStyle == 2) dashAttr = "stroke-dasharray=\"8,3,2,3\" ";
        else if (s.lineStyle == 3) dashAttr = "stroke-dasharray=\"2,2\" ";

        // Polyline
        ss << "<polyline fill=\"none\" stroke=\"" << col << "\" stroke-width=\"2.5\" " << dashAttr << "points=\"";
        for (const auto& pt : s.points)
        {
            const float x = 50.0f + static_cast<float>((pt.first - xMin) / (xMax - xMin)) * 550.0f;
            const float frac = static_cast<float>((pt.second - yMin) / (yMax - yMin));
            const float y = 280.0f - std::max(0.0f, std::min(1.0f, frac)) * 260.0f;
            ss << x << "," << y << " ";
        }
        ss << "\"/>\n";

        // Markers
        for (const auto& pt : s.points)
        {
            const float x = 50.0f + static_cast<float>((pt.first - xMin) / (xMax - xMin)) * 550.0f;
            const float frac = static_cast<float>((pt.second - yMin) / (yMax - yMin));
            const float y = 280.0f - std::max(0.0f, std::min(1.0f, frac)) * 260.0f;

            if (s.markerStyle == 1) // Rectangle
                ss << "<rect x=\"" << (x - 3.5f) << "\" y=\"" << (y - 3.5f) << "\" width=\"7\" height=\"7\" fill=\"" << col << "\" stroke=\"#000\"/>\n";
            else // Circle default
                ss << "<circle cx=\"" << x << "\" cy=\"" << y << "\" r=\"4\" fill=\"" << col << "\" stroke=\"#000\"/>\n";
        }

        // Legend entry
        const float legY = 40.0f + seriesIdx * 20.0f;
        ss << "<line x1=\"615\" y1=\"" << legY << "\" x2=\"635\" y2=\"" << legY << "\" stroke=\"" << col << "\" stroke-width=\"2.5\" " << dashAttr << "/>\n";
        ss << "<circle cx=\"625\" cy=\"" << legY << "\" r=\"3.5\" fill=\"" << col << "\"/>\n";
        ss << "<text x=\"645\" y=\"" << (legY + 3.0f) << "\" fill=\"#e0e0e0\" font-size=\"11\">" << s.label << "</text>\n";

        seriesIdx++;
    }

    ss << "</svg>\n";
    return ss.str();
}

} // namespace abdaudiolab::measurement

