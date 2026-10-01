// ==============================================================================
// ABDAudioLab - El rango de un mando se lee del NOMBRE UNICO del contrato
// ==============================================================================
//
// POR QUE ESTE TEST EXISTE.
//
// Porque el nombre del rango de un control cambio de `min`/`max`/`default` a
// `minVal`/`maxVal`/`defaultVal` en ABDSharedAssets, y los dos parsers de C++
// seguian leyendo el nombre corto. Sincronizar el snapshot sin tocar los
// parsers habria dejado los 34 perfiles cargando, pero con el 0.0/1.0/0.5 de
// los valores por defecto del parser: 68 controles de este catalogo tienen un
// `defaultVal` distinto de 0.5, y todos habrian caido a 0.5 sin que nada
// protestara. Un knob que va a 0.65 porque si en vez de a su valor de fabrica.
//
// El fallo es silencioso por construccion, que es lo que lo hace caro: no hay
// crash, no hay rojo, y el rango 0..1 es plausible para un mando normalizado.
// Un test que solo comprobara "minVal <= maxVal" no lo veria jamas. Este
// compara contra lo que DICE EL FICHERO, no contra un valor escrito aqui.
//
// Y anti-vacuido, siguiendo el criterio que ya usa test_ResourcePathHygiene:
// si el catalogo entero tuviera el default en 0.5, comparar contra el fichero
// pasaria sin mirar nada. Se exige que exista al menos un control cuyo valor
// por defecto NO sea 0.5, que es justo el caso que distingue.
//
// ------------------------------------------------------------------------------

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <juce_core/juce_core.h>

#include "core/LabResourcePaths.h"
#include "core/HardwareContractRegistry.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <fstream>
#include <string>
#include <unordered_map>

using namespace abdaudiolab::core;
using Catch::Approx;

namespace
{

/** Un control tal y como lo declara el JSON, leido aparte del registro. */
struct Declarado
{
    float minVal { 0.0f };
    float maxVal { 1.0f };
    float defaultVal { 0.5f };
    bool tieneRango { false };
    bool tieneDefault { false };
};

/**
 * Lee `functions[].controls[]` del JSON con nlohmann, sin pasar por el registro.
 *
 * Se lee el NOMBRE UNICO a proposito, y no el que el registro use: si el
 * registro se hubiera equivocado, esta lectura seria la que dice la verdad.
 */
std::unordered_map<std::string, Declarado> declaradosDe(const juce::File& contratoJson)
{
    std::unordered_map<std::string, Declarado> porControl;

    std::ifstream in(contratoJson.getFullPathName().toStdString());
    REQUIRE(in.good());
    nlohmann::json j;
    in >> j;

    for (const auto& funcion : j.value("functions", nlohmann::json::array()))
    {
        for (const auto& control : funcion.value("controls", nlohmann::json::array()))
        {
            Declarado d;
            d.tieneRango = control.contains("minVal") && control.contains("maxVal");
            d.tieneDefault = control.contains("defaultVal");

            if (d.tieneRango)
            {
                d.minVal = control.value("minVal", 0.0f);
                d.maxVal = control.value("maxVal", 1.0f);
            }

            if (d.tieneDefault)
                d.defaultVal = control.value("defaultVal", 0.5f);

            porControl[control.value("name", std::string("unnamed"))] = d;
        }
    }

    return porControl;
}

} // namespace

TEST_CASE("El registro lee el rango del mando del nombre que dice el contrato",
          "[contracts][registry][controls][rangename]")
{
    const auto contratosDir = abdaudiolab::core::contractsHardwareDir();
    REQUIRE(contratosDir.isDirectory());

    HardwareContractRegistry registry;
    REQUIRE(registry.loadContractsFromDirectory(contratosDir));

    const auto* dm12 = registry.findContractById("behringer_deepmind12");
    REQUIRE(dm12 != nullptr);

    // Anti-vacuido ANTES de comparar. Si no hay ni un control cuyo default sea
    // distinto de 0.5, el 0.5 del parser y el del fichero coinciden siempre y
    // este test no miraria nada. REQUIRE, no CHECK: no tiene sentido seguir.
    bool hayDefaultNoTrivial = false;

    for (const auto& funcion : dm12->functions)
        for (const auto& control : funcion.controls)
            if (std::abs(control.defaultVal - 0.5f) > 1e-6f)
                hayDefaultNoTrivial = true;

    REQUIRE(hayDefaultNoTrivial);

    // Y ahora si: la comparacion contra el fichero, no contra un valor escrito
    // aqui. Por eso este test no hay que tocarlo cuando cambie un valor de
    // fabrica, y por eso sigue siendo cierto al dia siguiente del que se escribio.
    const auto declarados = declaradosDe(contratosDir.getChildFile("behringer_deepmind12.json"));
    REQUIRE(!declarados.empty());

    std::size_t comparados = 0;

    for (const auto& funcion : dm12->functions)
    {
        for (const auto& control : funcion.controls)
        {
            const auto it = declarados.find(control.name);
            if (it == declarados.end())
                continue;

            const auto& d = it->second;

            if (d.tieneDefault)
            {
                INFO("control '" << control.name << "'");
                CHECK(control.defaultVal == Approx(d.defaultVal).margin(1e-6));
                ++comparados;
            }

            if (d.tieneRango)
            {
                INFO("control '" << control.name << "'");
                CHECK(control.minVal == Approx(d.minVal).margin(1e-6));
                CHECK(control.maxVal == Approx(d.maxVal).margin(1e-6));
            }
        }
    }

    REQUIRE(comparados > 0);
}

TEST_CASE("Un contrato con el nombre viejo sigue leyendose, y sin caer al default",
          "[contracts][registry][controls][rangename]")
{
    // El fallback al nombre corto es deliberado: un contrato que llegue de
    // fuera con `min`/`max`/`default` no debe caer al 0.0/1.0/0.5 en silencio,
    // que es un rango plausible y por tanto invisible. Aqui se escribe un
    // contrato con el nombre viejo y un rango que NO es el del fallback, y se
    // comprueba que el valor llega entero.
    juce::File dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("abdlab_rangename_legacy");

    dir.deleteRecursively();
    REQUIRE(dir.createDirectory());

    const std::string json = R"({
  "schemaVersion": "2.0",
  "id": "rangename_legacy_fixture",
  "displayName": "Fixture de nombre viejo",
  "deviceType": "MANUAL_EURORACK",
  "brand": "Fixture",
  "functions": [
    {
      "id": "f1",
      "name": "Corto",
      "blockType": "SpectrumFilter",
      "controls": [
        { "index": 1, "name": "Legacy Range", "type": "Knob", "min": -24.0, "max": 24.0, "default": -12.0 }
      ]
    }
  ]
})";

    {
        std::ofstream out((dir.getChildFile("rangename_legacy_fixture.json")
                               .getFullPathName()).toStdString());
        REQUIRE(out.good());
        out << json;
    }

    HardwareContractRegistry registry;
    REQUIRE(registry.loadContractsFromDirectory(dir));

    const auto* c = registry.findContractById("rangename_legacy_fixture");
    REQUIRE(c != nullptr);
    REQUIRE(c->functions.size() == 1);
    REQUIRE(c->functions[0].controls.size() == 1);

    const auto& control = c->functions[0].controls[0];

    // Si el parser leyera solo el nombre corto, estos tres serian 0.0/1.0/0.5.
    CHECK(control.minVal == Approx(-24.0f).margin(1e-6));
    CHECK(control.maxVal == Approx(24.0f).margin(1e-6));
    CHECK(control.defaultVal == Approx(-12.0f).margin(1e-6));

    dir.deleteRecursively();
}
