// ==============================================================================
// ABDAudioLab - Los avisos que no se autodestruyen, y como se dice una retencion
// ==============================================================================
//
// QUE HACE ESTO.
//
// El encargo de origen era "ensena el motivo de la cuarentena en el cajon, con
// un enlace al JSON". Eso esta hecho y vive en `DrawerHardwareTab`. Lo que se
// mide aqui es lo que se le anadio despues, y es una consecuencia de lo primero.
//
// ----------------------------------------------------------------------------
// POR QUE HAY UN PANEL Y NO UN LABEL MAS.
//
// Porque hay dos clases de aviso y se confundian en uno. El aviso de paso --
// "target verificado", "audio reconectado"-- se resuelve solo: se lee y se pasa,
// y un texto que se autodestruye a los seis segundos es justo lo que conviene.
//
// Una retencion no se resuelve sola. El hardware sigue sin aparecer dentro de
// tres semanas, y dentro de tres semanas nadie recuerda un texto que se borro en
// 1999. Y lo peor de un aviso que se borra es que ocupa sitio igual: el que lo
// vio creera que ya lo miro, y no lo miro.
//
// La seccion que lo demuestra es la del cierre: si este panel se borrara como
// el prompt, todo lo de arriba seria decoracion.
//
// ----------------------------------------------------------------------------
// QUE NO HACE ESTE TEST, Y POR QUE.
//
// No prueba el pixel, ni el color, ni que el boton abra el editor. Eso
// necesita un message loop y una maquina con un editor instalado, y un test que
// depende de las dos cosas no lo ejecuta nadie dos veces.
//
// Si mide el TEXTO, que es lo unico que aqui puede estar mal sin que se note al
// usar la aplicacion: que el texto diga "decision editorial, no fallo", y lo
// digan el registro, el adapter y el panel. Los tres. Porque si uno de los tres
// se queda sin actualizar, el canal equivocado es el que se lee primero.
// =============================================================================

#include <catch2/catch_test_macros.hpp>

#include <juce_core/juce_core.h>

#include "core/HardwareContractQuarantine.h"
#include "gui/StartupWarningsPanel.h"

using namespace abdaudiolab;

namespace
{

/** Un retenido de mentira, pero con las tres piezasinformadas. */
core::quarantine::Retenido retenidoDePrueba(const juce::String& nombre,
                                             const juce::String& motivo)
{
    return { juce::File::getSpecialLocation(juce::File::tempDirectory)
                 .getChildFile("abdlab_panel.json"),
             nombre,
             motivo };
}

} // namespace

TEST_CASE("El panel de avisos acumula y no se borra solo",
          "[gui][warnings][quarantine]")
{
    gui::StartupWarningsPanel panel;

    SECTION("Sin avisos el panel no tiene alto ni se ve")
    {
        // El fallo caro de un panel de avisos es el rectangulo vacio: arriba de
        // todo, en cada arranque limpio, que son los que mas se miran.
        CHECK_FALSE(panel.hasNotices());
        CHECK(panel.getPreferredHeight() == 0);
    }

    SECTION("Un aviso lo hace visible y le da alto")
    {
        panel.addNotice("algo que se ha dicho");

        CHECK(panel.hasNotices());
        CHECK(panel.getNoticeCount() == 1);
        CHECK(panel.getPreferredHeight() > 0);
    }

    SECTION("Los avisos se acumulan, y el alto crece con ellos")
    {
        panel.addNotice("primero");
        const int uno = panel.getPreferredHeight();

        panel.addNotice("segundo");
        const int dos = panel.getPreferredHeight();

        panel.addNotice("tercero");

        CHECK(panel.getNoticeCount() == 3);
        CHECK(dos > uno);
        CHECK(panel.getPreferredHeight() > dos);
    }

    SECTION("Cerrar no borra: el aviso sigue estando para releerlo")
    {
        // Es la diferencia con el prompt de seis segundos, y es el motivo de que
        // este panel exista. Si "cerrar" vaciara la lista, volver a leer un aviso
        // exigiria reescanear el catalogo entero.
        panel.addNotice("un retenido");
        panel.setVisible(false);

        CHECK_FALSE(panel.isVisible());
        CHECK(panel.getNoticeCount() == 1);
        CHECK(panel.getPreferredHeight() > 0);
    }

    SECTION("clearNotices si vacia, y vuelve a no tener alto")
    {
        // Lo usa el reescaneo, que empieza de cero.
        panel.addNotice("uno");
        panel.addNotice("otro");
        panel.clearNotices();

        CHECK(panel.getNoticeCount() == 0);
        CHECK(panel.getPreferredHeight() == 0);
    }

    SECTION("Un aviso vacio no cuenta como aviso")
    {
        // Un texto en blanco en una lista de avisos es un hueco que ocupa alto
        // y no dice nada, que es exactamente el defecto que se vino a evitar.
        panel.addNotice(juce::String());

        CHECK(panel.getNoticeCount() == 0);
        CHECK(panel.getPreferredHeight() == 0);
    }

    SECTION("Un indice fuera de rango no revienta")
    {
        panel.addNotice("uno");

        CHECK(panel.getNoticeText(-1).isEmpty());
        CHECK(panel.getNoticeText(7).isEmpty());
        CHECK_FALSE(panel.getNoticeText(0).isEmpty());
    }
}

TEST_CASE("Una retencion se dice IGUAL en el panel, en el aviso y en el log",
          "[gui][warnings][quarantine][schema]")
{
    // El fallo que este caso tapa es de los que no se ven mirando la pantalla:
    // el panel dice "decision editorial, no fallo", el log dice "[CUARENTENA]",
    // y un dia alguien anade un retenido nuevo y solo actualiza uno de los dos.
    // Entonces un canal dice que es una decision y el otro parece un error.
    const auto retenido = retenidoDePrueba("roland_aira_submodules",
                                           "de 31 bloques solo 7 casan");

    SECTION("Las tres palabras estan, y el panel las pone todas")
    {
        gui::StartupWarningsPanel panel;
        panel.addQuarantineNotice(retenido);

        REQUIRE(panel.getNoticeCount() == 1);

        const auto texto = panel.getNoticeText(0);

        INFO("el panel escribe: " << texto.toStdString());

        CHECK(texto.contains("decision editorial"));
        CHECK(texto.contains("no fallo"));
        CHECK(texto.contains("statusReason"));
        CHECK(texto.contains("roland_aira_submodules"));
    }

    SECTION("El panel no se inventa su propia redaccion")
    {
        // El texto sale del modulo de cuarentena. Este CHECK falla en cuanto
        // alguien escribe la frase dentro del panel en vez de llamarla, que es
        // la forma que tiene esto de volverse una segunda verdad.
        gui::StartupWarningsPanel panel;
        panel.addQuarantineNotice(retenido);

        CHECK(panel.getNoticeText(0)
                  == core::quarantine::descripcionDeRetenido(retenido));
    }

    SECTION("La linea de log es la descripcion con el prefijo delante")
    {
        const auto linea = core::quarantine::lineaDeLogDeRetencion(retenido);

        CHECK(linea.startsWith(core::quarantine::prefijoLog));
        CHECK(linea.contains("decision editorial"));

        // Y solo el prefijo de diferencia. Sin este CHECK, un dia se puede
        // escribir la linea de log a mano y un grep por el prefijo devuelve
        // lineas cuyo texto no casa con el del panel.
        CHECK(linea == juce::String(core::quarantine::prefijoLog) + " "
                      + core::quarantine::descripcionDeRetenido(retenido));
    }

    SECTION("El motivo no se pierde y el fichero sale")
    {
        // Un motivo sin su campo de origen no es un motivo: es una afirmacion
        // sin fuente, y la fuente es el JSON.
        const auto texto = core::quarantine::descripcionDeRetenido(retenido);

        CHECK(texto.contains("de 31 bloques solo 7 casan"));
        CHECK(texto.contains(retenido.fichero.getFullPathName()));
    }

    SECTION("Sin motivo, sale el texto inventado")
    {
        // Retenido sin `statusReason` no deja de retenerse, pero su motivo no
        // puede ser el vacio: un retenido mudo es indistinguible de un bug.
        const auto sinMotivo = retenidoDePrueba("algo_retenido", juce::String());
        const auto texto = core::quarantine::descripcionDeRetenido(sinMotivo);

        INFO("sin motivo dice: " << texto.toStdString());
        CHECK(texto.contains(core::quarantine::motivoPorDefecto));
    }

    SECTION("Sin fichero, el texto no inventa uno")
    {
        // El adapter no tiene path, y su linea de log no debe fingir que si.
        auto sinFichero = retenidoDePrueba("algo_retenido", "un motivo");
        sinFichero.fichero = juce::File();

        const auto texto = core::quarantine::descripcionDeRetenido(sinFichero);

        CHECK_FALSE(texto.contains("quarantined"));
        CHECK(texto.contains("algo_retenido"));
    }

    SECTION("El prefijo es el que se puede buscar, y esta fijado")
    {
        // El pin del literal, y tapa el mismo fallo que tapa el pin del enum del
        // esquema: nadie reescribe un prefijo por maldad, pero un dia alguien
        // lo "normaliza" al del registro porque era lo que habia antes, y
        // entonces un `grep "[CUARENTENA]"` -- que es JUSTO el comando con el
        // que se va a buscar por que falta un Aparato dentro de tres semanas--
        // deja de encontrar nada.
        //
        // Y no se cae solo. Sin este CHECK el resto del caso seguiria en verde
        // mientras el prefijo deja de existir, porque todo lo demas se compara
        // contra el propio prefijo y no contra un texto fijo: un guard que solo
        // sabe decir "lo mismo que antes" acaba aceptando cualquier cosa.
        CHECK(juce::String(core::quarantine::prefijoLog) == "[CUARENTENA]");

        // Con corchetes, que es lo que hace que se distinga de una etiqueta
        // mas dentro de un log lleno de etiquetas.
        CHECK(juce::String(core::quarantine::prefijoLog).startsWith("["));
        CHECK(juce::String(core::quarantine::prefijoLog).endsWith("]"));

        // Y la palabra en mayusculas dentro, que es la que se busca a ojo en
        // un log de arranque cuando no se sabe que se esta buscando.
        CHECK(juce::String(core::quarantine::prefijoLog).contains("CUARENTENA"));

        // Y no puede llevar el nombre de nadie. El del registro es el que
        // tienen los fallos de verdad, y si coincidieran se volveria al
        // problema original: dos clases de cosa con la misma etiqueta.
        CHECK_FALSE(juce::String(core::quarantine::prefijoLog)
                     .contains("HardwareContractRegistry"));
    }
    SECTION("El prefijo no aparece en el texto del panel")
    {
        // El prefijo es cosa del log. En pantalla, "[CUARENTENA] decision
        // editorial..." es ruido de terminal en mitad de una frase.
        gui::StartupWarningsPanel panel;
        panel.addQuarantineNotice(retenido);

        CHECK_FALSE(panel.getNoticeText(0).contains(core::quarantine::prefijoLog));
    }
}