/**
 * @file CalibrationPanelPainterStepper.cpp
 * @brief Header row, status badge and the two-card Step 2A / Step 2B stepper.
 * @author ABDSynths
 * @date 2026
 */

#include "CalibrationPanelPainter.h"

#include "CalibrationPanelMetrics.h"

#include "../SoundIdTheme.h"

namespace abdaudiolab::gui::calibrationpanel::painter
{

namespace
{

//==============================================================================
// Status badge of the header row.
//==============================================================================
struct BadgeStyle
{
    juce::String label;
    juce::Colour background { SoundIdTheme::bgCardHover };
    juce::Colour foreground { SoundIdTheme::textSecondary };
    float textHeight { 10.5f };
    float cornerRadius { 6.0f };
};

BadgeStyle headerBadgeStyle(const ViewState& view)
{
    if (view.digitalMode)
    {
        return view.digitalVerified
            ? BadgeStyle { "[ DIGITAL MODE ACTIVE ]", juce::Colour(0xffd1fae5), juce::Colour(0xff065f46) }
            : BadgeStyle { "[ VERIFICATION PENDING ]", juce::Colour(0xffe0e7ff), juce::Colour(0xff3730a3) };
    }

    switch (view.state)
    {
        case State::Success:
            return { "[ CALIBRATION COMPLETED ]", juce::Colour(0xffd1fae5), juce::Colour(0xff065f46) };
        case State::Skipped:
            return { "[ CALIBRATION BYPASSED ]", juce::Colour(0xfffef3c7), juce::Colour(0xff92400e) };
        case State::NoiseBaselinePassed:
            return { "[ STEP 2B: LOOPBACK READY ]", juce::Colour(0xffd1fae5), juce::Colour(0xff065f46) };
        case State::MeasuringNoiseBaseline:
            return { "[ STEP 2A: MEASURING BASELINE... ]", juce::Colour(0xffe0e7ff), juce::Colour(0xff3730a3) };
        case State::Measuring:
            return { "[ STEP 2B: MEASURING SWEEP... ]", juce::Colour(0xffe0e7ff), juce::Colour(0xff3730a3) };
        case State::Failed:
            return { "[ CHECK RETURN SIGNAL ]", juce::Colour(0xfffee2e2), SoundIdTheme::accentRed };
        case State::ReadyToMeasure:
        default:
            return { "[ STEP 2A: SAFETY PREPARATION ]", SoundIdTheme::bgCardHover, SoundIdTheme::textSecondary };
    }
}

//==============================================================================
// Step card model: 2A and 2B share the exact same renderer.
//==============================================================================
struct PillStyle
{
    juce::String label;
    juce::Colour background;
    juce::Colour foreground;
    float textHeight { 9.5f };
};

struct StepCardModel
{
    juce::Colour background { SoundIdTheme::bgCardHover };
    juce::Colour border { SoundIdTheme::borderSubtle };
    bool active { false };
    PillStyle pill;
    float pillWidth { 118.0f };
    juce::Colour circleFill { SoundIdTheme::textMuted };
    juce::String circleText { "2A" };
    float circleTextHeight { 10.0f };
    juce::String title;
    juce::Colour titleColour { SoundIdTheme::textPrimary };
    juce::String subtitle;
    juce::Colour subtitleColour { SoundIdTheme::textSecondary };
};

StepCardModel buildStepCard2A(const ViewState& view)
{
    const auto baselineState = view.noiseBaselineState;
    const bool active = view.subView == SubView::NoiseBaseline_2A;
    const bool passed = baselineState == NoiseBaselineState::Passed;
    const bool checking = baselineState == NoiseBaselineState::Checking;
    const bool contaminated = baselineState == NoiseBaselineState::Contaminated;
    const bool safetyAborted = baselineState == NoiseBaselineState::SafetyAborted;
    const bool deviceStopped = baselineState == NoiseBaselineState::DeviceStopped;
    const bool failed = contaminated || safetyAborted || deviceStopped;

    StepCardModel card;
    card.active = active;
    card.border = active ? SoundIdTheme::accentBlue : SoundIdTheme::borderSubtle;

    if (passed)
    {
        card.background = SoundIdTheme::accentGreen.withAlpha(active ? 0.12f : 0.06f);
        card.border = active ? SoundIdTheme::accentGreen : SoundIdTheme::accentGreen.withAlpha(0.6f);
    }
    else if (checking)
    {
        card.background = SoundIdTheme::accentBlue.withAlpha(0.12f);
        card.border = SoundIdTheme::accentBlue;
    }
    else if (failed)
    {
        card.background = SoundIdTheme::accentRed.withAlpha(0.10f);
        card.border = SoundIdTheme::accentRed;
    }

    card.pillWidth = 118.0f;
    if (passed)
        card.pill = { "[ PASS ]", juce::Colour(0xffd1fae5), juce::Colour(0xff065f46) };
    else if (checking)
        card.pill = { "[ CHECKING ]", juce::Colour(0xffe0e7ff), juce::Colour(0xff3730a3) };
    else if (deviceStopped)
    {
        card.pill = { "[ DEVICE STOPPED ]", juce::Colour(0xfffee2e2), SoundIdTheme::accentRed, 8.0f };
    }
    else if (contaminated)
        card.pill = { "[ CONTAMINATED ]", juce::Colour(0xfffee2e2), SoundIdTheme::accentRed, 9.0f };
    else if (safetyAborted)
        card.pill = { "[ SAFETY ABORT ]", juce::Colour(0xfffee2e2), SoundIdTheme::accentRed, 8.5f };
    else
        card.pill = { "[ PENDING ]", SoundIdTheme::bgCard, SoundIdTheme::accentBlue };

    card.circleFill = passed ? SoundIdTheme::accentGreen
                             : (active ? SoundIdTheme::accentBlue : SoundIdTheme::textMuted);
    card.circleText = passed ? juce::String("OK") : juce::String("2A");
    card.circleTextHeight = passed ? 9.5f : 10.0f;

    card.titleColour = passed ? SoundIdTheme::accentGreen
                              : (active ? SoundIdTheme::textPrimary : SoundIdTheme::textSecondary);
    card.title = "Paso 2A: Suelo de Ruido de Entrada";
    if (active)
        card.title += " [ACTIVA]";

    if (passed)
    {
        const juce::String noise = view.noiseReport.rmsDbfs < -120.0f
                                       ? juce::String("Silencio absoluto")
                                       : juce::String(view.noiseReport.rmsDbfs, 1) + " dBFS";
        card.subtitle = "Ruido: " + noise + " | Cable desconectado";
    }
    else if (checking)
        card.subtitle = "Midiendo 400 ms con salida muteada...";
    else if (deviceStopped)
        card.subtitle = "Dispositivo de audio detenido o desconectado";
    else if (failed)
        card.subtitle = "Entrada ruidosa o con senal. Comprueba cables.";
    else
        card.subtitle = "Cable desconectado | Salida en silencio (0.0f)";

    return card;
}

StepCardModel buildStepCard2B(const ViewState& view)
{
    const auto loopback = view.loopbackState;
    const bool active = view.subView == SubView::PhysicalLoopback_2B;
    const bool passed = loopback == LoopbackState::Passed;
    const bool measuringPreflight = loopback == LoopbackState::MeasuringPreflight;
    const bool measuringSweep = loopback == LoopbackState::MeasuringSweep;
    const bool stale = loopback == LoopbackState::Stale;
    const bool ready = loopback == LoopbackState::Ready;
    const bool locked = loopback == LoopbackState::Locked;
    const bool deviceStopped = loopback == LoopbackState::DeviceStopped;

    StepCardModel card;
    card.active = active;
    card.border = active ? SoundIdTheme::accentBlue : SoundIdTheme::borderSubtle;

    if (passed)
    {
        card.background = SoundIdTheme::accentGreen.withAlpha(active ? 0.12f : 0.06f);
        card.border = active ? SoundIdTheme::accentGreen : SoundIdTheme::accentGreen.withAlpha(0.6f);
    }
    else if (measuringPreflight || measuringSweep)
    {
        card.background = SoundIdTheme::accentBlue.withAlpha(0.12f);
        card.border = SoundIdTheme::accentBlue;
    }
    else if (stale)
    {
        card.background = SoundIdTheme::accentAmber.withAlpha(active ? 0.12f : 0.06f);
        card.border = SoundIdTheme::accentAmber;
    }
    else if (ready)
    {
        card.background = SoundIdTheme::accentGreen.withAlpha(active ? 0.12f : 0.06f);
        card.border = active ? SoundIdTheme::accentGreen : SoundIdTheme::accentGreen.withAlpha(0.7f);
    }
    else if (locked)
    {
        card.background = SoundIdTheme::bgCardHover.withAlpha(0.4f);
        card.border = SoundIdTheme::borderSubtle.withAlpha(0.4f);
    }
    else if (deviceStopped || isLoopbackFailureState(loopback))
    {
        card.background = SoundIdTheme::accentRed.withAlpha(0.08f);
        card.border = SoundIdTheme::accentRed;
    }

    card.pillWidth = 126.0f;
    if (passed)
        card.pill = { "[ PASS ]", juce::Colour(0xffd1fae5), juce::Colour(0xff065f46) };
    else if (measuringPreflight)
        card.pill = { "[ PREFLIGHT ]", juce::Colour(0xffe0e7ff), juce::Colour(0xff3730a3), 9.0f };
    else if (measuringSweep)
        card.pill = { "[ SWEEPING ]", juce::Colour(0xffe0e7ff), juce::Colour(0xff3730a3), 9.0f };
    else if (stale)
        card.pill = { "[ STALE ]", juce::Colour(0xfffef3c7), juce::Colour(0xff92400e) };
    else if (ready)
        card.pill = { "[ READY ]", SoundIdTheme::accentGreen, juce::Colours::white };
    else if (loopback == LoopbackState::Clipped)
        card.pill = { "[ CLIPPED ]", juce::Colour(0xfffee2e2), SoundIdTheme::accentRed, 9.0f };
    else if (loopback == LoopbackState::SignalTooLow)
        card.pill = { "[ LOW LEVEL ]", juce::Colour(0xfffef3c7), juce::Colour(0xff92400e), 8.5f };
    else if (deviceStopped)
        card.pill = { "[ DEVICE STOPPED ]", juce::Colour(0xfffee2e2), SoundIdTheme::accentRed, 8.5f };
    else if (isLoopbackFailureState(loopback))
        card.pill = { "[ FAIL ]", juce::Colour(0xfffee2e2), SoundIdTheme::accentRed };
    else
        card.pill = { "[ LOCKED ]", SoundIdTheme::bgCard.withAlpha(0.6f), SoundIdTheme::textMuted, 9.0f };

    card.circleText = juce::String("2B");
    if (passed)
    {
        card.circleFill = SoundIdTheme::accentGreen;
        card.circleText = juce::String("OK");
        card.circleTextHeight = 9.5f;
    }
    else if (ready)
    {
        card.circleFill = SoundIdTheme::accentGreen;
    }
    else
    {
        card.circleFill = locked ? SoundIdTheme::textMuted.withAlpha(0.4f) : SoundIdTheme::accentBlue;
    }

    card.titleColour = passed ? SoundIdTheme::accentGreen
                              : (active ? SoundIdTheme::textPrimary : SoundIdTheme::textSecondary);
    card.title = "Paso 2B: Medicion Barrido Loopback";
    if (active)
        card.title += " [ACTIVA]";

    card.subtitleColour = locked ? SoundIdTheme::textMuted : SoundIdTheme::textSecondary;
    if (passed)
        card.subtitle = "RTL: " + juce::String(view.loopbackReport.roundTripLatencyMs, 2) + " ms | Trim: "
                        + formatTrimDb(view.loopbackReport.recommendedTrimGain, 1);
    else if (stale)
        card.subtitle = "Desfasado: 2A se reinicio | Se requiere nuevo 2B";
    else if (ready)
        card.subtitle = "Cable patch conectado | Listo para preflight y sweep";
    else if (measuringPreflight)
        card.subtitle = "Preflight de seguridad (200 ms con salida muteada)...";
    else if (measuringSweep)
        card.subtitle = "Barrido Farina contractual (1300 ms)...";
    else if (locked)
        card.subtitle = "Bloqueado: Primero debe validarse el Paso 2A";
    else if (deviceStopped)
        card.subtitle = "Dispositivo de audio detenido o desconectado";
    else
        card.subtitle = "Comprueba cable loopback y Direct Monitor";

    return card;
}

void paintStepCard(juce::Graphics& g,
                   const juce::Rectangle<float>& cardBounds,
                   const StepCardModel& model)
{
    drawCardShell(g, cardBounds, model.background, model.border,
                  calibrationpanel::kInnerCardCornerRadius, model.active ? 2.0f : 1.0f);

    auto inner = cardBounds.reduced(10.0f, 8.0f);

    auto pill = inner.removeFromRight(model.pillWidth).withHeight(20.0f).withY(inner.getY() + 2.0f);
    drawBadge(g, pill, model.pill.label, model.pill.background, model.pill.foreground,
              model.pill.textHeight, juce::Justification::centred, 4.0f);

    auto circle = inner.removeFromLeft(26.0f).withSizeKeepingCentre(22.0f, 22.0f);
    g.setColour(model.circleFill);
    g.fillEllipse(circle);
    g.setFont(font(model.circleTextHeight, juce::Font::bold));
    g.setColour(juce::Colours::white);
    g.drawText(model.circleText, circle, juce::Justification::centred, false);

    inner.removeFromLeft(8.0f);

    g.setFont(font(11.5f, juce::Font::bold));
    g.setColour(model.titleColour);
    g.drawText(model.title, inner.removeFromTop(18.0f), juce::Justification::centredLeft, true);

    g.setFont(font(10.0f));
    g.setColour(model.subtitleColour);
    g.drawText(model.subtitle, inner, juce::Justification::topLeft, true);
}

} // namespace

//==============================================================================
void paintHeaderRow(juce::Graphics& g, const ViewState& view, juce::Rectangle<float> headerRow)
{
    g.setFont(font(18.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText("2. Audio Interface Calibration", headerRow.removeFromLeft(460.0f),
               juce::Justification::centredLeft, true);

    paintStatusBadge(g, view, headerRow.removeFromRight(220.0f).reduced(0.0f, 3.0f));
}

void paintStatusBadge(juce::Graphics& g, const ViewState& view, juce::Rectangle<float> badgeRect)
{
    const auto style = headerBadgeStyle(view);
    drawBadge(g, badgeRect, style.label, style.background, style.foreground,
              style.textHeight, juce::Justification::centred, style.cornerRadius);
}

void paintStepper(juce::Graphics& g,
                  const ViewState& view,
                  juce::Rectangle<float> stepperArea,
                  juce::Rectangle<int>& outCard2ABounds,
                  juce::Rectangle<int>& outCard2BBounds)
{
    const float cardWidth = (stepperArea.getWidth() - calibrationpanel::kStepperGapX) * 0.5f;
    const auto card2ABounds = stepperArea.removeFromLeft(cardWidth);
    stepperArea.removeFromLeft(calibrationpanel::kStepperGapX);
    const auto card2BBounds = stepperArea;

    outCard2ABounds = card2ABounds.toNearestInt();
    outCard2BBounds = card2BBounds.toNearestInt();

    paintStepCard(g, card2ABounds, buildStepCard2A(view));
    paintStepCard(g, card2BBounds, buildStepCard2B(view));
}

} // namespace abdaudiolab::gui::calibrationpanel::painter
