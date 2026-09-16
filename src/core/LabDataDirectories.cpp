/**
 * @file LabDataDirectories.cpp
 * @brief Implementation of deterministic LabDataDirectories resolution.
 * @author ABDSynths
 * @date 2026
 */

#include "LabDataDirectories.h"
#include <mutex>
#include <iostream>

namespace abdaudiolab::core
{

namespace
{

std::mutex g_overrideMutex;
std::optional<juce::File> g_explicitOverride;

constexpr const char* kMarkerFileName = "ABDAudioLab.workspace";
constexpr const char* kEnvVarName = "ABDAUDIOLAB_DATA_ROOT";

bool ensureSubdirectoriesAndWritable(LabDataDirectories& dirs, juce::String* outDiagnostic)
{
    if (dirs.dataRoot.existsAsFile())
    {
        if (outDiagnostic != nullptr)
            *outDiagnostic += "DataRoot path is an existing file, not a directory: " + dirs.dataRoot.getFullPathName() + "\n";
        return false;
    }

    if (!dirs.dataRoot.isDirectory())
    {
        auto res = dirs.dataRoot.createDirectory();
        if (res.failed())
        {
            if (outDiagnostic != nullptr)
                *outDiagnostic += "Failed to create DataRoot directory: " + res.getErrorMessage() + "\n";
            return false;
        }
    }

    dirs.experiments = dirs.dataRoot.getChildFile("experiments");
    if (!dirs.experiments.isDirectory())
    {
        auto res = dirs.experiments.createDirectory();
        if (res.failed())
        {
            if (outDiagnostic != nullptr)
                *outDiagnostic += "Failed to create experiments directory: " + res.getErrorMessage() + "\n";
            return false;
        }
    }

    dirs.exports = dirs.dataRoot.getChildFile("exports");
    if (!dirs.exports.isDirectory())
    {
        auto res = dirs.exports.createDirectory();
        if (res.failed())
        {
            if (outDiagnostic != nullptr)
                *outDiagnostic += "Failed to create exports directory: " + res.getErrorMessage() + "\n";
            return false;
        }
    }

    juce::String writeErr;
    if (!testDirectoryWritable(dirs.dataRoot, &writeErr) ||
        !testDirectoryWritable(dirs.experiments, &writeErr) ||
        !testDirectoryWritable(dirs.exports, &writeErr))
    {
        if (outDiagnostic != nullptr)
            *outDiagnostic += "Write permission probe failed: " + writeErr + "\n";
        dirs.isWritable = false;
        return false;
    }

    dirs.isWritable = true;
    return true;
}

} // namespace

bool testDirectoryWritable(const juce::File& dir, juce::String* outError)
{
    if (!dir.isDirectory())
    {
        if (outError != nullptr)
            *outError = "Target is not a directory: " + dir.getFullPathName();
        return false;
    }

    juce::String randSuffix = juce::String::toHexString(juce::Random::getSystemRandom().nextInt());
    juce::File probe = dir.getChildFile(".probe_write_" + randSuffix + ".tmp");

    if (!probe.replaceWithText("probe"))
    {
        if (outError != nullptr)
            *outError = "Could not write probe file in: " + dir.getFullPathName();
        return false;
    }

    if (!probe.deleteFile())
    {
        if (outError != nullptr)
            *outError = "Could not delete probe file in: " + dir.getFullPathName();
        return false;
    }

    return true;
}

juce::File findWorkspaceMarker(const juce::File& startingPoint, int maxLevels)
{
    juce::File current = startingPoint;
    if (current.existsAsFile())
        current = current.getParentDirectory();

    for (int level = 0; level <= maxLevels && current.isDirectory(); ++level)
    {
        juce::File marker = current.getChildFile(kMarkerFileName);
        if (marker.existsAsFile())
            return marker;

        juce::File parent = current.getParentDirectory();
        if (parent == current)
            break; // Raíz del sistema de archivos alcanzada
        current = parent;
    }

    return {};
}

bool setExplicitDataRootOverride(const juce::File& customRoot, juce::String& outError)
{
    std::lock_guard<std::mutex> lock(g_overrideMutex);

    if (customRoot.existsAsFile())
    {
        outError = "Explicit DataRoot cannot be an existing file: " + customRoot.getFullPathName();
        return false;
    }

    juce::File candidate = customRoot;
    if (!candidate.isDirectory())
    {
        auto res = candidate.createDirectory();
        if (res.failed())
        {
            outError = "Failed to create explicit DataRoot: " + res.getErrorMessage();
            return false;
        }
    }

    LabDataDirectories temp;
    temp.dataRoot = candidate;
    if (!ensureSubdirectoriesAndWritable(temp, &outError))
    {
        return false;
    }

    g_explicitOverride = candidate;
    return true;
}

void clearExplicitDataRootOverride()
{
    std::lock_guard<std::mutex> lock(g_overrideMutex);
    g_explicitOverride.reset();
}

LabDataDirectories resolveLabDataDirectories(juce::String* outDiagnostic)
{
    LabDataDirectories dirs;

    // 1. Prioridad 1: Override explícito en memoria
    {
        std::lock_guard<std::mutex> lock(g_overrideMutex);
        if (g_explicitOverride.has_value() && g_explicitOverride->isDirectory())
        {
            dirs.dataRoot = *g_explicitOverride;
            dirs.origin = DataRootOrigin::ExplicitOverride;
            dirs.resolutionReason = "Configurado explícitamente mediante setExplicitDataRootOverride";

            if (ensureSubdirectoriesAndWritable(dirs, outDiagnostic))
            {
                if (outDiagnostic != nullptr)
                    *outDiagnostic += "Resolved via ExplicitOverride: " + dirs.dataRoot.getFullPathName() + "\n";
                return dirs;
            }
        }
    }

    // 2. Prioridad 2: Variable de entorno ABDAUDIOLAB_DATA_ROOT
    juce::String envVal = juce::SystemStats::getEnvironmentVariable(kEnvVarName, {});
    if (envVal.isNotEmpty())
    {
        juce::File envDir(envVal.trim());
        if (envDir.existsAsFile())
        {
            if (outDiagnostic != nullptr)
                *outDiagnostic += "ABDAUDIOLAB_DATA_ROOT points to a file, rejected: " + envDir.getFullPathName() + "\n";
        }
        else
        {
            dirs.dataRoot = envDir;
            dirs.origin = DataRootOrigin::EnvironmentVariable;
            dirs.resolutionReason = "Definido por la variable de entorno " + std::string(kEnvVarName);

            if (ensureSubdirectoriesAndWritable(dirs, outDiagnostic))
            {
                if (outDiagnostic != nullptr)
                    *outDiagnostic += "Resolved via EnvironmentVariable: " + dirs.dataRoot.getFullPathName() + "\n";
                return dirs;
            }
        }
    }

    // 3. Prioridad 3: Marcador único ABDAudioLab.workspace
    // Probar primero desde CWD
    juce::File marker = findWorkspaceMarker(juce::File::getCurrentWorkingDirectory(), 6);
    if (!marker.existsAsFile())
    {
        // Probar desde la ruta del binario ejecutable
        auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
        marker = findWorkspaceMarker(exeDir, 6);
    }

    if (marker.existsAsFile())
    {
        dirs.dataRoot = marker.getParentDirectory();
        dirs.origin = DataRootOrigin::WorkspaceMarker;
        dirs.resolutionReason = "Marcador " + std::string(kMarkerFileName) + " detectado en " + marker.getFullPathName().toStdString();

        if (ensureSubdirectoriesAndWritable(dirs, outDiagnostic))
        {
            if (outDiagnostic != nullptr)
                *outDiagnostic += "Resolved via WorkspaceMarker: " + dirs.dataRoot.getFullPathName() + "\n";
            return dirs;
        }
    }

    // 4. Prioridad 4: Fallback a Documents de usuario
    auto docsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                       .getChildFile("ABDAudioLab");
    dirs.dataRoot = docsDir;
    dirs.origin = DataRootOrigin::UserDocumentsFallback;
    dirs.resolutionReason = "Fallback estándar multiplataforma en Documentos del usuario";

    if (ensureSubdirectoriesAndWritable(dirs, outDiagnostic))
    {
        if (outDiagnostic != nullptr)
            *outDiagnostic += "Resolved via UserDocumentsFallback: " + dirs.dataRoot.getFullPathName() + "\n";
        return dirs;
    }

    // Último recurso de emergencia en caso extremo de fallo en Documents
    dirs.isWritable = false;
    return dirs;
}

} // namespace abdaudiolab::core
