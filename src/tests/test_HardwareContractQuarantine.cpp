// ==============================================================================
// ABDAudioLab - Un contrato en cuarentena no se carga, y se dice por que
// ==============================================================================
//
// QUE HACE ESTO.
//
// `roland_aira_submodules.json` se identifica COMO el Roland AIRA Modular —
// `deviceType: AUTOMATED_SYSEX`, el `midiIdentification` del AIRA, la imagen del
// modelo— y su contenido es otro catálogo: de sus 31 bloques, solo 7 coinciden
// con los 31 módulos que declara el patch_spec del AIRA. Los otros 24 de cada
// lado no coinciden, y no es un cambio de nombres: el fichero trae un fuzz de
// germanio y un chorus de ensemble, y el AIRA Modular no tiene ninguno de los
// dos.
//
// Sin esto, el cajón de hardware enseñaba una tarjeta con el logo de Roland y
// 31 pruebas que no son las del Aparato. El usuario las lanzaba creyendo que
// medía el AIRA. Eso no es un dato feo: es un dato que miente.
//
// ----------------------------------------------------------------------------
// POR QUE LA MARCA ESTA EN EL CONTRATO Y NO EN UNA LISTA DE C++.
//
// Porque el registro de C++ no puede leer el helper de tests de ABDSharedAssets,
// y una lista propia aqui seria una segunda verdad que se desincroniza sola y
// sin ruido. La marca viaja con el dato: `"status": "quarantined"` y su
// `statusReason`, declarados en `hardware_profile.schema.json`. Quien copie el
// contrato a cualquier sitio —el snapshot versionado lo hace— se lleva la
// decisión y el motivo.
//
// Y el otro lado, en JS, `CUARENTENAS` mantiene la misma marca, con un test que
// mide las dos direcciones: un contrato de la lista sin marca en el fichero
// (que C++ cargaría) y un contrato marcado fuera de la lista (que el test no
// vigilaría).
//
// ----------------------------------------------------------------------------
// QUE NO HACE ESTE TEST, Y POR QUE.
//
// No comprueba que el catálogo entero esté bien. Comprueba UNA cosa: que un
// contrato con la marca no aparece, y que sin ella sí. Lo segundo es lo que hace
// que el primero signifique algo —si nada se cargara, "no aparece" no probaría
// nada— y por eso el caso negativo va con su propio contrato, escrito en un
// directorio temporal, y no con una excepcion al vuelo sobre el catálogo real.
//
// =============================================================================

#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>

#include "core/LabResourcePaths.h"
#include "core/HardwareContractRegistry.h"
#include "core/SharedHardwareContractAdapter.h"

#include <algorithm>
#include <fstream>
#include <string>

using namespace abdaudiolab::core;

namespace
{

/** Escribe un contrato minimo pero valido en un directorio temporal. */
juce::File escribirContrato(const juce::File& dir,
                            const juce::String& nombre,
                            const juce::String& id,
                            const juce::String& extras)
{
    const std::string json = std::string(R"({
  "schemaVersion": "2.0",
  "id": ")") + id.toStdString() + R"(",
  "displayName": "Contrato de prueba",
  "description": "Escrit por el test de cuarentena.",
  "deviceType": "MANUAL_EURORACK",
  "brand": "Prueba")" + extras.toStdString() + R"(
})";

    std::ofstream out((dir.getChildFile(nombre).getFullPathName()).toStdString());

    if (!out.good())
        return {};

    out << json;
    return dir.getChildFile(nombre);
}

} // namespace

TEST_CASE("Un contrato marcado como en cuarentena no llega al catalogo effective",
          "[contracts][registry][quarantine]")
{
    const auto contratosDir = abdaudiolab::core::contractsHardwareDir();
    REQUIRE(contratosDir.isDirectory());

    HardwareContractRegistry registry;
    REQUIRE(registry.loadContractsFromDirectory(contratosDir));
    REQUIRE(registry.hasContracts());

    // 1. El contrato retenido NO esta, ni por id ni por ninguna otra via.
    //
    // Se comprueba el `findContractById` y no solo el `getContracts().size()`,
    // porque un recuento que baja puede bajar por mil cosas —un JSON que dejo de
    // parsear, un `displayName` vacio— y aqui lo que importa es que ESTE
    // contrato no esta. Un recuento solo no distinguiria "lo retenimos" de "se
    // rompio otra vez".
    CHECK(registry.findContractById("roland_aira_submodules") == nullptr);

    // 2. Y el motivo es publicly legible, no un retenido mudo.
    //
    // Esto es lo que evita la pregunta de dentro de tres semanas. Un contrato
    // que desaparece sin dejar razon es indistinguible de un bug, y un bug que
    // se nota es mejor que una decision editorial que no se nota.
    const auto& retenidos = registry.getQuarantinedProfiles();
    bool encontrado = false;

    for (const auto& retenido : retenidos)
    {
        if (retenido.nombre != "roland_aira_submodules")
            continue;

        encontrado = true;
        INFO("motivo de la cuarentena: " << retenido.motivo.toStdString());
        CHECK_FALSE(retenido.motivo.isEmpty());

        // La medida va primero, a proposito: un motivo que solo dice "esta
        // dudoso" obliga a volver a medir para poder usarlo. Y va como variable
        // y no como `CHECK(expr, mensaje)` porque el macro no descompone una
        // expresion de juce::String, y el mensaje se pierde.
        const bool motivoTraeLaMedida = retenido.motivo.contains("7");
        CHECK(motivoTraeLaMedida);

        // Y el FICHERO. Este es el campo que hace que el boton "Abrir el
        // JSON para editar" de la ficha tenga algo que abrir: un retenido
        // con nombre y motivo pero sin ruta obliga a cazarlo a mano por el
        // arbol de repositorios, que es justo el viaje que la ficha evita.
        INFO("el retenido no trae el fichero de origen, asi que el boton de abrir no tendria nada que abrir");
        CHECK(retenido.fichero.existsAsFile());
    }

    CHECK(encontrado);

    // 3. Y el aviso sale por el canal de los demas, que es el unico que alguien
    //    mira. Un retenido que no avisa por ninguna parte es un retenido que
    //    parece un bug la proxima vez que se reinstale.
    bool avisado = false;

    for (const auto& aviso : registry.getWarnings())
        if (aviso.contains("roland_aira_submodules") && aviso.contains("cuarentena"))
            avisado = true;

    CHECK(avisado);
}

TEST_CASE("Un contrato SIN la marca se carga, y con la marca no: el caso negativo",
          "[contracts][registry][quarantine]")
{
    // Sin esta seccion, el test de arriba pasaria igual si el registro no
    // cargase NADA. Se escribe el mismo contrato dos veces, igual en todo salvo
    // en la marca, y se exige que la marca lo cambie todo. Un guard que depende
    // de que "algo" no pase no protege de "nada" pasandose.
    juce::File dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("abdlab_quarantine_contraste");

    dir.deleteRecursively();
    REQUIRE(dir.createDirectory());

    // Sin marca: se carga.
    {
        juce::File limpio;
        REQUIRE(escribirContrato(dir, "limpio.json", "quarantine_limpio",
                                 juce::String()).existsAsFile());

        HardwareContractRegistry registry;
        REQUIRE(registry.loadContractsFromDirectory(dir));
        REQUIRE(registry.getContracts().size() == 1);
        CHECK(registry.findContractById("quarantine_limpio") != nullptr);
        CHECK(registry.getQuarantinedProfiles().empty());
    }

    dir.deleteRecursively();
    REQUIRE(dir.createDirectory());

    // Con la marca: no se carga, y se dice por que.
    {
        juce::String extras = ",\n  \"status\": \"quarantined\",\n"
                              "  \"statusReason\": \"su contenido no es el de su cabecera\"";

        REQUIRE(escribirContrato(dir, "marcado.json", "quarantine_marcado", extras).existsAsFile());

        HardwareContractRegistry registry;
        REQUIRE(registry.loadContractsFromDirectory(dir));
        CHECK(registry.getContracts().size() == 0);
        CHECK(registry.findContractById("quarantine_marcado") == nullptr);
        REQUIRE(registry.getQuarantinedProfiles().size() == 1);

        // La clave es el NOMBRE DEL FICHERO sin extension, no el `id` del
        // contrato, y no es un descuido: la marca se mira ANTES de parsear —
        // un contrato retenido no debe llegar a construir sus funciones — asi que
        // en ese momento el id todavia no existe. Es la misma convencion que
        // `invalidLegacyProfiles`, que tambien guarda el nombre del fichero, y
        // por eso los dos conjuntos se pueden leer igual.
        //
        // En el catalogo real coinciden con el id, asi que la distincion no se ve
        // ahi. Aqui se ve, que es justo por lo que el caso va con un fichero cuyo
        // nombre y cuyo id NO son lo mismo.
        const auto& retenido = registry.getQuarantinedProfiles()[0];
        CHECK(retenido.nombre == "marcado");
        const bool motivoDiceLaCausa = retenido.motivo.contains("cabecera");
        CHECK(motivoDiceLaCausa);

        // Y el fichero es el que se escribio, con su nombre entero. Sin
        // esto el boton de la ficha abriria el directorio en vez del JSON.
        CHECK(retenido.fichero.existsAsFile());
        CHECK(retenido.fichero.getFileName() == juce::String("marcado.json"));
    }

    dir.deleteRecursively();
}

TEST_CASE("Un `status` desconocido NO retiene, pero avisa: retener es lo que hace dano",
          "[contracts][registry][quarantine]")
{
    // La regla es "retiene SOLO el literal `quarantined`", y al reves de lo que
    // parece. Retener esconde hardware, asi que un valor mal escrito tiene que
    // verse, no desaparecer. Y hay que probarlo en las dos direcciones, porque un
    // guard que solo sabe decir "no" acaba reteniendo cualquier cosa.
    juce::File dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("abdlab_quarantine_desconocido");

    dir.deleteRecursively();
    REQUIRE(dir.createDirectory());

    juce::String extras = ",\n  \"status\": \"quarantinead\",\n"
                          "  \"statusReason\": \"una tilde de menos\"";

    REQUIRE(escribirContrato(dir, "typo.json", "quarantine_typo", extras).existsAsFile());

    HardwareContractRegistry registry;
    REQUIRE(registry.loadContractsFromDirectory(dir));

    // Se carga: un valor desconocido no es una decision editorial.
    CHECK(registry.getContracts().size() == 1);
    CHECK(registry.findContractById("quarantine_typo") != nullptr);
    CHECK(registry.getQuarantinedProfiles().empty());

    // Pero no se carga en silencio. El estado viaja con el contrato, asi que se
    // puede mirar despues sin haber estado en el log.
    CHECK(registry.getContracts()[0].status == "quarantinead");

    bool avisado = false;

    for (const auto& aviso : registry.getWarnings())
        if (aviso.contains("quarantinead") && aviso.contains("no conoce"))
            avisado = true;

    CHECK(avisado);

    dir.deleteRecursively();
}

TEST_CASE("Un JSON ilegible o sin `status` no se retiene, se reporta como roto",
          "[contracts][registry][quarantine]")
{
    // Retener un contrato ilegible seria esconder un problema de permisos o un
    // JSON truncado detras de una decision editorial, que es la peor forma de
    // perder un error. La cuarentena es para lo que se sabe dudoso; lo que esta
    // roto se dice roto.
    juce::File dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("abdlab_quarantine_ilegible");

    dir.deleteRecursively();
    REQUIRE(dir.createDirectory());

    {
        std::ofstream out((dir.getChildFile("roto.json").getFullPathName()).toStdString());
        REQUIRE(out.good());
        out << "{ esto no es json";
    }

    // Y un fichero que no existe, para el caso de la carrera con el borrado.
    const auto ausente = quarantine::evaluar(dir.getChildFile("no_existe.json"));
    CHECK_FALSE(ausente.retenido);
    CHECK(ausente.motivo.isEmpty());

    HardwareContractRegistry registry;

    // Con un solo JSON roto no queda nada que cargar, asi que la carga falla. Y
    // lo que se mira es que NO se haya retenido: retenido y roto son dos
    // Diagnosticos distintos, y confundirlos hace que alguien "arregle" una
    // decision editorial reescribiendo el contrato.
    CHECK_FALSE(registry.loadContractsFromDirectory(dir));
    CHECK(registry.getQuarantinedProfiles().empty());
    CHECK(registry.getContracts().empty());
    CHECK(registry.getWarnings().size() == 1);
    CHECK(registry.getWarnings()[0].contains("roto"));

    dir.deleteRecursively();
}


// ----------------------------------------------------------------------------
// LA MISMA CUARENTENA, POR LA SEGUNDA PUERTA.
//
// El laboratorio tiene DOS caminos al catalogo de contratos: el registro local,
// que filtra, y el `SharedHardwareContractAdapter`, que carga por el registro
// compartido de ABDSharedCode. Ese registro es de otro repositorio y no conoce
// la marca: retenia el contrato y lo servia entero. Con el filtro en una sola de
// las dos puertas, el contrato en cuarentena llegaba igual por la otra, y todo lo
// que se decidio sobre el AIRA Modular seguia en pie para el adapter.
//
// Asi que el filtro se repite aqui, con la misma funcion y por el mismo motivo:
// la regla se llama, no se reescribe.
// ----------------------------------------------------------------------------

TEST_CASE("El adapter tampoco deja pasar un contrato en cuarentena",
          "[contracts][adapter][quarantine]")
{
    // El catalogo REAL, no uno de prueba: lo que se vigila aqui es que el
    // contrato que el registro local ya retenia no aparezca por la otra puerta.
    abd::hwid::HardwareContractRegistry shared;
    SharedHardwareContractAdapter adapter(shared);

    const auto contratosDir = abdaudiolab::core::contractsHardwareDir();
    REQUIRE(contratosDir.isDirectory());
    REQUIRE(adapter.loadFromShared(contratosDir));
    REQUIRE(adapter.hasContracts());

    CHECK(adapter.findContractById("roland_aira_submodules") == nullptr);

    // Y el motivo se puede leer. Un retenido mudo por esta puerta seria igual de
    // indistinguible de un bug que por la otra.
    bool encontrado = false;

    for (const auto& [id, motivo] : adapter.getQuarantinedProfiles())
    {
        if (id != "roland_aira_submodules")
            continue;

        encontrado = true;
        INFO("motivo de la cuarentena (adapter): " << motivo.toStdString());
        CHECK_FALSE(motivo.isEmpty());
        const bool motivoTraeLaMedida = motivo.contains("7");
        CHECK(motivoTraeLaMedida);
    }

    CHECK(encontrado);

    // NOTA SOBRE LA CLAVE, POR QUE AQUI SI ES EL `id`.
    //
    // El registro local guarda el NOMBRE DEL FICHERO sin extension, porque mira
    // la marca antes de parsear y en ese momento el id todavia no existe. El
    // adapter ya tiene los contratos parseados —los trae el registro compartido—,
    // asi que la clave natural es el id, y es lo que se comprueba arriba. No es
    // una inconsistencia: es cada puerta nombrando lo que de verdad tiene.
    // Lo que NO puede pasar es que las dos digan cosas distintas, y por eso las
    // dos llaman a la misma `quarantine::evaluar`.
}

TEST_CASE("El adapter carga el contrato SIN marca y retiene el marcado: el caso negativo",
          "[contracts][adapter][quarantine]")
{
    // Sin este caso, el de arriba pasaria igual si el adapter no cargase nada.
    // Se escriben dos contratos iguales en todo salvo en la marca, y se exige que
    // la marca cambie el resultado: ese es el unico modo de que "no aparece"
    // signifique "lo reteni" y no "no megele".
    juce::File dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("abdlab_adapter_cuarentena");

    dir.deleteRecursively();
    REQUIRE(dir.createDirectory());

    // 1. Sin marca: llega.
    {
        REQUIRE(escribirContrato(dir, "limpio.json", "adapter_limpio",
                                 juce::String()).existsAsFile());

        abd::hwid::HardwareContractRegistry shared;
        SharedHardwareContractAdapter adapter(shared);

        REQUIRE(adapter.loadFromShared(dir));
        REQUIRE(adapter.getContracts().size() == 1);
        CHECK(adapter.findContractById("adapter_limpio") != nullptr);
        CHECK(adapter.getQuarantinedProfiles().empty());
    }

    dir.deleteRecursively();
    REQUIRE(dir.createDirectory());

    // 2. Con la marca: no llega, y se dice por que.
    {
        juce::String extras = ",\n  \"status\": \"quarantined\",\n"
                              "  \"statusReason\": \"su contenido no es el de su cabecera\"";

        REQUIRE(escribirContrato(dir, "marcado.json", "adapter_marcado", extras).existsAsFile());

        abd::hwid::HardwareContractRegistry shared;
        SharedHardwareContractAdapter adapter(shared);

        // Cargar sale bien: no hay error que arreglar, hay una decision editorial.
        // Un retenido que se tratase como fallo haria que alguien regenerase un
        // contrato que ya esta bien.
        REQUIRE(adapter.loadFromShared(dir));
        CHECK(adapter.getContracts().size() == 0);
        CHECK(adapter.findContractById("adapter_marcado") == nullptr);

        // Y no se calla: "no hay contratos" a secas parece un fallo de carga.
        CHECK(adapter.getLastError().find("cuarentena") != std::string::npos);

        REQUIRE(adapter.getQuarantinedProfiles().size() == 1);
        CHECK(adapter.getQuarantinedProfiles()[0].first == "adapter_marcado");
        const bool motivoDiceLaCausa =
            adapter.getQuarantinedProfiles()[0].second.contains("cabecera");
        CHECK(motivoDiceLaCausa);
    }

    dir.deleteRecursively();
}
// ----------------------------------------------------------------------------
// EL LITERAL DE C++ Y EL ENUM DEL ESQUEMA TIENEN QUE SEGUIR SIENDO LO MISMO.
//
// Este es el test que cierra el agujero que quedaba despues de mover la regla a
// `HardwareContractQuarantine`, y es el que mas vale de todo este fichero.
//
// `quarantine::evaluar` de aqui y el helper de JS de ABDSharedAssets nombran el
// mismo campo y el mismo valor. Antes eso se ataba con ESTE caso: comparaba el
// literal de C++ contra el enum del esquema. Y no era suficiente, por dos
// razones que hacen falta las dos para entender lo de ahora.
//
// La primera es que este caso hace SKIP sin `ABDSharedAssets` al lado, que es el
// clon limpio y buena parte del CI. Sin el hermano, la mitad de C++ no se puede ni
// comparar, y lo que se rompe es una regla de la que C++ es la unica parte que
// lee el hardware. Un guard que se apaga solo es un guard apagado.
//
// La segunda es que comparar dos literales escritos a mano caza un cambio de
// NOMBRE, pero no una errata hasta que alguien ejecuta el test. Y una errata —una
// `d` de mas en `quarantined`— es el fallo que mas cara sale de todos: `evaluar`
// devuelve "no retenido" para siempre y el registro carga el contrato dudoso sin
// que se ponga rojo nada.
//
// POR QUE ESTE CASO SIGUE EXISTIENDO, Y QUE COMPRUEBA AHORA.
//
// Los literales ya no estan escritos en `HardwareContractQuarantine.h`. Salen de
// `HardwareContractQuarantine.generado.h`, que se escribe desde el enum del
// esquema, y el preflight lo comprueba con `--check` sin necesitar al hermano:
// esa es la puerta principal, y no la da este fichero.
//
// Este caso es la segunda boca, y aporta una cosa que el `--check` no: que el
// literal NO este escrito en el header. Si alguien lo pone —porque parece mas
// legible, porque ha tocado el header y lo ha hecho «claro»—, vuelve a haber dos
// mitades, y el fichero generado pasa a ser una de ellas en vez de la unica.
// Eso se comprueba leyendo el header, y no necesita ni esquema ni hermano.
//
// Y el SKIP sigue siendo honesto por lo que queda: la parte que compara contra
// el enum necesita el esquema, y sin el no hay contra que comparar. Se dice con
// SKIP, que en Catch2 NO cuenta como pasada, y no con un verde.
TEST_CASE("El literal de C++ sale del enum del esquema, y no esta escrito en el header",
          "[contracts][quarantine][schema]")
{
    // Lo primero, y lo que no necesita nada de fuera: el literal NO esta escrito
    // en el header. Es la mitad de la regla que se puede comprobar siempre, en
    // cualquier clon y sin el repositorio hermano, y es la que impide volver a
    // tener dos mitades.
    //
    // Sin esto, alguien podria "mejorar" el header poniendo el literal a mano
    // —se lee mejor— y volver exactamente al estado en el que un cambio de
    // nombre en el esquema dejaba a C++ mirando un valor que ya no existe.

    // Se piden por su ruta y no desde una raiz, porque `repoResource` rechaza la
    // ruta vacia: la raiz no es un recurso, es el sitio donde estan los recursos,
    // y empezar por "" seria empezar justo por lo que el helper dice que no vale.
    const auto header =
        abdaudiolab::core::repoResource("src/core/HardwareContractQuarantine.h");
    const auto generado =
        abdaudiolab::core::repoResource("src/core/HardwareContractQuarantine.generado.h");

    REQUIRE(header.existsAsFile());
    REQUIRE(generado.existsAsFile());

    const auto textoHeader = header.loadFileAsString();
    const auto textoGenerado = generado.loadFileAsString();

    INFO("el header tiene el literal a mano. El generado existe, pero entonces hay "
         "dos mitades y volver a necesitar que alguien se acuerde de compararlas.");
    for (const auto* literal : { quarantine::campoEstado, quarantine::valorEstado, quarantine::campoMotivo })
    {
        const auto conComillas = juce::String("\"") + juce::String(literal) + "\"";
        CHECK_FALSE(textoHeader.contains(conComillas));
    }

    // Y la otra mitad: lo que C++ mira sale del fichero generado, y el nombre de
    // ese fichero esta en el include. Sin el include, el alias compilaria solo
    // si el generado se incluyera por otra via, que es justo lo que no queremos.
    INFO("el header no incluye el fichero generado, asi que de donde salen los "
         "literales no esta escrito en ninguna parte.");
    CHECK(textoHeader.contains("HardwareContractQuarantine.generado.h"));

    // Y que lo generado este al dia con lo que el esquema declara. Esto ultimo
    // es lo que hacia este caso antes, y sigue haciendo falta: el `--check` del
    // preflight es la puerta, y esta es la que se ejecuta sin el repositorio
    // hermano y sin ejecutar el preflight.

    const auto esquemaFichero =
        abdaudiolab::core::sharedAssetsDir().getChildFile("contracts")
            .getChildFile("hardware_profile.schema.json");

    if (!esquemaFichero.existsAsFile())
    {
        SKIP("ABDSharedAssets/contracts/hardware_profile.schema.json no esta "
             "disponible en esta maquina. Sin el no hay contra que comparar el "
             "literal de C++, y un verde aqui seria mentira: verifica que las dos "
             "mitades de la regla dicen lo mismo, y sin la otra mitad no lo dice.");
    }

    nlohmann::json esquema;

    try
    {
        esquema = nlohmann::json::parse(esquemaFichero.loadFileAsString().toStdString());
    }
    catch (const std::exception& e)
    {
        FAIL("el esquema no se puede parsear: " << e.what()
             << ". Si el esquema esta roto, el enum que hay que comparar no existe, "
             << "y saltar aqui dejaria la comprobacion sin hacer.");
    }

    const auto propiedades = esquema.at("properties");

    const auto campoEstado = juce::String(quarantine::campoEstado);
    const auto campoMotivo = juce::String(quarantine::campoMotivo);
    const auto valorEstado = juce::String(quarantine::valorEstado);

    INFO("el esquema no declara " + campoEstado
         + ", asi que el valor de abajo no se puede validar contra nada");
    REQUIRE(propiedades.contains(quarantine::campoEstado));

    // El enum, no el valor suelto. Lo que tiene que decir el esquema es "este
    // estado, y solo este", y un enum con dos valores ya es otra regla: el
    // segundo retenido en C++ y quizas no en JS, o al reves.
    const auto estados = propiedades.at(quarantine::campoEstado).at("enum");
    const bool elEnumLoDeclara =
        std::find(estados.begin(), estados.end(), nlohmann::json(quarantine::valorEstado))
            != estados.end();

    INFO("el enum de " + campoEstado + " no contiene " + valorEstado
         + ", que es lo que C++ mira. El registro dejaria de retener en "
           "silencio y ningun test de este lado lo notaria.");
    CHECK(elEnumLoDeclara);

    // Y el motivo. El esquema lo declara con minLength 1, o sea que un retenido sin
    // motivo no llega ni a ser un contrato valido de los que se atan a el.
    INFO("el esquema no declara " + campoMotivo
         + ", el campo del que C++ saca el motivo que se muestra en el cajon");
    REQUIRE(propiedades.contains(quarantine::campoMotivo));

    const int longitudMinima =
        propiedades.at(quarantine::campoMotivo).value("minLength", 0);
    INFO("el esquema permite un " + campoMotivo
         + " vacio, y entonces el texto que C++ inventa cuando falta es la "
           "unica vez que alguien lee el motivo de un retenido.");
    CHECK(longitudMinima > 0);

    // Y que el texto inventado no este en ningun sitio por casualidad.
    CHECK(std::string(quarantine::motivoPorDefecto).find(quarantine::valorEstado)
          == std::string::npos);
}
