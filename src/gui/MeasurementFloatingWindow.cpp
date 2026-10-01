/**
 * @file MeasurementFloatingWindow.cpp
 * @brief Implementation of the measurement floating window.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementFloatingWindow.h"
#include "SoundIdTheme.h"
#include "gui/measurement/MeasurementViewerPanel.h"
#include "gui/measurement/MeasurementComparisonPanel.h"

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

void MeasurementFloatingWindow::updateTheme()
{
    setBackgroundColour (AppTheme::BackgroundApp);

    if (auto* viewer = dynamic_cast<measurement::MeasurementViewerPanel*> (getContentComponent()))
        viewer->updateTheme();
    else if (auto* comp = dynamic_cast<measurement::MeasurementComparisonPanel*> (getContentComponent()))
        comp->updateTheme();

    repaint();
}

void MeasurementFloatingWindow::closeButtonPressed()
{
    setVisible (false);
}

} // namespace abdaudiolab::gui