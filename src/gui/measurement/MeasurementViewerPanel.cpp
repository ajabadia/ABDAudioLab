/**
 * @file MeasurementViewerPanel.cpp
 * @brief Implementation of MeasurementViewerPanel.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementViewerPanel.h"
#include "MeasurementViewModelLoader.h"
#include "../SessionReportManager.h"
#include <thread>
#include <iomanip>
#include <sstream>

namespace abdaudiolab::gui::measurement
{

MeasurementViewerPanel::MeasurementViewerPanel()
{
    // Title & Subtitle
    lblTitle_.setFont(juce::Font(juce::FontOptions(18.0f)).boldened());
    lblTitle_.setColour(juce::Label::textColourId, juce::Colour(0xfff8fafc));
    lblTitle_.setText("Envelope Measurement Viewer", juce::dontSendNotification);
    addAndMakeVisible(lblTitle_);

    lblSubtitle_.setFont(juce::Font(juce::FontOptions(12.0f)));
    lblSubtitle_.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    lblSubtitle_.setText("Target: None | No measurement container loaded", juce::dontSendNotification);
    addAndMakeVisible(lblSubtitle_);

    // Domain Badge
    lblDomainBadge_.setFont(juce::Font(juce::FontOptions(10.5f)).boldened());
    lblDomainBadge_.setJustificationType(juce::Justification::centred);
    lblDomainBadge_.setVisible(false);
    addAndMakeVisible(lblDomainBadge_);

    // Status Badge
    lblStatusBadge_.setFont(juce::Font(juce::FontOptions(11.0f)).boldened());
    lblStatusBadge_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(lblStatusBadge_);

    // Integrity Badge
    lblIntegrityBadge_.setFont(juce::Font(juce::FontOptions(11.0f)).boldened());
    lblIntegrityBadge_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(lblIntegrityBadge_);

    // Diagnostic label
    lblDiagnostic_.setFont(juce::Font(juce::FontOptions(11.5f)));
    lblDiagnostic_.setColour(juce::Label::textColourId, juce::Colour(0xfffbbf24));
    addAndMakeVisible(lblDiagnostic_);

    // Curve Subcomponents
    addAndMakeVisible(curveComponent_);
    addAndMakeVisible(freqCurveComponent_);
    freqCurveComponent_.setVisible(false);

    // Audio Track Selector
    btnTrackOutput_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0284c7));
    btnTrackOutput_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff8fafc));
    btnTrackOutput_.onClick = [this] { selectAudioTrack(0); };
    btnTrackOutput_.setVisible(false);
    addAndMakeVisible(btnTrackOutput_);

    btnTrackInput_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    btnTrackInput_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff94a3b8));
    btnTrackInput_.onClick = [this] { selectAudioTrack(1); };
    btnTrackInput_.setVisible(false);
    addAndMakeVisible(btnTrackInput_);

    btnTrackIr_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    btnTrackIr_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff94a3b8));
    btnTrackIr_.onClick = [this] { selectAudioTrack(2); };
    btnTrackIr_.setVisible(false);
    addAndMakeVisible(btnTrackIr_);

    // Audio Player Subcomponent
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
        freqCurveComponent_.setCurve(model_.curve, model_.slopeFit, -1.0, false, false);
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

void MeasurementViewerPanel::selectAudioTrack(int trackIndex)
{
    selectedAudioTrack_ = trackIndex;
    bool verified = (model_.integrityStatus != UiIntegrityStatus::Corrupt);

    auto updateBtn = [](juce::TextButton& btn, bool active) {
        btn.setColour(juce::TextButton::buttonColourId, active ? juce::Colour(0xff0284c7) : juce::Colour(0xff1e293b));
        btn.setColour(juce::TextButton::textColourOffId, active ? juce::Colour(0xfff8fafc) : juce::Colour(0xff94a3b8));
    };

    updateBtn(btnTrackOutput_, trackIndex == 0);
    updateBtn(btnTrackInput_, trackIndex == 1);
    updateBtn(btnTrackIr_, trackIndex == 2);

    if (trackIndex == 1)
        audioPlayerComponent_.setAudioFile(model_.stimulusAudioFile, model_.expectedStimulusAudioSha256, verified);
    else if (trackIndex == 2)
        audioPlayerComponent_.setAudioFile(model_.impulseResponseFile, model_.expectedImpulseResponseSha256, verified);
    else
        audioPlayerComponent_.setAudioFile(model_.audioFile, model_.expectedAudioSha256, verified);
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

    // Title & Subtitle
    if (model_.measurementType == "filter")
    {
        juce::String topStr = model_.filterTopology.isNotEmpty() ? (model_.filterTopology.toUpperCase() + " ") : "";
        lblTitle_.setText(topStr + "Filter Response Measurement Viewer", juce::dontSendNotification);
    }
    else if (model_.measurementType == "dynamics")
    {
        lblTitle_.setText("MIDI Dynamics Response Measurement Viewer", juce::dontSendNotification);
    }
    else if (model_.measurementType == "modulation")
    {
        lblTitle_.setText("Cyclic Modulation (LFO) Measurement Viewer", juce::dontSendNotification);
    }
    else
    {
        lblTitle_.setText("Envelope Measurement Viewer", juce::dontSendNotification);
    }

    juce::String sub = "Target: " + (model_.dutName.isNotEmpty() ? model_.dutName : "Unknown")
                     + " (" + (model_.dutFormat.isNotEmpty() ? model_.dutFormat : "VST3") + ")"
                     + " | ID: " + model_.measurementId
                     + " | " + juce::String(static_cast<int>(model_.sampleRateHz)) + " Hz";
    lblSubtitle_.setText(sub, juce::dontSendNotification);

    // Setup cards
    metricCardViews_.clear();

    bool isMidiProxy = (model_.measurementType == "filter" && model_.measurementDomain == "synthesizedSpectralResponse");

    for (const auto& m : model_.metrics)
    {
        // Enforce metrological rule: do not show isolated transfer function cards for MIDI composite response
        if (isMidiProxy && (m.name == "cutoffFrequency" || m.name == "asymptoticSlope" || m.name == "qFactor"))
        {
            continue;
        }

        auto card = std::make_unique<MetricCard>();
        card->lblName.setFont(juce::Font(juce::FontOptions(10.5f)).boldened());
        card->lblName.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
        card->lblName.setText(m.name.toUpperCase(), juce::dontSendNotification);
        addAndMakeVisible(card->lblName);

        card->lblValue.setFont(juce::FontOptions("Consolas", 15.0f, juce::Font::bold));
        card->lblValue.setColour(juce::Label::textColourId, juce::Colour(0xfff8fafc));

        if (m.status == "not_observable")
        {
            card->lblValue.setText("not_observable", juce::dontSendNotification);
        }
        else
        {
            std::ostringstream valStream;
            valStream << std::fixed << std::setprecision(1) << m.value << " " << m.unit.toStdString();
            card->lblValue.setText(valStream.str(), juce::dontSendNotification);
        }
        addAndMakeVisible(card->lblValue);

        card->lblStatus.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
        if (m.status == "observed")
        {
            card->lblStatus.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
            card->lblStatus.setText("[OK] OBSERVED", juce::dontSendNotification);
        }
        else if (m.status == "not_observable")
        {
            card->lblStatus.setColour(juce::Label::textColourId, juce::Colour(0xff64748b));
            card->lblStatus.setText("[-] NOT OBSERVABLE", juce::dontSendNotification);
        }
        else if (m.status == "invalid")
        {
            card->lblStatus.setColour(juce::Label::textColourId, juce::Colour(0xfff87171));
            card->lblStatus.setText("[X] INVALID", juce::dontSendNotification);
        }
        else
        {
            card->lblStatus.setColour(juce::Label::textColourId, juce::Colour(0xfffbbf24));
            card->lblStatus.setText("[!] UNRELIABLE", juce::dontSendNotification);
        }
        addAndMakeVisible(card->lblStatus);

        metricCardViews_.push_back(std::move(card));
    }

    // If filter directTransferFunction and slopeFit present, add Slope Fit & R^2 card
    if (model_.measurementType == "filter" && !isMidiProxy && model_.slopeFit.has_value())
    {
        auto card = std::make_unique<MetricCard>();
        card->lblName.setFont(juce::Font(juce::FontOptions(10.5f)).boldened());
        card->lblName.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
        card->lblName.setText("SLOPE FIT & R²", juce::dontSendNotification);
        addAndMakeVisible(card->lblName);

        card->lblValue.setFont(juce::FontOptions("Consolas", 15.0f, juce::Font::bold));
        card->lblValue.setColour(juce::Label::textColourId, juce::Colour(0xfff8fafc));
        std::ostringstream r2Stream;
        r2Stream << "R²=" << std::fixed << std::setprecision(3) << model_.slopeFit->rSquared;
        card->lblValue.setText(r2Stream.str(), juce::dontSendNotification);
        addAndMakeVisible(card->lblValue);

        card->lblStatus.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
        bool goodFit = (model_.slopeFit->rSquared >= 0.90);
        card->lblStatus.setColour(juce::Label::textColourId, goodFit ? juce::Colour(0xff38bdf8) : juce::Colour(0xfffbbf24));
        card->lblStatus.setText(goodFit ? "[OK] OBSERVED" : "[!] UNRELIABLE", juce::dontSendNotification);
        addAndMakeVisible(card->lblStatus);

        metricCardViews_.push_back(std::move(card));
    }

    // Diagnostic reason
    if (model_.diagnosticReason.isNotEmpty() && model_.diagnosticReason != "Envelope successfully observed" && model_.diagnosticReason != "Filter response successfully observed")
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

    if (model_.measurementType == "filter")
    {
        curveComponent_.setVisible(false);
        freqCurveComponent_.setVisible(true);

        double cutoffHz = -1.0;
        bool isCutoffObs = false;
        for (const auto& m : model_.metrics)
        {
            if (m.name == "cutoffFrequency" && m.status == "observed" && m.value > 0.0)
            {
                cutoffHz = m.value;
                isCutoffObs = true;
                break;
            }
        }
        freqCurveComponent_.setCurve(model_.curve, model_.slopeFit, cutoffHz, isCutoffObs, verified);

        btnTrackOutput_.setVisible(true);
        btnTrackInput_.setVisible(model_.stimulusAudioFile.existsAsFile());
        btnTrackIr_.setVisible(model_.impulseResponseFile.existsAsFile());
        selectAudioTrack(0);
    }
    else
    {
        freqCurveComponent_.setVisible(false);
        curveComponent_.setVisible(true);
        curveComponent_.setCurve(model_.curve, verified);

        btnTrackOutput_.setVisible(false);
        btnTrackInput_.setVisible(false);
        btnTrackIr_.setVisible(false);
        audioPlayerComponent_.setAudioFile(model_.audioFile, model_.expectedAudioSha256, verified);
    }

    updateIntegrityUi();
    updateHeaderAndBadges();
    resized();
}

void MeasurementViewerPanel::updateHeaderAndBadges()
{
    // Domain badge
    if (model_.measurementDomain == "directTransferFunction")
    {
        lblDomainBadge_.setText("DIRECT TRANSFER [H(w)]", juce::dontSendNotification);
        lblDomainBadge_.setColour(juce::Label::backgroundColourId, juce::Colour(0xff0c4a6e));
        lblDomainBadge_.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
        lblDomainBadge_.setColour(juce::Label::outlineColourId, juce::Colour(0xff0284c7));
        lblDomainBadge_.setVisible(true);
    }
    else if (model_.measurementDomain == "synthesizedSpectralResponse")
    {
        lblDomainBadge_.setText("SYNTHESIZED SPECTRAL [PROXY]", juce::dontSendNotification);
        lblDomainBadge_.setColour(juce::Label::backgroundColourId, juce::Colour(0xff78350f));
        lblDomainBadge_.setColour(juce::Label::textColourId, juce::Colour(0xfffbbf24));
        lblDomainBadge_.setColour(juce::Label::outlineColourId, juce::Colour(0xffd97706));
        lblDomainBadge_.setVisible(true);
    }
    else if (model_.measurementType == "dynamics")
    {
        lblDomainBadge_.setText("DYNAMIC: MIDI_VELOCITY", juce::dontSendNotification);
        lblDomainBadge_.setColour(juce::Label::backgroundColourId, juce::Colour(0xff0c4a6e));
        lblDomainBadge_.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
        lblDomainBadge_.setColour(juce::Label::outlineColourId, juce::Colour(0xff0284c7));
        lblDomainBadge_.setVisible(true);
    }
    else if (model_.measurementType == "modulation")
    {
        juce::String dest = (model_.modulationResult.has_value() && model_.modulationResult->depth.unit == "cents") ? "PITCH" : "AMPLITUDE";
        lblDomainBadge_.setText("MODULATION: " + dest, juce::dontSendNotification);
        lblDomainBadge_.setColour(juce::Label::backgroundColourId, juce::Colour(0xff0c4a6e));
        lblDomainBadge_.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
        lblDomainBadge_.setColour(juce::Label::outlineColourId, juce::Colour(0xff0284c7));
        lblDomainBadge_.setVisible(true);
    }
    else
    {
        lblDomainBadge_.setVisible(false);
    }

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

    std::thread([safeThis, targetDir]() {
        juce::String diagnostic;
        bool ok = MeasurementViewModelLoader::verifyContainerIntegrity(targetDir, diagnostic);

        juce::MessageManager::callAsync([safeThis, ok, diagnostic]() {
            if (safeThis == nullptr)
                return;

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

    bool verified = (model_.integrityStatus != UiIntegrityStatus::Corrupt);
    if (model_.measurementType == "filter")
    {
        double cutoffHz = -1.0;
        bool isCutoffObs = false;
        for (const auto& m : model_.metrics)
        {
            if (m.name == "cutoffFrequency" && m.status == "observed" && m.value > 0.0)
            {
                cutoffHz = m.value;
                isCutoffObs = true;
                break;
            }
        }
        freqCurveComponent_.setCurve(model_.curve, model_.slopeFit, cutoffHz, isCutoffObs, verified);
    }
    else
    {
        curveComponent_.setCurve(model_.curve, verified);
    }

    selectAudioTrack(selectedAudioTrack_);
}

void MeasurementViewerPanel::openHtmlReportInBrowser()
{
    if (model_.htmlReportFile.existsAsFile())
    {
        juce::String err;
        SessionReportManager::launchHtmlReportInDefaultViewer(model_.htmlReportFile, err);
    }
}

void MeasurementViewerPanel::resized()
{
    auto b = getLocalBounds().reduced(16);

    // 1. Header (Top Row)
    auto topArea = b.removeFromTop(44);
    auto badgeArea = topArea.removeFromRight(500);

    if (lblDomainBadge_.isVisible())
    {
        lblDomainBadge_.setBounds(badgeArea.removeFromLeft(180).reduced(0, 8));
        badgeArea.removeFromLeft(8);
    }

    lblStatusBadge_.setBounds(badgeArea.removeFromLeft(140).reduced(0, 8));
    badgeArea.removeFromLeft(8);
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
    auto audioArea = b.removeFromBottom(100);
    audioPlayerComponent_.setBounds(audioArea);

    // 4b. Track selector row (if visible for filter)
    if (btnTrackOutput_.isVisible())
    {
        b.removeFromBottom(6);
        auto trackRow = b.removeFromBottom(26);
        btnTrackOutput_.setBounds(trackRow.removeFromLeft(140));
        trackRow.removeFromLeft(8);
        if (btnTrackInput_.isVisible())
        {
            btnTrackInput_.setBounds(trackRow.removeFromLeft(140));
            trackRow.removeFromLeft(8);
        }
        if (btnTrackIr_.isVisible())
        {
            btnTrackIr_.setBounds(trackRow.removeFromLeft(180));
        }
        b.removeFromBottom(8);
    }
    else
    {
        b.removeFromBottom(10);
    }

    // 5. Curve Visualizers (Remaining Space)
    curveComponent_.setBounds(b);
    freqCurveComponent_.setBounds(b);
}

void MeasurementViewerPanel::paint(juce::Graphics& g)
{
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
