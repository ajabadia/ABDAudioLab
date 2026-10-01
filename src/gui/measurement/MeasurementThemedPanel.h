/**
 * @file MeasurementThemedPanel.h
 * @brief The minimal contract of a measurement panel that knows how to re-theme itself.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace abdaudiolab::gui::measurement
{

/**
 * @brief Everything MeasurementFloatingWindow needs from what it contains.
 *
 * The floating window used to discover its content with two `dynamic_cast`s:
 * one for the viewer panel, one for the comparison panel. That was a closed list
 * written as a runtime question, and a runtime question about types is a question
 * that has to be re-asked and re-checked every time somebody types a new panel,
 * opens it from a new place, or wraps it in one more component. It is also, by
 * construction, the kind of thing that survives a refactor: rename a class, or
 * wrap a panel in a scrollable container, and the cast quietly stops matching
 * while the window keeps compiling and keeps opening. Nothing fails. The panel
 * just stops being re-themed.
 *
 * The list is still closed, and now the compiler is the one enforcing it. A panel
 * is re-themed because it says it is, in its own declaration, not because
 * somebody remembered to add its type to a chain of casts at the other end of the
 * codebase. There is no arrangement of panels that can be half-registered.
 *
 * It is a Component subclass rather than a side interface because the window
 * already owned the content through `setContentOwned`, and a second base class
 * would have meant the panel could be a Component and not be one at the same
 * time. Here the type answers both questions at once: I am a component, and here
 * is how I re-theme.
 */
class MeasurementThemedPanel : public juce::Component
{
public:
    // Explicito, y no heredado. `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR`
    // borra el constructor de copia, y MSVC deja de considerar que exista uno por
    // defecto: sin esta linea, `MeasurementViewerPanel::MeasurementViewerPanel()`
    // falla con un C2512 que no dice nada util, porque el unico defecto esta aqui
    // arriba y a 40 lineas de distancia.
    MeasurementThemedPanel() = default;
    ~MeasurementThemedPanel() override = default;

    /** @brief Reapplies the current theme to every piece of this panel. */
    virtual void updateTheme() = 0;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeasurementThemedPanel)
};

} // namespace abdaudiolab::gui::measurement