/**
 * @file AsyncLoopbackProfileLoader.cpp
 * @brief Implementation of AsyncLoopbackProfileLoader.
 * @author ABDSynths
 * @date 2026
 */

#include "AsyncLoopbackProfileLoader.h"

namespace abdaudiolab::calibration
{

AsyncLoopbackProfileLoader::AsyncLoopbackProfileLoader(
    const LoopbackProfileStore& store,
    std::string deviceName,
    double sampleRate,
    int bufferSize,
    CompletionCallback onComplete)
    : juce::ThreadPoolJob("AsyncLoopbackProfileLoader"),
      store_(store),
      deviceName_(std::move(deviceName)),
      sampleRate_(sampleRate),
      bufferSize_(bufferSize),
      onComplete_(std::move(onComplete))
{
}

juce::ThreadPoolJob::JobStatus AsyncLoopbackProfileLoader::runJob()
{
    if (shouldExit())
        return jobHasFinished;

    auto matchingProfile = store_.findMatchingProfile(deviceName_, sampleRate_, bufferSize_);

    if (shouldExit())
        return jobHasFinished;

    if (onComplete_)
    {
        // Safely post completion callback to the JUCE message thread
        juce::MessageManager::callAsync([cb = onComplete_, profile = std::move(matchingProfile)]() mutable {
            cb(std::move(profile));
        });
    }

    return jobHasFinished;
}

void AsyncLoopbackProfileLoader::dispatch(
    juce::ThreadPool& pool,
    const LoopbackProfileStore& store,
    const std::string& deviceName,
    double sampleRate,
    int bufferSize,
    CompletionCallback onComplete)
{
    auto* job = new AsyncLoopbackProfileLoader(store, deviceName, sampleRate, bufferSize, std::move(onComplete));
    pool.addJob(job, true); // true = ThreadPool takes ownership and deletes job upon completion
}

} // namespace abdaudiolab::calibration
