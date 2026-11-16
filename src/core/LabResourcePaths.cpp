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
    // Resolvedor de contenido compartido durante la migracion de
    // D:\desarrollos\ABDSynths\ABDSharedAssets y ABDSharedCode a
    // D:\desarrollos\ABDSynths\ABDShared.
    //
    // Precedencia documentada:
    //  1. <repo_root>/../ABDShared/Assets          (nuevo layout consolidado)
    //  2. buscada ascendentemente desde el ejecutable: ...
    //     <build_tree>/../ABDShared/Assets
    //  3. <repo_root>/../ABDSharedAssets           (legado, si existe)
    //  4. buscada ascendentemente desde el ejecutable: ...
    //     <build_tree>/../ABDSharedAssets
    //
    // Se mantiene no fatal y sin heap allocation en la ruta ordinaria:
    // la ausencia de assets compartidos no detiene la aplicacion.

    // 1. Hermano del repositorio: <repo>/../ABDShared/Assets.
    //    El layout consolidado de ABDShared mantiene la carpeta de assets en Assets.
    if (const auto root = resolveRepoRoot(); root.isResolved)
    {
        const auto sibling = root.root.getParentDirectory().getChildFile("ABDShared");

        if (sibling.isDirectory())
        {
            const auto assets = sibling.getChildFile("Assets");

            if (assets.isDirectory())
                return assets;
        }
    }

    // 2. Subiendo desde el ejecutable, que es donde vive el arbol de build.
    auto current = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                       .getParentDirectory();

    for (int level = 0; level < 7 && current.isDirectory(); ++level)
    {
        const auto parent = current.getParentDirectory();
        const auto candidate = current.getChildFile("ABDShared").getChildFile("Assets");

        if (candidate.isDirectory())
            return candidate;

        if (parent == current)
            break;

        current = parent;
    }

    // 3. Por compatibilidad con clones viejos o builds en vivo sin migrar,
    //    acepta tambien la ubicacion antigua de ABDSharedAssets.
    if (const auto root = resolveRepoRoot(); root.isResolved)
    {
        const auto legacy = root.root.getParentDirectory().getChildFile("ABDSharedAssets");

        if (legacy.isDirectory())
            return legacy;
    }

    auto currentLegacy = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                           .getParentDirectory();

    for (int level = 0; level < 7 && currentLegacy.isDirectory(); ++level)
    {
        const auto candidate = currentLegacy.getChildFile("ABDSharedAssets");

        if (candidate.isDirectory())
            return candidate;

        const auto parent = currentLegacy.getParentDirectory();

        if (parent == currentLegacy)
            break;

        currentLegacy = parent;
    }

    // No lanza a proposito: la ausencia de assets compartidos no es un error fatal.
    return {};
}

juce::File sharedContractsDir()
{
    // Resolvedor de contratos compartidos durante la migracion a
    // D:\desarrollos\ABDSynths\ABDShared.
    //
    // Precedencia:
    //  1. <repo_root>/../ABDShared/Contracts
    //  2. buscada ascendentemente desde el ejecutable: ...
    //     <build_tree>/../ABDShared/Contracts
    //  3. <resolved_shared_assets>/contracts       (legado via sharedAssetsDir())
    //  4. <repo_root>/../ABDSharedAssets/contracts (legado directo)

    // 1. Hermano del repositorio: <repo>/../ABDShared/Contracts
    if (const auto root = resolveRepoRoot(); root.isResolved)
    {
        const auto sibling = root.root.getParentDirectory().getChildFile("ABDShared");

        if (sibling.isDirectory())
        {
            const auto contracts = sibling.getChildFile("Contracts");

            if (contracts.isDirectory())
                return contracts;
        }
    }

    // 2. Subiendo desde el ejecutable
    auto current = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                       .getParentDirectory();

    for (int level = 0; level < 7 && current.isDirectory(); ++level)
    {
        const auto parent = current.getParentDirectory();
        const auto candidate = current.getChildFile("ABDShared").getChildFile("Contracts");

        if (candidate.isDirectory())
            return candidate;

        if (parent == current)
            break;

        current = parent;
    }

    // 3. Fallback a legacy: <repo>/../ABDSharedAssets/contracts
    if (const auto assets = sharedAssetsDir(); assets.isDirectory())
    {
        const auto contracts = assets.getChildFile("contracts");

        if (contracts.isDirectory())
            return contracts;
    }

    // 4. Fallback directo a ABDSharedAssets/contracts
    if (const auto root = resolveRepoRoot(); root.isResolved)
    {
        const auto legacy = root.root.getParentDirectory().getChildFile("ABDSharedAssets");

        if (legacy.isDirectory())
        {
            const auto contracts = legacy.getChildFile("contracts");

            if (contracts.isDirectory())
                return contracts;
        }
    }

    return {};
}

} // namespace abdaudiolab::core

