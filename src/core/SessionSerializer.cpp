#include "SessionSerializer.h"
#include <juce_core/juce_core.h>

namespace abdaudiolab::core
{

SessionSerializer::SessionSerializer()
{
    auto tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ABDAudioLab_Sessions");
    tempRoot.createDirectory();

    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("ABDAudioLab");
    appData.createDirectory();
    autoSaveFile = appData.getChildFile("~autosave_session.abdlabtest");

    workingTempDir = tempRoot.getChildFile("work_" + juce::String::toHexString(juce::Random::getSystemRandom().nextInt64()));
    workingTempDir.createDirectory();
    workingTempDir.getChildFile("raw_audio").createDirectory();
    workingTempDir.getChildFile("lut_matrices").createDirectory();
}

SessionSerializer::~SessionSerializer()
{
    cleanupTempSession();
}

void SessionSerializer::cleanupTempSession()
{
    if (workingTempDir.isDirectory())
    {
        workingTempDir.deleteRecursively();
    }
}

juce::File SessionSerializer::getRecoverableAutoSaveFile() const
{
    if (autoSaveFile.existsAsFile() && autoSaveFile.getSize() > 100)
    {
        return autoSaveFile;
    }
    return {};
}

nlohmann::json SessionSerializer::serializeManifestToJson(const SessionManifest& manifest)
{
    nlohmann::json j;
    j["appVersion"] = manifest.appVersion;
    j["buildNumber"] = manifest.buildNumber;
    j["formatVersion"] = manifest.formatVersion;
    j["timestamp"] = manifest.timestamp.empty() ? juce::Time::getCurrentTime().toISO8601(true).toStdString() : manifest.timestamp;
    j["hardwareId"] = manifest.hardwareId;
    j["hardwareDisplayName"] = manifest.hardwareDisplayName;
    j["activeFunctionId"] = manifest.activeFunctionId;
    j["activeFunctionName"] = manifest.activeFunctionName;
    j["sampleRate"] = manifest.sampleRate;
    j["lineCalibrationGainDb"] = manifest.lineCalibrationGainDb;
    j["noiseFloorThresholdDb"] = manifest.noiseFloorThresholdDb;
    j["totalMeasuredPoints"] = manifest.totalMeasuredPoints;

    nlohmann::json testsJson = nlohmann::json::array();
    for (const auto& t : manifest.tests)
    {
        nlohmann::json tj;
        tj["testName"] = t.testName.toStdString();
        tj["stimulusType"] = static_cast<int>(t.stimulusType);
        tj["burstDurationSec"] = t.burstDurationSec;
        tj["captureMode"] = t.captureMode.toStdString();

        nlohmann::json ctrls = nlohmann::json::array();
        for (const auto& c : t.controls)
        {
            nlohmann::json cj;
            cj["name"] = c.name.toStdString();
            cj["type"] = c.type.toStdString();
            cj["steps"] = c.steps;
            ctrls.push_back(cj);
        }
        tj["controls"] = ctrls;
        testsJson.push_back(tj);
    }
    j["tests"] = testsJson;
    j["recordedAudioFiles"] = manifest.recordedAudioFiles;
    j["matrixFiles"] = manifest.matrixFiles;

    return j;
}

bool SessionSerializer::deserializeManifestFromJson(const nlohmann::json& j, SessionManifest& outManifest)
{
    try
    {
        if (j.contains("appVersion")) outManifest.appVersion = j["appVersion"].get<std::string>();
        if (j.contains("buildNumber")) outManifest.buildNumber = j["buildNumber"].get<int>();
        if (j.contains("formatVersion")) outManifest.formatVersion = j["formatVersion"].get<std::string>();
        if (j.contains("timestamp")) outManifest.timestamp = j["timestamp"].get<std::string>();
        if (j.contains("hardwareId")) outManifest.hardwareId = j["hardwareId"].get<std::string>();
        if (j.contains("hardwareDisplayName")) outManifest.hardwareDisplayName = j["hardwareDisplayName"].get<std::string>();
        if (j.contains("activeFunctionId")) outManifest.activeFunctionId = j["activeFunctionId"].get<std::string>();
        if (j.contains("activeFunctionName")) outManifest.activeFunctionName = j["activeFunctionName"].get<std::string>();
        if (j.contains("sampleRate")) outManifest.sampleRate = j["sampleRate"].get<double>();
        if (j.contains("lineCalibrationGainDb")) outManifest.lineCalibrationGainDb = j["lineCalibrationGainDb"].get<float>();
        if (j.contains("noiseFloorThresholdDb")) outManifest.noiseFloorThresholdDb = j["noiseFloorThresholdDb"].get<float>();
        if (j.contains("totalMeasuredPoints")) outManifest.totalMeasuredPoints = j["totalMeasuredPoints"].get<int>();

        outManifest.tests.clear();
        if (j.contains("tests") && j["tests"].is_array())
        {
            for (const auto& tj : j["tests"])
            {
                gui::TestConfiguration tc;
                if (tj.contains("testName")) tc.testName = juce::String(tj["testName"].get<std::string>());
                if (tj.contains("stimulusType")) tc.stimulusType = static_cast<audio::StimulusType>(tj["stimulusType"].get<int>());
                if (tj.contains("burstDurationSec")) tc.burstDurationSec = tj["burstDurationSec"].get<float>();
                if (tj.contains("captureMode")) tc.captureMode = juce::String(tj["captureMode"].get<std::string>());

                if (tj.contains("controls") && tj["controls"].is_array())
                {
                    for (const auto& cj : tj["controls"])
                    {
                        gui::ControlStepConfig c;
                        if (cj.contains("name")) c.name = juce::String(cj["name"].get<std::string>());
                        if (cj.contains("type")) c.type = juce::String(cj["type"].get<std::string>());
                        if (cj.contains("steps")) c.steps = cj["steps"].get<int>();
                        tc.controls.push_back(c);
                    }
                }
                outManifest.tests.push_back(tc);
            }
        }
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool SessionSerializer::saveSessionToPackage(const juce::File& targetPackageFile,
                                             const SessionManifest& manifest,
                                             const std::vector<exporting::MeasuredPoint>& points)
{
    workingTempDir.createDirectory();
    auto rawAudioDir = workingTempDir.getChildFile("raw_audio");
    rawAudioDir.createDirectory();
    auto lutDir = workingTempDir.getChildFile("lut_matrices");
    lutDir.createDirectory();

    // 1. Write session_manifest.json
    auto manifestFile = workingTempDir.getChildFile("session_manifest.json");
    auto jsonObj = serializeManifestToJson(manifest);
    manifestFile.replaceWithText(jsonObj.dump(2));

    // 2. Write binary measured points
    auto pointsFile = lutDir.getChildFile("measured_points.bin");
    {
        juce::MemoryOutputStream mos;
        int numPoints = static_cast<int>(points.size());
        mos.writeInt(numPoints);
        for (const auto& pt : points)
        {
            mos.writeString(pt.testId);
            mos.writeFloat(pt.param1Normalized);
            mos.writeFloat(pt.param2Normalized);
            mos.writeFloat(pt.muSigmaValue.mean);
            mos.writeFloat(pt.muSigmaValue.stdDev);
            mos.writeFloat(pt.secondaryValue.mean);
            mos.writeFloat(pt.secondaryValue.stdDev);
            mos.writeFloat(pt.thdValue.mean);
            mos.writeFloat(pt.thdValue.stdDev);
        }
        pointsFile.replaceWithData(mos.getData(), mos.getDataSize());
    }

    // 3. Package working folder into target .abdlabtest using juce::ZipFile::Builder
    juce::ZipFile::Builder builder;
    
    // Add all files recursively from working directory
    juce::Array<juce::File> allFiles;
    workingTempDir.findChildFiles(allFiles, juce::File::findFiles, true);

    for (const auto& file : allFiles)
    {
        auto relPath = file.getRelativePathFrom(workingTempDir).replaceCharacter('\\', '/');
        builder.addFile(file, 6, relPath);
    }

    juce::File tempZip = targetPackageFile.getSiblingFile(targetPackageFile.getFileNameWithoutExtension() + "_tmp.zip");
    if (tempZip.existsAsFile()) tempZip.deleteFile();

    {
        juce::FileOutputStream fos(tempZip);
        if (!fos.openedOk() || !builder.writeToStream(fos, nullptr))
            return false;
    }

    if (targetPackageFile.existsAsFile())
        targetPackageFile.deleteFile();

    if (!tempZip.moveFileTo(targetPackageFile))
        return false;

    activeSessionFile = targetPackageFile;
    return true;
}

bool SessionSerializer::loadSessionFromPackage(const juce::File& sourcePackageFile,
                                               SessionManifest& outManifest,
                                               std::vector<exporting::MeasuredPoint>& outPoints,
                                               juce::String& outErrorMessage)
{
    if (!sourcePackageFile.existsAsFile())
    {
        outErrorMessage = "Session container file does not exist: " + sourcePackageFile.getFullPathName();
        return false;
    }

    cleanupTempSession();
    workingTempDir.createDirectory();

    juce::ZipFile zip(sourcePackageFile);
    if (zip.getNumEntries() == 0)
    {
        outErrorMessage = "Corrupted or empty .abdlabtest package archive.";
        return false;
    }

    auto uncompressResult = zip.uncompressTo(workingTempDir);
    if (uncompressResult.failed())
    {
        outErrorMessage = "Failed to extract package archive: " + uncompressResult.getErrorMessage();
        return false;
    }

    // 1. Read and parse session_manifest.json
    auto manifestFile = workingTempDir.getChildFile("session_manifest.json");
    if (!manifestFile.existsAsFile())
    {
        outErrorMessage = "Package is missing session_manifest.json.";
        return false;
    }

    try
    {
        auto jsonStr = manifestFile.loadFileAsString().toStdString();
        auto j = nlohmann::json::parse(jsonStr);
        if (!deserializeManifestFromJson(j, outManifest))
        {
            outErrorMessage = "Invalid session manifest structure.";
            return false;
        }
    }
    catch (const std::exception& e)
    {
        outErrorMessage = "JSON parsing error in session manifest: " + juce::String(e.what());
        return false;
    }

    // 2. Read binary measured points if present
    outPoints.clear();
    auto pointsFile = workingTempDir.getChildFile("lut_matrices").getChildFile("measured_points.bin");
    if (pointsFile.existsAsFile())
    {
        juce::MemoryBlock mb;
        pointsFile.loadFileAsData(mb);
        juce::MemoryInputStream mis(mb, false);
        int numPoints = mis.readInt();
        for (int i = 0; i < numPoints && !mis.isExhausted(); ++i)
        {
            exporting::MeasuredPoint pt;
            pt.testId = mis.readString().toStdString();
            pt.param1Normalized = mis.readFloat();
            pt.param2Normalized = mis.readFloat();
            pt.muSigmaValue.mean = mis.readFloat();
            pt.muSigmaValue.stdDev = mis.readFloat();
            pt.secondaryValue.mean = mis.readFloat();
            pt.secondaryValue.stdDev = mis.readFloat();
            pt.thdValue.mean = mis.readFloat();
            pt.thdValue.stdDev = mis.readFloat();
            outPoints.push_back(pt);
        }
    }

    // 3. Validate integrity: check that referenced audio files exist
    for (const auto& audioRelPath : outManifest.recordedAudioFiles)
    {
        auto audioFile = workingTempDir.getChildFile(audioRelPath);
        if (!audioFile.existsAsFile())
        {
            juce::Logger::writeToLog("[SessionSerializer WARNING] Missing audio entry: " + juce::String(audioRelPath));
        }
    }

    activeSessionFile = sourcePackageFile;
    return true;
}

bool SessionSerializer::triggerIncrementalAutoSave(const SessionManifest& manifest,
                                                  const std::vector<exporting::MeasuredPoint>& points)
{
    return saveSessionToPackage(autoSaveFile, manifest, points);
}

} // namespace abdaudiolab::core
