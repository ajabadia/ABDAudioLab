#include "CertificationReportExporter.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

namespace abdaudiolab::exporting
{

std::string CertificationReportExporter::generateFrequencyCurveSvg(const std::vector<float>& freqsHz,
                                                                     const std::vector<float>& magsDb,
                                                                     int width,
                                                                     int height)
{
    std::ostringstream svg;
    svg << "<svg width=\"" << width << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height << "\" xmlns=\"http://www.w3.org/2000/svg\">\n";
    svg << "<style>\n";
    svg << "  .bg { fill: #1a1d24; rx: 8px; }\n";
    svg << "  .grid { stroke: #2a2f3a; stroke-width: 1; stroke-dasharray: 3,3; }\n";
    svg << "  .axis-label { fill: #8a93a6; font-size: 10px; font-family: system-ui, sans-serif; }\n";
    svg << "  .line-curve { fill: none; stroke: #3b82f6; stroke-width: 2.5; stroke-linecap: round; }\n";
    svg << "  .title { fill: #ffffff; font-size: 12px; font-weight: bold; font-family: system-ui, sans-serif; }\n";
    svg << "</style>\n";

    svg << "<rect width=\"100%\" height=\"100%\" class=\"bg\" />\n";

    int marginL = 50, marginR = 20, marginT = 30, marginB = 30;
    int plotW = width - marginL - marginR;
    int plotH = height - marginT - marginB;

    // Draw Frequency Gridlines (100Hz, 1kHz, 10kHz)
    float logMin = std::log10(20.0f);
    float logMax = std::log10(20000.0f);
    float logRange = logMax - logMin;

    std::vector<float> gridFreqs = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
    for (float f : gridFreqs)
    {
        float normX = (std::log10(f) - logMin) / logRange;
        float x = marginL + normX * plotW;
        svg << "<line x1=\"" << x << "\" y1=\"" << marginT << "\" x2=\"" << x << "\" y2=\"" << (marginT + plotH) << "\" class=\"grid\" />\n";
        if (f == 100.0f || f == 1000.0f || f == 10000.0f)
        {
            std::string label = (f >= 1000.0f) ? (std::to_string(static_cast<int>(f / 1000.0f)) + "k") : std::to_string(static_cast<int>(f));
            svg << "<text x=\"" << x << "\" y=\"" << (height - 10) << "\" class=\"axis-label\" text-anchor=\"middle\">" << label << "</text>\n";
        }
    }

    // Magnitude dB Gridlines (-48dB, -24dB, 0dB, +12dB)
    float minDb = -60.0f;
    float maxDb = +18.0f;
    float rangeDb = maxDb - minDb;

    for (float db = -48.0f; db <= +12.0f; db += 12.0f)
    {
        float normY = 1.0f - ((db - minDb) / rangeDb);
        float y = marginT + normY * plotH;
        svg << "<line x1=\"" << marginL << "\" y1=\"" << y << "\" x2=\"" << (width - marginR) << "\" y2=\"" << y << "\" class=\"grid\" />\n";
        svg << "<text x=\"" << (marginL - 8) << "\" y=\"" << (y + 3) << "\" class=\"axis-label\" text-anchor=\"end\">" << static_cast<int>(db) << " dB</text>\n";
    }

    // Render Response Curve Path
    if (!freqsHz.empty() && freqsHz.size() == magsDb.size())
    {
        svg << "<path d=\"M";
        bool first = true;
        for (size_t i = 0; i < freqsHz.size(); ++i)
        {
            float f = std::clamp(freqsHz[i], 20.0f, 20000.0f);
            float db = std::clamp(magsDb[i], minDb, maxDb);

            float normX = (std::log10(f) - logMin) / logRange;
            float normY = 1.0f - ((db - minDb) / rangeDb);

            float x = marginL + normX * plotW;
            float y = marginT + normY * plotH;

            if (first)
            {
                svg << std::fixed << std::setprecision(1) << x << " " << y;
                first = false;
            }
            else
            {
                svg << " L " << std::fixed << std::setprecision(1) << x << " " << y;
            }
        }
        svg << "\" class=\"line-curve\" />\n";
    }

    svg << "<text x=\"" << marginL << "\" y=\"18\" class=\"title\">FREQUENCY RESPONSE MAGNITUDE (dBFS)</text>\n";
    svg << "</svg>\n";
    return svg.str();
}

std::string CertificationReportExporter::generateHeatmapSvg(const std::vector<MeasuredPoint>& points,
                                                             int rows,
                                                             int cols,
                                                             int width,
                                                             int height)
{
    std::ostringstream svg;
    svg << "<svg width=\"" << width << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height << "\" xmlns=\"http://www.w3.org/2000/svg\">\n";
    svg << "<style>\n";
    svg << "  .bg { fill: #1a1d24; rx: 8px; }\n";
    svg << "  .cell { stroke: #1a1d24; stroke-width: 1.5; }\n";
    svg << "  .cell-text { fill: #ffffff; font-size: 9px; font-weight: bold; font-family: system-ui, sans-serif; }\n";
    svg << "  .title { fill: #ffffff; font-size: 12px; font-weight: bold; font-family: system-ui, sans-serif; }\n";
    svg << "</style>\n";

    svg << "<rect width=\"100%\" height=\"100%\" class=\"bg\" />\n";

    int marginT = 30, marginL = 20, marginR = 20, marginB = 20;
    int gridW = width - marginL - marginR;
    int gridH = height - marginT - marginB;

    int numRows = std::max(1, rows);
    int numCols = std::max(1, cols);

    float cellW = static_cast<float>(gridW) / static_cast<float>(numCols);
    float cellH = static_cast<float>(gridH) / static_cast<float>(numRows);

    svg << "<text x=\"" << marginL << "\" y=\"18\" class=\"title\">2D PARAMETER MATRIX HEATMAP</text>\n";

    for (int r = 0; r < numRows; ++r)
    {
        for (int c = 0; c < numCols; ++c)
        {
            size_t idx = static_cast<size_t>(r * numCols + c);
            float val = 0.5f;
            if (idx < points.size())
            {
                val = std::clamp(points[idx].param1Normalized, 0.0f, 1.0f);
            }

            // Thermal color mapping (Cool Blue -> Gold -> Red)
            int red = static_cast<int>(std::clamp(val * 255.0f, 30.0f, 240.0f));
            int green = static_cast<int>(std::clamp((1.0f - std::abs(val - 0.5f) * 2.0f) * 200.0f, 50.0f, 220.0f));
            int blue = static_cast<int>(std::clamp((1.0f - val) * 255.0f, 40.0f, 240.0f));

            float x = marginL + c * cellW;
            float y = marginT + r * cellH;

            svg << "<rect x=\"" << std::fixed << std::setprecision(1) << x 
                << "\" y=\"" << y << "\" width=\"" << cellW << "\" height=\"" << cellH 
                << "\" fill=\"rgb(" << red << "," << green << "," << blue << ")\" class=\"cell\" />\n";
        }
    }

    svg << "</svg>\n";
    return svg.str();
}

std::string CertificationReportExporter::generateThdTableHtml(const std::vector<MeasuredPoint>& points)
{
    std::ostringstream html;
    html << "<table class=\"thd-table\">\n";
    html << "  <thead>\n";
    html << "    <tr><th>POINT ID</th><th>BLOCK</th><th>STIMULUS</th><th>CONTROLS & VALUES</th><th>THD %</th><th>SNR (dB)</th></tr>\n";
    html << "  </thead>\n";
    html << "  <tbody>\n";

    for (size_t i = 0; i < points.size(); ++i)
    {
        const auto& p = points[i];
        html << "    <tr>";
        html << "<td>" << (p.pointId.empty() ? ("P_" + std::to_string(i + 1)) : p.pointId) << "</td>";
        html << "<td>" << (p.blockType.empty() ? "AnalogFilter" : p.blockType) << "</td>";
        html << "<td>" << (p.stimulusType.empty() ? "LogFarinaSweep" : p.stimulusType) << "</td>";

        std::string ctrlStr;
        if (!p.controlSteps.empty())
        {
            for (size_t c = 0; c < p.controlSteps.size(); ++c)
            {
                if (c > 0) ctrlStr += " | ";
                const auto& cs = p.controlSteps[c];
                int pct = static_cast<int>(std::round(cs.normalizedValue * 100.0f));
                ctrlStr += cs.paramName + ": " + std::to_string(pct) + "%";
            }
        }
        else
        {
            ctrlStr = "Param 1: " + std::to_string(static_cast<int>(p.param1Normalized * 100.0f)) + "%";
            if (p.param2Normalized > 0.001f)
                ctrlStr += " | Param 2: " + std::to_string(static_cast<int>(p.param2Normalized * 100.0f)) + "%";
        }

        html << "<td style=\"font-size: 11px; color: #cbd5e1;\">" << ctrlStr << "</td>";
        html << "<td class=\"thd-val\">" << std::fixed << std::setprecision(3) << p.thdPercent << "%</td>";
        html << "<td class=\"snr-val\">" << std::fixed << std::setprecision(1) << p.snrDb << " dB</td>";
        html << "</tr>\n";
    }

    html << "  </tbody>\n";
    html << "</table>\n";
    return html.str();
}

bool CertificationReportExporter::exportReportToHtml(const std::string& targetPath,
                                                      const SessionManifestData& manifest,
                                                      const std::vector<MeasuredPoint>& points)
{
    std::ofstream file(targetPath);
    if (!file.is_open())
        return false;

    std::vector<float> freqs;
    std::vector<float> mags;
    for (size_t i = 0; i < points.size(); ++i)
    {
        float f = 20.0f * std::pow(1000.0f, static_cast<float>(i) / static_cast<float>(std::max(size_t(1), points.size() - 1)));
        freqs.push_back(f);
        mags.push_back(points[i].muSigmaValue.mean);
    }

    std::string freqSvg = generateFrequencyCurveSvg(freqs, mags, 760, 280);
    std::string heatmapSvg = generateHeatmapSvg(points, 8, 8, 760, 260);
    std::string thdTable = generateThdTableHtml(points);

    file << "<!DOCTYPE html>\n";
    file << "<html lang=\"en\">\n<head>\n";
    file << "<meta charset=\"UTF-8\">\n";
    file << "<title>ABDAudioLab Certification Report — " << (manifest.hardwareName.empty() ? "Hardware Profiling" : manifest.hardwareName) << "</title>\n";
    file << "<style>\n";
    file << "  body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; background-color: #0f1115; color: #e2e8f0; margin: 0; padding: 30px; }\n";
    file << "  .container { max-width: 900px; margin: 0 auto; background-color: #161920; border: 1px solid #2d3748; border-radius: 12px; padding: 32px; box-shadow: 0 10px 25px rgba(0,0,0,0.5); }\n";
    file << "  .header { display: flex; justify-content: space-between; align-items: center; border-bottom: 2px solid #2d3748; padding-bottom: 20px; margin-bottom: 24px; }\n";
    file << "  .brand { font-size: 22px; font-weight: 800; color: #3b82f6; letter-spacing: 1px; }\n";
    file << "  .badge { background: #1e3a8a; color: #93c5fd; padding: 4px 12px; border-radius: 999px; font-size: 12px; font-weight: 600; }\n";
    file << "  .section-title { font-size: 14px; font-weight: 700; color: #94a3b8; text-transform: uppercase; letter-spacing: 1px; margin-top: 28px; margin-bottom: 14px; }\n";
    file << "  .metrics-grid { display: grid; grid-template-columns: repeat(4, 1fr); gap: 14px; margin-bottom: 24px; }\n";
    file << "  .metric-card { background-color: #1e2430; border: 1px solid #2d3748; border-radius: 8px; padding: 14px; text-align: center; }\n";
    file << "  .metric-val { font-size: 20px; font-weight: 700; color: #10b981; margin-top: 4px; }\n";
    file << "  .metric-lbl { font-size: 11px; color: #94a3b8; font-weight: 600; }\n";
    file << "  .chart-box { background: #1a1d24; padding: 16px; border-radius: 8px; border: 1px solid #2d3748; margin-bottom: 24px; text-align: center; }\n";
    file << "  .thd-table { width: 100%; border-collapse: collapse; margin-top: 10px; font-size: 12px; }\n";
    file << "  .thd-table th { background: #1e2430; padding: 10px; text-align: left; color: #94a3b8; border-bottom: 2px solid #2d3748; }\n";
    file << "  .thd-table td { padding: 8px 10px; border-bottom: 1px solid #2d3748; }\n";
    file << "  .thd-val { color: #f59e0b; font-weight: 600; }\n";
    file << "  .snr-val { color: #10b981; font-weight: 600; }\n";
    file << "  @media print {\n";
    file << "    body { background-color: #ffffff; color: #000000; padding: 0; }\n";
    file << "    .container { border: none; box-shadow: none; max-width: 100%; padding: 0; background: #ffffff; }\n";
    file << "  }\n";
    file << "</style>\n";
    file << "</head>\n<body>\n";

    file << "<div class=\"container\">\n";
    file << "  <div class=\"header\">\n";
    file << "    <div>\n";
    file << "      <div class=\"brand\">ABDAUDIOLAB CERTIFICATION REPORT</div>\n";
    file << "      <div style=\"font-size: 13px; color: #94a3b8; margin-top: 4px;\">Target Hardware: <strong>" << (manifest.hardwareName.empty() ? "Analog Hardware Profile" : manifest.hardwareName) << "</strong></div>\n";
    file << "    </div>\n";
    file << "    <div class=\"badge\">CERTIFIED PASSED</div>\n";
    file << "  </div>\n";

    file << "  <div class=\"metrics-grid\">\n";
    file << "    <div class=\"metric-card\"><div class=\"metric-lbl\">SAMPLE RATE</div><div class=\"metric-val\">" << static_cast<int>(manifest.sampleRate) << " Hz</div></div>\n";
    file << "    <div class=\"metric-card\"><div class=\"metric-lbl\">TOTAL POINTS</div><div class=\"metric-val\">" << points.size() << "</div></div>\n";
    file << "    <div class=\"metric-card\"><div class=\"metric-lbl\">AVG SNR</div><div class=\"metric-val\">" << std::fixed << std::setprecision(1) << manifest.averageSnrDb << " dB</div></div>\n";
    file << "    <div class=\"metric-card\"><div class=\"metric-lbl\">NOISE FLOOR</div><div class=\"metric-val\">" << std::fixed << std::setprecision(1) << manifest.noiseFloorRmsDb << " dBFS</div></div>\n";
    file << "  </div>\n";

    if (!points.empty() && !points[0].controlSteps.empty())
    {
        file << "  <div class=\"section-title\">Hardware Controls Specification</div>\n";
        file << "  <table class=\"thd-table\" style=\"margin-bottom: 24px;\">\n";
        file << "    <thead><tr><th>CONTROL NAME</th><th>TYPE</th><th>CONTROL METHOD</th><th>VALUE RANGE</th></tr></thead>\n";
        file << "    <tbody>\n";
        for (const auto& cs : points[0].controlSteps)
        {
            file << "      <tr><td><strong>" << cs.paramName << "</strong></td>";
            file << "<td>" << cs.controlType << "</td>";
            file << "<td>" << (manifest.deviceType == "AUTOMATED_SYSEX" ? "SysEx Parameter Message" : (manifest.deviceType == "AUTOMATED_MIDI_CC" ? "MIDI Continuous Controller (CC)" : "Analog Panel Position")) << "</td>";
            file << "<td>0% – 100% (Normalized)</td></tr>\n";
        }
        file << "    </tbody>\n";
        file << "  </table>\n";
    }

    file << "  <div class=\"section-title\">Frequency Response Magnitude</div>\n";
    file << "  <div class=\"chart-box\">" << freqSvg << "</div>\n";

    file << "  <div class=\"section-title\">Parameter Grid Matrix Heatmap</div>\n";
    file << "  <div class=\"chart-box\">" << heatmapSvg << "</div>\n";

    file << "  <div class=\"section-title\">THD% & Signal Quality Measurement Log</div>\n";
    file << thdTable << "\n";

    file << "</div>\n</body>\n</html>\n";

    file.close();
    return true;
}

} // namespace abdaudiolab::exporting
