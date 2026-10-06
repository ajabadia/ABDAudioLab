/**
 * @file CalibrationPanelPainterInstructions.cpp
 * @brief Left column copy for Step 2 (analog sub-step 2A / 2B) plus the digital-mode body.
 * @author ABDSynths
 * @date 2026
 */

#include "CalibrationPanelPainter.h"

#include "CalibrationPanelMetrics.h"

#include <array>
#include <cstddef>

#include "../SoundIdTheme.h"

namespace abdaudiolab::gui::calibrationpanel::painter
{

namespace
{

constexpr float kInstructionRowHeight { 50.0f };
constexpr float kDigitalRowHeight { 62.0f };

/** @brief One numbered instruction row of the left column. */
struct InstructionStep
{
    juce::String title;
    juce::String description;
    juce::Colour accent;
};

std::array<InstructionStep, 4> buildSteps2A(const ViewState& view)
{
    const auto& input = view.inputChannelName;
    return { {
        { "1. Desconectar Cable de Loopback",
          "Desconecta el cable de loopback. Desconecta instrumentos y microfonos de " + input + ".\n"
          "La entrada debe medir exclusivamente el ruido propio del previo y conversores.",
          SoundIdTheme::accentGreen },
        { "2. Salida Digital en Silencio Absoluto",
          "La salida digital permanece muteada a 0.0f (-inf dBFS).\n"
          "Garantiza que no exista excitacion accidental durante la comprobacion.",
          SoundIdTheme::accentGreen },
        { "3. Fijar Ganancia de Entrada",
          "Ajusta la ganancia del previo a su nivel operativo habitual.\n"
          "No modifiques este potenciometro tras superar la medicion.",
          SoundIdTheme::accentBlue },
        { "4. Medicion de Suelo (400 ms)",
          "Mide 400 ms de entrada para verificar suelo < -45 dBFS y descartar bucles de feedback.",
          SoundIdTheme::accentGreen }
    } };
}

std::array<InstructionStep, 4> buildSteps2B(const ViewState& view)
{
    const auto& output = view.outputChannelName;
    const auto& input = view.inputChannelName;
    return { {
        { "1. Conectar Cable Patch Main Out -> Input",
          "Conecta un cable patch simple de " + output + " a " + input + ".\n"
          "Ruta fisica directa sin plugins, pedales ni previos intermedios.",
          SoundIdTheme::accentGreen },
        { "2. Direct Monitor Desactivado",
          "Apaga el Direct Monitor o gira el control Mix completamente hacia Playback.\n"
          "Garantiza que la senal capturada pase unicamente por el cable de loopback.",
          SoundIdTheme::accentGreen },
        { "3. Preflight de Seguridad (200 ms)",
          "Antes de emitir el sweep, se verifican 200 ms con salida muteada.\n"
          "Si detecta senal peligrosa (> -6 dBFS), aborta antes de emitir audio.",
          SoundIdTheme::accentAmber },
        { "4. Barrido Farina Contractual (1.300 ms)",
          "1.300 ms de captura (1.000 ms sweep Farina + 200 ms margen RTL + 100 ms cola).\n"
          "Calcula RTL exacta, trim tecnico, respuesta en frecuencia y SNR.",
          SoundIdTheme::accentGreen }
    } };
}

const std::array<InstructionStep, 3> digitalSteps
{
    {
        { "1. Digital Mode Active",
          "No loopback cable required. Analog interface calibration does not apply to plugins or virtual synthesizers.",
          SoundIdTheme::accentBlue },
        { "2. Direct Digital Path",
          "The application will use the digital path without DAC/ADC conversion compensation.",
          SoundIdTheme::accentBlue },
        { "3. Buffer Verification",
          "Verifies that the host and plugin respond at the selected sample rate and block size.",
          SoundIdTheme::accentBlue }
    }
};

template <std::size_t N>
void drawSteps(juce::Graphics& g,
               juce::Rectangle<float>& column,
               const std::array<InstructionStep, N>& steps,
               float rowHeight,
               float descriptionFontHeight,
               float gapAfter)
{
    for (std::size_t i = 0; i < N; ++i)
        drawInstructionStep(g, column, static_cast<int>(i + 1), steps[i].title,
                            steps[i].description, steps[i].accent, rowHeight,
                            descriptionFontHeight, gapAfter);
}

} // namespace

//==============================================================================
void paintDigitalIntro(juce::Graphics& g, juce::Rectangle<float>& content)
{
    g.setFont(font(12.5f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText("Digital Path Verification", content.removeFromTop(18.0f),
               juce::Justification::topLeft, true);
    content.removeFromTop(2.0f);

    g.setFont(font(11.5f));
    g.setColour(SoundIdTheme::textSecondary);
    g.drawText("Direct digital profiling path for virtual instruments and VST3 plugins. No analog conversion required.",
               content.removeFromTop(24.0f), juce::Justification::topLeft, true);
    content.removeFromTop(8.0f);
}

void paintDigitalInstructions(juce::Graphics& g, const ViewState&, juce::Rectangle<float>& leftColumn)
{
    drawSteps(g, leftColumn, digitalSteps, kDigitalRowHeight, 11.0f, 6.0f);
}

void paintDigitalStatusCard(juce::Graphics& g, const ViewState& view, juce::Rectangle<float> rightColumn)
{
    const auto card = rightColumn.withHeight(220.0f);
    drawCardShell(g, card, SoundIdTheme::bgCardHover, SoundIdTheme::borderSubtle,
                  calibrationpanel::kInnerCardCornerRadius, 1.0f);

    auto meterArea = card.reduced(14.0f, 12.0f);

    g.setFont(font(11.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textMuted);
    g.drawText("DIGITAL PATH STATUS", meterArea.removeFromTop(16.0f),
               juce::Justification::centredLeft, true);
    meterArea.removeFromTop(8.0f);

    g.setFont(font(12.0f, juce::Font::bold));
    g.setColour(view.digitalVerified ? SoundIdTheme::accentGreen : SoundIdTheme::accentAmber);
    g.drawText(view.digitalVerified ? "[ PASS ] Digital path verified"
                                    : "[ PENDING ] Digital verification pending",
               meterArea.removeFromTop(20.0f), juce::Justification::centredLeft, true);

    meterArea.removeFromTop(8.0f);
    g.setFont(font(11.0f));
    g.setColour(SoundIdTheme::textSecondary);
    g.drawText("Mode: Digital (VST3 Plugin / Virtual Synth)\n"
               "Analog Latency: 0 ms\n"
               "Nominal Level: 0 dBFS\n"
               "DAC/ADC Conversion: Not required",
               meterArea, juce::Justification::topLeft, true);
}

void paintAnalogInstructions(juce::Graphics& g, const ViewState& view, juce::Rectangle<float>& leftColumn)
{
    const bool is2A = view.subView == SubView::NoiseBaseline_2A;

    auto header = leftColumn.removeFromTop(20.0f);
    g.setFont(font(10.5f, juce::Font::bold));
    g.setColour(SoundIdTheme::textMuted);
    g.drawText(is2A ? "SUBPASO 2A: SUELO DE RUIDO DE ENTRADA (CABLE DESCONECTADO)"
                    : "SUBPASO 2B: MEDICION FISICA DE LOOPBACK (CABLE CONECTADO)",
               header, juce::Justification::centredLeft, true);
    leftColumn.removeFromTop(6.0f);

    drawSteps(g, leftColumn, is2A ? buildSteps2A(view) : buildSteps2B(view),
              kInstructionRowHeight, 10.5f, 4.0f);

    if (is2A)
    {
        if (view.noiseBaselineState == NoiseBaselineState::Passed)
            drawTipBox(g, leftColumn,
                       "Suelo de entrada verificado. Conecta el cable patch y haz clic en la tarjeta 2B arriba.",
                       SoundIdTheme::accentGreen, juce::Font::bold, 0.5f);
    }
    else
    {
        drawTipBox(g, leftColumn,
                   "Do not change cable routing, input gain, sample rate or buffer size after Step 2A has passed.",
                   SoundIdTheme::accentBlue, juce::Font::plain);
    }
}

} // namespace abdaudiolab::gui::calibrationpanel::painter
