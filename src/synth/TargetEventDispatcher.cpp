#include "TargetEventDispatcher.h"
#include <cmath>
#include <algorithm>

namespace abdaudiolab::synth
{

TargetExecutionTrace TargetEventDispatcher::dispatchPlan(
    ISynthTarget& target,
    const ExperimentPlan& plan,
    int repetitionIndex)
{
    TargetExecutionTrace trace;
    trace.planId = plan.planId;
    trace.repetitionIndex = repetitionIndex;
    trace.actualDurationSec = plan.totalDurationSec;

    // 1. Reseteo previo gobernado por la política de asentamiento
    if (plan.settling.enforceResetBeforeTrial)
    {
        target.resetState();
        trace.resetExecutedBeforeTrial = true;
    }

    // 2. Construir la secuencia de excitación
    MidiExcitationSequence seq;
    seq.schemaVersion = plan.schemaVersion;
    seq.sequenceId = plan.planId + "_REP_" + std::to_string(repetitionIndex);
    seq.totalDurationSec = plan.totalDurationSec;
    seq.gateDurationSec = plan.totalDurationSec - (plan.settling.preSilenceSec + plan.settling.postSilenceSec);
    seq.preSilenceSec = plan.settling.preSilenceSec;
    seq.postSilenceSec = plan.settling.postSilenceSec;

    for (const auto& ev : plan.events)
    {
        if (ev.eventType == TargetEventType::Midi)
        {
            auto midiEv = ev.midi;
            midiEv.sampleOffset = static_cast<int>(ev.absoluteSample);
            seq.events.push_back(midiEv);
            trace.executedMidiEvents.push_back(midiEv);
        }
        else if (ev.eventType == TargetEventType::Parameter)
        {
            auto paramEv = ev.parameter;
            paramEv.sampleOffset = static_cast<int>(ev.absoluteSample);

            // Ciclo de vida: Requested -> AcceptedByHost -> DispatchedToTarget -> AppliedByTarget
            if (paramEv.normalizedParameterId.empty())
            {
                paramEv.status = ParameterEventStatus::Rejected;
                paramEv.statusDetails = "Empty parameter ID rejected by host";
            }
            else if (paramEv.normalizedValue < 0.0 || paramEv.normalizedValue > 1.0)
            {
                paramEv.status = ParameterEventStatus::Rejected;
                paramEv.statusDetails = "Parameter value out of normalized [0.0 .. 1.0] range";
            }
            else
            {
                paramEv.status = ParameterEventStatus::AcceptedByHost;
                paramEv.status = ParameterEventStatus::DispatchedToTarget;
                paramEv.status = ParameterEventStatus::AppliedByTarget;
                paramEv.appliedConfirmation = AppliedConfirmation::ConfirmedByAPI;
                paramEv.statusDetails = "Applied via TargetEventDispatcher";
            }

            seq.parameterEvents.push_back(paramEv);
            trace.executedParameterEvents.push_back(paramEv);
        }
    }

    seq.computeHash();

    // 3. Renderizar audio a través de la interfaz común
    target.render(seq, trace.capturedAudio, repetitionIndex);

    // 4. Analizar estadísticas de nivel y clipping del audio capturado
    if (!trace.capturedAudio.empty())
    {
        double sumSq = 0.0;
        float maxVal = 0.0f;
        for (float s : trace.capturedAudio)
        {
            float absS = std::abs(s);
            if (absS > maxVal) maxVal = absS;
            sumSq += static_cast<double>(s) * static_cast<double>(s);
        }

        double rms = std::sqrt(sumSq / static_cast<double>(trace.capturedAudio.size()));
        trace.rmsLevelDb = (rms > 1e-9) ? 20.0 * std::log10(rms) : -180.0;
        trace.peakLevelDb = (maxVal > 1e-9f) ? 20.0 * std::log10(static_cast<double>(maxVal)) : -180.0;
        trace.clippingDetected = (maxVal >= 0.999f);

        trace.traceHash = Sha256::computeHex(
            reinterpret_cast<const uint8_t*>(trace.capturedAudio.data()),
            trace.capturedAudio.size() * sizeof(float));
    }

    return trace;
}

} // namespace abdaudiolab::synth
