#!/usr/bin/env python3
"""
ABDAudioLab - Lector de telemetria del runner de tests.

Traduce el log append-only que deja src/tests/support/TestTelemetry.cpp en una
atribucion: que test estaba corriendo cuando el proceso dejo de hablar, y por que
modo dejo de hacerlo.

QUE PROBLEMA RESUELVE
---------------------
Gate 6 corre `ABDAudioLab_Tests.exe "~[ves]"`. Cuando ese proceso muere en
silencio la consola no dice nada y el reporte de CI es "exit code 1" sin sujeto.
Con 318 casos, esa informacion no permite atribuir la muerte a nadie. Este script
es la mitad del contrato: la otra mitad es el modulo que escribe el log.

LOS MODOS DE MORIR Y COMO SE DISTINGUEN
---------------------------------------
  CAIDA        hay una linea EXCEPTION, SIGNAL o CRASH. El proceso revento y el
               manejador llego a ejecutarse: se imprimen los frames y el volcado.
  COLGADO      no hay excepcion, pero el hilo de liveness hablo DESPUES del ultimo
               evento del runner de tests. El proceso vive y no avanza, y su ultimo
               TICK dice cuantos milisegundos lleva dentro del test.
  INTERRUMPIDO no hay excepcion ni latido posterior. Alguien mato el proceso
               (timeout del runner, cancelacion del job). El diagnostico es la
               marca de tiempo del ultimo latido comparada con la del corte.
  TERMINADA    hay RUN-END. El runner llego al final. Si ademas hay latidos
               posteriores, se colgo en el CIERRE (un destructor estatico que
               espera un hilo que ya no existe), no en un test.

La distincion entre CAIDA, COLGADO e INTERRUMPIDO es la que un humano NO puede
hacer leyendo la consola del gate, y las tres se manifiestan igual: "exit code 1"
sin una sola linea util.

LIMITE HONESTO DE LA ATRIBUCION
-------------------------------
Un cuelgue que mas tarde mata el timeout del job y una muerte por kill externo
dejan el MISMO log: los latidos se detienen porque el proceso dejo de existir, y
desde dentro no se puede observar la causa. Lo que este lector afirma no es "el
proceso se colgo" sino algo mas debil y mas cierto: "el proceso estaba
demostrablemente vivo en el ultimo latido, y ese latido nombra el test". Eso basta
para el trabajo de verdad, que es dejar un sujeto con nombre donde antes solo
habia un codigo de salida. Cuando el log no alcanza para afirmar nada, el informe
lo dice en vez de rellenar el hueco con una conjetura.

USO
---
    python tools/test_telemetry_report.py                    # log mas reciente
    python tools/test_telemetry_report.py <log> [<log> ...]  # logs concretos
    python tools/test_telemetry_report.py --dir <directorio>
    python tools/test_telemetry_report.py --selftest         # autoverificacion

Sin argumentos busca el .log mas reciente en %TEMP%/abdaudiolab-tests/runner, o
en la ruta que marque ABD_TEST_TELEMETRY_DIR.

CODIGO DE SALIDA
----------------
0 TERMINADA, 1 COLGADO o INTERRUMPIDO, 2 CAIDA, 3 no se pudo leer nada. Sirve
para que un gate distinga "tests rojos" de "runner muerto" sin parsear texto.
"""

import argparse
import glob
import os
import re
import sys
import tempfile
from datetime import datetime, timezone

# Configurar salida segura UTF-8 en consolas Windows (la de este proyecto es cp1252).
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

LOG_PREFIX = "run-"
LOG_EXTENSION = ".log"

# Una linea de telemetria es:
#   <marca ISO 8601 UTC> pid=<n> <CLAVE> clave=valor clave="valor con espacios"
#
# La marca y el pid los escribe emit() y su formato no depende de lo que pase el
# llamante, asi que son parseables. La clave es el token que sigue al pid.
LINE_RE = re.compile(
    r'^(?P<stamp>\S+)\s+pid=(?P<pid>\d+)\s+(?P<kind>[A-Z][A-Z0-9-]*)\s*(?P<rest>.*)$'
)

# Los valores van entrecomillados cuando contienen espacios y sin ellos cuando no.
# Parsear a mano en vez de con shlex es deliberado: sh lexeriza una ruta de Windows
# con barras invertidas, y una linea de log no es una linea de shell.
VALUE_RE = re.compile(
    r'(?P<key>[A-Za-z_][A-Za-z0-9_]*)=(?:"(?P<quoted>[^"]*)"|(?P<bare>\S*))'
)

# Kinds que demuestran que el proceso revento en vez de morir sin hablar.
CRASH_KINDS = ("EXCEPTION", "SIGNAL", "CRASH", "RUNNER-FATAL")

GREEN = "\033[92m"
AMBER = "\033[93m"
RED = "\033[91m"
CYAN = "\033[96m"
BOLD = "\033[1m"
RESET = "\033[0m"

# El color se apaga solo si la salida no es una terminal: en un job de CI el log
# acaba en un fichero y los codigos de escape son ruido que nadie lee.
COLOR = sys.stdout.isatty()

# Orden de gravedad, de menos a mas. El informe de varios logs se queda con el mas
# grave, que es el unico que explica por que el gate fallo.
SEVERITY = {"TERMINADA": 0, "COLGADO": 1, "INTERRUMPIDO": 2, "CAIDA": 3}

VERDICT_COLOR = {
    "CAIDA": RED,
    "COLGADO": AMBER,
    "INTERRUMPIDO": AMBER,
    "TERMINADA": GREEN,
}


def print_status(message, color=CYAN, is_bold=False):
    if not COLOR:
        print(message)
        return

    prefix = BOLD if is_bold else ""
    print(f"{color}{prefix}{message}{RESET}")


# ─────────────────────────────────────────────────────────────────────────────
# Parseo
# ─────────────────────────────────────────────────────────────────────────────

def parse_attributes(rest):
    """Convierte `a=1 b="x y" c=` en {'a': '1', 'b': 'x y', 'c': ''}."""
    attributes = {}

    for match in VALUE_RE.finditer(rest):
        quoted = match.group("quoted")
        attributes[match.group("key")] = quoted if quoted is not None else match.group("bare")

    return attributes


def parse_line(line):
    match = LINE_RE.match(line)

    if match is None:
        return None

    return {
        "stamp": match.group("stamp"),
        "pid": match.group("pid"),
        "kind": match.group("kind"),
        "attributes": parse_attributes(match.group("rest")),
    }


def parse_timestamp(stamp):
    """ISO 8601 con 'Z'. Devuelve None si no se puede leer: un reloj raro no debe
    tumbar el informe que existe para diagnosticar precisamente un fallo raro."""
    try:
        return datetime.strptime(stamp, "%Y-%m-%dT%H:%M:%S.%fZ").replace(tzinfo=timezone.utc)
    except ValueError:
        return None


def read_events(path):
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        return [
            event
            for event in (parse_line(raw.rstrip("\n")) for raw in handle)
            if event is not None
        ]


# ─────────────────────────────────────────────────────────────────────────────
# Modelado de la corrida
# ─────────────────────────────────────────────────────────────────────────────

def slice_current_run(events):
    """Se queda solo con lo que va despues del ULTIMO RUNNER-START.

    El nombre del log lleva un indice de corrida, asi que el caso normal es un
    unico RUNNER-START. Si el fichero se recoloca a mano o dos procesos comparten
    nombre, este recorte evita mezclar el desenlace de una corrida con la apertura
    de otra: dos finales y dos veredictos en el mismo informe no dicen nada.
    """
    starts = [index for index, event in enumerate(events) if event["kind"] == "RUNNER-START"]

    if not starts:
        return events

    return events[starts[-1]:]


def build_timeline(events):
    """Empareja CASE-START con CASE-END y SECTION-START con SECTION-END.

    Una entrada al test se empareja con su salida por POSICION, no por nombre: un
    test con secciones entra en el cuerpo una vez por hoja del arbol, y emparejar
    por nombre perderia el desajuste justo en el caso interesante, que es una entrada
    mas de las que tienen salida.

    Los indices se guardan, no solo los valores, porque la distincion COLGADO contra
    INTERRUMPIDO es temporal: hace falta saber si el hilo de liveness hablo DESPUES
    del ultimo evento del runner, no solo si dijo algo en algun momento.
    """
    case_stack = []
    section_stack = []
    last_case = None
    last_tick_index = None
    last_test_event_index = None
    run_end_index = None
    crash = None
    frames = []
    slow = []
    runner_start = None

    for index, event in enumerate(events):
        kind = event["kind"]
        attributes = event["attributes"]

        if kind == "RUNNER-START":
            runner_start = event

        elif kind == "CASE-START":
            entry = {
                "stamp": event["stamp"],
                "part": attributes.get("part", "?"),
                "name": attributes.get("name", "(sin nombre)"),
                "at": attributes.get("at", "?"),
                "tags": attributes.get("tags", ""),
            }
            case_stack.append(entry)
            last_case = entry
            section_stack = []
            last_test_event_index = index

        elif kind == "CASE-END":
            if case_stack:
                case_stack.pop()
            section_stack = []
            last_test_event_index = index

        elif kind == "SECTION-START":
            # Catch2 avisa de cada seccion al entrar en ella, y las internas despues
            # que las externas: la pila es la ruta completa, no solo la hoja.
            section_stack.append(attributes.get("name", "(sin nombre)"))

        elif kind == "SECTION-END":
            if section_stack:
                section_stack.pop()

        elif kind == "TICK":
            last_tick_index = index

        elif kind == "RUN-END":
            run_end_index = index

        elif kind in CRASH_KINDS:
            if crash is None:
                crash = event

        elif kind == "FRAME":
            frames.append(attributes)

        elif kind == "SLOW":
            slow.append(attributes)

    return {
        "runner_start": runner_start,
        "unclosed_case": case_stack[-1] if case_stack else None,
        "last_case": last_case,
        "unclosed_sections": list(section_stack),
        "last_tick": events[last_tick_index] if last_tick_index is not None else None,
        # Ultimo TICK que es POSTERIOR al ultimo evento del runner de tests.
        "ticking_after_tests": (
            last_tick_index is not None
            and (last_test_event_index is None or last_tick_index > last_test_event_index)
        ),
        "crash": crash,
        "frames": frames,
        "slow": slow,
        "run_end": events[run_end_index] if run_end_index is not None else None,
        "ticking_after_run_end": (
            run_end_index is not None
            and last_tick_index is not None
            and last_tick_index > run_end_index
        ),
    }


def classify(model):
    """Decide el veredicto. Va separado del imprime porque es el nucleo del script
    y porque tools/test_test_telemetry_report.py lo ejercita con los cuatro modos."""
    if model["crash"] is not None:
        return "CAIDA", model["crash"]["kind"]

    if model["run_end"] is not None:
        if model["ticking_after_run_end"]:
            # Todos los tests pasaron pero el proceso no se fue: cuelgue en el
            # cierre. Con JUCE y hilos sin join eso pasa, y el sintoma sigue siendo
            # "exit code 1" sin asercion ninguna.
            return "TERMINADA", "RUN-END seguido de latidos: colgado en el cierre"
        return "TERMINADA", "RUN-END"

    if model["ticking_after_tests"]:
        return "COLGADO", "latido posterior al ultimo evento del runner"

    return "INTERRUMPIDO", "sin excepcion ni latido posterior"


# ─────────────────────────────────────────────────────────────────────────────
# Informe
# ─────────────────────────────────────────────────────────────────────────────

def describe_attribution(model):
    """El sujeto: el test sin cerrar y, dentro de el, la seccion sin cerrar. Es la
    atribucion que se pedia.

    El "ninguno" cambia de significado segun el veredicto, y confundirlos seria
    mentir en la direccion contraria: en una corrida TERMINADA todos los tests
    cerraron, asi que no hay sujeto porque no hay fallo que atribuir, no porque el
    proceso murio entre dos. En una corrida sin RUN-END, en cambio, no tener sujeto
    es exactamente la noticia: el proceso murio con la sesion cerrada y sin un
    culprit al que mirar."""
    case = model["unclosed_case"]
    lines = []

    if case is not None:
        lines.append(f"    {'test':<9}: {case['name']}")
        lines.append(f"    {'origen':<9}: {case['at']}")

        if case["tags"]:
            lines.append(f"    {'etiquetas':<9}: {case['tags']}")

        lines.append(
            f"    {'entrada':<9}: parte {case['part']} (una entrada por hoja del arbol de secciones)"
        )
    elif model["run_end"] is not None:
        lines.append(
            f"    {'test':<9}: ninguno sin cerrar, que es lo esperado: la corrida termino"
        )
    else:
        lines.append(
            f"    {'test':<9}: ninguno sin cerrar; el proceso murio ENTRE tests, con la"
            f"\n{'':>15}sesion cerrada y sin un sujeto al que atribuirlo"
        )

    if model["unclosed_sections"]:
        lines.append(f"    {'seccion':<9}: {' / '.join(model['unclosed_sections'])}")

    return lines


def describe_heartbeat(model):
    """El ultimo latido cuantifica un cuelgue: sin el, COLGADO es una palabra; con
    el, es "lleva 812 s dentro de este test"."""
    tick = model["last_tick"]

    if tick is None:
        return []

    attributes = tick["attributes"]
    lines = [
        f"    {'hora':<9}: {tick['stamp']} (UTC)",
        f"    {'proceso':<9}: {attributes.get('uptime_ms', '?')} ms desde el arranque",
        f"    {'test':<9}: {attributes.get('case') or '(ninguno)'}"
        f" ({attributes.get('case_ms', '?')} ms)",
        f"    {'seccion':<9}: {attributes.get('section') or '(ninguna)'}",
    ]

    return lines


def describe_run_length(model):
    """Cuanto lived la corrida. Sin esto el informe dice QUIEN pero no si el fallo
    lleva un segundo o veinte minutos, que son diagnosticos opuestos."""
    if model["runner_start"] is None or model["last_tick"] is None:
        return []

    started = parse_timestamp(model["runner_start"]["stamp"])
    ended = parse_timestamp(model["last_tick"]["stamp"])

    if started is None or ended is None:
        return []

    seconds = (ended - started).total_seconds()

    return [f"  {'duracion':<9}: {seconds:.1f} s hasta el ultimo latido"]


def recommendation(verdict, model, reason):
    dump = ""
    if model["runner_start"] is not None:
        dump = model["runner_start"]["attributes"].get("dump", "(sin registro)")

    if verdict == "CAIDA":
        return (
            f"Abrir el volcado y ejecutar `!analyze -v {dump}` en WinDbg, o cargarlo\n"
            "    con Visual Studio. Si el volcado no se pudo escribir, el par case/section\n"
            "    del veredicto sigue siendo la atribucion. El frame que importa es el\n"
            "    PRIMERO de la lista, no el ultimo: el ultimo es el manejador."
        )

    if verdict == "COLGADO":
        return (
            "El proceso estaba vivo y no avanzaba. Mirar los TESTS MAS LENTOS de este\n"
            "    mismo log: un unico test con un orden de magnitud sobre el resto es el\n"
            "    candidato, y los milisegundos del ultimo latido dicen cuanto llevaba\n"
            "    colgado. Si ninguno destaca, el cuelgue es de fondo de pila y hace falta\n"
            "    un volcado en vivo, no post mortem."
        )

    if verdict == "TERMINADA":
        if "cierre" in reason:
            return (
                "Todos los tests pasaron y el proceso se quedo vivo despues del RUN-END.\n"
                "    Eso no es un test lento: es un cuelgue en el cierre (destructor estatico\n"
                "    esperando a un hilo, o MessageManager sin apagar). Las aserciones estan\n"
                "    en verde, asi que el gate falla por el codigo de salida, no por un test."
            )

        return (
            "La corrida llego al final. Si el gate falla, el fallo es de aserciones, y las\n"
            "    tiene el reporte de Catch2, no este log."
        )

    return (
        "No hay excepcion ni latido posterior al ultimo evento del runner: el proceso\n"
        "    murio antes de que el hilo de liveness tuviera un turno mas (5 s). Puede ser\n"
        "    un kill externo o un fallo que tumbara tambien ese hilo. Si el ultimo test\n"
        "    abierto es el primero de su fichero, sospechar del fixture, no del cuerpo:\n"
        "    ahi no ha corrido todavia ningun codigo de prueba."
    )


def print_report(path):
    events = slice_current_run(read_events(path))
    model = build_timeline(events)
    verdict, reason = classify(model)

    print_status("=" * 78, CYAN)
    print_status(f"TELEMETRIA DEL RUNNER  {os.path.basename(path)}", CYAN, True)
    print_status("=" * 78, CYAN)

    if model["runner_start"] is not None:
        attributes = model["runner_start"]["attributes"]
        print_status(f"  {'dump':<9}: {attributes.get('dump', '(sin registro)')}")
        print_status(f"  {'latido':<9}: cada {attributes.get('tickMs', '?')} ms")
    else:
        print_status(f"  {'aviso':<9}: sin RUNNER-START; se atribuye igual", AMBER)

    for line in describe_run_length(model):
        print_status(line)

    print_status("")
    print_status(f"VEREDICTO: {verdict}  ({reason})", VERDICT_COLOR[verdict], True)
    print_status("")

    print_status("ATRIBUCION", BOLD)
    for line in describe_attribution(model):
        print_status(line)

    crash = model["crash"]
    if crash is not None:
        for key in ("code", "address", "number", "text", "case", "section"):
            value = crash["attributes"].get(key)
            if value:
                print_status(f"    {key:<9}: {value}")

    heartbeat = describe_heartbeat(model)
    if heartbeat:
        print_status("")
        print_status("ULTIMO LATIDO", BOLD)
        for line in heartbeat:
            print_status(line)

    run_end = model["run_end"]
    if run_end is not None:
        attributes = run_end["attributes"]
        print_status("")
        print_status("FIN DE LA CORRIDA", BOLD)
        print_status(f"    {'duracion':<9}: {int(attributes.get('ms', 0) or 0) / 1000.0:.1f} s")
        print_status(f"    {'aserciones':<9}: {attributes.get('assertions', '?')}")
        print_status(f"    {'casos':<9}: {attributes.get('testCases', '?')}")
        print_status(f"    {'abortando':<9}: {attributes.get('aborting', '?')}")

    frames = model["frames"]
    if frames:
        print_status("")
        print_status(f"PILA ({len(frames)} frames)", BOLD)
        for frame in frames[:12]:
            print_status(
                f"    {frame.get('index', '?'):>3}"
                f"  {frame.get('address', '?')}"
                f"  {frame.get('module', '?')}+{frame.get('rva', '0x0')}"
            )

    slow = model["slow"]
    if slow:
        print_status("")
        print_status("TESTS MAS LENTOS", BOLD)
        for index, entry in enumerate(slow):
            milliseconds = int(entry.get("ms", 0) or 0)
            print_status(
                f"    {index:>2}. {milliseconds / 1000.0:>8.2f} s  {entry.get('name', '?')}"
            )

    print_status("")
    print_status("SIGUIENTE PASO", BOLD)
    print_status(recommendation(verdict, model, reason))
    print_status("")

    return verdict


# ─────────────────────────────────────────────────────────────────────────────
# Descubrimiento
# ─────────────────────────────────────────────────────────────────────────────

def default_directory():
    override = os.environ.get("ABD_TEST_TELEMETRY_DIR")
    if override:
        return override

    temp = os.environ.get("TEMP") or os.environ.get("TMP") or "."
    return os.path.join(temp, "abdaudiolab-tests", "runner")


def discover_logs(directory):
    pattern = os.path.join(directory, LOG_PREFIX + "*" + LOG_EXTENSION)
    return sorted(glob.glob(pattern), key=os.path.getmtime)


def main():
    parser = argparse.ArgumentParser(
        description="Atribuye la muerte silenciosa del runner a un test concreto."
    )
    parser.add_argument("logs", nargs="*", help="ficheros de telemetria a leer")
    parser.add_argument("--dir", help="directorio donde buscar el log mas reciente")
    parser.add_argument("--latest", action="store_true", help="leer solo el mas reciente")
    parser.add_argument(
        "--selftest",
        action="store_true",
        help="comprobar la atribucion con logs sinteticos y salir",
    )
    arguments = parser.parse_args()

    if arguments.selftest:
        return selftest()

    paths = list(arguments.logs)

    if not paths or arguments.latest:
        directory = arguments.dir or default_directory()
        discovered = discover_logs(directory)

        if not discovered:
            print_status(f"no hay telemetria en {directory}", RED, True)
            print_status(
                "  Si la suite corrio con otro ABD_TEST_TELEMETRY_DIR, pasalo con --dir."
            )
            return 3

        paths = [discovered[-1]]

    worst = "TERMINADA"
    read_any = False

    for path in paths:
        if not os.path.isfile(path):
            print_status(f"no existe: {path}", RED, True)
            continue

        try:
            verdict = print_report(path)
        except OSError as error:
            print_status(f"no se pudo leer {path}: {error}", RED, True)
            continue

        read_any = True

        if SEVERITY[verdict] > SEVERITY[worst]:
            worst = verdict

    if not read_any:
        return 3

    return {"TERMINADA": 0, "COLGADO": 1, "INTERRUMPIDO": 1, "CAIDA": 2}[worst]


# ─────────────────────────────────────────────────────────────────────────────
# Autoverificacion
# ─────────────────────────────────────────────────────────────────────────────

# Los cuatro modos de muerte, con la forma exacta de linea que emite
# TestTelemetry.cpp. No son logarrafos inventados: si el modulo cambia el formato,
# estas lineas dejan deraticar el parser en el sentido contrario.
SYNTHETIC_LOGS = {
    "CAIDA": [
        '2026-10-01T10:00:00.000Z pid=4242 RUNNER-START log="run-4242-1.log" dump="run-4242-1.dmp" tickMs=5000',
        '2026-10-01T10:00:00.010Z pid=4242 RUN-START name="Gate 6 ~[ves]" total=318',
        '2026-10-01T10:00:00.100Z pid=4242 CASE-START part=0 name="VES Cz-101 envelope sweep" at=src/tests/test_VesCz101Envelope.cpp:210 tags=[ves][audio]',
        '2026-10-01T10:00:00.110Z pid=4242 SECTION-START name="attack"',
        '2026-10-01T10:00:05.005Z pid=4242 TICK alive=yes uptime_ms=5005 case="VES Cz-101 envelope sweep" section="attack" case_ms=4905',
        '2026-10-01T10:00:12.310Z pid=4242 EXCEPTION code=0xC0000005 address=00007FF6A1B2C3D4 case="VES Cz-101 envelope sweep" section="attack"',
        '2026-10-01T10:00:12.320Z pid=4242 CRASH case="VES Cz-101 envelope sweep" section="attack"',
        '2026-10-01T10:00:12.330Z pid=4242 FRAMES captured=24',
        '2026-10-01T10:00:12.340Z pid=4242 FRAME index=0 address=0x00007FF6A1B2C3D4 module=ABDAudioLab_Tests.exe rva=0x1B2C3D4',
        '2026-10-01T10:00:12.350Z pid=4242 FRAME index=1 address=0x00007FF6B2C40000 module=juce_audio_processors.dll rva=0x98A120',
        '2026-10-01T10:00:12.360Z pid=4242 DUMP writing="run-4242-1.dmp"',
        '2026-10-01T10:00:13.900Z pid=4242 DUMP written=yes',
    ],
    "COLGADO": [
        '2026-10-01T11:00:00.000Z pid=5150 RUNNER-START log="run-5150-1.log" dump="run-5150-1.dmp" tickMs=5000',
        '2026-10-01T11:00:00.010Z pid=5150 RUN-START name="Gate 6 ~[ves]" total=318',
        '2026-10-01T11:00:00.100Z pid=5150 CASE-START part=2 name="Live scan waits for buffer" at=src/tests/test_LiveScan.cpp:88 tags=[ves]',
        '2026-10-01T11:00:00.110Z pid=5150 SECTION-START name="escaneo en vivo"',
        '2026-10-01T11:08:00.000Z pid=5150 TICK alive=yes uptime_ms=480000 case="Live scan waits for buffer" section="escaneo en vivo" case_ms=479900',
        '2026-10-01T11:13:00.000Z pid=5150 TICK alive=yes uptime_ms=780000 case="Live scan waits for buffer" section="escaneo en vivo" case_ms=779900',
    ],
    # Interrumpido = el proceso murio sin que el hilo de liveness llegara a hablar
    # desde el ultimo evento del runner. Aqui el ultimo TICK es ANTERIOR al
    # CASE-START, no posterior: si fuera posterior, el proceso estaria
    # demostrablemente vivo y el veredicto correcto seria COLGADO. Esa es
    # precisamente la distincion que hace el script, y por eso el fixture tiene que
    # cumplirla o no estaria probando nada.
    "INTERRUMPIDO": [
        '2026-10-01T12:00:00.000Z pid=6161 RUNNER-START log="run-6161-1.log" dump="run-6161-1.dmp" tickMs=5000',
        '2026-10-01T12:00:00.010Z pid=6161 RUN-START name="Gate 6 ~[ves]" total=318',
        '2026-10-01T12:00:00.050Z pid=6161 TICK alive=yes uptime_ms=50 case="" section="" case_ms=0',
        '2026-10-01T12:00:00.100Z pid=6161 CASE-START part=0 name="Guided vs classic parity" at=src/tests/test_Integration01GuidedVsClassicAudio.cpp:640 tags=[ves][integration]',
        '2026-10-01T12:00:00.110Z pid=6161 SECTION-START name="bombeo"',
    ],
    # El caso limite del mismo veredicto: el proceso murio ENTRE tests, con la
    # corrida empezada y ningun test abierto. No hay sujeto al que atribuirlo, y
    # el informe tiene que decirlo en vez de inventar un culpable.
    "MUERTO_ENTRE_TESTS": [
        '2026-10-01T12:30:00.000Z pid=6262 RUNNER-START log="run-6262-1.log" dump="run-6262-1.dmp" tickMs=5000',
        '2026-10-01T12:30:00.010Z pid=6262 RUN-START name="Gate 6 ~[ves]" total=318',
        '2026-10-01T12:30:00.100Z pid=6262 CASE-START part=0 name="Un test corto" at=a.cpp:1 tags=[ves]',
        '2026-10-01T12:30:00.400Z pid=6262 CASE-END part=0 ms=300 assertions=4 ok=1',
    ],
    "TERMINADA": [
        '2026-10-01T13:00:00.000Z pid=7172 RUNNER-START log="run-7172-1.log" dump="run-7172-1.dmp" tickMs=5000',
        '2026-10-01T13:00:00.010Z pid=7172 RUN-START name="Gate 6 ~[ves]" total=2',
        '2026-10-01T13:00:00.100Z pid=7172 CASE-START part=0 name="Primer test" at=a.cpp:1 tags=[ves]',
        '2026-10-01T13:00:00.900Z pid=7172 CASE-END part=0 ms=800 assertions=12 ok=1',
        '2026-10-01T13:00:01.000Z pid=7172 CASE-START part=0 name="Segundo test" at=b.cpp:2 tags=[ves]',
        '2026-10-01T13:00:01.400Z pid=7172 CASE-END part=0 ms=400 assertions=4 ok=1',
        '2026-10-01T13:00:01.500Z pid=7172 RUN-END ms=1490 assertions=16 testCases=2 aborting=0',
        '2026-10-01T13:00:01.510Z pid=7172 SLOWEST count=2',
        '2026-10-01T13:00:01.511Z pid=7172 SLOW ms=800 name="Primer test"',
        '2026-10-01T13:00:01.512Z pid=7172 SLOW ms=400 name="Segundo test"',
    ],
    "COLGADO_EN_CIERRE": [
        '2026-10-01T14:00:00.000Z pid=8183 RUNNER-START log="run-8183-1.log" dump="run-8183-1.dmp" tickMs=5000',
        '2026-10-01T14:00:00.010Z pid=8183 RUN-START name="Gate 6 ~[ves]" total=1',
        '2026-10-01T14:00:00.100Z pid=8183 CASE-START part=0 name="Un test verde" at=a.cpp:1 tags=[ves]',
        '2026-10-01T14:00:00.500Z pid=8183 CASE-END part=0 ms=400 assertions=12 ok=1',
        '2026-10-01T14:00:00.600Z pid=8183 RUN-END ms=590 assertions=12 testCases=1 aborting=0',
        '2026-10-01T14:00:05.010Z pid=8183 TICK alive=yes uptime_ms=5010 case="" section="" case_ms=0',
    ],
}

# Que tienen que decir el veredicto y la atribucion en cada escenario. Es el
# contrato que el modulo de C++ y este lector comparten, escrito como asercion y no
# como prosa: si el formato de una linea cambia, esta tabla es la que se rompe
# primero, y esa es la intencion.
#
# scenarios que comparten veredicto se distinguen por el sujeto (test + seccion).
SELFTEST_EXPECTATIONS = {
    "CAIDA": ("CAIDA", "VES Cz-101 envelope sweep", "attack"),
    "COLGADO": ("COLGADO", "Live scan waits for buffer", "escaneo en vivo"),
    "INTERRUMPIDO": ("INTERRUMPIDO", "Guided vs classic parity", "bombeo"),
    "MUERTO_ENTRE_TESTS": ("INTERRUMPIDO", None, None),
    "TERMINADA": ("TERMINADA", None, None),
    "COLGADO_EN_CIERRE": ("TERMINADA", None, None),
}


def selftest():
    """Comprueba la atribucion sobre logs sinteticos de los cinco escenarios.

    Vive dentro de la herramienta y no en un test aparte porque su valor esta en
    poder ejecutarse sin la suite compilada: en este repositorio el ejecutable de
    tests no siempre esta disponible, y justamente cuando no lo esta es cuando hace
    falta poder confiar en el lector.
    """
    failures = []

    with tempfile.TemporaryDirectory() as directory:
        for scenario, lines in SYNTHETIC_LOGS.items():
            path = os.path.join(directory, f"{LOG_PREFIX}selftest-{scenario}{LOG_EXTENSION}")

            with open(path, "w", encoding="utf-8") as handle:
                handle.write("\n".join(lines) + "\n")

            model = build_timeline(slice_current_run(read_events(path)))
            verdict, reason = classify(model)
            expected_verdict, expected_case, expected_section = SELFTEST_EXPECTATIONS[scenario]

            if verdict != expected_verdict:
                failures.append(f"{scenario}: veredicto {verdict} en vez de {expected_verdict}")
                continue

            case = model["unclosed_case"]
            actual_case = case["name"] if case else None

            if actual_case != expected_case:
                failures.append(f"{scenario}: atribucion {actual_case!r} en vez de {expected_case!r}")

            sections = " / ".join(model["unclosed_sections"]) or None

            if sections != expected_section:
                failures.append(f"{scenario}: seccion {sections!r} en vez de {expected_section!r}")

            if scenario == "COLGADO" and model["last_tick"]["attributes"]["case_ms"] != "779900":
                failures.append("COLGADO: el ultimo latido no reporta los ms del caso")

            if scenario == "CAIDA" and len(model["frames"]) != 2:
                failures.append(f"CAIDA: {len(model['frames'])} frames en vez de 2")

            if scenario == "TERMINADA" and len(model["slow"]) != 2:
                failures.append("TERMINADA: la tabla de lentos no se reconstruye")

            if scenario == "COLGADO_EN_CIERRE" and "cierre" not in reason:
                failures.append(f"COLGADO_EN_CIERRE: el motivo {reason!r} no menciona el cierre")

            if scenario == "MUERTO_ENTRE_TESTS" and model["unclosed_case"] is not None:
                failures.append("MUERTO_ENTRE_TESTS: se invento un sujeto al que atribuir")

            print_status(f"  {scenario:<18} -> {verdict}", VERDICT_COLOR.get(expected_verdict, CYAN))

    if failures:
        print_status("")
        for failure in failures:
            print_status(f"  FALLO: {failure}", RED)
        return 1

    print_status("")
    print_status(
        f"selftest correcto en los {len(SYNTHETIC_LOGS)} escenarios", GREEN, True
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())