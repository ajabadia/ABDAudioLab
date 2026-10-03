#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "gui/measurement/MeasurementThemedPanel.h"

namespace abdaudiolab::gui
{

/**
 * @brief Ventana flotante que envuelve los paneles de medicion.
 *
 * Vivia dentro de `MainContentComponent.cpp`, en un `namespace` anonimo, bajo
 * un banner que hablaba del motor de audio y del puente FFT. No pertenece a
 * ninguno de los dos: es una ventana, y lo unico que la hacia parecer parte de
 * esa historia era que se abria desde ahi.
 *
 * Se extrae porque en el `.cpp` era imposible encontrarla. Un fichero de casi
 * 4000 lineas con 80 metodos y 15 secciones cuyos numeros de banner ya no
 * siguen un orden tiene una consecuencia muy concreta: el proximo cambio que
 * toque esta ventana va a acabar al lado del puente FFT, y va a growar la
 * zona que ya no se entiende.
 *
 * El contenido es `measurement::MeasurementThemedPanel`, y no una lista de
 * clases concretas: la lista cerrada sigue existiendo, pero la lleva el
 * compilador. Antes eran dos `dynamic_cast` al final de una cadena, y esa forma
 * de escribir una lista cerrada tiene un fallo que no se ve hasta que ocurre:
 * renombrar un panel, o envolverlo en un contenedor, y el cast deja de coincidir
 * en silencio. La ventana sigue compilando, sigue abriendose, y el panel ya no se
 * re-tematiza. Nadie se entera hasta que alguien cambia de tema oscuro en mitad
 * de una sesion y ve la mitad de la ventana del color anterior.
 *
 * Por eso hay DOS constructores y no uno con un puntero nulo opcional: el que
 * acepta un `MeasurementThemedPanel&` deja constancia en la llamada de que ese
 * contenido se re-tematiza, y el que acepta un `juce::Component*` deja constancia
 * de que no. Un constructor que acepta las dos cosas a la vez diria la mitad de
 * la verdad, y la otra mitad dependeria de que el puntero viniera bien.
 */
class MeasurementFloatingWindow : public juce::DocumentWindow
{
public:
    /** @brief Contenido que NO sabe re-tematizarse. El marco si. */
    MeasurementFloatingWindow (const juce::String& title,
                               juce::Component* contentComponent,
                               int defaultWidth,
                               int defaultHeight,
                               int minWidth,
                               int minHeight,
                               bool addToDesktop = true);

    /** @brief Contenido que SI sabe re-tematizarse. */
    MeasurementFloatingWindow (const juce::String& title,
                               measurement::MeasurementThemedPanel& contentPanel,
                               int defaultWidth,
                               int defaultHeight,
                               int minWidth,
                               int minHeight,
                               bool addToDesktop = true);

    /** @brief Reaplica el tema al marco y, si sabe, al panel que contenga. */
    void updateTheme();

    /** @brief Cierra en vez de destruir. */
    void closeButtonPressed() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeasurementFloatingWindow)

    /** El panel que se re-tematiza, o nullptr si el constructor recibio un
     *  `juce::Component` cualquiera. Nunca se deduce: se pasa, o no se pasa. */
    measurement::MeasurementThemedPanel* themedContent { nullptr };
};

} // namespace abdaudiolab::gui