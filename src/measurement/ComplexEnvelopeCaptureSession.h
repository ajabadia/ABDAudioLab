/**
 * @file ComplexEnvelopeCaptureSession.h
 * @brief Immutable capture session managing raw and latency-compensated audio buffers with telemetry.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "ComplexEnvelopeOrchestratorContracts.h"
#include <vector>
#include <string>
#include <span>
#include <cstdint>
#include <optional>

namespace abdaudiolab::measurement
{

/**
 * @brief Strictly immutable raw audio capture container.
 */
class RawAudioCapture
{
public:
    RawAudioCapture() = default;
    explicit RawAudioCapture(std::vector<float> audioData,
                             double sampleRate = 48000.0,
                             int bufferSize = 512,
                             std::string driverIdentity = "GenericDriver",
                             std::string captureStartClock = "0");

    [[nodiscard]] std::span<const float> getSamples() const noexcept { return samples_; }
    [[nodiscard]] size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] bool empty() const noexcept { return samples_.empty(); }

    [[nodiscard]] double getSampleRate() const noexcept { return sampleRate_; }
    [[nodiscard]] int getBufferSize() const noexcept { return bufferSize_; }
    [[nodiscard]] const std::string& getDriverIdentity() const noexcept { return driverIdentity_; }
    [[nodiscard]] const std::string& getCaptureStartClock() const noexcept { return captureStartClock_; }

    [[nodiscard]] const std::string& getRawFormat() const noexcept { return rawFormat_; }
    [[nodiscard]] const std::string& getChannelLayout() const noexcept { return channelLayout_; }
    [[nodiscard]] const std::string& getSampleType() const noexcept { return sampleType_; }
    [[nodiscard]] const std::string& getEndianness() const noexcept { return endianness_; }
    [[nodiscard]] const std::string& getSha256() const noexcept { return sha256_; }

    [[nodiscard]] int getUnderruns() const noexcept { return underruns_; }
    [[nodiscard]] int getOverruns() const noexcept { return overruns_; }
    [[nodiscard]] int getDropoutsDetected() const noexcept { return dropoutsDetected_; }

    void setTelemetry(int underruns, int overruns, int dropouts) noexcept
    {
        underruns_ = underruns;
        overruns_ = overruns;
        dropoutsDetected_ = dropouts;
    }

private:
    std::vector<float> samples_;
    double sampleRate_ { 48000.0 };
    int bufferSize_ { 512 };
    std::string driverIdentity_ { "GenericDriver" };
    std::string captureStartClock_ { "0" };

    std::string rawFormat_ { "PCM_FLOAT" };
    std::string channelLayout_ { "mono" };
    std::string sampleType_ { "float32" };
    std::string endianness_ { "little_endian" };
    std::string sha256_;

    int underruns_ { 0 };
    int overruns_ { 0 };
    int dropoutsDetected_ { 0 };
};

/**
 * @brief Dual-layer capture session preserving raw audio alongside latency-compensated audio.
 */
class ComplexEnvelopeCaptureSession
{
public:
    ComplexEnvelopeCaptureSession() = default;
    explicit ComplexEnvelopeCaptureSession(RawAudioCapture rawCapture);

    [[nodiscard]] const RawAudioCapture& getRawCapture() const noexcept { return rawCapture_; }

    /**
     * @brief Applies an audio chain calibration to generate the compensated buffer without modifying rawCapture.
     * @param calibration Calibrated round-trip latency.
     * @return AlignmentTransform performed (e.g. IntegerSampleShift or FractionalSampleShift).
     */
    AlignmentTransform applyCalibration(const AudioChainCalibration& calibration);

    [[nodiscard]] std::span<const float> getCompensatedSamples() const noexcept;
    [[nodiscard]] const std::string& getCompensatedSha256() const noexcept { return compensatedSha256_; }
    [[nodiscard]] bool hasCompensatedAudio() const noexcept { return !compensatedSamples_.empty(); }

    [[nodiscard]] const std::optional<AudioChainCalibration>& getAppliedCalibration() const noexcept
    {
        return appliedCalibration_;
    }

    [[nodiscard]] AlignmentTransform getAppliedTransform() const noexcept
    {
        return appliedTransform_;
    }

private:
    RawAudioCapture rawCapture_;
    std::vector<float> compensatedSamples_;
    std::string compensatedSha256_;
    std::optional<AudioChainCalibration> appliedCalibration_;
    AlignmentTransform appliedTransform_ { AlignmentTransform::None };
};

} // namespace abdaudiolab::measurement
