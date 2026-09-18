/**
 * @file ComplexEnvelopeSvgRenderer.cpp
 * @brief Implementation of deterministic and safe multi-series SVG renderer for complex envelopes.
 * @author ABDSynths
 * @date 2026
 */

#include "ComplexEnvelopeSvgRenderer.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

namespace abdaudiolab::measurement
{

namespace
{

[[nodiscard]] std::string toLowerAscii(std::string_view s)
{
    std::string res;
    res.reserve(s.size());
    for (char c : s)
    {
        res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return res;
}

} // anonymous namespace

bool ComplexEnvelopeSvgRenderer::validateSvgSafety(const std::string& svg, std::string& outError) noexcept
{
    outError.clear();
    std::string lower = toLowerAscii(svg);

    const std::vector<std::pair<std::string, std::string>> forbidden = {
        { "<script", "Contains forbidden <script> tag" },
        { "</script", "Contains forbidden </script> tag" },
        { "href=\"http", "Contains external http reference" },
        { "href='http", "Contains external http reference" },
        { "href=\"https", "Contains external https reference" },
        { "href='https", "Contains external https reference" },
        { "xlink:href", "Contains forbidden xlink:href attribute" },
        { "url(", "Contains forbidden url() construct" },
        { "<foreignobject", "Contains forbidden <foreignObject> element" },
        { "onclick=", "Contains inline event handler onclick" },
        { "onload=", "Contains inline event handler onload" },
        { "onerror=", "Contains inline event handler onerror" },
        { "onmouseover=", "Contains inline event handler onmouseover" },
        { "nan", "Contains invalid numeric value (NaN)" },
        { "infinity", "Contains invalid numeric value (Infinity)" },
        { "inf", "Contains invalid numeric value (Inf)" }
    };

    for (const auto& [pattern, reason] : forbidden)
    {
        if (lower.find(pattern) != std::string::npos)
        {
            outError = reason;
            return false;
        }
    }

    if (lower.find("<svg") == std::string::npos || lower.find("</svg>") == std::string::npos)
    {
        outError = "Malformed SVG: Missing opening <svg> or closing </svg> tag";
        return false;
    }

    return true;
}

std::string ComplexEnvelopeSvgRenderer::renderOverlaySvg(
    const MultiDomainEnvelopeCaptureRecord& record,
    const std::vector<ObservableComparisonReport>& /*comparisons*/,
    const std::vector<EnvelopeStageDescriptor>& nativeStages,
    EnvelopeDomain domain,
    const RenderOptions& options) noexcept
{
    const EnvelopeTrajectory* obsTraj = nullptr;
    std::string domainName = "Timbre";
    std::string yAxisUnit = "Normalized (0..1)";

    switch (domain)
    {
        case EnvelopeDomain::Pitch:
            obsTraj = &record.pitchTrajectory;
            domainName = "Pitch (DCO)";
            yAxisUnit = "Normalized / Semitones";
            break;
        case EnvelopeDomain::Timbre:
            obsTraj = &record.timbreTrajectory;
            domainName = "Timbre (DCW)";
            yAxisUnit = "Spectral Centroid Proxy (0..1)";
            break;
        case EnvelopeDomain::Amplitude:
            obsTraj = &record.amplitudeTrajectory;
            domainName = "Amplitude (DCA)";
            yAxisUnit = "RMS Envelope (0..1)";
            break;
    }

    const int plotWidth = options.width - options.paddingLeft - options.paddingRight;
    const int plotHeight = options.height - options.paddingTop - options.paddingBottom;

    double maxTimeMs = 1000.0;
    if (obsTraj != nullptr && !obsTraj->points.empty())
    {
        maxTimeMs = std::max(100.0, obsTraj->points.back().timeMs);
    }
    else if (record.totalDurationMs > 0.0)
    {
        maxTimeMs = record.totalDurationMs;
    }

    // Auto-scale Y min/max in [0, 1]
    const double minY = 0.0;
    const double maxY = 1.0;

    auto timeToX = [&](double tMs) -> double {
        double clampedT = std::clamp(tMs, 0.0, maxTimeMs);
        return options.paddingLeft + (clampedT / maxTimeMs) * plotWidth;
    };

    auto valToY = [&](double v) -> double {
        double clampedV = std::clamp(v, minY, maxY);
        return options.paddingTop + (1.0 - (clampedV - minY) / (maxY - minY)) * plotHeight;
    };

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);

    ss << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
       << "viewBox=\"0 0 " << options.width << " " << options.height << "\" "
       << "width=\"" << options.width << "\" height=\"" << options.height << "\" "
       << "style=\"background-color:#0f172a;font-family:system-ui,-apple-system,sans-serif;\">\n";

    // Style block (inline without urls or remote resources)
    ss << "<style>\n"
       << "  .grid { stroke: #1e293b; stroke-width: 1; stroke-dasharray: 2,4; }\n"
       << "  .axis { stroke: #475569; stroke-width: 1.5; }\n"
       << "  .title { fill: #f8fafc; font-size: 14px; font-weight: 600; }\n"
       << "  .label { fill: #94a3b8; font-size: 11px; }\n"
       << "  .legend { font-size: 11px; fill: #cbd5e1; }\n"
       << "  .stage-line { stroke: #64748b; stroke-width: 1; stroke-dasharray: 4,4; opacity: 0.6; }\n"
       << "  .stage-badge { font-size: 10px; font-weight: bold; }\n"
       << "</style>\n";

    // Header Title
    ss << "<text x=\"" << options.paddingLeft << "\" y=\"26\" class=\"title\">"
       << options.title << " - " << domainName << "</text>\n";

    // Background plot area
    ss << "<rect x=\"" << options.paddingLeft << "\" y=\"" << options.paddingTop << "\" "
       << "width=\"" << plotWidth << "\" height=\"" << plotHeight << "\" "
       << "fill=\"#090d16\" stroke=\"#334155\" stroke-width=\"1\" rx=\"4\"/>\n";

    // Horizontal grid lines and Y labels (5 steps)
    for (int i = 0; i <= 4; ++i)
    {
        double yVal = minY + (maxY - minY) * (static_cast<double>(i) / 4.0);
        double yPos = valToY(yVal);

        ss << "<line x1=\"" << options.paddingLeft << "\" y1=\"" << yPos << "\" "
           << "x2=\"" << (options.paddingLeft + plotWidth) << "\" y2=\"" << yPos << "\" class=\"grid\"/>\n";
        ss << "<text x=\"" << (options.paddingLeft - 8) << "\" y=\"" << (yPos + 4) << "\" "
           << "text-anchor=\"end\" class=\"label\">" << yVal << "</text>\n";
    }

    // Vertical grid lines and X labels (5 steps)
    for (int i = 0; i <= 5; ++i)
    {
        double tVal = maxTimeMs * (static_cast<double>(i) / 5.0);
        double xPos = timeToX(tVal);

        ss << "<line x1=\"" << xPos << "\" y1=\"" << options.paddingTop << "\" "
           << "x2=\"" << xPos << "\" y2=\"" << (options.paddingTop + plotHeight) << "\" class=\"grid\"/>\n";
        ss << "<text x=\"" << xPos << "\" y=\"" << (options.paddingTop + plotHeight + 18) << "\" "
           << "text-anchor=\"middle\" class=\"label\">" << static_cast<int>(tVal) << " ms</text>\n";
    }

    // Stage boundary markers (if native stages provided)
    if (options.showNativeStages && !nativeStages.empty())
    {
        double currentStageTimeMs = 0.0;
        double defaultStageDurationMs = maxTimeMs / static_cast<double>(std::max<size_t>(1, nativeStages.size()));

        for (const auto& stage : nativeStages)
        {
            double stageDur = (stage.durationMs > 0.0) ? stage.durationMs : defaultStageDurationMs;
            currentStageTimeMs += stageDur;
            if (currentStageTimeMs > maxTimeMs) break;

            double stageX = timeToX(currentStageTimeMs);
            ss << "<line x1=\"" << stageX << "\" y1=\"" << options.paddingTop << "\" "
               << "x2=\"" << stageX << "\" y2=\"" << (options.paddingTop + plotHeight) << "\" class=\"stage-line\"/>\n";

            // Stage label text
            ss << "<text x=\"" << stageX << "\" y=\"" << (options.paddingTop - 6) << "\" "
               << "text-anchor=\"middle\" fill=\"#94a3b8\" class=\"stage-badge\">S" << stage.stageIndex << "</text>\n";

            if (stage.isSustainPoint)
            {
                ss << "<circle cx=\"" << stageX << "\" cy=\"" << (options.paddingTop + 12) << "\" r=\"4\" fill=\"#f59e0b\"/>\n";
                ss << "<text x=\"" << stageX << "\" y=\"" << (options.paddingTop + 24) << "\" "
                   << "text-anchor=\"middle\" fill=\"#f59e0b\" class=\"stage-badge\">SUS</text>\n";
            }
            if (stage.isEndKeyOnPoint)
            {
                ss << "<circle cx=\"" << stageX << "\" cy=\"" << (options.paddingTop + 12) << "\" r=\"4\" fill=\"#ef4444\"/>\n";
                ss << "<text x=\"" << stageX << "\" y=\"" << (options.paddingTop + 24) << "\" "
                   << "text-anchor=\"middle\" fill=\"#ef4444\" class=\"stage-badge\">END</text>\n";
            }
        }
    }

    // Projected Reference Trajectory (Dashed amber line)
    if (options.showProjectedReference && !nativeStages.empty())
    {
        ss << "<path d=\"";
        double tAcc = 0.0;
        double defaultDur = maxTimeMs / static_cast<double>(std::max<size_t>(1, nativeStages.size()));
        double startLevelNorm = 0.0;

        ss << "M " << timeToX(0.0) << " " << valToY(startLevelNorm);

        for (const auto& stage : nativeStages)
        {
            double dur = (stage.durationMs > 0.0) ? stage.durationMs : defaultDur;
            tAcc += dur;
            double targetNorm = stage.targetLevel.has_value()
                                    ? std::clamp(*stage.targetLevel / 99.0, 0.0, 1.0)
                                    : 0.0;
            ss << " L " << timeToX(tAcc) << " " << valToY(targetNorm);
        }

        ss << "\" fill=\"none\" stroke=\"#f59e0b\" stroke-width=\"2.5\" stroke-dasharray=\"6,4\" stroke-linecap=\"round\"/>\n";
    }

    // Observed Acoustic Trajectory (Solid sky blue line with gap handling on silent/unreliable intervals)
    if (options.showObservedTrajectory && obsTraj != nullptr && !obsTraj->points.empty())
    {
        bool inSegment = false;
        std::ostringstream pathSs;
        pathSs << std::fixed << std::setprecision(2);

        for (const auto& pt : obsTraj->points)
        {
            bool isPointValid = pt.value.has_value() &&
                                (pt.status == "valid" || pt.status == "transient") &&
                                !std::isnan(*pt.value) && !std::isinf(*pt.value);

            if (isPointValid)
            {
                double px = timeToX(pt.timeMs);
                double py = valToY(*pt.value);

                if (!inSegment)
                {
                    pathSs << " M " << px << " " << py;
                    inSegment = true;
                }
                else
                {
                    pathSs << " L " << px << " " << py;
                }
            }
            else
            {
                // Break segment on silence or unreliable observation: leaves visible gap
                inSegment = false;
            }
        }

        std::string pathData = pathSs.str();
        if (!pathData.empty())
        {
            ss << "<path d=\"" << pathData << "\" fill=\"none\" stroke=\"#38bdf8\" stroke-width=\"2.5\" stroke-linejoin=\"round\"/>\n";
        }
    }
    else
    {
        ss << "<text x=\"" << (options.paddingLeft + plotWidth / 2) << "\" "
           << "y=\"" << (options.paddingTop + plotHeight / 2) << "\" "
           << "text-anchor=\"middle\" fill=\"#64748b\" font-size=\"13\">"
           << "[No observable trajectory data recorded in this domain]</text>\n";
    }

    // Legends and Metrological Honesty Notice
    int legendY = options.height - 20;

    // Legend 1: Observed
    ss << "<line x1=\"" << options.paddingLeft << "\" y1=\"" << (legendY - 4) << "\" "
       << "x2=\"" << (options.paddingLeft + 24) << "\" y2=\"" << (legendY - 4) << "\" "
       << "stroke=\"#38bdf8\" stroke-width=\"2.5\"/>\n";
    ss << "<text x=\"" << (options.paddingLeft + 30) << "\" y=\"" << legendY << "\" class=\"legend\">"
       << "Acoustic Observed (proxy: " << (domain == EnvelopeDomain::Timbre ? "spectral_centroid" : "energy") << ")</text>\n";

    // Legend 2: Projected Reference
    int leg2X = options.paddingLeft + 280;
    ss << "<line x1=\"" << leg2X << "\" y1=\"" << (legendY - 4) << "\" "
       << "x2=\"" << (leg2X + 24) << "\" y2=\"" << (legendY - 4) << "\" "
       << "stroke=\"#f59e0b\" stroke-width=\"2.5\" stroke-dasharray=\"6,4\"/>\n";
    ss << "<text x=\"" << (leg2X + 30) << "\" y=\"" << legendY << "\" class=\"legend\">"
       << "Projected Native Reference</text>\n";

    // Mandatory Metrological Label
    ss << "<text x=\"" << (options.width - options.paddingRight) << "\" y=\"" << legendY << "\" "
       << "text-anchor=\"end\" fill=\"#10b981\" font-size=\"11px\" font-weight=\"bold\">"
       << "[observable_agreement | phaseDistortionProxy: not_claimed]</text>\n";

    // Closing tag
    ss << "</svg>\n";

    return ss.str();
}

} // namespace abdaudiolab::measurement
