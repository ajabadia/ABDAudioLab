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
// POR QUE HAY DOS CONTENIDOS Y SOLO UNO ES DE LOS PANELES DE VERDAD.
//
// `updateTheme()` usaba dos `dynamic_cast`, uno por panel, y eso era una lista
// cerrada escrita como una pregunta de tipos en tiempo de ejecucion. Con el
// contrato `measurement::MeasurementThemedPanel` la lista la lleva el
// compilador, y aqui se pueden comprobar las DOS mitades sin construir la
// aplicacion:
//
//   - Un `Component` tonto que no implementa el contrato. No se re-tematiza,
//     pero el marco de la ventana si. Ese camino se puede equivocar sin que se
//     note al usar la aplicacion, y por eso es el que mas merece un test.
//   - Un panel de mentira que SI implementa el contrato. Antes esto no se
//     podia probar sin levantar un `MeasurementViewerPanel` entero con su view
//     model y su sesion, que es justamente lo que no se puede montar en un
//     runner headless. Con el contrato, un panel falso de seis lineas
//     comprueba lo mismo.
//
// Y el color del marco se comprueba DESPUES de romperlo a proposito: el
// constructor ya deja `AppTheme::BackgroundApp`, asi que mirar el color tal cual
// no distinguiria "updateTheme funciona" de "el constructor lo puso".
//
// ----------------------------------------------------------------------------
// QUE NO HACE ESTE TEST, Y POR QUE.
//
// No prueba el pixel, ni la barra de titulo nativa (no hay `ComponentPeer`
// porque no se llama a `addToDesktop()`), ni que el raton abra algo. Eso
// necesita un escritorio, y un test que necesita un escritorio no lo ejecuta
// nadie dos veces.
//
// Tampoco comprueba que los dos paneles REALES implementen el contrato. Eso lo
// dice el compilador: si `MeasurementViewerPanel` no heredara de
// `MeasurementThemedPanel`, no habria forma de pasarlo al constructor que
// re-tematiza, y `MainContentComponent.cpp` no compilaria.
// =============================================================================

#include <catch2/catch_test_macros.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include "gui/AppTheme.h"
#include "gui/MeasurementFloatingWindow.h"
#include "gui/measurement/MeasurementThemedPanel.h"

using namespace abdaudiolab;

namespace
{

/** Contenido que NO implementa el contrato, asi que la ventana no lo re-tematiza.
 *  El metodo existe a proposito, y no se llama nunca: si se llamara, el test
 *  estaria probando el camino del panel, no el del marco. */
class ContenidoGenerico : public juce::Component
{
public:
    int vecesThemed = 0;

    void updateTheme() { ++vecesThemed; }
};

/** El otro lado del contrato: un panel que SI sabe re-tematizarse. Seis lineas
 *  en vez de un `MeasurementViewerPanel` entero, y comprueba lo mismo, porque la
 *  ventana ya no lo conoce: solo conoce el contrato. */
class PanelDeMentira : public gui::measurement::MeasurementThemedPanel
{
public:
    int vecesThemed = 0;

    void updateTheme() override { ++vecesThemed; }
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

    SECTION ("updateTheme() repone el color del marco aunque el contenido no sepa tematizarse")
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

        // Y el contenido que no implementa el contrato no se toca. Que un
        // `Component` tenga un metodo `updateTheme` no lo convierte en panel:
        // se re-tematiza quien lo declara, no quien se parece a quien lo
        // declara.
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

    SECTION ("Con un panel que SI implementa el contrato, updateTheme() lo alcanza")
    {
        auto* panel = new PanelDeMentira();
        gui::MeasurementFloatingWindow ventana ("Contrato",
                                                *panel,
                                                640, 480,
                                                320, 240);

        // Montarla no tematiza: tematizar sigue siendo una decision, no un efecto
        // lateral de construirse. El constructor solo guarda a quien preguntar.
        REQUIRE (ventana.getContentComponent() == panel);
        CHECK (panel->vecesThemed == 0);

        ventana.updateTheme();
        CHECK (panel->vecesThemed == 1);

        // Y cada llamada llega: un panel que solo se re-tematiza al abrir la
        // ventana es un panel que se queda del color anterior en cuanto el tema
        // cambia con las dos abiertas, que es como se usaba la aplicacion.
        ventana.updateTheme();
        CHECK (panel->vecesThemed == 2);

        // Y cerrar y volver a tematizar no lo deja de lado.
        ventana.closeButtonPressed();
        ventana.updateTheme();
        CHECK (panel->vecesThemed == 3);
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
