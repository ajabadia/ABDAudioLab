#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "math/ModulationEstimator.h"
#include "math/ModulationMatrixProfile.h"

using namespace abdaudiolab::math;
using Catch::Matchers::WithinAbs;

TEST_CASE("ModulationEstimator: Pure linear response with 4 probe points", "[modulation][math]")
{
    // Modelo: Y = 0.5 * X + 10.0
    // Entradas típicas de velocidad/CC: 32, 64, 96, 127
    std::vector<ModulationProbePoint> points = {
        { 32.0f,  0.5f * 32.0f  + 10.0f },
        { 64.0f,  0.5f * 64.0f  + 10.0f },
        { 96.0f,  0.5f * 96.0f  + 10.0f },
        { 127.0f, 0.5f * 127.0f + 10.0f }
    };

    const int srcVelocity = 1;
    const int dstCutoff   = 10;

    auto node = ModulationEstimator::calculateNode(srcVelocity, dstCutoff, points);

    REQUIRE(node.sourceID == srcVelocity);
    REQUIRE(node.destID == dstCutoff);
    REQUIRE_THAT(static_cast<double>(node.kScalar), WithinAbs(0.5, 1e-4));
    REQUIRE_THAT(static_cast<double>(node.offsetC), WithinAbs(10.0, 1e-4));
    REQUIRE_THAT(static_cast<double>(node.rSquared), WithinAbs(1.0, 1e-4));
}

TEST_CASE("ModulationEstimator: Dead-zone response drops R-squared", "[modulation][math]")
{
    // Simula zona muerta: los valores de excitación < 64 producen 0 respuesta
    // y solo a partir de 64 se produce modulación creciente
    std::vector<ModulationProbePoint> deadZonePoints = {
        { 32.0f,  0.0f },
        { 64.0f,  0.0f },
        { 96.0f,  32.0f },
        { 127.0f, 63.0f }
    };

    auto node = ModulationEstimator::calculateNode(2, 20, deadZonePoints);

    // K debe ser positivo debido a la tendencia general
    REQUIRE(node.kScalar > 0.0f);
    // Debido a la no-linealidad de la zona muerta, R^2 no es 1.0 (debe estar entre 0.80 y 0.96)
    REQUIRE(node.rSquared < 0.98f);
    REQUIRE(node.rSquared > 0.70f);
}

TEST_CASE("ModulationEstimator: Edge cases with degenerate data", "[modulation][math]")
{
    SECTION("Insufficient points (< 2)")
    {
        std::vector<ModulationProbePoint> singlePoint = { { 64.0f, 10.0f } };
        auto node = ModulationEstimator::calculateNode(1, 2, singlePoint);

        REQUIRE(node.kScalar == 0.0f);
        REQUIRE(node.offsetC == 0.0f);
        REQUIRE(node.rSquared == 0.0f);
    }

    SECTION("Constant X input (division by zero safeguard)")
    {
        std::vector<ModulationProbePoint> identicalX = {
            { 64.0f, 10.0f },
            { 64.0f, 20.0f },
            { 64.0f, 30.0f }
        };
        auto node = ModulationEstimator::calculateNode(1, 2, identicalX);

        REQUIRE(node.kScalar == 0.0f);
        REQUIRE_THAT(static_cast<double>(node.offsetC), WithinAbs(20.0, 1e-4)); // media de Y
        REQUIRE(node.rSquared == 0.0f);
    }

    SECTION("Constant Y output (perfect horizontal slope)")
    {
        std::vector<ModulationProbePoint> horizontal = {
            { 32.0f,  15.0f },
            { 64.0f,  15.0f },
            { 96.0f,  15.0f },
            { 127.0f, 15.0f }
        };
        auto node = ModulationEstimator::calculateNode(1, 2, horizontal);

        REQUIRE_THAT(static_cast<double>(node.kScalar), WithinAbs(0.0, 1e-5));
        REQUIRE_THAT(static_cast<double>(node.offsetC), WithinAbs(15.0, 1e-4));
        REQUIRE_THAT(static_cast<double>(node.rSquared), WithinAbs(1.0, 1e-4));
    }
}

TEST_CASE("ModulationMatrixProfile: Sparse storage and JSON serialization", "[modulation][profile]")
{
    ModulationMatrixProfile profile;

    // Nodo 1: Source 1 (Velocity) -> Dest 10 (Cutoff)
    ModulationNode n1;
    n1.sourceID = 1;
    n1.destID = 10;
    n1.kScalar = 0.75f;
    n1.offsetC = 2.0f;
    n1.rSquared = 0.998f;
    n1.probePoints = { { 32.0f, 26.0f }, { 64.0f, 50.0f }, { 96.0f, 74.0f }, { 127.0f, 97.25f } };

    // Nodo 2: Source 2 (Aftertouch) -> Dest 15 (Resonance)
    ModulationNode n2;
    n2.sourceID = 2;
    n2.destID = 15;
    n2.kScalar = 1.25f;
    n2.offsetC = -5.0f;
    n2.rSquared = 0.985f;
    n2.probePoints = { { 32.0f, 35.0f }, { 64.0f, 75.0f }, { 96.0f, 115.0f }, { 127.0f, 153.75f } };

    profile.setNode(1, 10, n1);
    profile.setNode(2, 15, n2);

    REQUIRE(profile.hasNode(1, 10));
    REQUIRE(profile.hasNode(2, 15));
    REQUIRE_FALSE(profile.hasNode(1, 15));
    REQUIRE_FALSE(profile.hasNode(3, 10));

    auto retrieved1 = profile.getNode(1, 10);
    REQUIRE(retrieved1.sourceID == 1);
    REQUIRE(retrieved1.destID == 10);
    REQUIRE_THAT(static_cast<double>(retrieved1.kScalar), WithinAbs(0.75, 1e-4));

    auto allNodes = profile.getAllNodes();
    REQUIRE(allNodes.size() == 2);

    // Verificación de serialización juce::var / JSON
    juce::var jsonVar = profile.toDynamicVar();
    REQUIRE(jsonVar.isObject());
    
    auto* obj = jsonVar.getDynamicObject();
    REQUIRE(obj != nullptr);
    REQUIRE(obj->hasProperty("modulationNodes"));

    auto* nodesArray = obj->getProperty("modulationNodes").getArray();
    REQUIRE(nodesArray != nullptr);
    REQUIRE(nodesArray->size() == 2);

    juce::String jsonString = juce::JSON::toString(jsonVar);
    REQUIRE(jsonString.contains("kScalar"));
    REQUIRE(jsonString.contains("sourceID"));
    REQUIRE(jsonString.contains("destID"));

    // Verify object values directly via JUCE var
    auto firstNodeVar = (*nodesArray)[0];
    REQUIRE(firstNodeVar.isObject());
    REQUIRE(firstNodeVar.hasProperty("sourceID"));
    REQUIRE(firstNodeVar.hasProperty("kScalar"));

    profile.clear();
    REQUIRE_FALSE(profile.hasNode(1, 10));
    REQUIRE(profile.getAllNodes().empty());
}
