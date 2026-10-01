// ==============================================================================
// ABDAudioLab - La seccion de cuarentena del cajon de hardware
// ==============================================================================

#include <catch2/catch_test_macros.hpp>

#include "gui/drawers/DrawerHardwareTab.h"

using namespace abdaudiolab;

// Que se pincha aqui, y no lo pincha el otro test de cuarentena, es el ALTO de la
// seccion. El otro (`test_HardwareContractQuarantine.cpp`) responde a "el
// registro retiene lo que dice el contrato". Este responde a "si no hay nada
// retenido, el cajon no se queda un hueco reservado".
//
// Y ese es el fallo que costaria caro: uno esperaria que una seccion vacia
// midiese cero y no ocupase nada. Pero un alto minimo aunque no haya nada se ve
// como un rectangulo vacio al final del cajon en todas las instalaciones
// limpias, que son las que mas se miran, y alguien acabaria creyendo que hay
// algo roto debajo del ultimo control.
TEST_CASE("Seccion de cuarentena - alto cero si no hay retenidos", "[gui][drawer][quarantine]")
{
    SECTION("Sin retenidos no hay seccion")
    {
        CHECK(gui::quarantineSectionHeight(0) == 0);
    }

    SECTION("Con un retenido la seccion ya ocupa alto")
    {
        CHECK(gui::quarantineSectionHeight(1) > 0);
    }

    SECTION("Cada retenido anade exactamente su fila y nada mas")
    {
        // 16 px es la altura de una fila. Si un dia cambia, este CHECK avisa de
        // que hay que revisar tambien el gasto de filas de `paint`, que esta
        // escrito a mano al lado.
        const int uno = gui::quarantineSectionHeight(1);
        const int dos = gui::quarantineSectionHeight(2);
        const int cinco = gui::quarantineSectionHeight(5);

        CHECK(dos - uno == 16);
        CHECK(cinco - dos == 48);
    }

    SECTION("El alto de un retenido cubre titulo, fila y la nota al pie")
    {
        // 10 + 18 + 2 + 16 + 18 + 10. Es una cuenta a proposito y por eso esta
        // escrita a mano tambien en `QuarantineListCard::paint`, que gasta los
        // pixeles uno a uno. Si el CHECK falla, han cambiado las dos o ninguna.
        CHECK(gui::quarantineSectionHeight(1) == 74);
    }

    SECTION("Un numero disparatado de retenidos sigue dando un alto positivo")
    {
        // No es un caso real, es un suelo. Un alto negativo aqui no se ve:
        // `setBounds` lo acepta y lo que se descuadra es la seccion de al lado.
        CHECK(gui::quarantineSectionHeight(100000u) > 0);
    }
}

// ----------------------------------------------------------------------------
// LA FICHA DEL RETENIDO.
//
// Lo que se mide aqui es el ALTO de la ficha, y no sus pixeles. Pinta dos
// botones y un texto, y probarlo necesitaria un message loop, un pixel exacto
// y una fuente concreta: tres cosas que cambian con cada retoque del tema y
// hacen que el test solo dixera que el tema ha cambiado.
//
// El alto si es una decision: es lo que impide que el cajon se descuadre, y su
// regla es "no hay ficha si no hay nada abierto". El fallo caro es el de la
// seccion, repetido aqui: un hueco reservado abajo del todo que parece un
// rectangulo vacio y que alguien acaba leyendo como "hay algo roto aqui".
// ----------------------------------------------------------------------------
TEST_CASE("Ficha del retenido - sin fila abierta no hay ficha",
          "[gui][drawer][quarantine]")
{
    SECTION("Sin retenido abierto la ficha mide cero")
    {
        CHECK(gui::quarantineDetailHeight(false) == 0);
    }

    SECTION("Con un retenido abierto la ficha ya tiene sitio")
    {
        // Y aqui no hay cuenta a mano a proposito: el alto de la ficha tiene dos
        // botones, un nombre, un motivo y una ruta, y escribir el numero aqui
        // seria un segundo sitio que ajustar el dia que cambie uno de ellos. Lo
        // que importa es que seafijo, no cuanto sea: un alto que dependiera de
        // la longitud del motivo haria que el cajon creciera con cada correccion
        // de una palabra en un JSON de otro repositorio.
        CHECK(gui::quarantineDetailHeight(true) > 0);
    }

    SECTION("La ficha cabe sin comerse el cajon, y sin quedarse corta")
    {
        // Las dos cotas, y no el numero exacto. La de arriba evita que un alto
        // holgado empuje los controles de debajo fuera de la pantalla; la de
        // abajo evita que un alto recortado se coma el motivo, que es lo unico
        // que ha justificado abrir la ficha.
        CHECK(gui::quarantineDetailHeight(true) >= 96);
        CHECK(gui::quarantineDetailHeight(true) <= 240);

        // Y abrir la ficha solo puede hacer el cajon mas grande. Al reves
        // significa que el alto se esta restando de otro sitio.
        CHECK(gui::quarantineDetailHeight(true) > gui::quarantineDetailHeight(false));
    }
}
