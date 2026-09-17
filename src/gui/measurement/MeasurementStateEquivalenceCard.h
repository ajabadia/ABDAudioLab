/**
 * @file MeasurementStateEquivalenceCard.h
 * @brief UI card presenting pairwise state equivalence and cryptographic fixity evidence.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "MeasurementComparisonSession.h"

namespace abdaudiolab::gui::measurement
{

class MeasurementStateEquivalenceCard : public juce::Component,
                                       public MeasurementComparisonSession::Listener
{
public:
    explicit MeasurementStateEquivalenceCard(MeasurementComparisonSession& session);
    ~MeasurementStateEquivalenceCard() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // MeasurementComparisonSession::Listener interface
    void containerStateChanged(int containerId, ContainerLoadState newState) override;
    void containerListChanged() override;
    void domainFilterChanged() override;

private:
    void rebuildPairSelector();
    void updateSelectedPair();

    MeasurementComparisonSession& session_;
    juce::ComboBox cmbPairs_;
    std::vector<std::pair<int, int>> availablePairs_;
    std::optional<PairwiseComparisonResult> currentResult_;

    juce::Label lblTitle_ { {}, "Equivalencia de Estado Metrológica (Por Pares)" };
    juce::Label lblStatusBadge_;
    juce::Label lblReason_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeasurementStateEquivalenceCard)
};

} // namespace abdaudiolab::gui::measurement
