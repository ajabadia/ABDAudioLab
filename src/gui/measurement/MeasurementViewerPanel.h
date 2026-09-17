/**
 * @file MeasurementViewerPanel.h
 * @brief Main UI panel composing temporal curve, ADSR metric cards, audio player and FAIR integrity actions.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "MeasurementViewModel.h"
#include "MeasurementTemporalCurveComponent.h"
#include "MeasurementFrequencyCurveComponent.h"
#include "MeasurementAudioPlayerComponent.h"

namespace abdaudiolab::gui::measurement
{

class MeasurementViewerPanel : public juce::Component
{
public:
    MeasurementViewerPanel();
    ~MeasurementViewerPanel() override = default;

    /**
     * @brief Loads a measurement container asynchronously or synchronously.
     */
    bool loadContainer(const juce::File& containerDir, juce::String& outError);

    /**
     * @brief Directly sets an existing view model.
     */
    void setViewModel(const MeasurementViewModel& model);

    void paint(juce::Graphics& g) override;
    void resized() override;

    [[nodiscard]] const MeasurementViewModel& getViewModel() const noexcept { return model_; }

    /**
     * @brief Selects which audio track to feed into the audio player (0=Captured, 1=Stimulus, 2=IR).
     */
    void selectAudioTrack(int trackIndex);

private:
    void promptLoadContainer();
    void triggerBackgroundManifestVerification();
    void handleVerificationCompleted(bool ok, const juce::String& diagnostic);
    void openHtmlReportInBrowser();
    void updateIntegrityUi();
    void updateHeaderAndBadges();
    void layoutMetricCards();

    MeasurementViewModel model_;
    bool isVerifyingBackground_ { false };
    int selectedAudioTrack_ { 0 };

    // Header labels & badges
    juce::Label lblTitle_;
    juce::Label lblSubtitle_;
    juce::Label lblDomainBadge_;
    juce::Label lblStatusBadge_;
    juce::Label lblIntegrityBadge_;
    juce::Label lblDiagnostic_;

    // Metric Cards
    struct MetricCard
    {
        juce::Label lblName;
        juce::Label lblValue;
        juce::Label lblStatus;
    };
    std::vector<std::unique_ptr<MetricCard>> metricCardViews_;

    // Curve Visualizers (temporal for envelope, frequency for filter)
    MeasurementTemporalCurveComponent curveComponent_;
    MeasurementFrequencyCurveComponent freqCurveComponent_;

    // Audio Track Selector (for filter measurements)
    juce::TextButton btnTrackOutput_ { "Salida (Captura)" };
    juce::TextButton btnTrackInput_ { "Entrada (Barrido)" };
    juce::TextButton btnTrackIr_ { "Respuesta Impulsional (IR)" };

    // Audio Player
    MeasurementAudioPlayerComponent audioPlayerComponent_;

    // Action buttons
    juce::TextButton btnLoadContainer_ { "Cargar Contenedor..." };
    juce::TextButton btnOpenReport_ { "Abrir Informe HTML" };
    juce::TextButton btnVerifyManifest_ { "Verificar Manifiesto" };

    std::unique_ptr<juce::FileChooser> fileChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeasurementViewerPanel)
};

} // namespace abdaudiolab::gui::measurement
