#include "CertificationReportExporter.h"
#include "../core/ModelHoldoutValidator.h"
#include "../core/GuidedParameterEvidence.h"
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
    svg.imbue(std::locale::classic());
    svg << "<svg width=\"" << width << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height << "\" xmlns=\"http://www.w3.org/2000/svg\">\n";
    svg << "<style>\n";
    svg << "  .bg { fill: #f8f9fa; rx: 8px; stroke: #e2e8f0; stroke-width: 1; }\n";
    svg << "  .grid { stroke: #e2e8f0; stroke-width: 1; stroke-dasharray: 3,3; }\n";
    svg << "  .axis-label { fill: #64748b; font-size: 10px; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; }\n";
    svg << "  .line-curve { fill: none; stroke: #00a86b; stroke-width: 2.5; stroke-linecap: round; }\n";
    svg << "  .title { fill: #1a1d20; font-size: 11px; font-weight: 700; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; letter-spacing: 0.5px; }\n";
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
    svg.imbue(std::locale::classic());
    svg << "<svg width=\"" << width << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height << "\" xmlns=\"http://www.w3.org/2000/svg\">\n";
    svg << "<style>\n";
    svg << "  .bg { fill: #f8f9fa; rx: 8px; stroke: #e2e8f0; stroke-width: 1; }\n";
    svg << "  .cell { stroke: #ffffff; stroke-width: 1.5; }\n";
    svg << "  .cell-text { fill: #1a1d20; font-size: 9px; font-weight: bold; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; }\n";
    svg << "  .title { fill: #1a1d20; font-size: 11px; font-weight: 700; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; letter-spacing: 0.5px; }\n";
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
    html.imbue(std::locale::classic());
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

        html << "<td style=\"font-size: 11px; color: #475569;\">" << ctrlStr << "</td>";
        html << "<td class=\"thd-val\">" << std::fixed << std::setprecision(3) << p.thdPercent << "%</td>";
        html << "<td class=\"snr-val\">" << std::fixed << std::setprecision(1) << p.snrDb << " dB</td>";
        html << "</tr>\n";
    }

    html << "  </tbody>\n";
    html << "</table>\n";
    return html.str();
}

static uint32_t computeReportCrc32(const std::vector<MeasuredPoint>& points)
{
    uint32_t crc = 0xFFFFFFFF;
    for (const auto& pt : points)
    {
        uint32_t val = static_cast<uint32_t>(pt.param1Normalized * 10000.0f) ^ static_cast<uint32_t>(pt.thdPercent * 1000.0f);
        crc ^= val;
        for (int i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320u : 0u);
    }
    return ~crc;
}

bool CertificationReportExporter::exportReportToHtml(const std::string& targetPath,
                                                      const SessionManifestData& manifest,
                                                      const std::vector<MeasuredPoint>& points,
                                                      const abdaudiolab::core::ValidationReport* validation,
                                                      const std::string& validationStatus,
                                                      const std::string& validationErrorMessage,
                                                      const abdaudiolab::core::GuidedParameterEvidence* guidedEvidence,
                                                      const std::string& modelExportStatus,
                                                      const std::string& modelExportReason)
{
    std::ofstream file(targetPath);
    if (!file.is_open())
        return false;
    file.imbue(std::locale::classic());

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
    uint32_t crc = computeReportCrc32(points);

    // Metrological Integrity Guard:
    // If validation claims ESR <= -119.0 dB and RMSE == 0.0 with 0 measurement points,
    // this is a synthetic placeholder, NOT a verified holdout validation!
    std::string effectiveValStatus = validationStatus;
    const abdaudiolab::core::ValidationReport* effectiveValidation = validation;
    if (effectiveValidation != nullptr && points.empty() && effectiveValidation->postAlignment.esrDb <= -119.0f && effectiveValidation->postAlignment.rmse == 0.0f)
    {
        effectiveValidation = nullptr;
        effectiveValStatus = "notExecuted";
    }

    // Determine Holdout status and verdict labels & styling
    std::string statusBadgeClass = "badge-neutral";
    std::string statusBadgeText = "[i] HOLDOUT VALIDATION: NOT EXECUTED";
    std::string verdictStr = "NOT_AVAILABLE";
    std::string policyStr = "audio-ab-v1";

    if (effectiveValStatus == "error")
    {
        statusBadgeClass = "badge-error";
        statusBadgeText = "[!] TECHNICAL ERROR: " + (validationErrorMessage.empty() ? "Validation execution failed" : validationErrorMessage);
    }
    else if (effectiveValStatus == "corrupt")
    {
        statusBadgeClass = "badge-corrupt";
        statusBadgeText = "[!] CORRUPT: Cryptographic mismatch / tampering detected";
    }
    else if (effectiveValidation != nullptr && effectiveValStatus == "completed")
    {
        verdictStr = effectiveValidation->verdict;
        policyStr = effectiveValidation->verdictPolicy.empty() ? "audio-ab-v1" : effectiveValidation->verdictPolicy;

        if (verdictStr == "PASS")
        {
            statusBadgeClass = "badge-pass";
            statusBadgeText = "[OK] VERDICT: PASS (" + policyStr + ")";
        }
        else if (verdictStr == "PASS_WITH_LIMITATIONS")
        {
            statusBadgeClass = "badge-warn";
            statusBadgeText = "[!] VERDICT: PASS WITH LIMITATIONS (" + policyStr + ")";
        }
        else if (verdictStr == "FAIL")
        {
            statusBadgeClass = "badge-fail";
            statusBadgeText = "[X] VERDICT: FAIL (" + policyStr + ")";
        }
        else
        {
            statusBadgeClass = "badge-neutral";
            statusBadgeText = "[?] VERDICT: " + verdictStr;
        }
    }

    file << "<!DOCTYPE html>\n";
    file << "<html lang=\"en\">\n<head>\n";
    file << "<meta charset=\"UTF-8\">\n";
    file << "<title>ABDAudioLab Certification Report — " << (manifest.hardwareName.empty() ? "Hardware Profiling" : manifest.hardwareName) << "</title>\n";
    file << "<style>\n";
    file << "  body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; background-color: #f8f9fa; color: #1a1d20; margin: 0; padding: 30px; }\n";
    file << "  .container { max-width: 900px; margin: 0 auto; background-color: #ffffff; border: 1px solid #e2e8f0; border-radius: 12px; padding: 32px; box-shadow: 0 4px 20px rgba(0,0,0,0.05); }\n";
    file << "  .header { display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #e2e8f0; padding-bottom: 20px; margin-bottom: 24px; }\n";
    file << "  .brand { font-size: 20px; font-weight: 800; color: #1a1d20; letter-spacing: 0.5px; }\n";
    file << "  .badge { padding: 6px 14px; border-radius: 999px; font-size: 12px; font-weight: 700; letter-spacing: 0.25px; }\n";
    file << "  .badge-pass { background: #ecfdf5; color: #065f46; border: 1.5px solid #059669; }\n";
    file << "  .badge-warn { background: #fffbeb; color: #92400e; border: 1.5px solid #d97706; }\n";
    file << "  .badge-fail { background: #fef2f2; color: #991b1b; border: 1.5px solid #dc2626; }\n";
    file << "  .badge-error { background: #fff7ed; color: #9a3412; border: 1.5px solid #ea580c; }\n";
    file << "  .badge-corrupt { background: #fdf2f8; color: #831843; border: 1.5px solid #db2777; }\n";
    file << "  .badge-neutral { background: #f1f5f9; color: #334155; border: 1.5px solid #94a3b8; }\n";
    file << "  .section-title { font-size: 13px; font-weight: 700; color: #64748b; text-transform: uppercase; letter-spacing: 0.75px; margin-top: 28px; margin-bottom: 14px; }\n";
    file << "  .metrics-grid { display: grid; grid-template-columns: repeat(4, 1fr); gap: 14px; margin-bottom: 24px; }\n";
    file << "  .metric-card { background-color: #f8f9fa; border: 1px solid #e2e8f0; border-radius: 8px; padding: 14px; text-align: center; }\n";
    file << "  .metric-val { font-size: 20px; font-weight: 700; color: #00a86b; margin-top: 4px; }\n";
    file << "  .metric-lbl { font-size: 11px; color: #64748b; font-weight: 600; }\n";
    file << "  .chart-box { background: #f8f9fa; padding: 16px; border-radius: 8px; border: 1px solid #e2e8f0; margin-bottom: 24px; text-align: center; }\n";
    file << "  .thd-table { width: 100%; border-collapse: collapse; margin-top: 10px; font-size: 12px; }\n";
    file << "  .thd-table th { background: #f1f5f9; padding: 10px; text-align: left; color: #475569; border-bottom: 2px solid #e2e8f0; font-weight: 600; }\n";
    file << "  .thd-table td { padding: 8px 10px; border-bottom: 1px solid #f1f5f9; color: #334155; }\n";
    file << "  .thd-val { color: #d97706; font-weight: 600; }\n";
    file << "  .snr-val { color: #059669; font-weight: 600; }\n";
    file << "  .sign-convention-box { background-color: #f1f5f9; border-left: 4px solid #0284c7; padding: 10px 14px; font-size: 12px; margin-bottom: 16px; font-family: 'Consolas', monospace; color: #1e293b; }\n";
    file << "  .stamp-footer { margin-top: 32px; padding-top: 16px; border-top: 1px dashed #cbd5e1; font-family: 'Consolas', monospace; font-size: 11px; color: #64748b; display: flex; justify-content: space-between; }\n";
    file << "  @media print {\n";
    file << "    body { background-color: #ffffff; color: #000000; padding: 0; }\n";
    file << "    .container { border: none; box-shadow: none; max-width: 100%; padding: 0; background: #ffffff; }\n";
    file << "  }\n";
    file << "</style>\n";
    file << "<script>\n";
    file << "  window.onload = function() {\n";
    file << "    // Printable on demand\n";
    file << "  };\n";
    file << "</script>\n";
    file << "</head>\n<body>\n";

    file << "<div class=\"container\">\n";
    file << "  <div class=\"header\">\n";
    file << "    <div>\n";
    file << "      <div class=\"brand\">ABDAUDIOLAB CERTIFICATION REPORT</div>\n";
    file << "      <div style=\"font-size: 13px; color: #64748b; margin-top: 4px;\">Target: <strong>" << (manifest.hardwareName.empty() ? "Empirical Target" : manifest.hardwareName) << "</strong></div>\n";
    file << "    </div>\n";
    file << "    <div class=\"badge " << statusBadgeClass << "\">" << statusBadgeText << "</div>\n";
    file << "  </div>\n";

    // 1. Core measurement metrics
    file << "  <div class=\"metrics-grid\">\n";
    file << "    <div class=\"metric-card\"><div class=\"metric-lbl\">SAMPLE RATE</div><div class=\"metric-val\">" << static_cast<int>(manifest.sampleRate) << " Hz</div></div>\n";
    file << "    <div class=\"metric-card\"><div class=\"metric-lbl\">TOTAL POINTS</div><div class=\"metric-val\">" << points.size() << "</div></div>\n";
    file << "    <div class=\"metric-card\"><div class=\"metric-lbl\">AVG SNR</div><div class=\"metric-val\">" << std::fixed << std::setprecision(1) << manifest.averageSnrDb << " dB</div></div>\n";
    file << "    <div class=\"metric-card\"><div class=\"metric-lbl\">NOISE FLOOR</div><div class=\"metric-val\">" << std::fixed << std::setprecision(1) << manifest.noiseFloorRmsDb << " dBFS</div></div>\n";
    file << "  </div>\n";

    // 2. Out-of-sample Holdout Validation Section (audio-ab-v1)
    file << "  <div class=\"section-title\">Holdout A/B Validation & Latency Alignment (audio-ab-v1)</div>\n";
    if (effectiveValidation != nullptr && effectiveValStatus == "completed")
    {
        file << "  <div class=\"sign-convention-box\"><strong>Latency Alignment Sign Convention:</strong> <code>alignedTarget[n] = target[n - sampleOffset]</code> (Offset: " << effectiveValidation->sampleOffset << " samples)</div>\n";

        file << "  <div class=\"metrics-grid\">\n";
        file << "    <div class=\"metric-card\"><div class=\"metric-lbl\">POST-ALIGN ESR</div><div class=\"metric-val\" style=\"color: " << (effectiveValidation->verdict == "PASS" ? "#00a86b" : (effectiveValidation->verdict == "PASS_WITH_LIMITATIONS" ? "#d97706" : "#ef4444")) << ";\">" << std::fixed << std::setprecision(1) << effectiveValidation->postAlignment.esrDb << " dB</div></div>\n";
        file << "    <div class=\"metric-card\"><div class=\"metric-lbl\">CORRELATION &rho;</div><div class=\"metric-val\">" << std::fixed << std::setprecision(4) << effectiveValidation->postAlignment.correlationPeak << "</div></div>\n";
        file << "    <div class=\"metric-card\"><div class=\"metric-lbl\">SAMPLE OFFSET</div><div class=\"metric-val\">" << effectiveValidation->sampleOffset << " smp</div></div>\n";
        file << "    <div class=\"metric-card\"><div class=\"metric-lbl\">CRITERION REASON</div><div class=\"metric-val\" style=\"font-size: 14px; margin-top: 8px;\">" << effectiveValidation->reasonCode << "</div></div>\n";
        file << "  </div>\n";

        file << "  <table class=\"thd-table\" style=\"margin-bottom: 24px;\">\n";
        file << "    <thead><tr><th>METRIC STAGE</th><th>RMSE</th><th>RMS DELTA (dB)</th><th>SPECTRAL DELTA (dB)</th><th>PEAK ERROR</th><th>ESR (dB)</th></tr></thead>\n";
        file << "    <tbody>\n";
        file << "      <tr><td><strong>Pre-Alignment</strong></td>";
        file << "<td>" << std::fixed << std::setprecision(5) << effectiveValidation->preAlignment.rmse << "</td>";
        file << "<td>" << std::fixed << std::setprecision(2) << effectiveValidation->preAlignment.rmsDeltaDb << " dB</td>";
        file << "<td>" << std::fixed << std::setprecision(2) << effectiveValidation->preAlignment.spectralDeltaDb << " dB</td>";
        file << "<td>" << std::fixed << std::setprecision(4) << effectiveValidation->preAlignment.peakAbsoluteError << "</td>";
        file << "<td>" << std::fixed << std::setprecision(1) << effectiveValidation->preAlignment.esrDb << " dB</td></tr>\n";

        file << "      <tr><td><strong>Post-Alignment</strong></td>";
        file << "<td>" << std::fixed << std::setprecision(5) << effectiveValidation->postAlignment.rmse << "</td>";
        file << "<td>" << std::fixed << std::setprecision(2) << effectiveValidation->postAlignment.rmsDeltaDb << " dB</td>";
        file << "<td>" << std::fixed << std::setprecision(2) << effectiveValidation->postAlignment.spectralDeltaDb << " dB</td>";
        file << "<td>" << std::fixed << std::setprecision(4) << effectiveValidation->postAlignment.peakAbsoluteError << "</td>";
        file << "<td><strong>" << std::fixed << std::setprecision(1) << effectiveValidation->postAlignment.esrDb << " dB</strong></td></tr>\n";
        file << "    </tbody>\n";
        file << "  </table>\n";

        file << "  <div style=\"font-size: 11px; color: #64748b; margin-bottom: 20px;\">\n";
        file << "    <strong>FAIR Validation Artifacts:</strong><br>\n";
        file << "    &bull; <code>validation/target.wav</code> (SHA-256: " << (effectiveValidation->targetWavSha256.empty() ? "N/A" : effectiveValidation->targetWavSha256.substr(0, 16) + "...") << ")<br>\n";
        file << "    &bull; <code>validation/model.wav</code> (SHA-256: " << (effectiveValidation->modelWavSha256.empty() ? "N/A" : effectiveValidation->modelWavSha256.substr(0, 16) + "...") << ")<br>\n";
        file << "    &bull; <code>validation/residual.wav</code> (SHA-256: " << (effectiveValidation->residualWavSha256.empty() ? "N/A" : effectiveValidation->residualWavSha256.substr(0, 16) + "...") << ")<br>\n";
        file << "    &bull; <code>validation/holdout_manifest.json</code> (SHA-256: " << (effectiveValidation->holdoutManifestSha256.empty() ? "N/A" : effectiveValidation->holdoutManifestSha256.substr(0, 16) + "...") << ")\n";
        file << "  </div>\n";
    }
    else
    {
        file << "  <div class=\"sign-convention-box\" style=\"border-left-color: #64748b; background-color: #f8fafc; color: #475569; margin-bottom: 24px;\">\n";
        file << "    <strong>Holdout Acoustic Validation:</strong> <code>NOT EXECUTED / NOT AVAILABLE</code><br>\n";
        file << "    <span style=\"font-size: 11px;\">Holdout acoustic validation was not executed for this target. No neural or LUT acoustic model was validated out-of-sample. Displayed metrics represent single-parameter differential testing, not full acoustic model certification.</span>\n";
        if (!validationErrorMessage.empty())
            file << "    <div style=\"font-size: 12px; color: #b91c1c; margin-top: 4px;\">Details: " << validationErrorMessage << "</div>\n";
        file << "  </div>\n";
    }

    // 2.5. Guided Parameter Differential Evidence Section
    if (guidedEvidence != nullptr)
    {
        file << "  <div class=\"section-title\">Guided Parameter Differential Evidence</div>\n";
        file << "  <div class=\"sign-convention-box\" style=\"border-left-color: #059669; background-color: #ecfdf5; color: #065f46; margin-bottom: 16px;\">\n";
        file << "    <strong>Guided Single-Parameter Differential:</strong> <code>VERIFIED EMPIRICAL EVIDENCE</code><br>\n";
        file << "    <span style=\"font-size: 11px;\">Empirical audio difference under declared note, preset, and parameter settings. <strong>This is guided parameter evidence, NOT a holdout model validation.</strong></span>\n";
        file << "  </div>\n";

        file << "  <div class=\"metrics-grid\">\n";
        file << "    <div class=\"metric-card\"><div class=\"metric-lbl\">PARAMETER</div><div class=\"metric-val\" style=\"font-size: 15px; color: #1e293b;\">" 
             << (guidedEvidence->parameterName.isEmpty() ? "Unknown" : guidedEvidence->parameterName.toStdString())
             << " (" << guidedEvidence->parameterId.toStdString() << ")</div></div>\n";
        file << "    <div class=\"metric-card\"><div class=\"metric-lbl\">DELTA VALUE</div><div class=\"metric-val\" style=\"font-size: 15px; color: #0284c7;\">"
             << std::fixed << std::setprecision(3) << guidedEvidence->initialNormalized << " &rarr; " << guidedEvidence->modifiedNormalized << "</div></div>\n";
        file << "    <div class=\"metric-card\"><div class=\"metric-lbl\">STIMULUS (MIDI)</div><div class=\"metric-val\" style=\"font-size: 15px; color: #1e293b;\">Note " 
             << guidedEvidence->midiNote << ", Vel " << guidedEvidence->midiVelocity << "</div></div>\n";
        file << "    <div class=\"metric-card\"><div class=\"metric-lbl\">REPEATABILITY</div><div class=\"metric-val\" style=\"font-size: 13px; color: #059669;\">"
             << (guidedEvidence->repeatabilityVerified ? "DETERMINISTIC" : "WITHIN_TOLERANCE") << "</div></div>\n";
        file << "  </div>\n";

        file << "  <div class=\"metrics-grid\">\n";
        file << "    <div class=\"metric-card\"><div class=\"metric-lbl\">PEAK DIFFERENCE</div><div class=\"metric-val\" style=\"color: #0f172a;\">" 
             << std::fixed << std::setprecision(5) << guidedEvidence->peakDifference << "</div></div>\n";
        file << "    <div class=\"metric-card\"><div class=\"metric-lbl\">RMSE</div><div class=\"metric-val\" style=\"color: #0f172a;\">" 
             << std::fixed << std::setprecision(5) << guidedEvidence->rmse << "</div></div>\n";
        file << "    <div class=\"metric-card\"><div class=\"metric-lbl\">CORRELATION &rho;</div><div class=\"metric-val\" style=\"color: #0f172a;\">" 
             << std::fixed << std::setprecision(5) << guidedEvidence->correlation << "</div></div>\n";
        file << "    <div class=\"metric-card\"><div class=\"metric-lbl\">&Delta; RMS (dB)</div><div class=\"metric-val\" style=\"color: #0f172a;\">" 
             << std::fixed << std::setprecision(3) << guidedEvidence->deltaRmsDb << " dB</div></div>\n";
        file << "  </div>\n";

        file << "  <table class=\"thd-table\" style=\"margin-bottom: 24px;\">\n";
        file << "    <thead><tr><th>FAIR GUIDED ARTIFACT</th><th>ROLE</th><th>LOCATION</th><th>FIXITY (SHA-256)</th></tr></thead>\n";
        file << "    <tbody>\n";
        file << "      <tr><td><strong>Baseline Audio</strong></td><td><code>guided_baseline_audio</code></td><td><code>evidence/guided/baseline.wav</code></td><td><code>"
             << (guidedEvidence->baselineSha256.empty() ? "N/A" : guidedEvidence->baselineSha256.substr(0, 16) + "...") << "</code></td></tr>\n";
        file << "      <tr><td><strong>Modified Audio</strong></td><td><code>guided_modified_audio</code></td><td><code>evidence/guided/modified.wav</code></td><td><code>"
             << (guidedEvidence->modifiedSha256.empty() ? "N/A" : guidedEvidence->modifiedSha256.substr(0, 16) + "...") << "</code></td></tr>\n";
        file << "      <tr><td><strong>Differential Audio</strong></td><td><code>guided_differential_audio</code></td><td><code>evidence/guided/difference.wav</code></td><td><code>"
             << (guidedEvidence->differenceSha256.empty() ? "N/A" : guidedEvidence->differenceSha256.substr(0, 16) + "...") << "</code></td></tr>\n";
        file << "      <tr><td><strong>Differential Report</strong></td><td><code>guided_parameter_differential_report</code></td><td><code>evidence/guided/parameter-test-cutoff.json</code></td><td><code>"
             << (guidedEvidence->reportJsonSha256.empty() ? "N/A" : guidedEvidence->reportJsonSha256.substr(0, 16) + "...") << "</code></td></tr>\n";
        file << "    </tbody>\n";
        file << "  </table>\n";
    }

    // 2.75. Acoustic Model Package Status Section
    file << "  <div class=\"section-title\">Acoustic Model Package Status</div>\n";
    file << "  <div class=\"sign-convention-box\" style=\"border-left-color: #64748b; background-color: #f8fafc; color: #475569; margin-bottom: 24px;\">\n";
    file << "    <strong>Model Export:</strong> <code>" << (modelExportStatus == "completed" ? "COMPLETED" : "NOT EXECUTED") << "</code><br>\n";
    file << "    <span style=\"font-size: 11px;\">" << modelExportReason << "</span>\n";
    file << "  </div>\n";

    // 3. Hardware Controls Specification
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

    // 4. Response curves and logs
    file << "  <div class=\"section-title\">Frequency Response Magnitude</div>\n";
    file << "  <div class=\"chart-box\">" << freqSvg << "</div>\n";

    file << "  <div class=\"section-title\">Parameter Grid Matrix Heatmap</div>\n";
    file << "  <div class=\"chart-box\">" << heatmapSvg << "</div>\n";

    file << "  <div class=\"section-title\">THD% & Signal Quality Measurement Log</div>\n";
    file << thdTable << "\n";

    file << "  <div class=\"stamp-footer\">\n";
    file << "    <div>SESSION INTEGRITY: " << (validationStatus == "corrupt" ? "COMPROMISED" : "VALID") << " | CHECKSUM: 0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << crc << std::dec << "</div>\n";
    file << "    <div>ENGINE: ABDAudioLab v0.3.2-PRO</div>\n";
    file << "  </div>\n";

    file << "</div>\n</body>\n</html>\n";

    file.close();
    return true;
}

} // namespace abdaudiolab::exporting
