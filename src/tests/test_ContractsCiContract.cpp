// ==============================================================================
// ABDAudioLab - El CI tiene que mirar el snapshot, y se comprueba que lo haga
// ==============================================================================
//
// QUE HACE ESTO, Y POR QUE ES UNA GUARDA Y NO UN COMENTARIO.
//
// Hay dos formas de que el CI de este repositorio deje de vigilar el snapshot de
// contratos, y las dos son silenciosas:
//
//   1. Que no baje ABDSharedAssets. Entonces `sharedAssetsDir()` devuelve un
//      File invalido, el test de drift responde SKIP, y SKIP en Catch2 no es una
//      pasada. El job queda en verde sin haber comparado un solo fichero.
//
//   2. Que lo baje pero el filtro de rutas no incluya `contracts/`. Entonces un
//      PR que solo toca un JSON del snapshot —justo el PR que hay que cazar—
//      no dispara el workflow. El check aparece en verde, sin correr, para
//      siempre.
//
// Las dos sequitaron sin que nadie se enterara, porque un workflow que no se
// ejecuta no falla: no aparece ningun rojo en ninguna parte. Y las dos se
// pueden volver a quitar en un commit de limpieza, con la mejor de las
// intenciones.
//
// Por eso este test lee el workflow y mira que las tres piezas esten. Es un
// guard sobre un fichero de texto, que es lo mas fragil que existe, y aun asi
// compensa: el fallo de no tenerlo es un verde perpetuo.
//
// ----------------------------------------------------------------------------
// LO QUE NO COMPRUEBA, DICHO DE ANTES.
//
// No ejecuta el CI. No puede. Comprueba que el workflow DECLARA lo que tiene
// que declarar; que luego el job pase o no es cosa de GitHub. Un guard que
// el workflow no sustituye a correr el preflight, lo prepara: si el paso
// desaparece, esto se pone rojo el mismo dia en lugar de tres meses despues.
//
// Y no comprueba que los SHA de los hermanos sean los correctos. Eso lo sabe el
// checkout, no este fichero.

#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>

#include "core/LabResourcePaths.h"

#include <string>

namespace
{

/**
 * El workflow, una vez, y como texto plano.
 *
 * Se lee entero a proposito y no se parsea: no hay un parser de YAML en las
 * librerias que este target ya enlaza, y traer uno por leer un fichero de
 * configuracion seria mas codigo que el que vigila. Ademas lo que se busca son
 * cadenas literales —una ruta de filtro, el nombre de un checkout—, y eso es
 * texto, no estructura.
 */
juce::String workflowDeLaPuerta()
{
    const auto ruta = abdaudiolab::core::repoResource(".github/workflows/audio-ab-5d-ci.yml");

    // Sin fichero, este test no tiene nada que decir. Pero se dice, en lugar de
    // darlo por bueno: un guard que se apaga porque no encuentra el fichero es
    // un guard apagado.
    REQUIRE(ruta.existsAsFile());

    return ruta.loadFileAsString();
}

/**
 * Cuantas veces aparece un trozo, y no solo si aparece.
 *
 * Contar importa por una razon concreta: los filtros de rutas estan en DOS
 * sitios del workflow, en `push` y en `pull_request`, y con un solo filtro el
 * otro evento sigue sin dispararse. Un `contains` no distinguiria uno de dos.
 *
 * A mano porque `juce::String` no tiene contador de ocurrencias: tiene
 * `indexOf` y `lastIndexOf`, y un `countMatches` que no existe en esta
 * version se cuela sin avisar hasta que falla la compilacion. Ojo al orden de
 * los argumentos de `indexOf`: el indice va PRIMERO.
 */
int apariciones(const juce::String& texto, const char* trozo)
{
    const juce::String busqueda(trozo);
    int total = 0;
    int desde = 0;

    for (;;)
    {
        const auto donde = texto.indexOf(desde, busqueda);
        if (donde < 0)
            break;

        ++total;
        desde = donde + 1;
    }

    return total;
}

} // namespace

// ----------------------------------------------------------------------------

TEST_CASE("El CI baja ABDSharedAssets, o el test de drift hace SKIP",
          "[hygiene][contracts][ci][snapshot]")
{
    const auto w = workflowDeLaPuerta();

    // Anti-vacío. Un fichero de configuracion que se leyera a medias daria
    // `w == ""` y todas las busquedas darian cero, que es exactamente el falso
    // verde que este test existe para evitar.
    REQUIRE(w.length() > 1000);

    // El checkout tiene que ser del repositorio hermano, no un `path:` a secas.
    // Un checkout sin `repository:` seria este mismo repo dos veces.
    REQUIRE(w.contains("repository: ajabadia/ABDSharedAssets"));

    // Y tiene que ir en el job de Windows, que es el que compila y corre la
    // suite. Un checkout en el job del preflight no haria que el test de drift
    // dejase de hacer SKIP, porque ese test corre en el otro.
    REQUIRE(w.contains("Checkout ABDSharedAssets (contract origin)"));
}

TEST_CASE("El CI corre el snapshot y la cuarentena con etiqueta propia",
          "[hygiene][contracts][ci][snapshot][quarantine]")
{
    const auto w = workflowDeLaPuerta();

    // `[snapshot]` es el caso de drift, el que compara byte a byte. `[contracts]`
    // habria valido tambien y habria arrastrado trece ficheros de test, dos de
    // ellos de hosting real: un fallo de contrato tiene que leerse como un fallo
    // de contrato.
    REQUIRE(w.contains("[snapshot]"));

    // Y la puerta de cuarentena, que es el otro lado del encargo: un retenido
    // sin motivo sale del cajon sin estar marcado en NINGUN lenguaje.
    REQUIRE(w.contains("[quarantine]"));

    // Los dos en un paso propio, no escondidos en `[audioab_5d]`. Se comprueba
    // que existe un Gate 7 porque es lo que se lee en la pestaña: si el dia de
    // mañana se fusiona dentro del Gate 5, el check pierde su nombre y con el
    // nombre se pierde lo que se puede pedir como required.
    REQUIRE(w.contains("Gate 7"));
}

TEST_CASE("Un PR que solo toca contratos dispara el workflow",
          "[hygiene][contracts][ci][snapshot]")
{
    const auto w = workflowDeLaPuerta();

    // El filtro de rutas esta DOS veces: en `push` y en `pull_request`. Con una
    // sola vez, el otro evento sigue sin dispararse, y el snapshot se
    // desfasaria por el camino del push sin que nadie mire. Por eso se cuenta y
    // no se busca.
    //
    // Este es el fragmento que mas veces ha faltado. Sin el, todo lo de arriba
    // es correcto y no se ejecuta nunca.
    REQUIRE(apariciones(w, "'contracts/**'") >= 2);

    // Y el fichero de codigo que decide si el snapshot esta desfasado tambien
    // entra, o cambiar la logica del drift no moveria el CI.
    REQUIRE(apariciones(w, "src/tests/test_ContractsSnapshotDrift.cpp") >= 2);
}

TEST_CASE("El preflight de ABDSharedAssets corre en el CI del laboratorio",
          "[hygiene][contracts][ci][snapshot]")
{
    const auto w = workflowDeLaPuerta();

    // El preflight de contratos corre en su propio job hermetico en el CI del laboratorio,
    // con checkouts especificos de ABDAudioLab y ABDSharedAssets.
    REQUIRE(w.contains("contracts-preflight:"));

    // El step verifica de forma determinista la paridad exacta de los contratos de hardware.
    REQUIRE(w.contains("Verify hardware contracts parity (40 files)"));

    // Exige exactamente los 40 contratos de hardware en ambos repositorios.
    REQUIRE(w.contains("Expected exactly 40 hardware contracts in ABDSharedAssets"));
    REQUIRE(w.contains("Expected exactly 40 hardware contracts in ABDAudioLab"));

    // Comprueba que los nombres coinciden y que no sobran ni faltan archivos.
    REQUIRE(w.contains("Missing file in ABDAudioLab"));
    REQUIRE(w.contains("Extra file in ABDAudioLab"));

    // Verifica que el contenido binario es byte a byte identico.
    REQUIRE(w.contains("Content mismatch in contract"));

    // Sale con codigo 1 si existe cualquier desfase o discrepancia.
    REQUIRE(w.contains("Hardware contracts verification FAILED"));

    // Desacoplado: no clona repositorios hermanos ajenos.
    REQUIRE(!w.contains("fetch-missing-siblings"));

    // No depende de la regeneracion de ABDEep ni de pnpm run preflight.
    REQUIRE(!w.contains("pnpm run preflight"));
}