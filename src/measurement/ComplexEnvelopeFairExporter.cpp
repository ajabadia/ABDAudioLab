/**
 * @file ComplexEnvelopeFairExporter.cpp
 * @brief Implementation of transactional FAIR/LNL container export and integrity validation.
 * @author ABDSynths
 * @date 2026
 */

#include "ComplexEnvelopeFairExporter.h"
#include "ComplexEnvelopeSvgRenderer.h"
#include "ComplexEnvelopeHtmlReportGenerator.h"
#include "../synth/Sha256.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <nlohmann/json.hpp>
#include <fstream>

namespace abdaudiolab::measurement
{

namespace
{

bool writeStringToFile(const juce::File& file, const std::string& content)
{
    if (file.exists()) file.deleteFile();
    file.getParentDirectory().createDirectory();

    juce::FileOutputStream stream(file);
    if (!stream.openedOk()) return false;
    return stream.write(content.data(), content.size());
}

bool writeWavSamples(const juce::File& file, std::span<const float> samples, double sampleRateHz)
{
    if (file.exists()) file.deleteFile();
    file.getParentDirectory().createDirectory();

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(
        new juce::FileOutputStream(file),
        sampleRateHz,
        1,
        16,
        {},
        0));

    if (writer == nullptr) return false;
    if (samples.empty()) return true;

    int totalSamples = static_cast<int>(samples.size());
    juce::AudioBuffer<float> buf(1, totalSamples);
    for (int i = 0; i < totalSamples; ++i)
    {
        buf.setSample(0, i, samples[static_cast<size_t>(i)]);
    }

    return writer->writeFromAudioSampleBuffer(buf, 0, totalSamples);
}

std::optional<std::string> computeFileSha256(const juce::File& file)
{
    juce::MemoryBlock mb;
    if (!file.loadFileAsData(mb)) return std::nullopt;
    return abdaudiolab::synth::Sha256::computeHex(mb.getData(), mb.getSize());
}

bool isPathSafe(const std::string& relPath)
{
    if (relPath.empty()) return false;
    if (relPath.find("..") != std::string::npos) return false;
    if (relPath.find(':') != std::string::npos) return false;
    if (relPath.front() == '/' || relPath.front() == '\\') return false;
    return true;
}

} // anonymous namespace

bool ComplexEnvelopeFairExporter::exportContainer(
    const juce::File& targetDir,
    const ComplexEnvelopeOrchestrationResult& result,
    const ComplexEnvelopeExportSpec& spec,
    const std::vector<EnvelopeStageDescriptor>& nativeStages,
    std::span<const float> rawAudio,
    std::span<const float> compensatedAudio,
    juce::String& outError) noexcept
{
    outError = {};

    // 1. Validate Target Directory Path
    if (targetDir.getFullPathName().isEmpty())
    {
        outError = "Target directory path is empty.";
        return false;
    }

    // 2. Setup Staging Directory
    juce::File parentDir = targetDir.getParentDirectory();
    if (!parentDir.exists() && !parentDir.createDirectory())
    {
        outError = "Failed to create parent directory for container.";
        return false;
    }

    juce::File stagingDir = parentDir.getChildFile(targetDir.getFileName() + "_staging");
    if (stagingDir.exists())
    {
        stagingDir.deleteRecursively();
    }
    if (!stagingDir.createDirectory())
    {
        outError = "Failed to create staging directory for transactional export.";
        return false;
    }

    auto rollback = [&]() {
        stagingDir.deleteRecursively();
    };

    ComplexEnvelopeContainerManifest manifest;
    manifest.schemaVersion = "1.0.0";
    manifest.containerId = "lnl_" + spec.experimentId;
    manifest.experimentId = spec.experimentId;
    manifest.timestampPolicy = spec.timestampPolicy;
    manifest.exportTimestampUtc = (spec.timestampPolicy == TimestampPolicy::FixedForTest)
                                      ? spec.fixedTimestamp
                                      : spec.creationClock;

    manifest.timingResolution = result.timingResolution.toCanonicalJson();
    if (result.appliedCalibration.has_value())
    {
        manifest.calibration = result.appliedCalibration->toCanonicalJson();
    }
    else
    {
        manifest.calibration = nullptr;
    }

    nlohmann::ordered_json dutInfo;
    dutInfo["model"] = "CZ-101";
    dutInfo["sourceRawSha256"] = result.sourceRawAudioSha256;
    dutInfo["vendor"] = "Casio";
    manifest.dutIdentity = dutInfo;

    nlohmann::ordered_json conditions;
    conditions["midiVelocity"] = result.captureRecord.midiVelocity;
    conditions["nominalFrequencyHz"] = result.captureRecord.excitationFrequencyHz;
    conditions["noteDurationMs"] = result.captureRecord.noteDurationMs;
    conditions["sampleRateHz"] = spec.audioSampleRateHz;
    conditions["totalDurationMs"] = result.captureRecord.totalDurationMs;
    manifest.measurementConditions = conditions;

    std::vector<ComplexEnvelopeExportArtifact> artifacts;

    auto addArtifact = [&](const std::string& relPath,
                           const std::string& role,
                           const std::string& mediaType,
                           const std::string& canonicalization) -> bool {
        if (!isPathSafe(relPath))
        {
            outError = "Forbidden path traversal detected in artifact relativePath: " + juce::String(relPath);
            return false;
        }

        juce::File artFile = stagingDir.getChildFile(relPath);
        if (!artFile.existsAsFile())
        {
            outError = "Expected artifact was not written to disk: " + juce::String(relPath);
            return false;
        }

        auto sha = computeFileSha256(artFile);
        if (!sha.has_value())
        {
            outError = "Failed to compute SHA-256 for artifact: " + juce::String(relPath);
            return false;
        }

        ComplexEnvelopeExportArtifact a;
        a.relativePath = relPath;
        a.role = role;
        a.mediaType = mediaType;
        a.byteLength = static_cast<uint64_t>(artFile.getSize());
        a.sha256 = *sha;
        a.canonicalization = canonicalization;
        artifacts.push_back(a);
        return true;
    };

    // 3. Write spec.json
    {
        std::string specPath = "spec.json";
        if (!writeStringToFile(stagingDir.getChildFile(specPath), spec.toCanonicalJson().dump(2)))
        {
            outError = "Failed to write spec.json";
            rollback();
            return false;
        }
        if (!addArtifact(specPath, "envelope_spec", "application/json", "rfc8785"))
        {
            rollback();
            return false;
        }
    }

    // 4. Write data/envelope_record.json
    {
        std::string recPath = "data/envelope_record.json";
        if (!writeStringToFile(stagingDir.getChildFile(recPath), result.captureRecord.toCanonicalJson().dump(2)))
        {
            outError = "Failed to write data/envelope_record.json";
            rollback();
            return false;
        }
        if (!addArtifact(recPath, "envelope_record", "application/json", "rfc8785"))
        {
            rollback();
            return false;
        }
    }

    // 5. Write data/comparison_report.json
    {
        std::string compPath = "data/comparison_report.json";
        nlohmann::ordered_json compArray = nlohmann::ordered_json::array();
        for (const auto& c : result.comparisons)
        {
            compArray.push_back(c.toCanonicalJson());
        }

        if (!writeStringToFile(stagingDir.getChildFile(compPath), compArray.dump(2)))
        {
            outError = "Failed to write data/comparison_report.json";
            rollback();
            return false;
        }
        if (!addArtifact(compPath, "observable_comparison", "application/json", "rfc8785"))
        {
            rollback();
            return false;
        }
    }

    // 6. Write Audio Files (if requested)
    std::string relRawAudio = "";
    std::string relCompAudio = "";

    if (spec.includeRawAudio && !rawAudio.empty())
    {
        std::string rawWavPath = "audio/raw_capture.wav";
        if (!writeWavSamples(stagingDir.getChildFile(rawWavPath), rawAudio, spec.audioSampleRateHz))
        {
            outError = "Failed to write audio/raw_capture.wav";
            rollback();
            return false;
        }
        if (!addArtifact(rawWavPath, "raw_audio", "audio/wav", "pcm_wav"))
        {
            rollback();
            return false;
        }
        relRawAudio = "../audio/raw_capture.wav";
    }

    if (spec.includeCompensatedAudio && !compensatedAudio.empty())
    {
        std::string compWavPath = "audio/compensated_capture.wav";
        if (!writeWavSamples(stagingDir.getChildFile(compWavPath), compensatedAudio, spec.audioSampleRateHz))
        {
            outError = "Failed to write audio/compensated_capture.wav";
            rollback();
            return false;
        }
        if (!addArtifact(compWavPath, "compensated_audio", "audio/wav", "pcm_wav"))
        {
            rollback();
            return false;
        }
        relCompAudio = "../audio/compensated_capture.wav";
    }

    // 7. Write reports/complex_envelope_overlay.svg (if requested)
    if (spec.generateSvgOverlay)
    {
        std::string svgPath = "reports/complex_envelope_overlay.svg";
        std::string svgContent = ComplexEnvelopeSvgRenderer::renderOverlaySvg(
            result.captureRecord, result.comparisons, nativeStages, EnvelopeDomain::Timbre);

        std::string svgError;
        if (!ComplexEnvelopeSvgRenderer::validateSvgSafety(svgContent, svgError))
        {
            outError = "Generated SVG violated safety standards: " + juce::String(svgError);
            rollback();
            return false;
        }

        if (!writeStringToFile(stagingDir.getChildFile(svgPath), svgContent))
        {
            outError = "Failed to write reports/complex_envelope_overlay.svg";
            rollback();
            return false;
        }
        if (!addArtifact(svgPath, "vector_svg_overlay", "image/svg+xml", "none"))
        {
            rollback();
            return false;
        }
    }

    // 8. Write reports/envelope_report.html (if requested)
    if (spec.generateInteractiveHtml)
    {
        std::string htmlPath = "reports/envelope_report.html";
        std::string htmlContent = ComplexEnvelopeHtmlReportGenerator::generateInteractiveReportHtml(
            result, spec, nativeStages, relRawAudio, relCompAudio);

        if (!writeStringToFile(stagingDir.getChildFile(htmlPath), htmlContent))
        {
            outError = "Failed to write reports/envelope_report.html";
            rollback();
            return false;
        }
        if (!addArtifact(htmlPath, "interactive_report", "text/html", "none"))
        {
            rollback();
            return false;
        }
    }

    // 9. Write manifest.json (Non-circular: manifest is not inside its own artifacts list)
    manifest.artifacts = std::move(artifacts);
    {
        std::string manifestContent = manifest.toCanonicalJson().dump(2);
        if (!writeStringToFile(stagingDir.getChildFile("manifest.json"), manifestContent))
        {
            outError = "Failed to write manifest.json";
            rollback();
            return false;
        }
    }

    // 10. Atomic Rename / Commit Staging to Target Directory
    if (targetDir.exists())
    {
        if (!targetDir.deleteRecursively())
        {
            outError = "Failed to remove pre-existing target container directory.";
            rollback();
            return false;
        }
    }

    if (!stagingDir.moveFileTo(targetDir))
    {
        outError = "Failed to move staging directory to target container directory.";
        rollback();
        return false;
    }

    return true;
}

bool ComplexEnvelopeFairExporter::validateContainerIntegrity(
    const juce::File& containerDir,
    juce::String& outError) noexcept
{
    outError = {};

    if (!containerDir.isDirectory())
    {
        outError = "Container path is not an accessible directory: " + containerDir.getFullPathName();
        return false;
    }

    juce::File manifestFile = containerDir.getChildFile("manifest.json");
    if (!manifestFile.existsAsFile())
    {
        outError = "Container lacks manifest.json: " + containerDir.getFullPathName();
        return false;
    }

    std::string manifestStr = manifestFile.loadFileAsString().toStdString();
    nlohmann::json j;
    try
    {
        j = nlohmann::json::parse(manifestStr);
    }
    catch (const std::exception& e)
    {
        outError = "Failed to parse manifest.json: " + juce::String(e.what());
        return false;
    }

    if (!j.contains("artifacts") || !j["artifacts"].is_array())
    {
        outError = "manifest.json does not contain valid 'artifacts' array.";
        return false;
    }

    for (const auto& art : j["artifacts"])
    {
        if (!art.contains("relativePath") || !art.contains("sha256"))
        {
            outError = "Artifact descriptor missing required relativePath or sha256 fields.";
            return false;
        }

        std::string relPath = art["relativePath"].get<std::string>();
        std::string expectedSha = art["sha256"].get<std::string>();

        if (!isPathSafe(relPath))
        {
            outError = "Manifest contains unsafe artifact path: " + juce::String(relPath);
            return false;
        }

        juce::File artFile = containerDir.getChildFile(relPath);
        if (!artFile.existsAsFile())
        {
            outError = "Declared artifact missing from disk: " + juce::String(relPath);
            return false;
        }

        auto actualSha = computeFileSha256(artFile);
        if (!actualSha.has_value() || *actualSha != expectedSha)
        {
            outError = "Artifact SHA-256 mismatch for: " + juce::String(relPath) +
                       " (expected " + juce::String(expectedSha) + ", found " +
                       juce::String(actualSha.value_or("none")) + ")";
            return false;
        }
    }

    return true;
}

} // namespace abdaudiolab::measurement
