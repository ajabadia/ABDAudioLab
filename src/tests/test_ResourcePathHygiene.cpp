/**
 * @file test_ResourcePathHygiene.cpp
 * @brief Guard anti-regresión de hermeticidad de rutas.
 *
 * CONTEXTO
 * --------
 * Hasta POST-5D.5 la suite resolvia recursos del repositorio con
 * juce::File::getCurrentWorkingDirectory() en 37 helpers duplicados. Eso hacia
 * que el resultado dependiera del directorio de trabajo: el mismo ejecutable
 * daba 0 fallos lanzado desde la raiz del repositorio y 61 fallos lanzado desde
 * build/. La suite estaba verde por accidente del CWD, no por hermeticidad.
 *
 * Estos tests fallan si alguien reintroduce esa dependencia. Son deliberados:
 * un fallo aqui significa que un gate de CI puede volverse vacio o que la
 * suite dejara de ser reproducible, y ambos fallos son silenciosos.
 *
 * La excepcion declarada son los recursos que NO viven en el repositorio y por
 * tanto no pueden resolverse con LabResourcePaths: binarios de terceros (Dexed),
 * ROMs de VES y el directorio de trabajo del plugin worker. Cada uno debe
 * seguir Justificado en la lista de permitidos de abajo.
 *
 * @author ABDSynths
 * @date 2026
 */

#include <catch2/catch_test_macros.hpp>

#include "core/LabResourcePaths.h"

#include <cctype>
#include <fstream>
#include <set>
#include <string>
#include <vector>

namespace
{

/**
 * @brief Ficheros autorizados a sondear el CWD, con el motivo de cada uno.
 *
 * Anyadir una entrada aqui es una decision consciente: se documenta POR QUE el
 * recurso no puede resolverse contra la raiz del repositorio.
 *
 * INVARIANTE: una entrada que ya no usa getCurrentWorkingDirectory es una entrada
 * que hay que BORRAR. Una allowlist que acumula permisos caducados deja de proteger:
 * el fichero vuelve a estar libre de污泥 y el guard ya no lo vigila. No se puede
 * comprobar solo con este test (que ignora las entradas por construccion); se
 * revisa al migrar, y este listado es la prueba de que se hizo.
 */
const std::set<std::string>& allowedCwdConsumers()
{
    static const std::set<std::string> allowed
    {
        // Binarios de terceros instalados fuera del repositorio. El barrido
        // busca el .vst3 de Dexed subiendo desde el ejecutable y desde el CWD
        // porque no se sabe donde lo instalo quien ejecuta la suite.
        "test_GuidedPluginLoad_Dexed.cpp",              // plugin Dexed en Program Files / build

        // Mocks y rutas ficticias usadas en aserciones negativas de robustez.
        "test_WorkerProcessHost.cpp",                   // ejecutable inexistente deliberado

        // Los siguientes estaban aqui y se han ido saliendo de la lista segun se
        // migraban, porque ya no necesitan el CWD:
        //   test_DexedEnvelopeMeasurement_T5, test_TargetProfileDexedBehavior,
        //   test_TargetProfileDexedHosting  -> DEXED_VST3_PATH + repoResource()
        //   test_VesCz101SemanticControl, test_VesCz101Feasibility -> VES_ROM_DIR
        //   test_ProfilingSessionCoordinator   -> fixtures temporales via repoResource()
        //   test_SessionIoController, test_PluginHostManager,
        //   test_HardwareProfileValidation, test_UiCoordinatorGovernance,
        //   test_OutOfProcessVst3LifecycleAdapter, test_ExperimentStorage
        //                                    -> sus rutas ficticias nunca tocaron
        //                                       disco; la dependencia ya no existe
    };

    return allowed;
}

const char* const kGuardFileName = "test_ResourcePathHygiene.cpp";

/**
 * @brief Ficheros de PRODUCCION autorizados a sondear el CWD, con su motivo.
 *
 * Aqui la excepcion no es "no hay otro sitio": es "este recurso NO es del
 * repositorio". La distincion que sostiene el guard es:
 *
 *   - ENTRADAS del repo (contratos, perfiles, recetas, fixtures, assets, docs)
 *     se resuelven contra la raiz del repo y SIEMPRE con la misma API.
 *   - SALIDAS del usuario (destino de una exportacion, directorio inicial de
 *     un selector de ficheros) y ARTEFACTOS EXTERNOS (worker, VST3 de terceros,
 *     ABDSharedAssets) son relativas al entorno de ejecucion por naturaleza.
 *
 * Anyadir una entrada es una decision consciente y hay que justificar por que el
 * recurso no puede resolverse contra la raiz del repositorio.
 */
const std::set<std::string>& allowedProductionCwdConsumers()
{
    static const std::set<std::string> allowed
    {
        // El propio resolutor: sondear el CWD es literalmente su trabajo.
        "LabResourcePaths.cpp",

        // Binarios externos que viven en el arbol de build o en el de terceros.
        "OutOfProcessVst3LifecycleAdapter.cpp",   // ejecutable del plugin worker
        "SynthTargetLifecycleAdapters.cpp",       // VST3 de terceros ya cargados

        // Destinos elegidos por el usuario y artefactos que produce el worker.
        "ProfilingSessionController.cpp",         // destino de exportacion + guided/evidence del worker
        "AudioABVerificationModal.cpp",           // directorio inicial y nombre por defecto de un FileChooser
    };

    return allowed;
}

/** @brief Devuelve el nombre de fichero a partir de una ruta absoluta o relativa. */
std::string fileNameOf(const std::string& path)
{
    const auto slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

/**
 * @brief needles que delatan una REIMPLEMENTACION de la resolucion de rutas.
 *
 * Los dos barridos anteriores miran literales. Este mira el patron: si un
 * fichero de produccion calcula la raiz por su cuenta desde el ejecutable,
 * esta reimplementando algo que LabResourcePaths ya hace, y el resultado
 * dependera de cuantos niveles separen el .exe de la raiz. Ese fue
 * exactamente el fallo de HardwareDeviceDisplayCardComponent.cpp, que probaba
 * 7 ancestros a ciegas.
 *
 * Consultar el directorio del ejecutable NO es por si mismo un error: una
 * distribucion portable lleva los assets al lado del binario. Lo que no es
 * admisible es hacerlo SIN pasar por la API canonica. Por eso el barrido es
 * "sin excepcion": quien lo necesite tiene que declararlo abajo con su motivo.
 */
const std::vector<std::string>& exeProbeNeedles()
{
    static const std::vector<std::string> needles { "currentExecutableFile" };
    return needles;
}

/**
 * @brief Ficheros de PRODUCCION autorizados a consultar el ejecutable, con su motivo.
 *
 * Se separa de la allowlist de CWD a proposito: autorizar aqui no autoriza
 * sondear el directorio de trabajo, y viceversa. Un fichero puede estar en
 * una y no en la otra.
 */
const std::set<std::string>& allowedProductionExeProbes()
{
    static const std::set<std::string> allowed
    {
        "LabResourcePaths.cpp",                       // el resolutor: su trabajo

        // Distribucion portable: los assets van al lado del binario, no hay
        // arbol de repositorio que consultar. Se consulta UN nivel.
        "AssetLocator.h",                             // paso 4 de locateAsset
        "HardwareDeviceDisplayCardComponent.cpp",     // paso 4 de locateAssetFile

        // Artefactos externos que no viven en el arbol del repositorio.
        "OutOfProcessVst3LifecycleAdapter.cpp",       // ejecutable del plugin worker
        "SynthTargetLifecycleAdapters.cpp",           // VST3 de terceros ya cargados
    };

    return allowed;
}

/** @brief Resultado de un recorrido de escaneo. */
struct ScanResult
{
    std::vector<std::string> offenders;   ///< "fichero:linea -> linea de codigo"
    std::vector<std::string> unreadable;  ///< ficheros que no se pudieron abrir
    int scanned = 0;                      ///< ficheros leidos de verdad
};

/** @brief Minusculas ASCII, para comparar rutas sin sensibilidad a caja. */
std::string toLowerAscii (std::string s)
{
    for (auto& ch : s)
        ch = static_cast<char> (std::tolower (static_cast<unsigned char> (ch)));

    return s;
}

/**
 * @brief Normaliza una linea para comparar rutas CONTRA EL MISMO PATRON.
 *
 * En codigo C++ la misma ruta de Windows aparece en tres formas segun como se
 * escriba el literal, y las tres existen de verdad en el repositorio:
 *
 *   "D:/desarrollos/..."              barra normal
 *   R"(D:\desarrollos\...)"           raw string
 *   "D:\\desarrollos\\..."             literal escapado, con DOBLE barra en el fichero
 *
 * Un needle con barra simple solo encuentra la primera: las otras dos se le
 * escapan. Por eso se colapsa la barra doble a simple y despues se unifica
 * todo a barra normal, y los needles se escriben siempre en esa forma.
 */
std::string normalizeForPathMatch (const std::string& line)
{
    auto s = toLowerAscii (line);
    std::string out;
    out.reserve (s.size());

    for (std::size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == '\\')
        {
            out += '/';
            ++i;
            continue;
        }

        out += (s[i] == '\\') ? '/' : s[i];
    }

    return out;
}

/** @brief needles que delatan dependencia del directorio de trabajo. */
const std::vector<std::string>& cwdNeedles()
{
    static const std::vector<std::string> needles { "getCurrentWorkingDirectory" };
    return needles;
}

/**
 * @brief needles que delatan una ruta absoluta de la maquina del desarrollador.
 *
 * Se limitan a la unidad de desarrollo personal y al perfil de usuario de
 * Windows, que es lo que rompio CI. Deliberadamente NO se busca cualquier
 * "X:/": las rutas ficticias usadas a proposito en aserciones negativas
 * ("C:/Ruta/Ficticia/Inexistente.exe", "Z:/non_existent_drive_9999/...",
 * "C:\\autoexec.bat") son legitimas y deben seguir siendo validas.
 *
 * Se escriben en forma normalizada (barra normal, minusculas) porque la
 * comparacion pasa por normalizeForPathMatch(), que absorbe la barra invertida
 * y la doble barra de los literales escapados.
 */
const std::vector<std::string>& machinePathNeedles()
{
    static const std::vector<std::string> needles
    {
        "d:/desarrollos",
        "c:/users/",
    };

    return needles;
}

/**
 * @brief Recorre un arbol buscando en el codigo los patrones prohibidos dados.
 *
 * @param root        Directorio raiz del recorrido.
 * @param recursive   Si se baja a los subdirectorios.
 * @param extensions  Extensiones a considerar (p. ej. "*.cpp;*.h" con ';' como separador).
 * @param allowed     Nombres de fichero autorizados a contienen esos patrones.
 * @param selfName    Nombre del propio guard, que nombra los patrones que busca.
 * @param needles     Patrones prohibidos; basta con que aparezcan en la linea.
 * @param ignoreCase  Comparar sin sensibilidad a caja.
 * @param pathMode    Normalizar la linea antes de comparar (barras y escapes).
 * @param skipDir     Nombre de un subdirectorio a excluir (src/tests cuando se escanea src).
 */
ScanResult scanForForbiddenPatterns (const juce::File& root,
                                     bool recursive,
                                     const juce::String& extensions,
                                     const std::set<std::string>& allowed,
                                     const std::string& selfName,
                                     const std::vector<std::string>& needles,
                                     bool ignoreCase,
                                     bool pathMode,
                                     const std::string& skipDir = {})
{
    ScanResult result;

    juce::StringArray patterns;
    patterns.addTokens (extensions, ";", "");

    // juce::File::findChildFiles solo acepta UN wildcard (no hay sobrecarga con
    // StringArray), asi que se acumula el resultado de uno por patron.
    juce::Array<juce::File> found;

    for (const auto& pattern : patterns)
        found.addArray (root.findChildFiles (juce::File::findFiles, recursive, pattern));

    for (const auto& entry : found)
    {
        const auto name = fileNameOf (entry.getFileName().toStdString());

        if (! skipDir.empty() && entry.getParentDirectory().getFileName().toStdString() == skipDir)
            continue;

        if (allowed.count (name) > 0)
            continue;

        // Ruta ABSOLUTA a proposito: abrir por nombre desnudo lo resolveria
        // contra el CWD y el guard no leeria nada al ejecutarse desde la raiz
        // del repositorio, que es justo donde corre la CI.
        std::ifstream in (entry.getFullPathName().toStdString(), std::ios::binary);

        if (! in.is_open())
        {
            result.unreadable.push_back (name);
            continue;
        }

        ++result.scanned;

        // El guard nombra el patron que busca, asi que excluirse es inevitable.
        // Sigue leyendose, para que scanned siga siendo honesto.
        if (name == selfName)
            continue;

        std::string line;
        int lineNumber = 0;
        bool insideBlockComment = false;

        while (std::getline (in, line))
        {
            ++lineNumber;

            // La documentacion nombra este mismo patron al explicar el problema.
            if (insideBlockComment)
            {
                insideBlockComment = (line.find ("*/") == std::string::npos);
                continue;
            }

            const auto haystack = pathMode ? normalizeForPathMatch (line)
                                           : (ignoreCase ? toLowerAscii (line) : line);
            bool hit = false;

            for (const auto& rawNeedle : needles)
            {
                const auto needle = pathMode ? normalizeForPathMatch (rawNeedle)
                                             : (ignoreCase ? toLowerAscii (rawNeedle) : rawNeedle);

                if (haystack.find (needle) != std::string::npos)
                {
                    hit = true;
                    break;
                }
            }

            if (! hit)
            {
                const auto open = line.find ("/*");

                if (open != std::string::npos
                     && line.find ("*/", open + 2) == std::string::npos)
                {
                    insideBlockComment = true;
                }

                continue;
            }

            const auto firstNonSpace = line.find_first_not_of (" \t");

            if (firstNonSpace != std::string::npos
                 && line.compare (firstNonSpace, 2, "//") == 0)
            {
                continue;
            }

            result.offenders.push_back (name + ":" + std::to_string (lineNumber) + " -> " + line);
        }
    }

    return result;
}

} // namespace

TEST_CASE("Hygiene de rutas: ningun test resuelve datos del repo por getCurrentWorkingDirectory",
          "[hygiene][resourcepaths][hermetic]")
{
    const juce::File testsDir = juce::File(__FILE__).getParentDirectory();

    // Si __FILE__ no llegara como ruta utilizable, el recorrido seria vacio y
    // este test pasaria sin haber leido nada. Se falla de forma explicita.
    REQUIRE(testsDir.isDirectory());

    const auto result = scanForForbiddenPatterns (testsDir, false, "*.cpp",
                                                  allowedCwdConsumers(), kGuardFileName,
                                                  cwdNeedles(), true, false);

    INFO("Ficheros de test escaneados: " << result.scanned);
    INFO("Ficheros de test que sondean el CWD sin estar justificados: " << result.offenders.size());

    // Cada infraccion es su propio fallo con su propio mensaje: asi el reporte
    // nombra fichero y linea. FAIL_CHECK (no CHECK_MESSAGE, que es API de
    // Catch2 v2) continua la ejecucion y acumula un fallo por infraccion.
    for (const auto& offender : result.offenders)
        FAIL_CHECK(offender);

    for (const auto& missing : result.unreadable)
        FAIL_CHECK("No se pudo leer: " + missing);

    // Anti-vacuidad: un guard que no lee ficheros es un guard que no protege.
    // REQUIRE, no CHECK: no tiene sentido seguir si no escaneo nada.
    REQUIRE(result.unreadable.empty());
    REQUIRE(result.scanned > 0);
}

TEST_CASE("Hygiene de rutas: produccion no resuelve datos del repo por getCurrentWorkingDirectory",
          "[hygiene][resourcepaths][production]")
{
    const juce::File srcDir = juce::File(__FILE__).getParentDirectory().getParentDirectory();

    REQUIRE(srcDir.isDirectory());

    // Se excluye src/tests: lo cubre el caso anterior con su propia allowlist.
    const auto result = scanForForbiddenPatterns (srcDir, true, "*.cpp;*.h",
                                                  allowedProductionCwdConsumers(), kGuardFileName,
                                                  cwdNeedles(), true, false, "tests");

    INFO("Ficheros de produccion escaneados: " << result.scanned);
    INFO("Ficheros de produccion que sondean el CWD sin estar justificados: " << result.offenders.size());

    for (const auto& offender : result.offenders)
        FAIL_CHECK(offender);

    for (const auto& missing : result.unreadable)
        FAIL_CHECK("No se pudo leer: " + missing);

    REQUIRE(result.unreadable.empty());
    REQUIRE(result.scanned > 0);
}

TEST_CASE("Hygiene de rutas: ningun test codifica una ruta absoluta de mi maquina",
          "[hygiene][resourcepaths][hermetic]")
{
    // Este es el guard que faltaba. El anterior solo buscaba getCurrentWorkingDirectory,
    // pero la causa real del Run #6 roto fueron literales como
    // juce::File("D:/desarrollos/ABDSynths/ABDAudioLab/profiles/targets/..."),
    // que no mencionan el CWD y por tanto escapaban del barrido por completo.
    // Un guard que no ve el fallo que motives la incidencia no protege de el.
    const juce::File testsDir = juce::File(__FILE__).getParentDirectory();

    REQUIRE(testsDir.isDirectory());

    const auto result = scanForForbiddenPatterns (testsDir, false, "*.cpp",
                                                  {}, kGuardFileName,
                                                  machinePathNeedles(), true, true);

    INFO("Ficheros de test escaneados: " << result.scanned);
    INFO("Rutas absolutas personales encontradas: " << result.offenders.size());

    for (const auto& offender : result.offenders)
        FAIL_CHECK(offender);

    for (const auto& missing : result.unreadable)
        FAIL_CHECK("No se pudo leer: " + missing);

    REQUIRE(result.unreadable.empty());
    REQUIRE(result.scanned > 0);
}

TEST_CASE("Hygiene de rutas: produccion no codifica una ruta absoluta de mi maquina",
          "[hygiene][resourcepaths][production]")
{
    const juce::File srcDir = juce::File(__FILE__).getParentDirectory().getParentDirectory();

    REQUIRE(srcDir.isDirectory());

    // Sin allowlist: en produccion no hay ninguna ruta personal legitima. Las
    // rutas que quedan (Program Files, LOCALAPPDATA, el arbol de build) no
    // contienen ni la unidad de desarrollo ni el perfil de usuario.
    const auto result = scanForForbiddenPatterns (srcDir, true, "*.cpp;*.h",
                                                  {}, kGuardFileName,
                                                  machinePathNeedles(), true, true, "tests");

    INFO("Ficheros de produccion escaneados: " << result.scanned);
    INFO("Rutas absolutas personales encontradas: " << result.offenders.size());

    for (const auto& offender : result.offenders)
        FAIL_CHECK(offender);

    for (const auto& missing : result.unreadable)
        FAIL_CHECK("No se pudo leer: " + missing);

    REQUIRE(result.unreadable.empty());
    REQUIRE(result.scanned > 0);
}

TEST_CASE("Hygiene de rutas: produccion no reimplementa la resolucion desde el ejecutable",
          "[hygiene][resourcepaths][production]")
{
    // El fallo que este caso cierra: un fichero que localiza la raiz del repo
    // subiendo N ancestros desde el .exe. No contiene ningun literal ni ninguna
    // llamada a getCurrentWorkingDirectory, asi que los otros dos barridos lo
    // declaraban limpio mientras hacia exactamente lo que hay que evitar.
    const juce::File srcDir = juce::File(__FILE__).getParentDirectory().getParentDirectory();

    REQUIRE(srcDir.isDirectory());

    const auto result = scanForForbiddenPatterns (srcDir, true, "*.cpp;*.h",
                                                  allowedProductionExeProbes(), kGuardFileName,
                                                  exeProbeNeedles(), false, false, "tests");

    INFO("Ficheros de produccion escaneados: " << result.scanned);
    INFO("Ficheros de produccion que consultan el ejecutable sin justificacion: "
         << result.offenders.size());

    for (const auto& offender : result.offenders)
        FAIL_CHECK(offender);

    for (const auto& missing : result.unreadable)
        FAIL_CHECK("No se pudo leer: " + missing);

    REQUIRE(result.unreadable.empty());
    REQUIRE(result.scanned > 0);
}

TEST_CASE("Hygiene de rutas: el catalogo de contratos permanece versionado",
          "[hygiene][resourcepaths][contracts]")
{
    // Si contracts/hardware volviera a ser un symlink o desapareciera del
    // control de versiones, la resolucion por marcador seguiria "funcionando"
    // pero no habria catalogo que cargar. Esta es la invariante que
    // desbloqueo la suite completa.
    const auto contractsDir = abdaudiolab::core::contractsHardwareDir();
    REQUIRE(contractsDir.isDirectory());

    // Contratos nativos que deben seguir presentes en el catalogo legacy.
    REQUIRE(contractsDir.getChildFile("casio_cz101.json").existsAsFile());
    REQUIRE(contractsDir.getChildFile("roland_juno106.json").existsAsFile());
    REQUIRE(contractsDir.getChildFile("behringer_deepmind12.json").existsAsFile());

    // Los tres perfiles homologados fueron migrados a profiles/targets/*.target.json
    // y se retiraron fisicamente del catalogo legacy (HITO-10E E6). Su ausencia
    // aqui es invariante: si volvieran a aparecer, estarian duplicados.
    CHECK_FALSE(contractsDir.getChildFile("yamaha_dx7.json").existsAsFile());
    CHECK_FALSE(contractsDir.getChildFile("behringer_pro800.json").existsAsFile());
    CHECK_FALSE(contractsDir.getChildFile("boss_ds1_distortion.json").existsAsFile());

    // Y sus sustitutos canonicos deben existir en profiles/targets/.
    const auto targets = abdaudiolab::core::canonicalTargetsDir();
    REQUIRE(targets.getChildFile("yamaha_dx7.target.json").existsAsFile());
    REQUIRE(targets.getChildFile("behringer_pro800.target.json").existsAsFile());
    REQUIRE(targets.getChildFile("boss_ds1_distortion.target.json").existsAsFile());
}

TEST_CASE("Hygiene de rutas: la raiz se resuelve igual desde cualquier directorio",
          "[hygiene][resourcepaths][hermetic]")
{
    // El contrato central del modulo: resolver la raiz no puede depender del CWD.
    const auto fromHere = abdaudiolab::core::requireRepoRoot();
    REQUIRE(fromHere.isDirectory());
    REQUIRE(fromHere.getChildFile("ABDAudioLab.workspace").existsAsFile());
}
