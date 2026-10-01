/**
 * @file TestTelemetry.cpp
 * @brief Implementacion de la telemetria del runner (ver TestTelemetry.h).
 * @author ABDSynths
 * @date 2026
 */

#include "TestTelemetry.h"

#include <catch2/catch_test_case_info.hpp>
#include <catch2/catch_section_info.hpp>
#include <catch2/interfaces/catch_interfaces_reporter.hpp>
#include <catch2/internal/catch_test_case_registry_impl.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
// dbghelp va DESPUES de windows.h: al reves, el macro de min/max de Windows rompe
// las plantillas de la STL que MiniDumpWriteDump necesita.
#include <dbghelp.h>
#else
#include <csignal>
#include <execinfo.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace abdaudiolab::test::telemetry
{
namespace
{

constexpr std::size_t kMaxFrames = 62;
constexpr std::size_t kMaxNameChars = 512;
constexpr std::size_t kSlowestToReport = 20;
constexpr int kMaxRunIndex = 10000;

/** @brief Reloj monotono en milisegundos. Sirve para medir, no para fechar. */
long long monotonicMs() noexcept
{
#if defined(_WIN32)
    return static_cast<long long> (::GetTickCount64());
#else
    return std::chrono::duration_cast<std::chrono::milliseconds> (
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Recorder
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Dos DESCRIPTORES sobre el mismo fichero, no uno.
 *
 * El camino normal usa un mutex porque escriben varios hilos a la vez. El camino
 * de caida NO puede tomar ese mutex: si el proceso revienta mientras lo mantiene,
 * quedarse a la espera seria un segundo interbloqueo dentro del manejador de una
 * excepcion que ya va mal. Por eso son dos FILE* separados, ambos en modo append,
 * y el de caida nunca se bloquea. Una linea de mas o de menos en el volcado es
 * irrelevante; un volcado que no llega a escribirse es irrelevante entero.
 *
 * Los dos FILE* son el MISMO fichero pero con buffers INDEPENDIENTES: por eso
 * emitLocked() pasa el formato ya formateado a emit() con "%s" en vez de
 * formatear dos veces con dos va_list sobre el mismo formato.
 */
struct Recorder
{
    std::mutex mutex;
    std::FILE* normal { nullptr };
    std::FILE* crash { nullptr };
    std::string logPath;
    std::string dumpPath;
    std::atomic<bool> active { false };

    // Contexto para el manejador de caida, que lo lee con try_lock y no con
    // lock. Es un par de buffers fijos, no std::string: publicar un nombre de test
    // no puede fallar por falta de memoria.
    char currentCase[kMaxNameChars] { "" };
    char currentSection[kMaxNameChars] { "" };
    std::atomic<long long> caseStartedMs { 0 };
    std::atomic<long long> runStartedMs { 0 };
};

// Objeto de ambito de fichero, NO local estatico: el manejador de caida llama a
// recorder() en un estado del proceso en el que no se puede garantizar que el
// guard de inicializacion perezosa de un local estatico se haya resuelto sin
// bloquear el loader. Un objeto de ambito de fichero ya esta construido de aqui a
// que empiece main(), y el mutex es constexpr-construible.
Recorder g_recorder;

Recorder& recorder() noexcept
{
    return g_recorder;
}

/** @brief Marca de tiempo UTC con milisegundos: ordenable y parseable sin librerias. */
void utcStamp (char* out, const std::size_t n) noexcept
{
    const auto now = std::chrono::system_clock::now();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds> (
                            now - std::chrono::time_point_cast<std::chrono::seconds> (now))
                            .count();

    const auto raw = std::chrono::system_clock::to_time_t (now);
    std::tm broken {};

#if defined(_WIN32)
    ::gmtime_s (&broken, &raw);
#else
    ::gmtime_r (&raw, &broken);
#endif

    std::snprintf (out, n, "%04d-%02d-%02dT%02d:%02d:%02d.%03lldZ",
                   broken.tm_year + 1900, broken.tm_mon + 1, broken.tm_mday,
                   broken.tm_hour, broken.tm_min, broken.tm_sec,
                   static_cast<long long> (millis));
}

/** @brief Escribe una linea ya formateada y la descarga. Sin asignaciones. */
void emit (std::FILE* stream, const char* format, ...) noexcept
{
    if (stream == nullptr)
        return;

    char stamp[40];
    utcStamp (stamp, sizeof (stamp));

    const auto pid = static_cast<int> (
#if defined(_WIN32)
        ::GetCurrentProcessId()
#else
        ::getpid()
#endif
    );

    std::fprintf (stream, "%s pid=%d ", stamp, pid);

    va_list args;
    va_start (args, format);
    std::vfprintf (stream, format, args);
    va_end (args);

    std::fputc ('\n', stream);

    // Descargar en CADA linea es el punto: lo que sobrevivio a la caida es lo que
    // estaba en la pagina del fichero cuando el proceso murio. Un buffer de stdio
    // sin vaciar se pierde entero, y con el las ultimas horas de diagnostico.
    std::fflush (stream);
}

/** @brief Camino normal: con mutex. Si el mutex esta tomado, la linea se pierde. */
void emitLocked (const char* format, ...) noexcept
{
    auto& r = recorder();

    if (! r.active.load() || r.normal == nullptr)
        return;

    std::unique_lock<std::mutex> lock (r.mutex, std::try_to_lock);

    if (! lock.owns_lock())
        return;

    // 4096, no 1024: un nombre de test con sus tags cabe de sobra, pero el camino
    // es truncarse en silencio (vsnprintf no avisa) y un diagnostico truncado es
    // peor que uno ausente, porque parece completo.
    char stackBuffer[4096];

    va_list args;
    va_start (args, format);
    std::vsnprintf (stackBuffer, sizeof (stackBuffer), format, args);
    va_end (args);

    emit (r.normal, "%s", stackBuffer);
}

// ─────────────────────────────────────────────────────────────────────────────
// Contexto para el manejador de caida
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Copia el contexto actual a buffers fijos, con try_lock.
 *
 * Publicar el contexto NO puede fallar nunca. Si el mutex esta tomado (que
 * significa que otro hilo esta escribiendo justo ahora), el contexto publicado
 * se queda el que era: un test anterior como culpable es un diagnostico
 * equivocado, pero un manejador de excepcion que se cuelga esperando el mutex
 * es un proceso que no muere y del que CI no sabe nada.
 */
void publishContext (const char* caseName, const char* sectionPath) noexcept
{
    auto& r = recorder();

    if (! r.mutex.try_lock())
        return;

    std::snprintf (r.currentCase, kMaxNameChars, "%s", caseName != nullptr ? caseName : "");
    std::snprintf (r.currentSection, kMaxNameChars, "%s",
                   sectionPath != nullptr ? sectionPath : "");

    r.mutex.unlock();
}

/** @brief Lee el contexto publicado sin bloquear. Rellena "" si no se puede. */
void readContext (char* caseOut, const std::size_t caseChars,
                  char* sectionOut, const std::size_t sectionChars) noexcept
{
    auto& r = recorder();

    caseOut[0] = '\0';
    sectionOut[0] = '\0';

    if (! r.mutex.try_lock())
        return;

    std::snprintf (caseOut, caseChars, "%s", r.currentCase);
    std::snprintf (sectionOut, sectionChars, "%s", r.currentSection);

    r.mutex.unlock();
}

// ─────────────────────────────────────────────────────────────────────────────
// Manejador de caida
// ─────────────────────────────────────────────────────────────────────────────

void writeCrashFrames() noexcept
{
    auto& r = recorder();

    char where[kMaxNameChars] { "" };
    char section[kMaxNameChars] { "" };

    readContext (where, sizeof (where), section, sizeof (section));

    if (where[0] == '\0')
        std::snprintf (where, sizeof (where), "(contexto no publicado: el mutex lo tenia otro)");

    emit (r.crash, "CRASH case=\"%s\" section=\"%s\"", where, section);

#if defined(_WIN32)
    void* frames[kMaxFrames] = {};
    const auto captured = static_cast<int> (
        ::CaptureStackBackTrace (0, static_cast<DWORD> (kMaxFrames), frames, nullptr));

    emit (r.crash, "FRAMES captured=%d", captured);

    // IMAGEHLP_MODULEW64 pesa ~2,5 kB porque incluye CVData[MAX_PATH*3]. Va en
    // estatico de ambito de fichero y no en la pila: el manejador de una excepcion
    // de acceso invalido puede tener la pila ya tocada, y IMAGEHLP_MODULE64 esta
    // constante-construido (todo ceros), asi que no hay guardia de inicializacion.
    static IMAGEHLP_MODULEW64 module {};

    for (int i = 0; i < captured; ++i)
    {
        const auto address = reinterpret_cast<DWORD64> (frames[i]);

        // SymGetModuleInfo64 esta redefinido a la variante W bajo UNICODE, asi que
        // se llama a SymGetModuleInfoW64 por su nombre explicito y se imprime con
        // %ls. Pedir la variante ANSI con el nombre corto daria un puntero a
        // wchar_t printf con %s, y en el mejor caso "%ls" residual.
        const wchar_t* moduleName = nullptr;
        unsigned long long rva = 0;

        std::memset (&module, 0, sizeof (module));
        module.SizeOfStruct = sizeof (module);

        // SymGetModuleInfoW64 escribe en un buffer del llamante: no reserva
        // memoria, que es justo lo que se le pide a codigo que corre dentro de una
        // excepcion.
        if (::SymGetModuleInfoW64 (::GetCurrentProcess(), address, &module) != 0
             && module.ModuleName[0] != L'\0')
        {
            moduleName = module.ModuleName;

            // ImageSize es el tamano en memoria, pero el offset que se le pide a
            // Visual Studio y a WinDbg es la RVA: la direccion menos la base de
            // carga, no menos la direccion del fichero en disco.
            rva = static_cast<unsigned long long> (address - module.BaseOfImage);
        }

        // Todo en forma clave=valor, tambien en el frame. El lector de telemetria
        // parsea pares, no posiciones: un "#02" en columna fija obliga a quien lee
        // el log a contar caracteres, y el frame #0 es justo el que no se puede
        // perder. %ls sobre un FILE* estrecho convierte a la configuracion regional
        // vigente, y un nombre de modulo es ASCII.
        emit (r.crash, "FRAME index=%d address=0x%016llX module=%ls rva=0x%llX", i,
              static_cast<unsigned long long> (address),
              moduleName != nullptr ? moduleName : L"??", rva);
    }

    // El volcado lo escribe el SO con su propio empuje. Aqui no se symboliza:
    // resolver nombres reserva memoria, y reservar memoria en un manejador de
    // excepcion es la forma mas fiable de convertir un fallo en un cuelgue.
    emit (r.crash, "DUMP writing=\"%s\"", r.dumpPath.c_str());

    const HANDLE file = ::CreateFileA (r.dumpPath.c_str(), GENERIC_WRITE, 0, nullptr,
                                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    bool ok = false;

    if (file != INVALID_HANDLE_VALUE)
    {
        const auto written = ::MiniDumpWriteDump (::GetCurrentProcess(), ::GetCurrentThreadId(),
                                                  file, static_cast<MINIDUMP_TYPE> (
                                                      MiniDumpWithDataSegs
                                                      | MiniDumpWithHandleData
                                                      | MiniDumpWithThreadInfo
                                                      | MiniDumpWithIndirectlyReferencedMemory),
                                                  nullptr, nullptr, nullptr);
        ok = (written != FALSE);
        ::CloseHandle (file);
    }

    emit (r.crash, "DUMP written=%s", ok ? "yes" : "no");
    emit (r.crash, "DUMP-HINT dump=\"%s\" comando=\"!analyze -v %s\"", r.dumpPath.c_str(),
          r.dumpPath.c_str());
#else
    void* frames[kMaxFrames] = {};
    const auto captured = ::backtrace (frames, static_cast<int> (kMaxFrames));

    emit (r.crash, "FRAMES captured=%d", captured);

    for (int i = 0; i < captured; ++i)
        emit (r.crash, "FRAME index=%d address=%p module=? rva=0x0", i, frames[i]);
#endif
}

#if defined(_WIN32)
LONG WINAPI onUnhandledException (EXCEPTION_POINTERS* info) noexcept
{
    if (recorder().crash != nullptr)
    {
        const auto code = (info != nullptr && info->ExceptionRecord != nullptr)
                        ? info->ExceptionRecord->ExceptionCode
                        : 0;
        const auto addr = (info != nullptr && info->ExceptionRecord != nullptr)
                        ? info->ExceptionRecord->ExceptionAddress
                        : nullptr;

        char where[kMaxNameChars] { "" };
        char section[kMaxNameChars] { "" };

        readContext (where, sizeof (where), section, sizeof (section));

        emit (recorder().crash,
              "EXCEPTION code=0x%08lX address=%p case=\"%s\" section=\"%s\"",
              static_cast<unsigned long> (code), addr, where, section);
    }

    writeCrashFrames();

    // Devolver EXCEPTION_EXECUTE_HANDLER deja que el sistema termine el proceso con
    // su codigo de excepcion, que es lo que el gate espera para fallar. Devolver
    // EXCEPTION_CONTINUE_SEARCH lo deja morir sin escribir nada: el modo exacto en
    // el que se pierde el diagnostico.
    return EXCEPTION_EXECUTE_HANDLER;
}
#else
void onFatalSignal (int signalNumber) noexcept
{
    emit (recorder().crash, "SIGNAL number=%d", signalNumber);
    writeCrashFrames();
    std::_Exit (128 + signalNumber);
}
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Latido de liveness
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Hilo daemon: una linea cada kLivenessTickMs mientras el proceso viva.
 *
 * El latido lleva SIEMPRE el caso y la seccion en curso y los milisegundos que
 * lleva el caso corriendo. Eso es lo que convierte "exit code 1" en una
 * atribucion: si el ultimo TICK dice case_ms=812345, el proceso llevaba 13
 * minutos dentro de ese test y el culpable tiene nombre, aunque no haya linea
 * de excepcion ninguna.
 */
void livenessLoop() noexcept
{
    for (;;)
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (kLivenessTickMs));

        if (! recorder().active.load())
            return;

        char where[kMaxNameChars] { "" };
        char section[kMaxNameChars] { "" };

        readContext (where, sizeof (where), section, sizeof (section));

        const auto now = monotonicMs();
        const auto caseStart = recorder().caseStartedMs.load();

        // caseStart es 0 entre tests o antes del primer caso, y en ese caso la
        // resta daria el uptime del proceso, que no es lo que dice el campo.
        const auto caseMs = (caseStart > 0) ? (now - caseStart) : 0;

        emitLocked ("TICK alive=yes uptime_ms=%lld case=\"%s\" section=\"%s\" case_ms=%lld",
                    static_cast<long long> (now - recorder().runStartedMs.load()),
                    where, section, static_cast<long long> (caseMs));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Listener de Catch2
// ─────────────────────────────────────────────────────────────────────────────

struct Duration
{
    std::string name;
    long long milliseconds { 0 };
};

class TelemetryListener final : public Catch::EventListenerBase
{
public:
    using Catch::EventListenerBase::EventListenerBase;

    void testRunStarting (Catch::TestRunInfo const& info) override
    {
        m_runStart = std::chrono::steady_clock::now();
        recorder().runStartedMs.store (monotonicMs());

        // TestRunInfo solo lleva el nombre de la corrida en Catch2 3.5: el
        // inventario de casos hay que pedirlo al registro usando la configuracion
        // que el propio listener ya tiene. El total importa porque es lo que
        // convierte "se detuvo en el 41" en "de 318, 277 nunca llegaron a
        // ejecutarse", que es la mitad del valor de un RUN-START.
        m_totalTests = (m_config != nullptr)
                         ? static_cast<int> (Catch::getAllTestCasesSorted (*m_config).size())
                         : -1;

        emitLocked ("RUN-START name=\"%s\" total=%d",
                    static_cast<std::string> (info.name).c_str(), m_totalTests);
        publishContext ("", "");
    }

    /**
     * @brief Un CASE-START por ENTRADA al test, no por test.
     *
     * Catch2 entra en el cuerpo de un test una vez por cada hoja del arbol de
     * secciones: un test con cinco SECTION produce cinco ejecuciones y por tanto
     * cinco CASE-START con el mismo nombre y distinto partNumber. Esa granularidad
     * es la que permite atribuir la muerte a "la seccion 4 de este test" y no a
     * "este test", que con 318 casos no dice nada.
     */
    void testCasePartialStarting (Catch::TestCaseInfo const& info,
                                  std::uint64_t partNumber) override
    {
        m_caseName = info.name;
        m_caseStart = std::chrono::steady_clock::now();

        m_sectionNames.clear();
        m_sectionStarts.clear();

        // El fichero y la linea son la mitad del valor de la linea: el nombre de un
        // TEST_CASE lo elige quien lo escribe y se repite entre ficheros; el par
        // fichero:linea no.
        char buffer[kMaxNameChars] { "" };
        std::snprintf (buffer, sizeof (buffer), "%s:%u", info.lineInfo.file,
                       static_cast<unsigned> (info.lineInfo.line));

        emitLocked ("CASE-START part=%llu name=\"%s\" at=%s tags=[%s]",
                    static_cast<unsigned long long> (partNumber), info.name.c_str(), buffer,
                    info.tagsAsString().c_str());

        publishContext (m_caseName.c_str(), "");
        recorder().caseStartedMs.store (monotonicMs());
    }

    void sectionStarting (Catch::SectionInfo const& info) override
    {
        m_sectionNames.push_back (info.name);
        m_sectionStarts.push_back (std::chrono::steady_clock::now());

        emitLocked ("SECTION-START name=\"%s\"", info.name.c_str());
        publishContext (m_caseName.c_str(), buildSectionPath().c_str());
    }

    void sectionEnded (Catch::SectionStats const& stats) override
    {
        const auto elapsedMs = popSectionMs();

        // SectionStats SIENE Counts; TestCaseStats y TestRunStats no, tienen
        // Totals. Confundirlo compila con un struct_layout identico en los tres y
        // devuelve un numero sin relacion con nada.
        emitLocked ("SECTION-END name=\"%s\" ms=%lld assertions=%llu",
                    popSectionName().c_str(), static_cast<long long> (elapsedMs),
                    static_cast<unsigned long long> (stats.assertions.total()));
    }

    void testCasePartialEnded (Catch::TestCaseStats const& stats,
                               std::uint64_t partNumber) override
    {
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds> (
                                   std::chrono::steady_clock::now() - m_caseStart)
                                   .count();

        emitLocked ("CASE-END part=%llu ms=%lld assertions=%llu ok=%d",
                    static_cast<unsigned long long> (partNumber),
                    static_cast<long long> (elapsedMs),
                    static_cast<unsigned long long> (stats.totals.assertions.total()),
                    stats.totals.assertions.allOk() ? 1 : 0);

        m_durations.push_back (Duration { m_caseName, static_cast<long long> (elapsedMs) });

        publishContext ("", "");
        recorder().caseStartedMs.store (0);
    }

    /**
     * @brief El fallo interno de Catch2 tambien es una muerte silenciosa.
     *
     * Si el runner no puede ni construir su configuracion, este callback es lo
     * unico que se dispara: no hay excepcion, no hay asercion y no hay linea
     * final. Sin esto, ese caso se reporta igual que "exit code 1".
     */
    void fatalErrorEncountered (Catch::StringRef error) override
    {
        // StringRef no expone c_str(): se convierte explicitamente. Es el camino
        // frio (solo se llega si el runner no puede ni construir su configuracion)
        // asi que una asignacion aqui no cuesta nada.
        emitLocked ("RUNNER-FATAL text=\"%s\"", static_cast<std::string> (error).c_str());
    }

    void testRunEnded (Catch::TestRunStats const& stats) override
    {
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds> (
                                   std::chrono::steady_clock::now() - m_runStart)
                                   .count();

        emitLocked ("RUN-END ms=%lld assertions=%llu testCases=%llu aborting=%d",
                    static_cast<long long> (elapsedMs),
                    static_cast<unsigned long long> (stats.totals.assertions.total()),
                    static_cast<unsigned long long> (stats.totals.testCases.total()),
                    stats.aborting ? 1 : 0);

        // La tabla de duraciones es la otra mitad del encargo. Cuando el culpable
        // no es una excepcion sino un test lentisimo, no hay nada que capturar: lo
        // unico que lo delata es el reparto del tiempo.
        std::sort (m_durations.begin(), m_durations.end(),
                   [] (const Duration& a, const Duration& b)
                   { return a.milliseconds > b.milliseconds; });

        const auto reported = std::min (m_durations.size(), kSlowestToReport);

        emitLocked ("SLOWEST count=%zu", reported);

        for (std::size_t i = 0; i < reported; ++i)
            emitLocked ("SLOW #%02zu ms=%lld name=\"%s\"", i,
                        static_cast<long long> (m_durations[i].milliseconds),
                        m_durations[i].name.c_str());
    }

private:
    std::string buildSectionPath() const
    {
        std::string path;

        for (const auto& name : m_sectionNames)
        {
            if (! path.empty())
                path += " / ";

            path += name;
        }

        return path;
    }

    long long popSectionMs() noexcept
    {
        if (m_sectionStarts.empty())
            return 0;

        const auto start = m_sectionStarts.back();
        m_sectionStarts.pop_back();

        return std::chrono::duration_cast<std::chrono::milliseconds> (
                   std::chrono::steady_clock::now() - start)
            .count();
    }

    std::string popSectionName()
    {
        if (m_sectionNames.empty())
            return {};

        auto name = m_sectionNames.back();
        m_sectionNames.pop_back();
        return name;
    }

    std::chrono::steady_clock::time_point m_runStart {};
    std::chrono::steady_clock::time_point m_caseStart {};
    std::string m_caseName;
    std::vector<std::string> m_sectionNames;
    std::vector<std::chrono::steady_clock::time_point> m_sectionStarts;
    std::vector<Duration> m_durations;
    int m_totalTests { 0 };
};

} // namespace

// El registro es estatico a proposito: el listener existe antes de main(), asi que
// ningun test puede empezar a correr sin que haya alguien escuchando. La ACTIVACION
// se decide despues, en start(), para que un fallo al abrir el fichero desactive
// la telemetria en vez de romper la suite.
CATCH_REGISTER_LISTENER (TelemetryListener)

bool isActive() noexcept
{
    return recorder().active.load();
}

namespace
{

/**
 * @brief true si el entorno pide desactivar la telemetria.
 *
 * El opt-out existe porque un guard de diagnostico que no se puede apagar acaba
 * siendo el siguiente annoyance que la gente borra a lo bruto. El valor se
 * interpreta como bandera POSITIVA: solo desactiva si dice 0/false/off/no. La
 * lectura contraria, "la variable esta definida" como sinonimo de "desactivado",
 * hacia fallar en silencio en cuanto un CI definiera la variable para otra cosa.
 */
bool disabledByEnvironment() noexcept
{
    const auto* value = std::getenv (kTelemetryDisableEnvVar);

    if (value == nullptr)
        return false;

    const std::string flag { value };

    return flag == "0" || flag == "false" || flag == "off" || flag == "no";
}

/** @brief Une con una sola barra y sin duplicarla. */
std::string joinPath (const std::string& dir, const std::string& leaf)
{
    if (dir.empty())
        return leaf;

    auto last = dir.back();

    return (last == '/' || last == '\\' || last == ':') ? dir + leaf : dir + "/" + leaf;
}

/**
 * @brief Crea el directorio recursivamente sin excepciones ni C++17 filesystem.
 *
 * std::filesystem lanza, y esta funcion se llama desde el arranque del proceso:
 * una excepcion ahi sale antes de que Catch2 tenga manejador y se manifiesta como
 * la misma muerte silenciosa que se intenta instrumentar. Un fallo de creacion se
 * reporta con false y quien llama desactiva la telemetria.
 */
bool makeDirectories (const std::string& path)
{
    std::string partial = path;

    for (auto& ch : partial)
    {
        if (ch == '\\')
            ch = '/';
    }

    // La unidad ("C:") no es un directorio y no se puede crear. Se avanza hasta la
    // primera barra que la sigue y se empieza a crear desde ahi.
    auto start = partial.find (':');

    if (start != std::string::npos)
        ++start;

    std::string built = partial.substr (0, start);

    for (;;)
    {
        const auto slash = partial.find ('/', start);

        if (slash == std::string::npos)
            break;

        const auto piece = partial.substr (start, slash - start);

        if (! piece.empty())
        {
            built += piece;

#if defined(_WIN32)
            ::CreateDirectoryA (built.c_str(), nullptr);
#else
            ::mkdir (built.c_str(), 0755);
#endif

            built += '/';
        }

        start = slash + 1;
    }

    const auto lastPiece = partial.substr (start);

    if (! lastPiece.empty())
    {
        built += lastPiece;

#if defined(_WIN32)
        ::CreateDirectoryA (built.c_str(), nullptr);
#else
        ::mkdir (built.c_str(), 0755);
#endif
    }

#if defined(_WIN32)
    return ::GetFileAttributesA (partial.c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat info {};
    return ::stat (partial.c_str(), &info) == 0;
#endif
}

/** @brief true si la ruta ya existe. */
bool pathExists (const std::string& path) noexcept
{
#if defined(_WIN32)
    return ::GetFileAttributesA (path.c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat info {};
    return ::stat (path.c_str(), &info) == 0;
#endif
}

/** @brief Directorio del volcado: el del entorno, o el temporal del sistema. */
std::string resolveTelemetryDir()
{
    if (const auto* override_ = std::getenv (kTelemetryDirEnvVar);
         override_ != nullptr && *override_ != '\0')
    {
        return override_;
    }

#if defined(_WIN32)
    char tempDir[MAX_PATH] { "" };

    if (::GetTempPathA (MAX_PATH, tempDir) != 0)
        return joinPath (std::string (tempDir), "abdaudiolab-tests/runner");
#else
    if (const auto* tmp = std::getenv ("TMPDIR"); tmp != nullptr && *tmp != '\0')
        return joinPath (std::string (tmp), "abdaudiolab-tests/runner");
#endif

    return "abdaudiolab-tests/runner";
}

} // namespace

void start()
{
    auto& r = recorder();

    if (r.active.load())
        return;

    if (disabledByEnvironment())
        return;

    const auto dir = resolveTelemetryDir();

    if (! makeDirectories (dir))
        return;

    const auto pid = std::to_string (
#if defined(_WIN32)
        static_cast<long long> (::GetCurrentProcessId())
#else
        static_cast<long long> (::getpid())
#endif
    );

    // Un fichero por ejecucion. El numero de PID solo no basta: Windows recicla
    // PIDs con facilidad y dos corridas consecutivas en la misma maquina han de
    // quedar separadas, o el diagnostico mezcla los Deaths de una con los latidos
    // de la otra. El sufijo se incrementa hasta dar con un nombre libre.
    std::string stem;

    for (int index = 1; index <= kMaxRunIndex; ++index)
    {
        auto candidate = std::string (kTelemetryRunPrefix) + pid + "-"
                       + std::to_string (index);

        if (! pathExists (joinPath (dir, candidate + kTelemetryLogExtension)))
        {
            stem = candidate;
            break;
        }
    }

    if (stem.empty())
        return;

    r.logPath = joinPath (dir, stem + kTelemetryLogExtension);
    r.dumpPath = joinPath (dir, stem + kTelemetryDumpExtension);

    // "a" = append. Si el nombre libre ya estaba fechado por una carrera con otro
    // proceso, se sigue escribiendo encima en vez de perder lo que hubiera.
    r.normal = std::fopen (r.logPath.c_str(), "ab");
    r.crash = std::fopen (r.logPath.c_str(), "ab");

    if (r.normal == nullptr || r.crash == nullptr)
    {
        if (r.normal != nullptr)
            std::fclose (r.normal);

        if (r.crash != nullptr)
            std::fclose (r.crash);

        r.normal = nullptr;
        r.crash = nullptr;
        return;     // Sin telemetria, pero la suite sigue: nunca se rompe la puerta.
    }

    r.runStartedMs.store (monotonicMs());
    r.active.store (true);

#if defined(_WIN32)
    // SymInitialize solo puede llamarse una vez por proceso y sin threading, asi
    // que se hace aqui y NO dentro del manejador de caida. Sin simbolos,
    // SymGetModuleInfoW64 sigue devolviendo el modulo y el offset, que es
    // exactamente lo que se necesita para atribuir una direccion a un binario.
    ::SymInitialize (::GetCurrentProcess(), nullptr, TRUE);
    ::SetUnhandledExceptionFilter (&onUnhandledException);
#else
    std::signal (SIGSEGV, &onFatalSignal);
    std::signal (SIGABRT, &onFatalSignal);
    std::signal (SIGBUS, &onFatalSignal);
    std::signal (SIGFPE, &onFatalSignal);
    std::signal (SIGILL, &onFatalSignal);
#endif

    emitLocked ("RUNNER-START log=\"%s\" dump=\"%s\" tickMs=%d", r.logPath.c_str(),
                r.dumpPath.c_str(), kLivenessTickMs);

    std::thread (livenessLoop).detach();
}

} // namespace abdaudiolab::test::telemetry