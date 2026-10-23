/**
 * @file MeasurementComparisonPanel.cpp
 * @brief Implementation of MeasurementComparisonPanel.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementComparisonPanel.h"
#include "../AppTheme.h"
#include "../../core/LabDataDirectories.h"

namespace abdaudiolab::gui::measurement
{

MeasurementComparisonPanel::MeasurementComparisonPanel()
    : listPanel_(session_),
      comparisonCurveComponent_(session_),
      equivalenceCard_(session_)
{
    session_.addListener(this);

    addAndMakeVisible(lblHeader_);
    lblHeader_.setFont(juce::FontOptions(16.0f));


    addAndMakeVisible(btnExportReport_);
    btnExportReport_.onClick = [this]() { exportComparisonReport(); };

    addAndMakeVisible(lblProvenance_);
    lblProvenance_.setFont(juce::FontOptions(11.0f));

    addAndMakeVisible(listPanel_);
    addAndMakeVisible(comparisonCurveComponent_);
    addAndMakeVisible(equivalenceCard_);
    addAndMakeVisible(audioPlayerComponent_);

    updateTheme();
}

void MeasurementComparisonPanel::updateTheme()
{
    lblHeader_.setColour(juce::Label::textColourId, gui::AppTheme::TextPrimary);
    lblProvenance_.setColour(juce::Label::textColourId, gui::AppTheme::TextSecondary);


    btnExportReport_.setColour(juce::TextButton::buttonColourId, gui::AppTheme::SurfaceCard);
    btnExportReport_.setColour(juce::TextButton::textColourOffId, gui::AppTheme::TextPrimary);

    listPanel_.updateTheme();
    comparisonCurveComponent_.updateTheme();
    equivalenceCard_.updateTheme();
    audioPlayerComponent_.updateTheme();

    repaint();
}

MeasurementComparisonPanel::~MeasurementComparisonPanel()
{
    session_.removeListener(this);
}

void MeasurementComparisonPanel::containerStateChanged(int containerId, ContainerLoadState)
{
    if (containerId == session_.getActiveAudioContainerId())
        updateActiveAudioPlayer(containerId);
}

void MeasurementComparisonPanel::containerListChanged()
{
}

void MeasurementComparisonPanel::domainFilterChanged()
{
}

void MeasurementComparisonPanel::activeAudioSourceChanged(int activeContainerId)
{
    updateActiveAudioPlayer(activeContainerId);
}

void MeasurementComparisonPanel::updateActiveAudioPlayer(int activeContainerId)
{
    audioPlayerComponent_.clear();

    if (activeContainerId == -1)
    {
        lblProvenance_.setText("Audio Source: None", juce::dontSendNotification);
        return;
    }

    auto optEntry = session_.getContainerById(activeContainerId);
    if (!optEntry.has_value() || !optEntry->isPlayable())
    {
        lblProvenance_.setText("Audio Source: Blocked (Unverified or corrupt container)", juce::dontSendNotification);
        return;
    }

    const auto& vm = *optEntry->viewModel;
    audioPlayerComponent_.setAudioFile(vm.audioFile, vm.expectedAudioSha256, true);

    juce::String name = vm.dutName.isNotEmpty() ? vm.dutName : optEntry->containerDir.getFileName();
    lblProvenance_.setText("Verified Audio: " + name + " | SHA: " + vm.expectedAudioSha256.substring(0, 12) + "...", juce::dontSendNotification);
}

void MeasurementComparisonPanel::exportComparisonReport()
{
    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Save Comparative HTML Report...",
        core::resolveLabDataDirectories().reports.getChildFile("Measurement_Comparison_Report.html"),
        "*.html",
        true);

    fileChooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this](const juce::FileChooser& chooser)
    {
        auto result = chooser.getResult();
        if (result != juce::File())
        {
            juce::String err;
            abdaudiolab::measurement::MeasurementComparisonReportGenerator::generateReport(session_, result, err);
        }
    });
}

void MeasurementComparisonPanel::paint(juce::Graphics& g)
{
    g.fillAll(gui::AppTheme::BackgroundApp);
    g.setColour(gui::AppTheme::BorderSubtle);
    g.drawRect(getLocalBounds(), 1);
}

void MeasurementComparisonPanel::resized()
{
    auto area = getLocalBounds().reduced(12);

    // 1. Top Header: Title across top bar
    auto topBar = area.removeFromTop(32);
    lblHeader_.setBounds(topBar);

    area.removeFromTop(8);

    // 2. Bottom Action Bar: Export HTML Report button placed at the bottom (coherent with Viewer panel!)
    auto actionBar = area.removeFromBottom(34);
    btnExportReport_.setBounds(actionBar.removeFromLeft(200));

    area.removeFromBottom(8);

    // 3. Audio Player Row: Directly above action bar, matching MeasurementViewerPanel layout
    auto audioArea = area.removeFromBottom(105);
    lblProvenance_.setBounds(audioArea.removeFromTop(18));
    audioPlayerComponent_.setBounds(audioArea);

    area.removeFromBottom(8);

    // 4. Central Work Area
    // Right sidebar: Pairwise State Equivalence Card
    const int rightSidebarW = 300;
    auto rightArea = area.removeFromRight(rightSidebarW);
    equivalenceCard_.setBounds(rightArea);

    area.removeFromRight(8);

    // Left column: Container list (width 260px, fits all pills without truncation)
    const int leftListW = 260;
    auto leftArea = area.removeFromLeft(leftListW);
    listPanel_.setBounds(leftArea);

    area.removeFromLeft(8);

    // Center area: Multi-curve dynamics/timbre comparison
    comparisonCurveComponent_.setBounds(area);
}

} // namespace abdaudiolab::gui::measurement
