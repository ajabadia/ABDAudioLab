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
#include <nlohmann/json.hpp>
#include <vector>
#include <memory>
#include <optional>
#include <mutex>
#include <atomic>

namespace abdaudiolab::gui::measurement
{

/**
 * @brief Explicit lifecycle state for session shutdown and job draining.
 */
enum class SessionShutdownState
{
    Running,
    CancellationRequested,
    Draining,
    Drained,
    DrainTimedOut,
    Destroyed
};

[[nodiscard]] inline juce::String sessionShutdownStateToString(SessionShutdownState s) noexcept
{
    switch (s)
    {
        case SessionShutdownState::Running:               return "RUNNING";
        case SessionShutdownState::CancellationRequested: return "CANCELLATION_REQUESTED";
        case SessionShutdownState::Draining:              return "DRAINING";
        case SessionShutdownState::Drained:               return "DRAINED";
        case SessionShutdownState::DrainTimedOut:          return "DRAIN_TIMED_OUT";
        case SessionShutdownState::Destroyed:              return "DESTROYED";
    }
    return "UNKNOWN";
}

/**
 * @brief Configurable memory and resource quotas evaluated preventively before file loading.
 */
struct SessionResourceLimits
{
    int maxContainers { 64 };
    int64_t maxManifestBytes { 1024 * 1024 };        // 1 MB
    int64_t maxJsonBytes { 10 * 1024 * 1024 };        // 10 MB
    int maxCurvePoints { 100000 };
    int64_t maxAudioBytes { 100 * 1024 * 1024 };      // 100 MB
    int maxConcurrentLoads { 4 };

    [[nodiscard]] nlohmann::json toJson() const
    {
        return nlohmann::json{
            { "resourceLimits", {
                { "maxContainers", maxContainers },
                { "maxManifestBytes", maxManifestBytes },
                { "maxJsonBytes", maxJsonBytes },
                { "maxCurvePoints", maxCurvePoints },
                { "maxAudioBytes", maxAudioBytes },
                { "maxConcurrentLoads", maxConcurrentLoads }
            } }
        };
    }

    static SessionResourceLimits fromJson(const nlohmann::json& j)
    {
        SessionResourceLimits lim;
        if (j.contains("resourceLimits") && j["resourceLimits"].is_object())
        {
            const auto& r = j["resourceLimits"];
            lim.maxContainers = r.value("maxContainers", lim.maxContainers);
            lim.maxManifestBytes = r.value("maxManifestBytes", lim.maxManifestBytes);
            lim.maxJsonBytes = r.value("maxJsonBytes", lim.maxJsonBytes);
            lim.maxCurvePoints = r.value("maxCurvePoints", lim.maxCurvePoints);
            lim.maxAudioBytes = r.value("maxAudioBytes", lim.maxAudioBytes);
            lim.maxConcurrentLoads = r.value("maxConcurrentLoads", lim.maxConcurrentLoads);
        }
        return lim;
    }
};

/**
 * @brief Structured, reproducible record of measurement exclusion from comparison.
 */
struct ComparisonExclusionRecord
{
    std::string code;                /**< Stable machine-readable code (e.g. "metric_basis_mismatch") */
    std::string message;             /**< Clear human-readable diagnosis */
    std::string containerId;         /**< Identifier of excluded container */
    std::string metric;              /**< Concerned metric or property */
    std::string comparisonBasisHash; /**< Deterministic hash of decision inputs (timestamp excluded) */
    std::string rulesVersion { "exclusion-rules-1.0" };
    std::string timestampIso;        /**< Optional session event timestamp (not part of basis hash) */

    [[nodiscard]] static std::string computeBasisHash(const std::string& idOrLabelA,
                                                      const std::string& idOrLabelB,
                                                      const std::string& metricName,
                                                      const std::string& unit,
                                                      const std::string& domain,
                                                      const std::string& rulesVer = "exclusion-rules-1.0");

    [[nodiscard]] nlohmann::json toJson() const
    {
        return nlohmann::json{
            { "code", code },
            { "message", message },
            { "containerId", containerId },
            { "metric", metric },
            { "comparisonBasisHash", comparisonBasisHash },
            { "rulesVersion", rulesVersion },
            { "timestampIso", timestampIso }
        };
    }
};

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

    // Phase 20.11.3: Independent Level & Timbre Dimensions
    PairwiseStateEquivalence levelEquivalence { PairwiseStateEquivalence::NotComparable };
    PairwiseStateEquivalence timbreEquivalence { PairwiseStateEquivalence::NotComparable };
    double maxAudioDelta { 0.0 };
    double maxTimbreDeltaHz { 0.0 };

    // Strict Timbre Metrological Basis
    std::string timbreMetric { "spectralCentroidHz" };
    double windowStartMs { 0.0 };
    double windowEndMs { 0.0 };
    double sampleRateHz { 48000.0 };
    int fftSize { 2048 };
    std::string windowFunction { "Hann" };

    juce::String normalizationVersion;
};

/**
 * @brief Shared thread-safe state ensuring graceful shutdown without use-after-free.
 */
struct SharedSessionState
{
    std::mutex mutex;
    std::vector<LoadedContainerEntry> containers;
    SessionResourceLimits resourceLimits;
    std::vector<ComparisonExclusionRecord> exclusions;
    int nextContainerId { 1 };
    int activeAudioContainerId { -1 };
    std::optional<abdaudiolab::measurement::MeasurementExecutionDomain> domainFilter;
    std::atomic<uint64_t> sessionGeneration { 0 };
    std::atomic<bool> cancelToken { false };
    std::atomic<SessionShutdownState> shutdownState { SessionShutdownState::Running };
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
        virtual void sessionShutdownStateChanged(SessionShutdownState /*state*/) {}
    };

    MeasurementComparisonSession();
    ~MeasurementComparisonSession();

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

    int addContainerSync(const juce::File& containerDir);
    int addContainerAsync(const juce::File& containerDir,
                          std::function<void(int containerId, ContainerLoadState state)> onComplete = nullptr);

    void cancelPendingLoads();
    bool removeContainer(int containerId);
    void clear();

    [[nodiscard]] int getContainerCount() const;
    [[nodiscard]] std::optional<LoadedContainerEntry> getContainerById(int containerId) const;

    void setContainerSelectedForComparison(int containerId, bool selected);
    void setDomainFilter(std::optional<abdaudiolab::measurement::MeasurementExecutionDomain> filter);
    [[nodiscard]] std::optional<abdaudiolab::measurement::MeasurementExecutionDomain> getDomainFilter() const;

    [[nodiscard]] int getActiveAudioContainerId() const;
    bool setActiveAudioContainerId(int containerId);

    [[nodiscard]] std::vector<LoadedContainerEntry> getFilteredContainers() const;
    [[nodiscard]] std::vector<LoadedContainerEntry> getEligibleComparisonContainers() const;

    [[nodiscard]] PairwiseComparisonResult compareContainers(int containerIdA, int containerIdB) const;

    [[nodiscard]] static bool areMeasurementBasesCompatible(const MeasurementViewModel& a,
                                                            const MeasurementViewModel& b,
                                                            juce::String& outIncompatibilityReason);

    // Phase 20.11.2 Lifecycle & Resource Controls
    [[nodiscard]] SessionShutdownState getShutdownState() const noexcept;
    [[nodiscard]] uint64_t getSessionGeneration() const noexcept;

    [[nodiscard]] SessionResourceLimits getResourceLimits() const;
    void setResourceLimits(const SessionResourceLimits& limits);

    [[nodiscard]] std::vector<ComparisonExclusionRecord> getExclusionRecords() const;
    void addExclusionRecord(const ComparisonExclusionRecord& rec);

    int addLoadedContainerDirectlyForTesting(const LoadedContainerEntry& entry);

private:
    void notifyContainerStateChanged(int containerId, ContainerLoadState newState);
    void notifyContainerListChanged();
    void notifyActiveAudioSourceChanged(int activeContainerId);
    void notifyDomainFilterChanged();
    void notifyShutdownStateChanged(SessionShutdownState state);

    void assignVisualStyling(LoadedContainerEntry& entry, int index);

    std::shared_ptr<SharedSessionState> sharedState_;
    std::shared_ptr<juce::ThreadPool> threadPool_;
    juce::ListenerList<Listener> listeners_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeasurementComparisonSession)
};

} // namespace abdaudiolab::gui::measurement
