/**
 * @file MeasurementComparisonReportGenerator.cpp
 * @brief Implementation of MeasurementComparisonReportGenerator.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementComparisonReportGenerator.h"
#include "MeasurementSvgGenerator.h"
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

    // 2. SVG Comparison Charts (Level & Timbre)
    std::vector<MeasurementSvgGenerator::SvgSeries> levelSeries;
    std::vector<MeasurementSvgGenerator::SvgSeries> timbreSeries;

    for (const auto& entry : eligible)
    {
        if (entry.viewModel == nullptr)
            continue;

        const auto& vm = *entry.viewModel;
        std::string label = vm.dutName.isNotEmpty() ? vm.dutName.toStdString() : entry.containerDir.getFileName().toStdString();

        // Level series (dBFS)
        MeasurementSvgGenerator::SvgSeries ls;
        ls.id = std::to_string(entry.id);
        ls.label = label;
        ls.lineStyle = entry.dashPatternIndex;
        ls.markerStyle = entry.markerShapeIndex;

        if (vm.dynamicsResult.has_value() && !vm.dynamicsResult->amplitudeCurve.x.empty())
        {
            const auto& crv = vm.dynamicsResult->amplitudeCurve;
            for (size_t i = 0; i < crv.x.size() && i < crv.y.size(); ++i)
                ls.points.push_back({ crv.x[i], crv.y[i] });
        }
        else
        {
            for (size_t i = 0; i < vm.curve.x.size() && i < vm.curve.y.size(); ++i)
                ls.points.push_back({ vm.curve.x[i], vm.curve.y[i] });
        }

        if (!ls.points.empty())
            levelSeries.push_back(std::move(ls));

        // Timbre series (Spectral Centroid Hz)
        if (vm.dynamicsResult.has_value() && !vm.dynamicsResult->brightnessCurve.x.empty())
        {
            MeasurementSvgGenerator::SvgSeries ts;
            ts.id = std::to_string(entry.id);
            ts.label = label;
            ts.lineStyle = entry.dashPatternIndex;
            ts.markerStyle = entry.markerShapeIndex;
            const auto& crv = vm.dynamicsResult->brightnessCurve;
            for (size_t i = 0; i < crv.x.size() && i < crv.y.size(); ++i)
                ts.points.push_back({ crv.x[i], crv.y[i] });

            if (!ts.points.empty())
                timbreSeries.push_back(std::move(ts));
        }
    }

    ss << "<div class=\"card\">\n<h2>Superposición de Dinámica (Velocidad MIDI vs dBFS)</h2>\n";
    MeasurementSvgGenerator::SvgPlotSpec levelSpec;
    levelSpec.xLabel = "Velocidad MIDI";
    levelSpec.yLabel = "Nivel (dBFS)";
    levelSpec.xMin = 0.0;
    levelSpec.xMax = 127.0;
    levelSpec.yMin = -96.0;
    levelSpec.yMax = 0.0;
    levelSpec.width = 800;
    levelSpec.height = 320;
    levelSpec.yUnit = "dB";
    levelSpec.xTicks = { 0.0, 32.0, 64.0, 96.0, 127.0 };
    levelSpec.yTicks = { 0.0, -24.0, -48.0, -72.0, -96.0 };
    ss << MeasurementSvgGenerator::generateMultiSeriesSvg(levelSeries, levelSpec);
    ss << "</div>\n";

    if (!timbreSeries.empty())
    {
        ss << "<div class=\"card\">\n<h2>Superposición de Timbre (Velocidad MIDI vs Centroide Hz)</h2>\n";
        MeasurementSvgGenerator::SvgPlotSpec timbreSpec;
        timbreSpec.xLabel = "Velocidad MIDI";
        timbreSpec.yLabel = "Centroide (Hz)";
        timbreSpec.xMin = 0.0;
        timbreSpec.xMax = 127.0;
        timbreSpec.yMin = 0.0;
        timbreSpec.yMax = 8000.0;
        timbreSpec.width = 800;
        timbreSpec.height = 320;
        timbreSpec.yUnit = "Hz";
        timbreSpec.xTicks = { 0.0, 32.0, 64.0, 96.0, 127.0 };
        timbreSpec.yTicks = { 8000.0, 6000.0, 4000.0, 2000.0, 0.0 };
        ss << MeasurementSvgGenerator::generateMultiSeriesSvg(timbreSeries, timbreSpec);
        ss << "</div>\n";
    }

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

    // 4. Excluded / Corrupt Containers & Traceable Exclusion Records
    ss << "<div class=\"card\">\n<h2>Contenedores Excluidos de Comparación</h2>\n";
    std::vector<abdaudiolab::gui::measurement::LoadedContainerEntry> excluded;
    for (const auto& c : allContainers)
    {
        if (!c.isEligibleForComparison())
            excluded.push_back(c);
    }

    const auto exclusionRecords = session.getExclusionRecords();

    if (excluded.empty() && exclusionRecords.empty())
    {
        ss << "<p>Ningún contenedor fue excluido. Todos los elementos cargados cumplen los criterios metrológicos y de integridad.</p>\n";
    }
    else
    {
        if (!excluded.empty())
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

        if (!exclusionRecords.empty())
        {
            ss << "<h3 style=\"margin-top: 16px; color: #ffb74d;\">Exclusiones Deterministas Registradas (Reproducibles)</h3>\n";
            ss << "<table>\n<thead><tr><th>Contenedor</th><th>Métrica</th><th>Código</th><th>Regla</th><th>Hash Base de Decisión</th><th>Diagnóstico</th></tr></thead>\n<tbody>\n";
            for (const auto& rec : exclusionRecords)
            {
                ss << "<tr>\n";
                ss << "<td>" << rec.containerId << "</td>\n";
                ss << "<td>" << rec.metric << "</td>\n";
                ss << "<td><code>" << rec.code << "</code></td>\n";
                ss << "<td>" << rec.rulesVersion << "</td>\n";
                ss << "<td class=\"sha\">" << rec.comparisonBasisHash.substr(0, 16) << "...</td>\n";
                ss << "<td>" << rec.message << "</td>\n";
                ss << "</tr>\n";
            }
            ss << "</tbody></table>\n";
        }
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
