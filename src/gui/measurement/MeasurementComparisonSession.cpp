/**
 * @file MeasurementComparisonSession.cpp
 * @brief Implementation of MeasurementComparisonSession with cooperative cancellation,
 *        session generation monotonic tracking, preventive resource bounds, and safe lifecycle draining.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementComparisonSession.h"
#include "../../synth/Sha256.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>

namespace abdaudiolab::gui::measurement
{

static const std::vector<juce::Colour> kAccessibleColours = {
    juce::Colour(0xff00d4ff), // Vibrant Cyan
    juce::Colour(0xffffa726), // Warm Amber
    juce::Colour(0xffe040fb), // Electric Violet
    juce::Colour(0xff76ff03), // Lime Green
    juce::Colour(0xffff5252), // Bright Coral
    juce::Colour(0xff40c4ff), // Light Blue
    juce::Colour(0xffffeb3b), // Metrological Yellow
    juce::Colour(0xffb388ff)  // Lavender
};

std::string ComparisonExclusionRecord::computeBasisHash(const std::string& idOrLabelA,
                                                        const std::string& idOrLabelB,
                                                        const std::string& metricName,
                                                        const std::string& unit,
                                                        const std::string& domain,
                                                        const std::string& rulesVer)
{
    std::string a = idOrLabelA;
    std::string b = idOrLabelB;
    if (a > b)
        std::swap(a, b);

    const std::string raw = rulesVer + "|" + a + "|" + b + "|" + metricName + "|" + unit + "|" + domain;
    return abdaudiolab::synth::Sha256::computeHex(raw);
}

namespace
{

/**
 * @brief Helper to evaluate resource quotas before reading files or allocating vectors.
 */
bool checkResourceLimitsPreLoad(const juce::File& containerDir,
                                const SessionResourceLimits& limits,
                                int currentContainerCount,
                                juce::String& outError)
{
    if (currentContainerCount >= limits.maxContainers)
    {
        outError = "resource_limit_exceeded: maximum container count reached (" +
                   juce::String(limits.maxContainers) + ")";
        return false;
    }

    juce::File manifestFile = containerDir.getChildFile("manifest.json");
    if (manifestFile.existsAsFile() && manifestFile.getSize() > limits.maxManifestBytes)
    {
        outError = "resource_limit_exceeded: manifest.json size (" +
                   juce::String(manifestFile.getSize()) + " bytes) exceeds limit (" +
                   juce::String(limits.maxManifestBytes) + " bytes)";
        return false;
    }

    juce::Array<juce::File> jsonFiles;
    containerDir.findChildFiles(jsonFiles, juce::File::findFiles, true, "*.json");
    for (const auto& jf : jsonFiles)
    {
        if (jf.getSize() > limits.maxJsonBytes)
        {
            outError = "resource_limit_exceeded: JSON file " + jf.getFileName() + " (" +
                       juce::String(jf.getSize()) + " bytes) exceeds limit (" +
                       juce::String(limits.maxJsonBytes) + " bytes)";
            return false;
        }
    }

    juce::Array<juce::File> audioFiles;
    containerDir.findChildFiles(audioFiles, juce::File::findFiles, true, "*.wav");
    for (const auto& af : audioFiles)
    {
        if (af.getSize() > limits.maxAudioBytes)
        {
            outError = "resource_limit_exceeded: audio file " + af.getFileName() + " (" +
                       juce::String(af.getSize()) + " bytes) exceeds limit (" +
                       juce::String(limits.maxAudioBytes) + " bytes)";
            return false;
        }
    }

    return true;
}

/**
 * @brief Background job for loading and verifying FAIR measurement containers with cooperative checks.
 */
class ContainerLoadJob : public juce::ThreadPoolJob
{
public:
    ContainerLoadJob(std::weak_ptr<SharedSessionState> weakState,
                     uint64_t generation,
                     juce::File containerDir,
                     int assignedId,
                     std::function<void(int, ContainerLoadState)> onComplete)
        : juce::ThreadPoolJob("ContainerLoadJob_" + juce::String(assignedId)),
          weakState_(std::move(weakState)),
          generation_(generation),
          containerDir_(std::move(containerDir)),
          assignedId_(assignedId),
          onComplete_(std::move(onComplete))
    {
    }

    JobStatus runJob() override
    {
        auto isCancelled = [this]() -> bool
        {
            if (shouldExit())
                return true;
            auto state = weakState_.lock();
            if (!state)
                return true;
            if (state->cancelToken.load() || state->shutdownState.load() != SessionShutdownState::Running)
                return true;
            return generation_ != state->sessionGeneration.load();
        };

        if (isCancelled())
            return jobHasFinished;

        auto state = weakState_.lock();
        if (!state)
            return jobHasFinished;

        SessionResourceLimits limits;
        int currentCount = 0;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            limits = state->resourceLimits;
            currentCount = static_cast<int>(state->containers.size());
        }

        // 1. Preventive resource check prior to heavy disk or JSON operations
        juce::String resourceDiag;
        if (!checkResourceLimitsPreLoad(containerDir_, limits, currentCount, resourceDiag))
        {
            publishResult(ContainerLoadState::Rejected, resourceDiag, nullptr);
            return jobHasFinished;
        }

        if (isCancelled())
            return jobHasFinished;

        // 2. Load model from container
        MeasurementViewModel vm;
        juce::String loadErr;
        const bool loadOk = MeasurementViewModelLoader::loadFromContainer(containerDir_, vm, loadErr);

        if (isCancelled())
            return jobHasFinished;

        ContainerLoadState finalState = ContainerLoadState::Failed;
        juce::String diagnostic = loadErr;

        if (loadOk)
        {
            // Evaluate curve points quota
            const int pointCount = static_cast<int>(vm.curve.x.size());
            if (pointCount > limits.maxCurvePoints)
            {
                finalState = ContainerLoadState::Rejected;
                diagnostic = "resource_limit_exceeded: curve points (" + juce::String(pointCount) +
                             ") exceeds maxCurvePoints (" + juce::String(limits.maxCurvePoints) + ")";
                publishResult(finalState, diagnostic, nullptr);
                return jobHasFinished;
            }

            if (isCancelled())
                return jobHasFinished;

            // 3. Cryptographic integrity verification against manifest.json
            juce::String integrityDiag;
            const bool integrityOk = MeasurementViewModelLoader::verifyContainerIntegrity(containerDir_, integrityDiag);

            if (isCancelled())
                return jobHasFinished;

            if (integrityOk)
            {
                finalState = ContainerLoadState::Verified;
                diagnostic = "All artifacts verified against manifest.";
            }
            else
            {
                finalState = ContainerLoadState::Corrupt;
                diagnostic = integrityDiag.isNotEmpty() ? integrityDiag : "Integrity check failed.";
            }
        }
        else
        {
            if (vm.integrityStatus == UiIntegrityStatus::Corrupt)
                finalState = ContainerLoadState::Corrupt;
            else
                finalState = ContainerLoadState::Failed;
        }

        if (isCancelled())
            return jobHasFinished;

        auto vmPtr = std::make_shared<const MeasurementViewModel>(std::move(vm));
        publishResult(finalState, diagnostic, vmPtr);
        return jobHasFinished;
    }

private:
    void publishResult(ContainerLoadState finalState,
                       const juce::String& diagnostic,
                       std::shared_ptr<const MeasurementViewModel> vmPtr)
    {
        auto weakState = weakState_;
        const auto gen = generation_;
        const auto assignedId = assignedId_;
        auto onComplete = onComplete_;

        juce::MessageManager::callAsync([weakState, gen, assignedId, finalState, diagnostic, vmPtr, onComplete]()
        {
            auto state = weakState.lock();
            if (!state)
                return;

            // Strict triple-condition check to avoid stale callbacks
            if (state->cancelToken.load() ||
                state->shutdownState.load() != SessionShutdownState::Running ||
                state->sessionGeneration.load() != gen)
            {
                return;
            }

            bool wasActivated = false;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                for (auto& c : state->containers)
                {
                    if (c.id == assignedId)
                    {
                        c.loadState = finalState;
                        c.diagnosticReason = diagnostic;
                        c.viewModel = vmPtr;
                        if (state->activeAudioContainerId == -1 && c.isPlayable())
                        {
                            state->activeAudioContainerId = assignedId;
                            wasActivated = true;
                        }
                        break;
                    }
                }
            }

            if (onComplete != nullptr)
                onComplete(assignedId, finalState);
        });
    }

    std::weak_ptr<SharedSessionState> weakState_;
    uint64_t generation_;
    juce::File containerDir_;
    int assignedId_;
    std::function<void(int, ContainerLoadState)> onComplete_;
};

} // namespace

MeasurementComparisonSession::MeasurementComparisonSession()
    : sharedState_(std::make_shared<SharedSessionState>()),
      threadPool_(std::make_shared<juce::ThreadPool>(4))
{
}

MeasurementComparisonSession::~MeasurementComparisonSession()
{
    // Coordinated lifecycle teardown avoiding use-after-free
    if (sharedState_ != nullptr)
    {
        sharedState_->shutdownState.store(SessionShutdownState::CancellationRequested);
        sharedState_->cancelToken.store(true);
        sharedState_->sessionGeneration.fetch_add(1);

        sharedState_->shutdownState.store(SessionShutdownState::Draining);
        notifyShutdownStateChanged(SessionShutdownState::Draining);
    }

    if (threadPool_ != nullptr)
    {
        const bool drained = threadPool_->removeAllJobs(true, 3000);
        if (sharedState_ != nullptr)
        {
            if (drained)
            {
                sharedState_->shutdownState.store(SessionShutdownState::Drained);
                notifyShutdownStateChanged(SessionShutdownState::Drained);
            }
            else
            {
                sharedState_->shutdownState.store(SessionShutdownState::DrainTimedOut);
                notifyShutdownStateChanged(SessionShutdownState::DrainTimedOut);
            }
            sharedState_->shutdownState.store(SessionShutdownState::Destroyed);
            notifyShutdownStateChanged(SessionShutdownState::Destroyed);
        }
    }
}

void MeasurementComparisonSession::addListener(Listener* listener)
{
    listeners_.add(listener);
}

void MeasurementComparisonSession::removeListener(Listener* listener)
{
    listeners_.remove(listener);
}

void MeasurementComparisonSession::assignVisualStyling(LoadedContainerEntry& entry, int index)
{
    const size_t colourCount = kAccessibleColours.size();
    entry.traceColour = kAccessibleColours[static_cast<size_t>(index) % colourCount];
    entry.dashPatternIndex = index % 4;
    entry.markerShapeIndex = index % 4;
}

int MeasurementComparisonSession::addContainerSync(const juce::File& containerDir)
{
    LoadedContainerEntry entry;
    int assignedId = 0;
    SessionResourceLimits limits;
    int currentCount = 0;

    {
        std::lock_guard<std::mutex> lock(sharedState_->mutex);
        limits = sharedState_->resourceLimits;
        currentCount = static_cast<int>(sharedState_->containers.size());

        assignedId = sharedState_->nextContainerId++;
        entry.id = assignedId;
        entry.containerDir = containerDir;
        entry.loadState = ContainerLoadState::Loading;
        assignVisualStyling(entry, currentCount);
        sharedState_->containers.push_back(entry);
    }

    notifyContainerListChanged();

    // 1. Preventive resource check
    juce::String resourceDiag;
    if (!checkResourceLimitsPreLoad(containerDir, limits, currentCount, resourceDiag))
    {
        {
            std::lock_guard<std::mutex> lock(sharedState_->mutex);
            for (auto& c : sharedState_->containers)
            {
                if (c.id == assignedId)
                {
                    c.loadState = ContainerLoadState::Rejected;
                    c.diagnosticReason = resourceDiag;
                    break;
                }
            }
        }
        notifyContainerStateChanged(assignedId, ContainerLoadState::Rejected);
        return assignedId;
    }

    MeasurementViewModel vm;
    juce::String loadErr;
    const bool loadOk = MeasurementViewModelLoader::loadFromContainer(containerDir, vm, loadErr);

    ContainerLoadState finalState = ContainerLoadState::Failed;
    juce::String diagnostic = loadErr;

    if (loadOk)
    {
        const int pointCount = static_cast<int>(vm.curve.x.size());
        if (pointCount > limits.maxCurvePoints)
        {
            finalState = ContainerLoadState::Rejected;
            diagnostic = "resource_limit_exceeded: curve points (" + juce::String(pointCount) +
                         ") exceeds maxCurvePoints (" + juce::String(limits.maxCurvePoints) + ")";
        }
        else
        {
            juce::String integrityDiag;
            const bool integrityOk = MeasurementViewModelLoader::verifyContainerIntegrity(containerDir, integrityDiag);
            if (integrityOk)
            {
                finalState = ContainerLoadState::Verified;
                diagnostic = "All artifacts verified against manifest.";
            }
            else
            {
                finalState = ContainerLoadState::Corrupt;
                diagnostic = integrityDiag.isNotEmpty() ? integrityDiag : "Integrity check failed.";
            }
        }
    }
    else
    {
        if (vm.integrityStatus == UiIntegrityStatus::Corrupt)
            finalState = ContainerLoadState::Corrupt;
        else
            finalState = ContainerLoadState::Failed;
    }

    bool wasActivated = false;
    {
        std::lock_guard<std::mutex> lock(sharedState_->mutex);
        for (auto& c : sharedState_->containers)
        {
            if (c.id == assignedId)
            {
                c.loadState = finalState;
                c.diagnosticReason = diagnostic;
                c.viewModel = std::make_shared<const MeasurementViewModel>(std::move(vm));
                if (sharedState_->activeAudioContainerId == -1 && c.isPlayable())
                {
                    sharedState_->activeAudioContainerId = assignedId;
                    wasActivated = true;
                }
                break;
            }
        }
    }

    notifyContainerStateChanged(assignedId, finalState);
    if (wasActivated)
        notifyActiveAudioSourceChanged(assignedId);

    return assignedId;
}

int MeasurementComparisonSession::addContainerAsync(const juce::File& containerDir,
                                                    std::function<void(int, ContainerLoadState)> onComplete)
{
    LoadedContainerEntry entry;
    int assignedId = 0;
    uint64_t generation = 0;

    {
        std::lock_guard<std::mutex> lock(sharedState_->mutex);
        generation = sharedState_->sessionGeneration.load();
        assignedId = sharedState_->nextContainerId++;
        entry.id = assignedId;
        entry.containerDir = containerDir;
        entry.loadState = ContainerLoadState::Loading;
        assignVisualStyling(entry, static_cast<int>(sharedState_->containers.size()));
        sharedState_->containers.push_back(entry);
    }

    notifyContainerListChanged();

    auto job = std::make_unique<ContainerLoadJob>(
        sharedState_,
        generation,
        containerDir,
        assignedId,
        [this, onComplete](int id, ContainerLoadState state)
        {
            notifyContainerStateChanged(id, state);
            if (onComplete != nullptr)
                onComplete(id, state);
        });

    threadPool_->addJob(job.release(), true);
    return assignedId;
}

void MeasurementComparisonSession::cancelPendingLoads()
{
    sharedState_->cancelToken.store(true);
    sharedState_->sessionGeneration.fetch_add(1);

    if (threadPool_ != nullptr)
        threadPool_->removeAllJobs(false, 1000);

    {
        std::lock_guard<std::mutex> lock(sharedState_->mutex);
        for (auto& c : sharedState_->containers)
        {
            if (c.loadState == ContainerLoadState::Loading || c.loadState == ContainerLoadState::Pending)
            {
                c.loadState = ContainerLoadState::Failed;
                c.diagnosticReason = "Load operation cancelled by user.";
            }
        }
    }

    sharedState_->cancelToken.store(false);
    notifyContainerListChanged();
}

bool MeasurementComparisonSession::removeContainer(int containerId)
{
    bool removed = false;
    bool activeAudioChanged = false;

    {
        std::lock_guard<std::mutex> lock(sharedState_->mutex);
        auto it = std::remove_if(sharedState_->containers.begin(), sharedState_->containers.end(),
                                 [containerId](const LoadedContainerEntry& e) { return e.id == containerId; });
        if (it != sharedState_->containers.end())
        {
            sharedState_->containers.erase(it, sharedState_->containers.end());
            removed = true;

            for (size_t i = 0; i < sharedState_->containers.size(); ++i)
                assignVisualStyling(sharedState_->containers[i], static_cast<int>(i));

            if (sharedState_->activeAudioContainerId == containerId)
            {
                sharedState_->activeAudioContainerId = -1;
                for (const auto& c : sharedState_->containers)
                {
                    if (c.isPlayable())
                    {
                        sharedState_->activeAudioContainerId = c.id;
                        break;
                    }
                }
                activeAudioChanged = true;
            }
        }
    }

    if (removed)
    {
        notifyContainerListChanged();
        if (activeAudioChanged)
            notifyActiveAudioSourceChanged(sharedState_->activeAudioContainerId);
    }

    return removed;
}

void MeasurementComparisonSession::clear()
{
    sharedState_->cancelToken.store(true);
    sharedState_->sessionGeneration.fetch_add(1);

    if (threadPool_ != nullptr)
        threadPool_->removeAllJobs(false, 1000);

    {
        std::lock_guard<std::mutex> lock(sharedState_->mutex);
        sharedState_->containers.clear();
        sharedState_->exclusions.clear();
        sharedState_->activeAudioContainerId = -1;
        sharedState_->nextContainerId = 1;
    }

    sharedState_->cancelToken.store(false);
    notifyContainerListChanged();
    notifyActiveAudioSourceChanged(-1);
}

int MeasurementComparisonSession::getContainerCount() const
{
    std::lock_guard<std::mutex> lock(sharedState_->mutex);
    return static_cast<int>(sharedState_->containers.size());
}

std::optional<LoadedContainerEntry> MeasurementComparisonSession::getContainerById(int containerId) const
{
    std::lock_guard<std::mutex> lock(sharedState_->mutex);
    for (const auto& c : sharedState_->containers)
    {
        if (c.id == containerId)
            return c;
    }
    return std::nullopt;
}

void MeasurementComparisonSession::setContainerSelectedForComparison(int containerId, bool selected)
{
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(sharedState_->mutex);
        for (auto& c : sharedState_->containers)
        {
            if (c.id == containerId)
            {
                if (c.selectedForComparison != selected)
                {
                    c.selectedForComparison = selected;
                    changed = true;
                }
                break;
            }
        }
    }

    if (changed)
        notifyContainerListChanged();
}

void MeasurementComparisonSession::setDomainFilter(std::optional<abdaudiolab::measurement::MeasurementExecutionDomain> filter)
{
    {
        std::lock_guard<std::mutex> lock(sharedState_->mutex);
        sharedState_->domainFilter = filter;
    }
    notifyDomainFilterChanged();
}

std::optional<abdaudiolab::measurement::MeasurementExecutionDomain> MeasurementComparisonSession::getDomainFilter() const
{
    std::lock_guard<std::mutex> lock(sharedState_->mutex);
    return sharedState_->domainFilter;
}

int MeasurementComparisonSession::getActiveAudioContainerId() const
{
    std::lock_guard<std::mutex> lock(sharedState_->mutex);
    return sharedState_->activeAudioContainerId;
}

bool MeasurementComparisonSession::setActiveAudioContainerId(int containerId)
{
    bool updated = false;
    {
        std::lock_guard<std::mutex> lock(sharedState_->mutex);
        if (containerId == -1)
        {
            sharedState_->activeAudioContainerId = -1;
            updated = true;
        }
        else
        {
            for (const auto& c : sharedState_->containers)
            {
                if (c.id == containerId && c.isPlayable())
                {
                    sharedState_->activeAudioContainerId = containerId;
                    updated = true;
                    break;
                }
            }
        }
    }

    if (updated)
        notifyActiveAudioSourceChanged(containerId);

    return updated;
}

std::vector<LoadedContainerEntry> MeasurementComparisonSession::getFilteredContainers() const
{
    std::lock_guard<std::mutex> lock(sharedState_->mutex);
    if (!sharedState_->domainFilter.has_value())
        return sharedState_->containers;

    std::vector<LoadedContainerEntry> result;
    for (const auto& c : sharedState_->containers)
    {
        if (c.viewModel != nullptr)
        {
            if (c.viewModel->executionDomain == *sharedState_->domainFilter)
                result.push_back(c);
        }
        else
        {
            result.push_back(c);
        }
    }
    return result;
}

std::vector<LoadedContainerEntry> MeasurementComparisonSession::getEligibleComparisonContainers() const
{
    std::lock_guard<std::mutex> lock(sharedState_->mutex);
    std::vector<LoadedContainerEntry> result;
    for (const auto& c : sharedState_->containers)
    {
        if (!c.isEligibleForComparison())
            continue;

        if (sharedState_->domainFilter.has_value() && c.viewModel != nullptr)
        {
            if (c.viewModel->executionDomain != *sharedState_->domainFilter)
                continue;
        }

        result.push_back(c);
    }
    return result;
}

bool MeasurementComparisonSession::areMeasurementBasesCompatible(const MeasurementViewModel& a,
                                                                 const MeasurementViewModel& b,
                                                                 juce::String& outReason)
{
    // 1. Metric meaning and units
    if (a.curve.yUnit != b.curve.yUnit)
    {
        outReason = "Incompatible Y-axis units: '" + a.curve.yUnit + "' vs '" + b.curve.yUnit + "'";
        return false;
    }

    // 2. Disallow mixing Peak with RMS
    const bool aIsRms = a.curve.yName.containsIgnoreCase("RMS") || a.measurementType.containsIgnoreCase("RMS");
    const bool bIsRms = b.curve.yName.containsIgnoreCase("RMS") || b.measurementType.containsIgnoreCase("RMS");
    if (aIsRms != bIsRms)
    {
        outReason = "Cannot overlay Peak level with RMS power metric.";
        return false;
    }

    // 3. Disallow mixing Centroid with Rolloff
    const bool aIsRolloff = a.curve.yName.containsIgnoreCase("Rolloff") || a.measurementType.containsIgnoreCase("Rolloff");
    const bool bIsRolloff = b.curve.yName.containsIgnoreCase("Rolloff") || b.measurementType.containsIgnoreCase("Rolloff");
    if (aIsRolloff != bIsRolloff)
    {
        outReason = "Cannot overlay Spectral Centroid with Spectral Rolloff metric.";
        return false;
    }

    // 4. Domain consistency: disallow digital vs analog roundtrip overlay without compensation
    if (a.executionDomain == abdaudiolab::measurement::MeasurementExecutionDomain::Vst3OfflineDigital &&
        b.executionDomain == abdaudiolab::measurement::MeasurementExecutionDomain::CombinedDutAndChain)
    {
        outReason = "Cannot overlay pure Vst3OfflineDigital with uncompensated CombinedDutAndChain.";
        return false;
    }

    // 5. Sample rate compatibility
    if (std::abs(a.sampleRateHz - b.sampleRateHz) > 1.0)
    {
        outReason = "Incompatible sample rates: " + juce::String(a.sampleRateHz) + " Hz vs " + juce::String(b.sampleRateHz) + " Hz";
        return false;
    }

    outReason = "Compatible measurement basis";
    return true;
}

PairwiseComparisonResult MeasurementComparisonSession::compareContainers(int containerIdA, int containerIdB) const
{
    PairwiseComparisonResult res;
    res.containerIdA = containerIdA;
    res.containerIdB = containerIdB;

    auto optA = getContainerById(containerIdA);
    auto optB = getContainerById(containerIdB);

    if (!optA.has_value() || !optB.has_value() ||
        optA->loadState != ContainerLoadState::Verified || optB->loadState != ContainerLoadState::Verified ||
        optA->viewModel == nullptr || optB->viewModel == nullptr)
    {
        res.equivalence = PairwiseStateEquivalence::NotComparable;
        res.reason = "One or both containers are not in Verified state.";
        return res;
    }

    const auto& vmA = *optA->viewModel;
    const auto& vmB = *optB->viewModel;

    res.containerLabelA = vmA.dutName.isNotEmpty() ? vmA.dutName : optA->containerDir.getFileName();
    res.containerLabelB = vmB.dutName.isNotEmpty() ? vmB.dutName : optB->containerDir.getFileName();

    if (vmA.pluginIdentity.has_value() && vmB.pluginIdentity.has_value())
    {
        res.pluginBinarySha256A = vmA.pluginIdentity->binarySha256;
        res.pluginBinarySha256B = vmB.pluginIdentity->binarySha256;

        if (vmA.pluginIdentity->binarySha256 != vmB.pluginIdentity->binarySha256 ||
            vmA.pluginIdentity->uid != vmB.pluginIdentity->uid)
        {
            res.equivalence = PairwiseStateEquivalence::NotComparable;
            res.reason = "Different plugin binaries or component CIDs.";
            return res;
        }
    }

    res.stateSha256A = vmA.expectedAudioSha256;
    res.stateSha256B = vmB.expectedAudioSha256;

    // 1. Level Dimension Comparison
    if (vmA.curve.y.size() == vmB.curve.y.size() && !vmA.curve.y.empty())
    {
        double maxDelta = 0.0;
        for (size_t i = 0; i < vmA.curve.y.size(); ++i)
        {
            const double delta = std::abs(vmA.curve.y[i] - vmB.curve.y[i]);
            maxDelta = std::max(maxDelta, delta);
        }
        res.maxAudioDelta = maxDelta;

        if (maxDelta == 0.0)
            res.levelEquivalence = PairwiseStateEquivalence::BitExact;
        else if (maxDelta <= 1e-4)
            res.levelEquivalence = PairwiseStateEquivalence::SemanticallyEquivalent;
        else
            res.levelEquivalence = PairwiseStateEquivalence::NotEquivalent;
    }
    else
    {
        res.levelEquivalence = PairwiseStateEquivalence::NotComparable;
    }

    // 2. Timbre Dimension Comparison (Spectral Centroid Hz)
    bool timbreBasisCompatible = false;
    if (vmA.dynamicsResult.has_value() && vmB.dynamicsResult.has_value() &&
        !vmA.dynamicsResult->brightnessCurve.y.empty() &&
        vmA.dynamicsResult->brightnessCurve.y.size() == vmB.dynamicsResult->brightnessCurve.y.size())
    {
        const auto& dynA = *vmA.dynamicsResult;
        const auto& dynB = *vmB.dynamicsResult;

        // Check sample rate match
        const bool srMatch = (std::abs(vmA.sampleRateHz - vmB.sampleRateHz) < 1.0);

        // Check window & validity across discrete points
        bool anyUnreliableOrSilent = false;
        bool windowMismatch = false;

        const size_t numPoints = std::min(dynA.points.size(), dynB.points.size());
        for (size_t p = 0; p < numPoints; ++p)
        {
            if (dynA.points[p].status == "silent" || dynA.points[p].status == "unreliable" ||
                dynB.points[p].status == "silent" || dynB.points[p].status == "unreliable")
            {
                anyUnreliableOrSilent = true;
                break;
            }

            if (std::abs(dynA.points[p].measurementWindowStartMs - dynB.points[p].measurementWindowStartMs) > 1.0 ||
                std::abs(dynA.points[p].measurementWindowEndMs - dynB.points[p].measurementWindowEndMs) > 1.0)
            {
                windowMismatch = true;
                break;
            }
        }

        if (srMatch && !anyUnreliableOrSilent && !windowMismatch)
        {
            timbreBasisCompatible = true;
            res.timbreMetric = "spectralCentroidHz";
            res.sampleRateHz = vmA.sampleRateHz;

            if (dynA.spectralMetadata.has_value())
            {
                res.fftSize = dynA.spectralMetadata->fftSize;
                res.windowFunction = dynA.spectralMetadata->window.toStdString();
            }
            if (!dynA.points.empty())
            {
                res.windowStartMs = dynA.points.front().measurementWindowStartMs;
                res.windowEndMs = dynA.points.front().measurementWindowEndMs;
            }

            double maxTimbreDelta = 0.0;
            const auto& brA = dynA.brightnessCurve.y;
            const auto& brB = dynB.brightnessCurve.y;
            for (size_t i = 0; i < brA.size(); ++i)
            {
                const double tDelta = std::abs(brA[i] - brB[i]);
                maxTimbreDelta = std::max(maxTimbreDelta, tDelta);
            }
            res.maxTimbreDeltaHz = maxTimbreDelta;

            if (maxTimbreDelta == 0.0)
                res.timbreEquivalence = PairwiseStateEquivalence::BitExact;
            else if (maxTimbreDelta <= 1.0) // within 1 Hz tolerance
                res.timbreEquivalence = PairwiseStateEquivalence::SemanticallyEquivalent;
            else
                res.timbreEquivalence = PairwiseStateEquivalence::NotEquivalent;
        }
    }

    if (!timbreBasisCompatible)
    {
        res.timbreEquivalence = PairwiseStateEquivalence::NotComparable;
    }

    // 2b. Phase 20.11.4: Temporal Timbre Dimension C(t)
    bool temporalBasisCompatible = false;
    if (vmA.dynamicsResult.has_value() && vmB.dynamicsResult.has_value() &&
        !vmA.dynamicsResult->points.empty() &&
        vmA.dynamicsResult->points.size() == vmB.dynamicsResult->points.size())
    {
        const auto& ptsA = vmA.dynamicsResult->points;
        const auto& ptsB = vmB.dynamicsResult->points;

        const bool srMatch = (std::abs(vmA.sampleRateHz - vmB.sampleRateHz) < 1.0);
        bool stftMatch = true;
        if (vmA.dynamicsResult->spectralMetadata.has_value() && vmB.dynamicsResult->spectralMetadata.has_value())
        {
            if (vmA.dynamicsResult->spectralMetadata->fftSize != vmB.dynamicsResult->spectralMetadata->fftSize ||
                vmA.dynamicsResult->spectralMetadata->window != vmB.dynamicsResult->spectralMetadata->window)
            {
                stftMatch = false;
            }
        }

        bool anySilentOrInvalid = false;
        bool timeWindowMismatch = false;

        for (size_t i = 0; i < ptsA.size(); ++i)
        {
            if (ptsA[i].status == "silent" || ptsA[i].status == "unreliable" ||
                ptsB[i].status == "silent" || ptsB[i].status == "unreliable")
            {
                anySilentOrInvalid = true;
                break;
            }
            if (std::abs(ptsA[i].measurementWindowStartMs - ptsB[i].measurementWindowStartMs) > 1.0 ||
                std::abs(ptsA[i].measurementWindowEndMs - ptsB[i].measurementWindowEndMs) > 1.0)
            {
                timeWindowMismatch = true;
                break;
            }
        }

        if (srMatch && stftMatch && !anySilentOrInvalid && !timeWindowMismatch)
        {
            temporalBasisCompatible = true;
            double maxTempDelta = 0.0;
            for (size_t i = 0; i < ptsA.size(); ++i)
            {
                const double diff = std::abs(ptsA[i].spectralCentroidHz - ptsB[i].spectralCentroidHz);
                maxTempDelta = std::max(maxTempDelta, diff);
            }
            res.maxTemporalTimbreDeltaHz = maxTempDelta;

            if (maxTempDelta == 0.0)
                res.temporalTimbreEquivalence = PairwiseStateEquivalence::BitExact;
            else if (maxTempDelta <= 1.0)
                res.temporalTimbreEquivalence = PairwiseStateEquivalence::SemanticallyEquivalent;
            else
                res.temporalTimbreEquivalence = PairwiseStateEquivalence::NotEquivalent;
        }
    }

    if (!temporalBasisCompatible)
    {
        res.temporalTimbreEquivalence = PairwiseStateEquivalence::NotComparable;
    }

    // 3. Consolidated Equivalence & Descriptive Reason
    if (res.levelEquivalence == PairwiseStateEquivalence::NotComparable)
    {
        res.equivalence = PairwiseStateEquivalence::NotComparable;
        res.reason = "Different point counts or missing curve data.";
    }
    else if (res.levelEquivalence == PairwiseStateEquivalence::BitExact)
    {
        if (res.timbreEquivalence == PairwiseStateEquivalence::NotEquivalent)
        {
            res.equivalence = PairwiseStateEquivalence::NotEquivalent;
            res.reason = "Level is bit-exact, but timbre divergence observed (maxTimbreDelta = " +
                         juce::String(res.maxTimbreDeltaHz, 1) + " Hz).";
        }
        else if (res.timbreEquivalence == PairwiseStateEquivalence::SemanticallyEquivalent)
        {
            res.equivalence = PairwiseStateEquivalence::SemanticallyEquivalent;
            res.reason = "Level is bit-exact, timbre semantically equivalent (maxTimbreDelta = " +
                         juce::String(res.maxTimbreDeltaHz, 1) + " Hz).";
        }
        else
        {
            res.equivalence = PairwiseStateEquivalence::BitExact;
            res.reason = "Identical curve values across all sample points (maxDelta = 0.0).";
        }
    }
    else if (res.levelEquivalence == PairwiseStateEquivalence::SemanticallyEquivalent)
    {
        if (res.timbreEquivalence == PairwiseStateEquivalence::NotEquivalent)
        {
            res.equivalence = PairwiseStateEquivalence::NotEquivalent;
            res.reason = "Level semantically equivalent, but timbre divergence observed (maxTimbreDelta = " +
                         juce::String(res.maxTimbreDeltaHz, 1) + " Hz).";
        }
        else
        {
            res.equivalence = PairwiseStateEquivalence::SemanticallyEquivalent;
            res.reason = "Numerically equivalent within metrological tolerance (maxDelta <= 1e-4).";
        }
    }
    else // NotEquivalent
    {
        res.equivalence = PairwiseStateEquivalence::NotEquivalent;
        if (res.timbreEquivalence == PairwiseStateEquivalence::NotEquivalent)
            res.reason = "Both level and timbre divergence observed between instances.";
        else
            res.reason = "Acoustic level divergence observed between instances.";
    }

    return res;
}

SessionShutdownState MeasurementComparisonSession::getShutdownState() const noexcept
{
    return sharedState_->shutdownState.load();
}

uint64_t MeasurementComparisonSession::getSessionGeneration() const noexcept
{
    return sharedState_->sessionGeneration.load();
}

SessionResourceLimits MeasurementComparisonSession::getResourceLimits() const
{
    std::lock_guard<std::mutex> lock(sharedState_->mutex);
    return sharedState_->resourceLimits;
}

void MeasurementComparisonSession::setResourceLimits(const SessionResourceLimits& limits)
{
    std::lock_guard<std::mutex> lock(sharedState_->mutex);
    sharedState_->resourceLimits = limits;
}

std::vector<ComparisonExclusionRecord> MeasurementComparisonSession::getExclusionRecords() const
{
    std::lock_guard<std::mutex> lock(sharedState_->mutex);
    return sharedState_->exclusions;
}

void MeasurementComparisonSession::addExclusionRecord(const ComparisonExclusionRecord& rec)
{
    std::lock_guard<std::mutex> lock(sharedState_->mutex);
    sharedState_->exclusions.push_back(rec);
}

void MeasurementComparisonSession::notifyContainerStateChanged(int containerId, ContainerLoadState newState)
{
    listeners_.call(&Listener::containerStateChanged, containerId, newState);
}

void MeasurementComparisonSession::notifyContainerListChanged()
{
    listeners_.call(&Listener::containerListChanged);
}

void MeasurementComparisonSession::notifyActiveAudioSourceChanged(int activeContainerId)
{
    listeners_.call(&Listener::activeAudioSourceChanged, activeContainerId);
}

void MeasurementComparisonSession::notifyDomainFilterChanged()
{
    listeners_.call(&Listener::domainFilterChanged);
}

void MeasurementComparisonSession::notifyShutdownStateChanged(SessionShutdownState state)
{
    listeners_.call(&Listener::sessionShutdownStateChanged, state);
}

int MeasurementComparisonSession::addLoadedContainerDirectlyForTesting(const LoadedContainerEntry& entry)
{
    std::lock_guard<std::mutex> lock(sharedState_->mutex);
    auto e = entry;
    if (e.id <= 0)
        e.id = sharedState_->nextContainerId++;
    sharedState_->containers.push_back(e);
    return e.id;
}

} // namespace abdaudiolab::gui::measurement
