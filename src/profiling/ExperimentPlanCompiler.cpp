#include "ExperimentPlanCompiler.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace abdaudiolab::profiling
{

synth::ExperimentPlan ExperimentPlanCompiler::compileToExperimentPlan(
    const MeasurementRecipe& recipe,
    const CompilationDefaults& defaults)
{
    synth::ExperimentPlan plan;
    plan.schemaVersion = "1.0.0";
    plan.recipeId = recipe.recipeId;
    plan.planId = "PLAN_" + recipe.recipeId;
    plan.recipeType = "MeasurementRecipe_" + assistanceLevelToString(recipe.assistanceLevel);
    plan.sampleRate = defaults.defaultSampleRate;

    plan.settling.preSilenceSec = defaults.preSilenceSec;
    plan.settling.postSilenceSec = defaults.postSilenceSec;
    plan.randomization.randomSeed = recipe.excitation.seed.value_or(defaults.randomSeed);
    plan.randomization.repetitionsPerCondition = std::max(1, recipe.excitation.repetitions);
    plan.randomization.isDeterministic = true;

    double currentTimeSec = defaults.preSilenceSec;
    const int repetitions = std::max(1, recipe.excitation.repetitions);

    // Si no hay puntos de medición declarados, creamos un punto neutro por defecto
    std::vector<MeasurementPointConfig> points = recipe.measurement.points;
    if (points.empty())
    {
        points.push_back(MeasurementPointConfig{ "default_stimulus", 0.0 });
    }

    // Si no hay notas declaradas, creamos una nota de referencia C4
    std::vector<NoteExcitationConfig> notes = recipe.excitation.notes;
    if (notes.empty())
    {
        notes.push_back(NoteExcitationConfig{ 60, 0.5, 250.0, 50.0 });
    }

    for (int rep = 0; rep < repetitions; ++rep)
    {
        for (const auto& pt : points)
        {
            for (const auto& n : notes)
            {
                // 1. Evento de Parámetro (fijar control antes del estímulo)
                synth::TargetEvent paramEv;
                paramEv.eventType = synth::TargetEventType::Parameter;
                paramEv.scheduledTimeMs = currentTimeSec * 1000.0;
                paramEv.absoluteSample = static_cast<int64_t>(std::lround(currentTimeSec * plan.sampleRate));
                paramEv.parameter.normalizedParameterId = pt.parameter;
                paramEv.parameter.nativeParameterId = pt.parameter;
                paramEv.parameter.normalizedValue = pt.normalizedValue;
                paramEv.parameter.scheduledTimeMs = paramEv.scheduledTimeMs;
                paramEv.parameter.absoluteSample = paramEv.absoluteSample;
                plan.events.push_back(paramEv);

                // Tiempo de asentamiento previo a la nota
                const double noteStartSec = currentTimeSec + (n.settlingMs / 1000.0);
                const int64_t noteOnSample = static_cast<int64_t>(std::lround(noteStartSec * plan.sampleRate));

                // 2. Evento MIDI NoteOn
                synth::TargetEvent noteOnEv;
                noteOnEv.eventType = synth::TargetEventType::Midi;
                noteOnEv.scheduledTimeMs = noteStartSec * 1000.0;
                noteOnEv.absoluteSample = noteOnSample;
                noteOnEv.midi.type = synth::TimedMidiType::NoteOn;
                noteOnEv.midi.channel = defaults.defaultMidiChannel;
                noteOnEv.midi.noteNumber = n.midiNote;
                noteOnEv.midi.velocity = static_cast<float>(n.velocity);
                noteOnEv.midi.scheduledTimeMs = noteOnEv.scheduledTimeMs;
                noteOnEv.midi.sampleOffset = static_cast<int>(noteOnSample);
                plan.events.push_back(noteOnEv);

                // 3. Evento MIDI NoteOff
                const double noteOffSec = noteStartSec + (n.gateMs / 1000.0);
                const int64_t noteOffSample = static_cast<int64_t>(std::lround(noteOffSec * plan.sampleRate));

                synth::TargetEvent noteOffEv;
                noteOffEv.eventType = synth::TargetEventType::Midi;
                noteOffEv.scheduledTimeMs = noteOffSec * 1000.0;
                noteOffEv.absoluteSample = noteOffSample;
                noteOffEv.midi.type = synth::TimedMidiType::NoteOff;
                noteOffEv.midi.channel = defaults.defaultMidiChannel;
                noteOffEv.midi.noteNumber = n.midiNote;
                noteOffEv.midi.velocity = 0.0f;
                noteOffEv.midi.scheduledTimeMs = noteOffEv.scheduledTimeMs;
                noteOffEv.midi.sampleOffset = static_cast<int>(noteOffSample);
                plan.events.push_back(noteOffEv);

                // 4. Ventana de observación acústica
                synth::ObservationWindow win;
                std::ostringstream oss;
                oss << recipe.recipeId << "_P_" << pt.parameter << "_"
                    << static_cast<int>(std::lround(pt.normalizedValue * 1000.0))
                    << "_N" << n.midiNote << "_R" << (rep + 1);
                win.windowId = oss.str();
                win.startTimeMs = noteStartSec * 1000.0;
                win.durationMs = n.gateMs + n.settlingMs;
                win.startSample = noteOnSample;
                win.endSample = static_cast<int64_t>(std::lround((noteOffSec + (n.settlingMs / 1000.0)) * plan.sampleRate));
                win.targetParameterId = pt.parameter;
                win.domain = "Filter";
                plan.windows.push_back(win);

                // Avanzar tiempo para el siguiente ensayo
                currentTimeSec = noteOffSec + (n.settlingMs / 1000.0) + defaults.postSilenceSec;
            }
        }
    }

    plan.totalDurationSec = currentTimeSec;

    // Ordenar eventos cronológicamente
    std::sort(plan.events.begin(), plan.events.end(), [](const synth::TargetEvent& a, const synth::TargetEvent& b) {
        return a.absoluteSample < b.absoluteSample;
    });

    // Calcular hash canónico del plan puro (Ajuste 1: agnóstico a hardware)
    plan.computeHash();

    return plan;
}

std::string ExperimentPlanCompiler::computeExperimentPlanHash(const synth::ExperimentPlan& plan)
{
    return plan.planHash;
}

std::string ExperimentPlanCompiler::computeResolvedExecutionPlanHash(
    const std::string& experimentPlanHash,
    const ExecutionEnvironment& env)
{
    std::string blob = experimentPlanHash + "\n"
                     + env.driver + "\n"
                     + std::to_string(env.sampleRate) + "\n"
                     + std::to_string(env.blockSize) + "\n"
                     + std::to_string(env.channels) + "\n"
                     + env.deviceName + "\n"
                     + env.pluginBinary + "\n"
                     + env.pluginVersion + "\n"
                     + std::to_string(env.latencySamples) + "\n";

    std::vector<std::string> sortedCaps = env.discoveredCapabilities;
    std::sort(sortedCaps.begin(), sortedCaps.end());
    for (const auto& cap : sortedCaps)
    {
        blob += "CAP:" + cap + "\n";
    }

    return synth::Sha256::computeHex(blob);
}

ResolveExecutionPlanResult ExperimentPlanCompiler::resolveExecutionPlan(
    const synth::ExperimentPlan& plan,
    const ExecutionEnvironment& env,
    const MeasurementRecipe* sourceRecipe)
{
    ResolveExecutionPlanResult result;

    if (sourceRecipe != nullptr)
    {
        // 1. Validar frecuencia de muestreo permitida
        const auto& allowedRates = sourceRecipe->targetConstraints.allowedSampleRatesHz;
        if (!allowedRates.empty())
        {
            const int currentRate = static_cast<int>(std::lround(env.sampleRate));
            if (std::find(allowedRates.begin(), allowedRates.end(), currentRate) == allowedRates.end())
            {
                ValidationDiagnostic diag;
                diag.severity = DiagnosticSeverity::Error;
                diag.code = "ERR_CAPABILITY_SAMPLE_RATE_UNSUPPORTED";
                diag.jsonPointer = "/environment/sampleRate";
                diag.message = "Frecuencia de muestreo no soportada por la receta: " + std::to_string(currentRate) + " Hz";
                result.diagnostics.push_back(diag);
            }
        }

        // 2. Validar canales mínimos
        if (sourceRecipe->targetConstraints.channels > 0 && env.channels < sourceRecipe->targetConstraints.channels)
        {
            ValidationDiagnostic diag;
            diag.severity = DiagnosticSeverity::Error;
            diag.code = "ERR_CAPABILITY_INSUFFICIENT_CHANNELS";
            diag.jsonPointer = "/environment/channels";
            diag.message = "Canales insuficientes en el entorno: se requieren "
                         + std::to_string(sourceRecipe->targetConstraints.channels)
                         + ", pero el entorno solo provee " + std::to_string(env.channels);
            result.diagnostics.push_back(diag);
        }

        // 3. Validar capacidades requeridas
        for (const auto& reqCap : sourceRecipe->targetConstraints.requiredCapabilities)
        {
            if (std::find(env.discoveredCapabilities.begin(), env.discoveredCapabilities.end(), reqCap) == env.discoveredCapabilities.end())
            {
                ValidationDiagnostic diag;
                diag.severity = DiagnosticSeverity::Error;
                diag.code = "ERR_CAPABILITY_MISSING";
                diag.jsonPointer = "/environment/discoveredCapabilities";
                diag.message = "Capacidad requerida no provista por el target/entorno: '" + reqCap + "'";
                result.diagnostics.push_back(diag);
            }
        }
    }

    if (!result.diagnostics.empty())
    {
        return result;
    }

    ResolvedExecutionPlan resolved;
    resolved.experimentPlan = plan;
    resolved.environment = env;

    // Si el sample rate del entorno difiere del plan lógico, re-escalar offsets de muestras
    if (env.sampleRate > 0.0 && plan.sampleRate > 0.0 && std::abs(env.sampleRate - plan.sampleRate) > 1e-3)
    {
        const double ratio = env.sampleRate / plan.sampleRate;
        for (auto& ev : resolved.experimentPlan.events)
        {
            ev.absoluteSample = static_cast<int64_t>(std::lround(ev.absoluteSample * ratio));
            ev.midi.sampleOffset = static_cast<int>(ev.absoluteSample);
            ev.parameter.absoluteSample = ev.absoluteSample;
        }
        for (auto& win : resolved.experimentPlan.windows)
        {
            win.startSample = static_cast<int64_t>(std::lround(win.startSample * ratio));
            win.endSample = static_cast<int64_t>(std::lround(win.endSample * ratio));
        }
        resolved.experimentPlan.sampleRate = env.sampleRate;
    }

    resolved.totalDurationSec = plan.totalDurationSec;
    resolved.totalSamples = static_cast<int64_t>(std::lround(plan.totalDurationSec * env.sampleRate));
    resolved.resolvedExecutionPlanHash = computeResolvedExecutionPlanHash(plan.planHash, env);

    result.resolvedPlan = resolved;
    return result;
}

ResolveExecutionPlanResult ExperimentPlanCompiler::resolveExecutionPlan(
    const MeasurementRecipe& recipe,
    const TargetProfile& profile,
    const ExecutionEnvironment& environment,
    const CompilationDefaults& defaults)
{
    ResolveExecutionPlanResult result;

    // 1. Validar que cada punto semántico de la receta esté mapeado en el TargetProfile
    for (size_t i = 0; i < recipe.measurement.points.size(); ++i)
    {
        const auto& pt = recipe.measurement.points[i];
        const auto* mapping = profile.findMappingForSemanticId(pt.getSemanticId());
        if (mapping == nullptr)
        {
            ValidationDiagnostic diag;
            diag.severity = DiagnosticSeverity::Error;
            diag.code = "ERR_TARGET_PROFILE_SEMANTIC_ID_UNMAPPED";
            diag.jsonPointer = "/measurement/points/" + std::to_string(i) + "/semanticId";
            diag.message = "El parametro semantico '" + pt.getSemanticId() + "' no esta mapeado en el TargetProfile '" + profile.targetProfileId + "'";
            result.diagnostics.push_back(diag);
        }
        else
        {
            auto transportKind = mapping->getTransportKind();
            const auto& transList = profile.capabilities.controlTransports;
            if (!transList.empty() && std::find(transList.begin(), transList.end(), transportKind) == transList.end())
            {
                ValidationDiagnostic diag;
                diag.severity = DiagnosticSeverity::Error;
                diag.code = "ERR_CAPABILITY_TRANSPORT_UNSUPPORTED";
                diag.jsonPointer = "/parameters/" + std::to_string(i) + "/technicalIdentifier";
                diag.message = "El transporte requerido para '" + pt.getSemanticId() + "' no esta declarado en capabilities del TargetProfile";
                result.diagnostics.push_back(diag);
            }
        }
    }

    // 2. Validar compatibilidad de sample rate del entorno frente al TargetProfile
    const int currentRate = static_cast<int>(std::lround(environment.sampleRate));
    const auto& supportedRates = profile.capabilities.sampleRatesHz;
    if (!supportedRates.empty() && std::find(supportedRates.begin(), supportedRates.end(), currentRate) == supportedRates.end())
    {
        ValidationDiagnostic diag;
        diag.severity = DiagnosticSeverity::Error;
        diag.code = "ERR_CAPABILITY_SAMPLE_RATE_UNSUPPORTED";
        diag.jsonPointer = "/capabilities/sampleRatesHz";
        diag.message = "Frecuencia de muestreo " + std::to_string(currentRate) + " Hz no soportada por el TargetProfile '" + profile.targetProfileId + "'";
        result.diagnostics.push_back(diag);
    }

    // 3. Validar compatibilidad de canales del entorno frente a audioOutput del TargetProfile
    const auto& supportedChannels = profile.capabilities.audioOutput.supportedChannelCounts;
    if (!supportedChannels.empty() && std::find(supportedChannels.begin(), supportedChannels.end(), environment.channels) == supportedChannels.end())
    {
        ValidationDiagnostic diag;
        diag.severity = DiagnosticSeverity::Error;
        diag.code = "ERR_CAPABILITY_CHANNELS_UNSUPPORTED";
        diag.jsonPointer = "/capabilities/audioOutput/supportedChannelCounts";
        diag.message = "La configuracion de " + std::to_string(environment.channels) + " canales no esta soportada por el TargetProfile '" + profile.targetProfileId + "'";
        result.diagnostics.push_back(diag);
    }

    // Validar compatibilidad de canal layout de la receta frente a supportedObservationLayouts
    if (recipe.targetConstraints.channels > 0)
    {
        std::string reqLayout = (recipe.targetConstraints.channels == 1) ? "mono" : "stereo";
        const auto& obsLayouts = profile.capabilities.audioOutput.supportedObservationLayouts;
        if (!obsLayouts.empty() && std::find(obsLayouts.begin(), obsLayouts.end(), reqLayout) == obsLayouts.end())
        {
            ValidationDiagnostic diag;
            diag.severity = DiagnosticSeverity::Error;
            diag.code = "ERR_CAPABILITY_CHANNELS_UNSUPPORTED";
            diag.jsonPointer = "/capabilities/audioOutput/supportedObservationLayouts";
            diag.message = "El layout de observacion '" + reqLayout + "' requerido por la receta no esta soportado por el TargetProfile";
            result.diagnostics.push_back(diag);
        }
    }

    if (!result.diagnostics.empty())
    {
        return result;
    }

    // Compilación determinista del ExperimentPlan
    synth::ExperimentPlan plan = compileToExperimentPlan(recipe, defaults);

    // Resolución física
    ResolvedExecutionPlan resolved;
    resolved.experimentPlan = plan;
    resolved.environment = environment;

    if (environment.sampleRate > 0.0 && std::abs(environment.sampleRate - plan.sampleRate) > 0.001)
    {
        const double ratio = environment.sampleRate / plan.sampleRate;
        for (auto& evt : resolved.experimentPlan.events)
        {
            evt.absoluteSample = static_cast<int64_t>(std::lround(evt.absoluteSample * ratio));
            evt.midi.sampleOffset = static_cast<int>(evt.absoluteSample);
            evt.parameter.absoluteSample = evt.absoluteSample;
        }
        for (auto& win : resolved.experimentPlan.windows)
        {
            win.startSample = static_cast<int64_t>(std::lround(win.startSample * ratio));
            win.endSample = static_cast<int64_t>(std::lround(win.endSample * ratio));
        }
        resolved.experimentPlan.sampleRate = environment.sampleRate;
    }

    // Enriquecer eventos de parámetro con identificador técnico nativo y exactitud de transporte
    for (auto& evt : resolved.experimentPlan.events)
    {
        if (evt.eventType == synth::TargetEventType::Parameter)
        {
            const auto* mapping = profile.findMappingForSemanticId(evt.parameter.normalizedParameterId);
            if (mapping != nullptr)
            {
                std::visit([&evt](auto&& tid) {
                    using T = std::decay_t<decltype(tid)>;
                    if constexpr (std::is_same_v<T, InternalParameterIdentifier>)
                    {
                        evt.parameter.nativeParameterId = tid.parameterKey;
                        evt.parameter.transportAccuracy = synth::TransportAccuracy::SampleAccurate;
                    }
                    else if constexpr (std::is_same_v<T, Vst3ParameterIdentifier>)
                    {
                        evt.parameter.nativeParameterId = tid.parameterId.empty() ? ("Param_" + std::to_string(tid.parameterIndex)) : tid.parameterId;
                        evt.parameter.transportAccuracy = synth::TransportAccuracy::BlockAccurate;
                    }
                    else if constexpr (std::is_same_v<T, MidiCcIdentifier>)
                    {
                        evt.parameter.nativeParameterId = "CC_" + std::to_string(tid.controllerNumber);
                        evt.parameter.transportAccuracy = synth::TransportAccuracy::Timestamped;
                    }
                    else if constexpr (std::is_same_v<T, MidiSysExIdentifier>)
                    {
                        evt.parameter.nativeParameterId = tid.messageTemplate;
                        evt.parameter.transportAccuracy = synth::TransportAccuracy::BestEffort;
                    }
                    else if constexpr (std::is_same_v<T, ManualOperatorIdentifier>)
                    {
                        evt.parameter.nativeParameterId = tid.instructionId;
                        evt.parameter.transportAccuracy = synth::TransportAccuracy::BestEffort;
                    }
                }, mapping->technicalIdentifier);
            }
        }
    }

    resolved.totalDurationSec = plan.totalDurationSec;
    resolved.totalSamples = static_cast<int64_t>(std::lround(plan.totalDurationSec * environment.sampleRate));
    resolved.resolvedExecutionPlanHash = computeResolvedExecutionPlanHash(plan.planHash, environment);

    result.resolvedPlan = resolved;
    return result;
}

core::ProfilingSession ExperimentPlanCompiler::createProfilingSession(
    const ResolvedExecutionPlan& resolvedPlan,
    const gui::session::TargetSelectionState& targetState,
    const TargetProfile* profile)
{
    core::ProfilingSession session;
    core::ProfilingMetadata meta;

    meta.hardwareName = targetState.targetName.empty() ? "SyntheticAudioFixture" : targetState.targetName;
    meta.targetModule = targetState.manufacturer.empty() ? "ABDAudioLab" : targetState.manufacturer;
    meta.operatorMode = "AUTOMATED_MIDI_NOTES";

    if (profile != nullptr)
    {
        meta.hardwareName = profile->displayName;
        meta.targetModule = profile->vendor;

        const auto& transports = profile->capabilities.controlTransports;
        if (std::find(transports.begin(), transports.end(), ControlTransportKind::ManualOperator) != transports.end())
        {
            meta.operatorMode = "MANUAL_OPERATOR";
        }
        else if (std::find(transports.begin(), transports.end(), ControlTransportKind::MidiContinuousController) != transports.end())
        {
            meta.operatorMode = "AUTOMATED_MIDI_CC";
        }
        else if (std::find(transports.begin(), transports.end(), ControlTransportKind::MidiSysEx) != transports.end())
        {
            meta.operatorMode = "AUTOMATED_SYSEX";
        }
    }

    meta.sampleRate = resolvedPlan.environment.sampleRate;
    session.setMetadata(meta);

    for (const auto& win : resolvedPlan.experimentPlan.windows)
    {
        core::TestCase tc;
        tc.testId = win.windowId;
        tc.functionalBlockType = "SpectrumFilter";
        tc.stimulusType = audio::StimulusType::Silence;
        tc.stimulusDurationSec = std::max(0.05, win.durationMs / 1000.0);
        tc.isAutonomousSynth = true;
        tc.midiChannel = 1;
        tc.midiNoteNumber = 60;
        tc.midiVelocity = 0.5f;
        tc.noteGateDurationSec = 0.25f;
        tc.stabilizationWaitMs = 50.0;
        tc.numPasses = 1;

        if (profile != nullptr && profile->measurementPolicies.defaultSettlingTimeMs > 0)
        {
            tc.stabilizationWaitMs = static_cast<double>(profile->measurementPolicies.defaultSettlingTimeMs);
        }

        // Localizar NoteOn correspondiente a esta ventana
        for (const auto& ev : resolvedPlan.experimentPlan.events)
        {
            if (ev.eventType == synth::TargetEventType::Midi && ev.midi.type == synth::TimedMidiType::NoteOn)
            {
                if (ev.absoluteSample >= win.startSample && ev.absoluteSample <= win.endSample)
                {
                    tc.midiNoteNumber = ev.midi.noteNumber;
                    tc.midiVelocity = ev.midi.velocity;
                    break;
                }
            }
        }

        // Localizar NoteOff correspondiente para la duración exacta del gate
        for (const auto& ev : resolvedPlan.experimentPlan.events)
        {
            if (ev.eventType == synth::TargetEventType::Midi && ev.midi.type == synth::TimedMidiType::NoteOff)
            {
                if (ev.absoluteSample >= win.startSample && ev.absoluteSample <= win.endSample)
                {
                    const double gateSamples = static_cast<double>(ev.absoluteSample - win.startSample);
                    if (resolvedPlan.environment.sampleRate > 0.0)
                    {
                        tc.noteGateDurationSec = static_cast<float>(gateSamples / resolvedPlan.environment.sampleRate);
                    }
                    break;
                }
            }
        }

        // Localizar paso de parámetro activo
        core::ParameterStep step;
        step.paramName = win.targetParameterId;
        step.normalizedValue = 0.0f;
        for (const auto& ev : resolvedPlan.experimentPlan.events)
        {
            if (ev.eventType == synth::TargetEventType::Parameter && ev.parameter.normalizedParameterId == win.targetParameterId)
            {
                if (ev.absoluteSample <= win.startSample)
                {
                    step.normalizedValue = static_cast<float>(ev.parameter.normalizedValue);
                }
            }
        }

        if (profile != nullptr)
        {
            const auto* mapping = profile->findMappingForSemanticId(win.targetParameterId);
            if (mapping != nullptr)
            {
                std::visit([&step, &tc](auto&& tid) {
                    using T = std::decay_t<decltype(tid)>;
                    if constexpr (std::is_same_v<T, MidiCcIdentifier>)
                    {
                        step.paramIndex = tid.controllerNumber;
                        step.rawValue = std::clamp(static_cast<int>(std::round(step.normalizedValue * 127.0f)), 0, 127);
                        step.controlType = "MidiCC";
                        tc.excitationMode = core::ExcitationMode::MidiNotes;
                    }
                    else if constexpr (std::is_same_v<T, MidiSysExIdentifier>)
                    {
                        step.controlType = "SysEx";
                        step.rawValue = std::clamp(static_cast<int>(std::round(step.normalizedValue * 127.0f)), 0, 127);
                        tc.excitationMode = core::ExcitationMode::MidiNotes;
                    }
                    else if constexpr (std::is_same_v<T, ManualOperatorIdentifier>)
                    {
                        step.controlType = tid.controlWidget.empty() ? "Knob" : tid.controlWidget;
                        step.id = tid.instructionId;
                        tc.excitationMode = core::ExcitationMode::ManualCapture;
                        tc.presetRecipe.recipeType = "MANUAL_PATCH";
                        tc.presetRecipe.description = tid.confirmationPrompt;
                    }
                }, mapping->technicalIdentifier);
            }
        }

        tc.parameterSteps.push_back(step);
        session.addTestCase(tc);
    }

    return session;
}

} // namespace abdaudiolab::profiling
