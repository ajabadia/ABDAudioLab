/**
 * @file MeasurementAudioPlayerComponent.cpp
 * @brief Implementation of MeasurementAudioPlayerComponent.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementAudioPlayerComponent.h"
#include "MeasurementViewModelLoader.h"
#include <iomanip>
#include <sstream>

namespace abdaudiolab::gui::measurement
{

MeasurementAudioPlayerComponent::MeasurementAudioPlayerComponent()
{
    formatManager_.registerBasicFormats();
    thumbnailCache_ = std::make_unique<juce::AudioThumbnailCache>(5);
    thumbnail_ = std::make_unique<juce::AudioThumbnail>(512, formatManager_, *thumbnailCache_);
    thumbnail_->addChangeListener(this);

    // Play / Pause Button
    btnPlayPause_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0284c7));
    btnPlayPause_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff8fafc));
    btnPlayPause_.onClick = [this] { handlePlayPause(); };
    addAndMakeVisible(btnPlayPause_);

    // Stop Button
    btnStop_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    btnStop_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffcbd5e1));
    btnStop_.onClick = [this] { handleStop(); };
    addAndMakeVisible(btnStop_);

    // Time Label
    lblTime_.setFont(juce::Font("Consolas", 12.0f, juce::Font::plain));
    lblTime_.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    lblTime_.setText("0.00 / 0.00 s", juce::dontSendNotification);
    addAndMakeVisible(lblTime_);

    // Status Label
    lblStatus_.setFont(juce::Font(11.5f, juce::Font::bold));
    lblStatus_.setColour(juce::Label::textColourId, juce::Colour(0xff38bdf8));
    lblStatus_.setText("No audio loaded", juce::dontSendNotification);
    addAndMakeVisible(lblStatus_);

    btnPlayPause_.setEnabled(false);
    btnStop_.setEnabled(false);

    startTimerHz(25); // 25 fps UI update for playhead
}

MeasurementAudioPlayerComponent::~MeasurementAudioPlayerComponent()
{
    stopTimer();
    transportSource_.setSource(nullptr);
    readerSource_.reset();
}

void MeasurementAudioPlayerComponent::setAudioFile(const juce::File& audioFile,
                                                   const juce::String& expectedSha256,
                                                   bool isIntegrityVerified)
{
    audioFile_ = audioFile;
    expectedSha256_ = expectedSha256;
    isIntegrityVerified_ = isIntegrityVerified;
    isCorrupt_ = !isIntegrityVerified_;

    transportSource_.stop();
    transportSource_.setSource(nullptr);
    readerSource_.reset();

    if (!audioFile_.existsAsFile())
    {
        lblStatus_.setText("[!] Audio file not found", juce::dontSendNotification);
        lblStatus_.setColour(juce::Label::textColourId, juce::Colour(0xfffbbf24));
        btnPlayPause_.setEnabled(false);
        btnStop_.setEnabled(false);
        thumbnail_->clear();
        repaint();
        return;
    }

    // If already flagged corrupt from container check, block immediately
    if (isCorrupt_)
    {
        lblStatus_.setText("[X] Playback blocked: Corrupt or unverified container", juce::dontSendNotification);
        lblStatus_.setColour(juce::Label::textColourId, juce::Colour(0xfff87171));
        btnPlayPause_.setEnabled(false);
        btnStop_.setEnabled(false);
        thumbnail_->setSource(new juce::FileInputSource(audioFile_));
        repaint();
        return;
    }

    // Load reader and prepare transport
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager_.createReaderFor(audioFile_));
    if (reader != nullptr)
    {
        double sRate = reader->sampleRate;
        readerSource_ = std::make_unique<juce::AudioFormatReaderSource>(reader.release(), true);
        transportSource_.setSource(readerSource_.get(), 0, nullptr, sRate);

        thumbnail_->setSource(new juce::FileInputSource(audioFile_));

        lblStatus_.setText("[OK] Reference audio loaded (PCM WAV)", juce::dontSendNotification);
        lblStatus_.setColour(juce::Label::textColourId, juce::Colour(0xff34d399));
        btnPlayPause_.setEnabled(true);
        btnStop_.setEnabled(true);
        btnPlayPause_.setButtonText("Play");
    }
    else
    {
        lblStatus_.setText("[X] Unsupported audio format", juce::dontSendNotification);
        lblStatus_.setColour(juce::Label::textColourId, juce::Colour(0xfff87171));
        btnPlayPause_.setEnabled(false);
        btnStop_.setEnabled(false);
    }

    updateTimeLabel();
    repaint();
}

void MeasurementAudioPlayerComponent::clear()
{
    transportSource_.stop();
    transportSource_.setSource(nullptr);
    readerSource_.reset();
    thumbnail_->clear();
    audioFile_ = juce::File();
    expectedSha256_ = "";
    isIntegrityVerified_ = true;
    isCorrupt_ = false;

    btnPlayPause_.setEnabled(false);
    btnStop_.setEnabled(false);
    btnPlayPause_.setButtonText("Play");
    lblStatus_.setText("No audio loaded", juce::dontSendNotification);
    lblStatus_.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    lblTime_.setText("0.00 / 0.00 s", juce::dontSendNotification);
    repaint();
}

bool MeasurementAudioPlayerComponent::isPlaying() const noexcept
{
    return transportSource_.isPlaying();
}

void MeasurementAudioPlayerComponent::handlePlayPause()
{
    if (isCorrupt_ || !audioFile_.existsAsFile())
        return;

    if (transportSource_.isPlaying())
    {
        transportSource_.stop();
        btnPlayPause_.setButtonText("Play");
    }
    else
    {
        // Mandatory Adjustment 2: On-demand hash re-verification before starting playback
        if (!MeasurementViewModelLoader::verifyAudioFileSha256(audioFile_, expectedSha256_))
        {
            isCorrupt_ = true;
            isIntegrityVerified_ = false;
            transportSource_.stop();
            btnPlayPause_.setEnabled(false);
            btnStop_.setEnabled(false);
            lblStatus_.setText("[X] Playback blocked: Audio artifact was modified on disk", juce::dontSendNotification);
            lblStatus_.setColour(juce::Label::textColourId, juce::Colour(0xfff87171));

            if (onPlaybackBlockedByCorruption != nullptr)
                onPlaybackBlockedByCorruption("Audio artifact was modified or corrupted on disk prior to playback");

            repaint();
            return;
        }

        transportSource_.start();
        btnPlayPause_.setButtonText("Pause");
    }
}

void MeasurementAudioPlayerComponent::handleStop()
{
    transportSource_.stop();
    transportSource_.setPosition(0.0);
    btnPlayPause_.setButtonText("Play");
    updateTimeLabel();
    repaint();
}

void MeasurementAudioPlayerComponent::updateTimeLabel()
{
    double cur = transportSource_.getCurrentPosition();
    double total = transportSource_.getLengthInSeconds();
    if (total <= 0.0 && thumbnail_ != nullptr)
        total = thumbnail_->getTotalLength();

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << cur << " / " << total << " s";
    lblTime_.setText(ss.str(), juce::dontSendNotification);
}

void MeasurementAudioPlayerComponent::timerCallback()
{
    if (transportSource_.isPlaying())
    {
        updateTimeLabel();
        if (transportSource_.hasStreamFinished())
        {
            handleStop();
        }
        repaint();
    }
}

void MeasurementAudioPlayerComponent::changeListenerCallback(juce::ChangeBroadcaster* /*source*/)
{
    repaint();
}

void MeasurementAudioPlayerComponent::mouseDown(const juce::MouseEvent& e)
{
    if (waveformBounds_.contains(e.position) && !isCorrupt_ && thumbnail_->getTotalLength() > 0.0)
    {
        double ratio = (e.position.x - waveformBounds_.getX()) / waveformBounds_.getWidth();
        ratio = std::clamp(ratio, 0.0, 1.0);
        double targetPos = ratio * thumbnail_->getTotalLength();
        transportSource_.setPosition(targetPos);
        updateTimeLabel();
        repaint();
    }
}

void MeasurementAudioPlayerComponent::resized()
{
    auto b = getLocalBounds();
    int pad = 12;

    auto ctrlArea = b.removeFromTop(36).reduced(pad, 4);
    btnPlayPause_.setBounds(ctrlArea.removeFromLeft(70));
    ctrlArea.removeFromLeft(8);
    btnStop_.setBounds(ctrlArea.removeFromLeft(60));
    ctrlArea.removeFromLeft(14);
    lblTime_.setBounds(ctrlArea.removeFromLeft(120));
    ctrlArea.removeFromLeft(10);
    lblStatus_.setBounds(ctrlArea);

    waveformBounds_ = b.reduced(pad, 8).toFloat();
}

void MeasurementAudioPlayerComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // 1. Dark container background
    g.setColour(juce::Colour(0xff0f172a));
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(juce::Colour(0xff1e293b));
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

    // 2. Waveform area background
    g.setColour(juce::Colour(0xff111827));
    g.fillRoundedRectangle(waveformBounds_, 4.0f);

    if (isCorrupt_)
    {
        g.setColour(juce::Colour(0xff7f1d1d));
        g.drawRoundedRectangle(waveformBounds_, 4.0f, 1.5f);
        g.setColour(juce::Colour(0xfff87171));
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText("[X] PLAYBACK BLOCKED — ARTIFACT HASH MISMATCH", waveformBounds_, juce::Justification::centred, false);
        return;
    }

    if (thumbnail_ == nullptr || thumbnail_->getTotalLength() <= 0.0)
    {
        g.setColour(juce::Colour(0xff64748b));
        g.setFont(juce::Font(12.0f));
        g.drawText("No waveform available", waveformBounds_, juce::Justification::centred, false);
        return;
    }

    // 3. Draw waveform channels
    g.setColour(juce::Colour(0xff0284c7));
    thumbnail_->drawChannels(g, waveformBounds_.toNearestInt(), 0.0, thumbnail_->getTotalLength(), 1.0f);

    // 4. Draw playhead
    double len = thumbnail_->getTotalLength();
    if (len > 0.0)
    {
        double curPos = transportSource_.getCurrentPosition();
        float playheadX = waveformBounds_.getX() + static_cast<float>(curPos / len) * waveformBounds_.getWidth();
        playheadX = std::clamp(playheadX, waveformBounds_.getX(), waveformBounds_.getRight());

        g.setColour(juce::Colour(0xff38bdf8));
        g.drawLine(playheadX, waveformBounds_.getY(), playheadX, waveformBounds_.getBottom(), 2.0f);
    }
}

} // namespace abdaudiolab::gui::measurement
