/**
 * @file MeasurementComparisonSession.h
 * @brief Session manager for loading, verifying, filtering, and comparing multiple FAIR/LNL containers.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include "MeasurementViewModel.h"
#include "MeasurementViewModelLoader.h"
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <vector>
#include <memory>
#include <optional>
#include <mutex>

namespace abdaudiolab::gui::measurement
{

/**
 * @brief Explicit load and integrity state for containers in a comparison session.
 */
enum class ContainerLoadState
{
    Pending,
    Loading,
    Verified,
    Corrupt,
    Rejected,
    Failed
};

[[nodiscard]] inline juce::String containerLoadStateToString(ContainerLoadState state) noexcept
{
    switch (state)
    {
        case ContainerLoadState::Pending:  return "PENDING";
        case ContainerLoadState::Loading:  return "LOADING";
        case ContainerLoadState::Verified: return "VERIFIED";
        case ContainerLoadState::Corrupt:  return "CORRUPT";
        case ContainerLoadState::Rejected: return "REJECTED";
        case ContainerLoadState::Failed:   return "FAILED";
    }
    return "UNKNOWN";
}

/**
 * @brief Representation of an entry in the comparison session.
 */
struct LoadedContainerEntry
{
    int id { 0 };
    juce::File containerDir;
    ContainerLoadState loadState { ContainerLoadState::Pending };
    juce::String diagnosticReason;

    // Immutable view model once verified
    std::shared_ptr<const MeasurementViewModel> viewModel;

    // UI state
    bool selectedForComparison { true };
    juce::Colour traceColour { juce::Colours::cyan };
    int dashPatternIndex { 0 };  // 0: Solid, 1: Dashed, 2: Dot-Dash, 3: Dotted
    int markerShapeIndex { 0 };  // 0: Circle, 1: Square, 2: Triangle, 3: Diamond

    [[nodiscard]] bool isEligibleForComparison() const noexcept
    {
        return loadState == ContainerLoadState::Verified &&
               selectedForComparison &&
               viewModel != nullptr;
    }

    [[nodiscard]] bool isPlayable() const noexcept
    {
        return loadState == ContainerLoadState::Verified &&
               viewModel != nullptr &&
               viewModel->isPlaybackAllowed();
    }
};

/**
 * @brief Pairwise equivalence classification between two verified containers.
 */
enum class PairwiseStateEquivalence
{
    BitExact,
    SemanticallyEquivalent,
    NotEquivalent,
    NotComparable
};

[[nodiscard]] inline juce::String pairwiseEquivalenceToString(PairwiseStateEquivalence eq) noexcept
{
    switch (eq)
    {
        case PairwiseStateEquivalence::BitExact:               return "BitExact";
        case PairwiseStateEquivalence::SemanticallyEquivalent: return "SemanticallyEquivalent";
        case PairwiseStateEquivalence::NotEquivalent:          return "NotEquivalent";
        case PairwiseStateEquivalence::NotComparable:          return "NotComparable";
    }
    return "Unknown";
}

/**
 * @brief Detailed evidence for a pairwise container comparison.
 */
struct PairwiseComparisonResult
{
    int containerIdA { -1 };
    int containerIdB { -1 };
    juce::String containerLabelA;
    juce::String containerLabelB;
    PairwiseStateEquivalence equivalence { PairwiseStateEquivalence::NotComparable };
    juce::String reason;

    // Provenance details
    juce::String pluginBinarySha256A;
    juce::String pluginBinarySha256B;
    juce::String stateSha256A;
    juce::String stateSha256B;
    juce::String stimulusSha256A;
    juce::String stimulusSha256B;
    double maxAudioDelta { 0.0 };
    juce::String normalizationVersion;
};

/**
 * @class MeasurementComparisonSession
 * @brief Thread-safe session coordinator for multi-container FAIR measurement comparison.
 */
class MeasurementComparisonSession
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void containerStateChanged(int /*containerId*/, ContainerLoadState /*newState*/) {}
        virtual void containerListChanged() {}
        virtual void activeAudioSourceChanged(int /*activeContainerId*/) {}
        virtual void domainFilterChanged() {}
    };

    MeasurementComparisonSession();
    ~MeasurementComparisonSession();

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

    /**
     * @brief Adds a container directory synchronously (suitable for unit tests or immediate inspection).
     */
    int addContainerSync(const juce::File& containerDir);

    /**
     * @brief Adds a container directory asynchronously in a background thread.
     * @param onComplete Callback invoked when verification finishes.
     * @return The unique ID assigned to the container entry.
     */
    int addContainerAsync(const juce::File& containerDir,
                          std::function<void(int containerId, ContainerLoadState state)> onComplete = nullptr);

    /**
     * @brief Cancels any ongoing background loading and marks pending items as Failed.
     */
    void cancelPendingLoads();

    /**
     * @brief Removes a container by ID.
     */
    bool removeContainer(int containerId);

    /**
     * @brief Clears all containers.
     */
    void clear();

    [[nodiscard]] int getContainerCount() const;
    [[nodiscard]] std::optional<LoadedContainerEntry> getContainerById(int containerId) const;

    void setContainerSelectedForComparison(int containerId, bool selected);
    void setDomainFilter(std::optional<abdaudiolab::measurement::MeasurementExecutionDomain> filter);
    [[nodiscard]] std::optional<abdaudiolab::measurement::MeasurementExecutionDomain> getDomainFilter() const;

    [[nodiscard]] int getActiveAudioContainerId() const;
    bool setActiveAudioContainerId(int containerId);

    /**
     * @brief Returns containers that match current domain filter and are not in uninitialized states.
     */
    [[nodiscard]] std::vector<LoadedContainerEntry> getFilteredContainers() const;

    /**
     * @brief Returns containers strictly eligible for comparison curves and export.
     */
    [[nodiscard]] std::vector<LoadedContainerEntry> getEligibleComparisonContainers() const;

    /**
     * @brief Computes pairwise state equivalence between two containers.
     */
    [[nodiscard]] PairwiseComparisonResult compareContainers(int containerIdA, int containerIdB) const;

    /**
     * @brief Evaluates whether two containers have compatible measurement basis for curve overlay.
     */
    [[nodiscard]] static bool areMeasurementBasesCompatible(const MeasurementViewModel& a,
                                                            const MeasurementViewModel& b,
                                                            juce::String& outIncompatibilityReason);

private:
    void notifyContainerStateChanged(int containerId, ContainerLoadState newState);
    void notifyContainerListChanged();
    void notifyActiveAudioSourceChanged(int activeContainerId);
    void notifyDomainFilterChanged();

    void assignVisualStyling(LoadedContainerEntry& entry, int index);

    mutable std::mutex sessionMutex_;
    std::vector<LoadedContainerEntry> containers_;
    int nextContainerId_ { 1 };
    int activeAudioContainerId_ { -1 };
    std::optional<abdaudiolab::measurement::MeasurementExecutionDomain> domainFilter_;

    juce::ListenerList<Listener> listeners_;
    std::unique_ptr<juce::ThreadPool> threadPool_;
    std::atomic<bool> isCancelling_ { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeasurementComparisonSession)
};

} // namespace abdaudiolab::gui::measurement
