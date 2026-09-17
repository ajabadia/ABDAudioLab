/**
 * @file MeasurementReportGenerator.cpp
 * @brief Implementation of MeasurementReportGenerator.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementReportGenerator.h"
#include "MeasurementSvgGenerator.h"
#include <sstream>
#include <iomanip>

namespace abdaudiolab::measurement
{

std::string MeasurementReportGenerator::generateFilterReportHtml(const MeasurementSpec& spec,
                                                                 const MeasurementResult& result,
                                                                 const std::string& relCapturedAudio,
                                                                 const std::string& relStimulusAudio,
                                                                 const std::string& relImpulseResponse)
{
    std::ostringstream h;
    h << "<!DOCTYPE html>\n<html lang=\"es\">\n<head>\n"
      << "  <meta charset=\"UTF-8\">\n"
      << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
      << "  <title>Reporte de Medición de Respuesta de Filtro — " << spec.measurementId << "</title>\n"
      << "  <style>\n"
      << "    body { font-family: Inter, -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #0b0f19; color: #f1f5f9; margin: 0; padding: 32px 24px; line-height: 1.5; }\n"
      << "    .container { max-width: 860px; margin: 0 auto; background: #111827; border: 1px solid #1f2937; border-radius: 12px; padding: 32px; box-shadow: 0 10px 30px rgba(0,0,0,0.5); }\n"
      << "    h1 { font-size: 22px; font-weight: 700; color: #f8fafc; margin: 0 0 8px 0; }\n"
      << "    .subtitle { font-size: 13px; color: #94a3b8; margin-bottom: 24px; }\n"
      << "    .badge { display: inline-block; padding: 4px 10px; border-radius: 9999px; font-size: 11px; font-weight: 700; text-transform: uppercase; letter-spacing: 0.5px; }\n"
      << "    .badge-completed { background: #065f46; color: #34d399; border: 1px solid #059669; }\n"
      << "    .badge-unreliable { background: #78350f; color: #fbbf24; border: 1px solid #d97706; }\n"
      << "    .badge-invalid { background: #7f1d1d; color: #f87171; border: 1px solid #dc2626; }\n"
      << "    .badge-failed { background: #450a0a; color: #fca5a5; border: 1px solid #991b1b; }\n"
      << "    .badge-skipped { background: #374151; color: #9ca3af; border: 1px solid #4b5563; }\n"
      << "    .badge-observed { background: #0c4a6e; color: #38bdf8; border: 1px solid #0284c7; }\n"
      << "    .alert { padding: 14px 18px; border-radius: 8px; margin: 18px 0; font-size: 13px; }\n"
      << "    .alert-warning { background: #1c1917; border-left: 4px solid #f59e0b; color: #fef3c7; }\n"
      << "    .alert-info { background: #082f49; border-left: 4px solid #0284c7; color: #e0f2fe; }\n"
      << "    .section-title { font-size: 14px; font-weight: 600; text-transform: uppercase; letter-spacing: 0.75px; color: #94a3b8; margin: 28px 0 12px 0; border-bottom: 1px solid #1f2937; padding-bottom: 6px; }\n"
      << "    table { width: 100%; border-collapse: collapse; font-size: 13px; margin: 12px 0 24px 0; }\n"
      << "    th { text-align: left; background: #1e293b; color: #94a3b8; padding: 10px 14px; font-size: 11px; text-transform: uppercase; font-weight: 600; letter-spacing: 0.5px; border-bottom: 1px solid #334155; }\n"
      << "    td { padding: 10px 14px; border-bottom: 1px solid #1f2937; color: #cbd5e1; }\n"
      << "    tr:hover td { background: #1e293b; }\n"
      << "    code { font-family: 'JetBrains Mono', Consolas, monospace; font-size: 12px; background: #1e293b; padding: 2px 6px; border-radius: 4px; color: #38bdf8; }\n"
      << "    .meta-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 12px; margin: 16px 0; }\n"
      << "    .meta-card { background: #1e293b; padding: 12px 16px; border-radius: 8px; border: 1px solid #334155; }\n"
      << "    .meta-label { font-size: 11px; text-transform: uppercase; color: #94a3b8; margin-bottom: 4px; }\n"
      << "  </style>\n</head>\n<body>\n<div class=\"container\">\n";

    // Header & Status
    std::string badgeClass = "badge-completed";
    std::string statusLabel = "COMPLETED";
    if (result.status == MeasurementStatus::unreliable) { badgeClass = "badge-unreliable"; statusLabel = "UNRELIABLE"; }
    else if (result.status == MeasurementStatus::invalid) { badgeClass = "badge-invalid"; statusLabel = "INVALID"; }
    else if (result.status == MeasurementStatus::failed) { badgeClass = "badge-failed"; statusLabel = "FAILED"; }
    else if (result.status == MeasurementStatus::skipped) { badgeClass = "badge-skipped"; statusLabel = "SKIPPED"; }

    bool isDirect = (spec.measurementDomain == "directTransferFunction" || result.measurementDomain == "directTransferFunction");

    h << "  <div style=\"display: flex; justify-content: space-between; align-items: flex-start;\">\n"
      << "    <div>\n"
      << "      <h1>Filter Measurement: " << statusLabel << "</h1>\n"
      << "      <div class=\"subtitle\">Target: <strong>" << result.dut.name << "</strong> (" << result.dut.format << ") | ID: <code>" << spec.measurementId << "</code></div>\n"
      << "    </div>\n"
      << "    <div style=\"display: flex; gap: 8px; align-items: center;\">\n"
      << "      <span class=\"badge " << (isDirect ? "badge-observed" : "badge-unreliable") << "\">"
      << (isDirect ? "DIRECT TRANSFER FUNCTION [H(&omega;)]" : "SYNTHESIZED SPECTRAL RESPONSE [PROXY]") << "</span>\n"
      << "      <span class=\"badge\" style=\"background: #334155; color: #f8fafc;\">Topology: "
      << (result.filterTopology.empty() ? spec.filterTopology : result.filterTopology) << "</span>\n"
      << "      <span class=\"badge " << badgeClass << "\">" << statusLabel << "</span>\n"
      << "    </div>\n"
      << "  </div>\n";

    // Domain Advisory Banner
    if (isDirect)
    {
        h << "  <div class=\"alert alert-info\">\n"
          << "    <strong>Direct Transfer Function [H(&omega;)]</strong>: Measured via input audio log-sine sweep and Farina deconvolution. Direct transfer function is isolated and observable.\n"
          << "  </div>\n";
    }
    else
    {
        h << "  <div class=\"alert alert-warning\">\n"
          << "    <strong>Synthesized Spectral Response (Proxy)</strong>: Measured via declared MIDI NoteOn excitation on synthesizer output. Direct transfer function of the isolated filter is NOT observable and is not claimed.\n"
          << "  </div>\n";
    }

    // Diagnostic reason alert
    if (!result.reason.empty() && result.reason != "Filter response successfully observed")
    {
        h << "  <div class=\"alert alert-warning\">\n"
          << "    <strong>Reason:</strong> <code>" << result.reason << "</code>\n";
        if (result.observability.reason.has_value())
            h << "    <p style=\"margin: 4px 0 0 0;\">" << *result.observability.reason << "</p>\n";
        h << "  </div>\n";
    }

    // Section 1: Execution Metadata
    h << "  <div class=\"section-title\">Execution & Metrological Metadata</div>\n"
      << "  <div class=\"meta-grid\">\n"
      << "    <div class=\"meta-card\"><div class=\"meta-label\">Sampling Rate</div><div class=\"meta-value\">" << static_cast<int>(result.execution.sampleRateHz) << " Hz</div></div>\n"
      << "    <div class=\"meta-card\"><div class=\"meta-label\">Block Size / Latency</div><div class=\"meta-value\">" << result.execution.blockSize << " spl / " << result.execution.latencySamples << " spl</div></div>\n"
      << "    <div class=\"meta-card\"><div class=\"meta-label\">Analyzer Engine</div><div class=\"meta-value\">" << result.analyzer.name << " (v" << result.analyzer.version << ")</div></div>\n"
      << "    <div class=\"meta-card\"><div class=\"meta-label\">Measurement Domain</div><div class=\"meta-value\">" << (isDirect ? "directTransferFunction" : "synthesizedSpectralResponse") << "</div></div>\n"
      << "  </div>\n";

    // Section 2: Filter Metrics Table
    h << "  <div class=\"section-title\">Observed Filter Metrics</div>\n"
      << "  <table>\n"
      << "    <thead><tr><th>Metric</th><th>Observed Value</th><th>Unit</th><th>Status</th><th>Diagnostic Reason</th></tr></thead>\n"
      << "    <tbody>\n";

    for (const auto& m : result.metrics)
    {
        // Enforce metrological rule: do not show direct transfer metrics as valid if domain is MIDI proxy
        if (!isDirect && (m.name == "cutoffFrequency" || m.name == "asymptoticSlope" || m.name == "qFactor"))
        {
            continue; // Skip isolated filter transfer function metrics for MIDI composite response
        }

        std::string mBadge = "badge-observed";
        if (m.status == "unreliable") mBadge = "badge-unreliable";
        else if (m.status == "invalid") mBadge = "badge-invalid";
        else if (m.status == "failed") mBadge = "badge-failed";
        else if (m.status == "not_observable") mBadge = "badge-skipped";

        h << "      <tr>\n"
          << "        <td><strong>" << m.name.toStdString() << "</strong></td>\n"
          << "        <td style=\"font-weight: 600; color: #f8fafc;\">" << std::fixed << std::setprecision(2) << m.value << "</td>\n"
          << "        <td><code>" << m.unit.toStdString() << "</code></td>\n"
          << "        <td><span class=\"badge " << mBadge << "\">" << m.status.toStdString() << "</span></td>\n"
          << "        <td><span style=\"font-size: 11px; color: #94a3b8;\">" << m.reason.toStdString() << "</span></td>\n"
          << "      </tr>\n";
    }

    h << "    </tbody>\n  </table>\n";

    // Section 2b: Asymptotic Slope Fit Region (if available)
    if (result.slopeFit.has_value() && isDirect)
    {
        h << "  <div class=\"meta-card\" style=\"margin-bottom: 20px;\">\n"
          << "    <div class=\"meta-label\">Asymptotic Slope Fit Quality (&ge; 1.4 fc)</div>\n"
          << "    <div style=\"display: flex; gap: 24px; font-size: 13px; margin-top: 6px;\">\n"
          << "      <div>Fit Region: <strong>" << std::fixed << std::setprecision(1) << result.slopeFit->frequencyStartHz << " - "
          << result.slopeFit->frequencyEndHz << " Hz</strong></div>\n"
          << "      <div>Goodness of Fit (R&sup2;): <strong>" << std::setprecision(4) << result.slopeFit->rSquared << "</strong></div>\n"
          << "      <div>Points: <strong>" << result.slopeFit->sampleCount << "</strong></div>\n"
          << "    </div>\n"
          << "  </div>\n";
    }

    // Section 3: Vector Frequency Curve
    h << "  <div class=\"section-title\">Frequency Response H(&omega;)</div>\n";
    if (!result.curve.x.empty())
    {
        double cutoffHz = -1.0;
        for (const auto& m : result.metrics)
        {
            if (m.name == "cutoffFrequency" && m.value > 0.0)
            {
                cutoffHz = m.value;
                break;
            }
        }

        h << "  <div style=\"margin: 16px 0; text-align: center;\">\n"
          << MeasurementSvgGenerator::generateFilterCurveSvg(result.curve.x, result.curve.y, result.slopeFit, cutoffHz, 796, 280)
          << "  </div>\n";
    }
    else
    {
        h << "  <div class=\"alert alert-info\">No frequency curve persisted (signal is unobservable or execution incomplete).</div>\n";
    }

    // Section 4: Acoustic Artifacts & Audio Playback
    if (!relCapturedAudio.empty() || !relStimulusAudio.empty() || !relImpulseResponse.empty())
    {
        h << "  <div class=\"section-title\">Acoustic Artifacts &amp; Audio Auditing</div>\n";

        if (!relCapturedAudio.empty())
        {
            h << "  <div style=\"background: #1e293b; padding: 12px 16px; border-radius: 8px; margin-bottom: 8px; display: flex; align-items: center; justify-content: space-between;\">\n"
              << "    <div><span style=\"font-weight: 600;\">Captured Audio (Device Output)</span></div>\n"
              << "    <audio controls preload=\"none\" style=\"width: 55%;\" src=\"" << relCapturedAudio << "\"></audio>\n"
              << "    <a href=\"" << relCapturedAudio << "\" style=\"color: #38bdf8; text-decoration: none; font-size: 12px; font-weight: 600;\">Download WAV</a>\n"
              << "  </div>\n";
        }

        if (!relStimulusAudio.empty())
        {
            h << "  <div style=\"background: #1e293b; padding: 12px 16px; border-radius: 8px; margin-bottom: 8px; display: flex; align-items: center; justify-content: space-between;\">\n"
              << "    <div><span style=\"font-weight: 600;\">Stimulus Audio (Sweep Input)</span></div>\n"
              << "    <audio controls preload=\"none\" style=\"width: 55%;\" src=\"" << relStimulusAudio << "\"></audio>\n"
              << "    <a href=\"" << relStimulusAudio << "\" style=\"color: #38bdf8; text-decoration: none; font-size: 12px; font-weight: 600;\">Download WAV</a>\n"
              << "  </div>\n";
        }

        if (!relImpulseResponse.empty())
        {
            h << "  <div style=\"background: #1e293b; padding: 12px 16px; border-radius: 8px; margin-bottom: 8px; display: flex; align-items: center; justify-content: space-between;\">\n"
              << "    <div><span style=\"font-weight: 600;\">Impulse Response (Deconvolved IR)</span></div>\n"
              << "    <audio controls preload=\"none\" style=\"width: 55%;\" src=\"" << relImpulseResponse << "\"></audio>\n"
              << "    <a href=\"" << relImpulseResponse << "\" style=\"color: #38bdf8; text-decoration: none; font-size: 12px; font-weight: 600;\">Download WAV</a>\n"
              << "  </div>\n";
        }
    }

    // Section 5: FAIR Cryptographic Provenance
    h << "  <div class=\"section-title\">FAIR Cryptographic Provenance</div>\n"
      << "  <table>\n"
      << "    <thead><tr><th>Artifact</th><th>FAIR Role</th><th>Fixity SHA-256</th></tr></thead>\n"
      << "    <tbody>\n"
      << "      <tr><td>Measurement Spec</td><td><code>measurement_spec</code></td><td><code>" << spec.measurementId << "</code></td></tr>\n"
      << "      <tr><td>Measurement Stimulus</td><td><code>measurement_stimulus</code></td><td><code>" << spec.stimulus.sha256 << "</code></td></tr>\n"
      << "      <tr><td>Measurement Result</td><td><code>measurement_result</code></td><td><code>" << spec.measurementId << "</code></td></tr>\n"
      << "      <tr><td>Filter Response Curve</td><td><code>filter_response_curve</code></td><td><code>filter_response_curve.json</code></td></tr>\n";

    if (!relCapturedAudio.empty())
        h << "      <tr><td>Captured Audio</td><td><code>measurement_captured_audio</code></td><td><code>" << result.artifacts.audioSha256 << "</code></td></tr>\n";
    if (!relStimulusAudio.empty())
        h << "      <tr><td>Stimulus Audio</td><td><code>measurement_stimulus_audio</code></td><td><code>audio_stimulus.wav</code></td></tr>\n";
    if (!relImpulseResponse.empty())
        h << "      <tr><td>Impulse Response</td><td><code>measurement_impulse_response</code></td><td><code>impulse_response.wav</code></td></tr>\n";

    h << "      <tr><td>Measurement Report</td><td><code>measurement_report</code></td><td><code>measurement_report.html</code></td></tr>\n"
      << "    </tbody>\n"
      << "  </table>\n";

    h << "</div>\n</body>\n</html>\n";
    return h.str();
}

std::string MeasurementReportGenerator::generateReportHtml(const MeasurementSpec& spec,
                                                           const MeasurementResult& result,
                                                           const std::string& relativeAudioPath)
{
    if (spec.measurementType == "filter" || result.measurementType == "filter")
    {
        return generateFilterReportHtml(spec, result, relativeAudioPath);
    }

    std::ostringstream h;
    h << "<!DOCTYPE html>\n<html lang=\"es\">\n<head>\n"
      << "  <meta charset=\"UTF-8\">\n"
      << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
      << "  <title>Reporte de Medición de Respuesta Acústica — " << spec.measurementId << "</title>\n"
      << "  <style>\n"
      << "    body { font-family: Inter, -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #0b0f19; color: #f1f5f9; margin: 0; padding: 32px 24px; line-height: 1.5; }\n"
      << "    .container { max-width: 860px; margin: 0 auto; background: #111827; border: 1px solid #1f2937; border-radius: 12px; padding: 32px; box-shadow: 0 10px 30px rgba(0,0,0,0.5); }\n"
      << "    h1 { font-size: 22px; font-weight: 700; color: #f8fafc; margin: 0 0 8px 0; }\n"
      << "    .subtitle { font-size: 13px; color: #94a3b8; margin-bottom: 24px; }\n"
      << "    .badge { display: inline-block; padding: 4px 10px; border-radius: 9999px; font-size: 11px; font-weight: 700; text-transform: uppercase; letter-spacing: 0.5px; }\n"
      << "    .badge-completed { background: #065f46; color: #34d399; border: 1px solid #059669; }\n"
      << "    .badge-unreliable { background: #78350f; color: #fbbf24; border: 1px solid #d97706; }\n"
      << "    .badge-invalid { background: #7f1d1d; color: #f87171; border: 1px solid #dc2626; }\n"
      << "    .badge-failed { background: #450a0a; color: #fca5a5; border: 1px solid #991b1b; }\n"
      << "    .badge-skipped { background: #374151; color: #9ca3af; border: 1px solid #4b5563; }\n"
      << "    .badge-observed { background: #0c4a6e; color: #38bdf8; border: 1px solid #0284c7; }\n"
      << "    .alert { padding: 14px 18px; border-radius: 8px; margin: 18px 0; font-size: 13px; }\n"
      << "    .alert-warning { background: #1c1917; border-left: 4px solid #f59e0b; color: #fef3c7; }\n"
      << "    .alert-info { background: #082f49; border-left: 4px solid #0284c7; color: #e0f2fe; }\n"
      << "    .section-title { font-size: 14px; font-weight: 600; text-transform: uppercase; letter-spacing: 0.75px; color: #94a3b8; margin: 28px 0 12px 0; border-bottom: 1px solid #1f2937; padding-bottom: 6px; }\n"
      << "    table { width: 100%; border-collapse: collapse; font-size: 13px; margin: 12px 0 24px 0; }\n"
      << "    th { text-align: left; background: #1e293b; color: #94a3b8; padding: 10px 14px; font-size: 11px; text-transform: uppercase; font-weight: 600; letter-spacing: 0.5px; border-bottom: 1px solid #334155; }\n"
      << "    td { padding: 10px 14px; border-bottom: 1px solid #1f2937; color: #cbd5e1; }\n"
      << "    tr:hover td { background: #1e293b; }\n"
      << "    code { font-family: 'JetBrains Mono', Consolas, monospace; font-size: 12px; background: #1e293b; padding: 2px 6px; border-radius: 4px; color: #38bdf8; }\n"
      << "    .meta-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 12px; margin: 16px 0; }\n"
      << "    .meta-card { background: #1e293b; padding: 12px 16px; border-radius: 8px; border: 1px solid #334155; }\n"
      << "    .meta-label { font-size: 11px; text-transform: uppercase; color: #94a3b8; margin-bottom: 4px; }\n"
      << "  </style>\n</head>\n<body>\n<div class=\"container\">\n";

    // Header & Status
    std::string badgeClass = "badge-completed";
    std::string statusLabel = "COMPLETED";
    if (result.status == MeasurementStatus::unreliable) { badgeClass = "badge-unreliable"; statusLabel = "UNRELIABLE"; }
    else if (result.status == MeasurementStatus::invalid) { badgeClass = "badge-invalid"; statusLabel = "INVALID"; }
    else if (result.status == MeasurementStatus::failed) { badgeClass = "badge-failed"; statusLabel = "FAILED"; }
    else if (result.status == MeasurementStatus::skipped) { badgeClass = "badge-skipped"; statusLabel = "SKIPPED"; }

    h << "  <div style=\"display: flex; justify-content: space-between; align-items: flex-start;\">\n"
      << "    <div>\n"
      << "      <h1>Envelope measurement: " << statusLabel << "</h1>\n"
      << "      <div class=\"subtitle\">Target: <strong>" << result.dut.name << "</strong> (" << result.dut.format << ") | ID: <code>" << spec.measurementId << "</code></div>\n"
      << "    </div>\n"
      << "    <div><span class=\"badge " << badgeClass << "\">" << statusLabel << "</span></div>\n"
      << "  </div>\n";

    // Diagnostic reason alert (if not clean completed or has note)
    if (!result.reason.empty() && result.reason != "Envelope successfully observed")
    {
        h << "  <div class=\"alert alert-warning\">\n"
          << "    <strong>Reason:</strong> <code>" << result.reason << "</code>\n";
        if (result.observability.reason.has_value())
            h << "    <p style=\"margin: 4px 0 0 0;\">" << *result.observability.reason << "</p>\n";
        h << "  </div>\n";
    }

    // Section 1: Execution Metadata
    h << "  <div class=\"section-title\">Execution & Metrological Metadata</div>\n"
      << "  <div class=\"meta-grid\">\n"
      << "    <div class=\"meta-card\"><div class=\"meta-label\">Sampling Rate</div><div class=\"meta-value\">" << static_cast<int>(result.execution.sampleRateHz) << " Hz</div></div>\n"
      << "    <div class=\"meta-card\"><div class=\"meta-label\">Block Size / Latency</div><div class=\"meta-value\">" << result.execution.blockSize << " spl / " << result.execution.latencySamples << " spl</div></div>\n"
      << "    <div class=\"meta-card\"><div class=\"meta-label\">Analyzer Engine</div><div class=\"meta-value\">" << result.analyzer.name << " (v" << result.analyzer.version << ")</div></div>\n"
      << "    <div class=\"meta-card\"><div class=\"meta-label\">Stimulus Type</div><div class=\"meta-value\">" << stimulusTypeToString(result.stimulus.type) << " (Note " << result.stimulus.midiNoteNumber << ", Vel " << result.stimulus.midiVelocity << ")</div></div>\n"
      << "  </div>\n";

    // Section 2: Strongly-Typed Metrics
    h << "  <div class=\"section-title\">Observed ADSR Metrics</div>\n"
      << "  <table>\n"
      << "    <thead><tr><th>Metric</th><th>Observed Value</th><th>Unit</th><th>Status</th></tr></thead>\n"
      << "    <tbody>\n";

    for (const auto& m : result.metrics)
    {
        std::string mBadge = (m.status == "observed") ? "badge-observed" : "badge-unreliable";
        h << "      <tr>\n"
          << "        <td><strong>" << m.name.toStdString() << "</strong></td>\n"
          << "        <td style=\"font-weight: 600; color: #f8fafc;\">" << std::fixed << std::setprecision(1) << m.value << "</td>\n"
          << "        <td><code>" << m.unit.toStdString() << "</code></td>\n"
          << "        <td><span class=\"badge " << mBadge << "\">" << m.status.toStdString() << "</span></td>\n"
          << "      </tr>\n";
    }

    h << "    </tbody>\n  </table>\n";

    // Section 3: Vector Temporal Curve
    h << "  <div class=\"section-title\">Temporal Envelope Trajectory</div>\n";
    if (!result.curve.x.empty())
    {
        h << "  <div style=\"margin: 16px 0; text-align: center;\">\n"
          << MeasurementSvgGenerator::generateTemporalCurveSvg(result.curve.x, result.curve.y, 796, 260)
          << "  </div>\n";
    }
    else
    {
        h << "  <div class=\"alert alert-info\">No temporal curve persisted (signal is unobservable or execution incomplete).</div>\n";
    }

    // Section 4: Audio Playback Controls
    if (!relativeAudioPath.empty())
    {
        h << "  <div class=\"section-title\">Acoustic Artifact & Playback</div>\n"
          << "  <div style=\"background: #1e293b; padding: 16px; border-radius: 8px; display: flex; align-items: center; justify-content: space-between;\">\n"
          << "    <audio controls preload=\"none\" style=\"width: 70%;\" src=\"" << relativeAudioPath << "\"></audio>\n"
          << "    <a href=\"" << relativeAudioPath << "\" style=\"color: #38bdf8; text-decoration: none; font-size: 12px; font-weight: 600;\">Download WAV</a>\n"
          << "  </div>\n";
    }

    // Section 5: FAIR Cryptographic Provenance
    h << "  <div class=\"section-title\">FAIR Cryptographic Provenance</div>\n"
      << "  <table>\n"
      << "    <thead><tr><th>Artifact</th><th>FAIR Role</th><th>Fixity SHA-256</th></tr></thead>\n"
      << "    <tbody>\n"
      << "      <tr><td>Measurement Spec</td><td><code>measurement_spec</code></td><td><code>" << spec.measurementId << "</code></td></tr>\n"
      << "      <tr><td>Measurement Stimulus</td><td><code>measurement_stimulus</code></td><td><code>" << spec.stimulus.sha256 << "</code></td></tr>\n"
      << "      <tr><td>Measurement Result</td><td><code>measurement_result</code></td><td><code>" << spec.measurementId << "</code></td></tr>\n"
      << "      <tr><td>Envelope Curve</td><td><code>envelope_curve</code></td><td><code>envelope_curve.json</code></td></tr>\n"
      << "      <tr><td>Acoustic Audio</td><td><code>measurement_baseline_audio</code></td><td><code>" << result.artifacts.audioSha256 << "</code></td></tr>\n"
      << "    </tbody>\n"
      << "  </table>\n";

    h << "</div>\n</body>\n</html>\n";
    return h.str();
}

} // namespace abdaudiolab::measurement
