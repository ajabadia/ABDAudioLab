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
 * POST-5D.5 cerro ademas la SEGUNDA mitad del problema: los tests escribian
 * DENTRO del arbol del repositorio (docs/qa/runs/*.json, fixtures/evaluations/*.json,
 * assets/presets/*.json), de modo que una corrida dejaba git status sucio y un
 * artefacto regenerado sin querer se confundia con un cambio de codigo. La
 * invariante es ahora explicita: la unica via por la que un test obtiene una ruta
 * del repo para ESCRITURA es abdaudiolab::test::artifactDir(), que por defecto
 * devuelve un temporal y solo devuelve el repo con ABD_REGENERATE_ARTIFACTS=1.
 *
 * POST-5D.5 amplitudes el alcance a lo que no es codigo. Un enlace markdown
 * `file:///d:/desarrollos/...` no rompe la ejecucion: rompe la lectura. No resuelve
 * en el navegador de nadie mas y, en GitHub, aparece como texto plano, asi que el
 * documento que lo contiene parece actualizado mientras su unica referencia viva
 * acaba de romperse en silencio. Hay dos barridos mas, sobre docs/ y sobre los 43
 * PLAN_*.md / MATRIX_*.md / ACTA_*.md sueltos de la raiz, que concentran mas rutas
 * personales que docs/ entero.
 *
 * contracts/ y fixtures/ NO se barren, y no por descuido. contracts/ es copia byte a
 * byte de ABDSharedAssets/contracts y ya tiene su propio guard de identidad
 * (test_ContractsSnapshotDrift.cpp). Anadirle una segunda autoridad crearia un
 * conflicto: si upstream escribiera una ruta personal en un campo de ejemplo, la CI
 * de aqui se pondria roja y el unico arreglo que cumpliria este guard, editar la
 * copia, romperia la identidad byte a byte. fixtures/ no tiene ningun campo de ruta.
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

#include "support/LabTestScratch.h"

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
 * el fichero vuelve a estar libre de lodo y el guard ya no lo vigila. No se puede
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
 * @brief Como interpreta el recorrido la sintaxis de comentarios del fichero.
 *
 * Existe porque el stripper de comentarios es un LECTOR DE C++, y aplicado a
 * markdown se traga el resto del fichero. docs/ROADMAP.md contiene el glob
 * `raw_audio/*.wav`: la secuencia barra-asterisco abre, en la mente del
 * stripper, un comentario de bloque que ningun asterisco-barra posterior cierra.
 * Todas las lineas siguientes quedan invisibles para el barrido. Medido sobre el
 * mismo conjunto de ficheros: 22 infracciones con el stripper, 73 sin el.
 *
 * En .cpp/.h no se nota porque ahi los comentarios cierran. Eso lo hace una
 * hipotesis latente y no una garantia, y un guard que depende de una hipotesis
 * no protege.
 *
 * NOTA al editar: este comentario NO puede contener la secuencia literal
 * asterisco-barra, porque cerraria el propio comentario de bloque. Es la misma
 * trampa que el guard evita en los .cpp, y por eso se dice aqui.
 */
enum class CommentSyntax
{
    cxx,    ///< sintaxis de C++: comentarios de bloque y de linea se ignoran
    none    ///< el formato no tiene comentarios: no se ignora ninguna linea
};

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
 * @param skipDirs    Nombres de subdirectorios a excluir. Es un conjunto y no un
 * @param commentSyntax  Como tratar comentarios: none para markdown, JSON y YAML.
 */
ScanResult scanForForbiddenPatterns (const juce::File& root,
                                     bool recursive,
                                     const juce::String& extensions,
                                     const std::set<std::string>& allowed,
                                     const std::string& selfName,
                                     const std::vector<std::string>& needles,
                                     bool ignoreCase,
                                     bool pathMode,
                                     const std::set<std::string>& skipDirs = {},
                                     CommentSyntax commentSyntax = CommentSyntax::cxx)
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

        // Se compara por NOMBRE de directorio, no por ruta: asi el mismo conjunto
        // sirve para excluir src/tests de un recorrido sobre src y para excluir dos
        // arboles vendorizados de docs/ que cuelgan en distintas profundidades.
        const auto parentName = entry.getParentDirectory().getFileName().toStdString();

        if (skipDirs.count (parentName) > 0)
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
                if (commentSyntax == CommentSyntax::cxx)
                {
                    const auto open = line.find ("/*");

                    if (open != std::string::npos
                         && line.find ("*/", open + 2) == std::string::npos)
                    {
                        insideBlockComment = true;
                    }
                }

                continue;
            }

            const auto firstNonSpace = line.find_first_not_of (" \t");

            if (commentSyntax == CommentSyntax::cxx
                 && firstNonSpace != std::string::npos
                 && line.compare (firstNonSpace, 2, "//") == 0)
            {
                continue;
            }

            result.offenders.push_back (name + ":" + std::to_string (lineNumber) + " -> " + line);
        }
    }

    return result;
}

/**
 * @brief Ficheros autorizados a pedir una ruta del REPO, con el motivo de cada uno.
 *
 * Estos tests NO escriben: leen recursos versionados como fixture de entrada
 * (perfiles de target, snapshots de UI, evaluaciones aprobadas). La regla que
 * vigina este listado no es "no toques el repo", sino "la UNICA via por la que
 * un test puede obtener una ruta del repo con fines de ESCRITURA es
 * abdaudiolab::test::artifactDir()". Por eso los lectores legitimos estan aqui
 * y los escritores, no: un escritor aparece como infraccion del guard.
 *
 * INVARIANTE: una entrada que ya no lea del repo es una entrada que hay que
 * BORRAR. Y cualquier escritor nuevo se migra a artifactDir(), no se autoriza
 * aqui.
 */
const std::set<std::string>& allowedRepoPathReaders()
{
    static const std::set<std::string> allowed
    {
        // Prueba la propia API de resolucion: repoResource(), optionalRepoResource()
        // y todos los accesores de directorio. Es el unico sitio donde probarlos.
        { "test_LabResourcePaths.cpp" },

        // Cargan perfiles/targets/*.target.json versionados como fixture de entrada
        // del perfil de transporte, y el catalogo de contratos nativo.
        { "test_TargetProfileTransportSafety.cpp" },

        // Cargan el perfil Dexed versionado del repo como fixture de entrada del
        // comportamiento esperado; no lo generan ni lo modifican.
        { "test_TargetProfileDexedBehavior.cpp" },
        { "test_TargetProfileDexedHosting.cpp" },

        // read_fixture_file() abre fixtures/ui/*.snapshot versionados. El propio
        // helper se llama read_ y devuelve std::string: es lectura pura.
        { "test_UiCompositionSeam6.cpp" },

        // Parten de fixtures/evaluations/fixture_approved.json versionado como
        // estado inicial del controlador, y de ahi evolves.
        { "test_ProfilingSessionController.cpp" },
        { "test_ProfilingSessionCoordinator.cpp" },

        // Lee entero el workflow de CI para verificar por texto los contratos que
        // exige (filtro de rutas, nombre del checkout). repoResource() es la
        // lectura, no la escritura: la escritura sigue siendo artifactDir().
        { "test_ContractsCiContract.cpp" },

        // Workstream de cuarentena de hardware: leen el catalogo compartido de
        // assets. Aun sin migrar al scratch, y siguen siendo solo lectura.
        { "test_ContractsSnapshotDrift.cpp" },
        { "test_HardwareContractQuarantine.cpp" },
    };

    return allowed;
}

/**
 * @brief needles que delatan una ruta del repo obtenida para ESCRIBIR.
 *
 * Solo los accesores que existen porque alguien escribio ahi alguna vez. Los
 * lectores (contractsHardwareDir, canonicalTargetsDir, profilingPresetsDir,
 * profilesDir, docsQaDir, fixturesDir) quedan fuera a proposito: leer el repo
 * es legitimo yynessimo, y prohibirlo seria un guard que obliga a falsejar el
 * codigo en lugar de a protegerlo.
 */
const std::vector<std::string>& repoWritePathNeedles()
{
    static const std::vector<std::string> needles
    {
        "docsQaRunsDir",           // informes de corrida de docs/qa/runs
        "fixturesEvaluationsDir",  // fixtures/evaluations
        "exportedLutsDir",         // exported_luts
        "assetsDir",               // assets/ (incluye assets/presets)
        "repoResource",            // raiz del repo por ruta relativa; ingiere optionalRepoResource
    };

    return needles;
}

/**
 * @brief Ficheros autorizados a construir su temporal con getSpecialLocation().
 *
 * La invariante es que cada test tenga SU directorio, vaciado al entrar. Ese
 * contrato lo cumple abdaudiolab::test::scratchDir(), y por eso el helper
 * puede citar tempDirectory: es el unico sitio del proyecto autorizado a hacerlo.
 *
 * Estas cuatro entradas quedan PENDIENTES de migracion (workstream de
 * cuarentena de hardware). Cuando se cierren, la entrada se borra: una
 * allowlist que acumula permisos caducados deja de proteger.
 */
const std::set<std::string>& allowedRawTempDirUsers()
{
    static const std::set<std::string> allowed
    {
        { "test_ContractsSnapshotDrift.cpp" },
        { "test_HardwareContractQuarantine.cpp" },
        { "test_HardwareContractRangeFieldNames.cpp" },
        { "test_StartupWarningsPanel.cpp" },
    };

    return allowed;
}

/**
 * @brief needles que delatan un temporal crudo en vez de un scratch por test.
 *
 * Se buscan las DOS grafias porque JUCE permite espacear la llamada y el codigo
 * del repo ya usa las dos ("getSpecialLocation(" y "getSpecialLocation ("): un
 * needle con una sola grafia deja pasar la otra y el guard protege a medias.
 *
 * NO se busca simplemente "tempDirectory": los RAII E2ETempDirectory /
 * TestTempDirectory / IntegrationTempDirectory / SmokeTempDirectory la usan en
 * el NOMBRE del tipo, y esos ya cumplen el contrato (temp propio, vaciado y
 * borrado al salir) por su cuenta.
 */
const std::vector<std::string>& rawTempDirNeedles()
{
    static const std::vector<std::string> needles
    {
        "getSpecialLocation(juce::File::tempDirectory)",
        "getSpecialLocation (juce::File::tempDirectory)",
    };

    return needles;
}
/**
 * @brief Documentos autorizados a CITAR las rutas que el resto del repo prohibe.
 *
 * No es lo mismo que un consumidor: estos ficheros no resuelven nada contra la
 * maquina de nadie. Su asunto ES la ruta, asi que tienen que escribirla para poder
 * auditarla, registrarla o prohibirla. Un enlace file:/// a d:/desarrollos/...
 * dentro de uno de ellos es la evidencia, no el defecto.
 *
 * INVARIANTE distinta de las otras allowlists: una entrada puede quedarse aqui
 * para siempre sin que sea deuda, porque el fichero sigue siendo un documento
 * sobre el problema. Lo que no puede es crecer sin motivo nuevo.
 */
const std::set<std::string>& allowedPathCitationDocuments()
{
    static const std::set<std::string> allowed
    {
        // El inventario: su contenido ES la tabla de rutas que encuentra.
        { "POST_5D5_HARDCODED_PATHS_INVENTORY.md" },

        // Actas que registran el incidente que las produjo (Gate 6) y el informe
        // de calidad con el literal de cada hallazgo.
        { "ACTA_HITO_AUDIO_AB_5D.md" },
        { "CODE_QUALITY_REPORT.md" },

        // Documentos que ENSEÑAN el patron para que no se repita.
        { "GUIDE_ISSUES_TO_AVOID.md" },
        { "PLAN.md" },

        // El contrato que las prohibe cita una como ejemplo de lo no permitido.
        { "MEASUREMENT_RECIPE_CONTRACT.md" },

        // Transcripcion de una herramienta, no un documento del repo: escribe la
        // linea de comandos con la ruta del ejecutable. Esta en .gitignore y no
        // existe en la CI, pero el recorrido es sobre el ARBOL DE TRABAJO, no
        // sobre el indice de git, asi que aqui se ve. Entrada que desaparece
        // sola el dia que se borre el fichero.
        { ".aider.chat.history.md" },
    };

    return allowed;
}

/**
 * @brief Arboles de docs/ que son COPIAS DE TERCEROS y que este repo no mantiene.
 *
 * docs/google ia research/ y docs/take 5 to lab/ traen su propio .gitmodules y su
 * propio .gitignore: son arboles upstream, no codigo nuestro. Medido hoy: aportan
 * cero infracciones. Se excluyen igual porque un barrido que depende de que la
 * dependencia vendorizada se porte bien no es un barrido, y porque nadie aqui
 * puede arreglar lo que salga.
 *
 * Se comparan por NOMBRE de directorio, que es como funciona skipDirs.
 */
const std::set<std::string>& vendoredDocumentationTrees()
{
    static const std::set<std::string> trees
    {
        { "google ia research" },
        { "take 5 to lab" },
    };

    return trees;
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
                                                  cwdNeedles(), true, false, { "tests" });

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
                                                  machinePathNeedles(), true, true, { "tests" });

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
                                                  exeProbeNeedles(), false, false, { "tests" });

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
TEST_CASE("Hygiene de escritura: ningun test pide una ruta del repo fuera de artifactDir",
          "[hygiene][resourcepaths][writes]")
{
    // Este es el guard que convierte "ningun test escribe en ficheros versionados"
    // de convencion en invariante. Los barridos anteriores miraban
    // getCurrentWorkingDirectory y las rutas absolutas, pero los casos que de
    // verdad ensuciaban el arbol (docs/qa/runs/*.json, fixtures/evaluations/*.json,
    // assets/presets/*.json) no tocaban ninguno de los dos: escribian a traves de
    // un ACCESOR que devuelve una ruta del repo, y ningun patron de texto delata
    // un accessor. Un guard que no ve el fallo que motives la incidencia no
    // protege de el.
    const juce::File testsDir = juce::File(__FILE__).getParentDirectory();

    REQUIRE(testsDir.isDirectory());

    const auto result = scanForForbiddenPatterns (testsDir, false, "*.cpp",
                                                  allowedRepoPathReaders(), kGuardFileName,
                                                  repoWritePathNeedles(), true, false);

    INFO("Ficheros de test escaneados: " << result.scanned);
    INFO("Ficheros de test que piden una ruta del repo sin justificar: " << result.offenders.size());

    for (const auto& offender : result.offenders)
        FAIL_CHECK(offender);

    for (const auto& missing : result.unreadable)
        FAIL_CHECK("No se pudo leer: " + missing);

    REQUIRE(result.unreadable.empty());
    REQUIRE(result.scanned > 0);
}

TEST_CASE("Hygiene de escritura: las fixtures de un test viven en su scratch, no en %TEMP% crudo",
          "[hygiene][resourcepaths][writes]")
{
    // Un temporal crudo tiene dos fallos que el scratch por test no tiene: el
    // nombre fijo lo comparten dos tests que se pisan, y nadie lo borra, asi que
    // los restos se acumulan en %TEMP% y una corrida interrumpida hereda un
    // directorio a medio construir como si fuera valido.
    const juce::File testsDir = juce::File(__FILE__).getParentDirectory();

    REQUIRE(testsDir.isDirectory());

    const auto tests = scanForForbiddenPatterns (testsDir, false, "*.cpp",
                                                 allowedRawTempDirUsers(), kGuardFileName,
                                                 rawTempDirNeedles(), true, false);

    INFO("Ficheros de test escaneados: " << tests.scanned);
    INFO("Ficheros de test con temporal crudo: " << tests.offenders.size());

    for (const auto& offender : tests.offenders)
        FAIL_CHECK(offender);

    for (const auto& missing : tests.unreadable)
        FAIL_CHECK("No se pudo leer: " + missing);

    REQUIRE(tests.unreadable.empty());
    REQUIRE(tests.scanned > 0);

    // El helper es el unico autorizado a citar tempDirectory: es el punto donde se
    // decide el scratch raiz. Sin este segundo barrido, migrar un test seria solo
    // mover el problema de sitio.
    const juce::File supportDir = testsDir.getChildFile("support");

    REQUIRE(supportDir.isDirectory());

    const auto support = scanForForbiddenPatterns (supportDir, false, "*.cpp",
                                                    { "LabTestScratch.cpp" }, kGuardFileName,
                                                    rawTempDirNeedles(), true, false);

    INFO("Helpers escaneados: " << support.scanned);

    for (const auto& offender : support.offenders)
        FAIL_CHECK(offender);

    for (const auto& missing : support.unreadable)
        FAIL_CHECK("No se pudo leer: " + missing);

    REQUIRE(support.unreadable.empty());
    REQUIRE(support.scanned > 0);
}

TEST_CASE("LabTestScratch: el scratch de un test existe, se vacia y nunca cae en el repo",
          "[hygiene][resourcepaths][writes][scratch]")
{
    using namespace abdaudiolab::test;

    // 1. El scratch se crea y vive en el temporal del sistema.
    const auto root = scratchRoot();
    const auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory);

    REQUIRE(tempDir.isDirectory());
    REQUIRE(root.isAChildOf (tempDir));

    const auto dir = scratchDir ("hygiene_self_check");

    REQUIRE(dir.isDirectory());
    REQUIRE(dir.isAChildOf (root));
    REQUIRE_FALSE(isInsideRepo (dir));

    // 2. Volver a pedirlo lo vacia: dos corridas seguidas del mismo test empiezan
    // desde el mismo estado, y una corrida interrumpida no hereda restos.
    dir.getChildFile("basura.txt").replaceWithText ("contenido");

    REQUIRE(dir.getChildFile("basura.txt").existsAsFile());

    const auto again = scratchDir ("hygiene_self_check");

    REQUIRE(again == dir);
    REQUIRE_FALSE(again.getChildFile("basura.txt").exists());

    // 3. clearScratchDir() borra, y es idempotente.
    clearScratchDir ("hygiene_self_check");
    REQUIRE_FALSE(dir.exists());
    REQUIRE_NOTHROW (clearScratchDir ("hygiene_self_check"));

    // 4. La invariante que hace que el helper sirva de algo. Se comprueba sobre
    //    la raiz del repo y sobre un directorio REAL suyo, porque isAChildOf() no
    //    es reflexivo y el caso de igualdad es justo el peligroso.
    const auto repoRoot = abdaudiolab::core::requireRepoRoot();

    REQUIRE(isInsideRepo (repoRoot));
    REQUIRE(isInsideRepo (repoRoot.getChildFile ("docs")));
    REQUIRE(isInsideRepo (repoRoot.getChildFile ("docs/qa/runs")));
    REQUIRE_FALSE (isInsideRepo (root));
    REQUIRE_FALSE (isInsideRepo (tempDir));
    REQUIRE_FALSE (isInsideRepo (juce::File()));

    // 5. Un nombre de test con acentos, comas y corchetes (como los que Catch2
    //    genera al desglosar secciones) se vuelve un nombre de directorio valido.
    const auto sanitized = sanitizeScratchName ("Fase 20.11.5: [T5] Medicion, \"caso\"");

    REQUIRE(sanitized.isNotEmpty());
    REQUIRE(sanitized.length() <= 96);

    for (auto ch : sanitized)
        REQUIRE((std::isalnum (static_cast<unsigned char> (ch)) != 0
                 || ch == '.' || ch == '_' || ch == '-'));

    REQUIRE(sanitizeScratchName ("").isEmpty());

    // 6. artifactDir() sin opt-in devuelve el scratch; con opt-in, el repo. La
    //    rama se elige con el valor REAL del proceso, de modo que correr la suite
    //    con ABD_REGENERATE_ARTIFACTS=1 ejercita la otra mitad sin duplicar test.
    const auto artifact = artifactDir ("docs/qa/runs", "hygiene_self_check");

    if (artifactRegenerationEnabled())
    {
        REQUIRE(artifact == abdaudiolab::core::docsQaRunsDir());
        REQUIRE(isInsideRepo (artifact));
    }
    else
    {
        REQUIRE(artifact.isAChildOf (root));
        REQUIRE_FALSE(isInsideRepo (artifact));
    }

    clearScratchDir ("hygiene_self_check");
    clearScratchDir ("hygiene_self_check_docs_qa_runs");
}
TEST_CASE("Hygiene de rutas: ningun documento de docs/ enlaza a la maquina del autor",
          "[hygiene][resourcepaths][docs]")
{
    // Lo que este guard caza es distinto de lo que cazan los de codigo: un
    // enlace markdown `file:///d:/desarrollos/...` no rompe la ejecucion, rompe
    // la lectura. El enlace no resuelve en la maquina de nadie mas y el documento
    // que lo contiene parece actualizado cuando su unica referencia viva acaba
    // de romperse en silencio.
    const auto repoRoot = abdaudiolab::core::requireRepoRoot();
    const juce::File docsDir = repoRoot.getChildFile("docs");

    REQUIRE(repoRoot.isDirectory());
    REQUIRE(docsDir.isDirectory());

    // CommentSyntax::none, no cxx. Markdown no tiene comentarios, y el stripper
    // de C++ aplicado a un glob como `raw_audio/*.wav` entra en modo comentario y
    // no vuelve a salir: las 253 lineas siguientes de docs/ROADMAP.md quedaban
    // invisibles. Medido: 22 infracciones con el stripper, 73 sin el.
    const auto result = scanForForbiddenPatterns (docsDir, true, "*.md;*.json;*.txt;*.yml",
                                                  allowedPathCitationDocuments(), kGuardFileName,
                                                  machinePathNeedles(), true, true,
                                                  vendoredDocumentationTrees(),
                                                  CommentSyntax::none);

    INFO("Documentos de docs/ escaneados: " << result.scanned);
    INFO("Documentos de docs/ que enlazan a una ruta personal: " << result.offenders.size());

    for (const auto& offender : result.offenders)
        FAIL_CHECK(offender);

    for (const auto& missing : result.unreadable)
        FAIL_CHECK("No se pudo leer: " + missing);

    REQUIRE(result.unreadable.empty());
    REQUIRE(result.scanned > 0);
}

TEST_CASE("Hygiene de rutas: ningun documento de la raiz del repo cita la maquina del autor",
          "[hygiene][resourcepaths][docs]")
{
    // Los 43 PLAN_*.md, MATRIX_*.md y ACTA_*.md sueltos en la raiz quedan fuera
    // de cualquier barrido que se fije en docs/. Y concentran mas rutas
    // personales que docs/ entero, asi que dejarlos fuera haria que el guard
    // protegiera la mitad pequena del problema.
    const auto repoRoot = abdaudiolab::core::requireRepoRoot();

    REQUIRE(repoRoot.isDirectory());

    // No recursivo a proposito: en la raiz solo interesan los documentos, y
    // bajar entraria en src/, docs/ y el arbol vendorizado.
    const auto result = scanForForbiddenPatterns (repoRoot, false, "*.md",
                                                  allowedPathCitationDocuments(), kGuardFileName,
                                                  machinePathNeedles(), true, true, {},
                                                  CommentSyntax::none);

    INFO("Documentos de la raiz escaneados: " << result.scanned);
    INFO("Documentos de la raiz que citan una ruta personal: " << result.offenders.size());

    for (const auto& offender : result.offenders)
        FAIL_CHECK(offender);

    for (const auto& missing : result.unreadable)
        FAIL_CHECK("No se pudo leer: " + missing);

    REQUIRE(result.unreadable.empty());
    REQUIRE(result.scanned > 0);
}
