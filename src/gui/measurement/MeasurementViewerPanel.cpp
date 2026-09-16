/**
 * @file MeasurementViewerPanel.cpp
 * @brief Implementation of MeasurementViewerPanel.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementViewerPanel.h"
#include "MeasurementViewModelLoader.h"
#include <thread>
#include <iomanip>
#include <sstream>

namespace abdaudiolab::gui::measurement
{

MeasurementViewerPanel::MeasurementViewerPanel()
{
    // Title & Subtitle
    lblTitle_.setFont(juce::Font(18.0f, juce::Font::bold));
    lblTitle_.setColour(juce::Label::textColourId, juce::Colour(0xfff8fafc));
    lblTitle_.setText("Envelope Measurement Viewer", juce::dontSendNotification);
    addAndMakeVisible(lblTitle_);

    lblSubtitle_.setFont(juce::Font(12.0f, juce::Font::plain));
    lblSubtitle_.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    lblSubtitle_.setText("Target: None | No measurement container loaded", juce::dontSendNotification);
    addAndMakeVisible(lblSubtitle_);

    // Status Badge
    lblStatusBadge_.setFont(juce::Font(11.0f, juce::Font::bold));
    lblStatusBadge_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(lblStatusBadge_);

    // Integrity Badge
    lblIntegrityBadge_.setFont(juce::Font(11.0f, juce::Font::bold));
    lblIntegrityBadge_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(lblIntegrityBadge_);

    // Diagnostic label
    lblDiagnostic_.setFont(juce::Font(11.5f, juce::Font::plain));
    lblDiagnostic_.setColour(juce::Label::textColourId, juce::Colour(0xfffbbf24));
    addAndMakeVisible(lblDiagnostic_);

    // Subcomponents
    addAndMakeVisible(curveComponent_);
    addAndMakeVisible(audioPlayerComponent_);

    // Wire on-demand corruption callback from player
    audioPlayerComponent_.onPlaybackBlockedByCorruption = [this](const juce::String& diag) {
        model_.integrityStatus = UiIntegrityStatus::Corrupt;
        model_.statusText = "CORRUPT";
        model_.statusIcon = "[X]";
        model_.integrityDiagnostic = diag;
        updateIntegrityUi();
        updateHeaderAndBadges();
        curveComponent_.setCurve(model_.curve, false);
    };

    // Action Buttons
    btnOpenReport_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0284c7));
    btnOpenReport_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff8fafc));
    btnOpenReport_.onClick = [this] { openHtmlReportInBrowser(); };
    addAndMakeVisible(btnOpenReport_);

    btnVerifyManifest_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    btnVerifyManifest_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffcbd5e1));
    btnVerifyManifest_.onClick = [this] { triggerBackgroundManifestVerification(); };
    addAndMakeVisible(btnVerifyManifest_);

    updateIntegrityUi();
    updateHeaderAndBadges();
}

bool MeasurementViewerPanel::loadContainer(const juce::File& containerDir, juce::String& outError)
{
    MeasurementViewModel model;
    if (!MeasurementViewModelLoader::loadFromContainer(containerDir, model, outError))
        return false;

    setViewModel(model);
    return true;
}

void MeasurementViewerPanel::setViewModel(const MeasurementViewModel& model)
{
    model_ = model;

    // Subtitle
    juce::String sub = "Target: " + (model_.dutName.isNotEmpty() ? model_.dutName : "Unknown")
                     + " (" + (model_.dutFormat.isNotEmpty() ? model_.dutFormat : "VST3") + ")"
                     + " | ID: " + model_.measurementId
                     + " | " + juce::String(static_cast<int>(model_.sampleRateHz)) + " Hz";
    lblSubtitle_.setText(sub, juce::dontSendNotification);

    // Setup cards
    metricCardViews_.clear();
    for (const auto& m : model_.metrics)
    {
        auto card = std::make_unique<MetricCard>();
        card->lblName.setFont(juce::Font(10.5f, juce::Font::bold));
        card->lblName.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
        card->lblName.setText(m.name.toUpperCase(), juce::dontSendNotification);
        addAndMakeVisible(card->lblName);

        card->lblValue.setFont(juce::Font("Consolas", 15.0f, juce::Font::bold));
        card->lblValue.setColour(juce::Label::textColourId, juce::Colour(0xfff8fafc));
        std::ostringstream valStream;
        valStream << std::fixed << std::setprecision(1) << m.value << " " << m.unit.toStdString();
        card->lblValue.setText(valStream.str(), juce::dontSendNotification);
        addAndMakeVisible(card->lblValue);

        card->lblStatus.setFont(juce::Font(10.0f, juce::Font::bold));
        bool isObs = (m.status == "observed");
        card->lblStatus.setColour(juce::Label::textColourId, isObs ? juce::Colour(0xff38bdf8) : juce::Colour(0xfffbbf24));
        card->lblStatus.setText(isObs ? "[OK] OBSERVED" : "[!] UNRELIABLE", juce::dontSendNotification);
        addAndMakeVisible(card->lblStatus);

        metricCardViews_.push_back(std::move(card));
    }

    // Diagnostic reason
    if (model_.diagnosticReason.isNotEmpty() && model_.diagnosticReason != "Envelope successfully observed")
    {
        lblDiagnostic_.setText("[!] Reason: " + model_.diagnosticReason, juce::dontSendNotification);
        lblDiagnostic_.setVisible(true);
    }
    else
    {
        lblDiagnostic_.setVisible(false);
    }

    // Update curve & audio subcomponents
    bool verified = (model_.integrityStatus != UiIntegrityStatus::Corrupt);
    curveComponent_.setCurve(model_.curve, verified);
    audioPlayerComponent_.setAudioFile(model_.audioFile, model_.expectedAudioSha256, verified);

    updateIntegrityUi();
    updateHeaderAndBadges();
    resized();
}

void MeasurementViewerPanel::updateHeaderAndBadges()
{
    // Status badge (Never PASS)
    juce::String fullStatus = model_.statusText + " " + model_.statusIcon;
    lblStatusBadge_.setText(fullStatus, juce::dontSendNotification);

    if (model_.statusText == "COMPLETED")
    {
        lblStatusBadge_.setColour(juce::Label::backgroundColourId, juce::Colour(0xff065f46));
        lblStatusBadge_.setColour(juce::Label::textColourId, juce::Colour(0xff34d399));
        lblStatusBadge_.setColour(juce::Label::outlineColourId, juce::Colour(0xff059669));
    }
    else if (model_.statusText == "UNRELIABLE")
    {
        lblStatusBadge_.setColour(juce::Label::backgroundColourId, juce::Colour(0xff78350f));
        lblStatusBadge_.setColour(juce::Label::textColourId, juce::Colour(0xfffbbf24));
        lblStatusBadge_.setColour(juce::Label::outlineColourId, juce::Colour(0xffd97706));
    }
    else if (model_.statusText == "CORRUPT" || model_.statusText == "INVALID")
    {
        lblStatusBadge_.setColour(juce::Label::backgroundColourId, juce::Colour(0xff7f1d1d));
        lblStatusBadge_.setColour(juce::Label::textColourId, juce::Colour(0xfff87171));
        lblStatusBadge_.setColour(juce::Label::outlineColourId, juce::Colour(0xffdc2626));
    }
    else
    {
        lblStatusBadge_.setColour(juce::Label::backgroundColourId, juce::Colour(0xff374151));
        lblStatusBadge_.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
        lblStatusBadge_.setColour(juce::Label::outlineColourId, juce::Colour(0xff4b5563));
    }
}

void MeasurementViewerPanel::updateIntegrityUi()
{
    switch (model_.integrityStatus)
    {
        case UiIntegrityStatus::Verified:
            lblIntegrityBadge_.setText("INTEGRITY [OK] VERIFIED", juce::dontSendNotification);
            lblIntegrityBadge_.setColour(juce::Label::backgroundColourId, juce::Colour(0xff0c4a6e));
            lblIntegrityBadge_.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
            lblIntegrityBadge_.setColour(juce::Label::outlineColourId, juce::Colour(0xff0284c7));
            break;

        case UiIntegrityStatus::Corrupt:
            lblIntegrityBadge_.setText("INTEGRITY [X] CORRUPT", juce::dontSendNotification);
            lblIntegrityBadge_.setColour(juce::Label::backgroundColourId, juce::Colour(0xff7f1d1d));
            lblIntegrityBadge_.setColour(juce::Label::textColourId, juce::Colour(0xfff87171));
            lblIntegrityBadge_.setColour(juce::Label::outlineColourId, juce::Colour(0xffdc2626));
            break;

        case UiIntegrityStatus::Verifying:
            lblIntegrityBadge_.setText("[*] VERIFYING HASHES...", juce::dontSendNotification);
            lblIntegrityBadge_.setColour(juce::Label::backgroundColourId, juce::Colour(0xff1e293b));
            lblIntegrityBadge_.setColour(juce::Label::textColourId, juce::Colour(0xfffbbf24));
            lblIntegrityBadge_.setColour(juce::Label::outlineColourId, juce::Colour(0xffd97706));
            break;

        case UiIntegrityStatus::Unchecked:
        default:
            lblIntegrityBadge_.setText("INTEGRITY [!] UNCHECKED", juce::dontSendNotification);
            lblIntegrityBadge_.setColour(juce::Label::backgroundColourId, juce::Colour(0xff374151));
            lblIntegrityBadge_.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
            lblIntegrityBadge_.setColour(juce::Label::outlineColourId, juce::Colour(0xff4b5563));
            break;
    }
}

void MeasurementViewerPanel::triggerBackgroundManifestVerification()
{
    if (isVerifyingBackground_ || !model_.containerDirectory.isDirectory())
        return;

    isVerifyingBackground_ = true;
    btnVerifyManifest_.setEnabled(false);
    btnVerifyManifest_.setButtonText("Verificando...");
    model_.integrityStatus = UiIntegrityStatus::Verifying;
    updateIntegrityUi();

    juce::File targetDir = model_.containerDirectory;
    juce::Component::SafePointer<MeasurementViewerPanel> safeThis(this);

    // Mandatory Adjustment 1: Run verification completely off audio/message thread in worker thread
    // SafePointer prevents accessing a destroyed component if panel is closed during verification
    std::thread([safeThis, targetDir]() {
        juce::String diagnostic;
        bool ok = MeasurementViewModelLoader::verifyContainerIntegrity(targetDir, diagnostic);

        // Safe async return to Message Thread
        juce::MessageManager::callAsync([safeThis, ok, diagnostic]() {
            if (safeThis == nullptr)
                return; // Component was destroyed while verifying in background

            safeThis->handleVerificationCompleted(ok, diagnostic);
        });
    }).detach();
}

void MeasurementViewerPanel::handleVerificationCompleted(bool ok, const juce::String& diagnostic)
{
    isVerifyingBackground_ = false;
    btnVerifyManifest_.setEnabled(true);
    btnVerifyManifest_.setButtonText("Verificar Manifiesto");

    if (ok)
    {
        model_.integrityStatus = UiIntegrityStatus::Verified;
        model_.integrityDiagnostic = diagnostic;
        if (model_.statusText == "CORRUPT")
        {
            model_.statusText = (model_.measurementStatus == abdaudiolab::measurement::MeasurementStatus::completed) ? "COMPLETED" : "UNRELIABLE";
            model_.statusIcon = (model_.statusText == "COMPLETED") ? "[OK]" : "[!]";
        }
    }
    else
    {
        model_.integrityStatus = UiIntegrityStatus::Corrupt;
        model_.statusText = "CORRUPT";
        model_.statusIcon = "[X]";
        model_.integrityDiagnostic = diagnostic;
    }

    updateIntegrityUi();
    updateHeaderAndBadges();
    curveComponent_.setCurve(model_.curve, model_.integrityStatus != UiIntegrityStatus::Corrupt);
    audioPlayerComponent_.setAudioFile(model_.audioFile, model_.expectedAudioSha256, model_.integrityStatus != UiIntegrityStatus::Corrupt);
}

void MeasurementViewerPanel::openHtmlReportInBrowser()
{
    if (model_.htmlReportFile.existsAsFile())
    {
        juce::URL(model_.htmlReportFile).launchInDefaultBrowser();
    }
}

void MeasurementViewerPanel::resized()
{
    auto b = getLocalBounds().reduced(16);

    // 1. Header (Top Row)
    auto topArea = b.removeFromTop(44);
    auto badgeArea = topArea.removeFromRight(320);
    lblStatusBadge_.setBounds(badgeArea.removeFromLeft(140).reduced(0, 8));
    badgeArea.removeFromLeft(10);
    lblIntegrityBadge_.setBounds(badgeArea.reduced(0, 8));

    lblTitle_.setBounds(topArea.removeFromTop(24));
    lblSubtitle_.setBounds(topArea);

    b.removeFromTop(6);

    // Diagnostic Reason alert (if visible)
    if (lblDiagnostic_.isVisible())
    {
        lblDiagnostic_.setBounds(b.removeFromTop(24));
        b.removeFromTop(4);
    }

    // 2. Metric Cards Row
    if (!metricCardViews_.empty())
    {
        auto cardRow = b.removeFromTop(64);
        int numCards = static_cast<int>(metricCardViews_.size());
        int cardGap = 8;
        int cardWidth = (cardRow.getWidth() - (numCards - 1) * cardGap) / numCards;

        for (int i = 0; i < numCards; ++i)
        {
            auto cBounds = cardRow.removeFromLeft(cardWidth);
            if (i < numCards - 1) cardRow.removeFromLeft(cardGap);

            auto& card = metricCardViews_[static_cast<size_t>(i)];
            card->lblName.setBounds(cBounds.removeFromTop(18).reduced(6, 0));
            card->lblValue.setBounds(cBounds.removeFromTop(24).reduced(6, 0));
            card->lblStatus.setBounds(cBounds.reduced(6, 0));
        }
        b.removeFromTop(10);
    }

    // 3. Action Buttons (Bottom Row)
    auto bottomArea = b.removeFromBottom(36);
    btnOpenReport_.setBounds(bottomArea.removeFromLeft(170));
    bottomArea.removeFromLeft(10);
    btnVerifyManifest_.setBounds(bottomArea.removeFromLeft(170));

    b.removeFromBottom(10);

    // 4. Audio Player (Middle-Bottom)
    auto audioArea = b.removeFromBottom(110);
    audioPlayerComponent_.setBounds(audioArea);

    b.removeFromBottom(10);

    // 5. Curve Visualizer (Remaining Space)
    curveComponent_.setBounds(b);
}

void MeasurementViewerPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Dark canvas background
    g.setColour(juce::Colour(0xff0b0f19));
    g.fillAll();

    // Paint cards background
    if (!metricCardViews_.empty())
    {
        for (const auto& card : metricCardViews_)
        {
            auto cb = card->lblName.getBounds().toFloat();
            cb.setHeight(64.0f);
            cb.setX(cb.getX() - 6.0f);
            cb.setWidth(cb.getWidth() + 12.0f);

            g.setColour(juce::Colour(0xff111827));
            g.fillRoundedRectangle(cb, 6.0f);
            g.setColour(juce::Colour(0xff1e293b));
            g.drawRoundedRectangle(cb, 6.0f, 1.0f);
        }
    }
}

} // namespace abdaudiolab::gui::measurement
