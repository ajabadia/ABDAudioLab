/**
 * @file MeasurementComparisonSession.cpp
 * @brief Implementation of MeasurementComparisonSession.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementComparisonSession.h"
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

MeasurementComparisonSession::MeasurementComparisonSession()
{
    threadPool_ = std::make_unique<juce::ThreadPool>(2);
}

MeasurementComparisonSession::~MeasurementComparisonSession()
{
    cancelPendingLoads();
    if (threadPool_ != nullptr)
        threadPool_->removeAllJobs(true, 4000);
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
    {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        assignedId = nextContainerId_++;
        entry.id = assignedId;
        entry.containerDir = containerDir;
        entry.loadState = ContainerLoadState::Loading;
        assignVisualStyling(entry, static_cast<int>(containers_.size()));
        containers_.push_back(entry);
    }

    notifyContainerListChanged();

    MeasurementViewModel vm;
    juce::String loadErr;
    const bool loadOk = MeasurementViewModelLoader::loadFromContainer(containerDir, vm, loadErr);

    ContainerLoadState finalState = ContainerLoadState::Failed;
    juce::String diagnostic = loadErr;

    if (loadOk)
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
    else
    {
        if (vm.integrityStatus == UiIntegrityStatus::Corrupt)
            finalState = ContainerLoadState::Corrupt;
        else
            finalState = ContainerLoadState::Failed;
    }

    {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        for (auto& c : containers_)
        {
            if (c.id == assignedId)
            {
                c.loadState = finalState;
                c.diagnosticReason = diagnostic;
                c.viewModel = std::make_shared<const MeasurementViewModel>(std::move(vm));
                if (activeAudioContainerId_ == -1 && c.isPlayable())
                    activeAudioContainerId_ = assignedId;
                break;
            }
        }
    }

    notifyContainerStateChanged(assignedId, finalState);
    return assignedId;
}

int MeasurementComparisonSession::addContainerAsync(const juce::File& containerDir,
                                                    std::function<void(int, ContainerLoadState)> onComplete)
{
    LoadedContainerEntry entry;
    int assignedId = 0;
    {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        assignedId = nextContainerId_++;
        entry.id = assignedId;
        entry.containerDir = containerDir;
        entry.loadState = ContainerLoadState::Loading;
        assignVisualStyling(entry, static_cast<int>(containers_.size()));
        containers_.push_back(entry);
    }

    notifyContainerListChanged();

    threadPool_->addJob([this, containerDir, assignedId, onComplete]()
    {
        if (isCancelling_.load())
            return;

        MeasurementViewModel vm;
        juce::String loadErr;
        const bool loadOk = MeasurementViewModelLoader::loadFromContainer(containerDir, vm, loadErr);

        ContainerLoadState finalState = ContainerLoadState::Failed;
        juce::String diagnostic = loadErr;

        if (loadOk)
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
        else
        {
            if (vm.integrityStatus == UiIntegrityStatus::Corrupt)
                finalState = ContainerLoadState::Corrupt;
            else
                finalState = ContainerLoadState::Failed;
        }

        auto vmPtr = std::make_shared<const MeasurementViewModel>(std::move(vm));

        juce::MessageManager::callAsync([this, assignedId, finalState, diagnostic, vmPtr, onComplete]()
        {
            bool wasActivated = false;
            {
                std::lock_guard<std::mutex> lock(sessionMutex_);
                for (auto& c : containers_)
                {
                    if (c.id == assignedId)
                    {
                        c.loadState = finalState;
                        c.diagnosticReason = diagnostic;
                        c.viewModel = vmPtr;
                        if (activeAudioContainerId_ == -1 && c.isPlayable())
                        {
                            activeAudioContainerId_ = assignedId;
                            wasActivated = true;
                        }
                        break;
                    }
                }
            }

            notifyContainerStateChanged(assignedId, finalState);
            if (wasActivated)
                notifyActiveAudioSourceChanged(assignedId);

            if (onComplete != nullptr)
                onComplete(assignedId, finalState);
        });
    });

    return assignedId;
}

void MeasurementComparisonSession::cancelPendingLoads()
{
    isCancelling_.store(true);
    if (threadPool_ != nullptr)
        threadPool_->removeAllJobs(false, 1000);

    {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        for (auto& c : containers_)
        {
            if (c.loadState == ContainerLoadState::Loading || c.loadState == ContainerLoadState::Pending)
            {
                c.loadState = ContainerLoadState::Failed;
                c.diagnosticReason = "Load operation cancelled by user.";
            }
        }
    }
    isCancelling_.store(false);
    notifyContainerListChanged();
}

bool MeasurementComparisonSession::removeContainer(int containerId)
{
    bool removed = false;
    bool activeAudioChanged = false;

    {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        auto it = std::remove_if(containers_.begin(), containers_.end(),
                                 [containerId](const LoadedContainerEntry& e) { return e.id == containerId; });
        if (it != containers_.end())
        {
            containers_.erase(it, containers_.end());
            removed = true;

            // Re-index visual styling
            for (size_t i = 0; i < containers_.size(); ++i)
                assignVisualStyling(containers_[i], static_cast<int>(i));

            if (activeAudioContainerId_ == containerId)
            {
                activeAudioContainerId_ = -1;
                for (const auto& c : containers_)
                {
                    if (c.isPlayable())
                    {
                        activeAudioContainerId_ = c.id;
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
            notifyActiveAudioSourceChanged(activeAudioContainerId_);
    }
    return removed;
}

void MeasurementComparisonSession::clear()
{
    cancelPendingLoads();
    {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        containers_.clear();
        activeAudioContainerId_ = -1;
    }
    notifyContainerListChanged();
    notifyActiveAudioSourceChanged(-1);
}

int MeasurementComparisonSession::getContainerCount() const
{
    std::lock_guard<std::mutex> lock(sessionMutex_);
    return static_cast<int>(containers_.size());
}

std::optional<LoadedContainerEntry> MeasurementComparisonSession::getContainerById(int containerId) const
{
    std::lock_guard<std::mutex> lock(sessionMutex_);
    for (const auto& c : containers_)
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
        std::lock_guard<std::mutex> lock(sessionMutex_);
        for (auto& c : containers_)
        {
            if (c.id == containerId && c.selectedForComparison != selected)
            {
                c.selectedForComparison = selected;
                changed = true;
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
        std::lock_guard<std::mutex> lock(sessionMutex_);
        if (domainFilter_ == filter)
            return;
        domainFilter_ = filter;
    }
    notifyDomainFilterChanged();
}

std::optional<abdaudiolab::measurement::MeasurementExecutionDomain> MeasurementComparisonSession::getDomainFilter() const
{
    std::lock_guard<std::mutex> lock(sessionMutex_);
    return domainFilter_;
}

int MeasurementComparisonSession::getActiveAudioContainerId() const
{
    std::lock_guard<std::mutex> lock(sessionMutex_);
    return activeAudioContainerId_;
}

bool MeasurementComparisonSession::setActiveAudioContainerId(int containerId)
{
    bool updated = false;
    {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        if (containerId == -1)
        {
            activeAudioContainerId_ = -1;
            updated = true;
        }
        else
        {
            for (const auto& c : containers_)
            {
                if (c.id == containerId)
                {
                    if (c.isPlayable())
                    {
                        activeAudioContainerId_ = containerId;
                        updated = true;
                    }
                    break;
                }
            }
        }
    }

    if (updated)
        notifyActiveAudioSourceChanged(activeAudioContainerId_);

    return updated;
}

std::vector<LoadedContainerEntry> MeasurementComparisonSession::getFilteredContainers() const
{
    std::lock_guard<std::mutex> lock(sessionMutex_);
    if (!domainFilter_.has_value())
        return containers_;

    std::vector<LoadedContainerEntry> res;
    for (const auto& c : containers_)
    {
        if (c.viewModel != nullptr && c.viewModel->executionDomain == *domainFilter_)
            res.push_back(c);
        else if (c.viewModel == nullptr)
            res.push_back(c); // Always show pending/loading for user feedback
    }
    return res;
}

std::vector<LoadedContainerEntry> MeasurementComparisonSession::getEligibleComparisonContainers() const
{
    std::lock_guard<std::mutex> lock(sessionMutex_);
    std::vector<LoadedContainerEntry> res;
    for (const auto& c : containers_)
    {
        if (c.isEligibleForComparison())
        {
            if (!domainFilter_.has_value() || c.viewModel->executionDomain == *domainFilter_)
                res.push_back(c);
        }
    }
    return res;
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

    // Check plugin identity and binary
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

    // Check audio delta and state fixity if available
    res.stateSha256A = vmA.expectedAudioSha256; // fallback indicator
    res.stateSha256B = vmB.expectedAudioSha256;

    // Evaluate curve delta
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
        {
            res.equivalence = PairwiseStateEquivalence::BitExact;
            res.reason = "Identical curve values across all sample points (maxDelta = 0.0).";
        }
        else if (maxDelta <= 1e-4)
        {
            res.equivalence = PairwiseStateEquivalence::SemanticallyEquivalent;
            res.reason = "Numerically equivalent within metrological tolerance (maxDelta <= 1e-4).";
        }
        else
        {
            res.equivalence = PairwiseStateEquivalence::NotEquivalent;
            res.reason = "Acoustic divergence observed between instances.";
        }
    }
    else
    {
        res.equivalence = PairwiseStateEquivalence::NotComparable;
        res.reason = "Different point counts or missing curve data.";
    }

    return res;
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

} // namespace abdaudiolab::gui::measurement
