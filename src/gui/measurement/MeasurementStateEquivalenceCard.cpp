/**
 * @file MeasurementStateEquivalenceCard.cpp
 * @brief Implementation of MeasurementStateEquivalenceCard.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementStateEquivalenceCard.h"

namespace abdaudiolab::gui::measurement
{

MeasurementStateEquivalenceCard::MeasurementStateEquivalenceCard(MeasurementComparisonSession& session)
    : session_(session)
{
    session_.addListener(this);

    addAndMakeVisible(lblTitle_);
    lblTitle_.setFont(juce::FontOptions(14.0f));
    lblTitle_.setColour(juce::Label::textColourId, juce::Colour(0xff00d4ff));

    addAndMakeVisible(cmbPairs_);
    cmbPairs_.onChange = [this]() { updateSelectedPair(); };

    addAndMakeVisible(lblStatusBadge_);
    lblStatusBadge_.setFont(juce::FontOptions(11.0f));
    lblStatusBadge_.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(lblReason_);
    lblReason_.setFont(juce::FontOptions(11.0f));
    lblReason_.setColour(juce::Label::textColourId, juce::Colour(0xffc0c0d0));

    rebuildPairSelector();
}

MeasurementStateEquivalenceCard::~MeasurementStateEquivalenceCard()
{
    session_.removeListener(this);
}

void MeasurementStateEquivalenceCard::containerStateChanged(int, ContainerLoadState)
{
    rebuildPairSelector();
}

void MeasurementStateEquivalenceCard::containerListChanged()
{
    rebuildPairSelector();
}

void MeasurementStateEquivalenceCard::domainFilterChanged()
{
    rebuildPairSelector();
}

void MeasurementStateEquivalenceCard::rebuildPairSelector()
{
    const int previousSelectedId = cmbPairs_.getSelectedId();
    cmbPairs_.clear(juce::dontSendNotification);
    availablePairs_.clear();

    const auto eligible = session_.getEligibleComparisonContainers();
    int pairIndex = 1;

    for (size_t i = 0; i < eligible.size(); ++i)
    {
        for (size_t j = i + 1; j < eligible.size(); ++j)
        {
            const auto& a = eligible[i];
            const auto& b = eligible[j];

            juce::String nameA = (a.viewModel != nullptr && a.viewModel->dutName.isNotEmpty()) ? a.viewModel->dutName : a.containerDir.getFileName();
            juce::String nameB = (b.viewModel != nullptr && b.viewModel->dutName.isNotEmpty()) ? b.viewModel->dutName : b.containerDir.getFileName();

            cmbPairs_.addItem(nameA + " <-> " + nameB, pairIndex);
            availablePairs_.push_back({ a.id, b.id });
            pairIndex++;
        }
    }

    if (cmbPairs_.getNumItems() > 0)
    {
        if (previousSelectedId > 0 && previousSelectedId <= cmbPairs_.getNumItems())
            cmbPairs_.setSelectedId(previousSelectedId, juce::sendNotification);
        else
            cmbPairs_.setSelectedId(1, juce::sendNotification);
    }
    else
    {
        currentResult_.reset();
        lblStatusBadge_.setText("SIN PARES SUFICIENTES", juce::dontSendNotification);
        lblStatusBadge_.setColour(juce::Label::backgroundColourId, juce::Colour(0xff3a3a48));
        lblReason_.setText("Carga al menos 2 contenedores verificados para comparar equivalencia de estado.", juce::dontSendNotification);
        repaint();
    }
}

void MeasurementStateEquivalenceCard::updateSelectedPair()
{
    const int selIdx = cmbPairs_.getSelectedItemIndex();
    if (selIdx >= 0 && selIdx < static_cast<int>(availablePairs_.size()))
    {
        const auto pair = availablePairs_[static_cast<size_t>(selIdx)];
        currentResult_ = session_.compareContainers(pair.first, pair.second);

        juce::Colour badgeCol = juce::Colour(0xff607d8b);
        if (currentResult_->equivalence == PairwiseStateEquivalence::BitExact)
            badgeCol = juce::Colour(0xff00c853); // Emerald Green
        else if (currentResult_->equivalence == PairwiseStateEquivalence::SemanticallyEquivalent)
            badgeCol = juce::Colour(0xffffa726); // Amber
        else if (currentResult_->equivalence == PairwiseStateEquivalence::NotEquivalent)
            badgeCol = juce::Colour(0xffd50000); // Red

        lblStatusBadge_.setText(pairwiseEquivalenceToString(currentResult_->equivalence).toUpperCase(), juce::dontSendNotification);
        lblStatusBadge_.setColour(juce::Label::backgroundColourId, badgeCol);
        lblStatusBadge_.setColour(juce::Label::textColourId, juce::Colours::white);

        lblReason_.setText(currentResult_->reason, juce::dontSendNotification);
    }
    repaint();
}

void MeasurementStateEquivalenceCard::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff16161c));
    g.setColour(juce::Colour(0xff2d2d3d));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 4.0f, 1.0f);

    if (!currentResult_.has_value())
        return;

    auto area = getLocalBounds().reduced(12);
    area.removeFromTop(60); // Header, dropdown and badge

    g.setColour(juce::Colour(0xff252535));
    g.drawHorizontalLine(area.getY() - 6, 12.0f, static_cast<float>(getWidth() - 12));

    g.setFont(juce::FontOptions(11.0f));
    g.setColour(juce::Colour(0xff8e8ea0));

    const int rowH = 18;
    auto row1 = area.removeFromTop(rowH);
    juce::String dimText = "Nivel: " + pairwiseEquivalenceToString(currentResult_->levelEquivalence) +
                           " (Delta: " + juce::String(currentResult_->maxAudioDelta, 6) + ")";
    if (currentResult_->timbreEquivalence != PairwiseStateEquivalence::NotComparable)
    {
        dimText += " | Timbre: " + pairwiseEquivalenceToString(currentResult_->timbreEquivalence) +
                   " (Delta: " + juce::String(currentResult_->maxTimbreDeltaHz, 1) + " Hz)";
    }
    g.drawText(dimText, row1, juce::Justification::centredLeft);

    auto row2 = area.removeFromTop(rowH);
    juce::String binA = currentResult_->pluginBinarySha256A.substring(0, 16);
    juce::String binB = currentResult_->pluginBinarySha256B.substring(0, 16);
    g.drawText("Plugin Binary SHA: " + (binA.isNotEmpty() ? binA : "n/a") + " vs " + (binB.isNotEmpty() ? binB : "n/a"), row2, juce::Justification::centredLeft);

    auto row3 = area.removeFromTop(rowH);
    juce::String stA = currentResult_->stateSha256A.substring(0, 16);
    juce::String stB = currentResult_->stateSha256B.substring(0, 16);
    g.drawText("State Audio SHA: " + (stA.isNotEmpty() ? stA : "n/a") + " vs " + (stB.isNotEmpty() ? stB : "n/a"), row3, juce::Justification::centredLeft);
}

void MeasurementStateEquivalenceCard::resized()
{
    auto area = getLocalBounds().reduced(8);
    auto topArea = area.removeFromTop(24);

    lblTitle_.setBounds(topArea.removeFromLeft(getWidth() - 190));
    lblStatusBadge_.setBounds(topArea.removeFromRight(180));

    area.removeFromTop(4);
    auto dropArea = area.removeFromTop(24);
    cmbPairs_.setBounds(dropArea.removeFromLeft(getWidth() - 16));

    area.removeFromTop(4);
    lblReason_.setBounds(area.removeFromTop(18));
}

} // namespace abdaudiolab::gui::measurement
