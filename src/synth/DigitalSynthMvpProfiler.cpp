#include "DigitalSynthMvpProfiler.h"
#include "SyntheticSynthTarget.h"
#include "Sha256.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <sstream>

namespace abdaudiolab::synth
{

DigitalSynthMvpProfiler::DigitalSynthMvpProfiler(double sampleRate)
    : sampleRate_(sampleRate)
{
}

SynthProfileReport DigitalSynthMvpProfiler::runFixtureSession(const SynthPresetState& preset,
                                                             SyntheticSynthFixture& fixture,
                                                             int noteNumber,
                                                             int numPassesPerCondition)
{
    SyntheticSynthTarget target(fixture);
    return runTargetSession(preset, target, noteNumber, numPassesPerCondition);
}

SynthProfileReport DigitalSynthMvpProfiler::runTargetSession(const SynthPresetState& preset,
                                                            ISynthTarget& target,
                                                            int noteNumber,
                                                            int numPassesPerCondition)
{
    ProcessingSpec spec;
    spec.sampleRate = sampleRate_;
    spec.blockSize = 256;
    spec.numChannels = 2;
    target.prepare(spec);
    target.loadState(preset);

    TrialAudioRenderer source = [&](const MidiExcitationSequence& seq, int rep) {
        target.resetState();
        std::vector<float> audio;
        target.render(seq, audio, rep);
        return audio;
    };

    auto report = runMvpSession(preset, source, noteNumber, numPassesPerCondition);
    report.stateValidation = stateAppliedStatusToString(target.verifyState());
    return report;
}

SynthProfileReport DigitalSynthMvpProfiler::runMvpSession(const SynthPresetState& preset,
                                                         const TrialAudioRenderer& audioSource,
                                                         int noteNumber,
                                                         int numPassesPerCondition)
{
    SynthProfileReport report;
    report.presetId = preset.presetId;
    report.stateHash = preset.stateHash;
    report.rawSysExHash = preset.rawSysExHash;
    report.normalizedParameterHash = preset.normalizedParameterHash;
    report.stateValidation = stateAppliedStatusToString(preset.stateStatus);

    if (numPassesPerCondition < 1 || sampleRate_ <= 0.0)
    {
        report.behaviorValidation = "INVALID_MEASUREMENT";
        return report;
    }

    // -------------------------------------------------------------------------
    // 1. Calibración temporal impulsiva previa
    // -------------------------------------------------------------------------
    auto calibSeq = MidiExcitationSequence::createNoteTrial(1, noteNumber, 110, 0.02, sampleRate_, 0.05, 0.10);
    std::vector<std::vector<float>> calibTakes;
    calibTakes.reserve(3);
    for (int rep = 0; rep < 3; ++rep)
    {
        calibTakes.push_back(audioSource(calibSeq, rep));
    }

    size_t scheduledCalibNoteOn = static_cast<size_t>(std::lround(calibSeq.preSilenceSec * sampleRate_));
    auto transportCalib = MidiAudioSynchronizer::calibrateTransportOffset(calibTakes, sampleRate_, scheduledCalibNoteOn, 0.15);

    report.midiAudioOffsetMs = transportCalib.transportOffsetEstimateMs;
    report.midiAudioOffsetStdDevMs = transportCalib.transportOffsetUncertaintyMs;

    // -------------------------------------------------------------------------
    // 2. Bucle Factorial de 9 condiciones (3 velocidades x 3 duraciones) x N
    // -------------------------------------------------------------------------
    report.conditions.reserve(velocityGrid_.size() * durationGrid_.size());

    std::vector<double> allOnsetJitters;
    std::vector<double> allPitchCents;
    std::vector<double> peakGainsVel40;
    std::vector<double> peakGainsVel110;
    std::vector<double> attacksDurationShort;
    std::vector<double> attacksDurationLong;

    bool anyClippingDetected = false;
    bool anyNoteDropped = false;

    for (int vel : velocityGrid_)
    {
        for (double dur : durationGrid_)
        {
            ConditionSummary cond;
            cond.velocityByte = vel;
            cond.gateDurationSec = dur;
            cond.numPasses = numPassesPerCondition;
            cond.trials.reserve(static_cast<size_t>(numPassesPerCondition));

            auto seq = MidiExcitationSequence::createNoteTrial(1, noteNumber, vel, dur, sampleRate_, 0.05, 0.80);

            std::vector<double> condAttacks;
            std::vector<double> condDecays;
            std::vector<double> condSustains;
            std::vector<double> condReleases;
            std::vector<double> condPitchErrors;
            std::vector<double> condOnsets;

            size_t noteOnSample = static_cast<size_t>(std::lround(seq.preSilenceSec * sampleRate_));
            size_t noteOffSample = noteOnSample + static_cast<size_t>(std::lround(dur * sampleRate_));

            for (int p = 0; p < numPassesPerCondition; ++p)
            {
                SynthTrialObservation trial;
                trial.passIndex = p + 1;

                auto audio = audioSource(seq, p);

                if (audio.empty())
                {
                    trial.isValid = false;
                    trial.invalidReason = "Audio buffer vacio";
                    anyNoteDropped = true;
                    cond.trials.push_back(trial);
                    continue;
                }

                // Checksum SHA-256 del audio grabado
                trial.audioSha256Hash = Sha256::computeHex(audio.data(), audio.size() * sizeof(float));

                // Detección de clipping y nivel de señal
                float maxAbs = 0.0f;
                int clipCount = 0;
                for (float s : audio)
                {
                    float a = std::abs(s);
                    if (a > maxAbs) maxAbs = a;
                    if (a >= 0.999f) clipCount++;
                }

                trial.quality.clippingDetected = (clipCount > 0);
                trial.quality.clippedSamplesCount = clipCount;
                if (trial.quality.clippingDetected) anyClippingDetected = true;

                if (maxAbs < 1e-4f)
                {
                    trial.isValid = false;
                    trial.invalidReason = "Nota perdida / ausencia de sonido";
                    anyNoteDropped = true;
                    cond.trials.push_back(trial);
                    continue;
                }

                // Onset y sincronización
                auto novelty = MidiAudioSynchronizer::computeNoveltyFunction(audio, sampleRate_, 64);
                size_t onsetSample = MidiAudioSynchronizer::detectOnsetSample(novelty, 64, noteOnSample, sampleRate_);
                double onsetTimeMs = (static_cast<double>(onsetSample) / sampleRate_) * 1000.0;
                condOnsets.push_back(onsetTimeMs);

                // Envolvente
                trial.envelope = SynthEnvelopeAnalyzer::analyzeEnvelope(audio, sampleRate_, noteOnSample, noteOffSample, onsetSample);

                // Sincronización temporal con desacoplamiento de calibración
                TimedMidiEvent noteOnEv { TimedMidiType::NoteOn, 1, noteNumber, seq.normalizedVelocity, static_cast<int>(noteOnSample), seq.preSilenceSec * 1000.0 };
                trial.timing = MidiAudioSynchronizer::synchronizeTrial(audio, sampleRate_, noteOnEv, transportCalib, trial.envelope.attackTimeMs.value);

                // Estimación de tono sobre región sostenida
                size_t pitchStart = onsetSample + static_cast<size_t>(std::lround((trial.envelope.attackTimeMs.value / 1000.0) * sampleRate_)) + 64;
                size_t pitchWindow = static_cast<size_t>(std::min(sampleRate_ * 0.10, static_cast<double>(noteOffSample > pitchStart ? noteOffSample - pitchStart : 256)));
                double nominalHz = SynthPitchEstimator::midiNoteToFrequencyHz(noteNumber);
                auto pEst = SynthPitchEstimator::estimatePitch(audio, sampleRate_, nominalHz, pitchStart, pitchWindow);

                trial.pitch.nominalFrequencyHz = nominalHz;
                trial.pitch.estimate = pEst;
                trial.pitch.analysisWindowStartMs = (static_cast<double>(pitchStart) / sampleRate_) * 1000.0;
                trial.pitch.analysisWindowDurationMs = (static_cast<double>(pitchWindow) / sampleRate_) * 1000.0;

                // Registro para agregación
                if (trial.timing.netAttackMs.isReliable())
                    condAttacks.push_back(trial.timing.netAttackMs.value);
                if (trial.envelope.decayTimeMs.isReliable())
                    condDecays.push_back(trial.envelope.decayTimeMs.value);
                if (trial.envelope.sustainLevelDb.isReliable())
                    condSustains.push_back(trial.envelope.sustainLevelDb.value);
                if (trial.envelope.releaseTimeMs.isReliable())
                    condReleases.push_back(trial.envelope.releaseTimeMs.value);
                if (pEst.status == MetricStatus::EstimatedWithUncertainty)
                    condPitchErrors.push_back(pEst.centsError);

                if (vel == 40) peakGainsVel40.push_back(trial.envelope.peakAmplitudeDbfs);
                if (vel == 110) peakGainsVel110.push_back(trial.envelope.peakAmplitudeDbfs);

                if (dur <= 0.06) attacksDurationShort.push_back(trial.timing.netAttackMs.value);
                if (dur >= 1.90) attacksDurationLong.push_back(trial.timing.netAttackMs.value);

                cond.validPassesCount++;
                cond.trials.push_back(trial);
            }

            // Agregación estadística de la condición
            auto computeStats = [](const std::vector<double>& vals) -> std::pair<double, double> {
                if (vals.empty()) return { 0.0, 0.0 };
                double mean = std::accumulate(vals.begin(), vals.end(), 0.0) / static_cast<double>(vals.size());
                double sumSq = 0.0;
                for (double v : vals) sumSq += (v - mean) * (v - mean);
                double stdDev = (vals.size() > 1) ? std::sqrt(sumSq / static_cast<double>(vals.size() - 1)) : 0.0;
                return { mean, stdDev };
            };

            auto [attMean, attStd] = computeStats(condAttacks);
            cond.netAttackMs.value = attMean;
            cond.netAttackMs.stdDev = attStd;
            cond.netAttackMs.status = condAttacks.empty() ? MetricStatus::Invalid : MetricStatus::EstimatedWithUncertainty;

            auto [decMean, decStd] = computeStats(condDecays);
            cond.decayMs.value = decMean;
            cond.decayMs.stdDev = decStd;
            cond.decayMs.status = condDecays.empty() ? MetricStatus::NotObservableInGate : MetricStatus::EstimatedWithUncertainty;

            auto [susMean, susStd] = computeStats(condSustains);
            cond.sustainDb.value = susMean;
            cond.sustainDb.stdDev = susStd;
            cond.sustainDb.status = condSustains.empty() ? MetricStatus::NotObservableInGate : MetricStatus::EstimatedWithUncertainty;

            auto [relMean, relStd] = computeStats(condReleases);
            cond.releaseMs.value = relMean;
            cond.releaseMs.stdDev = relStd;
            cond.releaseMs.status = condReleases.empty() ? MetricStatus::Invalid : MetricStatus::EstimatedWithUncertainty;

            auto [pMean, pStd] = computeStats(condPitchErrors);
            cond.pitchErrorCents.value = pMean;
            cond.pitchErrorCents.stdDev = pStd;
            cond.pitchErrorCents.status = condPitchErrors.empty() ? MetricStatus::Unreliable : MetricStatus::EstimatedWithUncertainty;

            auto [onMean, onStd] = computeStats(condOnsets);
            cond.onsetRepeatabilityMs.value = onStd;
            cond.onsetRepeatabilityMs.stdDev = 0.01;
            cond.onsetRepeatabilityMs.status = MetricStatus::Observed;
            allOnsetJitters.push_back(onStd);

            for (double c : condPitchErrors) allPitchCents.push_back(c);

            report.conditions.push_back(cond);
        }
    }

    // -------------------------------------------------------------------------
    // 3. Falsabilidad, Hipótesis y Diagnósticos Globales
    // -------------------------------------------------------------------------

    // Repetibilidad global de onset
    if (!allOnsetJitters.empty())
    {
        report.onsetRepeatabilityMs = std::accumulate(allOnsetJitters.begin(), allOnsetJitters.end(), 0.0) / static_cast<double>(allOnsetJitters.size());
    }

    // Pitch global
    if (!allPitchCents.empty())
    {
        double sum = std::accumulate(allPitchCents.begin(), allPitchCents.end(), 0.0);
        double mean = sum / static_cast<double>(allPitchCents.size());
        double sumSq = 0.0;
        for (double c : allPitchCents) sumSq += (c - mean) * (c - mean);
        double stdDev = std::sqrt(sumSq / static_cast<double>(allPitchCents.size()));
        report.pitchMeanCents = mean;
        report.pitchStdDevCents = stdDev;
    }

    // Parámetros de envolvente representativos globales (tomados de la condición nominal intermedia vel 64, dur 2.0s)
    for (const auto& c : report.conditions)
    {
        if (c.velocityByte == 64 && c.gateDurationSec >= 1.90)
        {
            report.attackMs = c.netAttackMs;
            report.decayMs = c.decayMs;
            report.sustainDb = c.sustainDb;
            report.releaseMs = c.releaseMs;
            break;
        }
    }

    // Falsabilidad 1: Comprobar si la duración de compuerta altera el ataque
    double shortAttMean = attacksDurationShort.empty() ? 0.0 : std::accumulate(attacksDurationShort.begin(), attacksDurationShort.end(), 0.0) / attacksDurationShort.size();
    double longAttMean = attacksDurationLong.empty() ? 0.0 : std::accumulate(attacksDurationLong.begin(), attacksDurationLong.end(), 0.0) / attacksDurationLong.size();
    double deltaAttack = std::abs(shortAttMean - longAttMean);

    if (deltaAttack > 0.8)
    {
        report.durationInvarianceStatus = "ANOMALY_DURATION_ALTERS_ATTACK";
        report.notesAndWarnings.push_back("Advertencia: El ataque medido cambia significativamente con la duracion de la nota.");
    }
    else
    {
        report.durationInvarianceStatus = "VERIFIED_ATTACK_INVARIANT";
    }

    // Falsabilidad 2: Respuesta a velocidad
    double meanGain40 = peakGainsVel40.empty() ? 0.0 : std::accumulate(peakGainsVel40.begin(), peakGainsVel40.end(), 0.0) / peakGainsVel40.size();
    double meanGain110 = peakGainsVel110.empty() ? 0.0 : std::accumulate(peakGainsVel110.begin(), peakGainsVel110.end(), 0.0) / peakGainsVel110.size();
    double deltaGain = std::abs(meanGain110 - meanGain40);

    if (deltaGain >= 2.0)
    {
        report.velocitySensitivityStatus = "OBSERVED";
    }
    else
    {
        report.velocitySensitivityStatus = "NOT_OBSERVED_IN_ANCHOR";
    }

    // Veredicto global de comportamiento
    if (anyNoteDropped)
    {
        report.behaviorValidation = "REJECTED";
        report.notesAndWarnings.push_back("Fallo: Se detectaron notas perdidas o silencios espurios durante los ensayos.");
    }
    else if (anyClippingDetected)
    {
        report.behaviorValidation = "INVALID_MEASUREMENT";
        report.notesAndWarnings.push_back("Medicion invalida: Se detecto recorte (clipping) en la captura de audio.");
    }
    else if (report.durationInvarianceStatus == "ANOMALY_DURATION_ALTERS_ATTACK" || report.onsetRepeatabilityMs > 1.0)
    {
        report.behaviorValidation = "INCONCLUSIVE";
    }
    else
    {
        report.behaviorValidation = "PASSED";
    }

    std::string expBlob = report.schemaVersion + "\n"
                        + "SR=" + std::to_string(sampleRate_) + "\n"
                        + "PASSES=" + std::to_string(numPassesPerCondition) + "\n"
                        + "OFFSET=" + std::to_string(report.midiAudioOffsetMs) + "\n"
                        + "VERDICT=" + report.behaviorValidation + "\n";
    report.experimentHash = Sha256::computeHex(expBlob);

    return report;
}

} // namespace abdaudiolab::synth
