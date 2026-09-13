#include "SoundIdGuidedWorkflowContainer.h"
#include "../SoundIdTheme.h"

namespace abdaudiolab::gui::soundid
{

SoundIdGuidedWorkflowContainer::SoundIdGuidedWorkflowContainer(session::ProfilingSessionController& controller)
    : controller_(controller),
      targetView_(controller),
      runView_(controller),
      resultsView_(controller)
{
    addAndMakeVisible(topHeaderStrip_);
    addChildComponent(targetView_);
    addChildComponent(runView_);
    addChildComponent(resultsView_);

    activeGeneration_ = controller_.getActiveGeneration();
    controller_.addListener(this);

    // Inicializar vistas con el snapshot actual
    auto initialSnap = controller_.getCurrentSnapshot();
    onSessionSnapshotUpdated(initialSnap);
}

SoundIdGuidedWorkflowContainer::~SoundIdGuidedWorkflowContainer()
{
    controller_.removeListener(this);
}

void SoundIdGuidedWorkflowContainer::onSessionSnapshotUpdated(const session::ProfilingSessionSnapshot& snapshot)
{
    // Protección estricta contra snapshots de generaciones anteriores o desordenados
    if (snapshot.controllerGeneration < activeGeneration_)
        return;

    if (snapshot.controllerGeneration == activeGeneration_ &&
        snapshot.monotonicSequence <= lastObservedSequence_)
        return;

    lastObservedSequence_ = snapshot.monotonicSequence;
    activeGeneration_ = snapshot.controllerGeneration;

    topHeaderStrip_.updateFromSnapshot(snapshot);
    targetView_.updateFromSnapshot(snapshot);
    runView_.updateFromSnapshot(snapshot);
    resultsView_.updateFromSnapshot(snapshot);

    showStageComponent(snapshot.workflowStage);
}

void SoundIdGuidedWorkflowContainer::onAlertRaised(const session::UiAlert& alert)
{
    juce::ignoreUnused(alert);
    // Las alertas activas se reflejan automáticamente en el snapshot y la cabecera
}

void SoundIdGuidedWorkflowContainer::onWorkflowStageChanged(session::ProfilingWorkflowStage newStage)
{
    showStageComponent(newStage);
}

void SoundIdGuidedWorkflowContainer::onSessionStatusChanged(session::ProfilingSessionStatus newStatus)
{
    juce::ignoreUnused(newStatus);
}

void SoundIdGuidedWorkflowContainer::showStageComponent(session::ProfilingWorkflowStage stage)
{
    currentStage_ = stage;

    targetView_.setVisible(stage == session::ProfilingWorkflowStage::TargetSelection);
    runView_.setVisible(stage == session::ProfilingWorkflowStage::ConfigureAndStart ||
                        stage == session::ProfilingWorkflowStage::ProfilingActive);
    resultsView_.setVisible(stage == session::ProfilingWorkflowStage::ReviewResults);

    resized();
}

void SoundIdGuidedWorkflowContainer::updateTelemetry(double sampleRate, int blockSize, double cpuPercent)
{
    topHeaderStrip_.setHardwareTelemetry(sampleRate, blockSize, cpuPercent);
}

void SoundIdGuidedWorkflowContainer::paint(juce::Graphics& g)
{
    g.fillAll(SoundIdTheme::bgLight);
}

void SoundIdGuidedWorkflowContainer::resized()
{
    auto area = getLocalBounds();

    // Banda superior persistente fija en 36px
    topHeaderStrip_.setBounds(area.removeFromTop(36));

    // El lienzo restante lo ocupa la vista activa de la etapa
    if (targetView_.isVisible())
        targetView_.setBounds(area);
    else if (runView_.isVisible())
        runView_.setBounds(area);
    else if (resultsView_.isVisible())
        resultsView_.setBounds(area);
}

} // namespace abdaudiolab::gui::soundid
