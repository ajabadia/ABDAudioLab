#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include "ExperimentPlan.h"

namespace abdaudiolab::synth
{

/**
 * @brief Interfaz base para recetas científicas generadoras de planes de ensayo.
 * Desacopla la formulación experimental del transporte físico o virtual.
 */
class IExperimentRecipe
{
public:
    virtual ~IExperimentRecipe() = default;
    [[nodiscard]] virtual std::string recipeType() const = 0;
    [[nodiscard]] virtual std::string recipeId() const = 0;
    [[nodiscard]] virtual ExperimentPlan generatePlan(double sampleRate,
                                                      const SettlingPolicy& settling,
                                                      const RandomizationPolicy& randomization) const = 0;
};

/**
 * @brief Receta 1: Excitación de notas base (matriz factorial de notas, velocidades y compuertas).
 */
class NoteExcitationRecipe : public IExperimentRecipe
{
public:
    NoteExcitationRecipe(std::string id, int note, float velocity, double gateSec)
        : id_(std::move(id)), note_(note), velocity_(velocity), gateSec_(gateSec)
    {
    }

    [[nodiscard]] std::string recipeType() const override { return "NoteExcitation"; }
    [[nodiscard]] std::string recipeId() const override { return id_; }

    [[nodiscard]] ExperimentPlan generatePlan(double sampleRate,
                                              const SettlingPolicy& settling,
                                              const RandomizationPolicy& randomization) const override
    {
        ExperimentPlan plan;
        plan.recipeId = id_;
        plan.planId = id_ + "_PLAN";
        plan.recipeType = recipeType();
        plan.sampleRate = sampleRate;
        plan.settling = settling;
        plan.randomization = randomization;
        plan.totalDurationSec = settling.preSilenceSec + gateSec_ + settling.postSilenceSec;

        int onsetSample = static_cast<int>(std::lround(settling.preSilenceSec * sampleRate));
        int offSample = onsetSample + static_cast<int>(std::lround(gateSec_ * sampleRate));

        // Evento 1: NoteOn
        TargetEvent evOn;
        evOn.eventType = TargetEventType::Midi;
        evOn.midi = TimedMidiEvent{ TimedMidiType::NoteOn, 1, note_, velocity_, onsetSample, settling.preSilenceSec * 1000.0 };
        evOn.absoluteSample = onsetSample;
        evOn.scheduledTimeMs = settling.preSilenceSec * 1000.0;
        plan.events.push_back(evOn);

        // Evento 2: NoteOff
        TargetEvent evOff;
        evOff.eventType = TargetEventType::Midi;
        evOff.midi = TimedMidiEvent{ TimedMidiType::NoteOff, 1, note_, 0.0f, offSample, (settling.preSilenceSec + gateSec_) * 1000.0 };
        evOff.absoluteSample = offSample;
        evOff.scheduledTimeMs = (settling.preSilenceSec + gateSec_) * 1000.0;
        plan.events.push_back(evOff);

        // Ventana de observación para la nota
        ObservationWindow win;
        win.windowId = "WIN_NOTE_" + std::to_string(note_);
        win.startSample = onsetSample;
        win.endSample = offSample;
        win.startTimeMs = settling.preSilenceSec * 1000.0;
        win.durationMs = gateSec_ * 1000.0;
        win.targetParameterId = "note_energy";
        win.domain = "Oscillator";
        plan.windows.push_back(win);

        plan.computeHash();
        return plan;
    }

private:
    std::string id_;
    int note_ { 60 };
    float velocity_ { 0.8f };
    double gateSec_ { 0.25 };
};

/**
 * @brief Receta 2: Barrido por escalones estables (ParameterStepRecipe).
 * Mide el estado estacionario y los transitorios de conmutación.
 */
class ParameterStepRecipe : public IExperimentRecipe
{
public:
    ParameterStepRecipe(std::string id,
                        std::string paramId,
                        std::vector<double> stepValues,
                        double stepDurationSec = 0.25,
                        int noteNumber = 60)
        : id_(std::move(id)),
          paramId_(std::move(paramId)),
          stepValues_(std::move(stepValues)),
          stepDurationSec_(stepDurationSec),
          noteNumber_(noteNumber)
    {
        if (stepValues_.empty())
        {
            stepValues_ = { 0.0, 0.25, 0.50, 0.75, 1.00 };
        }
    }

    [[nodiscard]] std::string recipeType() const override { return "ParameterStep"; }
    [[nodiscard]] std::string recipeId() const override { return id_; }

    [[nodiscard]] ExperimentPlan generatePlan(double sampleRate,
                                              const SettlingPolicy& settling,
                                              const RandomizationPolicy& randomization) const override
    {
        ExperimentPlan plan;
        plan.recipeId = id_;
        plan.planId = id_ + "_PLAN";
        plan.recipeType = recipeType();
        plan.sampleRate = sampleRate;
        plan.settling = settling;
        plan.randomization = randomization;

        double stepTotal = stepDurationSec_ + settling.interStepSettlingSec;
        plan.totalDurationSec = settling.preSilenceSec + static_cast<double>(stepValues_.size()) * stepTotal + settling.postSilenceSec;

        double currentTime = settling.preSilenceSec;
        for (size_t k = 0; k < stepValues_.size(); ++k)
        {
            int stepOnset = static_cast<int>(std::lround(currentTime * sampleRate));
            int stepOff = stepOnset + static_cast<int>(std::lround(stepDurationSec_ * sampleRate));

            // Parámetro aplicado justo al inicio del paso
            TargetEvent evP;
            evP.eventType = TargetEventType::Parameter;
            evP.parameter.normalizedParameterId = paramId_;
            evP.parameter.normalizedValue = stepValues_[k];
            evP.parameter.absoluteSample = stepOnset;
            evP.parameter.scheduledTimeMs = currentTime * 1000.0;
            evP.parameter.status = ParameterEventStatus::Requested;
            evP.absoluteSample = stepOnset;
            evP.scheduledTimeMs = currentTime * 1000.0;
            plan.events.push_back(evP);

            // Nota MIDI activada durante el paso
            TargetEvent evOn;
            evOn.eventType = TargetEventType::Midi;
            evOn.midi = TimedMidiEvent{ TimedMidiType::NoteOn, 1, noteNumber_, 0.8f, stepOnset, currentTime * 1000.0 };
            evOn.absoluteSample = stepOnset;
            evOn.scheduledTimeMs = currentTime * 1000.0;
            plan.events.push_back(evOn);

            TargetEvent evOff;
            evOff.eventType = TargetEventType::Midi;
            evOff.midi = TimedMidiEvent{ TimedMidiType::NoteOff, 1, noteNumber_, 0.0f, stepOff, (currentTime + stepDurationSec_) * 1000.0 };
            evOff.absoluteSample = stepOff;
            evOff.scheduledTimeMs = (currentTime + stepDurationSec_) * 1000.0;
            plan.events.push_back(evOff);

            ObservationWindow win;
            win.windowId = "WIN_STEP_" + std::to_string(k) + "_V" + std::to_string(stepValues_[k]);
            win.startSample = stepOnset;
            win.endSample = stepOff;
            win.startTimeMs = currentTime * 1000.0;
            win.durationMs = stepDurationSec_ * 1000.0;
            win.targetParameterId = paramId_;
            win.domain = "Filter";
            plan.windows.push_back(win);

            currentTime += stepTotal;
        }

        plan.computeHash();
        return plan;
    }

private:
    std::string id_;
    std::string paramId_;
    std::vector<double> stepValues_;
    double stepDurationSec_ { 0.25 };
    int noteNumber_ { 60 };
};

/**
 * @brief Receta 3: Rampa continua durante compuerta sostenida (ParameterRampRecipe).
 * Mide monotonicidad, resolución de transporte, suavizado y latencia efectiva.
 */
class ParameterRampRecipe : public IExperimentRecipe
{
public:
    ParameterRampRecipe(std::string id,
                        std::string paramId,
                        double startValue = 0.0,
                        double endValue = 1.0,
                        double rampDurationSec = 0.50,
                        int numUpdates = 20,
                        int noteNumber = 60)
        : id_(std::move(id)),
          paramId_(std::move(paramId)),
          startVal_(startValue),
          endVal_(endValue),
          rampDurationSec_(rampDurationSec),
          numUpdates_(std::max(2, numUpdates)),
          noteNumber_(noteNumber)
    {
    }

    [[nodiscard]] std::string recipeType() const override { return "ParameterRamp"; }
    [[nodiscard]] std::string recipeId() const override { return id_; }

    [[nodiscard]] ExperimentPlan generatePlan(double sampleRate,
                                              const SettlingPolicy& settling,
                                              const RandomizationPolicy& randomization) const override
    {
        ExperimentPlan plan;
        plan.recipeId = id_;
        plan.planId = id_ + "_PLAN";
        plan.recipeType = recipeType();
        plan.sampleRate = sampleRate;
        plan.settling = settling;
        plan.randomization = randomization;
        plan.totalDurationSec = settling.preSilenceSec + rampDurationSec_ + settling.postSilenceSec;

        int onsetSample = static_cast<int>(std::lround(settling.preSilenceSec * sampleRate));
        int offSample = onsetSample + static_cast<int>(std::lround(rampDurationSec_ * sampleRate));

        // NoteOn
        TargetEvent evOn;
        evOn.eventType = TargetEventType::Midi;
        evOn.midi = TimedMidiEvent{ TimedMidiType::NoteOn, 1, noteNumber_, 0.8f, onsetSample, settling.preSilenceSec * 1000.0 };
        evOn.absoluteSample = onsetSample;
        evOn.scheduledTimeMs = settling.preSilenceSec * 1000.0;
        plan.events.push_back(evOn);

        // Sub-eventos de rampa espaciados uniformemente
        for (int i = 0; i < numUpdates_; ++i)
        {
            double frac = static_cast<double>(i) / static_cast<double>(numUpdates_ - 1);
            double val = startVal_ + frac * (endVal_ - startVal_);
            double t = settling.preSilenceSec + frac * rampDurationSec_;
            int s = static_cast<int>(std::lround(t * sampleRate));

            TargetEvent evP;
            evP.eventType = TargetEventType::Parameter;
            evP.parameter.normalizedParameterId = paramId_;
            evP.parameter.normalizedValue = val;
            evP.parameter.absoluteSample = s;
            evP.parameter.scheduledTimeMs = t * 1000.0;
            evP.parameter.status = ParameterEventStatus::Requested;
            evP.absoluteSample = s;
            evP.scheduledTimeMs = t * 1000.0;
            plan.events.push_back(evP);
        }

        // NoteOff
        TargetEvent evOff;
        evOff.eventType = TargetEventType::Midi;
        evOff.midi = TimedMidiEvent{ TimedMidiType::NoteOff, 1, noteNumber_, 0.0f, offSample, (settling.preSilenceSec + rampDurationSec_) * 1000.0 };
        evOff.absoluteSample = offSample;
        evOff.scheduledTimeMs = (settling.preSilenceSec + rampDurationSec_) * 1000.0;
        plan.events.push_back(evOff);

        ObservationWindow win;
        win.windowId = "WIN_RAMP_" + paramId_;
        win.startSample = onsetSample;
        win.endSample = offSample;
        win.startTimeMs = settling.preSilenceSec * 1000.0;
        win.durationMs = rampDurationSec_ * 1000.0;
        win.targetParameterId = paramId_;
        win.domain = "Filter";
        plan.windows.push_back(win);

        plan.computeHash();
        return plan;
    }

private:
    std::string id_;
    std::string paramId_;
    double startVal_ { 0.0 };
    double endVal_ { 1.0 };
    double rampDurationSec_ { 0.50 };
    int numUpdates_ { 20 };
    int noteNumber_ { 60 };
};

/**
 * @brief Receta 4: Perturbación local central y estimación del Jacobiano (LocalPerturbationRecipe).
 * Genera ensayos emparejados en p0 - delta, p0 y p0 + delta para aproximar J_ij = dPhi / dp_j.
 */
class LocalPerturbationRecipe : public IExperimentRecipe
{
public:
    LocalPerturbationRecipe(std::string id,
                            std::string paramId,
                            double operatingPoint = 0.50,
                            double delta = 0.05,
                            double trialDurationSec = 0.20,
                            int noteNumber = 60)
        : id_(std::move(id)),
          paramId_(std::move(paramId)),
          operatingPoint_(operatingPoint),
          delta_(delta),
          trialDurationSec_(trialDurationSec),
          noteNumber_(noteNumber)
    {
    }

    [[nodiscard]] std::string recipeType() const override { return "LocalPerturbation"; }
    [[nodiscard]] std::string recipeId() const override { return id_; }
    [[nodiscard]] double delta() const noexcept { return delta_; }
    [[nodiscard]] double operatingPoint() const noexcept { return operatingPoint_; }
    [[nodiscard]] const std::string& targetParameter() const noexcept { return paramId_; }

    [[nodiscard]] ExperimentPlan generatePlan(double sampleRate,
                                              const SettlingPolicy& settling,
                                              const RandomizationPolicy& randomization) const override
    {
        ExperimentPlan plan;
        plan.recipeId = id_;
        plan.planId = id_ + "_PLAN";
        plan.recipeType = recipeType();
        plan.sampleRate = sampleRate;
        plan.settling = settling;
        plan.randomization = randomization;

        std::vector<double> points = {
            std::max(0.0, operatingPoint_ - delta_),
            operatingPoint_,
            std::min(1.0, operatingPoint_ + delta_)
        };

        double stepTotal = trialDurationSec_ + settling.interStepSettlingSec;
        plan.totalDurationSec = settling.preSilenceSec + static_cast<double>(points.size()) * stepTotal + settling.postSilenceSec;

        double currentTime = settling.preSilenceSec;
        for (size_t k = 0; k < points.size(); ++k)
        {
            int onset = static_cast<int>(std::lround(currentTime * sampleRate));
            int off = onset + static_cast<int>(std::lround(trialDurationSec_ * sampleRate));

            TargetEvent evP;
            evP.eventType = TargetEventType::Parameter;
            evP.parameter.normalizedParameterId = paramId_;
            evP.parameter.normalizedValue = points[k];
            evP.parameter.absoluteSample = onset;
            evP.parameter.scheduledTimeMs = currentTime * 1000.0;
            evP.parameter.status = ParameterEventStatus::Requested;
            evP.absoluteSample = onset;
            evP.scheduledTimeMs = currentTime * 1000.0;
            plan.events.push_back(evP);

            TargetEvent evOn;
            evOn.eventType = TargetEventType::Midi;
            evOn.midi = TimedMidiEvent{ TimedMidiType::NoteOn, 1, noteNumber_, 0.8f, onset, currentTime * 1000.0 };
            evOn.absoluteSample = onset;
            evOn.scheduledTimeMs = currentTime * 1000.0;
            plan.events.push_back(evOn);

            TargetEvent evOff;
            evOff.eventType = TargetEventType::Midi;
            evOff.midi = TimedMidiEvent{ TimedMidiType::NoteOff, 1, noteNumber_, 0.0f, off, (currentTime + trialDurationSec_) * 1000.0 };
            evOff.absoluteSample = off;
            evOff.scheduledTimeMs = (currentTime + trialDurationSec_) * 1000.0;
            plan.events.push_back(evOff);

            ObservationWindow win;
            win.windowId = "WIN_PERTURB_" + std::to_string(k);
            win.startSample = onset;
            win.endSample = off;
            win.startTimeMs = currentTime * 1000.0;
            win.durationMs = trialDurationSec_ * 1000.0;
            win.targetParameterId = paramId_;
            win.domain = "Filter";
            plan.windows.push_back(win);

            currentTime += stepTotal;
        }

        plan.computeHash();
        return plan;
    }

private:
    std::string id_;
    std::string paramId_;
    double operatingPoint_ { 0.50 };
    double delta_ { 0.05 };
    double trialDurationSec_ { 0.20 };
    int noteNumber_ { 60 };
};

/**
 * @brief Receta 5: Diferenciación emparejada estado a estado (PairwiseDifferentialRecipe).
 * Mide Delta y(t) = y(p + Delta p, t) - y(p, t) cancelando la componente fija del oscilador.
 */
class PairwiseDifferentialRecipe : public IExperimentRecipe
{
public:
    PairwiseDifferentialRecipe(std::string id,
                               std::string paramId,
                               double baseValue = 0.50,
                               double deltaValue = 0.20,
                               double durationSec = 0.25,
                               int noteNumber = 60)
        : id_(std::move(id)),
          paramId_(std::move(paramId)),
          baseValue_(baseValue),
          deltaValue_(deltaValue),
          durationSec_(durationSec),
          noteNumber_(noteNumber)
    {
    }

    [[nodiscard]] std::string recipeType() const override { return "PairwiseDifferential"; }
    [[nodiscard]] std::string recipeId() const override { return id_; }

    [[nodiscard]] ExperimentPlan generatePlan(double sampleRate,
                                              const SettlingPolicy& settling,
                                              const RandomizationPolicy& randomization) const override
    {
        ExperimentPlan plan;
        plan.recipeId = id_;
        plan.planId = id_ + "_PLAN";
        plan.recipeType = recipeType();
        plan.sampleRate = sampleRate;
        plan.settling = settling;
        plan.randomization = randomization;

        std::vector<double> pairValues = { baseValue_, std::min(1.0, baseValue_ + deltaValue_) };
        double stepTotal = durationSec_ + settling.interStepSettlingSec;
        plan.totalDurationSec = settling.preSilenceSec + 2.0 * stepTotal + settling.postSilenceSec;

        double currentTime = settling.preSilenceSec;
        for (int i = 0; i < 2; ++i)
        {
            int onset = static_cast<int>(std::lround(currentTime * sampleRate));
            int off = onset + static_cast<int>(std::lround(durationSec_ * sampleRate));

            TargetEvent evP;
            evP.eventType = TargetEventType::Parameter;
            evP.parameter.normalizedParameterId = paramId_;
            evP.parameter.normalizedValue = pairValues[static_cast<size_t>(i)];
            evP.parameter.absoluteSample = onset;
            evP.parameter.scheduledTimeMs = currentTime * 1000.0;
            evP.parameter.status = ParameterEventStatus::Requested;
            evP.absoluteSample = onset;
            evP.scheduledTimeMs = currentTime * 1000.0;
            plan.events.push_back(evP);

            TargetEvent evOn;
            evOn.eventType = TargetEventType::Midi;
            evOn.midi = TimedMidiEvent{ TimedMidiType::NoteOn, 1, noteNumber_, 0.8f, onset, currentTime * 1000.0 };
            evOn.absoluteSample = onset;
            evOn.scheduledTimeMs = currentTime * 1000.0;
            plan.events.push_back(evOn);

            TargetEvent evOff;
            evOff.eventType = TargetEventType::Midi;
            evOff.midi = TimedMidiEvent{ TimedMidiType::NoteOff, 1, noteNumber_, 0.0f, off, (currentTime + durationSec_) * 1000.0 };
            evOff.absoluteSample = off;
            evOff.scheduledTimeMs = (currentTime + durationSec_) * 1000.0;
            plan.events.push_back(evOff);

            ObservationWindow win;
            win.windowId = "WIN_DIFF_PAIR_" + std::to_string(i);
            win.startSample = onset;
            win.endSample = off;
            win.startTimeMs = currentTime * 1000.0;
            win.durationMs = durationSec_ * 1000.0;
            win.targetParameterId = paramId_;
            win.domain = "Filter";
            plan.windows.push_back(win);

            currentTime += stepTotal;
        }

        plan.computeHash();
        return plan;
    }

private:
    std::string id_;
    std::string paramId_;
    double baseValue_ { 0.50 };
    double deltaValue_ { 0.20 };
    double durationSec_ { 0.25 };
    int noteNumber_ { 60 };
};

/**
 * @brief Receta 6: Diseño factorial fraccionado para parejas de parámetros (FactorialInteractionRecipe).
 * Explora el espacio cruzado (pA x pB) en 4 vértices (0.2, 0.8)x(0.2, 0.8).
 */
class FactorialInteractionRecipe : public IExperimentRecipe
{
public:
    FactorialInteractionRecipe(std::string id,
                               std::string paramA,
                               std::string paramB,
                               double lowA = 0.25, double highA = 0.75,
                               double lowB = 0.25, double highB = 0.75,
                               double durationSec = 0.20,
                               int noteNumber = 60)
        : id_(std::move(id)),
          paramA_(std::move(paramA)),
          paramB_(std::move(paramB)),
          lowA_(lowA), highA_(highA),
          lowB_(lowB), highB_(highB),
          durationSec_(durationSec),
          noteNumber_(noteNumber)
    {
    }

    [[nodiscard]] std::string recipeType() const override { return "FactorialInteraction"; }
    [[nodiscard]] std::string recipeId() const override { return id_; }

    [[nodiscard]] ExperimentPlan generatePlan(double sampleRate,
                                              const SettlingPolicy& settling,
                                              const RandomizationPolicy& randomization) const override
    {
        ExperimentPlan plan;
        plan.recipeId = id_;
        plan.planId = id_ + "_PLAN";
        plan.recipeType = recipeType();
        plan.sampleRate = sampleRate;
        plan.settling = settling;
        plan.randomization = randomization;

        struct PairPoint { double a; double b; };
        std::vector<PairPoint> grid = {
            { lowA_, lowB_ },
            { highA_, lowB_ },
            { lowA_, highB_ },
            { highA_, highB_ }
        };

        double stepTotal = durationSec_ + settling.interStepSettlingSec;
        plan.totalDurationSec = settling.preSilenceSec + static_cast<double>(grid.size()) * stepTotal + settling.postSilenceSec;

        double currentTime = settling.preSilenceSec;
        for (size_t k = 0; k < grid.size(); ++k)
        {
            int onset = static_cast<int>(std::lround(currentTime * sampleRate));
            int off = onset + static_cast<int>(std::lround(durationSec_ * sampleRate));

            TargetEvent evPA;
            evPA.eventType = TargetEventType::Parameter;
            evPA.parameter.normalizedParameterId = paramA_;
            evPA.parameter.normalizedValue = grid[k].a;
            evPA.parameter.absoluteSample = onset;
            evPA.parameter.scheduledTimeMs = currentTime * 1000.0;
            evPA.parameter.status = ParameterEventStatus::Requested;
            evPA.absoluteSample = onset;
            evPA.scheduledTimeMs = currentTime * 1000.0;
            plan.events.push_back(evPA);

            TargetEvent evPB;
            evPB.eventType = TargetEventType::Parameter;
            evPB.parameter.normalizedParameterId = paramB_;
            evPB.parameter.normalizedValue = grid[k].b;
            evPB.parameter.absoluteSample = onset;
            evPB.parameter.scheduledTimeMs = currentTime * 1000.0;
            evPB.parameter.status = ParameterEventStatus::Requested;
            evPB.absoluteSample = onset;
            evPB.scheduledTimeMs = currentTime * 1000.0;
            plan.events.push_back(evPB);

            TargetEvent evOn;
            evOn.eventType = TargetEventType::Midi;
            evOn.midi = TimedMidiEvent{ TimedMidiType::NoteOn, 1, noteNumber_, 0.8f, onset, currentTime * 1000.0 };
            evOn.absoluteSample = onset;
            evOn.scheduledTimeMs = currentTime * 1000.0;
            plan.events.push_back(evOn);

            TargetEvent evOff;
            evOff.eventType = TargetEventType::Midi;
            evOff.midi = TimedMidiEvent{ TimedMidiType::NoteOff, 1, noteNumber_, 0.0f, off, (currentTime + durationSec_) * 1000.0 };
            evOff.absoluteSample = off;
            evOff.scheduledTimeMs = (currentTime + durationSec_) * 1000.0;
            plan.events.push_back(evOff);

            ObservationWindow win;
            win.windowId = "WIN_INTERACTION_" + std::to_string(k);
            win.startSample = onset;
            win.endSample = off;
            win.startTimeMs = currentTime * 1000.0;
            win.durationMs = durationSec_ * 1000.0;
            win.targetParameterId = paramA_ + ":" + paramB_;
            win.domain = "Filter";
            plan.windows.push_back(win);

            currentTime += stepTotal;
        }

        plan.computeHash();
        return plan;
    }

private:
    std::string id_;
    std::string paramA_;
    std::string paramB_;
    double lowA_ { 0.25 };
    double highA_ { 0.75 };
    double lowB_ { 0.25 };
    double highB_ { 0.75 };
    double durationSec_ { 0.20 };
    int noteNumber_ { 60 };
};

/**
 * @brief Receta 7: Secuencia pseudoaleatoria multinivel APRBS (PRBSExcitationRecipe).
 * Conmutación multinivel (ej. 0.25 -> 0.75 -> 0.40 -> 0.90 -> 0.10) para medir
 * dinámica, constante de tiempo de suavizado interno y respuesta transitoria.
 */
class PRBSExcitationRecipe : public IExperimentRecipe
{
public:
    PRBSExcitationRecipe(std::string id,
                         std::string paramId,
                         std::vector<double> levels = { 0.25, 0.75, 0.40, 0.90, 0.10 },
                         double chipDurationSec = 0.10,
                         int noteNumber = 60)
        : id_(std::move(id)),
          paramId_(std::move(paramId)),
          levels_(std::move(levels)),
          chipDurationSec_(chipDurationSec),
          noteNumber_(noteNumber)
    {
        if (levels_.empty())
        {
            levels_ = { 0.25, 0.75, 0.40, 0.90, 0.10 };
        }
    }

    [[nodiscard]] std::string recipeType() const override { return "PRBSExcitation"; }
    [[nodiscard]] std::string recipeId() const override { return id_; }

    [[nodiscard]] ExperimentPlan generatePlan(double sampleRate,
                                              const SettlingPolicy& settling,
                                              const RandomizationPolicy& randomization) const override
    {
        ExperimentPlan plan;
        plan.recipeId = id_;
        plan.planId = id_ + "_PLAN";
        plan.recipeType = recipeType();
        plan.sampleRate = sampleRate;
        plan.settling = settling;
        plan.randomization = randomization;

        double noteDuration = static_cast<double>(levels_.size()) * chipDurationSec_;
        plan.totalDurationSec = settling.preSilenceSec + noteDuration + settling.postSilenceSec;

        int onsetSample = static_cast<int>(std::lround(settling.preSilenceSec * sampleRate));
        int offSample = onsetSample + static_cast<int>(std::lround(noteDuration * sampleRate));

        // NoteOn sostenido durante toda la secuencia APRBS
        TargetEvent evOn;
        evOn.eventType = TargetEventType::Midi;
        evOn.midi = TimedMidiEvent{ TimedMidiType::NoteOn, 1, noteNumber_, 0.8f, onsetSample, settling.preSilenceSec * 1000.0 };
        evOn.absoluteSample = onsetSample;
        evOn.scheduledTimeMs = settling.preSilenceSec * 1000.0;
        plan.events.push_back(evOn);

        for (size_t k = 0; k < levels_.size(); ++k)
        {
            double t = settling.preSilenceSec + static_cast<double>(k) * chipDurationSec_;
            int s = static_cast<int>(std::lround(t * sampleRate));

            TargetEvent evP;
            evP.eventType = TargetEventType::Parameter;
            evP.parameter.normalizedParameterId = paramId_;
            evP.parameter.normalizedValue = levels_[k];
            evP.parameter.absoluteSample = s;
            evP.parameter.scheduledTimeMs = t * 1000.0;
            evP.parameter.status = ParameterEventStatus::Requested;
            evP.absoluteSample = s;
            evP.scheduledTimeMs = t * 1000.0;
            plan.events.push_back(evP);

            ObservationWindow win;
            win.windowId = "WIN_APRBS_CHIP_" + std::to_string(k);
            win.startSample = s;
            win.endSample = s + static_cast<int>(std::lround(chipDurationSec_ * sampleRate));
            win.startTimeMs = t * 1000.0;
            win.durationMs = chipDurationSec_ * 1000.0;
            win.targetParameterId = paramId_;
            win.domain = "Filter";
            plan.windows.push_back(win);
        }

        // NoteOff
        TargetEvent evOff;
        evOff.eventType = TargetEventType::Midi;
        evOff.midi = TimedMidiEvent{ TimedMidiType::NoteOff, 1, noteNumber_, 0.0f, offSample, (settling.preSilenceSec + noteDuration) * 1000.0 };
        evOff.absoluteSample = offSample;
        evOff.scheduledTimeMs = (settling.preSilenceSec + noteDuration) * 1000.0;
        plan.events.push_back(evOff);

        plan.computeHash();
        return plan;
    }

private:
    std::string id_;
    std::string paramId_;
    std::vector<double> levels_;
    double chipDurationSec_ { 0.10 };
    int noteNumber_ { 60 };
};

} // namespace abdaudiolab::synth
