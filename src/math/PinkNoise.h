#pragma once

#include <cstdint>

namespace abdaudiolab::math
{

/**
 * @brief Fuente unica del ruido blanco y del ruido rosa.
 *
 * Existian DOS copias de esta matematica, byte a byte, en dos capas distintas:
 * el generador de estimulo del laboratorio y el coordinador de estimulacion de
 * medicion. No se unifican porque suenen parecido: se unifican porque las dos
 * DEVUELVEN LA MISMA SECUENCIA para la misma semilla, y eso es una propiedad
 * que se puede comprobar, mientras que "suena igual" no se puede comprobar con
 * nada. Dos copias de una regla numerica son la forma mas barata de perder un
 * dia entero comparando hashes que no cuadran sin saber por que.
 *
 * Y vive en `math/`, no en `audio/`, porque no es audio: no reproduce nada, no
 * toca el dispositivo ni el hilo, es un filtro de recurrencia y un LCG de 32
 * bits. `audio/` es el que lo consume, y `measurement/` tambien, y mientras
 * viviera en `audio/` este segundo hadia de incluir una capa de la que no
 * depende: la inversion de capas entra por un `#include` y no se ve en ningun
 * diagrama. Un LCG con siete filas de filtro tiene su sitio al lado de las otras
 * matematicas del laboratorio, y desde ahi las dos capas bajan.
 *
 * Sin asignaciones, sin estado global y sin virtuales: se llama desde el hilo
 * de audio, aqui no hay nada que pueda fallar ni que pueda reservar memoria.
 */
class PinkNoiseGenerator
{
public:
    /** @brief Semilla por defecto. Es un numero fijo, no una constante magica:
     *         aparece en `reset()` y aqui, y si divergen los dos sitios dejan de
     *         ser comparables sin que nada se entere. */
    static constexpr uint32_t defaultSeed { 0x48271983u };

    explicit PinkNoiseGenerator (uint32_t seed = defaultSeed) noexcept
    {
        reseed (seed);
    }

    // El generador es copiable a proposito y sin avisar: son 8 escalares y
    // cabe en cualquier sitio. Lo peligroso seria que se copiara por error
    // creyendo que es un handle, porque las dos copias darian la MISMA serie.
    // Por eso lleva nombre de generador y no de fuente de ruido.

    /** @brief Vuelve a la semilla inicial y limpia el filtro. */
    void reseed (uint32_t seed) noexcept
    {
        randomSeed = seed;
        clearFilterState();
    }

    /**
     * @brief Limpia SOLO el estado del filtro Voss-McCartney, sin tocar la
     *        semilla.
     *
     * Son dos cosas distintas y por eso son dos metodos. `reset()` reinicia el
     * generador entero, con la misma serie desde el principio; `setStimulus()`
     * en cambio reinicia el filtro y deja que la serie de ruido CONTINUE. Si
     * ambas fueran una sola, cambiar de estimulo reiniciaria el azar y dos
     * fragmentos de la misma sesion dejarian de ser continuos.
     */
    void clearFilterState() noexcept
    {
        b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0f;
    }

    /** @brief Un blanco uniforme en [-1, 1) del LCG de 32 bits. */
    float nextWhite() noexcept
    {
        randomSeed = randomSeed * 1664525u + 1013904223u;
        return (static_cast<float> (randomSeed) / 2147483648.0f) - 1.0f;
    }

    /**
     * @brief Un rosa por Voss-McCartney, 7 filas, en rango nominal de +-1.
     *
     * Dos detalles que parecen erratas y no lo son, y que por eso estan
     * comentados aqui y no en el sitio de cada llamada:
     *
     * - `b6` entra en la suma ANTES de actualizarse. Es un retardo de una
     *   muestra deliberado: es lo que hace que la septima fila contribuya con
     *   el mismo peso que las otras seis. Si se moviera la asignacion antes de
     *   la suma, la primera vez que `b6` se usaria valdria 0 y el filtro
     *   tendria una fila menos durante toda la serie.
     * - El `* 0.11f` se aplica FUERA de la suma, despues de haberla cerrado.
     *   Multiplicar cada fila por 0.11 en vez de la suma final no es lo mismo:
     *   reparte el error de redondeo en siete sumandos en vez de uno, y el
     *   resultado no coincide ni en el ultimo bit.
     */
    float nextPink() noexcept
    {
        const float white = nextWhite();

        b0 = 0.99886f * b0 + white * 0.0555179f;
        b1 = 0.99332f * b1 + white * 0.0750759f;
        b2 = 0.96900f * b2 + white * 0.1538520f;
        b3 = 0.86650f * b3 + white * 0.3104856f;
        b4 = 0.55000f * b4 + white * 0.5329522f;
        b5 = -0.7616f * b5 - white * 0.0168980f;

        const float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;
        b6 = white * 0.115926f;

        return pink * 0.11f;
    }

private:
    uint32_t randomSeed { defaultSeed };
    float b0 { 0.0f }, b1 { 0.0f }, b2 { 0.0f }, b3 { 0.0f }, b4 { 0.0f }, b5 { 0.0f }, b6 { 0.0f };
};

} // namespace abdaudiolab::math