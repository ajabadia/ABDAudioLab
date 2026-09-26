#include "RecipeSummaryCardComponent.h"
#include "../SoundIdTheme.h"
#include <sstream>
#include <iomanip>

namespace abdaudiolab::gui::recipes
{

namespace
{

std::string truncateHash(const std::string& hash)
{
    if (hash.size() <= 12)
        return hash;
    return hash.substr(0, 8) + "..." + hash.substr(hash.size() - 4);
}

} // namespace

RecipeSummaryCardComponent::RecipeSummaryCardComponent()
{
    lblTitle_.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    lblTitle_.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(lblTitle_);

    lblAssistanceBadge_.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblAssistanceBadge_.setColour(juce::Label::textColourId, SoundIdTheme::accentBlue);
    addAndMakeVisible(lblAssistanceBadge_);

    lblRecipeId_.setFont(juce::FontOptions(10.5f, juce::Font::plain));
    lblRecipeId_.setColour(juce::Label::textColourId, SoundIdTheme::textMuted);
    addAndMakeVisible(lblRecipeId_);

    lblExcitationSummary_.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    lblExcitationSummary_.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(lblExcitationSummary_);

    lblMeasurementSummary_.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    lblMeasurementSummary_.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(lblMeasurementSummary_);

    lblEnvironmentSummary_.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    lblEnvironmentSummary_.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    addAndMakeVisible(lblEnvironmentSummary_);

    lblEvaluationSummary_.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    lblEvaluationSummary_.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    addAndMakeVisible(lblEvaluationSummary_);

    lblDocumentHash_.setFont(juce::FontOptions(10.0f, juce::Font::plain));
    lblDocumentHash_.setColour(juce::Label::textColourId, SoundIdTheme::textMuted);
    addAndMakeVisible(lblDocumentHash_);

    lblPlanHash_.setFont(juce::FontOptions(10.0f, juce::Font::plain));
    lblPlanHash_.setColour(juce::Label::textColourId, SoundIdTheme::textMuted);
    addAndMakeVisible(lblPlanHash_);

    lblCompatibilityBanner_.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    addAndMakeVisible(lblCompatibilityBanner_);

    clear();
}

void RecipeSummaryCardComponent::setRecipe(const profiling::MeasurementRecipe& recipe,
                                          const std::string& recipeDocumentHash,
                                          const std::optional<std::string>& experimentPlanHash)
{
    currentRecipe_ = recipe;
    recipeDocumentHash_ = recipeDocumentHash;
    experimentPlanHash_ = experimentPlanHash;
    resolutionResult_ = std::nullopt;
    updateLabels();
    repaint();
}

void RecipeSummaryCardComponent::setResolutionStatus(const profiling::ResolveExecutionPlanResult& resolveResult)
{
    resolutionResult_ = resolveResult;
    updateLabels();
    repaint();
}

void RecipeSummaryCardComponent::clear()
{
    currentRecipe_ = std::nullopt;
    recipeDocumentHash_.clear();
    experimentPlanHash_ = std::nullopt;
    resolutionResult_ = std::nullopt;

    lblTitle_.setText("Ninguna receta seleccionada", juce::dontSendNotification);
    lblAssistanceBadge_.setText({}, juce::dontSendNotification);
    lblRecipeId_.setText({}, juce::dontSendNotification);
    lblExcitationSummary_.setText("Seleccione una receta del catalogo para inspeccionar sus parametros.", juce::dontSendNotification);
    lblMeasurementSummary_.setText({}, juce::dontSendNotification);
    lblEnvironmentSummary_.setText({}, juce::dontSendNotification);
    lblEvaluationSummary_.setText({}, juce::dontSendNotification);
    lblDocumentHash_.setText({}, juce::dontSendNotification);
    lblPlanHash_.setText({}, juce::dontSendNotification);
    lblCompatibilityBanner_.setText({}, juce::dontSendNotification);
    repaint();
}

void RecipeSummaryCardComponent::updateLabels()
{
    if (!currentRecipe_.has_value())
        return;

    const auto& r = *currentRecipe_;

    lblTitle_.setText(juce::String::fromUTF8(r.displayName.c_str()), juce::dontSendNotification);
    lblAssistanceBadge_.setText("[" + juce::String(profiling::assistanceLevelToString(r.assistanceLevel)) + "]", juce::dontSendNotification);
    lblRecipeId_.setText("ID: " + juce::String(r.recipeId) + " (rev " + juce::String(r.revision) + ")", juce::dontSendNotification);

    // Resumen de Excitación
    juce::String excText = "Excitacion: ";
    if (!r.excitation.notes.empty())
    {
        const auto& n0 = r.excitation.notes[0];
        excText += "Nota " + juce::String(n0.midiNote) + " (vel " + juce::String(n0.velocity, 2)
                 + ", gate " + juce::String(static_cast<int>(n0.gateMs)) + " ms, settling "
                 + juce::String(static_cast<int>(n0.settlingMs)) + " ms)";
        if (r.excitation.notes.size() > 1)
        {
            excText += " [+ " + juce::String(r.excitation.notes.size() - 1) + " notas]";
        }
    }
    excText += " x " + juce::String(r.excitation.repetitions) + " reps";
    lblExcitationSummary_.setText(excText, juce::dontSendNotification);

    // Resumen de Medición
    juce::String measText = "Puntos de medicion: " + juce::String(r.measurement.points.size()) + " pts";
    if (!r.measurement.points.empty())
    {
        measText += " (" + juce::String(r.measurement.points[0].parameter);
        if (r.measurement.points.size() <= 4)
        {
            measText += ": ";
            for (size_t i = 0; i < r.measurement.points.size(); ++i)
            {
                if (i > 0) measText += ", ";
                measText += juce::String(r.measurement.points[i].normalizedValue, 2);
            }
        }
        measText += ")";
    }
    measText += " | Calibracion: " + juce::String(r.measurement.calibrationPolicy);
    lblMeasurementSummary_.setText(measText, juce::dontSendNotification);

    // Requisitos de Entorno
    juce::String envText = "Requisitos: " + juce::String(r.targetConstraints.channels) + " can.";
    if (!r.targetConstraints.allowedSampleRatesHz.empty())
    {
        envText += " | Sample rates: ";
        for (size_t i = 0; i < r.targetConstraints.allowedSampleRatesHz.size(); ++i)
        {
            if (i > 0) envText += ", ";
            envText += juce::String(r.targetConstraints.allowedSampleRatesHz[i]) + " Hz";
        }
    }
    lblEnvironmentSummary_.setText(envText, juce::dontSendNotification);

    // Evaluación
    juce::String evalText = "Evaluacion: SNR >= " + juce::String(r.evaluationPolicy.minimumSnrDb, 1) + " dB"
                          + " | THD <= " + juce::String(r.evaluationPolicy.maximumThdPercent, 2) + " %"
                          + " | f0 +/- " + juce::String(r.evaluationPolicy.f0ToleranceCents, 1) + " c";
    lblEvaluationSummary_.setText(evalText, juce::dontSendNotification);

    // Hashes
    lblDocumentHash_.setText("recipeDocumentHash: " + juce::String(truncateHash(recipeDocumentHash_)), juce::dontSendNotification);
    if (experimentPlanHash_.has_value())
    {
        lblPlanHash_.setText("experimentPlanHash: " + juce::String(truncateHash(*experimentPlanHash_)), juce::dontSendNotification);
    }
    else
    {
        lblPlanHash_.setText("experimentPlanHash: [pendiente de compilacion]", juce::dontSendNotification);
    }

    // Banner de Compatibilidad
    if (resolutionResult_.has_value())
    {
        if (resolutionResult_->succeeded())
        {
            lblCompatibilityBanner_.setText("Compatible con el target actual", juce::dontSendNotification);
            lblCompatibilityBanner_.setColour(juce::Label::textColourId, SoundIdTheme::accentGreen);
        }
        else
        {
            juce::String errStr = "Incompatible: ";
            if (!resolutionResult_->diagnostics.empty())
                errStr += juce::String::fromUTF8(resolutionResult_->diagnostics[0].message.c_str());
            lblCompatibilityBanner_.setText(errStr, juce::dontSendNotification);
            lblCompatibilityBanner_.setColour(juce::Label::textColourId, SoundIdTheme::accentRed);
        }
    }
    else
    {
        lblCompatibilityBanner_.setText("Entorno pendiente de evaluacion", juce::dontSendNotification);
        lblCompatibilityBanner_.setColour(juce::Label::textColourId, SoundIdTheme::textMuted);
    }
}

void RecipeSummaryCardComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Fondo de tarjeta con bordes redondeados y estilo SoundID
    g.setColour(SoundIdTheme::bgCard);
    g.fillRoundedRectangle(bounds, 6.0f);

    g.setColour(SoundIdTheme::borderCard);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);
}

void RecipeSummaryCardComponent::resized()
{
    auto area = getLocalBounds().reduced(12, 10);

    // Cabecera: Título y Badge
    auto headerArea = area.removeFromTop(22);
    lblAssistanceBadge_.setBounds(headerArea.removeFromRight(90));
    lblTitle_.setBounds(headerArea);

    lblRecipeId_.setBounds(area.removeFromTop(16));
    area.removeFromTop(4);

    lblExcitationSummary_.setBounds(area.removeFromTop(18));
    lblMeasurementSummary_.setBounds(area.removeFromTop(18));
    lblEnvironmentSummary_.setBounds(area.removeFromTop(18));
    lblEvaluationSummary_.setBounds(area.removeFromTop(18));

    area.removeFromTop(6);
    lblDocumentHash_.setBounds(area.removeFromTop(14));
    lblPlanHash_.setBounds(area.removeFromTop(14));

    area.removeFromTop(6);
    lblCompatibilityBanner_.setBounds(area.removeFromTop(20));
}

} // namespace abdaudiolab::gui::recipes
