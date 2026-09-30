/**
 * @file test_LabResourcePaths.cpp
 * @brief Tests del resolvedor único de rutas del repositorio.
 *
 * Estos tests son la red de seguridad del refactor que eliminó 37 helpers de
 * rutas duplicados. Verifican que la resolución NO depende del directorio de
 * trabajo, que es exactamente la propiedad que faltaba antes.
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>

#include "core/LabResourcePaths.h"

#include <fstream>

using namespace abdaudiolab::core;

TEST_CASE("LabResourcePaths: la raiz del repositorio se resuelve sin depender del CWD",
          "[core][resourcepaths][hermetic]")
{
    const auto resolution = resolveRepoRoot();

    REQUIRE(resolution.isResolved);
    REQUIRE(resolution.root.isDirectory());

    // El marcador versionado debe ser la prueba de que es la raiz correcta.
    REQUIRE(resolution.root.getChildFile("ABDAudioLab.workspace").existsAsFile());
    REQUIRE(resolution.root.getChildFile("CMakeLists.txt").existsAsFile());
}

TEST_CASE("LabResourcePaths: requireRepoRoot() agrees with resolveRepoRoot()",
          "[core][resourcepaths]")
{
    const auto root = requireRepoRoot();
    REQUIRE(root.isDirectory());
    REQUIRE(root.getChildFile("ABDAudioLab.workspace").existsAsFile());
}

TEST_CASE("LabResourcePaths: el catalogo de contratos esta versionado en el repositorio",
          "[core][resourcepaths][contracts]")
{
    // Este test documenta la invariante que desbloqueó toda la suite: si
    // contracts/hardware desapareciera del control de versiones, la resolucion
    // por marcador seguiria funcionando pero NO habria catalogo que cargar.
    const auto contractsDir = contractsHardwareDir();
    REQUIRE(contractsDir.isDirectory());

    const auto jsonFiles = contractsDir.findChildFiles(juce::File::findFiles, false, "*.json");
    REQUIRE(jsonFiles.size() >= 39);
}

TEST_CASE("LabResourcePaths: los accesores con nombre resuelven dentro de la raiz",
          "[core][resourcepaths]")
{
    const auto root = requireRepoRoot();

    CHECK(contractsHardwareDir().isDirectory());
    CHECK(canonicalTargetsDir().isDirectory());
    CHECK(profilingPresetsDir().isDirectory());
    CHECK(profilesDir().isDirectory());
    CHECK(docsQaDir().isDirectory());
    CHECK(fixturesDir().isDirectory());
    CHECK(assetsDir().isDirectory());

    // Ningun accesor puede escapar de la raiz del repositorio.
    CHECK(contractsHardwareDir().isAChildOf(root));
    CHECK(canonicalTargetsDir().isAChildOf(root));
    CHECK(profilingPresetsDir().isAChildOf(root));
    CHECK(docsQaDir().isAChildOf(root));
}

TEST_CASE("LabResourcePaths: preset de perfilado versionado disponible",
          "[core][resourcepaths][presets]")
{
    const auto presets = profilingPresetsDir();
    REQUIRE(presets.isDirectory());
    REQUIRE(presets.getChildFile("quick_vcf_3pts.json").existsAsFile());
}

TEST_CASE("LabResourcePaths: se rechazan rutas relativas inseguras",
          "[core][resourcepaths][security]")
{
    CHECK_FALSE(isSafeRepoRelativePath("../escape"));
    CHECK_FALSE(isSafeRepoRelativePath("presets/../../escape"));
    CHECK_FALSE(isSafeRepoRelativePath("C:/absolute/drive"));
    CHECK_FALSE(isSafeRepoRelativePath("\\\\server\\share"));
    CHECK_FALSE(isSafeRepoRelativePath("/posix/absolute"));
    CHECK_FALSE(isSafeRepoRelativePath("   "));

    CHECK(isSafeRepoRelativePath("presets/profiling"));
    CHECK(isSafeRepoRelativePath("contracts/hardware"));
    CHECK(isSafeRepoRelativePath("docs/qa/runs"));

    CHECK_THROWS_AS(repoResource("../escape"), std::runtime_error);
    CHECK_THROWS_AS(repoResource("C:/absolute"), std::runtime_error);
}

TEST_CASE("LabResourcePaths: la resolucion es estable entre llamadas",
          "[core][resourcepaths]")
{
    // Deliberadamente NO se memoriza el resultado: los tests que manipulan
    // ABDAUDIOLAB_REPO_ROOT deben observar su efecto de inmediato.
    const auto first = requireRepoRoot();
    const auto second = requireRepoRoot();
    CHECK(first == second);
}
