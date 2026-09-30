/**
 * @file LabResourcePaths.h
 * @brief Single deterministic owner for resolving ABDAudioLab repository resources.
 *
 * WHY THIS EXISTS
 * ---------------
 * Path resolution was previously re-implemented in 37 local helpers across the
 * test suite and 9 production files. Almost all of them resolved resources via
 * juce::File::getCurrentWorkingDirectory(), optionally probing one parent
 * directory. That made the suite CWD-dependent: running ABDAudioLab_Tests.exe
 * from build/ instead of the repository root failed 61 test cases, while the
 * same binary run from the root passed all 895. The suite was green by accident
 * of the working directory rather than by hermeticity.
 *
 * This module removes that class of defect by anchoring every lookup to a
 * versioned marker file (ABDAudioLab.workspace) that is committed to git and
 * therefore present in any clean clone, at any working directory.
 *
 * RESOLUTION HIERARCHY (deterministic, no drive-letter guessing)
 * ------------------------------------------------------------
 *   1. ABDAUDIOLAB_REPO_ROOT environment variable, if it validates.
 *   2. Ascend from the current executable's directory to the filesystem root,
 *      looking for the marker.
 *   3. Ascend from the current working directory to the filesystem root.
 *   4. Fail with a diagnostic listing every candidate that was probed.
 *
 * There is deliberately no fallback to a hard-coded drive or user directory.
 *
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_core/juce_core.h>
#include <string>
#include <vector>

namespace abdaudiolab::core
{

/**
 * @brief Resultado de resolución de la raíz del repositorio.
 */
struct RepoRootResolution
{
    juce::File root;                   /**< Raíz validada, o inválida si isResolved == false. */
    bool isResolved { false };
    std::vector<std::string> probed;   /**< Candidatos probados, en orden de intento. */
    std::string reason;                /**< Motivo legible del éxito o del fallo. */
};

/**
 * @brief Resuelve la raíz del repositorio ABDAudioLab usando el marcador
 *        versionado ABDAudioLab.workspace.
 *
 * Esta es la única función del proyecto autorizada para localizar la raíz del
 * repositorio. No memoriza el resultado entre llamadas para que los tests que
 * manipulan ABDAUDIOLAB_REPO_ROOT puedan observar su efecto de inmediato.
 *
 * @return RepoRootResolution con isResolved == true si se encontró la raíz.
 */
[[nodiscard]] RepoRootResolution resolveRepoRoot();

/**
 * @brief Resuelve la raíz del repositorio o lanza una excepción descriptiva.
 * @throws std::runtime_error con la lista de candidatos probados si no se encuentra.
 */
[[nodiscard]] juce::File requireRepoRoot();

/**
 * @brief Resuelve un recurso relativo a la raíz del repositorio.
 * @param relativePath Ruta relativa con separadores '/' (p. ej. "presets/profiling").
 * @throws std::runtime_error si la ruta es unsafe o si no se puede resolver la raíz.
 */
[[nodiscard]] juce::File repoResource(const juce::String& relativePath);

/**
 * @brief Variante NO lanzante de repoResource(), para codigo de produccion.
 *
 * repoResource() lanza excepcion cuando no encuentra la raiz: en un test eso es
 * lo correcto (fallo ruidoso, con la lista de candidatos probados). En la
 * aplicacion no lo es: un producto lanzado fuera del arbol del repositorio
 * abortaria el arranque, cuando antes degradaba con elegancia y seguia
 * funcionando sin contratos.
 *
 * @return El recurso, o un juce::File inválido si no se pudo resolver.
 *         Los llamadores comprueban con isDirectory()/existsAsFile().
 */
[[nodiscard]] juce::File optionalRepoResource(const juce::String& relativePath);

/**
 * @brief Devuelve true si la ruta relativa es segura (sin absolutas, sin "..",
 *        sin tokens de red). Centraliza la validación que antes se repetía
 *        en varios helpers de test.
 */
[[nodiscard]] bool isSafeRepoRelativePath(const juce::String& relativePath);

// ---------------------------------------------------------------------------
// Accesores con nombre para los recursos de uso frecuente en la suite.
// Cada uno es una delgada capa sobre repoResource(), no una implementación nueva.
// ---------------------------------------------------------------------------

/** @brief contracts/hardware/ — catálogo de contratos de hardware versionado. */
[[nodiscard]] juce::File contractsHardwareDir();

/** @brief profiles/targets/ — perfiles de destino canónicos. */
[[nodiscard]] juce::File canonicalTargetsDir();

/** @brief presets/profiling/ — recetas de perfilado. */
[[nodiscard]] juce::File profilingPresetsDir();

/** @brief profiles/ — raíz de perfiles. */
[[nodiscard]] juce::File profilesDir();

/** @brief docs/qa/ — artefactos de QA canónicos de Audio A/B 5D. */
[[nodiscard]] juce::File docsQaDir();

/** @brief docs/qa/runs/ — reportes de corrida de Audio A/B 5D. */
[[nodiscard]] juce::File docsQaRunsDir();

/** @brief fixtures/ — fixtures de test herméticas. */
[[nodiscard]] juce::File fixturesDir();

/** @brief fixtures/evaluations/ — evaluaciones de fixture. */
[[nodiscard]] juce::File fixturesEvaluationsDir();

/** @brief assets/ — recursos de interfaz. */
[[nodiscard]] juce::File assetsDir();

/** @brief exported_luts/ — LUTs exportados. */
[[nodiscard]] juce::File exportedLutsDir();

/**
 * @brief ABDSharedAssets/ — repositorio hermano de assets compartidos.
 *
 * NO es un recurso de este repositorio: vive al lado, en
 * <padre-de-la-raiz>/ABDSharedAssets, y no está versionado aquí. Por eso no
 * puede resolverse con repoResource(): se busca primero como hermano de la raíz
 * del repositorio y, si no existe, subiendo desde el ejecutable.
 *
 * Antes cada consumidor lo localizaba por su cuenta, casi siempre probando el
 * directorio de trabajo y con una ruta absoluta de la máquina del autor como
 * último recurso. Centralizarlo aquí hace que esa búsqueda sea única y
 * observable desde los tests.
 *
 * @return El directorio si existe; un File inválido si no se encuentra.
 *         No lanza: un producto puede funcionar sin assets compartidos.
 */
[[nodiscard]] juce::File sharedAssetsDir();

} // namespace abdaudiolab::core
