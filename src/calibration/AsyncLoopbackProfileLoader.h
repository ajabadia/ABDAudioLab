/**
 * @file AsyncLoopbackProfileLoader.h
 * @brief Asynchronous, non-blocking disk loader for .abdloop profiles using juce::ThreadPoolJob.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "LoopbackProfileStore.h"
#include <functional>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <memory>
#include <optional>

namespace abdaudiolab::calibration
{

/**
 * @class AsyncLoopbackProfileLoader
 * @brief ThreadPool job that searches and loads matching .abdloop profiles off the message thread.
 */
class AsyncLoopbackProfileLoader : public juce::ThreadPoolJob
{
public:
    using CompletionCallback = std::function<void(std::optional<LoopbackProfile>)>;

    AsyncLoopbackProfileLoader(
        const LoopbackProfileStore& store,
        std::string deviceName,
        double sampleRate,
        int bufferSize,
        CompletionCallback onComplete);

    ~AsyncLoopbackProfileLoader() override = default;

    /**
     * @brief Background execution executed on worker thread.
     */
    JobStatus runJob() override;

    /**
     * @brief Helper to dispatch the job to a given thread pool.
     */
    static void dispatch(
        juce::ThreadPool& pool,
        const LoopbackProfileStore& store,
        const std::string& deviceName,
        double sampleRate,
        int bufferSize,
        CompletionCallback onComplete);

private:
    LoopbackProfileStore store_;
    std::string deviceName_;
    double sampleRate_{48000.0};
    int bufferSize_{256};
    CompletionCallback onComplete_;
};

} // namespace abdaudiolab::calibration
