// =============================================================================
// ABDAudioLab - La ventana flotante de medicion, y por que no depende del panel
// =============================================================================
//
// QUE HACE ESTO.
//
// La ventana de medicion se extrajo de `MainContentComponent.cpp` a
// `src/gui/MeasurementFloatingWindow.{h,cpp}`. La extraccion se comprobo
// compilando sus dos unidades y mirando simbolos con dumpbin, que es una forma
// de no comprobar nada: `dumpbin` responde igual si el codigo es correcto y si
// el enlazado se va a caer al abrir la aplicacion. Lo que faltaba era que la
// ventana se montara de verdad.
//
// Aqui se monta. En memoria, sin escritorio: JUCE construye un `Component`
// entero con `MessageManager` pero sin `addToDesktop()`, asi que esto corre en
// el runner headless de Catch2.
//
// ----------------------------------------------------------------------------
// POR QUE EL CONTENIDO ES UN COMPONENTE TONTO Y NO UNO DE LOS DOS PANELES.
//
// Porque `updateTheme()` tiene una lista cerrada de dos paneles concretos a los
// que re-tematiza, y los dos son `dynamic_cast`. Si aqui metiesemos uno de
// ellos, el test pasaria por el camino que ya se sabe que funciona y no
// diria nada del otro: el que se puede equivocar sin que se note al usar la
// aplicacion, que es el marco de la ventana.
//
// Por eso el contenido es un `Component` que no es ninguno de los dos. Los dos
// `dynamic_cast` no coinciden, y aun asi el marco tiene que quedar con el color
// del tema. Y el color se comprueba DESPUES de romperlo a proposito: el
// constructor ya pone `AppTheme::BackgroundApp`, asi que mirar el color tal
// cual no distinguiria "updateTheme funciona" de "el constructor lo puso".
//
// ----------------------------------------------------------------------------
// QUE NO HACE ESTE TEST, Y POR QUE.
//
// No prueba el pixel, ni la barra de titulo nativa (no hay `ComponentPeer`
// porque no se llama a `addToDesktop()`), ni que el raton abra algo. Eso
// necesita un escritorio, y un test que necesita un escritorio no lo ejecuta
// nadie dos veces.
//
// Tampoco prueba el camino POSITIVO: que con un `MeasurementViewerPanel` o un
// `MeasurementComparisonPanel` dentro, `updateTheme()` llegue a llamarles. Eso
// necesita el contexto completo de la aplicacion (el view model, la sesion), y
// un panel de medicion a medio construir no mide nada. Aqui se comprueba que la
// ventana no DEPENDE de esos dos, que es la parte que estaba sin probar.
// =============================================================================

#include <catch2/catch_test_macros.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include "gui/AppTheme.h"
#include "gui/MeasurementFloatingWindow.h"

using namespace abdaudiolab;

namespace
{

/** Contenido que no es ninguno de los dos paneles que `updateTheme()` conoce. */
class ContenidoGenerico : public juce::Component
{
public:
    int vecesThemed = 0;

    void updateTheme() { ++vecesThemed; }
};

/** Un `juce::Colour` no sabe imprimirse, asi que Catch2 lo saca como `{?}`. */
std::string comoHex (juce::Colour color)
{
    // Ocho digitos con los ceros a la izquierda: `toHexString` los quita.
    juce::String hex = juce::String::toHexString (static_cast<juce::uint32> (color.getARGB()));

    while (hex.length() < 8)
        hex = juce::String ("0") + hex;

    return (juce::String ("#") + hex).toStdString();
}

} // namespace

TEST_CASE ("La ventana flotante se monta y se re-tematiza sin conocer su contenido",
           "[gui][measurement][floating_window]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    SECTION ("El constructor se queda el contenido y la geometria que le pasan")
    {
        auto* contenido = new ContenidoGenerico();
        gui::MeasurementFloatingWindow ventana ("Prueba headless",
                                                contenido,
                                                640, 480,
                                                320, 240);

        CHECK (ventana.getContentComponent() == contenido);
        CHECK (ventana.getWidth()  == 640);
        CHECK (ventana.getHeight() == 480);
        CHECK (ventana.isResizable());
        CHECK (ventana.getTitle() == juce::String ("Prueba headless"));

        // Montarla no tematiza: tematizar es una decision, no un efecto lateral
        // de construirse.
        CHECK (contenido->vecesThemed == 0);
    }

    SECTION ("setResizeLimits deja los limites que se le piden")
    {
        gui::MeasurementFloatingWindow ventana ("Limites",
                                                new ContenidoGenerico(),
                                                800, 600,
                                                320, 240);

        // `DocumentWindow` hereda de `ResizableWindow`, y en esta JUCE los
        // limites viven en el `ComponentBoundsConstrainer`, no en getters de
        // `Component`.
        const auto* limites = ventana.getConstrainer();

        REQUIRE (limites != nullptr);
        CHECK (limites->getMinimumWidth()  == 320);
        CHECK (limites->getMinimumHeight() == 240);
        CHECK (limites->getMaximumWidth()  == 2560);
        CHECK (limites->getMaximumHeight() == 1440);
    }

    SECTION ("updateTheme() repone el color del marco aunque ningun dynamic_cast coincida")
    {
        auto* contenido = new ContenidoGenerico();
        gui::MeasurementFloatingWindow ventana ("Tema",
                                                contenido,
                                                640, 480,
                                                320, 240);

        // El constructor ya deja el fondo en el color del tema. Sin romperlo
        // antes, este CHECK passaria aunque `updateTheme()` no hiciera nada.
        ventana.setBackgroundColour (juce::Colours::magenta);
        INFO ("sabotaje: " << comoHex (ventana.getBackgroundColour()));
        CHECK (ventana.getBackgroundColour() == juce::Colours::magenta);

        // El diagnostico que vera quien lea un fallo tiene que ser legible; si
        // `comoHex` se rompiera, todos los INFO de este test dirian "{?}" y el
        // fallo volveria a ser inaccionable.
        CHECK (comoHex (juce::Colours::magenta) == "#ffff00ff");

        ventana.updateTheme();

        INFO ("tras updateTheme: " << comoHex (ventana.getBackgroundColour()));
        CHECK (ventana.getBackgroundColour() == gui::AppTheme::BackgroundApp);

        // Y el contenido desconocido no se toca: la lista cerrada sigue siendo
        // una lista cerrada, no un `updateTheme()` generico.
        CHECK (contenido->vecesThemed == 0);
    }

    SECTION ("updateTheme() se puede llamar dos veces sin que cambie nada")
    {
        gui::MeasurementFloatingWindow ventana ("Idempotente",
                                                new ContenidoGenerico(),
                                                640, 480,
                                                320, 240);

        ventana.updateTheme();
        const auto colorTrasUna = ventana.getBackgroundColour();

        ventana.updateTheme();

        INFO ("tras la primera: " << comoHex (colorTrasUna)
                                 << " | tras la segunda: "
                                 << comoHex (ventana.getBackgroundColour()));
        CHECK (ventana.getBackgroundColour() == colorTrasUna);
        CHECK (ventana.getBackgroundColour() == gui::AppTheme::BackgroundApp);
    }

    SECTION ("Cerrar oculta la ventana y deja el contenido vivo")
    {
        auto* contenido = new ContenidoGenerico();
        gui::MeasurementFloatingWindow ventana ("Cierre",
                                                contenido,
                                                640, 480,
                                                320, 240);

        ventana.setVisible (true);
        ventana.closeButtonPressed();

        // Si `closeButtonPressed()` destruyera la ventana, esto ya estaria
        // colgando de un `this` que no existe. Es el fallo caro: no un assert,
        // un acceso a memoria liberada la proxima vez que se tematice.
        CHECK_FALSE (ventana.isVisible());
        CHECK (ventana.getContentComponent() == contenido);

        // Y tematizar despues de cerrar tampoco revienta.
        CHECK_NOTHROW (ventana.updateTheme());
        INFO ("re-tematizado tras cerrar: " << comoHex (ventana.getBackgroundColour()));
        CHECK (ventana.getBackgroundColour() == gui::AppTheme::BackgroundApp);
    }

    SECTION ("Dos ventanas no se pisan")
    {
        auto* uno = new ContenidoGenerico();
        auto* otro = new ContenidoGenerico();

        gui::MeasurementFloatingWindow primera  ("Primera",  uno,   640, 480, 320, 240);
        gui::MeasurementFloatingWindow segunda ("Segunda", otro,  300, 200, 100, 100);

        CHECK (primera.getContentComponent()  == uno);
        CHECK (segunda.getContentComponent() == otro);
        CHECK (primera.getWidth()  == 640);
        CHECK (segunda.getWidth() == 300);

        segunda.updateTheme();

        CHECK (uno->vecesThemed   == 0);
        CHECK (otro->vecesThemed  == 0);
        INFO ("segunda: " << comoHex (segunda.getBackgroundColour()));
        CHECK (segunda.getBackgroundColour() == gui::AppTheme::BackgroundApp);
    }
}
