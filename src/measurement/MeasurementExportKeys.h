/**
 * @file MeasurementExportKeys.h
 * @brief The JSON keys this laboratory writes when exporting a measurement, from one place.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <cstddef>
#include <string_view>

// La mitad generada. Se escribe desde ABDSharedAssets/scripts/claves-export-medicion.json
// con `node scripts/generar-claves-export-cpp.mjs` y no se edita: lo que este header
// hace es reexponerlo con los nombres que el resto del codigo ya usa, y poner la
// puerta que hace que un renombrado no pase en silencio.
#include "MeasurementExportKeys.generado.h"

namespace abdaudiolab::measurement::claves
{

//==============================================================================
// POR QUE ESTO EXISTE.
//
// Porque las claves estaban tecleadas en `MeasurementSerialization.cpp` y en
// `MeasurementContainerExporter.cpp`, y en NINGUN contrato del repositorio. Un
// renombrado en el lado que lee dejaba al laboratorio escribiendo la clave vieja,
// y el fallo es SILENCIOSO por construccion: el JSON salia bien formado, el panel
// no encontraba lo que buscaba, y no habia ni rojo ni crash.
//
// No es un riesgo hipotetico. Con `minVal`/`maxVal`/`defaultVal` ya ocurrio: el
// esquema no los declaraba, dos parsers de C++ seguian leyendo con el nombre
// corto, y 68 controles del catalogo cargaron con el 0.0/1.0/0.5 del parser en
// vez de con su valor de fabrica. Un knob que va a 0.65 porque si, y ninguna
// prueba se entere.
//
// Y por eso las claves estan en un CONTRATO y no en un header: un header se
// puede editar sin que nadie mire, y un contrato se revisa. El
// `scripts/generar-claves-export-cpp.mjs` de ABDSharedAssets lee el contrato y
// escribe la cabecera generada, y el preflight corre su `--check`. Un renombrado
// en el contrato pone el preflight en rojo ANTES de que exista una compilacion
// que pueda mentir.
//
// Lo que queda aqui es lo que el generador no puede decidir por si solo: un
// inventario que este codigo sabe atender, y la puerta que obliga a que coincida
// con el catalogo.
//==============================================================================

/**
 * QUE SABE ATENDER ESTE CODIGO.
 *
 * El generador declara las claves del CONTRATO, que es mas de lo que el
 * laboratorio escribe: el indice tambien trae las de `modulationSideband`, que
 * se escriben desde dentro del grupo de modulacion, y las de los subobjetos de
 * umbral, latencia, frecuencia, profundidad y forma de onda.
 *
 * Aqui se declara CUALES ha aprendido a escribir este repositorio, y los
 * `static_assert` de abajo dicen que la lista y el catalogo cuentan lo mismo.
 *
 * Lo que se busca no es que no cambien: van a cambiar, y tienen que poder. Lo que
 * se busca es que cambien pasando por aqui. Una clave nueva en el contrato que
 * nadie escribe no es un olvido, es una puerta que alguien decidio abrir y no lo
 * dijo; una clave que se escribe y no esta en el contrato es peor, porque no hay
 * ni contrato que la respalde.
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * POR QUE ES UN ARRAY Y NO VARIABLES SUELTAS
 *
 * Porque el invariante de abajo compara dos NUMEROS, y con variables sueltas no
 * hay forma de obtener el segundo: habria que escribir las claves otra vez en una
 * lista aparte, y esa segunda lista es un segundo sitio donde poner una clave,
 * que es justo lo que este header vino a eliminar. Con un array, el recuento sale
 * del propio array y la lista de comprobacion es la MISMA.
 *
 * Con una excepcion a proposito: las claves internas de `rateHz`, `depth` y
 * `waveform` se listan con su nombre JSON —`name`, `value`, `unit`, `status`,
 * `reason`— y salen repetidas. Repetidas y no prefijadas porque en el JSON son
 * esas, y lo que se comprueba es que ese nombre exista en el indice. El prefijo
 * lo pone el generador en el C++, que es donde hace falta para que no se pisen.
 * ─────────────────────────────────────────────────────────────────────────────
 */
namespace attendidas
{
    /**
     * El inventario: una entrada por clave que este codigo sabe escribir.
     *
     * Las repetidas cuentan como repetidas, y eso es lo que hace que el invariante
     * del recuento tenga sentido: el indice del generador tambien las cuenta una
     * vez por subobjeto que las declara.
     */
    inline constexpr std::string_view lista[] = {
        // El grupo de calibracion.
        "analogCalibration",
        "calibrationId", "sampleRateHz", "blockSize", "roundTripLatencySamples",
        "snrDb", "peakDbfs", "dcOffsetDb", "status",

        // Sus umbrales. Son un JUICIO y no una medida, pero se escriben aqui y por
        // eso son claves que este codigo tiene que saber nombrar.
        "thresholds",
        "snrDbMin", "peakDbfsMax", "dcOffsetDbMax",

        // Y el reparto de la latencia, que decide donde se puede corregir.
        "latencyBreakdown",
        "estimatedHostLatencySamples", "estimatedHardwareLatencySamples",

        // El grupo de bandas laterales. Se escriben desde dentro del grupo de
        // modulacion, asi que el codigo de este repositorio no las nombra
        // directamente, pero las escribe y por eso estan en el inventario.
        "sidebands",
        "carrierFrequencyHz", "sidebandFrequencyHz", "order", "levelRelativeToCarrierDb",

        // El grupo de modulacion.
        "modulationResult",
        "targetDestination", "rateHz", "rateMethod", "depth", "waveform",
        "sidebands", "spectralMetadata", "timeCurve", "spectrumCurve",

        // Las claves internas de los tres subobjetos. Sale repetidas porque en el
        // JSON son las mismas, y porque el indice las cuenta una vez por subobjeto.
        "name", "value", "unit", "status", "reason",   // de rateHz
        "name", "value", "unit", "status", "reason",   // de depth
        "waveform", "status", "confidence",            // del subobjeto waveform
    };
}

//==============================================================================
// LOS INVARIANTES.
//
// Y POR QUE ESTAN EN UN `static_assert` Y NO EN UN TEST.
//
// Un test corre en el CI y se puede saltarse; un `static_assert` corre en cada
// compilacion que alguien haga, incluido el que no pasa por ningun CI. Para un
// fallo que no da crash, esa es la diferencia entre una puerta y una costumbre.
//
// Y hay una razon mas, que es la que hizo falta para escribir este header: la
// version anterior de este invariante era una TAUTOLOGIA. Decia
// `numeroAtendidas == generado::numeroClaves` con `numeroAtendidas` DEFINIDO como
// `generado::numeroClaves`, o sea `X == X`, y salia verde con cualquier cosa. Un
// test de JavaScript no lo puede ver, porque mira el TEXTO del header y no su
// valor; solo el compilador lo evalua. Este header se reescribio por eso.
//
// Compara CONTENIDO, no solo longitudes: dos indices del mismo tamaño pueden ser
// nombres distintos, que es justo el renombrado que hay que cazar.
//==============================================================================
namespace detalle
{
    /**
     * Cuantas claves afirma atender este codigo.
     *
     * Sale del array y NO del catalogo, y esa es toda la diferencia: si saliera de
     * `generado::numeroClaves`, el invariante compararia un numero consigo mismo.
     */
    inline constexpr std::size_t numeroAtendidas =
        sizeof(attendidas::lista) / sizeof(attendidas::lista[0]);

    /** Dice si `a` esta en el indice del generador. */
    [[nodiscard]] constexpr bool estaDeclarada(std::string_view a) noexcept
    {
        for (auto clave : generado::todasLasClaves)
            if (clave == a)
                return true;
        return false;
    }

    /**
     * Dice si TODAS las claves del inventario estan en el indice del generador.
     *
     * Va despues de `estaDeclarada` y no antes: en una funcion `constexpr` el
     * nombre tiene que estar declarado antes de usarse, y poner esta comprobacion
     * primero daba un "no se encontro el identificador" que no llegaba a decir nada
     * del problema de verdad.
     */
    [[nodiscard]] constexpr bool todasAtendidasEstanDeclaradas() noexcept
    {
        for (auto clave : attendidas::lista)
            if (!estaDeclarada(clave))
                return false;
        return true;
    }
}

/**
 * El inventario y el catalogo cuentan lo mismo.
 *
 * Compara el numero de entradas del INVENTARIO con el numero de entradas del
 * INDICE, y los dos salen de sitios distintos: el primero de este fichero, el
 * segundo del generador.
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * POR QUE NO COMPARA CON `numeroClaves`
 *
 * Porque `numeroClaves` es el numero de ENTRADAS y aqui hay dos claves que salen
 * dos veces: `sidebands` la declara el grupo de modulacion y lo declara tambien su
 * contenedor. El indice las cuenta las dos; el catalogo las declara una vez por
 * grupo. Comparar un total con el total daria verde con un desfase de una clave, y
 * comparar el total con el de unicas daria rojo siempre.
 *
 * Y el de unicas no sirve para esto: `name`, `value`, `unit`, `status` y `reason`
 * salen en tres subobjetos y son un solo nombre. El numero que compara las dos
 * listas en igualdad es el de entradas, no el de nombres.
 * ─────────────────────────────────────────────────────────────────────────────
 */
static_assert(
    detalle::numeroAtendidas == sizeof(generado::todasLasClaves) / sizeof(generado::todasLasClaves[0]),
    "el inventario de MeasurementExportKeys.h y el indice del generador no cuentan lo mismo. "
    "Si el catalogo ha ganado una clave, esta escrita aqui con su nombre en `attendidas`; si la que "
    "sobra es de aqui, se quita. Lo que no puede pasar es dejarlas a medias: una clave en el "
    "contrato que nadie escribe no avisa de nada, y una que se escribe sin contrato es peor.");

// Y que cada entrada exista DE VERDAD. Sin esta vuelta el invariante de arriba solo
// miraria el numero: dos listas del mismo tamaño con nombres distintos saldarian en
// verde, que es el renombrado entero sin que nadie se entere.
//
// Se recorre el ARRAY ENTERO y no nueve nombres escritos aqui a mano. No es que
// quede mas corto: es que una lista escrita a mano hay que actualizar a mano
// cuando el catalogo crece, y se olvida. El array es la misma lista que el
// invariante de arriba, asi que crece sola.
static_assert(detalle::todasAtendidasEstanDeclaradas(),
    "una clave del inventario no la declara el catalogo. Esta escrito un nombre aqui que no sale "
    "de ABDSharedAssets/scripts/claves-export-medicion.json, que es justo lo que este header existe "
    "para que no vuelva a pasar: una clave tecleada a mano no se renombra con el resto.");

// Y que el indice tenga tantas entradas como el generador dice. Los dos numeros los
// escribe el generador, asi que que no cuadren solo pasa si su forma cambio, y que no
// cuadren es justo lo que haria que el primer invariante mirara una lista mas corta
// de lo que cree.
static_assert(sizeof(generado::todasLasClaves) / sizeof(generado::todasLasClaves[0])
                  == generado::numeroClaves,
    "el indice de claves y el numero declarado no cuadran. Los dos los escribe el generador, asi que "
    "esto solo pasa si su forma cambio: se regenera con node scripts/generar-claves-export-cpp.mjs "
    "en ABDSharedAssets.");

} // namespace abdaudiolab::measurement::claves