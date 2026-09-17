/**
 * @file MeasurementComparisonPanel.cpp
 * @brief Implementation of MeasurementComparisonPanel.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementComparisonPanel.h"

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
    lblHeader_.setColour(juce::Label::textColourId, juce::Colour(0xff00d4ff));

    addAndMakeVisible(cmbMetricMode_);
    cmbMetricMode_.addItem("Nivel (dBFS)", 1);
    cmbMetricMode_.addItem("Timbre (Centroide Hz)", 2);
    cmbMetricMode_.addItem("Timbre (Rolloff Hz)", 3);
    cmbMetricMode_.setSelectedId(1, juce::dontSendNotification);
    cmbMetricMode_.onChange = [this]()
    {
        const int id = cmbMetricMode_.getSelectedId();
        if (id == 1)
            comparisonCurveComponent_.setComparisonMode(DynamicsComparisonMode::LevelDbfs);
        else if (id == 2)
            comparisonCurveComponent_.setComparisonMode(DynamicsComparisonMode::TimbreCentroidHz);
        else if (id == 3)
            comparisonCurveComponent_.setComparisonMode(DynamicsComparisonMode::TimbreRolloffHz);
    };

    addAndMakeVisible(btnExportReport_);
    btnExportReport_.onClick = [this]() { exportComparisonReport(); };

    addAndMakeVisible(lblProvenance_);
    lblProvenance_.setFont(juce::FontOptions(11.0f));
    lblProvenance_.setColour(juce::Label::textColourId, juce::Colour(0xff9e9eb0));

    addAndMakeVisible(listPanel_);
    addAndMakeVisible(comparisonCurveComponent_);
    addAndMakeVisible(equivalenceCard_);
    addAndMakeVisible(audioPlayerComponent_);
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
        lblProvenance_.setText("Fuente de Audio: Ninguna", juce::dontSendNotification);
        return;
    }

    auto optEntry = session_.getContainerById(activeContainerId);
    if (!optEntry.has_value() || !optEntry->isPlayable())
    {
        lblProvenance_.setText("Fuente de Audio: Bloqueada (Contenedor no verificado o corrupto)", juce::dontSendNotification);
        return;
    }

    const auto& vm = *optEntry->viewModel;
    audioPlayerComponent_.setAudioFile(vm.audioFile, vm.expectedAudioSha256, true);

    juce::String name = vm.dutName.isNotEmpty() ? vm.dutName : optEntry->containerDir.getFileName();
    lblProvenance_.setText("Audio Verificado: " + name + " | SHA: " + vm.expectedAudioSha256.substring(0, 12) + "...", juce::dontSendNotification);
}

void MeasurementComparisonPanel::exportComparisonReport()
{
    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Guardar Informe Comparativo HTML...",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Measurement_Comparison_Report.html"),
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
    g.fillAll(juce::Colour(0xff101015));
    g.setColour(juce::Colour(0xff22222d));
    g.drawRect(getLocalBounds(), 1);
}

void MeasurementComparisonPanel::resized()
{
    auto area = getLocalBounds().reduced(8);

    // Top master bar
    auto topBar = area.removeFromTop(32);
    btnExportReport_.setBounds(topBar.removeFromRight(170));
    topBar.removeFromRight(8);
    cmbMetricMode_.setBounds(topBar.removeFromRight(190));
    topBar.removeFromRight(8);
    lblHeader_.setBounds(topBar);

    area.removeFromTop(6);

    // Bottom section: Audio player and provenance
    auto bottomArea = area.removeFromBottom(105);
    lblProvenance_.setBounds(bottomArea.removeFromTop(18));
    audioPlayerComponent_.setBounds(bottomArea);

    area.removeFromBottom(6);

    // Right sidebar: State Equivalence Card
    const int rightSidebarW = 320;
    auto rightArea = area.removeFromRight(rightSidebarW);
    equivalenceCard_.setBounds(rightArea);

    area.removeFromRight(6);

    // Split remaining area between container list (left) and dynamics comparison (center)
    const int leftListW = 320;
    auto leftArea = area.removeFromLeft(leftListW);
    listPanel_.setBounds(leftArea);

    area.removeFromLeft(6);
    comparisonCurveComponent_.setBounds(area);
}

} // namespace abdaudiolab::gui::measurement
