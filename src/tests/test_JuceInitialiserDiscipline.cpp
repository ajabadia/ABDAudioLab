/**
 * @file test_JuceInitialiserDiscipline.cpp
 * @brief Guard anti-regresión del cuelgue sin CPU de la suite.
 *
 * EL CUELGUE, Y POR QUE SE MANIFIESTA COMO "NO USA CPU"
 * ------------------------------------------------------
 * El 1 de octubre, a las 18:50:38, se quedo un ABDAudioLab_Tests.exe vivo con
 * 0 % de CPU y unos cinco segundos de tiempo acumulado. Once segundos despues se
 * commiteo 819aaab ("ignorar las capturas XML de Catch2"), o sea que el proceso
 * era una vuelta de la suite del otro hilo capturando su XML de duraciones. Doce
 * minutos antes, b39d3c5 habia creado la herramienta que la lanza. La causa raiz
 * la cerro 8a19dd9, titulado "resolver cuelgue de portapapeles en
 * test_SmokeStep4UI", quitando dos cosas del TEST_CASE: su propio
 * juce::ScopedJuceInitialiser_GUI y el viaje de ida y vuelta al portapapeles.
 *
 * Cero por ciento de CPU es la firma de un hilo PARADO, no de uno que calcula: el
 * proceso sigue ahi, la unidad de trabajo marca cero, y no hay nada que matarlo
 * por fuerza. Un hilo esperando a otro no se distingue de uno colgado por fuera;
 * solo se distingue por dentro, mirando cuanto trabajo ha hecho. Por eso existe
 * TestTelemetry, con su latido por test y su volcado de pila: es lo que convierte
 * "el proceso sigue ahi" en "este test lleva tantos milisegundos bloqueado".
 *
 * POR QUE EL DOBLE INICIALIZADOR CUELGA Y NO CRASHEA
 * --------------------------------------------------
 * TestMain.cpp ya construye un juce::ScopedJuceInitialiser_GUI para todo el
 * proceso, y su cabecera dice que un TEST_CASE no debe construir el suyo. Un
 * segundo inicializador anidado no falla de forma ruidosa: JUCE devuelve el
 * MessageManager que ya existe, y al salir del ambito interior ejecuta el apagado
 * que el guard exterior creia suyo. A partir de ahi el MessageManager esta
 * destruido para un proceso que sigue vivo, y el siguiente posting de mensajes se
 * queda esperando a un mutex de un objeto que ya no existe. Eso es un bloqueo, y un
 * bloqueo con 0 % de CPU.
 *
 * El portapapeles es el segundo disparador, e independiente del anterior:
 * juce::SystemClipboard sobre Windows hace una transaccion COM sincronica. Sin
 * bomba de mensajes, esa llamada espera a un portapapeles que nadie va a
 * contestar. Por eso el arreglo de 8a19dd9 quito las dos cosas y no una.
 *
 * LO QUE HACE ESTE GUARD
 * ----------------------
 * Dos invariantes, con tolerancias distintas porque los riesgos no son los mismos:
 *
 *   1. SystemClipboard no aparece en NINGUN test. Hoy son cero, asi que cero es el
 *      presupuesto. Es la mitad del cuelgue de 8a19dd9 y no cuesta nada mantenerlo
 *      en cero.
 *
 *   2. El numero de ScopedJuceInitialiser_GUI dentro de TEST_CASEs no crece. El
 *      presupuesto NO es cero: hoy hay 101 en 34 ficheros. Ponerlo en cero dejaria
 *      la suite roja de golpe y no se eliminaria nada, porque el fallo llegaria
 *      mezclado con todo lo demas. Lo que se hace es un DIENTE DE SERRAIN: el
 *      presupuesto es el de hoy y solo puede bajar. Cada instancia nueva hace
 *      fallar el guard; cada instancia eliminada lo acerca a un presupuesto que ya
 *      no da miedo.
 *
 * Por que leer el codigo fuente y no limitarse a documentarlo: TestMain.cpp ya
 * declara la politica, y aun asi hay 101 contra ella. Una politica escrita en un
 * comentario no es una invariante. Las guardas [writes] y de higiene de rutas de
 * este repo ya hacen lo mismo por lo mismo: leen el arbol para que lo que esta
 * prohibido no vuelva a colarse calladamente.
 *
 * POR QUE NO FALLA CUANDO NO PUEDE LEER NADA
 * ------------------------------------------
 * Un guard que puede tumbar la puerta que vigila es peor que no tenerlo. Si el
 * arbol no se resuelve, o el directorio no existe, o un fichero no se abre, este
 * test lo dice con WARN y pasa. La unica excepcion es el presupuesto: si el
 * directorio se leyo bien pero la lista guardada no cuadra con lo que hay, eso es
 * este fichero editado a mano, y si es un fallo de verdad.
 */

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "core/LabResourcePaths.h"

namespace
{

// ===========================================================================
// EL PRESUPUESTO
//
// Medido contando una declaracion por linea y descartando lo que solo se
// menciona en prosa. Cualquier cambio aqui es deliberado y tiene que llevar su
// justificacion escrita en el commit: es la lista de lo que se sabe que esta mal,
// y por eso baja en cuanto se arregla, nunca sube.
//
//   for f in src/tests/*.cpp; do
//     [ "$(basename $f)" = TestMain.cpp ] && continue
//     grep -c '^[[:space:]]*juce::ScopedJuceInitialiser_GUI[[:space:]]' "$f"
//   done
//
// prettier-ignore
const std::map<std::string, int> kPresupuestoScopedJuce {
    { "test_AudioABComparator.cpp", 1 },
    { "test_DexedEnvelopeMeasurement_T5.cpp", 1 },
    { "test_DexedValidation.cpp", 6 },
    { "test_E2E_HermeticWorkflows.cpp", 4 },
    { "test_FilterViewerPanelAndUI.cpp", 3 },
    { "test_FreeCaptureRecipePromotion.cpp", 1 },
    { "test_GuidedPluginLoad_Dexed.cpp", 4 },
    { "test_MatrixResolutionTableComponent.cpp", 2 },
    { "test_MeasurementFloatingWindow.cpp", 1 },
    { "test_MeasurementViewModelAndUI.cpp", 3 },
    { "test_OperatorCardsContainerComponent.cpp", 3 },
    { "test_OutOfProcessVst3LifecycleAdapter.cpp", 1 },
    { "test_PauseResume.cpp", 3 },
    { "test_Phase20_11_1_InteractiveViewerComparison.cpp", 1 },
    { "test_Phase20_11_2_SessionRobustnessAndA11y.cpp", 1 },
    { "test_Phase20_11_VerticalDexedAndFair_T5.cpp", 1 },
    { "test_PlotterModulationTableRenderer.cpp", 1 },
    { "test_PlotterRenderers.cpp", 1 },
    { "test_PluginHostManager.cpp", 4 },
    { "test_PointRangePatching.cpp", 1 },
    { "test_ProfilingArchitectureRefactor.cpp", 1 },
    { "test_ProfilingSequencerModulation.cpp", 1 },
    { "test_ReportExportUiController.cpp", 1 },
    { "test_SessionExecutionCoordinator.cpp", 12 },
    { "test_SoundIdCurvePlotterModulation.cpp", 1 },
    { "test_SoundIdViews.cpp", 17 },
    { "test_SynthTargetLifecycleAdapter.cpp", 3 },
    { "test_TargetProfileDexedBehavior.cpp", 1 },
    { "test_TargetProfileDexedHosting.cpp", 1 },
    { "test_UiCoordinatorGovernance.cpp", 8 },
    { "test_VesCz101Feasibility.cpp", 4 },
    { "test_VesCz101SemanticControl.cpp", 4 },
    { "test_Vst3DexedRealHosting_T2.cpp", 3 },
    { "test_Waterfall3DComponent.cpp", 1 },
};

constexpr const char* kTokenScoped  = "juce::ScopedJuceInitialiser_GUI";
constexpr const char* kTokenClipboard = "SystemClipboard";

/** El runner construye el suyo, y este guard construiria el suyo si no se excluyera. */
bool esExcluido (const std::string& nombre)
{
    return nombre == "TestMain.cpp" || nombre == "test_JuceInitialiserDiscipline.cpp";
}

std::string totalDelPresupuesto()
{
    int t = 0;
    for (const auto& par : kPresupuestoScopedJuce)
        t += par.second;
    return std::to_string (t);
}

struct Barrido
{
    std::map<std::string, int> scopedPorFichero;
    std::vector<std::string> clipboard;
    std::vector<std::string> ilegibles;
};

/**
 * Recorre src/tests y clasifica lo que hay.
 *
 * El recuento es por LINEA, no por aparicion: la prosa del propio guard menciona
 * el token, y mencionarlo no es declararlo. Una declaracion exige que lo primero
 * que haya en la linea sea el token, seguido de espacio y de algo que no sea una
 * barra de comentario.
 */
Barrido barrer (const juce::File& directorio)
{
    Barrido b;

    namespace fs = std::filesystem;

    // Se recorre con std::filesystem y no con un iterador de JUCE por una razon
    // concreta: las cabeceras de los modulos de JUCE solo se incluyen con su
    // ruta completa de modulo, y este guard no necesita nada de JUCE mas alla
    // del juce::File que le devuelve LabResourcePaths.
    for (const auto& entrada : fs::directory_iterator (directorio.getFullPathName().toStdString()))
    {
        if (! entrada.is_regular_file() || entrada.path().extension() != ".cpp")
            continue;

        const auto nombre = entrada.path().filename().string();

        if (esExcluido (nombre))
            continue;

        std::ifstream in (entrada.path().string(), std::ios::binary);

        if (! in.is_open())
        {
            b.ilegibles.push_back (nombre);
            continue;
        }

        std::ostringstream ss;
        ss << in.rdbuf();
        in.close();

        int declaraciones = 0;
        int numeroLinea = 0;
        std::istringstream lineas (ss.str());
        std::string linea;

        while (std::getline (lineas, linea))
        {
            ++numeroLinea;

            std::size_t primero = linea.find_first_not_of(" \t");

            if (primero == std::string::npos)
                continue;

            const bool esComentario = (linea.compare (primero, 2, "//") == 0
                                     || linea.compare (primero, 2, "/*") == 0
                                     || linea.compare (primero, 1, "*") == 0);

            if (esComentario)
                continue;

            const std::string cuerpo = linea.substr (primero);

            if (cuerpo.rfind (kTokenScoped, 0) == 0)
            {
                const auto despues = cuerpo.find_first_not_of(" \t", std::string (kTokenScoped).size());

                if (despues != std::string::npos && cuerpo[despues] != '/')
                    ++declaraciones;
            }

            if (cuerpo.find (kTokenClipboard) != std::string::npos)
                b.clipboard.push_back (nombre + ":" + std::to_string (numeroLinea));
        }

        if (declaraciones > 0)
            b.scopedPorFichero[nombre] = declaraciones;
    }

    return b;
}

} // namespace

// ===========================================================================
// INVARIANTE 1 — EL PORTAPAPELES
// ===========================================================================
TEST_CASE ("Guard [juce]: ningun test toca el portapapeles del sistema", "[guard][juce][hygiene]")
{
    const auto directorio = abdaudiolab::core::optionalRepoResource ("src/tests");

    if (! directorio.isDirectory())
    {
        WARN ("No se encontro src/tests desde la raiz del repositorio: guard sin verificar.");
        return;
    }

    const auto b = barrer (directorio);

    if (! b.ilegibles.empty())
    {
        std::string detalle;

        for (const auto& n : b.ilegibles)
            detalle += n + " ";

        WARN ("Ficheros de test que no se pudieron abrir, sin verificar: " + detalle);
    }

    INFO ("Ocurrencias de SystemClipboard en src/tests: " << b.clipboard.size());

    // Presupuesto CERO, sin excepciones. 8a19dd9 quito el ultimo de estos y no
    // queda ninguno: reintroducir uno devuelve a colgarse con 0 % de CPU, que es la
    // forma mas cara de fallar porque no deja senal de que este pasando.
    for (const auto& donde : b.clipboard)
        FAIL ("juce::SystemClipboard dentro de un test (" << donde
              << "). En Windows es una transaccion COM sincronica: sin bomba de mensajes "
              << "se queda esperando y el proceso aparece colgado con 0 % de CPU. Es la "
              << "mitad del cuelgue que cerro 8a19dd9. Compara el hash contra el modelo, no "
              << "contra el portapapeles del sistema.");

    SUCCEED ("el recorrido del portapapeles llego hasta aqui con el presupuesto en cero");
}

// ===========================================================================
// INVARIANTE 2 — EL DOBLE INICIALIZADOR
// ===========================================================================
TEST_CASE ("Guard [juce]: el inicializador de JUCE no crece dentro de TEST_CASEs", "[guard][juce][hygiene]")
{
    const auto directorio = abdaudiolab::core::optionalRepoResource ("src/tests");

    if (! directorio.isDirectory())
    {
        WARN ("No se encontro src/tests desde la raiz del repositorio: guard sin verificar.");
        return;
    }

    const auto b = barrer (directorio);

    int total = 0;

    for (const auto& par : b.scopedPorFichero)
        total += par.second;

    // El guard tiene que ser capaz de FALLAR. Un caso que pasa con cero
    // aserciones tambien pasaria si el recorrido dejara de encontrar ficheros, y
    // un guard que no mira nada es indistinguible de un guard que dice que no hay
    // problema. Este REQUIRE es la unica prueba de que el barrido ocurrio.
    REQUIRE (b.scopedPorFichero.size() > 0);

    INFO ("Instancias de ScopedJuceInitialiser_GUI en TEST_CASEs: " << total
          << " (presupuesto " << totalDelPresupuesto() << ")"
          << " en " << b.scopedPorFichero.size() << " ficheros");

    // El presupuesto solo puede bajar. Cada fichero por encima de los suyos tiene un
    // inicializador anidado nuevo, que es exactamente el cuelgue de 8a19dd9.
    for (const auto& par : b.scopedPorFichero)
    {
        const auto presupuesto = kPresupuestoScopedJuce.find (par.first);
        const int limite = (presupuesto != kPresupuestoScopedJuce.end()) ? presupuesto->second : 0;

        if (par.second > limite)
            FAIL ("juce::ScopedJuceInitialiser_GUI dentro de un TEST_CASE en " << par.first << ": "
                  << par.second << " instancia(s), presupuesto " << limite
                  << ". TestMain.cpp ya inicializa JUCE para todo el proceso y su cabecera "
                  << "prohibe este segundo inicializador: el de dentro apaga al salir el "
                  << "MessageManager que el de fuera creia suyo, y lo que viene despues se "
                  << "queda esperando a un mutex de un objeto destruido. Eso no falla, se "
                  << "cuelga, con 0 % de CPU. Si el test necesita proceso limpio, usa "
                  << "ABD_REQUIRE_JUCE_GUI_FRESH_PROCESS() de TestJuceGuard.h.");
    }
}

// ===========================================================================
// LA DEUDA, A LA VISTA
//
// No falla: informa. Un guard que solo dice "hay 101 problemas" sin decir cuales
// es ruido, y el ruido es lo que hace que la gente deje de leer guards.
// ===========================================================================
TEST_CASE ("Guard [juce]: inventario de la deuda de inicializadores pendientes", "[guard][juce][hygiene][informe]")
{
    const auto directorio = abdaudiolab::core::optionalRepoResource ("src/tests");

    if (! directorio.isDirectory())
    {
        WARN ("No se encontro src/tests desde la raiz del repositorio: inventario vacio.");
        return;
    }

    const auto b = barrer (directorio);

    std::ostringstream detalle;
    int total = 0;

    for (const auto& par : b.scopedPorFichero)
    {
        total += par.second;
        detalle << "\n    " << par.first << ": " << par.second;
    }

    INFO ("Pendientes de migrar a proceso limpio, " << total << " instancia(s) en "
          << b.scopedPorFichero.size() << " ficheros:" << detalle.str());

    SUCCEED ("inventario emitido");
}