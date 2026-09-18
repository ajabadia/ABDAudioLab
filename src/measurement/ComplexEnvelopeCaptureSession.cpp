/**
 * @file ComplexEnvelopeCaptureSession.cpp
 * @brief Implementation of immutable capture container and latency compensation.
 * @author ABDSynths
 * @date 2026
 */

#include "ComplexEnvelopeCaptureSession.h"
#include "../synth/Sha256.h"
#include <algorithm>
#include <cmath>

namespace abdaudiolab::measurement
{

namespace
{
std::string computeAudioSha256(const float* data, size_t count)
{
    if (data == nullptr || count == 0)
        return "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"; // SHA-256 of empty
    const auto* bytePtr = reinterpret_cast<const uint8_t*>(data);
    return abdaudiolab::synth::Sha256::computeHex(bytePtr, count * sizeof(float));
}
} // anonymous namespace

RawAudioCapture::RawAudioCapture(std::vector<float> audioData,
                                 double sampleRate,
                                 int bufferSize,
                                 std::string driverIdentity,
                                 std::string captureStartClock)
    : samples_(std::move(audioData))
    , sampleRate_(sampleRate)
    , bufferSize_(bufferSize)
    , driverIdentity_(std::move(driverIdentity))
    , captureStartClock_(std::move(captureStartClock))
{
    sha256_ = computeAudioSha256(samples_.data(), samples_.size());
}

ComplexEnvelopeCaptureSession::ComplexEnvelopeCaptureSession(RawAudioCapture rawCapture)
    : rawCapture_(std::move(rawCapture))
{
}

AlignmentTransform ComplexEnvelopeCaptureSession::applyCalibration(const AudioChainCalibration& calibration)
{
    appliedCalibration_ = calibration;

    const auto rawSpan = rawCapture_.getSamples();
    if (rawSpan.empty())
    {
        compensatedSamples_.clear();
        compensatedSha256_ = computeAudioSha256(nullptr, 0);
        appliedTransform_ = AlignmentTransform::None;
        return appliedTransform_;
    }

    if (calibration.compensationDomain == CompensationDomain::TimeAxisOnly ||
        calibration.compensationDomain == CompensationDomain::NotApplied)
    {
        // Compensation is applied purely to time axis / timestamps; audio samples are untouched
        compensatedSamples_.clear();
        compensatedSha256_ = rawCapture_.getSha256();
        appliedTransform_ = AlignmentTransform::None;
        return appliedTransform_;
    }

    int shift = calibration.roundTripLatencySamples;

    if (shift == 0)
    {
        compensatedSamples_.assign(rawSpan.begin(), rawSpan.end());
        compensatedSha256_ = rawCapture_.getSha256();
        appliedTransform_ = AlignmentTransform::None;
        return appliedTransform_;
    }

    if (shift < 0 || static_cast<size_t>(shift) >= rawSpan.size())
    {
        // Out of bounds or unsupportable shift
        compensatedSamples_.clear();
        compensatedSha256_ = computeAudioSha256(nullptr, 0);
        appliedTransform_ = AlignmentTransform::Rejected;
        return appliedTransform_;
    }

    // Advance by shift samples (delay compensation)
    size_t newSize = rawSpan.size() - static_cast<size_t>(shift);
    compensatedSamples_.resize(newSize);
    std::copy(rawSpan.begin() + shift, rawSpan.end(), compensatedSamples_.begin());

    compensatedSha256_ = computeAudioSha256(compensatedSamples_.data(), compensatedSamples_.size());
    appliedTransform_ = AlignmentTransform::IntegerSampleShift;
    return appliedTransform_;
}

std::span<const float> ComplexEnvelopeCaptureSession::getCompensatedSamples() const noexcept
{
    if (hasCompensatedAudio())
        return compensatedSamples_;
    return rawCapture_.getSamples();
}

} // namespace abdaudiolab::measurement
