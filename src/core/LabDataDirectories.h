/**
 * @file LabDataDirectories.h
 * @brief Single deterministic service for resolving ABDAudioLab data directories (experiments/ and exports/).
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_core/juce_core.h>
#include <string>
#include <optional>

namespace abdaudiolab::core
{

/**
 * @brief Procedencia de la resolución del DataRoot.
 */
enum class DataRootOrigin
{
    ExplicitOverride,       /**< Configurado programáticamente o por ajustes del usuario. */
    EnvironmentVariable,    /**< Definido mediante la variable ABDAUDIOLAB_DATA_ROOT. */
    WorkspaceMarker,        /**< Detectado mediante el marcador único ABDAudioLab.workspace. */
    UserDocumentsFallback   /**< Fallback estándar multiplataforma en Documents/ABDAudioLab. */
};

[[nodiscard]] inline std::string dataRootOriginToString(DataRootOrigin origin)
{
    switch (origin)
    {
        case DataRootOrigin::ExplicitOverride:     return "ExplicitOverride";
        case DataRootOrigin::EnvironmentVariable:  return "EnvironmentVariable";
        case DataRootOrigin::WorkspaceMarker:      return "WorkspaceMarker";
        case DataRootOrigin::UserDocumentsFallback: return "UserDocumentsFallback";
        default:                                   return "Unknown";
    }
}

/**
 * @brief Conjunto unificado e inmutable de rutas de datos del laboratorio.
 */
struct LabDataDirectories
{
    juce::File dataRoot;
    juce::File experiments;
    juce::File exports;
    DataRootOrigin origin { DataRootOrigin::UserDocumentsFallback };
    std::string resolutionReason;
    bool isWritable { false };

    [[nodiscard]] bool isValid() const noexcept
    {
        return dataRoot.isDirectory() && experiments.isDirectory() && exports.isDirectory() && isWritable;
    }
};

/**
 * @brief Resuelve deterministamente las carpetas de datos del laboratorio según la prioridad formal:
 * 1. Override explícito en memoria (setExplicitDataRootOverride).
 * 2. Variable de entorno ABDAUDIOLAB_DATA_ROOT.
 * 3. Marcador único ABDAudioLab.workspace.
 * 4. Fallback a User Documents (%USERPROFILE%/Documents/ABDAudioLab o ~/Documents/ABDAudioLab).
 *
 * @param outDiagnostic Cadena opcional donde se registra la traza de diagnóstico.
 * @return LabDataDirectories válidas y verificadas para escritura.
 */
[[nodiscard]] LabDataDirectories resolveLabDataDirectories(juce::String* outDiagnostic = nullptr);

/**
 * @brief Configura un override explícito en memoria para la raíz de datos (por ejemplo desde UI o tests).
 * Valida normalización, rechaza archivos y comprueba permisos de escritura.
 * @return true si la ruta es válida y escribible, false si falló la validación.
 */
bool setExplicitDataRootOverride(const juce::File& customRoot, juce::String& outError);

/**
 * @brief Limpia cualquier override explícito configurado en memoria.
 */
void clearExplicitDataRootOverride();

/**
 * @brief Busca el marcador único ABDAudioLab.workspace ascendiendo por el árbol de directorios.
 * @param startingPoint Carpeta de inicio (CWD, directorio del ejecutable, etc.).
 * @param maxLevels Niveles máximos a subir en la jerarquía (por defecto 6).
 * @return Archivo marcador si fue encontrado, o juce::File() inválido si no.
 */
[[nodiscard]] juce::File findWorkspaceMarker(const juce::File& startingPoint, int maxLevels = 6);

/**
 * @brief Comprueba si un directorio tiene permisos de escritura efectivos mediante una sonda temporal.
 */
[[nodiscard]] bool testDirectoryWritable(const juce::File& dir, juce::String* outError = nullptr);

} // namespace abdaudiolab::core
