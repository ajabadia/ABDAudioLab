/**
 * @file MeasurementComparisonPanel.h
 * @brief Master UI panel composing container list, multi-curve comparison, state equivalence card, and verified audio player.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "MeasurementComparisonSession.h"
#include "MeasurementContainerListPanel.h"
#include "MeasurementDynamicsComparisonComponent.h"
#include "MeasurementStateEquivalenceCard.h"
#include "MeasurementAudioPlayerComponent.h"
#include "../../measurement/MeasurementComparisonReportGenerator.h"

namespace abdaudiolab::gui::measurement
{

class MeasurementComparisonPanel : public juce::Component,
                                   public MeasurementComparisonSession::Listener
{
public:
    MeasurementComparisonPanel();
    ~MeasurementComparisonPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // MeasurementComparisonSession::Listener interface
    void containerStateChanged(int containerId, ContainerLoadState newState) override;
    void containerListChanged() override;
    void activeAudioSourceChanged(int activeContainerId) override;
    void domainFilterChanged() override;

    [[nodiscard]] MeasurementComparisonSession& getSession() noexcept { return session_; }

private:
    void exportComparisonReport();
    void updateActiveAudioPlayer(int activeContainerId);

    MeasurementComparisonSession session_;

    // Sub-components
    MeasurementContainerListPanel listPanel_;
    MeasurementDynamicsComparisonComponent comparisonCurveComponent_;
    MeasurementStateEquivalenceCard equivalenceCard_;
    MeasurementAudioPlayerComponent audioPlayerComponent_;

    // Master toolbar
    juce::Label lblHeader_ { {}, "ABDAudioLab — Comparador Multivariante de Mediciones FAIR / LNL" };
    juce::ComboBox cmbMetricMode_;
    juce::TextButton btnExportReport_ { "Exportar Informe HTML..." };
    juce::Label lblProvenance_ { {}, "Fuente de Audio: Ninguna" };

    std::unique_ptr<juce::FileChooser> fileChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeasurementComparisonPanel)
};

} // namespace abdaudiolab::gui::measurement
