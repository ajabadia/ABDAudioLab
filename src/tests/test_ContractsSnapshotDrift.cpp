// ==============================================================================
// ABDAudioLab - El snapshot de contratos no puede quedarse viejo sin que se note
// ==============================================================================
//
// EL FALLO QUE ESTE TEST EXISTE PARA TAPONAR.
//
// `contracts/hardware/` es una COPIA de `ABDSharedAssets/contracts/`, versionada
// aqui a proposito —el .gitignore lo dice: "contracts/ is deliberately NOT
// ignored"—, porque el registro de hardware y la suite la necesitan en un clon
// limpio y en CI, donde no hay hermano al que enlazar.
//
// Y una copia versionada es exactamente el comienzo de dos verdades. El origen
// cambia, la copia no, y no pasa nada: los dos lados siguen dando verde, porque
// ninguna de las dos comprobaciones que existen mira a la otra. El panel dibuja
// con el contrato viejo, el motor registra con el nuevo, y el unico sintoma es
// que un comportamiento medido no se reproduce.
//
// QUE SE COMPARA, Y POR QUE BYTE A BYTE.
//
// Los *.json del origen contra los *.json de la copia, y byte a byte, no "el
// mismo JSON". Dos ficheros con el mismo contenido y distinto formato son la
// misma verdad, y un guard que los declara distintos obliga a normalizar
// formato por formato, que es trabajo de nadie y motivo para desactivar el
// guard. El unico caso donde el byte a byte importa es el que ya se ha dado: el
// repo de origen declara `*.json text eol=lf` y este antes no declaraba nada, de
// modo que git convertia a CRLF al reescribir y la copia dejaba de coincidir
// sin que nadie hubiera tocado un contrato. El unico cambio era el fin de
// linea, y es justo el que un diff no enseña.
//
// QUE PASA CUANDO EL HERMANO NO ESTA, Y POR QUE NO ES UN "TODO BIEN".
//
// `sharedAssetsDir()` devuelve un File inválido cuando no encuentra
// ABDSharedAssets, que es el caso de un clon limpio y el del CI de este
// repositorio: el workflow hace checkout de ABDAudioLab y ABDSharedCode, y NO
// de ABDSharedAssets. Alli no hay contra qué comparar, y la respuesta honesta
// es SKIP, que en Catch2 es una corrida que NO cuenta como pasada.
//
// Un guard que se declares verde sin mirar es peor que no tener guard: es un
// guard que se desactiva solo. Por eso el caso sin origen salta con SKIP y un
// motivo, y por eso la LOGICA de la comparacion se prueba aparte, con
// directorios temporales, en un caso que SI se ejecuta siempre.
//
// ----------------------------------------------------------------------------
// EL ALCANCE REAL DE ESTA GUARDA, DICHO DE ANTES.
//
// Detecta el desfase en el momento en que se introduce, que es cuando alguien
// edita un contrato en ABDSharedAssets: quien edita un repositorio lo tiene a
// mano, asi que ahi esta el hermano. En un clon sin hermano este test no puede
// decir nada, y no lo disimula. Lo que SI hace ahi es lo unico que puede:
// comprobar que la copia esta integra, y que su contenido es el que el
// inventario de mas arriba fija en 40 ficheros y 6 esquemas.
//
// Que el CI de este repositorio lo vigile en cada push exige anadir ahi un
// checkout fijado de ABDSharedAssets, que es una decision de politica de refs
// —cual SHA se fija— y por eso no se ha hecho desde un test.
// =============================================================================

#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>

#include "core/LabResourcePaths.h"
#include "core/ExperimentStorage.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <string>
#include <vector>

using namespace abdaudiolab::core;

namespace
{

/** Lo que separa dos catalogos. Vacio = identicos. */
struct Diferencia
{
    std::vector<std::string> distintos;  /**< Mismo nombre, distinto contenido. */
    std::vector<std::string> faltan;     /**< En el origen, ausentes en la copia. */
    std::vector<std::string> sobran;     /**< En la copia, ausentes en el origen. */
    std::size_t comparados { 0 };        /**< Ficheros leidos en ambos lados. */

    [[nodiscard]] bool vacia() const noexcept
    {
        return distintos.empty() && faltan.empty() && sobran.empty();
    }
};

/** Los *.json de un directorio, ordenados, para que el fallo sea legible. */
std::vector<std::string> jsonDe(const juce::File& dir)
{
    std::vector<std::string> nombres;

    if (!dir.isDirectory())
        return nombres;

    for (const auto& entry : dir.findChildFiles(juce::File::findFiles, false, "*.json"))
        nombres.push_back(entry.getFileName().toStdString());

    std::sort(nombres.begin(), nombres.end());
    return nombres;
}

void escribir(const juce::File& archivo, const std::string& contenido)
{
    std::ofstream out(archivo.getFullPathName().toStdString(), std::ios::binary);

    if (out.good())
        out << contenido;
}

/**
 * Compara la copia contra el origen. Es una FUNCION, y no codigo suelto dentro
 * del test, por una razon concreta: la comparacion tiene que poder probarse sin
 * el hermano al lado. El caso de mas abajo la ejercita con dos directorios
 * temporales y con cada uno de los tres fallos que puede detectar, y ese caso se
 * ejecuta siempre, tambien en un clon sin ABDSharedAssets. Asi la logica de la
 * guarda nunca queda sin probar, que es lo que le pasa a un guard que solo se
 * puede ejercitar en la maquina de quien lo escribio.
 */
Diferencia compararCatalogos(const juce::File& origen, const juce::File& copia)
{
    Diferencia d;

    const auto enOrigen = jsonDe(origen);
    const auto enCopia = jsonDe(copia);
    const std::set<std::string> delOrigen(enOrigen.begin(), enOrigen.end());
    const std::set<std::string> deLaCopia(enCopia.begin(), enCopia.end());

    for (const auto& nombre : enOrigen)
    {
        if (deLaCopia.count(nombre) == 0)
        {
            d.faltan.push_back(nombre);
            continue;
        }

        ++d.comparados;

        // `computeFileSha256` es el helper que ya usa el resto del proyecto para
        // el calculo canonico. Reusarlo es lo que evita que este test tenga su
        // propia idea de como se hashea un fichero, que es la via clasica de que
        // dos guards comparen cosas distintas y los dos tengan razon.
        if (ExperimentStorage::computeFileSha256(origen.getChildFile(nombre))
            != ExperimentStorage::computeFileSha256(copia.getChildFile(nombre)))
            d.distintos.push_back(nombre);
    }

    for (const auto& nombre : enCopia)
        if (delOrigen.count(nombre) == 0)
            d.sobran.push_back(nombre);

    return d;
}

/** Crea un directorio temporal vacio y limpio. */
juce::File dirTemporal(const juce::String& nombre)
{
    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile(nombre);

    dir.deleteRecursively();
    dir.createDirectory();
    return dir;
}

} // namespace

// ----------------------------------------------------------------------------
// LA PUERTA CIEGA, Y POR QUE ESTE CASO ESTA SEPARADO DEL DE ARRIBA.
//
// El caso de arriba empieza con `REQUIRE(copia.isDirectory())` y por eso, en
// teoria, ya deberia canto: si la copia es una junction, en Windows
// `isDirectory()` sigue diciendo que SI, porque el enlace resuelve a un
// directorio. Ese `REQUIRE` no muerde.
//
// Y entonces la comparacion lee el origen a traves del enlace, compara el
// origen consigo mismo, y dice que los 40 ficheros son iguales. El caso pasa en
// verde sin haber comparado nada. Es la misma puerta ciega que el preflight
// tapa, vista desde C++: dos guard, un solo agujero.
//
// Por eso el caso va aparte y NO depende del hermano. Solo mira como esta la
// ruta, que es una pregunta que tiene respuesta en un clon limpio y en el CI
// tanto si ABDSharedAssets esta como si no. Un guard que depende de un
// repositorio hermano y ademas de un enlace son dos dependencias que se
// pueden ir a la vez.
TEST_CASE("El snapshot de contratos es un directorio de verdad, no una junction",
          "[hygiene][contracts][drift][snapshot]")
{
    const auto copia = abdaudiolab::core::contractsHardwareDir();

    if (!copia.exists())
    {
        // Sin copia no hay nada que mirar. Y esto NO es la puerta ciega: la
        // puerta ciega es que HAYA algo y no sea una copia. Borrar el snapshot
        // es una decision legitima —de hecho es el arreglo que el propio
        // preflight sugiere—, asi que esto tiene que seguir siendo verde.
        SKIP("contracts/hardware no existe. No hay snapshot que pueda ser un enlace.");
    }

    // Esta es toda la guarda. Una linea.
    //
    // REQUIRE y no CHECK porque no tiene sentido seguir: si la ruta es un
    // enlace, todo lo que se compare a continuacion se compara con el origen
    // y el resultado no significa nada. El orden importa, y por eso va ANTES
    // del `isDirectory()` de abajo, no despues.
    //
    // En Windows JUCE resuelve `isSymbolicLink()` mirando
    // FILE_ATTRIBUTE_REPARSE_POINT, y una junction es un reparse point, asi
    // que una junction de directorio sale aqui como enlace. Ese `true` en un
    // `isDirectory()` tambien es lo que hace el agujero.
    REQUIRE_FALSE(copia.isSymbolicLink());

    // Y el segundo `REQUIRE` que el caso de arriba tiene pero que aqui ya no
    // hace falta: con lo de arribarumbo, `isDirectory()` ya no puede decir que
    // si por un enlace. Se deja igualmente, porque tambien avisa del otro
    // fallo posible —un fichero con nombre de directorio— y no cuesta nada.
    REQUIRE(copia.isDirectory());
}

TEST_CASE("El snapshot de contratos coincide con ABDSharedAssets byte a byte",
          "[hygiene][contracts][drift][snapshot]")
{
    const auto origen = abdaudiolab::core::sharedAssetsDir().getChildFile("contracts");

    if (!origen.isDirectory())
    {
        // Sin hermano no hay contra qué comparar. Se dice con SKIP, y no con un
        // `return` que dejaria el caso en verde: un guard verde sin mirar es un
        // guard que se ha desactivado solo, que es peor que no tenerlo.
        SKIP("ABDSharedAssets/contracts no esta disponible en esta maquina. "
             "Se ha buscado como hermano de la raiz del repositorio y subiendo "
             "desde el ejecutable (ver LabResourcePaths::sharedAssetsDir). "
             "En un clon limpio y en el CI de este repositorio el caso NO se "
             "comprueba: por eso el test siguiente, que si se ejecuta siempre, "
             "prueba la logica de la comparacion con directorios temporales.");
    }

    REQUIRE(origen.isDirectory());

    const auto copia = abdaudiolab::core::contractsHardwareDir();
    REQUIRE(copia.isDirectory());

    const auto d = compararCatalogos(origen, copia);

    // Anti-vacuido, y en la forma que ya usa el resto del proyecto: si la
    // comparacion no ha leido nada, esto pasaria sin mirar nada. REQUIRE, no
    // CHECK: no tiene sentido seguir.
    REQUIRE(d.comparados > 0);
    INFO("ficheros comparados: " << d.comparados);

    if (d.vacia())
        return;

    // El mensaje dice QUE hacer, no solo que algo va mal. "El snapshot esta
    // desfasado" obliga a investigar; "copia estos 9 ficheros desde
    // ABDSharedAssets/contracts" se ejecuta y se acaba.
    FAIL_CHECK("El snapshot " << copia.getFullPathName().toStdString()
                             << " no coincide con el origen " << origen.getFullPathName().toStdString());

    for (const auto& nombre : d.distintos)
        FAIL_CHECK("  distinto:  " << nombre << "  (el origen cambio, la copia no)");

    for (const auto& nombre : d.faltan)
        FAIL_CHECK("  falta en la copia: " << nombre);

    for (const auto& nombre : d.sobran)
        FAIL_CHECK("  sobra en la copia:  " << nombre << "  (esta en la copia y no en el origen)");

    FAIL_CHECK("Se arregla copiando los .json del origen encima de la copia, y "
               "despues ajustando los recuentos del inventario de contratos.");
}

TEST_CASE("La comparacion del snapshot detecta los tres fallos, sin necesitar el hermano",
          "[hygiene][contracts][drift][snapshot]")
{
    // Este caso NO depende de ABDSharedAssets, y por eso se ejecuta siempre: en
    // el clon limpio y en el CI donde el caso de arriba hace SKIP.
    //
    // Lo que protege es lo unico que puede quedar sin probar en un entorno sin
    // hermano: que la comparacion detecte las tres cosas que puede ir mal. Un
    // guard que en la maquina de quien lo escribio hacia algo distinto de lo
    // que el mensaje dice, y que en todas las demas no hace nada, es un guard
    // que parece funcionar y no funciona.
    const auto origen = dirTemporal("abdlab_drift_origen");
    const auto copia = dirTemporal("abdlab_drift_copia");

    REQUIRE(origen.isDirectory());
    REQUIRE(copia.isDirectory());

    const std::string a = "{\"id\":\"a\"}\n";
    const std::string b = "{\"id\":\"b\"}\n";

    SECTION("identicos -> nada que decir")
    {
        escribir(origen.getChildFile("a.json"), a);
        escribir(copia.getChildFile("a.json"), a);
        escribir(origen.getChildFile("b.json"), b);
        escribir(copia.getChildFile("b.json"), b);

        const auto d = compararCatalogos(origen, copia);
        CHECK(d.vacia());
        CHECK(d.comparados == 2);
    }

    SECTION("mismo nombre, distinto contenido -> distinto")
    {
        escribir(origen.getChildFile("a.json"), a);
        escribir(copia.getChildFile("a.json"), b);

        const auto d = compararCatalogos(origen, copia);
        CHECK_FALSE(d.vacia());
        REQUIRE(d.distintos.size() == 1);
        CHECK(d.distintos[0] == "a.json");
        CHECK(d.faltan.empty());
        CHECK(d.sobran.empty());
    }

    SECTION("una diferencia de un solo byte tambien se ve")
    {
        // El caso que un "mismo JSON" no veria: dos ficheros que se parsean
        // igual y no son iguales. Con el fin de linea, es el que ya se dio de
        // verdad entre los dos repos, asi que no es hipotetico.
        escribir(origen.getChildFile("a.json"), a);
        escribir(copia.getChildFile("a.json"), a + " ");

        const auto d = compararCatalogos(origen, copia);
        REQUIRE(d.distintos.size() == 1);
        CHECK(d.distintos[0] == "a.json");
    }

    SECTION("en el origen y no en la copia -> falta")
    {
        escribir(origen.getChildFile("a.json"), a);
        escribir(copia.getChildFile("b.json"), b);

        const auto d = compararCatalogos(origen, copia);
        REQUIRE(d.faltan.size() == 1);
        CHECK(d.faltan[0] == "a.json");
        CHECK(d.distintos.empty());
    }

    SECTION("en la copia y no en el origen -> sobra")
    {
        // El caso raro, y el que mas molesta si se ignora: un contrato retirado
        // del origen que sigue en la copia. No desincroniza el contenido, pero
        // el laboratorio lo seguiria cargando.
        escribir(origen.getChildFile("a.json"), a);
        escribir(copia.getChildFile("a.json"), a);
        escribir(copia.getChildFile("retirado.json"), b);

        const auto d = compararCatalogos(origen, copia);
        REQUIRE(d.sobran.size() == 1);
        CHECK(d.sobran[0] == "retirado.json");
        CHECK(d.distintos.empty());
    }

    SECTION("una copia VACIA no se confunde con una copia al dia")
    {
        // Si `jsonDe` devolviera vacio por cualquier motivo —un directorio mal
        // resuelto, un filtro que no casa—, "todo igual" y "no hay nada" darian
        // el mismo resultado. Se comprueba que el recuento de comparados es el
        // que hace que esos dos casos no se confundan.
        escribir(origen.getChildFile("a.json"), a);

        const auto d = compararCatalogos(origen, copia);
        CHECK(d.comparados == 0);
        REQUIRE(d.faltan.size() == 1);
        CHECK(d.faltan[0] == "a.json");
    }

    origen.deleteRecursively();
    copia.deleteRecursively();
}
