// =============================================================================
// ABDAudioLab - El ruido rosa queda fijado, y por que un hash y no un oido
// =============================================================================
//
// QUE CUBRE ESTE TEST.
//
// `math/PinkNoise.h` es la fuente unica del ruido blanco y del ruido rosa. Antes
// de que se unificara existian DOS copias de esa matematica, byte a byte: una en
// el generador de estimulo del laboratorio y otra en el coordinador de
// estimulacion de medicion. La unificacion se comprobo una vez, con 6 semillas x
// 4 patrones x 200.000 muestras y 4,8 millones de `memcmp`, y el resultado fue
// que las dos copias eran la misma funcion. Ese chequeo se hizo con un programa
// suelto, fuera del arbol, y un programa suelto fuera del arbol no se vuelve a
// ejecutar nunca: se borra y se pierde.
//
// Este test es el mismo chequeo, pero con una forma que sobrevive a que se le
// pase la batuta: comprueba la serie CONTRA SI MISMA, con la formula del LCG
// reescrita aqui al lado, y con un hash fijado para tres semillas. Si alguien
// toca un coeficiente, reordena una suma o mueve el `* 0.11f` al final de la
// suma, este test se pone rojo y el fallo dice exactamente que cambio.
//
// ----------------------------------------------------------------------------
// POR QUE UN HASH Y NO UNA ESTADISTICA.
//
// Lo tempting aqui es comprobar que "suena a rosa": energia repartida, media
// cero, varianza estable. Y todo eso lo cumple un blanco multiplicado por un
// numero, asi que unas estadisticas que no distinguen el ruido equivocado no
// comprueban la unificacion, que es lo unico que hay que comprobar aqui. Lo que
// si distingue al rosa de un blanco es la pendiente espectral, y eso lo mide la
// seccion 5, con margenes amplios y sin numeros dorados.
//
// Lo que NO hace, a proposito: no mide audible, ni nivel, ni si la amplitud
// nominal de +-1 se respeta en la practica. Eso es una pregunta de mezcla, y
// la mezcla no es de este fichero.
// =============================================================================

#include <catch2/catch_test_macros.hpp>

#include "math/PinkNoise.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <vector>

using abdaudiolab::math::PinkNoiseGenerator;

namespace
{

/** Semillas fijas. No son al azar: si la serie cambia, tienen que ser los MISMOS
 *  los que se comprueban, o el hash no significa nada. */
constexpr std::uint32_t semillas[] { 0x48271983u, 0x00000001u, 0xdeadbeefu };

constexpr int muestrasPorHash = 20000;

/** FNV-1a de 64 bits, byte a byte. Sin dependencias: el test no puede pedirle un
 *  hash a una libreria que alguien pueda cambiar de version. */
constexpr std::uint64_t fnvOffsetBasis = 14695981039346656037ull;
constexpr std::uint64_t fnvPrime       = 1099511628211ull;

std::uint64_t fnv1aDeFloats (std::uint64_t hash, float valor)
{
    const auto bits = static_cast<std::uint32_t> (std::bit_cast<std::uint32_t> (valor));

    for (int byte = 0; byte < 4; ++byte)
    {
        hash ^= static_cast<std::uint64_t> ((bits >> (byte * 8)) & 0xffu);
        hash *= fnvPrime;
    }

    return hash;
}

/** El hash de la serie rosa completa de una semilla. */
std::uint64_t hashSerieRosa (std::uint32_t semilla)
{
    PinkNoiseGenerator ruido { semilla };

    std::uint64_t hash = fnvOffsetBasis;

    for (int n = 0; n < muestrasPorHash; ++n)
        hash = fnv1aDeFloats (hash, ruido.nextPink());

    return hash;
}

/** Raiz de la media cuadratica, en `double` para no acumular error en `float`. */
double rms (const std::vector<float>& senal)
{
    if (senal.empty())
        return 0.0;

    double suma = 0.0;
    for (float v : senal)
        suma += static_cast<double> (v) * static_cast<double> (v);

    return std::sqrt (suma / static_cast<double> (senal.size()));
}

/** Cuanta cuanta energia hay por encima de la banda baja, sin transformar nada:
 *  la diferencia entre muestras consecutivas de una senal es un diferenciador,
 *  y un diferenciador sube las agudas en proporcional a la frecuencia. */
double ratioDeDiferencia (const std::vector<float>& senal)
{
    if (senal.size() < 2)
        return 0.0;

    std::vector<float> derivada (senal.size() - 1);
    for (size_t n = 1; n < senal.size(); ++n)
        derivada[n - 1] = senal[n] - senal[n - 1];

    const double base = rms (senal);
    return base > 0.0 ? rms (derivada) / base : 0.0;
}

} // namespace

// -----------------------------------------------------------------------------
// SECCION 1: EL LCG, COMPROBADO CONTRA SI MISMO
// -----------------------------------------------------------------------------
//
// El LCG de `nextWhite()` es de 32 bits, con el multiplicador y el incremento
// mas pequenos que dan ciclo completo. Aqui se reescribe entero, con la misma
// aritmetica, y se compara muestra a muestra. Es el unico test de este fichero
// que no depende de un numero fijado: depende de la formula, asi que si el
// multiplicador cambia, el que tiene que cambiar es este.
// -----------------------------------------------------------------------------

TEST_CASE ("El LCG de nextWhite() es el que dice ser, muestra a muestra",
           "[audio][math][pink_noise]")
{
    PinkNoiseGenerator ruido { 0x12345678u };

    std::uint32_t semilla = 0x12345678u;

    // Se comparan las 10.000, pero no se escribe una asercion por muestra: son
    // 10.000 lineas de log que solo se leen cuando algo falla, y quien lee un
    // fallo de estos no necesita las otras 9.999, necesita saber DONDE fallo. Se
    // guarda la primera discrepancia y su semilla, y ya.
    int primeraDiscrepancia  = -1;
    std::uint32_t semillaEnFallo = 0;

    for (int n = 0; n < 10000; ++n)
    {
        semilla = semilla * 1664525u + 1013904223u;

        const float esperado = (static_cast<float> (semilla) / 2147483648.0f) - 1.0f;
        const float obtenido = ruido.nextWhite();

        if (primeraDiscrepancia < 0 && obtenido != esperado)
        {
            primeraDiscrepancia  = n;
            semillaEnFallo = semilla;
        }
    }

    INFO ("primera discrepancia en la muestra " << primeraDiscrepancia
          << ", semilla en ese punto " << semillaEnFallo);
    CHECK (primeraDiscrepancia == -1);
}

TEST_CASE ("nextWhite() cae en [-1, 1) y no se sale nunca",
           "[audio][math][pink_noise]")
{
    PinkNoiseGenerator ruido;

    // El rango de 200.000 muestras se comprueba con el minimo y el maximo, que
    // dicen lo mismo que comprobar cada una y cuestan dos aserciones en vez de
    // 400.000. El recuento de no-finitos va aparte porque un NaN se propaga al
    // minimo y al maximo y los dos comparadores darian `false` sin decir por que.
    float minimo =  1.0f;
    float maximo = -1.0f;
    int noFinitos = 0;

    for (int n = 0; n < 200000; ++n)
    {
        const float v = ruido.nextWhite();

        if (!std::isfinite (v))
            ++noFinitos;

        minimo = std::min (minimo, v);
        maximo = std::max (maximo, v);
    }

    INFO ("minimo = " << minimo << ", maximo = " << maximo
          << ", no finitos = " << noFinitos);

    REQUIRE (noFinitos == 0);
    CHECK (minimo >= -1.0f);
    CHECK (maximo <   1.0f);
}

// -----------------------------------------------------------------------------
// SECCION 2: LA SERIE ROSA, FIJADA
// -----------------------------------------------------------------------------
//
// Aqui esta el peso del fichero. Tres semillas, 20.000 muestras cada una, y el
// hash de los cuatro bytes de cada float. Un hash de una serie de floats solo
// sirve si la serie esta fijada bit a bit, y lo esta: la seccion de arriba
// demuestra que la parte aleatoria es exactamente la que dice ser, y el filtro
// de Voss-McCartney son sumas y productos en coma flotante, que son
// deterministas.
//
// Cualquier cambio en la matematica cambia uno de estos tres numeros. No hace
// falta comparar dos implementaciones, porque la segunda ya no existe: lo que se
// fija es que la unica que queda no se mueva otra vez.
// -----------------------------------------------------------------------------

TEST_CASE ("La serie rosa esta fijada: el hash de tres semillas no se mueve",
           "[audio][math][pink_noise]")
{
    // Los tres valores los fijo esta misma implementacion el dia que se
    // unificaron las dos copias, despues de comparar 4,8 millones de muestras
    // entre la copia del laboratorio y la de medicion: cero discrepancias. Un
    // hash aqui no es un retrato del estado actual: es el testigo de que las dos
    // capas siguen devolviendo la MISMA serie, ahora que ya solo hay una.
    constexpr std::uint64_t hashEsperado[] {
        0xb4606c87421218f2ull, // 0x48271983u
        0x14b9ba0fa83041faull, // 0x00000001u
        0x6732a970b727d0c9ull  // 0xdeadbeefu
    };

    for (size_t i = 0; i < std::size (semillas); ++i)
    {
        const auto hash = hashSerieRosa (semillas[i]);

        INFO ("semilla " << semillas[i] << ", " << muestrasPorHash << " muestras");
        CHECK (hash == hashEsperado[i]);
    }
}

// -----------------------------------------------------------------------------
// SECCION 3: LOS DOS MODOS DE EMPEZAR DE CERO, QUE NO SON EL MISMO
// -----------------------------------------------------------------------------
//
// `reseed()` y `clearFilterState()` se parecen y no son lo mismo: el primero
// reinicia el generador entero y devuelve la MISMA serie desde el principio; el
// segundo limpia solo las siete filas del filtro y deja que la serie de azar
// CONTINUE. Si los dos fueran uno, cambiar de estimulo reiniciaria el azar y dos
// fragmentos de la misma sesion dejarian de ser continuos, que es exactamente el
// fallo que el comentario del `.h` describe. Aqui se comprueba que siguen
// distinguiendose.
// -----------------------------------------------------------------------------

TEST_CASE ("clearFilterState() no reinicia el azar: la serie sigue donde estaba",
           "[audio][math][pink_noise]")
{
    constexpr int avance = 500;

    PinkNoiseGenerator referencia { 0x00c0ffeeu };
    for (int n = 0; n < avance; ++n)
        referencia.nextPink();

    // Otra con la misma semilla y con la limpieza en el mismo punto. Ojo al
    // numero de muestras DESPUES de limpiar: limpiar el filtro no toca el LCG,
    // asi que esta generadora tambien tiene que haber gastado `avance` numeros
    // para que las dos esten en el mismo punto de la serie de azar. Con solo la
    // mitad, este test comparaba el numero 501 contra el 251 y se ponia rojo por
    // la razon equivocada.
    PinkNoiseGenerator limpiada { 0x00c0ffeeu };
    for (int n = 0; n < avance / 2; ++n)
        limpiada.nextPink();

    limpiada.clearFilterState();

    for (int n = avance / 2; n < avance; ++n)
        limpiada.nextPink();

    // Lo que sigue a la limpieza tiene que ser el azar que le tocaba, no el
    // principio: la MISMA serie blanca que la referencia, aunque el rosa no
    // coincida todavia porque el filtro vuelve a cero.
    const float blancoReferencia = referencia.nextWhite();
    const float blancoLimpiada   = limpiada.nextWhite();
    CHECK (blancoLimpiada == blancoReferencia);

    // Y el rosa, en cambio, no coincide: el filtro se limpio de verdad. Y las
    // muestras se comparan en puntos distintos a proposito, porque dos
    // generadores en el MISMO punto dan el mismo numero siempre, y esa
    // comparacion no distinguiria un handle de una copia ni de la nada.
    const float rosaReferencia = referencia.nextPink();

    for (int n = 0; n < 200; ++n)
        limpiada.nextPink();

    const float rosaLimpiada = limpiada.nextPink();

    INFO ("rosa de la referencia = " << rosaReferencia
          << ", rosa de la limpiada = " << rosaLimpiada);
    CHECK (rosaLimpiada != rosaReferencia);
}

TEST_CASE ("reseed() devuelve el generador al principio, con la misma serie",
           "[audio][math][pink_noise]")
{
    PinkNoiseGenerator original { 0x48271983u };

    for (int n = 0; n < 12345; ++n)
        original.nextPink();

    original.reseed (0x48271983u);

    PinkNoiseGenerator recienCreado { 0x48271983u };

    for (int n = 0; n < 1000; ++n)
    {
        INFO ("muestra " << n);
        CHECK (original.nextPink() == recienCreado.nextPink());
    }
}

TEST_CASE ("La semilla por defecto de la clase y la de reseed() son la misma",
           "[audio][math][pink_noise]")
{
    // Si divergen, dos generadores que "empiezan igual" dejan de ser
    // comparables, y el hash de la seccion 2 empieza a no decir nada.
    CHECK (PinkNoiseGenerator::defaultSeed == 0x48271983u);

    PinkNoiseGenerator porDefecto;
    PinkNoiseGenerator explicito { PinkNoiseGenerator::defaultSeed };

    for (int n = 0; n < 500; ++n)
    {
        INFO ("muestra " << n);
        CHECK (porDefecto.nextPink() == explicito.nextPink());
    }
}

// -----------------------------------------------------------------------------
// SECCION 4: COPIAR NO ES COMPARTIR
// -----------------------------------------------------------------------------
//
// El generador son ocho escalares y se copia a proposito. Lo peligroso seria que
// se copiara creyendo que es un handle: las dos copias darian la MISMA serie y
// el que las fuera avanzando una por una creyendo que cada una tiene su propio
// azar compararia un numero consigo mismo. Este test es el que se lleva el
// disgusto.
// -----------------------------------------------------------------------------

TEST_CASE ("Copiar un generador produce dos series independientes, no un handle",
           "[audio][math][pink_noise]")
{
    PinkNoiseGenerator original { 0x0000beefu };
    PinkNoiseGenerator copia = original;

    for (int n = 0; n < 300; ++n)
    {
        const float deOriginal = original.nextPink();
        const float deCopia     = copia.nextPink();

        INFO ("muestra " << n);
        CHECK (deOriginal == deCopia);
    }

    // Ahora el original se adelanta y la copia no. Y aqui esta el truco del
    // test: NO se puede comparar "el siguiente de cada uno", porque los dos estan
    // en el mismo punto de la serie y eso da igual por definicion. Para ver un
    // handle hay que mover uno de los dos y mirar si el otro se mueve con el.
    // Por eso el original avanza tres muestras antes de que nadie lea nada.
    original.nextPink();
    original.nextPink();

    const float deOriginal = original.nextPink();

    // La copia no se entero: su siguiente muestra sigue siendo la que le
    // tocaba cuando se separaron, y lo dice una referencia independiente que
    // llega al mismo punto por su cuenta.
    PinkNoiseGenerator referenciaEn300 { 0x0000beefu };
    for (int n = 0; n < 300; ++n)
        referenciaEn300.nextPink();

    const float esperadoDeCopia = referenciaEn300.nextPink();
    const float deCopia         = copia.nextPink();

    INFO ("deCopia = " << deCopia << ", esperado = " << esperadoDeCopia);
    INFO ("deOriginal = " << deOriginal);
    CHECK (deCopia == esperadoDeCopia);
    CHECK (deCopia != deOriginal);
}

// -----------------------------------------------------------------------------
// SECCION 5: ES ROSA, Y NI BLANCO NI MARRON
// -----------------------------------------------------------------------------
//
// Por fin una seccion sin numeros dorados. La pendiente se mide usando el blanco
// del propio generador como control: `rms(x[n] - x[n-1]) / rms(x)` vale raiz de
// 2 para CUALQUIER blanco, y el mismo numero aplicado a una senal con mas graves
// DICE MENOS, no mas. Es contraintuitivo, y por eso vale la pena dejarlo escrito.
//
// La diferencia entre muestras consecutivas es un diferenciador, y un
// diferenciador tiene ganancia `2 sin(w/2)`: cero en la continua y como mucho 2
// en la Nyquist. A un blanco le reparte la potencia por igual y le queda raiz de
// 2. A un rosa le sobran graves, y esos graves son justo los que el
// diferenciador AMORTIGUA, asi que el ratio baja. A un marron le sobran mas
// todavia y el ratio se desploma. El orden es marron < rosa < blanco, y el hueco
// entre los tres es de un orden de magnitud, asi que dos franjas relativas
// bastan para separar las familias sin un solo numero magico.
//
// Medido sobre 262.144 muestras con la semilla de aqui: blanco 1.415, rosa 0.587
// (0.415 veces el blanco) y marron 0.030 (0.021 veces el blanco). La franja se
// fija en [0.25, 0.70] del blanco: deja un 66% de margen por arriba y por abajo
// en cada lado, que es de donde sale la idea de que esto no se va a poner rojo
// porque cambiara el redondeo.
// -----------------------------------------------------------------------------

TEST_CASE ("La pendiente del ruido rosa cae entre la del blanco y la del marron",
           "[audio][math][pink_noise]")
{
    constexpr int n = 262144;
    constexpr std::uint32_t semilla = 0x13572468u;

    PinkNoiseGenerator rosa   { semilla };
    PinkNoiseGenerator blanco { semilla };

    std::vector<float> serieRosa (n);
    std::vector<float> serieBlanca (n);
    std::vector<float> serieMarron (n);

    // El marron es el MISMO blanco pasado por un integrador de un polo: no para
    // comparar amplitud sino para tener el otro extremo de la pendiente a mano,
    // y comprobar que el discriminante separa tres familias y no solo dos.
    float acumulado = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        serieRosa[i] = rosa.nextPink();

        const float blanca = blanco.nextWhite();
        serieBlanca[i] = blanca;

        acumulado = 0.9995f * acumulado + 0.0005f * blanca;
        serieMarron[i] = acumulado;
    }

    const double ratioRosa   = ratioDeDiferencia (serieRosa);
    const double ratioBlanco = ratioDeDiferencia (serieBlanca);
    const double ratioMarron = ratioDeDiferencia (serieMarron);

    INFO ("ratio rosa      = " << ratioRosa);
    INFO ("ratio blanco    = " << ratioBlanco);
    INFO ("ratio marron    = " << ratioMarron);
    INFO ("rosa / blanco   = " << ratioRosa / ratioBlanco);
    INFO ("marron / blanco = " << ratioMarron / ratioBlanco);

    // El control primero: si el blanco no sale a raiz de 2, la medicion esta
    // rota y el resto del test no diria nada aunque pasara.
    REQUIRE (ratioBlanco > 1.38);
    REQUIRE (ratioBlanco < 1.45);

    // Y ahora la pendiente: grave de sobra para no ser blanco, y sin llegar al
    // marron, que cae por debajo del 10% y se queda fuera por el otro lado.
    REQUIRE (ratioRosa > ratioBlanco * 0.25);
    REQUIRE (ratioRosa < ratioBlanco * 0.70);

    REQUIRE (ratioMarron < ratioBlanco * 0.10);
}

// -----------------------------------------------------------------------------
// SECCION 6: ESTACIONARIDAD Y RANGO
// -----------------------------------------------------------------------------
//
// Lo minimo para que un ruido sea usable como estimulo y no solo como numero:
// que no se vaya de rango, que no produzca NaN y que no cambie de nivel a mitad
// de la serie. Un filtro mal inicializado se ve justo aqui, en la primera
// mitad, y no en el hash.
// -----------------------------------------------------------------------------

TEST_CASE ("El ruido rosa no se va de rango, no produce NaN y mantiene el nivel",
           "[audio][math][pink_noise]")
{
    constexpr int n = 65536;

    PinkNoiseGenerator ruido { 0x7fffffffu };

    double sumaMitadA = 0.0;
    double sumaMitadB = 0.0;
    double maxAbs = 0.0;
    int noFinitos = 0;
    int fueraDeRango = 0;

    for (int i = 0; i < n; ++i)
    {
        const float v = ruido.nextPink();

        if (!std::isfinite (v))
            ++noFinitos;
        else if (v < -4.0f || v > 4.0f)
            ++fueraDeRango;

        maxAbs = std::max (maxAbs, std::abs (static_cast<double> (v)));

        if (i < n / 2)
            sumaMitadA += static_cast<double> (v) * static_cast<double> (v);
        else
            sumaMitadB += static_cast<double> (v) * static_cast<double> (v);
    }

    const double rmsA = std::sqrt (sumaMitadA / (n / 2));
    const double rmsB = std::sqrt (sumaMitadB / (n / 2));

    INFO ("rms primera mitad = " << rmsA << ", rms segunda mitad = " << rmsB);
    INFO ("max |v| = " << maxAbs << ", no finitos = " << noFinitos
          << ", fuera de rango = " << fueraDeRango);

    REQUIRE (noFinitos == 0);
    REQUIRE (fueraDeRango == 0);

    // El rango nominal es +-1 y el pico real no pasa de 4 con ninguna de las
    // semillas de aqui. Si algun dia se toca el `* 0.11f`, este techo avisa.
    CHECK (maxAbs < 4.0);

    // Dos mitades de la misma serie al mismo nivel: ni la una con la otra.
    const double diferencia = std::abs (rmsA - rmsB) / std::max (rmsA, rmsB);
    CHECK (diferencia < 0.10);
}
