/**
 * @file MeasurementFloatingWindow.cpp
 * @brief Implementation of the measurement floating window.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementFloatingWindow.h"
#include "SoundIdTheme.h"

namespace abdaudiolab::gui
{

MeasurementFloatingWindow::MeasurementFloatingWindow (const juce::String& title,
                                                     juce::Component* contentComponent,
                                                     int defaultWidth,
                                                     int defaultHeight,
                                                     int minWidth,
                                                     int minHeight)
    : DocumentWindow (title, AppTheme::BackgroundApp, DocumentWindow::allButtons)
{
    setUsingNativeTitleBar (true);
    setResizable (true, true);
    setResizeLimits (minWidth, minHeight, 2560, 1440);
    setContentOwned (contentComponent, true);
    centreWithSize (defaultWidth, defaultHeight);
}

MeasurementFloatingWindow::MeasurementFloatingWindow (const juce::String& title,
                                                     measurement::MeasurementThemedPanel& contentPanel,
                                                     int defaultWidth,
                                                     int defaultHeight,
                                                     int minWidth,
                                                     int minHeight)
    : MeasurementFloatingWindow (title, &contentPanel, defaultWidth, defaultHeight, minWidth, minHeight)
{
    themedContent = &contentPanel;
}

void MeasurementFloatingWindow::updateTheme()
{
    setBackgroundColour (AppTheme::BackgroundApp);

    // Sin `dynamic_cast` y sin preguntar de que tipo es el contenido: el puntero
    // lo deja el constructor que elige el llamante, y esa es toda la informacion
    // que hace falta.
    if (themedContent != nullptr)
        themedContent->updateTheme();

    repaint();
}

void MeasurementFloatingWindow::closeButtonPressed()
{
    setVisible (false);
}

} // namespace abdaudiolab::gui