#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

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
 * `updateTheme()` conoce los dos paneles concretos que admite. Es una lista
 * cerrada a proposito: esta ventana existe para ellos, y cuando aparezca un
 * tercero se decide aqui si entra o si necesita su propia ventana, en vez de
 * repartirse el `dynamic_cast` por todas las clases que abren ventanas.
 */
class MeasurementFloatingWindow : public juce::DocumentWindow
{
public:
    MeasurementFloatingWindow (const juce::String& title,
                               juce::Component* contentComponent,
                               int defaultWidth,
                               int defaultHeight,
                               int minWidth,
                               int minHeight);

    /** @brief Reaplica el tema al marco y al panel que contenga. */
    void updateTheme();

    /** @brief Cierra en vez de destruir. */
    void closeButtonPressed() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeasurementFloatingWindow)
};

} // namespace abdaudiolab::gui