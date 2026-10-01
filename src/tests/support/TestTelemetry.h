/**
 * @file TestTelemetry.h
 * @brief Telemetria del runner: heartbeat por test y volcado de pila al morir.
 *
 * QUE PROBLEMA RESUELVE
 * ---------------------
 * Gate 6 corre `ABDAudioLab_Tests.exe "~[ves]"`. Cuando ese proceso muere en
 * silencio, la consola no dice nada: no hay fallo de Catch2, no hay asercion, no
 * hay linea final. El paso de CI falla y el reporte es "exit code 1" sin sujeto.
 * Con 318 casos, esa informacion no permite atribuir la muerte a nadie.
 *
 * Este modulo deja, para cada test y cada seccion, un latido en un fichero
 * append-only que se escribe y se descarga linea a linea. Cuando el proceso muere,
 * la ultima linea sin pareja es el culpable. Cuando el proceso se queda colgado,
 * los latidos siguen avanzando y la ultima marca de tiempo dice desde cuando y en
 * que test.
 *
 * LAS TRES FORMAS DE MORIR Y COMO SE DISTINGUEN
 * --------------------------------------------
 *   1. CAIDA  el proceso revienta. El manejador escribe EXCEPTION + CRASH + FRAME
 *             por frame + un minidump. Si el manejador no pudo escribir, la ultima
 *             linea de telemetria sigue diciendo donde estaba.
 *   2. COLGADO  el proceso vive y no avanza. Un hilo daemon escribe TICK cada
 *             kLivenessTickMs, asi que se distingue de la caida: si los TICK
 *             siguen llegando despues del ultimo CASE-START, el test esta
 *             bloqueado, no muerto, y el TICK dice cuantos milisegundos lleva.
 *   3. INTERRUMPIDO  alguien mata el proceso (timeout del runner, cancelacion).
 *             No hay EXCEPTION ni TICK posterior; el diagnostico es la marca de
 *             tiempo del ultimo latido comparada con la del corte.
 *
 * tools/test_telemetry_report.py lee el log y da esa atribucion. No hace falta
 * ningun binario adicional ni registro en el arbol del repositorio.
 *
 * DONDE SE ESCRIBE, Y POR QUE NO EN EL REPO
 * ------------------------------------------
 * En %TEMP%/abdaudiolab-tests/runner, el mismo scratch que usa el resto de la
 * suite, y se puede redirigir con ABD_TEST_TELEMETRY_DIR. Escribir en el arbol
 * del repositorio esta prohibido por el propio guard [writes] de esta suite: una
 * instrumentacion que ensucia el arbol que se supone que vigila no sirve de nada.
 * Por eso NO se usa scratchDir(), que ademas lanza si se le pide un directorio
 * dentro del repo: aqui el destino se elige a mano y se descarta si falla.
 *
 * NUNCA FALLA LA SUITE
 * --------------------
 * Si no se puede crear el directorio, o el fichero, o el disco esta lleno, el
 * modulo se desactiva y sigue. Un guard que puede tumbar la puerta que vigila es
 * peor que no tenerlo. La desactivacion se puede comprobar con isActive().
 *
 * @author ABDSynths
 * @date 2026
 */

#pragma once

namespace abdaudiolab::test::telemetry
{

/** Variable de entorno que redirige el directorio del log y del volcado. */
inline constexpr const char* kTelemetryDirEnvVar = "ABD_TEST_TELEMETRY_DIR";

/**
 * Variable de entorno que desactiva la telemetria entera.
 *
 * Se interpreta como bandera POSITIVA: solo desactiva con 0 / false / off / no.
 * Leerla como "si esta definida, apagada" hacia que un CI que definiese la
 * variable por otra razon apagase el diagnostico sin que nadie se entere.
 */
inline constexpr const char* kTelemetryDisableEnvVar = "ABD_TEST_TELEMETRY";

/**
 * Prefijo del nombre de los ficheros de telemetria.
 *
 * El nombre completo es `run-<pid>-<n>.<log|dmp>`. El PID solo no basta porque
 * Windows lo recicla: dos corridas consecutivas tienen que quedar en ficheros
 * distintos o el diagnostico mezcla la muerte de una con los latidos de la otra.
 */
inline constexpr const char* kTelemetryRunPrefix = "run-";

/** Extension del log de telemetria. */
inline constexpr const char* kTelemetryLogExtension = ".log";

/** Extension del volcado de proceso. */
inline constexpr const char* kTelemetryDumpExtension = ".dmp";

/** Periodo del latido de liveness, en milisegundos. */
inline constexpr int kLivenessTickMs = 5000;

/** @brief Prepara el recorder, el listener y los manejadores de caida. */
void start();

/** @brief true si la telemetria quedo activa. Util para diagnosticos. */
[[nodiscard]] bool isActive() noexcept;

} // namespace abdaudiolab::test::telemetry