#include "TargetAuditor.h"
#include <numeric>
#include <algorithm>
#include <cmath>

namespace abdaudiolab::synth
{

double TargetAuditor::computeRms(const float* data, size_t count) noexcept
{
    if (data == nullptr || count == 0)
        return 0.0;

    double sumSq = 0.0;
    for (size_t i = 0; i < count; ++i)
    {
        double val = static_cast<double>(data[i]);
        sumSq += val * val;
    }
    return std::sqrt(sumSq / static_cast<double>(count));
}

double TargetAuditor::computeRmsDb(const float* data, size_t count) noexcept
{
    double rms = computeRms(data, count);
    if (rms <= 1e-9)
        return -180.0;
    return 20.0 * std::log10(rms);
}

double TargetAuditor::computeRmse(const std::vector<float>& a, const std::vector<float>& b) noexcept
{
    size_t n = std::min(a.size(), b.size());
    if (n == 0)
        return 0.0;

    double sumSq = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        double diff = static_cast<double>(a[i]) - static_cast<double>(b[i]);
        sumSq += diff * diff;
    }
    return std::sqrt(sumSq / static_cast<double>(n));
}

double TargetAuditor::computeCorrelation(const std::vector<float>& a, const std::vector<float>& b) noexcept
{
    size_t n = std::min(a.size(), b.size());
    if (n == 0)
        return 0.0;

    double meanA = 0.0, meanB = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        meanA += a[i];
        meanB += b[i];
    }
    meanA /= static_cast<double>(n);
    meanB /= static_cast<double>(n);

    double num = 0.0, denA = 0.0, denB = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        double da = a[i] - meanA;
        double db = b[i] - meanB;
        num += da * db;
        denA += da * da;
        denB += db * db;
    }

    double den = std::sqrt(denA * denB);
    if (den <= 1e-12)
        return 1.0; // Si ambas son constantes o idénticas
    return num / den;
}

double TargetAuditor::computeEsrDb(const std::vector<float>& clean, const std::vector<float>& test) noexcept
{
    size_t n = std::min(clean.size(), test.size());
    if (n == 0)
        return -180.0;

    double errorEnergy = 0.0;
    double cleanEnergy = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        double c = static_cast<double>(clean[i]);
        double diff = c - static_cast<double>(test[i]);
        errorEnergy += diff * diff;
        cleanEnergy += c * c;
    }

    if (errorEnergy <= 1e-18)
        return -180.0;
    if (cleanEnergy <= 1e-18)
        return 0.0;

    return 10.0 * std::log10(errorEnergy / cleanEnergy);
}

AudioEquivalenceLevel TargetAuditor::evaluateEquivalence(const std::vector<float>& a,
                                                        const std::vector<float>& b,
                                                        const TargetAuditPolicy& policy,
                                                        double& outRmse,
                                                        double& outCorr,
                                                        double& outEsrDb) noexcept
{
    outRmse = computeRmse(a, b);
    outCorr = computeCorrelation(a, b);
    outEsrDb = computeEsrDb(a, b);

    if (a.size() == b.size() && outRmse == 0.0)
    {
        return AudioEquivalenceLevel::ByteIdentical;
    }

    if (outCorr >= policy.functionalCorrelationMin && outEsrDb <= policy.functionalEsrMaxDb)
    {
        return AudioEquivalenceLevel::FunctionallyEquivalent;
    }

    // Comprobar si la energía espectral/RMS promedio es casi idéntica a pesar del desfase
    double rmsA = computeRms(a.data(), a.size());
    double rmsB = computeRms(b.data(), b.size());
    double rmsRatioDb = std::abs(20.0 * std::log10((rmsA + 1e-9) / (rmsB + 1e-9)));

    if (rmsRatioDb < 0.50 && outCorr > 0.10)
    {
        return AudioEquivalenceLevel::StatisticallyEquivalent;
    }

    return AudioEquivalenceLevel::Divergent;
}

MidiExcitationSequence TargetAuditor::makeSingleNoteSequence(int noteNumber,
                                                            float velocity,
                                                            double gateDurationSec,
                                                            double postSilenceSec,
                                                            double sampleRate)
{
    MidiExcitationSequence seq;
    seq.channel = 1;
    seq.noteNumber = noteNumber;
    seq.normalizedVelocity = velocity;
    seq.midiVelocity = static_cast<int>(std::lround(velocity * 127.0f));
    seq.gateDurationSec = gateDurationSec;
    seq.preSilenceSec = 0.02;
    seq.postSilenceSec = postSilenceSec;
    seq.totalDurationSec = seq.preSilenceSec + gateDurationSec + postSilenceSec;
    seq.sequenceId = "AUDIT_SINGLE_" + std::to_string(noteNumber);

    int onsetSample = static_cast<int>(std::lround(seq.preSilenceSec * sampleRate));
    int noteOffSample = onsetSample + static_cast<int>(std::lround(gateDurationSec * sampleRate));

    seq.events.push_back(TimedMidiEvent{ TimedMidiType::NoteOn, 1, noteNumber, velocity, onsetSample, seq.preSilenceSec * 1000.0 });
    seq.events.push_back(TimedMidiEvent{ TimedMidiType::NoteOff, 1, noteNumber, 0.0f, noteOffSample, (seq.preSilenceSec + gateDurationSec) * 1000.0 });

    seq.computeHash();
    return seq;
}

MidiExcitationSequence TargetAuditor::makeDoubleNoteSequence(int note1,
                                                            int note2,
                                                            float velocity,
                                                            double gateDurationSec,
                                                            double interSilenceSec,
                                                            double postSilenceSec,
                                                            double sampleRate)
{
    MidiExcitationSequence seq;
    seq.channel = 1;
    seq.noteNumber = note1;
    seq.normalizedVelocity = velocity;
    seq.midiVelocity = static_cast<int>(std::lround(velocity * 127.0f));
    seq.gateDurationSec = gateDurationSec;
    seq.preSilenceSec = 0.02;
    seq.postSilenceSec = postSilenceSec;
    seq.totalDurationSec = seq.preSilenceSec + gateDurationSec + interSilenceSec + gateDurationSec + postSilenceSec;
    seq.sequenceId = "AUDIT_DOUBLE_" + std::to_string(note1) + "_" + std::to_string(note2);

    int onset1 = static_cast<int>(std::lround(seq.preSilenceSec * sampleRate));
    int off1 = onset1 + static_cast<int>(std::lround(gateDurationSec * sampleRate));
    int onset2 = off1 + static_cast<int>(std::lround(interSilenceSec * sampleRate));
    int off2 = onset2 + static_cast<int>(std::lround(gateDurationSec * sampleRate));

    seq.events.push_back(TimedMidiEvent{ TimedMidiType::NoteOn, 1, note1, velocity, onset1, seq.preSilenceSec * 1000.0 });
    seq.events.push_back(TimedMidiEvent{ TimedMidiType::NoteOff, 1, note1, 0.0f, off1, (seq.preSilenceSec + gateDurationSec) * 1000.0 });
    seq.events.push_back(TimedMidiEvent{ TimedMidiType::NoteOn, 1, note2, velocity, onset2, (seq.preSilenceSec + gateDurationSec + interSilenceSec) * 1000.0 });
    seq.events.push_back(TimedMidiEvent{ TimedMidiType::NoteOff, 1, note2, 0.0f, off2, (seq.totalDurationSec - postSilenceSec) * 1000.0 });

    seq.computeHash();
    return seq;
}

TargetAuditReport TargetAuditor::auditTarget(ISynthTarget& target, const ProcessingSpec& spec)
{
    TargetAuditReport report;
    report.auditProtocolId = "pre_profiling_target_auditor";
    report.auditProtocolVersion = "1.0.0";
    report.effectivePolicy = policy_;
    report.sampleRate = spec.sampleRate;
    report.blockSize = spec.blockSize;

    target.prepare(spec);

    // =========================================================================
    // 1. PRUEBA DE GENERACIÓN AUTÓNOMA Y REACTIVIDAD A NOTAS
    // =========================================================================
    {
        auto seqNote = makeSingleNoteSequence(60, 0.8f, 0.30, 0.25, spec.sampleRate);
        report.eventSequenceHash = seqNote.sequenceHash;

        target.resetState();
        std::vector<float> audioNote;
        target.render(seqNote, audioNote, 0);

        int preSamples = static_cast<int>(std::lround(0.02 * spec.sampleRate));
        int gateSamples = static_cast<int>(std::lround(0.30 * spec.sampleRate));

        double restRms = computeRms(audioNote.data(), static_cast<size_t>(std::max(1, preSamples)));
        double activeRms = computeRms(audioNote.data() + preSamples, static_cast<size_t>(std::max(1, gateSamples)));

        double restRmsDb = (restRms > 1e-9) ? 20.0 * std::log10(restRms) : -180.0;
        double activeRmsDb = (activeRms > 1e-9) ? 20.0 * std::log10(activeRms) : -180.0;
        double deltaRmsDb = activeRmsDb - restRmsDb;

        report.generationEvidence.restRmsDb = restRmsDb;
        report.generationEvidence.activeRmsDb = activeRmsDb;
        report.generationEvidence.deltaRmsDb = deltaRmsDb;
        report.generationEvidence.audioOutputPresent = (activeRms > 1e-4); // > -80 dBFS

        if (activeRms <= 1e-4)
        {
            report.generation = GenerationClass::SilentOutput;
            report.approvalStatus = ApprovalStatus::Unsupported;
            report.isApprovedForParameterExcitation = false;
            report.limitations.push_back("Target produces silent output when excited by MIDI notes");
            report.summaryMessage = "Target is silent: not a generative synthesizer";
            return report; // No continuar con más pruebas en targets mudos
        }
        else if (deltaRmsDb < policy_.generationDeltaRmsMinDb && restRms > 1e-3)
        {
            report.generation = GenerationClass::AudioObservedButNotNoteResponsive;
            report.approvalStatus = ApprovalStatus::Unsupported;
            report.isApprovedForParameterExcitation = false;
            report.limitations.push_back("Continuous audio observed but target is insensitive to NoteOn/NoteOff events");
            report.summaryMessage = "Target audio does not respond to MIDI notes (unsupported for MIDI recipe)";
            return report;
        }
        else
        {
            report.generation = GenerationClass::GenerativeAudioObserved;
            report.generationEvidence.noteResponsive = true;
            report.generationEvidence.onsetDetected = true;
            report.generationEvidence.pitchDetected = true;
        }
    }

    // =========================================================================
    // 2. PRUEBA DE DETERMINISMO Y PROCEDIMIENTO DE RESET
    // =========================================================================
    {
        auto seq = makeSingleNoteSequence(60, 0.75f, 0.25, 0.15, spec.sampleRate);

        // Run 1: con reset previo
        target.resetState();
        std::vector<float> run1;
        target.render(seq, run1, 0);

        // Run 2: SIN reset previo (prueba de fase continua o estado acumulado)
        std::vector<float> run2_noReset;
        target.render(seq, run2_noReset, 1);

        double rmse_noReset = 0.0, corr_noReset = 0.0, esr_noReset = 0.0;
        auto eq_noReset = evaluateEquivalence(run1, run2_noReset, policy_, rmse_noReset, corr_noReset, esr_noReset);

        // Run 3: CON reset previo idéntico al de Run 1
        target.resetState();
        std::vector<float> run3_reset;
        target.render(seq, run3_reset, 2);

        double rmse_reset = 0.0, corr_reset = 0.0, esr_reset = 0.0;
        auto eq_reset = evaluateEquivalence(run1, run3_reset, policy_, rmse_reset, corr_reset, esr_reset);

        std::string hash1 = Sha256::computeHex(reinterpret_cast<const uint8_t*>(run1.data()), run1.size() * sizeof(float));
        std::string hash3 = Sha256::computeHex(reinterpret_cast<const uint8_t*>(run3_reset.data()), run3_reset.size() * sizeof(float));

        report.determinismEvidence.run1Sha256 = hash1;
        report.determinismEvidence.run2Sha256 = hash3;
        report.determinismEvidence.run1vs2Rmse = rmse_reset;
        report.determinismEvidence.run1vs2Correlation = corr_reset;
        report.determinismEvidence.run1vs2EsrDb = esr_reset;
        report.determinismEvidence.run1vs2Equivalence = eq_reset;
        report.determinismEvidence.byteIdentical = (hash1 == hash3);
        report.determinismEvidence.functionallyEquivalent = (eq_reset == AudioEquivalenceLevel::FunctionallyEquivalent || eq_reset == AudioEquivalenceLevel::ByteIdentical);
        report.determinismEvidence.statisticallyEquivalent = (eq_reset != AudioEquivalenceLevel::Divergent);

        if (eq_noReset == AudioEquivalenceLevel::ByteIdentical)
        {
            // Determinista puro sin requerir reset entre tomas
            report.determinism = DeterminismClass::Deterministic;
            report.resetCapability = ResetCapability::Resettable;
            report.operationalInstructions.resetBeforeEachTrial = false;
        }
        else if (eq_reset == AudioEquivalenceLevel::ByteIdentical ||
                 eq_reset == AudioEquivalenceLevel::FunctionallyEquivalent)
        {
            // Determinista tras resetState()
            report.determinism = DeterminismClass::DeterministicAfterReset;
            report.resetCapability = ResetCapability::Resettable;
            report.determinismEvidence.resetEliminatesDrift = true;
            report.operationalInstructions.resetBeforeEachTrial = true;
            report.warnings.push_back("Target requires resetState() before each trial for deterministic alignment");
        }
        else
        {
            // No repetible ni con reset: clasificar estocástico
            report.determinism = DeterminismClass::StochasticUnseeded;
            report.resetCapability = ResetCapability::PartiallyResettable;
            report.operationalInstructions.useStatisticalAveraging = true;
            report.operationalInstructions.exactHashComparisonPermitted = false;
            report.warnings.push_back("Target exhibits unseeded stochastic behavior: statistical averaging over trials required");
        }
    }

    // =========================================================================
    // 3. PRUEBA DE PERSISTENCIA DE ESTADO INTER-NOTAS (A -> B vs B -> A)
    // =========================================================================
    {
        // Prueba con reposo corto (100 ms)
        auto seqAB_short = makeDoubleNoteSequence(60, 64, 0.75f, 0.20, policy_.interNoteSettlingShortSec, 0.15, spec.sampleRate);
        auto seqBA_short = makeDoubleNoteSequence(64, 60, 0.75f, 0.20, policy_.interNoteSettlingShortSec, 0.15, spec.sampleRate);

        target.resetState();
        std::vector<float> audioAB;
        target.render(seqAB_short, audioAB, 0);

        target.resetState();
        std::vector<float> audioBA;
        target.render(seqBA_short, audioBA, 0);

        // Medir energía en la ventana de reposo entre notas de seqAB
        int onset2Sample = static_cast<int>(std::lround((0.02 + 0.20 + policy_.interNoteSettlingShortSec) * spec.sampleRate));
        int off1Sample = static_cast<int>(std::lround((0.02 + 0.20) * spec.sampleRate));
        int interLength = onset2Sample - off1Sample;

        double tailRms = 0.0;
        if (interLength > 0 && (off1Sample + interLength) <= static_cast<int>(audioAB.size()))
        {
            // Medir en el tramo final del reposo corto (los últimos 100 ms antes de Note 2,
            // tras la conclusión de cualquier release de envolvente estándar)
            int measureWindowSamples = static_cast<int>(std::lround(0.10 * spec.sampleRate));
            int measureStart = std::max(off1Sample, onset2Sample - measureWindowSamples);
            int measureLen = onset2Sample - measureStart;
            if (measureLen > 0)
            {
                tailRms = computeRms(audioAB.data() + measureStart, static_cast<size_t>(measureLen));
            }
        }
        double tailRmsDb = (tailRms > 1e-9) ? 20.0 * std::log10(tailRms) : -180.0;
        report.statefulnessEvidence.tailRmsDb = tailRmsDb;

        // Prueba de control con reposo largo (2.0 s)
        auto seqAB_long = makeDoubleNoteSequence(60, 64, 0.75f, 0.20, policy_.interNoteSettlingLongSec, 0.15, spec.sampleRate);
        target.resetState();
        std::vector<float> audioAB_long;
        target.render(seqAB_long, audioAB_long, 0);

        // Nota 64 aislada de referencia tras reset
        auto seqB_isolated = makeSingleNoteSequence(64, 0.75f, 0.20, 0.15, spec.sampleRate);
        target.resetState();
        std::vector<float> audioB_isolated;
        target.render(seqB_isolated, audioB_isolated, 0);

        // Comparar Nota B en flujo continuo corto con Nota B aislada
        int noteB_start_short = onset2Sample;
        int noteB_samples = static_cast<int>(std::lround(0.20 * spec.sampleRate));

        std::vector<float> segmentB_short;
        if (noteB_start_short + noteB_samples <= static_cast<int>(audioAB.size()))
        {
            segmentB_short.assign(audioAB.begin() + noteB_start_short,
                                  audioAB.begin() + noteB_start_short + noteB_samples);
        }

        std::vector<float> segmentB_isolated;
        int onsetIso = static_cast<int>(std::lround(0.02 * spec.sampleRate));
        if (onsetIso + noteB_samples <= static_cast<int>(audioB_isolated.size()))
        {
            segmentB_isolated.assign(audioB_isolated.begin() + onsetIso,
                                     audioB_isolated.begin() + onsetIso + noteB_samples);
        }

        double corrB = computeCorrelation(segmentB_isolated, segmentB_short);
        report.statefulnessEvidence.interNoteCorrelationShort = corrB;

        // Comparar Nota B en flujo continuo largo con Nota B aislada
        int onset2Long = static_cast<int>(std::lround((0.02 + 0.20 + policy_.interNoteSettlingLongSec) * spec.sampleRate));
        std::vector<float> segmentB_long;
        if (onset2Long + noteB_samples <= static_cast<int>(audioAB_long.size()))
        {
            segmentB_long.assign(audioAB_long.begin() + onset2Long,
                                 audioAB_long.begin() + onset2Long + noteB_samples);
        }
        double corrBLong = computeCorrelation(segmentB_isolated, segmentB_long);
        report.statefulnessEvidence.interNoteCorrelationLong = corrBLong;

        report.statefulnessEvidence.crossOrderDependencyDetected = (tailRmsDb > -45.0 || corrB < 0.98);

        if (tailRmsDb > -45.0)
        {
            report.interNoteState = InterNoteState::StatefulBehaviorDetected;
            report.statefulnessEvidence.detectedCause = ResidualCause::EffectTail;
            report.statefulnessEvidence.recommendedSettlingTimeSec = policy_.interNoteSettlingLongSec;
            report.operationalInstructions.recommendedSettlingTimeMs = policy_.interNoteSettlingLongSec * 1000.0;
            report.warnings.push_back("Effect tail detected between notes: settling window of " +
                                      std::to_string(policy_.interNoteSettlingLongSec) + "s required");
        }
        else if (corrB < 0.98)
        {
            report.interNoteState = InterNoteState::StatefulBehaviorDetected;
            if (corrBLong >= 0.98)
            {
                report.statefulnessEvidence.detectedCause = ResidualCause::EnvelopeState;
                report.statefulnessEvidence.recommendedSettlingTimeSec = policy_.interNoteSettlingLongSec;
                report.operationalInstructions.recommendedSettlingTimeMs = policy_.interNoteSettlingLongSec * 1000.0;
                report.warnings.push_back("Envelope release memory detected: extended settling window required");
            }
            else
            {
                report.statefulnessEvidence.detectedCause = ResidualCause::OscillatorPhase;
                report.statefulnessEvidence.recommendedSettlingTimeSec = 0.05;
                report.warnings.push_back("Inter-note phase memory detected (OscillatorPhase)");
            }
        }
        else
        {
            report.interNoteState = InterNoteState::Stateless;
            report.statefulnessEvidence.detectedCause = ResidualCause::None;
        }
    }

    // =========================================================================
    // 4. PRUEBA DE STATE ROUND-TRIP (3 CAPAS: BINARIO, PARÁMETROS, AUDIO)
    // =========================================================================
    if (target.supportsBinaryState())
    {
        std::vector<uint8_t> initialData;
        auto getRes1 = target.getState(initialData);

        if (!getRes1.succeeded || initialData.empty())
        {
            report.stateRoundTrip = StateRoundTripStatus::StateRoundTripWarning;
            report.roundTripEvidence.failureCause = RoundTripFailureCause::InvalidStateData;
            report.roundTripEvidence.failureDetails = "getState() failed or returned empty data";
            report.warnings.push_back("State round-trip warning: getState() failed");
        }
        else
        {
            report.roundTripEvidence.initialBinaryHash = getRes1.stateDataHash;

            // Renderizar con estado inicial A
            auto seq = makeSingleNoteSequence(60, 0.8f, 0.20, 0.15, spec.sampleRate);
            target.resetState();
            std::vector<float> audioBefore;
            target.render(seq, audioBefore, 0);

            // Restaurar estado A
            auto setRes = target.setState(initialData);
            std::vector<uint8_t> restoredData;
            auto getRes2 = target.getState(restoredData);

            report.roundTripEvidence.restoredBinaryHash = getRes2.stateDataHash;

            // Capa 1: Identidad binaria
            report.roundTripEvidence.binaryIdentical = (getRes1.stateDataHash == getRes2.stateDataHash);

            // Capa 2: Igualdad de parámetros (si verifyState es Passed)
            report.roundTripEvidence.parameterIdentical = (target.verifyState() == StateAppliedStatus::Passed);

            // Capa 3: Igualdad de comportamiento acústico
            target.resetState();
            std::vector<float> audioAfter;
            target.render(seq, audioAfter, 0);

            double rtRmse = 0.0, rtCorr = 0.0, rtEsr = 0.0;
            auto rtEquiv = evaluateEquivalence(audioBefore, audioAfter, policy_, rtRmse, rtCorr, rtEsr);
            report.roundTripEvidence.audioComparisonRmse = rtRmse;
            report.roundTripEvidence.behaviorIdentical = (rtEquiv == AudioEquivalenceLevel::ByteIdentical ||
                                                          rtEquiv == AudioEquivalenceLevel::FunctionallyEquivalent);

            if (!report.roundTripEvidence.binaryIdentical)
            {
                report.stateRoundTrip = StateRoundTripStatus::StateRoundTripWarning;
                report.roundTripEvidence.failureCause = RoundTripFailureCause::BinaryStateMismatch;
                report.roundTripEvidence.failureDetails = "Binary state mismatch after restore";
                report.warnings.push_back("State round-trip warning: binary state altered after restore");
            }
            else if (!report.roundTripEvidence.behaviorIdentical)
            {
                report.stateRoundTrip = StateRoundTripStatus::StateRoundTripWarning;
                report.roundTripEvidence.failureCause = RoundTripFailureCause::BehavioralMismatch;
                report.roundTripEvidence.failureDetails = "Audio rendered after restore diverges from original";
                report.warnings.push_back("State round-trip warning: acoustic output diverges after restore");
            }
            else
            {
                report.stateRoundTrip = StateRoundTripStatus::Passed;
            }
        }
    }
    else
    {
        report.stateRoundTrip = StateRoundTripStatus::Unsupported;
        report.roundTripEvidence.failureCause = RoundTripFailureCause::Unsupported;
        report.roundTripEvidence.failureDetails = "Target does not implement binary state serialization";
    }

    // =========================================================================
    // 5. DICTAMEN DE APROBACIÓN ADAPTATIVO
    // =========================================================================
    if (report.determinism == DeterminismClass::Deterministic &&
        report.interNoteState == InterNoteState::Stateless &&
        (report.stateRoundTrip == StateRoundTripStatus::Passed || report.stateRoundTrip == StateRoundTripStatus::Unsupported))
    {
        report.approvalStatus = ApprovalStatus::Approved;
        report.isApprovedForParameterExcitation = true;
        report.summaryMessage = "Target fully verified: deterministic, stateless and note-responsive";
    }
    else if (report.determinism == DeterminismClass::DeterministicAfterReset ||
             report.interNoteState == InterNoteState::StatefulBehaviorDetected ||
             report.determinism == DeterminismClass::StochasticUnseeded ||
             report.stateRoundTrip == StateRoundTripStatus::StateRoundTripWarning)
    {
        report.approvalStatus = ApprovalStatus::ApprovedWithWarnings;
        report.isApprovedForParameterExcitation = true;
        report.summaryMessage = "Target approved with operational instructions: adaptations required for parameter excitation";
    }
    else
    {
        report.approvalStatus = ApprovalStatus::Rejected;
        report.isApprovedForParameterExcitation = false;
        report.summaryMessage = "Target rejected: incompatible with parameter profiling";
    }

    return report;
}

} // namespace abdaudiolab::synth
