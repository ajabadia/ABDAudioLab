/**
 * @file MeasurementComparisonReportGenerator.cpp
 * @brief Implementation of MeasurementComparisonReportGenerator.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementComparisonReportGenerator.h"
#include <sstream>
#include <iomanip>

namespace abdaudiolab::measurement
{

bool MeasurementComparisonReportGenerator::generateReport(const abdaudiolab::gui::measurement::MeasurementComparisonSession& session,
                                                          const juce::File& destinationFile,
                                                          juce::String& outError)
{
    const juce::String html = generateReportHtml(session);
    destinationFile.deleteFile();
    if (!destinationFile.replaceWithText(html))
    {
        outError = "Failed to write HTML report to: " + destinationFile.getFullPathName();
        return false;
    }
    return true;
}

juce::String MeasurementComparisonReportGenerator::generateReportHtml(const abdaudiolab::gui::measurement::MeasurementComparisonSession& session)
{
    const auto eligible = session.getEligibleComparisonContainers();
    const auto allContainers = session.getFilteredContainers();

    std::ostringstream ss;
    ss << "<!DOCTYPE html>\n<html lang=\"es\">\n<head>\n";
    ss << "<meta charset=\"UTF-8\">\n";
    ss << "<title>ABDAudioLab — Informe Comparativo FAIR/LNL</title>\n";
    ss << "<style>\n"
       << "  body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; background: #0e0e13; color: #e0e0e8; margin: 0; padding: 24px; }\n"
       << "  .container { max-width: 1100px; margin: 0 auto; }\n"
       << "  h1 { color: #00d4ff; font-size: 24px; margin-bottom: 4px; }\n"
       << "  .subtitle { color: #8e8ea0; font-size: 13px; margin-bottom: 24px; }\n"
       << "  .card { background: #181820; border: 1px solid #282836; border-radius: 8px; padding: 20px; margin-bottom: 20px; }\n"
       << "  .badge { display: inline-block; padding: 3px 8px; border-radius: 4px; font-size: 11px; font-weight: bold; text-transform: uppercase; }\n"
       << "  .badge-verified { background: #00c853; color: #fff; }\n"
       << "  .badge-corrupt { background: #d50000; color: #fff; }\n"
       << "  .badge-excluded { background: #e65100; color: #fff; }\n"
       << "  .badge-bitexact { background: #00e676; color: #000; }\n"
       << "  .badge-semantic { background: #ffb300; color: #000; }\n"
       << "  .badge-notcomp { background: #546e7a; color: #fff; }\n"
       << "  table { width: 100%; border-collapse: collapse; margin-top: 12px; font-size: 12px; }\n"
       << "  th { text-align: left; padding: 8px; background: #22222e; color: #00d4ff; border-bottom: 1px solid #333344; }\n"
       << "  td { padding: 8px; border-bottom: 1px solid #252535; }\n"
       << "  .sha { font-family: monospace; font-size: 10px; color: #a0a0c0; }\n"
       << "  svg { background: #121218; border-radius: 6px; border: 1px solid #282838; width: 100%; height: auto; }\n"
       << "</style>\n</head>\n<body>\n<div class=\"container\">\n";

    ss << "<h1>ABDAudioLab — Informe Comparativo Multivariante FAIR / LNL</h1>\n";
    ss << "<div class=\"subtitle\">Generado: " << juce::Time::getCurrentTime().toISO8601(true).toStdString()
       << " | Dominios Metrológicos Segregados | Cero Suposiciones de Linealidad</div>\n";

    // 1. Executive Summary Card
    ss << "<div class=\"card\">\n<h2>Resumen de Sesión</h2>\n";
    ss << "<p>Contenedores Totales: <strong>" << allContainers.size()
       << "</strong> | Verificados: <strong>" << eligible.size()
       << "</strong> | Excluidos / Corruptos: <strong>" << (allContainers.size() - eligible.size())
       << "</strong></p>\n</div>\n";

    // 2. SVG Comparison Chart
    ss << "<div class=\"card\">\n<h2>Superposición de Dinámica (Velocidad MIDI vs dBFS)</h2>\n";
    ss << "<svg viewBox=\"0 0 800 320\" xmlns=\"http://www.w3.org/2000/svg\">\n";

    // Grid
    ss << "<rect x=\"50\" y=\"20\" width=\"550\" height=\"260\" fill=\"#161620\" stroke=\"#2e2e40\"/>\n";
    for (int v : { 0, 32, 64, 96, 127 })
    {
        const float x = 50.0f + (static_cast<float>(v) / 127.0f) * 550.0f;
        ss << "<line x1=\"" << x << "\" y1=\"20\" x2=\"" << x << "\" y2=\"280\" stroke=\"#262636\"/>\n";
        ss << "<text x=\"" << x << "\" y=\"295\" fill=\"#8e8ea0\" font-size=\"10\" text-anchor=\"middle\">" << v << "</text>\n";
    }
    for (int db = 0; db >= -96; db -= 24)
    {
        const float frac = static_cast<float>(db + 96) / 96.0f;
        const float y = 280.0f - frac * 260.0f;
        ss << "<line x1=\"50\" y1=\"" << y << "\" x2=\"600\" y2=\"" << y << "\" stroke=\"#262636\"/>\n";
        ss << "<text x=\"42\" y=\"" << y + 3.0f << "\" fill=\"#8e8ea0\" font-size=\"10\" text-anchor=\"end\">" << db << " dB</text>\n";
    }

    // Series
    const char* kColours[] = { "#00d4ff", "#ffa726", "#e040fb", "#76ff03", "#ff5252" };
    int seriesIdx = 0;

    for (const auto& entry : eligible)
    {
        if (entry.viewModel == nullptr)
            continue;

        const auto& vm = *entry.viewModel;
        std::vector<std::pair<double, double>> pts;

        if (vm.dynamicsResult.has_value() && !vm.dynamicsResult->amplitudeCurve.x.empty())
        {
            const auto& crv = vm.dynamicsResult->amplitudeCurve;
            for (size_t i = 0; i < crv.x.size() && i < crv.y.size(); ++i)
                pts.push_back({ crv.x[i], crv.y[i] });
        }
        else
        {
            for (size_t i = 0; i < vm.curve.x.size() && i < vm.curve.y.size(); ++i)
                pts.push_back({ vm.curve.x[i], vm.curve.y[i] });
        }

        if (pts.empty())
            continue;

        const char* col = kColours[seriesIdx % 5];
        std::string dashAttr = "";
        if (entry.dashPatternIndex == 1) dashAttr = "stroke-dasharray=\"6,3\" ";
        else if (entry.dashPatternIndex == 2) dashAttr = "stroke-dasharray=\"8,3,2,3\" ";
        else if (entry.dashPatternIndex == 3) dashAttr = "stroke-dasharray=\"2,2\" ";

        ss << "<polyline fill=\"none\" stroke=\"" << col << "\" stroke-width=\"2.5\" " << dashAttr << "points=\"";
        for (const auto& pt : pts)
        {
            const float x = 50.0f + (static_cast<float>(pt.first) / 127.0f) * 550.0f;
            const float frac = static_cast<float>((pt.second + 96.0) / 96.0);
            const float y = 280.0f - std::max(0.0f, std::min(1.0f, frac)) * 260.0f;
            ss << x << "," << y << " ";
        }
        ss << "\"/>\n";

        // Draw points with markers
        for (const auto& pt : pts)
        {
            const float x = 50.0f + (static_cast<float>(pt.first) / 127.0f) * 550.0f;
            const float frac = static_cast<float>((pt.second + 96.0) / 96.0);
            const float y = 280.0f - std::max(0.0f, std::min(1.0f, frac)) * 260.0f;

            if (entry.markerShapeIndex == 1) // rect
                ss << "<rect x=\"" << x - 3.5f << "\" y=\"" << y - 3.5f << "\" width=\"7\" height=\"7\" fill=\"" << col << "\" stroke=\"#000\"/>\n";
            else
                ss << "<circle cx=\"" << x << "\" cy=\"" << y << "\" r=\"4\" fill=\"" << col << "\" stroke=\"#000\"/>\n";
        }

        // Legend entry in SVG
        const float legY = 40.0f + seriesIdx * 20.0f;
        ss << "<line x1=\"615\" y1=\"" << legY << "\" x2=\"635\" y2=\"" << legY << "\" stroke=\"" << col << "\" stroke-width=\"2.5\" " << dashAttr << "/>\n";
        ss << "<circle cx=\"625\" cy=\"" << legY << "\" r=\"3.5\" fill=\"" << col << "\"/>\n";
        std::string legLabel = vm.dutName.isNotEmpty() ? vm.dutName.toStdString() : entry.containerDir.getFileName().toStdString();
        ss << "<text x=\"645\" y=\"" << legY + 3.0f << "\" fill=\"#e0e0e0\" font-size=\"11\">" << legLabel << "</text>\n";

        seriesIdx++;
    }

    ss << "</svg>\n</div>\n";

    // 3. Table of Eligible Verified Series
    ss << "<div class=\"card\">\n<h2>Series Verificadas y Comparadas</h2>\n";
    ss << "<table>\n<thead><tr><th>ID</th><th>DUT / Modelo</th><th>Dominio</th><th>Métrica</th><th>Unidad</th><th>Puntos</th><th>Audio Artifact</th><th>SHA-256</th></tr></thead>\n<tbody>\n";

    for (const auto& entry : eligible)
    {
        if (entry.viewModel == nullptr) continue;
        const auto& vm = *entry.viewModel;
        ss << "<tr>\n";
        ss << "<td>" << entry.id << "</td>\n";
        ss << "<td><strong>" << (vm.dutName.isNotEmpty() ? vm.dutName.toStdString() : "DUT") << "</strong></td>\n";
        ss << "<td>" << vm.executionDomainText.toStdString() << "</td>\n";
        ss << "<td>" << (vm.curve.yName.isNotEmpty() ? vm.curve.yName.toStdString() : vm.measurementType.toStdString()) << "</td>\n";
        ss << "<td>" << vm.curve.yUnit.toStdString() << "</td>\n";
        ss << "<td>" << vm.curve.x.size() << "</td>\n";
        ss << "<td>" << vm.audioFile.getFileName().toStdString() << "</td>\n";
        ss << "<td class=\"sha\">" << vm.expectedAudioSha256.toStdString() << "</td>\n";
        ss << "</tr>\n";
    }
    ss << "</tbody></table>\n</div>\n";

    // 4. Excluded / Corrupt Containers
    ss << "<div class=\"card\">\n<h2>Contenedores Excluidos de Comparación</h2>\n";
    std::vector<abdaudiolab::gui::measurement::LoadedContainerEntry> excluded;
    for (const auto& c : allContainers)
    {
        if (!c.isEligibleForComparison())
            excluded.push_back(c);
    }

    if (excluded.empty())
    {
        ss << "<p>Ningún contenedor fue excluido. Todos los elementos cargados cumplen los criterios metrológicos y de integridad.</p>\n";
    }
    else
    {
        ss << "<table>\n<thead><tr><th>ID</th><th>Carpeta / Nombre</th><th>Estado</th><th>Motivo de Exclusión</th></tr></thead>\n<tbody>\n";
        for (const auto& c : excluded)
        {
            ss << "<tr>\n<td>" << c.id << "</td>\n";
            ss << "<td>" << c.containerDir.getFileName().toStdString() << "</td>\n";
            std::string badgeClass = (c.loadState == abdaudiolab::gui::measurement::ContainerLoadState::Corrupt) ? "badge-corrupt" : "badge-excluded";
            ss << "<td><span class=\"badge " << badgeClass << "\">" << abdaudiolab::gui::measurement::containerLoadStateToString(c.loadState).toStdString() << "</span></td>\n";
            ss << "<td>" << (c.diagnosticReason.isNotEmpty() ? c.diagnosticReason.toStdString() : "Incompatible or unverified") << "</td>\n</tr>\n";
        }
        ss << "</tbody></table>\n";
    }
    ss << "</div>\n";

    // 5. Pairwise State Equivalence Matrix
    ss << "<div class=\"card\">\n<h2>Matriz de Equivalencia de Estado por Pares (A ↔ B)</h2>\n";
    ss << "<table>\n<thead><tr><th>Par de Comparación</th><th>Clasificación</th><th>Razón Metrológica</th><th>Max Δ Audio</th><th>SHA Plugin A vs B</th></tr></thead>\n<tbody>\n";

    for (size_t i = 0; i < eligible.size(); ++i)
    {
        for (size_t j = i + 1; j < eligible.size(); ++j)
        {
            const auto pairRes = session.compareContainers(eligible[i].id, eligible[j].id);
            std::string badgeClass = "badge-notcomp";
            if (pairRes.equivalence == abdaudiolab::gui::measurement::PairwiseStateEquivalence::BitExact)
                badgeClass = "badge-bitexact";
            else if (pairRes.equivalence == abdaudiolab::gui::measurement::PairwiseStateEquivalence::SemanticallyEquivalent)
                badgeClass = "badge-semantic";
            else if (pairRes.equivalence == abdaudiolab::gui::measurement::PairwiseStateEquivalence::NotEquivalent)
                badgeClass = "badge-corrupt";

            ss << "<tr>\n";
            ss << "<td><strong>" << pairRes.containerLabelA.toStdString() << "</strong> ↔ <strong>" << pairRes.containerLabelB.toStdString() << "</strong></td>\n";
            ss << "<td><span class=\"badge " << badgeClass << "\">" << abdaudiolab::gui::measurement::pairwiseEquivalenceToString(pairRes.equivalence).toStdString() << "</span></td>\n";
            ss << "<td>" << pairRes.reason.toStdString() << "</td>\n";
            ss << "<td>" << std::fixed << std::setprecision(6) << pairRes.maxAudioDelta << "</td>\n";
            ss << "<td class=\"sha\">" << pairRes.pluginBinarySha256A.substring(0, 8).toStdString() << "... vs " << pairRes.pluginBinarySha256B.substring(0, 8).toStdString() << "...</td>\n";
            ss << "</tr>\n";
        }
    }
    ss << "</tbody></table>\n</div>\n";

    ss << "</div>\n</body>\n</html>\n";
    return juce::String(ss.str());
}

} // namespace abdaudiolab::measurement
