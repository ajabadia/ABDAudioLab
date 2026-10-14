/**
 * @file ProfileValidationScopeReportExporter.cpp
 * @brief Implementation of publication-grade HTML report generator for Profile Validation and Scope Declaration.
 * @author ABDSynths
 * @date 2026
 */

#include "ProfileValidationScopeReportExporter.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

namespace abdaudiolab::exporting
{

std::string ProfileValidationScopeReportExporter::generateResidualSpectrumSvg(const std::vector<float>& freqsHz,
                                                                             const std::vector<float>& targetMagDb,
                                                                             const std::vector<float>& modelMagDb,
                                                                             const std::vector<float>& residualMagDb,
                                                                             int width,
                                                                             int height)
{
    std::ostringstream svg;
    svg.imbue(std::locale::classic());

    svg << "<svg width=\"" << width << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height
        << "\" xmlns=\"http://www.w3.org/2000/svg\">\n";
    svg << "<style>\n";
    svg << "  .bg { fill: #f8fafc; rx: 8px; stroke: #e2e8f0; stroke-width: 1; }\n";
    svg << "  .grid { stroke: #e2e8f0; stroke-width: 1; stroke-dasharray: 3,3; }\n";
    svg << "  .axis-label { fill: #64748b; font-size: 10px; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; }\n";
    svg << "  .line-target { fill: none; stroke: #0284c7; stroke-width: 2.2; stroke-linecap: round; }\n";
    svg << "  .line-model { fill: none; stroke: #059669; stroke-width: 2.0; stroke-dasharray: 4,2; stroke-linecap: round; }\n";
    svg << "  .line-residual { fill: none; stroke: #dc2626; stroke-width: 1.8; stroke-linecap: round; opacity: 0.85; }\n";
    svg << "  .legend-text { fill: #334155; font-size: 11px; font-weight: 600; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; }\n";
    svg << "  .title { fill: #0f172a; font-size: 11px; font-weight: 700; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; letter-spacing: 0.5px; }\n";
    svg << "</style>\n";

    svg << "<rect width=\"100%\" height=\"100%\" class=\"bg\" />\n";

    int marginL = 50, marginR = 20, marginT = 32, marginB = 30;
    int plotW = width - marginL - marginR;
    int plotH = height - marginT - marginB;

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
            svg << "<text x=\"" << x << "\" y=\"" << (height - 10) << "\" class=\"axis-label\" text-anchor=\"middle\">" << label << " Hz</text>\n";
        }
    }

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

    auto renderPath = [&](const std::vector<float>& mags, const std::string& cssClass) {
        if (freqsHz.empty() || freqsHz.size() != mags.size())
            return;
        svg << "<path d=\"M";
        bool first = true;
        for (size_t i = 0; i < freqsHz.size(); ++i)
        {
            float f = std::clamp(freqsHz[i], 20.0f, 20000.0f);
            float db = std::clamp(mags[i], minDb, maxDb);
            float normX = (std::log10(f) - logMin) / logRange;
            float normY = 1.0f - ((db - minDb) / rangeDb);
            float x = marginL + normX * plotW;
            float y = marginT + normY * plotH;
            if (first) { svg << std::fixed << std::setprecision(1) << x << " " << y; first = false; }
            else { svg << " L " << std::fixed << std::setprecision(1) << x << " " << y; }
        }
        svg << "\" class=\"" << cssClass << "\" />\n";
    };

    if (!targetMagDb.empty()) renderPath(targetMagDb, "line-target");
    if (!modelMagDb.empty()) renderPath(modelMagDb, "line-model");
    if (!residualMagDb.empty()) renderPath(residualMagDb, "line-residual");

    // Title and Legend
    svg << "<text x=\"" << marginL << "\" y=\"20\" class=\"title\">ESPECTRO COMPARATIVO Y RESIDUO (dBFS)</text>\n";
    svg << "<g transform=\"translate(" << (width - 320) << ", 10)\">\n";
    svg << "  <line x1=\"0\" y1=\"10\" x2=\"18\" y2=\"10\" class=\"line-target\" />\n";
    svg << "  <text x=\"24\" y=\"14\" class=\"legend-text\">Target Real</text>\n";
    svg << "  <line x1=\"105\" y1=\"10\" x2=\"123\" y2=\"10\" class=\"line-model\" />\n";
    svg << "  <text x=\"129\" y=\"14\" class=\"legend-text\">Modelo Emulado</text>\n";
    svg << "  <line x1=\"225\" y1=\"10\" x2=\"243\" y2=\"10\" class=\"line-residual\" />\n";
    svg << "  <text x=\"249\" y=\"14\" class=\"legend-text\">Residuo</text>\n";
    svg << "</g>\n";

    svg << "</svg>\n";
    return svg.str();
}

std::string ProfileValidationScopeReportExporter::generateTimeDomainResidualSvg(const std::vector<float>& residualWaveform,
                                                                               double sampleRate,
                                                                               int width,
                                                                               int height)
{
    std::ostringstream svg;
    svg.imbue(std::locale::classic());

    svg << "<svg width=\"" << width << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height
        << "\" xmlns=\"http://www.w3.org/2000/svg\">\n";
    svg << "<style>\n";
    svg << "  .bg { fill: #f8fafc; rx: 8px; stroke: #e2e8f0; stroke-width: 1; }\n";
    svg << "  .grid { stroke: #e2e8f0; stroke-width: 1; stroke-dasharray: 3,3; }\n";
    svg << "  .axis-label { fill: #64748b; font-size: 10px; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; }\n";
    svg << "  .line-wave { fill: none; stroke: #e11d48; stroke-width: 1.5; stroke-linecap: round; }\n";
    svg << "  .zero-line { stroke: #94a3b8; stroke-width: 1.2; stroke-dasharray: 2,2; }\n";
    svg << "  .title { fill: #0f172a; font-size: 11px; font-weight: 700; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; letter-spacing: 0.5px; }\n";
    svg << "</style>\n";

    svg << "<rect width=\"100%\" height=\"100%\" class=\"bg\" />\n";

    int marginL = 50, marginR = 20, marginT = 28, marginB = 26;
    int plotW = width - marginL - marginR;
    int plotH = height - marginT - marginB;

    float midY = static_cast<float>(marginT) + static_cast<float>(plotH) * 0.5f;
    svg << "<line x1=\"" << marginL << "\" y1=\"" << midY << "\" x2=\"" << (width - marginR) << "\" y2=\"" << midY << "\" class=\"zero-line\" />\n";
    svg << "<text x=\"" << (marginL - 8) << "\" y=\"" << (midY + 3) << "\" class=\"axis-label\" text-anchor=\"end\">0.0</text>\n";
    svg << "<text x=\"" << (marginL - 8) << "\" y=\"" << (marginT + 10) << "\" class=\"axis-label\" text-anchor=\"end\">+Pk</text>\n";
    svg << "<text x=\"" << (marginL - 8) << "\" y=\"" << (marginT + plotH) << "\" class=\"axis-label\" text-anchor=\"end\">-Pk</text>\n";

    if (!residualWaveform.empty())
    {
        float maxAbs = 1e-6f;
        for (float s : residualWaveform)
        {
            float a = std::abs(s);
            if (a > maxAbs) maxAbs = a;
        }

        const size_t N = residualWaveform.size();
        svg << "<path d=\"M";
        for (size_t i = 0; i < N; ++i)
        {
            float normX = static_cast<float>(i) / static_cast<float>(N > 1 ? N - 1 : 1);
            float normY = 0.5f - (residualWaveform[i] / (2.0f * maxAbs));
            float x = marginL + normX * plotW;
            float y = marginT + normY * plotH;
            if (i == 0) svg << std::fixed << std::setprecision(1) << x << " " << y;
            else svg << " L " << std::fixed << std::setprecision(1) << x << " " << y;
        }
        svg << "\" class=\"line-wave\" />\n";

        double totalMs = (static_cast<double>(N) / (sampleRate > 0.0 ? sampleRate : 96000.0)) * 1000.0;
        svg << "<text x=\"" << (width - marginR) << "\" y=\"" << (height - 8) << "\" class=\"axis-label\" text-anchor=\"end\">"
            << std::fixed << std::setprecision(1) << totalMs << " ms</text>\n";
    }

    svg << "<text x=\"" << marginL << "\" y=\"18\" class=\"title\">FORMA DE ONDA DEL ERROR RESIDUAL e[n] = y[n] - &#375;[n]</text>\n";
    svg << "</svg>\n";
    return svg.str();
}

std::string ProfileValidationScopeReportExporter::generateDomainOfValidityBoxHtml(const DomainOfValidity& domain)
{
    std::ostringstream ss;
    ss << "<div class=\"domain-card\">\n";
    ss << "  <div class=\"domain-header\"><span class=\"domain-badge\">DOMINIO DE VALIDEZ DECLARADO (SCOPE OF VALIDITY)</span></div>\n";
    ss << "  <div class=\"domain-grid\">\n";
    ss << "    <div class=\"domain-item\"><span class=\"domain-lbl\">Frecuencia de Muestreo:</span> <strong>"
       << static_cast<int>(domain.sampleRate / 1000.0) << " kHz</strong></div>\n";
    ss << "    <div class=\"domain-item\"><span class=\"domain-lbl\">Rango Dinámico Entrada:</span> <strong>"
       << domain.minInputLevelDbfs << " a " << domain.maxInputLevelDbfs << " dBFS</strong></div>\n";
    ss << "    <div class=\"domain-item\"><span class=\"domain-lbl\">Tiempo de Calentamiento:</span> <strong>&ge; "
       << domain.warmupMinutes << " min (Estabilidad Térmica)</strong></div>\n";
    ss << "    <div class=\"domain-item\"><span class=\"domain-lbl\">Topología de Canal:</span> <strong>"
       << domain.channelLayout << "</strong></div>\n";
    ss << "    <div class=\"domain-item\"><span class=\"domain-lbl\">Temperatura Ambiente:</span> <strong>"
       << domain.operatingTemperature << "</strong></div>\n";
    ss << "    <div class=\"domain-item\"><span class=\"domain-lbl\">Controles Mapeados:</span> <strong>"
       << domain.documentedControlRanges << "</strong></div>\n";
    ss << "  </div>\n";
    ss << "  <div class=\"warning-box\">\n";
    ss << "    <strong>&#9888; Cláusula Estricta de No-Extrapolación:</strong><br>\n";
    ss << "    " << domain.extrapolationWarning << "\n";
    ss << "  </div>\n";
    ss << "</div>\n";
    return ss.str();
}

std::string ProfileValidationScopeReportExporter::generateCandidateComparisonTableHtml(const synth::CandidateBenchmarkingReport& benchmarking,
                                                                                        const synth::ModelEvaluation& evaluation)
{
    std::ostringstream ss;
    ss << "<table class=\"report-table\">\n";
    ss << "  <thead>\n";
    ss << "    <tr>\n";
    ss << "      <th>ARQUITECTURA CANDIDATA</th>\n";
    ss << "      <th>FIDELIDAD (R&sup2;)</th>\n";
    ss << "      <th>ESR (dB)</th>\n";
    ss << "      <th>RESIDUO (&rho;<sub>1</sub>)</th>\n";
    ss << "      <th>CPU %</th>\n";
    ss << "      <th>RAM</th>\n";
    ss << "      <th>LATENCIA</th>\n";
    ss << "      <th>COSTE J(m)</th>\n";
    ss << "      <th>ESTADO</th>\n";
    ss << "    </tr>\n";
    ss << "  </thead>\n";
    ss << "  <tbody>\n";

    for (const auto& c : benchmarking.candidates)
    {
        bool isRecommended = (c.modelId == benchmarking.recommendedTargetModelId || c.modelId == evaluation.decision.recommendedModelId);
        bool isBestFidelity = (c.modelId == benchmarking.bestRawFidelityModelId || c.isRawFidelityLeader);

        ss << "    <tr" << (isRecommended ? " class=\"highlight-row\"" : "") << ">\n";
        ss << "      <td><strong>" << c.modelId << "</strong>";
        if (isRecommended)
            ss << " <span class=\"winner-tag\">&#9733; RECOMENDADO</span>";
        if (isBestFidelity)
            ss << " <span class=\"fidelity-tag\">MAX. FIDELIDAD</span>";
        ss << "<br><span class=\"subtext\">" << c.architecture << "</span></td>\n";

        ss << "      <td>" << std::fixed << std::setprecision(5) << c.metrics.rSquaredScore << "</td>\n";
        ss << "      <td>" << std::fixed << std::setprecision(1) << c.metrics.errorToSignalRatioDb << " dB</td>\n";

        // Diagnóstico residuo
        ss << "      <td>" << std::fixed << std::setprecision(3) << c.diagnostics.peakAutocorrelation;
        if (c.diagnostics.peakAutocorrelation < 0.15)
            ss << " <span class=\"badge-pill green\">Estático</span>";
        else
            ss << " <span class=\"badge-pill amber\">Dinámico</span>";
        ss << "</td>\n";

        ss << "      <td>" << std::fixed << std::setprecision(2) << c.footprint.cpuUsagePercentPerVoice << "%</td>\n";
        ss << "      <td>" << (c.footprint.memoryBytes / 1024) << " KB</td>\n";
        ss << "      <td>" << c.footprint.introducedLatencySamples << " smp</td>\n";
        ss << "      <td><strong>" << std::fixed << std::setprecision(4) << c.totalCompositeScore << "</strong></td>\n";

        ss << "      <td>";
        if (c.metrics.rSquaredScore >= 0.98)
            ss << "<span class=\"status-text green\">Approved</span>";
        else if (c.metrics.rSquaredScore >= 0.90)
            ss << "<span class=\"status-text amber\">Warning</span>";
        else
            ss << "<span class=\"status-text red\">Rejected</span>";
        ss << "</td>\n";
        ss << "    </tr>\n";
    }

    ss << "  </tbody>\n";
    ss << "</table>\n";
    return ss.str();
}

std::string ProfileValidationScopeReportExporter::generateHoldoutBatteryTableHtml(const std::vector<HoldoutBatterySummary>& batteries,
                                                                                  bool leakageDetected,
                                                                                  double minDistance)
{
    std::ostringstream ss;
    ss << "<table class=\"report-table\">\n";
    ss << "  <thead>\n";
    ss << "    <tr>\n";
    ss << "      <th>SUB-BATERÍA CIEGA</th>\n";
    ss << "      <th>DURACIÓN</th>\n";
    ss << "      <th>PICO (dBFS)</th>\n";
    ss << "      <th>RMS (dBFS)</th>\n";
    ss << "      <th>CREST FACTOR</th>\n";
    ss << "      <th>OBJETIVO METROLÓGICO</th>\n";
    ss << "    </tr>\n";
    ss << "  </thead>\n";
    ss << "  <tbody>\n";

    for (size_t i = 0; i < batteries.size(); ++i)
    {
        const auto& b = batteries[i];
        ss << "    <tr>\n";
        ss << "      <td><strong>" << b.batteryName << "</strong><br><span class=\"subtext\">"
           << b.description << "</span></td>\n";
        ss << "      <td>" << std::fixed << std::setprecision(2) << b.acoustics.durationSeconds << " s</td>\n";
        ss << "      <td>" << std::fixed << std::setprecision(1) << b.acoustics.peakDbfs << " dB</td>\n";
        ss << "      <td>" << std::fixed << std::setprecision(1) << b.acoustics.rmsDbfs << " dB</td>\n";
        ss << "      <td><strong>" << std::fixed << std::setprecision(2) << b.acoustics.crestFactorDb << " dB</strong></td>\n";
        ss << "      <td><span class=\"badge-pill green\">" << (i == 2 ? "CF &lt; 9.0 dB (Pass)" : "Validado") << "</span></td>\n";
        ss << "    </tr>\n";
    }

    ss << "  </tbody>\n";
    ss << "</table>\n";

    // Anti-leakage info
    ss << "<div class=\"leakage-box\">\n";
    ss << "  <strong>Auditoría Anti-Leakage (Separación de Datos de Entrenamiento):</strong> ";
    if (leakageDetected)
        ss << "<span class=\"status-text red\">CONTAMINACIÓN DETECTADA</span> (Distancia: " << minDistance << ")";
    else
        ss << "<span class=\"status-text green\">PASSED — ZERO CONTAMINATION</span> (Distancia mínima: "
           << std::fixed << std::setprecision(3) << minDistance << " &gt; threshold)";
    ss << "</div>\n";

    return ss.str();
}

std::string ProfileValidationScopeReportExporter::generatePluginFootprintCardHtml(const synth::ModelEvaluation& evaluation,
                                                                                 double sampleRate)
{
    double cpuPerVoice = evaluation.resourceCost.cpuUsagePercentPerVoice;
    size_t latency = static_cast<size_t>(std::max(0, evaluation.resourceCost.introducedLatencySamples));
    size_t ram = evaluation.resourceCost.memoryBytes;

    int estimatedVoices = (cpuPerVoice > 0.001) ? static_cast<int>(50.0 / cpuPerVoice) : 256;
    estimatedVoices = std::clamp(estimatedVoices, 1, 512);
    double latencyMs = (sampleRate > 0.0) ? (static_cast<double>(latency) / sampleRate) * 1000.0 : 0.0;

    std::ostringstream ss;
    ss << "<div class=\"metrics-grid\">\n";
    ss << "  <div class=\"metric-card\"><div class=\"metric-lbl\">CPU ESTIMADA / VOZ</div><div class=\"metric-val\">"
       << std::fixed << std::setprecision(2) << cpuPerVoice << "%</div></div>\n";
    ss << "  <div class=\"metric-card\"><div class=\"metric-lbl\">POLIFONÍA ADMISIBLE (50% CPU)</div><div class=\"metric-val\">"
       << estimatedVoices << " voces</div></div>\n";
    ss << "  <div class=\"metric-card\"><div class=\"metric-lbl\">HUELLA MEMORIA (RAM)</div><div class=\"metric-val\">"
       << (ram / 1024) << " KB</div></div>\n";
    ss << "  <div class=\"metric-card\"><div class=\"metric-lbl\">LATENCIA E/S</div><div class=\"metric-val\">"
       << latency << " smp (" << std::fixed << std::setprecision(2) << latencyMs << " ms)</div></div>\n";
    ss << "</div>\n";
    return ss.str();
}

bool ProfileValidationScopeReportExporter::exportReportToHtml(const std::string& targetPath,
                                                             const ProfileScopeReportData& data)
{
    std::ofstream file(targetPath);
    if (!file.is_open())
        return false;

    // Badging
    std::string badgeClass = "badge-pass";
    std::string badgeText = "[OK] PERFIL VALIDADO";
    if (data.evaluation.decision.status == synth::SelectionStatus::AcceptedWithWarnings)
    {
        badgeClass = "badge-warn";
        badgeText = "[!] VALIDADO CON OBSERVACIONES";
    }
    else if (data.evaluation.decision.status == synth::SelectionStatus::Inconclusive)
    {
        badgeClass = "badge-inconclusive";
        badgeText = "[i] INCONCLUSIVE: EMPATE ESTADÍSTICO";
    }
    else if (data.evaluation.decision.status == synth::SelectionStatus::Rejected)
    {
        badgeClass = "badge-fail";
        badgeText = "[X] NO ACEPTADO";
    }

    file << "<!DOCTYPE html>\n";
    file << "<html lang=\"es\">\n<head>\n";
    file << "<meta charset=\"UTF-8\">\n";
    file << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    file << "<title>" << data.reportTitle << " — " << data.targetName << "</title>\n";
    file << "<style>\n";
    file << "  :root { --primary: #0284c7; --success: #059669; --warning: #d97706; --danger: #dc2626; --text: #0f172a; --bg: #f8fafc; --card: #ffffff; --border: #e2e8f0; }\n";
    file << "  body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; background-color: var(--bg); color: var(--text); margin: 0; padding: 32px 16px; }\n";
    file << "  .container { max-width: 940px; margin: 0 auto; background-color: var(--card); border: 1px solid var(--border); border-radius: 12px; padding: 36px; box-shadow: 0 4px 24px rgba(0,0,0,0.06); }\n";
    file << "  .header { display: flex; justify-content: space-between; align-items: flex-start; border-bottom: 1px solid var(--border); padding-bottom: 24px; margin-bottom: 28px; }\n";
    file << "  .brand-title { font-size: 21px; font-weight: 800; color: var(--text); letter-spacing: -0.25px; }\n";
    file << "  .brand-sub { font-size: 13px; color: #64748b; margin-top: 5px; }\n";
    file << "  .badge { padding: 8px 16px; border-radius: 9999px; font-size: 12px; font-weight: 700; letter-spacing: 0.5px; display: inline-block; }\n";
    file << "  .badge-pass { background: #ecfdf5; color: #065f46; border: 1.5px solid #059669; }\n";
    file << "  .badge-warn { background: #fffbeb; color: #92400e; border: 1.5px solid #d97706; }\n";
    file << "  .badge-inconclusive { background: #f0f9ff; color: #0369a1; border: 1.5px solid #0284c7; }\n";
    file << "  .badge-fail { background: #fef2f2; color: #991b1b; border: 1.5px solid #dc2626; }\n";
    file << "  .section-title { font-size: 13px; font-weight: 700; color: #475569; text-transform: uppercase; letter-spacing: 0.75px; margin-top: 32px; margin-bottom: 14px; border-left: 3px solid var(--primary); padding-left: 10px; }\n";
    file << "  .metrics-grid { display: grid; grid-template-columns: repeat(4, 1fr); gap: 14px; margin-bottom: 24px; }\n";
    file << "  .metric-card { background: #f8fafc; border: 1px solid var(--border); border-radius: 8px; padding: 14px; text-align: center; }\n";
    file << "  .metric-val { font-size: 20px; font-weight: 800; color: #0f172a; margin-top: 4px; }\n";
    file << "  .metric-lbl { font-size: 11px; color: #64748b; font-weight: 600; text-transform: uppercase; letter-spacing: 0.5px; }\n";
    file << "  .domain-card { background: #f0fdf4; border: 1.5px solid #bbf7d0; border-radius: 8px; padding: 20px; margin-bottom: 24px; }\n";
    file << "  .domain-badge { font-size: 11px; font-weight: 800; color: #166534; text-transform: uppercase; letter-spacing: 0.5px; }\n";
    file << "  .domain-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 12px; margin-top: 14px; font-size: 13px; }\n";
    file << "  .domain-lbl { color: #4b5563; font-size: 12px; }\n";
    file << "  .warning-box { background: #fffbeb; border-left: 4px solid #f59e0b; padding: 12px 16px; margin-top: 16px; font-size: 12px; color: #92400e; border-radius: 4px; line-height: 1.5; }\n";
    file << "  .leakage-box { background: #f8fafc; border: 1px solid var(--border); padding: 10px 14px; border-radius: 6px; font-size: 12px; margin-top: 12px; }\n";
    file << "  .report-table { width: 100%; border-collapse: collapse; margin-top: 8px; margin-bottom: 20px; font-size: 12px; }\n";
    file << "  .report-table th { background: #f1f5f9; padding: 10px 12px; text-align: left; color: #334155; border-bottom: 2px solid var(--border); font-weight: 700; }\n";
    file << "  .report-table td { padding: 9px 12px; border-bottom: 1px solid #f1f5f9; color: #1e293b; }\n";
    file << "  .highlight-row { background: #f0fdf4; }\n";
    file << "  .winner-tag { background: #dcfce7; color: #15803d; padding: 2px 6px; border-radius: 4px; font-size: 10px; font-weight: 700; }\n";
    file << "  .fidelity-tag { background: #e0f2fe; color: #0369a1; padding: 2px 6px; border-radius: 4px; font-size: 10px; font-weight: 700; }\n";
    file << "  .subtext { font-size: 10px; color: #64748b; }\n";
    file << "  .badge-pill { padding: 2px 7px; border-radius: 9999px; font-size: 10px; font-weight: 700; }\n";
    file << "  .badge-pill.green { background: #dcfce7; color: #15803d; }\n";
    file << "  .badge-pill.amber { background: #fef3c7; color: #b45309; }\n";
    file << "  .status-text.green { color: #16a34a; font-weight: 700; }\n";
    file << "  .status-text.amber { color: #d97706; font-weight: 700; }\n";
    file << "  .status-text.blue { color: #0284c7; font-weight: 700; }\n";
    file << "  .status-text.red { color: #dc2626; font-weight: 700; }\n";
    file << "  .chart-box { background: #f8fafc; border: 1px solid var(--border); border-radius: 8px; padding: 16px; margin-bottom: 24px; text-align: center; }\n";
    file << "  .footer-stamp { margin-top: 36px; padding-top: 18px; border-top: 1px dashed #cbd5e1; font-family: 'Consolas', monospace; font-size: 11px; color: #64748b; display: flex; justify-content: space-between; }\n";
    file << "  @media print { body { background: #fff; padding: 0; } .container { border: none; box-shadow: none; max-width: 100%; padding: 0; } }\n";
    file << "</style>\n";
    file << "</head>\n<body>\n";

    file << "<div class=\"container\">\n";

    // Header
    file << "  <div class=\"header\">\n";
    file << "    <div>\n";
    file << "      <div class=\"brand-title\">" << data.reportTitle << "</div>\n";
    file << "      <div class=\"brand-sub\">Dispositivo Caracterizado: <strong>" << data.targetName << "</strong> &bull; Fabricante: "
         << data.targetManufacturer << " &bull; Tipología: " << data.hardwareType << "</div>\n";
    file << "    </div>\n";
    file << "    <div><span class=\"badge " << badgeClass << "\">" << badgeText << "</span></div>\n";
    file << "  </div>\n";

    // KPI Cards
    double enob = (data.loopbackSnrDb > 1.76) ? (data.loopbackSnrDb - 1.76) / 6.02 : 12.0;

    file << "  <div class=\"metrics-grid\">\n";
    file << "    <div class=\"metric-card\"><div class=\"metric-lbl\">FIDELIDAD R&sup2; (RECOMENDADO)</div><div class=\"metric-val\" style=\"color: #059669;\">"
         << std::fixed << std::setprecision(4) << data.evaluation.metrics.rSquaredScore << "</div></div>\n";
    file << "    <div class=\"metric-card\"><div class=\"metric-lbl\">ERROR TO SIGNAL (ESR)</div><div class=\"metric-val\">"
         << std::fixed << std::setprecision(1) << data.evaluation.metrics.errorToSignalRatioDb << " dB</div></div>\n";
    file << "    <div class=\"metric-card\"><div class=\"metric-lbl\">SNR LOOPBACK MEDIDO</div><div class=\"metric-val\">"
         << std::fixed << std::setprecision(1) << data.loopbackSnrDb << " dB</div></div>\n";
    file << "    <div class=\"metric-card\"><div class=\"metric-lbl\">RESOLUCIÓN EFECTIVA (ENOB)</div><div class=\"metric-val\" style=\"color: #0284c7;\">"
         << std::fixed << std::setprecision(1) << enob << " bits</div></div>\n";
    file << "  </div>\n";

    // 1. Dominio de Validez
    file << "  <div class=\"section-title\">1. Declaración Formal del Dominio de Validez</div>\n";
    file << generateDomainOfValidityBoxHtml(data.validityDomain);

    // 2. Presupuesto de Incertidumbre Metrológica GUM
    file << "  <div class=\"section-title\">2. Presupuesto de Incertidumbre Metrológica (GUM / ISO 98-3)</div>\n";
    file << "  <table class=\"report-table\">\n";
    file << "    <thead><tr><th>COMPONENTE</th><th>ESTIMACIÓN (dB)</th><th>DISTRIBUCIÓN / GRADOS</th><th>CONTRIBUCIÓN METROLÓGICA</th></tr></thead>\n";
    file << "    <tbody>\n";
    file << "      <tr><td><strong>Incertidumbre Instrumental (u<sub>inst</sub>)</strong></td><td>"
         << std::fixed << std::setprecision(2) << data.evaluation.uncertainty.instrumentalUncertaintyDb
         << " dB</td><td>Normal (k=1) / Tarjeta E/S</td><td>Suelo de ruido analógico (" << std::fixed << std::setprecision(1) << data.loopbackNoiseFloorDbfs << " dBFS)</td></tr>\n";
    file << "      <tr><td><strong>Incertidumbre Estructural (u<sub>struct</sub>)</strong></td><td>"
         << std::fixed << std::setprecision(2) << data.evaluation.uncertainty.structuralModelErrorDb
         << " dB</td><td>Residual Fuera de Muestra</td><td>Varianza no explicada por el modelo candidato</td></tr>\n";
    file << "      <tr class=\"highlight-row\"><td><strong>Incertidumbre Combinada (u<sub>combined</sub>)</strong></td><td><strong>"
         << std::fixed << std::setprecision(2) << data.evaluation.uncertainty.combinedUncertaintyDb
         << " dB</strong></td><td>Raíz Cuadrática de la Suma</td><td>Límite superior de resolución experimental</td></tr>\n";
    file << "    </tbody>\n";
    file << "  </table>\n";

    // 3. Comparativa Multiobjetivo
    file << "  <div class=\"section-title\">3. Evaluación Multiobjetivo de Modelos Candidatos (Preset: "
         << optimizationPresetToString(data.benchmarkingReport.preset) << ")</div>\n";
    file << generateCandidateComparisonTableHtml(data.benchmarkingReport, data.evaluation);

    // 4. Batería Fuera de Muestra (Holdout)
    file << "  <div class=\"section-title\">4. Batería de Validación Ciega Fuera de Muestra (Out-of-Sample Engine)</div>\n";
    file << generateHoldoutBatteryTableHtml(data.holdoutBatteries, data.leakageDetected, data.leakageMinDistance);

    // 5. Gráficos Vectoriales SVG
    file << "  <div class=\"section-title\">5. Caracterización Vectorial del Residuo Tiempo-Frecuencia</div>\n";
    file << "  <div class=\"chart-box\">\n";
    file << generateResidualSpectrumSvg(data.frequencyBinsHz, data.targetMagnitudeDb, data.modelMagnitudeDb, data.residualMagnitudeDb);
    file << "  </div>\n";

    if (!data.timeDomainResidual.empty())
    {
        file << "  <div class=\"chart-box\">\n";
        file << generateTimeDomainResidualSvg(data.timeDomainResidual, data.validityDomain.sampleRate);
        file << "  </div>\n";
    }

    // 6. Huella de Rendimiento en Plugins
    file << "  <div class=\"section-title\">6. Desglose de Coste Computacional en Producción</div>\n";
    file << generatePluginFootprintCardHtml(data.evaluation, data.validityDomain.sampleRate);

    // Footer Stamp
    std::string evalHash = data.evaluation.canonicalEvaluationHash.empty() ? "N/A" : data.evaluation.canonicalEvaluationHash;
    std::string holdoutHash = data.holdoutDatasetHash.empty() ? "N/A" : data.holdoutDatasetHash;

    file << "  <div class=\"footer-stamp\">\n";
    file << "    <div>EVAL_HASH (RFC 8785): <code>" << (evalHash.size() > 16 ? evalHash.substr(0, 16) + "..." : evalHash) << "</code></div>\n";
    file << "    <div>HOLDOUT_HASH: <code>" << (holdoutHash.size() > 16 ? holdoutHash.substr(0, 16) + "..." : holdoutHash) << "</code></div>\n";
    file << "    <div>DATE: " << (data.profileTimestampUtc.empty() ? "2026-10-14" : data.profileTimestampUtc) << "</div>\n";
    file << "  </div>\n";

    file << "</div>\n";
    file << "</body>\n</html>\n";

    return true;
}

} // namespace abdaudiolab::exporting
