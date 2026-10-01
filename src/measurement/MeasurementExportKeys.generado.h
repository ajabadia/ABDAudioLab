// ==============================================================================
// ABDAudioLab - GENERADO. NO EDITAR ESTE FICHERO A MANO.
// ==============================================================================
//
// Las claves de JSON que este laboratorio escribe al exportar una medicion.
// Las DECLARA ABDSharedAssets/scripts/claves-export-medicion.json, una por struct
// que las escribe, con su tipo y con lo que significan. Ninguno de estos nombres
// esta tecleado en ningun sitio de este repositorio: sale de ahi.
//
//   Se escribe con:  node scripts/generar-claves-export-cpp.mjs     (ABDSharedAssets)
//   Se comprueba con: node scripts/generar-claves-export-cpp.mjs --check
//
// El preflight corre el --check, y eso es lo que lo ata: no necesita al hermano
// para decidir, solo necesita el catalogo.
//
// SI ESTE FICHERO SE DESFASA, EL LABORATORIO ESTA ESCRIBIENDO UNA CLAVE QUE
// NADIE LEE. Eso no da crash ni rojo: el JSON sale bien formado y el panel no
// encuentra lo que busca. Por eso el desfasado se comprueba antes de nada.
// ==============================================================================

#pragma once

#include <cstddef>
#include <string_view>

namespace abdaudiolab::measurement::claves::generado
{

/** Cuantas claves declara el catalogo, contando los contenedores. */
inline constexpr std::size_t numeroClaves = 44;

// ==============================================================================
// analogChainCalibration
// ==============================================================================
//
// La funcion estatica `analogCalibrationToJson` de ABDAudioLab/src/measurement/MeasurementSerialization.cpp, y el bloque `analogCalibration` del manifiesto que escribe `writeManifestJson` en MeasurementContainerExporter.cpp.
//
// El manifiesto es un SEGUNDO sitio que escribe este grupo, y escribe solo cuatro claves. Se declara cual de las cuales en `vaAlManifiesto`, porque la diferencia es invisible al leer el JSON de salida —uno trae catorce campos y el otro cuatro— y es justo el tipo de asimetria que hace que un panel se quede sin datos sin que el laboratorio se entere.

namespace analogChainCalibration
{

/** La clave del objeto padre. null en el catalogo = cuelga de la raiz. */
inline constexpr std::string_view contenedor = "analogCalibration";

/** Identificador de esta calibracion. El que alguien lee dentro de tres semanas para saber que medicion es esta. */
inline constexpr std::string_view calibrationId = "calibrationId";

/** Frecuencia a la que se calibro, en Hz. Va en el manifiesto porque sin ella una latencia en muestras no se puede convertir a segundos. */
inline constexpr std::string_view sampleRateHz = "sampleRateHz";

/** Tamano de bloque con el que se calibro. El par (sampleRateHz, blockSize) es lo que hace comparables dos calibraciones. */
inline constexpr std::string_view blockSize = "blockSize";

/** Latencia de ida y vuelta en muestras. Es la cifra que se corrige al comparar ensayos, y va al manifiesto porque es la que se necesita sin abrir el fichero grande. */
inline constexpr std::string_view roundTripLatencySamples = "roundTripLatencySamples";

/** Relacion senal-ruido en dB. Si esta por debajo del umbral, el resto de las cifras de este grupo no significan nada. */
inline constexpr std::string_view snrDb = "snrDb";

/** Pico alcanzado en dBFS. Un pico en 0 significa que la captura saturo, y todo lo demas de la calibracion es ruido de un amplificador que ya no cabe. */
inline constexpr std::string_view peakDbfs = "peakDbfs";

/** Desnivel de continua en dB. Es el sintoma de un acoplamiento que las demas cifras de este grupo no Teach a ver: un ruido de fondo alto tambien las sube. */
inline constexpr std::string_view dcOffsetDb = "dcOffsetDb";

/** Como termino la calibracion: si se midio o no. La lectura usa `not_measured` cuando la clave no esta, y esa distincion es la que separa una calibracion mala de una calibracion que nadie ha hecho. */
inline constexpr std::string_view status = "status";
/** El valor que la lectura usa cuando la clave no esta. */
inline constexpr auto statusPorDefecto = "not_measured";

/** Los umbrales contra los que se juzgo esta calibracion. Van aparte de las cifras porque son un JUICIO y no una medida: pueden cambiar sin que cambie el hardware, y por eso no deben viajar mezclados con lo medido. */
namespace thresholds
{

/** La clave del subobjeto. El namespace ocupa el nombre del alias. */
inline constexpr std::string_view contenedor = "thresholds";

/** SNR minima aceptable en dB. Debajo de esto la calibracion se considera fallida. */
inline constexpr std::string_view snrDbMin = "snrDbMin";
/** El valor que la lectura usa cuando la clave no esta. */
inline constexpr auto snrDbMinPorDefecto = 18;

/** Pico maximo aceptable en dBFS. El valor por defecto es negativo porque un pico POSITIVO ya es saturacion. */
inline constexpr std::string_view peakDbfsMax = "peakDbfsMax";
/** El valor que la lectura usa cuando la clave no esta. */
inline constexpr auto peakDbfsMaxPorDefecto = -0.5;

/** Desnivel maximo aceptable en dB, tambien negativo: el signo es el que dice que se compara en magnitud y no de signo. */
inline constexpr std::string_view dcOffsetDbMax = "dcOffsetDbMax";
/** El valor que la lectura usa cuando la clave no esta. */
inline constexpr auto dcOffsetDbMaxPorDefecto = -60;

} // namespace thresholds

/** De donde sale la latencia de ida y vuelta. Va desglosada porque el reparto entre host y hardware decide donde se puede corregir: una latencia de host se compensa, una de hardware no. */
namespace latencyBreakdown
{

/** La clave del subobjeto. El namespace ocupa el nombre del alias. */
inline constexpr std::string_view contenedor = "latencyBreakdown";

/** Parte que se atribuye al host, en muestras. Es una estimacion y no una medida: por eso el nombre lo dice, y por eso no se puede compensaar con ella. */
inline constexpr std::string_view estimatedHostLatencySamples = "estimatedHostLatencySamples";

/** Parte que se atribuye al hardware, en muestras. Esta si es una medida del recorrido fisico de la senal. */
inline constexpr std::string_view estimatedHardwareLatencySamples = "estimatedHardwareLatencySamples";

} // namespace latencyBreakdown

} // namespace analogChainCalibration

// ==============================================================================
// modulationSideband
// ==============================================================================
//
// La funcion estatica `modulationSidebandToJson` de ABDAudioLab/src/measurement/MeasurementSerialization.cpp.
//
// Es el UNICO grupo que se escribe como array: `sidebands` es la lista y estas claves son las de cada elemento. Por eso el generador emite la clave del contenedor aparte de las de dentro, y el consumidor tiene que saber cuales son.

namespace modulationSideband
{

/** La clave del objeto padre. null en el catalogo = cuelga de la raiz. */
inline constexpr std::string_view contenedor = "sidebands";

/** Frecuencia de la portadora en Hz, la que se modulaba. Es la referencia contra la que se mide todo lo demas del grupo. */
inline constexpr std::string_view carrierFrequencyHz = "carrierFrequencyHz";

/** Frecuencia de esta banda lateral en Hz. Junto con `order` dice si la banda es superior o inferior a la portadora. */
inline constexpr std::string_view sidebandFrequencyHz = "sidebandFrequencyHz";

/** Orden de la banda lateral: -1 es la inferior, +1 la superior, y hay mas cuando la modulacion no es simple. La lectura usa 1 por defecto, que es la superior: es la unica que se ve cuando la modulacion es de indice variable. */
inline constexpr std::string_view order = "order";
/** El valor que la lectura usa cuando la clave no esta. */
inline constexpr auto orderPorDefecto = 1;

/** Nivel de esta banda respecto a la portadora, en dB. Es NEGATIVO siempre que la banda existe, asi que un 0 dB no es modulacion fuerte: son dos portadoras, que es otra medicion y otra historia. */
inline constexpr std::string_view levelRelativeToCarrierDb = "levelRelativeToCarrierDb";

} // namespace modulationSideband

// ==============================================================================
// modulationResult
// ==============================================================================
//
// La funcion estatica `modulationResultToJson` de ABDAudioLab/src/measurement/MeasurementSerialization.cpp.
//
// Los tres subobjetos —`rateHz`, `depth` y `waveform`— tienen las MISMAS claves internas (`name`, `value`, `unit`, `status`, `reason`). Por eso los alias generados llevan el nombre del subobjeto delante: si no, tres claves distintas en el JSON se llamarian igual en C++ y el renombrado de una pondria las tres en silencio.

namespace modulationResult
{

/** La clave del objeto padre. null en el catalogo = cuelga de la raiz. */
inline constexpr std::string_view contenedor = "modulationResult";

/** Destino de la matriz que se estaba modulando. Es lo que une esta medicion con un contrato de matriz de modulacion: sin el, un resultado no se sabe a que destino pertenece. */
inline constexpr std::string_view targetDestination = "targetDestination";
/** El valor que la lectura usa cuando la clave no esta. */
inline constexpr auto targetDestinationPorDefecto = "unknown";

/** La frecuencia de modulacion, medida o estimada. `value` va siempre en Hz; lo que cambia entre medida y estimacion es `status` y `reason`. */
namespace rateHz
{

/** La clave del subobjeto. El namespace ocupa el nombre del alias. */
inline constexpr std::string_view contenedor = "rateHz";

/** Nombre de la magnitud, para que el panel no tenga que deducirlo del valor. No es la clave del dato: es su etiqueta. */
inline constexpr std::string_view name = "name";
/** El valor que la lectura usa cuando la clave no esta. */
inline constexpr auto namePorDefecto = "rate";

/** El valor medido, en la unidad que dice `unit`. Un 0 aqui es un valor real y no una ausencia: por eso `status` es lo que dice si se midio. */
inline constexpr std::string_view value = "value";

/** Unidad del valor: Hz, porque la frecuencia de modulacion SIEMPRE se expresa en hercios. Va declarada porque el subobjeto de al lado guarda una profundidad, que va en cents. */
inline constexpr std::string_view unit = "unit";
/** El valor que la lectura usa cuando la clave no esta. */
inline constexpr auto unitPorDefecto = "Hz";

/** Si el valor se midio o se estimo. Es la clave que separa un numero de una conjetura, y la que hay que mirar antes de publicar el valor. */
inline constexpr std::string_view status = "status";
/** El valor que la lectura usa cuando la clave no esta. */
inline constexpr auto statusPorDefecto = "observed";

/** Por que se estimo, si se estimo. Solo se escribe cuando no esta vacio, y es la unica pista de por que el valor no es de fiar. */
inline constexpr std::string_view reason = "reason";

} // namespace rateHz

/** Como se obtuvo la frecuencia de modulacion. Sin esto, un valor leido del pico del espectro y uno interpolado son indistinguibles, y son medidas de distinta calidad. */
inline constexpr std::string_view rateMethod = "rateMethod";
/** El valor que la lectura usa cuando la clave no esta. */
inline constexpr auto rateMethodPorDefecto = "spectral_peak";

/** La profundidad de la modulacion. Misma forma que `rateHz` porque se miden igual, y por eso tienen las mismas claves internas. */
namespace depth
{

/** La clave del subobjeto. El namespace ocupa el nombre del alias. */
inline constexpr std::string_view contenedor = "depth";

/** Nombre de la magnitud, con el mismo papel que en `rateHz`: etiqueta, no dato. */
inline constexpr std::string_view name = "name";
/** El valor que la lectura usa cuando la clave no esta. */
inline constexpr auto namePorDefecto = "depth";

/** El valor medido, en la unidad que dice `unit`. Aqui el 0 SI es una respuesta valida: una modulacion de amplitud cero es una medicion, no una ausencia. */
inline constexpr std::string_view value = "value";

/** Unidad del valor: cents, que es como se expresa la profundidad. Distinta de la unidad de `rateHz` a proposito, y es el motivo de que este subobjeto no comparta claves con el. */
inline constexpr std::string_view unit = "unit";
/** El valor que la lectura usa cuando la clave no esta. */
inline constexpr auto unitPorDefecto = "cents";

/** Si el valor se midio o se estimo. Con el mismo criterio que en `rateHz`, porque la forma de la medicion es la misma. */
inline constexpr std::string_view status = "status";
/** El valor que la lectura usa cuando la clave no esta. */
inline constexpr auto statusPorDefecto = "observed";

/** Por que se estimo, si se estimo. Solo se escribe cuando no esta vacio. */
inline constexpr std::string_view reason = "reason";

} // namespace depth

/** La forma de onda detectada. Es la unica parte del grupo que NO es un valor con unidades: es una clasificacion, y por eso lleva `confidence` en vez de unidad. */
namespace waveform
{

/** La clave del subobjeto. El namespace ocupa el nombre del alias. */
inline constexpr std::string_view contenedor = "waveform";

/** La forma de onda detectada. Se llama igual que su contenedor porque el uno dice COMO es la forma y el otro la GUARDA. */
inline constexpr std::string_view waveform = "waveform";
/** El valor que la lectura usa cuando la clave no esta. */
inline constexpr auto waveformPorDefecto = "none";

/** Si se pudo determinar. `not_observable` es un resultado, no un fallo: hay modulaciones cuya forma no se puede clasificar y eso hay que poder decirlo. */
inline constexpr std::string_view status = "status";
/** El valor que la lectura usa cuando la clave no esta. */
inline constexpr auto statusPorDefecto = "not_observable";

/** Confianza de la clasificacion, de 0 a 1. Es el numero que hay que mirar antes de fiarse de `waveform`, porque la forma es la parte mas frágil del grupo. */
inline constexpr std::string_view confidence = "confidence";

} // namespace waveform

/** Las bandas laterales medidas. Los elementos son los del grupo `modulationSideband`, y esta clave es la unica parte del grupo que es una lista. */
inline constexpr std::string_view sidebands = "sidebands";

/** Como se computo el espectro. Se escribe dentro del grupo de modulacion pero lo produce `spectralAnalysisToJson`, que es compartido con la medicion de dinamica: sus claves NO se declaran aqui porque no son de este grupo. */
inline constexpr std::string_view spectralMetadata = "spectralMetadata";

/** La envolvente temporal de la modulacion. La produce `curveToJson`, compartida con el resto de mediciones, asi que tampoco son claves de este grupo. */
inline constexpr std::string_view timeCurve = "timeCurve";

/** El espectro de la modulacion en dB. Lo produce `curveToJson` igual que la curva temporal, y por la misma razon no son claves de este grupo. */
inline constexpr std::string_view spectrumCurve = "spectrumCurve";

} // namespace modulationResult

// ==============================================================================
// EL INDICE, Y PARA QUE ESTA
// ==============================================================================
//
// Un array con el nombre de cada clave, para poder recorrerlas todas sin
// escribirlas una a una. Un consumidor que solo sepa atender las que hay
// en su lista no se entera de que hay una nueva, y esa es la forma que
// toma un clave que se escribe y no se lee.
//
// El `numeroClaves` de arriba y la longitud de este array tienen que
// coincidir, y el `static_assert` de `MeasurementExportKeys.h` lo comprueba.
// ==============================================================================

inline constexpr std::string_view todasLasClaves[] = {
    "analogCalibration",
    "calibrationId",
    "sampleRateHz",
    "blockSize",
    "roundTripLatencySamples",
    "snrDb",
    "peakDbfs",
    "dcOffsetDb",
    "status",
    "thresholds",
    "snrDbMin",
    "peakDbfsMax",
    "dcOffsetDbMax",
    "latencyBreakdown",
    "estimatedHostLatencySamples",
    "estimatedHardwareLatencySamples",
    "sidebands",
    "carrierFrequencyHz",
    "sidebandFrequencyHz",
    "order",
    "levelRelativeToCarrierDb",
    "modulationResult",
    "targetDestination",
    "rateHz",
    "name",
    "value",
    "unit",
    "status",
    "reason",
    "rateMethod",
    "depth",
    "name",
    "value",
    "unit",
    "status",
    "reason",
    "waveform",
    "waveform",
    "status",
    "confidence",
    "sidebands",
    "spectralMetadata",
    "timeCurve",
    "spectrumCurve",
};

/**
 * Cuantas claves DISTINTAS hay en el indice, contando cada nombre una vez.
 *
 * No es lo mismo que `numeroClaves`, y la diferencia es el motivo de que este
 * numero exista: `rateHz`, `depth` y `waveform` declaran las MISMAS claves
 * internas —`name`, `value`, `unit`, `status`, `reason`—, asi que el indice las
 * cuenta cinco veces y el catalogo las declara tres. El conteo de arriba es la
 * suma de las entradas; este es el numero de nombres.
 *
 * Es lo que compara el `static_assert` de `MeasurementExportKeys.h` con el
 * inventario que ese header mantiene, y por eso se emite aqui: el header no
 * puede contarlo sin recorrer el indice, y un recorrido en tiempo de compilacion
 * para obtener una constante es un numero que cambia de sitio cuando cambia el
 * generador, que es justo cuando nadie lo mira.
 */
inline constexpr std::size_t numeroEntradasUnicas = 35;

} // namespace abdaudiolab::measurement::claves::generado
