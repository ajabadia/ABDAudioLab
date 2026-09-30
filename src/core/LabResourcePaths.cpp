/**
 * @file LabResourcePaths.cpp
 * @brief Implementación del resolvedor único de rutas del repositorio.
 * @author ABDSynths
 * @date 2026
 */

#include "core/LabResourcePaths.h"

#include <sstream>
#include <stdexcept>

namespace abdaudiolab::core
{
namespace
{

/** Marcador de raíz del workspace. Versionado en git, por lo que existe en cualquier clon limpio. */
constexpr const char* kWorkspaceMarker = "ABDAudioLab.workspace";

/** Subdirectorios que un candidato debe exponer para considerarse raíz válida del repositorio. */
juce::File markerFromCandidate(const juce::File& candidate)
{
    if (!candidate.isDirectory())
        return {};

    return candidate.getChildFile(kWorkspaceMarker);
}

/** true si el directorio es la raíz del repositorio ABDAudioLab. */
bool isRepoRoot(const juce::File& candidate)
{
    return markerFromCandidate(candidate).existsAsFile();
}

/**
 * @brief Asciende desde un punto hasta la raíz del sistema de archivos
 *        parando en el primer directorio que contenga el marcador.
 */
juce::File ascendToRepoRoot(juce::File start, std::vector<std::string>& probed)
{
    if (start == juce::File())
        return {};

    if (start.existsAsFile())
        start = start.getParentDirectory();

    while (start.isDirectory())
    {
        probed.push_back(start.getFullPathName().toStdString());

        if (isRepoRoot(start))
            return start;

        const juce::File parent = start.getParentDirectory();

        if (parent == start || !parent.exists())
            break;

        start = parent;
    }

    return {};
}

} // namespace

RepoRootResolution resolveRepoRoot()
{
    RepoRootResolution out;

    // 1. Override explícito por variable de entorno.
    const auto envRoot =
        juce::SystemStats::getEnvironmentVariable("ABDAUDIOLAB_REPO_ROOT", juce::String());

    if (envRoot.isNotEmpty())
    {
        const juce::File candidate(envRoot);
        out.probed.push_back("ABDAUDIOLAB_REPO_ROOT=" + envRoot.toStdString());

        if (isRepoRoot(candidate))
        {
            out.root = candidate;
            out.isResolved = true;
            out.reason = "ABDAUDIOLAB_REPO_ROOT";
            return out;
        }

        // Definida pero inválida: fallar de inmediato es más seguro que caer al CWD
        // y resolver silenciosamente contra otra copia del repositorio.
        std::ostringstream oss;
        oss << "ABDAUDIOLAB_REPO_ROOT esta definida pero no apunta a la raiz de ABDAudioLab.\n"
            << "  Ruta: " << envRoot.toStdString() << "\n"
            << "  Falta el marcador: " << kWorkspaceMarker << "\n";
        out.reason = oss.str();
        return out;
    }

    // 2. Ascenso desde el directorio del ejecutable.
    const juce::File exeFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);

    if (exeFile != juce::File())
    {
        if (out.root = ascendToRepoRoot(exeFile, out.probed); out.root != juce::File())
        {
            out.isResolved = true;
            out.reason = "Ascendido desde el ejecutable: " + out.root.getFullPathName().toStdString();
            return out;
        }
    }

    // 3. Ascenso desde el directorio de trabajo actual.
    if (out.root = ascendToRepoRoot(juce::File::getCurrentWorkingDirectory(), out.probed);
        out.root != juce::File())
    {
        out.isResolved = true;
        out.reason = "Ascendido desde el directorio de trabajo: "
                      + out.root.getFullPathName().toStdString();
        return out;
    }

    // 4. Fallo determinista con la traza completa.
    std::ostringstream oss;
    oss << "No se encontro la raiz de ABDAudioLab (marcador '" << kWorkspaceMarker << "').\n"
        << "Se probaron los siguientes candidatos:\n";

    for (const auto& candidate : out.probed)
        oss << " - " << candidate << "\n";

    out.reason = oss.str();
    return out;
}

juce::File requireRepoRoot()
{
    const auto resolution = resolveRepoRoot();

    if (!resolution.isResolved)
        throw std::runtime_error(resolution.reason);

    return resolution.root;
}

bool isSafeRepoRelativePath(const juce::String& relativePath)
{
    const auto trimmed = relativePath.trim();

    if (trimmed.isEmpty())
        return false;

    // Rechaza rutas absolutas de Windows, UNC y POSIX.
    if (trimmed.containsChar(':') || trimmed.startsWithChar('/') || trimmed.startsWithChar('\\'))
        return false;

    juce::StringArray tokens;
    tokens.addTokens(trimmed, "/\\", "\"");

    for (const auto& token : tokens)
    {
        if (token == "..")
            return false;
    }

    return true;
}

juce::File repoResource(const juce::String& relativePath)
{
    if (!isSafeRepoRelativePath(relativePath))
        throw std::runtime_error("repoResource(): ruta relativa insegura: "
                                 + relativePath.toStdString());

    return requireRepoRoot().getChildFile(relativePath);
}

juce::File optionalRepoResource(const juce::String& relativePath)
{
    if (!isSafeRepoRelativePath(relativePath))
        return {};

    const auto root = resolveRepoRoot();
    return root.isResolved ? root.root.getChildFile(relativePath) : juce::File();
}

juce::File contractsHardwareDir()      { return repoResource("contracts/hardware"); }
juce::File canonicalTargetsDir()       { return repoResource("profiles/targets"); }
juce::File profilingPresetsDir()       { return repoResource("presets/profiling"); }
juce::File profilesDir()               { return repoResource("profiles"); }
juce::File docsQaDir()                 { return repoResource("docs/qa"); }
juce::File docsQaRunsDir()             { return repoResource("docs/qa/runs"); }
juce::File fixturesDir()               { return repoResource("fixtures"); }
juce::File fixturesEvaluationsDir()    { return repoResource("fixtures/evaluations"); }
juce::File assetsDir()                 { return repoResource("assets"); }
juce::File exportedLutsDir()           { return repoResource("exported_luts"); }

juce::File sharedAssetsDir()
{
    // 1. Hermano de la raiz del repositorio: <repo>/../ABDSharedAssets.
    if (const auto root = resolveRepoRoot(); root.isResolved)
    {
        const auto sibling = root.root.getParentDirectory().getChildFile("ABDSharedAssets");

        if (sibling.isDirectory())
            return sibling;
    }

    // 2. Subiendo desde el ejecutable, que es donde vive el arbol de build.
    auto current = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                       .getParentDirectory();

    for (int level = 0; level < 7 && current.isDirectory(); ++level)
    {
        const auto candidate = current.getChildFile("ABDSharedAssets");

        if (candidate.isDirectory())
            return candidate;

        const auto parent = current.getParentDirectory();

        if (parent == current)
            break;

        current = parent;
    }

    // No lanza a proposito: la ausencia de assets compartidos no es un error fatal.
    return {};
}

} // namespace abdaudiolab::core
