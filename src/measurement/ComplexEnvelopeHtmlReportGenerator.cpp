/**
 * @file ComplexEnvelopeHtmlReportGenerator.cpp
 * @brief Implementation of offline-safe interactive HTML report generator for complex envelopes.
 * @author ABDSynths
 * @date 2026
 */

#include "ComplexEnvelopeHtmlReportGenerator.h"
#include "ComplexEnvelopeSvgRenderer.h"
#include <sstream>
#include <iomanip>

namespace abdaudiolab::measurement
{

std::string ComplexEnvelopeHtmlReportGenerator::generateInteractiveReportHtml(
    const ComplexEnvelopeOrchestrationResult& result,
    const ComplexEnvelopeExportSpec& spec,
    const std::vector<EnvelopeStageDescriptor>& nativeStages,
    const std::string& relRawAudioPath,
    const std::string& relCompAudioPath) noexcept
{
    std::ostringstream ss;

    // Render multi-domain SVGs inline
    ComplexEnvelopeSvgRenderer::RenderOptions svgOpts;
    svgOpts.width = 920;
    svgOpts.height = 420;

    std::string svgTimbre = ComplexEnvelopeSvgRenderer::renderOverlaySvg(
        result.captureRecord, result.comparisons, nativeStages, EnvelopeDomain::Timbre, svgOpts);

    std::string svgPitch = ComplexEnvelopeSvgRenderer::renderOverlaySvg(
        result.captureRecord, result.comparisons, nativeStages, EnvelopeDomain::Pitch, svgOpts);

    std::string svgAmp = ComplexEnvelopeSvgRenderer::renderOverlaySvg(
        result.captureRecord, result.comparisons, nativeStages, EnvelopeDomain::Amplitude, svgOpts);

    // HTML Skeleton
    ss << "<!DOCTYPE html>\n"
       << "<html lang=\"en\">\n"
       << "<head>\n"
       << "  <meta charset=\"UTF-8\">\n"
       << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
       << "  <meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; img-src data:;\">\n"
       << "  <title>ABDAudioLab - Complex Envelope Observable Report</title>\n"
       << "  <style>\n"
       << "    :root {\n"
       << "      --bg-dark: #090d16;\n"
       << "      --panel-bg: #0f172a;\n"
       << "      --card-bg: #1e293b;\n"
       << "      --text-main: #f8fafc;\n"
       << "      --text-muted: #94a3b8;\n"
       << "      --border-color: #334155;\n"
       << "      --primary: #38bdf8;\n"
       << "      --accent: #f59e0b;\n"
       << "      --success: #10b981;\n"
       << "      --danger: #ef4444;\n"
       << "    }\n"
       << "    * { box-sizing: border-box; margin: 0; padding: 0; }\n"
       << "    body { font-family: system-ui, -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;\n"
       << "           background: var(--bg-dark); color: var(--text-main); line-height: 1.5; padding: 24px; }\n"
       << "    .container { max-width: 1060px; margin: 0 auto; }\n"
       << "    header { margin-bottom: 24px; border-bottom: 1px solid var(--border-color); padding-bottom: 16px; }\n"
       << "    h1 { font-size: 24px; font-weight: 700; color: var(--text-main); margin-bottom: 6px; }\n"
       << "    .badge-bar { display: flex; gap: 8px; flex-wrap: wrap; margin-top: 8px; }\n"
       << "    .badge { font-size: 12px; font-weight: 600; padding: 3px 8px; border-radius: 4px; border: 1px solid; }\n"
       << "    .badge-success { background: rgba(16,185,129,0.1); color: var(--success); border-color: rgba(16,185,129,0.3); }\n"
       << "    .badge-warning { background: rgba(245,158,11,0.1); color: var(--accent); border-color: rgba(245,158,11,0.3); }\n"
       << "    .badge-primary { background: rgba(56,189,248,0.1); color: var(--primary); border-color: rgba(56,189,248,0.3); }\n"
       << "    .card { background: var(--panel-bg); border: 1px solid var(--border-color); border-radius: 8px; padding: 20px; margin-bottom: 24px; }\n"
       << "    .tabs { display: flex; gap: 4px; border-bottom: 1px solid var(--border-color); margin-bottom: 16px; }\n"
       << "    .tab-btn { background: none; border: none; padding: 10px 18px; color: var(--text-muted); font-size: 13px; font-weight: 600; cursor: pointer; border-bottom: 2px solid transparent; }\n"
       << "    .tab-btn.active { color: var(--primary); border-bottom-color: var(--primary); }\n"
       << "    .tab-content { display: none; }\n"
       << "    .tab-content.active { display: block; }\n"
       << "    .svg-wrapper { background: #090d16; border: 1px solid var(--border-color); border-radius: 6px; padding: 12px; display: flex; justify-content: center; overflow-x: auto; }\n"
       << "    .grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; }\n"
       << "    table { width: 100%; border-collapse: collapse; font-size: 12px; margin-top: 10px; }\n"
       << "    th, td { padding: 8px 12px; text-align: left; border-bottom: 1px solid var(--border-color); }\n"
       << "    th { color: var(--text-muted); font-weight: 600; text-transform: uppercase; letter-spacing: 0.5px; }\n"
       << "    .metrology-box { background: rgba(56,189,248,0.05); border-left: 4px solid var(--primary); padding: 14px 18px; border-radius: 0 6px 6px 0; margin-bottom: 24px; }\n"
       << "    .metrology-box p { font-size: 13px; color: #cbd5e1; }\n"
       << "    .audio-bar { display: flex; gap: 20px; align-items: center; margin-top: 12px; padding: 12px; background: var(--card-bg); border-radius: 6px; }\n"
       << "    audio { height: 32px; }\n"
       << "  </style>\n"
       << "</head>\n"
       << "<body>\n"
       << "<div class=\"container\">\n"
       << "  <header>\n"
       << "    <h1>Complex Envelope Observable Measurement Report</h1>\n"
       << "    <p style=\"color:var(--text-muted);font-size:13px;\">Experiment ID: " << spec.experimentId
       << " | Creation Clock: " << (spec.timestampPolicy == TimestampPolicy::FixedForTest ? spec.fixedTimestamp : spec.creationClock)
       << " | Author: " << spec.author << "</p>\n"
       << "    <div class=\"badge-bar\">\n"
       << "      <span class=\"badge badge-success\">Status: " << result.status << "</span>\n"
       << "      <span class=\"badge badge-primary\">observable_agreement</span>\n"
       << "      <span class=\"badge badge-warning\">phaseDistortionProxy: not_claimed</span>\n"
       << "    </div>\n"
       << "  </header>\n";

    // Metrological Notice Box
    ss << "  <div class=\"metrology-box\">\n"
       << "    <strong style=\"color:var(--text-main);\">Honest Metrological Label:</strong>\n"
       << "    <p>Acoustic spectral centroid and high-frequency rolloff serve as physically observable proxies for timbre evolution in the common normalized space [0, 1]. "
       << "This report does not claim internal synthesis phase angle, hardware state index, or proprietary parameter reconstruction.</p>\n"
       << "  </div>\n";

    // Interactive Viewer Section
    ss << "  <div class=\"card\">\n"
       << "    <div class=\"tabs\">\n"
       << "      <button class=\"tab-btn active\" onclick=\"switchTab('timbre')\">Timbre (DCW)</button>\n"
       << "      <button class=\"tab-btn\" onclick=\"switchTab('pitch')\">Pitch (DCO)</button>\n"
       << "      <button class=\"tab-btn\" onclick=\"switchTab('amp')\">Amplitude (DCA)</button>\n"
       << "    </div>\n"
       << "    <div id=\"tab-timbre\" class=\"tab-content active\">\n"
       << "      <div class=\"svg-wrapper\">" << svgTimbre << "</div>\n"
       << "    </div>\n"
       << "    <div id=\"tab-pitch\" class=\"tab-content\">\n"
       << "      <div class=\"svg-wrapper\">" << svgPitch << "</div>\n"
       << "    </div>\n"
       << "    <div id=\"tab-amp\" class=\"tab-content\">\n"
       << "      <div class=\"svg-wrapper\">" << svgAmp << "</div>\n"
       << "    </div>\n"
       << "  </div>\n";

    // Two column telemetry & stages
    ss << "  <div class=\"grid-2\">\n";

    // Left card: Timing & Alignment Resolution
    ss << "    <div class=\"card\">\n"
       << "      <h3 style=\"font-size:15px;margin-bottom:12px;\">Temporal Reference Resolution</h3>\n"
       << "      <table>\n"
       << "        <tr><td>Timing Method</td><td><strong>" << timingReferenceTypeToString(result.timingResolution.type) << "</strong></td></tr>\n"
       << "        <tr><td>Resolution Status</td><td>" << result.timingResolution.status << "</td></tr>\n"
       << "        <tr><td>Offset Samples</td><td>" << (result.timingResolution.offsetSamples.has_value() ? std::to_string(*result.timingResolution.offsetSamples) : "N/A") << "</td></tr>\n"
       << "        <tr><td>Peak Ratio (R2/R1)</td><td>" << (result.timingResolution.peakRatio.has_value() ? std::to_string(*result.timingResolution.peakRatio) : "N/A") << "</td></tr>\n"
       << "        <tr><td>Ambiguity Margin</td><td>" << (result.timingResolution.ambiguityMargin.has_value() ? std::to_string(*result.timingResolution.ambiguityMargin) : "N/A") << "</td></tr>\n"
       << "        <tr><td>Alignment Confidence</td><td>" << (result.timingResolution.confidence.has_value() ? std::to_string(*result.timingResolution.confidence) : "N/A") << "</td></tr>\n"
       << "        <tr><td>Raw Audio SHA-256</td><td style=\"font-family:monospace;font-size:10px;\">" << result.sourceRawAudioSha256 << "</td></tr>\n"
       << "      </table>\n"
       << "    </div>\n";

    // Right card: Observable Comparison Metrics
    ss << "    <div class=\"card\">\n"
       << "      <h3 style=\"font-size:15px;margin-bottom:12px;\">Observable Comparison Metrics</h3>\n";

    if (!result.comparisons.empty())
    {
        const auto& comp = result.comparisons.front();
        ss << "      <table>\n"
           << "        <tr><td>Observable Domain</td><td><strong>" << comp.observableDomain << "</strong></td></tr>\n"
           << "        <tr><td>Comparison Status</td><td>" << comp.comparisonStatus << "</td></tr>\n"
           << "        <tr><td>Comparison Space</td><td>" << comp.comparisonSpace << "</td></tr>\n"
           << "        <tr><td>Trajectory RMSE</td><td>" << (comp.trajectoryRmse.has_value() ? std::to_string(*comp.trajectoryRmse) : "null") << "</td></tr>\n"
           << "        <tr><td>Pearson Correlation (r)</td><td>" << (comp.correlation.has_value() ? std::to_string(*comp.correlation) : "null (zero variance)") << "</td></tr>\n"
           << "        <tr><td>Mean Absolute Error</td><td>" << (comp.meanAbsoluteError.has_value() ? std::to_string(*comp.meanAbsoluteError) : "null") << "</td></tr>\n"
           << "        <tr><td>Coverage Ratio</td><td>" << (comp.coverageRatio.has_value() ? std::to_string(*comp.coverageRatio) : "null") << "</td></tr>\n"
           << "        <tr><td>Alignment Transform</td><td>" << alignmentTransformToString(comp.alignmentTransform) << "</td></tr>\n"
           << "      </table>\n";
    }
    else
    {
        ss << "      <p style=\"color:var(--text-muted);font-size:13px;\">No comparisons evaluated.</p>\n";
    }

    ss << "    </div>\n"
       << "  </div>\n";

    // Native Stages Table (if available)
    if (!nativeStages.empty())
    {
        ss << "  <div class=\"card\">\n"
           << "    <h3 style=\"font-size:15px;margin-bottom:12px;\">Native 8-Stage Envelopes</h3>\n"
           << "    <table>\n"
           << "      <thead>\n"
           << "        <tr><th>Stage</th><th>Rate</th><th>Level</th><th>Sustain</th><th>End</th><th>Duration (ms)</th><th>Status</th></tr>\n"
           << "      </thead>\n"
           << "      <tbody>\n";

        for (const auto& stage : nativeStages)
        {
            ss << "        <tr>\n"
               << "          <td><strong>S" << stage.stageIndex << "</strong></td>\n"
               << "          <td>" << (stage.rateOrSlope.has_value() ? std::to_string(static_cast<int>(*stage.rateOrSlope)) : "-") << "</td>\n"
               << "          <td>" << (stage.targetLevel.has_value() ? std::to_string(static_cast<int>(*stage.targetLevel)) : "-") << "</td>\n"
               << "          <td>" << (stage.isSustainPoint ? "<span style=\"color:var(--accent);font-weight:bold;\">YES</span>" : "-") << "</td>\n"
               << "          <td>" << (stage.isEndKeyOnPoint ? "<span style=\"color:var(--danger);font-weight:bold;\">YES</span>" : "-") << "</td>\n"
               << "          <td>" << (stage.durationMs > 0.0 ? std::to_string(stage.durationMs) : "estimated") << "</td>\n"
               << "          <td><span style=\"color:var(--success);\">observable</span></td>\n"
               << "        </tr>\n";
        }

        ss << "      </tbody>\n"
           << "    </table>\n"
           << "  </div>\n";
    }

    // Audio Artifacts Player Section (offline links)
    if (!relRawAudioPath.empty() || !relCompAudioPath.empty())
    {
        ss << "  <div class=\"card\">\n"
           << "    <h3 style=\"font-size:15px;margin-bottom:12px;\">Acoustic Audio Artifacts</h3>\n";

        if (!relRawAudioPath.empty())
        {
            ss << "    <div class=\"audio-bar\">\n"
               << "      <span style=\"width:180px;font-size:13px;color:var(--text-muted);\">Raw Capture Audio:</span>\n"
               << "      <audio controls src=\"" << relRawAudioPath << "\"></audio>\n"
               << "    </div>\n";
        }
        if (!relCompAudioPath.empty())
        {
            ss << "    <div class=\"audio-bar\">\n"
               << "      <span style=\"width:180px;font-size:13px;color:var(--text-muted);\">Compensated Audio:</span>\n"
               << "      <audio controls src=\"" << relCompAudioPath << "\"></audio>\n"
               << "    </div>\n";
        }

        ss << "  </div>\n";
    }

    // Interactive Tab Switching Script (strictly Vanilla JS, zero external dependencies)
    ss << "  <script>\n"
       << "    function switchTab(name) {\n"
       << "      var contents = document.querySelectorAll('.tab-content');\n"
       << "      for (var i = 0; i < contents.length; ++i) contents[i].classList.remove('active');\n"
       << "      var btns = document.querySelectorAll('.tab-btn');\n"
       << "      for (var i = 0; i < btns.length; ++i) btns[i].classList.remove('active');\n"
       << "      var target = document.getElementById('tab-' + name);\n"
       << "      if (target) target.classList.add('active');\n"
       << "      var clickedBtn = event.currentTarget || event.target;\n"
       << "      if (clickedBtn) clickedBtn.classList.add('active');\n"
       << "    }\n"
       << "  </script>\n"
       << "</div>\n"
       << "</body>\n"
       << "</html>\n";

    return ss.str();
}

} // namespace abdaudiolab::measurement
