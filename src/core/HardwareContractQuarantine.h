/**
 * @file HardwareContractQuarantine.h
 * @brief The quarantine rule, in one place, for every C++ door that filters contracts.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_core/juce_core.h>
#include <nlohmann/json.hpp>

// La mitad generada de la regla. Se escribe desde el enum del esquema con
// `ABDSharedAssets/scripts/generar-cuarentena-cpp.mjs` y no se edita: lo que este
// header hace es reexponerlo con los nombres que el resto del codigo ya usa, y
// asi quien llame a la regla no necesita saber que hay dos mitades.
#include "HardwareContractQuarantine.generado.h"

namespace abdaudiolab::core::quarantine
{

//==============================================================================
// LA REGLA, Y POR QUE ESTA SUELTA DE UN FICHERO PROPIO.
//
// Durante un tiempo la regla estuvo repartida en dos sitios: el registro
// filtraba al cargar el directorio y el adapter la comprobaba al montar su cache.
// Los dos acababan en la MISMA funcion, pero esa funcion vivía dentro de
// `HardwareContractRegistry`, y vivir dentro del registro significaba dos cosas
// malas a la vez.
//
// La primera es de lectura: quien quisiera razonar sobre la regla —para
// auditarla, para escribir un test, para pedirla en una revision— tenía que
// abrir el registro entero, que es un fichero que hace de todo. Y la segunda es
// de dependencia: la regla es una regla DEL DATO, no del registro. Que el
// registro la guarde no dice nada de quien mas la necesita.
//
// Aquí vive sola, y solo ella decide.
//
//==============================================================================
// LO QUE DICE LA REGLA.
//
// Solo el literal `valorEstado` en el campo `campoEstado` retiene. Cualquier
// otra cosa —un `status` mal escrito, un estado nuevo que nadie ha declarado—
// NO retiene.
//
// Y eso parece lo contrario de lo prudente, y es justo lo contrario. Retener es
// lo DESTRUCTIVO: esconde hardware del cajon sin que nada lo diga. Un valor mal
// escrito tiene que verse, no desaparecer. Si un dia se anade un estado nuevo, lo
// que tiene que pasar es que este ojo lo ignore y el aviso de "estado
// desconocido" lo ponga quien carga, que es donde si hay donde avisar.
//
// Y los tres nombres que separan el resto de este fichero NO estan escritos aqui, y
// eso es lo que ha cambiado de verdad.
//
// Antes eran literales en este header, atados al esquema por dos tests
// cruzados. Los tests funcionaban, y aun asi dejaban pasar lo que mas duele:
// los dos hacen SKIP cuando ABDSharedAssets no esta al lado, que es el clon
// limpio. Sin el hermano, la mitad de C++ no se puede ni comparar con la otra, y
// lo que se rompe es una regla de la que C++ es la unica parte que lee el
// hardware. Un nombre tecleado puede quedarse viejo sin que nadie lo note, y una
// errata —una `d` de mas— es exactamente el fallo que un test cruzado caza tarde.
//
// Ahora vienen de `HardwareContractQuarantine.generado.h`, que se escribe desde
// el enum del esquema. No hay dos mitades que comparar: hay un fichero
// generado, y lo unico que puede quedar desfasado es el fichero, que el
// preflight comprueba con `--check` sin necesitar al hermano. Una errata ya no
// tiene donde colarse, porque en este repositorio no hay ningun nombre tecleado.

/** El campo del contrato que lleva la marca. Del esquema, no escrito aqui. */
inline constexpr auto campoEstado = generado::campoEstado;

/** El unico valor de `campoEstado` que retiene. Del enum del esquema. */
inline constexpr auto valorEstado = generado::valorEstado;

/** El campo del contrato que lleva el motivo. Del esquema, derivado del anterior. */
inline constexpr auto campoMotivo = generado::campoMotivo;

/**
 * Lo que se dice de un retenido que no dice por que.
 *
 * Va escrito y no se deja en blanco porque un retenido sin explicacion es
 * exactamente el fallo que la marca se inventorio para tapar: alguien pregunta
 * dentro de tres semanas por que falta un Aparato y no hay donde leerlo.
 */
inline constexpr const char* motivoPorDefecto = "sin statusReason en el contrato";

/**
 * Que decide la regla sobre un contrato.
 *
 * Motivo incluido, y no un `bool` a secas: el motivo es lo unico que llega al
 * log, al callback y al cajon, y sin el un retenido es hardware que no esta.
 */
struct Resultado
{
    bool retenido { false };
    juce::String motivo;
};

/**
 * Un contrato retenido, con lo necesario para quitarle la retencion.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * POR QUE ESTO ES UN STRUCT Y NO UN `std::pair`.
 *
 * Porque hace falta el FICHERO, y el fichero no cabe en un par. Un par de
 * nombre y motivo no sabe donde esta el JSON, y sin el path no se puede ni
 * abrirlo ni decirselo a nadie. Y como no se puede, la respuesta a "levanta la
 * retencion" se queda en "edita el fichero", sin decir cual: que es un viaje de
 * cazar el `.json` a mano por un arbol de repositorios.
 *
 * Y un par al que se le anade el path por otro lado son DOS containers que se
 * pueden desincronizar solos, con un retenido en el primero y ninguno en el
 * segundo. Un struct con los tres campos juntos hace que eso no tenga forma
 * de ocurrir.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * POR QUE EL NOMBRE ES EL DEL FICHERO Y NO EL `id`.
 *
 * Porque el registro mira la marca ANTES de parsear, y en ese momento el `id` no
 * existe todavia: se leeria el JSON entero para sacar un campo que todavia no
 * se sabe cual es. De ahi el nombre sin extension.
 *
 * El `SharedHardwareContractAdapter` presenta el mismo dato con la clave que si
 * tiene —el `id`, que su registro compartido ya le ha dado parseado— y eso no
 * es una inconsistencia: es cada puerta nombrando lo que de verdad tiene. Lo que
 * no puede pasar es que las dos digan cosas distintas, y por eso las dos
 * llaman a `evaluar` y no a su propia copia de la regla.
 */
struct Retenido
{
    /** El JSON de origen. Es lo que hay que editar para levantar la retencion. */
    juce::File fichero;

    /** El nombre del fichero sin extension. Es lo que ve la persona. */
    juce::String nombre;

    /** El `statusReason`, o el texto por defecto si el contrato no lo traia. */
    juce::String motivo;
};

//==============================================================================
// COMO SE DICE UNA RETENCION.
//
// Las tres palabras de este bloque --"decision editorial", "no fallo" y
// `statusReason`-- son deliberadas, y no son redaccion. Son las que alguien va
// a buscar dentro de tres semanas para entender por que falta un Aparato, y
// mientras no las encuentre en el log tendra que suponer que el programa fallo,
// que es la suposicion que lleva a perder una tarde buscando un JSON roto que
// no existe.
//
// Y estan aqui, y no en el registro ni en el adapter, porque las dos puertas
// tienen que decirlo IGUAL. Si una dice "retenido" y la otra dice "omitido",
// quien lea los dos logs creera que son dos problemas distintos, y son el
// mismo.
//
// Y el prefijo va aparte, y con corchetes, por una razon de los logs de verdad:
// un grep por "[CUARENTENA]" devuelve exactamente las decisiones editoriales y
// nada mas. Sin el, las lineas de la cuarentena se mezclan con las del error que
// motivo la carga, y un log de arranque deja de distingir "esto se decidio" de
// "esto se rompio".
//
// El motivo va sin recortar y con su campo de origen delante, porque un motivo
// suelto pierde de donde sale: "de 31 bloques solo 7 casan" no dice que leerlo
// obliga a abrir el JSON.
//==============================================================================

/** El prefijo de las lineas de log de la cuarentena. Distinguible de un fallo a ojo. */
inline constexpr const char* prefijoLog = "[CUARENTENA]";

/**
 * @brief La frase de una retencion, sin el prefijo de log.
 *
 * La misma para el registro y para el adapter, y la reutiliza tambien el panel
 * de avisos del arranque, para que no sea una tercera redaccion.
 */
[[nodiscard]] juce::String descripcionDeRetencion(const juce::String& nombre,
                                                  const juce::String& motivo);

/** Lo mismo, sobre un `Retenido`, que ademas sabe donde esta el JSON a editar. */
[[nodiscard]] juce::String descripcionDeRetenido(const Retenido& retenido);

/** Lo mismo, con el prefijo. Es la linea que se escribe en el log. */
[[nodiscard]] juce::String lineaDeLogDeRetencion(const Retenido& retenido);

//==============================================================================
// LAS DOS PUERTAS DE LECTURA.
//
// La de JSON, para quien ya tiene el contrato parseado —el registro, el
// adapter, cualquier test—.
//
// Y la de fichero, que existe por un motivo de ARRANQUE, no de comodidad: el
// registro se llama con cada JSON del catalogo, y muchos no son contratos sino
// recetas, matrices y patches. Leerlos enteros para mirar dos claves seria tirar
// el doble de trabajo en el arranque, que es justo cuando se nota. Esta puerta
// lee solo `status` y `statusReason`.
//
//==============================================================================

/**
 * @brief Aplica la regla al contrato ya parseado.
 *
 * Un `contrato` que no es un objeto no retiene. Un fichero ilegible tampoco, y
 * eso parece una incoherencia hasta que se lee el motivo: retener un contrato
 * ilegible seria esconder un problema de permisos o de JSON truncado detras de
 * una decision editorial, que es la peor forma de perder un error.
 */
[[nodiscard]] Resultado evaluar(const nlohmann::json& contrato);

/**
 * @brief Aplica la regla a un contrato en disco, leyendo solo lo necesario.
 *
 * @see evaluar(const nlohmann::json&), que es la MISMA regla.
 */
[[nodiscard]] Resultado evaluar(const juce::File& contrato);

} // namespace abdaudiolab::core::quarantine
