#include "CalibrationProfileStore.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <cctype>

namespace abdaudiolab::calibration
{

CalibrationProfileStore::CalibrationProfileStore(juce::File storageDirectory)
    : storageDir((storageDirectory != juce::File() && storageDirectory.getFullPathName().isNotEmpty())
                     ? storageDirectory
                     : getDefaultStorageDirectory())
{
}

juce::File CalibrationProfileStore::getDefaultStorageDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("ABDAudioLab")
        .getChildFile("calibration_profiles");
}

bool CalibrationProfileStore::isValidProfileId(const std::string& profileId)
{
    if (profileId.empty() || profileId.length() > 128)
        return false;

    return std::all_of(profileId.begin(), profileId.end(), [](char c) {
        return (c >= 'a' && c <= 'z') ||
               (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') ||
               c == '_' || c == '-';
    });
}

bool CalibrationProfileStore::isRecordValidForSaving(const CalibrationRecord& record, std::string& outError)
{
    if (!isValidProfileId(record.profileId))
    {
        outError = "Identificador de perfil no valido (solo se permiten caracteres alfanumericos, guiones y guiones bajos, max 128 caracteres)";
        return false;
    }

    if (record.schemaVersion != 1)
    {
        outError = "Version de esquema no soportada (se requiere schemaVersion == 1)";
        return false;
    }

    if (!record.calibrationResult.isCalibrated)
    {
        outError = "La medicion no esta marcada como calibrada valida (isCalibrated == false)";
        return false;
    }

    if (record.calibrationResult.clippingDetected)
    {
        outError = "Se detecto saturacion (clipping) en la medicion; no se permite guardar una calibracion comprometida";
        return false;
    }

    return true;
}

std::string CalibrationProfileStore::getCurrentUtcIsoTimestamp()
{
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm gmt {};
#if defined(_WIN32)
    gmtime_s(&gmt, &tt);
#else
    gmtime_r(&tt, &gmt);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &gmt);
    return std::string(buf);
}

std::string CalibrationProfileStore::generateDefaultProfileId(const std::string& deviceName, const std::string& isoTimestamp)
{
    std::string cleanDevice;
    for (char c : deviceName)
    {
        if (std::isalnum(static_cast<unsigned char>(c)))
            cleanDevice += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        else if (c == ' ' || c == '_' || c == '-')
            cleanDevice += '-';
    }

    // Colapsar guiones repetidos
    std::string compactDevice;
    bool lastWasDash = false;
    for (char c : cleanDevice)
    {
        if (c == '-')
        {
            if (!lastWasDash && !compactDevice.empty())
                compactDevice += '-';
            lastWasDash = true;
        }
        else
        {
            compactDevice += c;
            lastWasDash = false;
        }
    }
    if (!compactDevice.empty() && compactDevice.back() == '-')
        compactDevice.pop_back();

    std::string cleanTime;
    for (char c : isoTimestamp)
    {
        if (std::isalnum(static_cast<unsigned char>(c)))
            cleanTime += c;
        else if (c == '-')
            cleanTime += "";
        else if (c == ':')
            cleanTime += "";
    }

    if (cleanTime.empty())
        cleanTime = "unknown-time";

    std::string id = "calibration-";
    if (!compactDevice.empty())
        id += compactDevice + "-";
    id += cleanTime;

    if (id.length() > 120)
        id = id.substr(0, 120);

    return id;
}

CalibrationProfileStore::SaveResult CalibrationProfileStore::save(const CalibrationRecord& record, bool overwrite)
{
    std::string error;
    if (!isRecordValidForSaving(record, error))
    {
        return { false, record.profileId, error };
    }

    if (!storageDir.exists())
    {
        auto res = storageDir.createDirectory();
        if (res.failed())
        {
            return { false, record.profileId, "No se pudo crear el directorio de perfiles: " + res.getErrorMessage().toStdString() };
        }
    }

    auto targetFile = storageDir.getChildFile(juce::String(record.profileId) + ".json");
    auto tempFile   = storageDir.getChildFile(juce::String(record.profileId) + ".json.tmp");

    if (targetFile.existsAsFile() && !overwrite)
    {
        return { false, record.profileId, "El perfil ya existe y no se solicito sobrescritura" };
    }

    try
    {
        nlohmann::json j;
        j["schemaVersion"] = record.schemaVersion;
        j["profileId"] = record.profileId;
        j["createdAt"] = record.createdAt.empty() ? getCurrentUtcIsoTimestamp() : record.createdAt;

        j["deviceSnapshot"] = {
            { "deviceName", record.deviceSnapshot.deviceName },
            { "driverType", record.deviceSnapshot.driverType },
            { "sampleRate", record.deviceSnapshot.sampleRate },
            { "bufferSizeSamples", record.deviceSnapshot.bufferSizeSamples }
        };

        j["routingSnapshot"] = {
            { "inputChannelIndex", record.routingSnapshot.inputChannelIndex },
            { "inputChannelLabel", record.routingSnapshot.inputChannelLabel },
            { "outputChannelIndex", record.routingSnapshot.outputChannelIndex },
            { "outputChannelLabel", record.routingSnapshot.outputChannelLabel }
        };

        float trimDb = (record.calibrationResult.recommendedTrimGain > 1e-6f)
            ? 20.0f * std::log10(record.calibrationResult.recommendedTrimGain)
            : -96.0f;

        j["calibrationResult"] = {
            { "isCalibrated", record.calibrationResult.isCalibrated },
            { "roundTripLatencySamples", record.calibrationResult.latencySamples },
            { "roundTripLatencyMs", record.calibrationResult.roundTripLatencyMs },
            { "recommendedTrimGain", record.calibrationResult.recommendedTrimGain },
            { "recommendedTrimGainDb", trimDb },
            { "peakInDbfs", record.calibrationResult.peakInDbfs },
            { "snrDb", record.calibrationResult.snrDb },
            { "frequencyFlatnessDb", record.calibrationResult.frequencyFlatnessDb },
            { "phaseInversionDetected", record.calibrationResult.phaseInversionDetected },
            { "phaseInversionCorrelation", record.calibrationResult.phaseInversionCorrelation },
            { "clippingDetected", record.calibrationResult.clippingDetected },
            { "clippedSamplesCount", record.calibrationResult.clippedSamplesCount },
            { "dcOffsetVolts", record.calibrationResult.dcOffsetVolts },
            { "frequenciesHz", record.calibrationResult.freqsHz },
            { "magnitudeDb", record.calibrationResult.magnitudeDb },
            { "inverseCorrectionDb", record.calibrationResult.inverseCorrectionDb }
        };

        j["provenance"] = {
            { "applicationVersion", record.provenance.applicationVersion },
            { "calibrationAlgorithmVersion", record.provenance.calibrationAlgorithmVersion }
        };

        std::string jsonStr = j.dump(2);

        // Fase 1: Escribir en archivo temporal {profileId}.json.tmp
        {
            std::ofstream out(tempFile.getFullPathName().toStdString(), std::ios::out | std::ios::trunc);
            if (!out.is_open())
            {
                return { false, record.profileId, "Error al abrir archivo temporal para escritura" };
            }
            out << jsonStr;
            out.flush();
            if (out.fail())
            {
                out.close();
                tempFile.deleteFile();
                return { false, record.profileId, "Error durante la escritura del archivo temporal" };
            }
        }

        if (!tempFile.existsAsFile() || tempFile.getSize() == 0)
        {
            tempFile.deleteFile();
            return { false, record.profileId, "El archivo temporal generado esta vacio o no existe" };
        }

        // Fase 2: Renombrado atomico a destino final
        if (targetFile.existsAsFile())
        {
            if (!targetFile.deleteFile())
            {
                tempFile.deleteFile();
                return { false, record.profileId, "No se pudo eliminar el archivo anterior para sobrescribir" };
            }
        }

        if (!tempFile.moveFileTo(targetFile))
        {
            tempFile.deleteFile();
            return { false, record.profileId, "Fallo el renombramiento atomico del archivo temporal" };
        }

        return { true, record.profileId, "" };
    }
    catch (const std::exception& e)
    {
        if (tempFile.existsAsFile())
            tempFile.deleteFile();
        return { false, record.profileId, std::string("Excepcion al guardar perfil: ") + e.what() };
    }
    catch (...)
    {
        if (tempFile.existsAsFile())
            tempFile.deleteFile();
        return { false, record.profileId, "Excepcion desconocida al guardar perfil" };
    }
}

std::optional<CalibrationRecord> CalibrationProfileStore::load(const std::string& profileId) const
{
    if (!isValidProfileId(profileId))
        return std::nullopt;

    auto targetFile = storageDir.getChildFile(juce::String(profileId) + ".json");
    if (!targetFile.existsAsFile())
        return std::nullopt;

    try
    {
        std::ifstream in(targetFile.getFullPathName().toStdString());
        if (!in.is_open())
            return std::nullopt;

        nlohmann::json j;
        in >> j;

        if (!j.is_object())
            return std::nullopt;

        int schema = j.value("schemaVersion", 0);
        if (schema != 1)
            return std::nullopt;

        // Si este archivo es un CalibrationSnapshot nuevo, adaptarlo a CalibrationRecord
        if (j.contains("compatibility") && j.contains("result"))
        {
            auto snapOpt = CalibrationSnapshot::fromJsonSafe(j);
            if (snapOpt.has_value() && snapOpt->result.isValid())
            {
                CalibrationRecord rec;
            rec.schemaVersion = 1;
            rec.profileId = snapOpt->profileId;
            rec.createdAt = snapOpt->createdAt;
            rec.deviceSnapshot.deviceName = snapOpt->compatibility.deviceStableId;
            rec.deviceSnapshot.driverType = snapOpt->compatibility.driverType;
            rec.deviceSnapshot.sampleRate = snapOpt->compatibility.sampleRateHz;
            rec.deviceSnapshot.bufferSizeSamples = snapOpt->compatibility.bufferSamples;
            rec.routingSnapshot.inputChannelIndex = snapOpt->compatibility.inputChannelIndex;
            rec.routingSnapshot.outputChannelIndex = snapOpt->compatibility.outputChannelIndex;
            rec.routingSnapshot.outputChannelLabel = "Output " + std::to_string(snapOpt->compatibility.outputChannelIndex + 1);
            rec.routingSnapshot.inputChannelLabel = "Input " + std::to_string(snapOpt->compatibility.inputChannelIndex + 1);
            rec.calibrationResult.isCalibrated = snapOpt->result.isValid();
            rec.calibrationResult.sampleRate = snapOpt->compatibility.sampleRateHz;
            rec.calibrationResult.recommendedTrimGain = std::pow(10.0f, snapOpt->result.interfaceTrimDb / 20.0f);
            rec.calibrationResult.targetHeadroomDbfs = -3.0f;
            rec.calibrationResult.roundTripLatencyMs = static_cast<float>(snapOpt->result.rtlMs);
            rec.calibrationResult.latencySamples = snapOpt->result.rtlSamples;
            rec.calibrationResult.peakInDbfs = snapOpt->result.peakDbfs;
            rec.calibrationResult.snrDb = snapOpt->result.snrDb;
            rec.calibrationResult.frequencyFlatnessDb = snapOpt->result.flatnessDeltaDb;
            rec.calibrationResult.phaseInversionDetected = (snapOpt->result.polarity == "Inverted");
            rec.calibrationResult.phaseInversionCorrelation = (snapOpt->result.polarity == "Inverted") ? -1.0f : 1.0f;
            rec.calibrationResult.clippingDetected = (snapOpt->result.clippingSamples > 0);
            rec.calibrationResult.clippedSamplesCount = snapOpt->result.clippingSamples;
            rec.calibrationResult.deviceName = juce::String(snapOpt->compatibility.deviceStableId);
            rec.calibrationResult.timestamp = juce::String(snapOpt->createdAt);

            rec.provenance.applicationVersion = "2.1.0";
            rec.provenance.calibrationAlgorithmVersion = 1;
            return rec;
        }
    }

        CalibrationRecord rec;
        rec.schemaVersion = schema;
        rec.profileId = j.value("profileId", profileId);
        rec.createdAt = j.value("createdAt", "");

        if (j.contains("deviceSnapshot") && j["deviceSnapshot"].is_object())
        {
            const auto& dev = j["deviceSnapshot"];
            rec.deviceSnapshot.deviceName = dev.value("deviceName", "");
            rec.deviceSnapshot.driverType = dev.value("driverType", "");
            rec.deviceSnapshot.sampleRate = dev.value("sampleRate", 48000.0);
            rec.deviceSnapshot.bufferSizeSamples = dev.value("bufferSizeSamples", 256);
        }

        if (j.contains("routingSnapshot") && j["routingSnapshot"].is_object())
        {
            const auto& rout = j["routingSnapshot"];
            rec.routingSnapshot.inputChannelIndex = rout.value("inputChannelIndex", 0);
            rec.routingSnapshot.inputChannelLabel = rout.value("inputChannelLabel", "Input 1");
            rec.routingSnapshot.outputChannelIndex = rout.value("outputChannelIndex", 0);
            rec.routingSnapshot.outputChannelLabel = rout.value("outputChannelLabel", "Output 1");
        }

        if (j.contains("calibrationResult") && j["calibrationResult"].is_object())
        {
            const auto& cal = j["calibrationResult"];
            rec.calibrationResult.isCalibrated = cal.value("isCalibrated", false);
            rec.calibrationResult.latencySamples = cal.value("roundTripLatencySamples", 0);
            rec.calibrationResult.roundTripLatencyMs = cal.value("roundTripLatencyMs", 0.0f);
            rec.calibrationResult.recommendedTrimGain = cal.value("recommendedTrimGain", 1.0f);
            rec.calibrationResult.peakInDbfs = cal.value("peakInDbfs", -100.0f);
            rec.calibrationResult.snrDb = cal.value("snrDb", 0.0f);
            rec.calibrationResult.frequencyFlatnessDb = cal.value("frequencyFlatnessDb", 0.0f);
            rec.calibrationResult.phaseInversionDetected = cal.value("phaseInversionDetected", false);
            rec.calibrationResult.phaseInversionCorrelation = cal.value("phaseInversionCorrelation", 1.0f);
            rec.calibrationResult.clippingDetected = cal.value("clippingDetected", false);
            rec.calibrationResult.clippedSamplesCount = cal.value("clippedSamplesCount", 0);
            rec.calibrationResult.dcOffsetVolts = cal.value("dcOffsetVolts", 0.0f);
            rec.calibrationResult.sampleRate = rec.deviceSnapshot.sampleRate;
            rec.calibrationResult.deviceName = juce::String(rec.deviceSnapshot.deviceName);
            rec.calibrationResult.timestamp = juce::String(rec.createdAt);

            if (cal.contains("frequenciesHz") && cal["frequenciesHz"].is_array())
                rec.calibrationResult.freqsHz = cal["frequenciesHz"].get<std::vector<float>>();
            if (cal.contains("magnitudeDb") && cal["magnitudeDb"].is_array())
                rec.calibrationResult.magnitudeDb = cal["magnitudeDb"].get<std::vector<float>>();
            if (cal.contains("inverseCorrectionDb") && cal["inverseCorrectionDb"].is_array())
                rec.calibrationResult.inverseCorrectionDb = cal["inverseCorrectionDb"].get<std::vector<float>>();
        }

        if (j.contains("provenance") && j["provenance"].is_object())
        {
            const auto& prov = j["provenance"];
            rec.provenance.applicationVersion = prov.value("applicationVersion", "");
            rec.provenance.calibrationAlgorithmVersion = prov.value("calibrationAlgorithmVersion", 1);
        }

        return rec;
    }
    catch (...)
    {
        return std::nullopt;
    }
}

std::vector<CalibrationRecord> CalibrationProfileStore::list() const
{
    std::vector<CalibrationRecord> records;
    if (!storageDir.isDirectory())
        return records;

    juce::Array<juce::File> files = storageDir.findChildFiles(juce::File::findFiles, false, "*.json");

    for (const auto& file : files)
    {
        // Ignorar estrictamente archivos temporales si tuvieran extension doble .json.tmp
        if (file.getFileName().endsWithIgnoreCase(".tmp"))
            continue;

        std::string id = file.getFileNameWithoutExtension().toStdString();
        if (!isValidProfileId(id))
            continue;

        auto rec = load(id);
        if (rec.has_value())
        {
            records.push_back(std::move(*rec));
        }
    }

    // Ordenar de forma estable: createdAt descendente (mas reciente primero); desempate por profileId
    std::sort(records.begin(), records.end(), [](const CalibrationRecord& a, const CalibrationRecord& b) {
        if (a.createdAt != b.createdAt)
            return a.createdAt > b.createdAt;
        return a.profileId < b.profileId;
    });

    return records;
}

bool CalibrationProfileStore::remove(const std::string& profileId)
{
    if (!isValidProfileId(profileId))
        return false;

    auto targetFile = storageDir.getChildFile(juce::String(profileId) + ".json");
    if (!targetFile.existsAsFile())
        return false;

    return targetFile.deleteFile();
}

CalibrationProfileStore::SaveResult CalibrationProfileStore::saveSnapshot(const CalibrationSnapshot& snapshot, bool overwrite)
{
    if (!isValidProfileId(snapshot.profileId))
    {
        return { false, snapshot.profileId, "Identificador de perfil no valido" };
    }
    if (!snapshot.result.isValid())
    {
        return { false, snapshot.profileId, "La medicion no esta marcada como valida o presento saturacion" };
    }

    if (!storageDir.exists())
    {
        auto res = storageDir.createDirectory();
        if (res.failed())
        {
            return { false, snapshot.profileId, "No se pudo crear el directorio de perfiles: " + res.getErrorMessage().toStdString() };
        }
    }

    auto targetFile = storageDir.getChildFile(juce::String(snapshot.profileId) + ".json");
    auto tempFile   = storageDir.getChildFile(juce::String(snapshot.profileId) + ".json.tmp");

    if (targetFile.existsAsFile() && !overwrite)
    {
        return { false, snapshot.profileId, "El perfil ya existe y no se solicito sobrescritura" };
    }

    try
    {
        auto j = snapshot.toJson();
        if (!tempFile.replaceWithText(j.dump(2)))
        {
            return { false, snapshot.profileId, "Error al escribir archivo temporal .tmp" };
        }

        if (targetFile.existsAsFile())
        {
            if (!targetFile.deleteFile())
            {
                tempFile.deleteFile();
                return { false, snapshot.profileId, "No se pudo reemplazar el archivo existente de perfil" };
            }
        }

        if (!tempFile.moveFileTo(targetFile))
        {
            return { false, snapshot.profileId, "Error al promover archivo temporal a destino final" };
        }

        return { true, snapshot.profileId, "" };
    }
    catch (const std::exception& e)
    {
        if (tempFile.existsAsFile())
            tempFile.deleteFile();
        return { false, snapshot.profileId, std::string("Fallo al serializar snapshot a JSON: ") + e.what() };
    }
}

std::optional<CalibrationSnapshot> CalibrationProfileStore::loadSnapshot(const std::string& profileId) const
{
    if (!isValidProfileId(profileId))
        return std::nullopt;

    auto file = storageDir.getChildFile(juce::String(profileId) + ".json");
    if (!file.existsAsFile())
        return std::nullopt;

    try
    {
        auto text = file.loadFileAsString();
        auto j = nlohmann::json::parse(text.toStdString());

        // Try direct snapshot parse first
        if (j.contains("compatibility") && j.contains("result"))
        {
            auto snapOpt = CalibrationSnapshot::fromJsonSafe(j);
            if (snapOpt.has_value() && snapOpt->result.isValid())
                return snapOpt;
        }

        // Try legacy CalibrationRecord fallback conversion
        auto recOpt = load(profileId);
        if (recOpt.has_value())
        {
            const auto& rec = *recOpt;
            CalibrationCompatibility compat;
            compat.deviceStableId = rec.deviceSnapshot.deviceName;
            compat.driverType = rec.deviceSnapshot.driverType;
            compat.sampleRateHz = rec.deviceSnapshot.sampleRate;
            compat.bufferSamples = rec.deviceSnapshot.bufferSizeSamples;
            compat.inputChannelIndex = rec.routingSnapshot.inputChannelIndex;
            compat.outputChannelIndex = rec.routingSnapshot.outputChannelIndex;
            compat.routingDescription = rec.routingSnapshot.outputChannelLabel + " -> " + rec.routingSnapshot.inputChannelLabel;

            CalibrationCaptureMetadata cap;
            int rateKhz = static_cast<int>(std::lround(compat.sampleRateHz / 1000.0));
            std::string dispName = compat.deviceStableId + " — " + compat.routingDescription + " — " + std::to_string(rateKhz) + " kHz";

            return CalibrationSnapshot::create(dispName, rec.profileId, compat, cap, rec.calibrationResult, 0);
        }

        return std::nullopt;
    }
    catch (...)
    {
        return std::nullopt;
    }
}

std::vector<CalibrationSnapshot> CalibrationProfileStore::listSnapshots() const
{
    std::vector<CalibrationSnapshot> snapshots;
    if (!storageDir.exists() || !storageDir.isDirectory())
        return snapshots;

    auto files = storageDir.findChildFiles(juce::File::findFiles, false, "*.json");
    for (const auto& file : files)
    {
        if (file.getFileName().endsWithIgnoreCase(".tmp"))
            continue;

        auto snap = loadSnapshot(file.getFileNameWithoutExtension().toStdString());
        if (snap.has_value())
            snapshots.push_back(std::move(*snap));
    }

    std::sort(snapshots.begin(), snapshots.end(), [](const CalibrationSnapshot& a, const CalibrationSnapshot& b) {
        if (a.createdAt != b.createdAt)
            return a.createdAt > b.createdAt;
        return a.profileId < b.profileId;
    });

    return snapshots;
}

} // namespace abdaudiolab::calibration
