/**
 * @file MeasurementAudioPlayerComponent.h
 * @brief Native audio player and waveform component with on-demand SHA-256 integrity verification.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>

namespace abdaudiolab::gui::measurement
{

class MeasurementAudioPlayerComponent : public juce::Component,
                                        public juce::ChangeListener,
                                        private juce::Timer
{
public:
    MeasurementAudioPlayerComponent();
    ~MeasurementAudioPlayerComponent() override;

    void setAudioFile(const juce::File& audioFile,
                      const juce::String& expectedSha256,
                      bool isIntegrityVerified);

    void clear();
    void updateTheme();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    std::function<void(const juce::String& diagnostic)> onPlaybackBlockedByCorruption;

    [[nodiscard]] bool isPlaying() const noexcept;

private:
    void timerCallback() override;
    void handlePlayPause();
    void handleStop();
    void updateTimeLabel();

    juce::AudioFormatManager formatManager_;
    std::unique_ptr<juce::AudioThumbnailCache> thumbnailCache_;
    std::unique_ptr<juce::AudioThumbnail> thumbnail_;

    juce::AudioTransportSource transportSource_;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource_;

    juce::File audioFile_;
    juce::String expectedSha256_;
    bool isIntegrityVerified_ { true };
    bool isCorrupt_ { false };

    juce::TextButton btnPlayPause_ { "Play" };
    juce::TextButton btnStop_ { "Stop" };
    juce::Label lblTime_;
    juce::Label lblStatus_;

    juce::Rectangle<float> waveformBounds_;
};

} // namespace abdaudiolab::gui::measurement
