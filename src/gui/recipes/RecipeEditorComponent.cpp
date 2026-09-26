#include "RecipeEditorComponent.h"
#include "../SoundIdTheme.h"

namespace abdaudiolab::gui::recipes
{

RecipeEditorComponent::RecipeEditorComponent()
{
    setupControls();
}

void RecipeEditorComponent::setupControls()
{
    // Nivel
    levelSelector_.onLevelChanged = [this](RecipeEditorView view) {
        if (controller_ != nullptr)
        {
            controller_->setEditorView(view);
            updateFromState();
        }
    };
    addAndMakeVisible(levelSelector_);

    // Badge de estado
    lblStatusBadge_.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblStatusBadge_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(lblStatusBadge_);

    // Botón Restablecer
    btnResetPreset_.onClick = [this]() {
        if (controller_ != nullptr)
        {
            controller_->resetToPreset();
            updateFromState();
            if (onRecipeModified)
                onRecipeModified();
        }
    };
    addAndMakeVisible(btnResetPreset_);

    // Grupo Configurable
    addAndMakeVisible(groupConfigurable_);

    sliderRepetitions_.setRange(1, 10, 1);
    sliderRepetitions_.setSliderStyle(juce::Slider::IncDecButtons);
    sliderRepetitions_.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 40, 20);
    sliderRepetitions_.onValueChange = [this]() { commitConfigurableEdits(); };
    addAndMakeVisible(lblRepetitions_);
    addAndMakeVisible(sliderRepetitions_);

    sliderMidiNote_.setRange(0, 127, 1);
    sliderMidiNote_.setSliderStyle(juce::Slider::LinearBar);
    sliderMidiNote_.onValueChange = [this]() { commitConfigurableEdits(); };
    addAndMakeVisible(lblMidiNote_);
    addAndMakeVisible(sliderMidiNote_);

    sliderGateMs_.setRange(50.0, 2000.0, 10.0);
    sliderGateMs_.setSliderStyle(juce::Slider::LinearBar);
    sliderGateMs_.onValueChange = [this]() { commitConfigurableEdits(); };
    addAndMakeVisible(lblGateMs_);
    addAndMakeVisible(sliderGateMs_);

    sliderSettlingMs_.setRange(10.0, 500.0, 5.0);
    sliderSettlingMs_.setSliderStyle(juce::Slider::LinearBar);
    sliderSettlingMs_.onValueChange = [this]() { commitConfigurableEdits(); };
    addAndMakeVisible(lblSettlingMs_);
    addAndMakeVisible(sliderSettlingMs_);

    comboPointSet_.addItem("3 puntos (0.0, 0.5, 1.0)", 1);
    comboPointSet_.addItem("11 puntos (0.0 .. 1.0)", 2);
    comboPointSet_.onChange = [this]() { commitConfigurableEdits(); };
    addAndMakeVisible(lblPointSet_);
    addAndMakeVisible(comboPointSet_);

    // Grupo Avanzado
    addAndMakeVisible(groupAdvanced_);

    sliderMinSnr_.setRange(20.0, 120.0, 1.0);
    sliderMinSnr_.setSliderStyle(juce::Slider::LinearBar);
    sliderMinSnr_.onValueChange = [this]() { commitAdvancedEdits(); };
    addAndMakeVisible(lblMinSnr_);
    addAndMakeVisible(sliderMinSnr_);

    sliderMaxThd_.setRange(0.001, 20.0, 0.01);
    sliderMaxThd_.setSliderStyle(juce::Slider::LinearBar);
    sliderMaxThd_.onValueChange = [this]() { commitAdvancedEdits(); };
    addAndMakeVisible(lblMaxThd_);
    addAndMakeVisible(sliderMaxThd_);

    sliderF0Tol_.setRange(0.1, 100.0, 0.1);
    sliderF0Tol_.setSliderStyle(juce::Slider::LinearBar);
    sliderF0Tol_.onValueChange = [this]() { commitAdvancedEdits(); };
    addAndMakeVisible(lblF0Tol_);
    addAndMakeVisible(sliderF0Tol_);

    comboCalPolicy_.addItem("Required", 1);
    comboCalPolicy_.addItem("Optional", 2);
    comboCalPolicy_.addItem("None", 3);
    comboCalPolicy_.onChange = [this]() { commitAdvancedEdits(); };
    addAndMakeVisible(lblCalPolicy_);
    addAndMakeVisible(comboCalPolicy_);

    // Diagnósticos
    lblDiagnosticError_.setFont(juce::FontOptions(11.0f, juce::Font::italic));
    lblDiagnosticError_.setColour(juce::Label::textColourId, SoundIdTheme::accentRed);
    addAndMakeVisible(lblDiagnosticError_);
}

void RecipeEditorComponent::setController(RecipeExecutionController* controller)
{
    controller_ = controller;
    updateFromState();
}

void RecipeEditorComponent::updateFromState()
{
    if (controller_ == nullptr)
        return;

    isUpdatingControls_ = true;

    const auto& state = controller_->getCurrentState();
    levelSelector_.setSelectedView(state.activeView);

    // Estado del preset
    if (state.isModifiedFromBase)
    {
        lblStatusBadge_.setText("Ajuste temporal no guardado", juce::dontSendNotification);
        lblStatusBadge_.setColour(juce::Label::textColourId, SoundIdTheme::accentAmber);
        btnResetPreset_.setEnabled(true);
    }
    else
    {
        lblStatusBadge_.setText("Preset original intacto", juce::dontSendNotification);
        lblStatusBadge_.setColour(juce::Label::textColourId, SoundIdTheme::accentGreen);
        btnResetPreset_.setEnabled(false);
    }

    // Diagnóstico
    if (!state.localEditDiagnostics.empty())
    {
        lblDiagnosticError_.setText("Error de validación: " + state.localEditDiagnostics[0].message, juce::dontSendNotification);
    }
    else
    {
        lblDiagnosticError_.setText("", juce::dontSendNotification);
    }

    // Poblar controles desde workingRecipe
    const auto& rec = state.workingRecipe;
    sliderRepetitions_.setValue(rec.excitation.repetitions, juce::dontSendNotification);

    if (!rec.excitation.notes.empty())
    {
        sliderMidiNote_.setValue(rec.excitation.notes[0].midiNote, juce::dontSendNotification);
        sliderGateMs_.setValue(rec.excitation.notes[0].gateMs, juce::dontSendNotification);
        sliderSettlingMs_.setValue(rec.excitation.notes[0].settlingMs, juce::dontSendNotification);
    }

    if (rec.measurement.points.size() <= 3)
        comboPointSet_.setSelectedId(1, juce::dontSendNotification);
    else
        comboPointSet_.setSelectedId(2, juce::dontSendNotification);

    sliderMinSnr_.setValue(rec.evaluationPolicy.minimumSnrDb, juce::dontSendNotification);
    sliderMaxThd_.setValue(rec.evaluationPolicy.maximumThdPercent, juce::dontSendNotification);
    sliderF0Tol_.setValue(rec.evaluationPolicy.f0ToleranceCents, juce::dontSendNotification);

    if (rec.measurement.calibrationPolicy == "Optional")
        comboCalPolicy_.setSelectedId(2, juce::dontSendNotification);
    else if (rec.measurement.calibrationPolicy == "None")
        comboCalPolicy_.setSelectedId(3, juce::dontSendNotification);
    else
        comboCalPolicy_.setSelectedId(1, juce::dontSendNotification);

    // Visibilidad según vista activa
    bool isQuick = (state.activeView == RecipeEditorView::Quick);
    bool isAdv = (state.activeView == RecipeEditorView::Advanced);

    groupConfigurable_.setVisible(!isQuick);
    lblRepetitions_.setVisible(!isQuick);
    sliderRepetitions_.setVisible(!isQuick);
    lblMidiNote_.setVisible(!isQuick);
    sliderMidiNote_.setVisible(!isQuick);
    lblGateMs_.setVisible(!isQuick);
    sliderGateMs_.setVisible(!isQuick);
    lblSettlingMs_.setVisible(!isQuick);
    sliderSettlingMs_.setVisible(!isQuick);
    lblPointSet_.setVisible(!isQuick);
    comboPointSet_.setVisible(!isQuick);

    groupAdvanced_.setVisible(isAdv);
    lblMinSnr_.setVisible(isAdv);
    sliderMinSnr_.setVisible(isAdv);
    lblMaxThd_.setVisible(isAdv);
    sliderMaxThd_.setVisible(isAdv);
    lblF0Tol_.setVisible(isAdv);
    sliderF0Tol_.setVisible(isAdv);
    lblCalPolicy_.setVisible(isAdv);
    comboCalPolicy_.setVisible(isAdv);

    isUpdatingControls_ = false;
    resized();
    repaint();
}

void RecipeEditorComponent::commitConfigurableEdits()
{
    if (isUpdatingControls_ || controller_ == nullptr)
        return;

    int reps = static_cast<int>(sliderRepetitions_.getValue());
    int note = static_cast<int>(sliderMidiNote_.getValue());
    double gate = sliderGateMs_.getValue();
    double settling = sliderSettlingMs_.getValue();

    controller_->updateRepetitions(reps);
    controller_->updateNoteExcitation(note, 0.5, gate, settling);

    // Puntos
    if (comboPointSet_.getSelectedId() == 1)
    {
        std::vector<profiling::MeasurementPointConfig> pts = {
            { "vcf.cutoff", 0.0 }, { "vcf.cutoff", 0.5 }, { "vcf.cutoff", 1.0 }
        };
        controller_->updatePointSet(pts);
    }
    else if (comboPointSet_.getSelectedId() == 2)
    {
        std::vector<profiling::MeasurementPointConfig> pts;
        for (int i = 0; i <= 10; ++i)
            pts.push_back({ "vcf.cutoff", i * 0.1 });
        controller_->updatePointSet(pts);
    }

    updateFromState();
    if (onRecipeModified)
        onRecipeModified();
}

void RecipeEditorComponent::commitAdvancedEdits()
{
    if (isUpdatingControls_ || controller_ == nullptr)
        return;

    double snr = sliderMinSnr_.getValue();
    double thd = sliderMaxThd_.getValue();
    double f0 = sliderF0Tol_.getValue();
    std::string cal = comboCalPolicy_.getText().toStdString();

    controller_->updateEvaluationPolicy(snr, thd, f0);
    controller_->updateCalibrationPolicy(cal);

    updateFromState();
    if (onRecipeModified)
        onRecipeModified();
}

void RecipeEditorComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(SoundIdTheme::bgCard);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(SoundIdTheme::borderCard);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);
}

void RecipeEditorComponent::resized()
{
    auto area = getLocalBounds().reduced(12, 10);

    // Barra superior: Selector de nivel, badge y reset
    auto topBar = area.removeFromTop(28);
    levelSelector_.setBounds(topBar.removeFromLeft(240));
    topBar.removeFromLeft(12);
    btnResetPreset_.setBounds(topBar.removeFromRight(130));
    topBar.removeFromRight(8);
    lblStatusBadge_.setBounds(topBar);

    area.removeFromTop(8);

    if (lblDiagnosticError_.getText().isNotEmpty())
    {
        lblDiagnosticError_.setBounds(area.removeFromTop(18));
        area.removeFromTop(4);
    }

    if (groupConfigurable_.isVisible())
    {
        auto configArea = area.removeFromTop(130);
        groupConfigurable_.setBounds(configArea);
        auto inner = configArea.reduced(8, 18);

        auto r1 = inner.removeFromTop(24);
        lblRepetitions_.setBounds(r1.removeFromLeft(110));
        sliderRepetitions_.setBounds(r1.removeFromLeft(90));
        r1.removeFromLeft(20);
        lblMidiNote_.setBounds(r1.removeFromLeft(80));
        sliderMidiNote_.setBounds(r1);

        inner.removeFromTop(6);
        auto r2 = inner.removeFromTop(24);
        lblGateMs_.setBounds(r2.removeFromLeft(140));
        sliderGateMs_.setBounds(r2.removeFromLeft(100));
        r2.removeFromLeft(20);
        lblSettlingMs_.setBounds(r2.removeFromLeft(140));
        sliderSettlingMs_.setBounds(r2);

        inner.removeFromTop(6);
        auto r3 = inner.removeFromTop(24);
        lblPointSet_.setBounds(r3.removeFromLeft(140));
        comboPointSet_.setBounds(r3.removeFromLeft(220));

        area.removeFromTop(8);
    }

    if (groupAdvanced_.isVisible())
    {
        auto advArea = area.removeFromTop(100);
        groupAdvanced_.setBounds(advArea);
        auto inner = advArea.reduced(8, 18);

        auto r1 = inner.removeFromTop(24);
        lblMinSnr_.setBounds(r1.removeFromLeft(120));
        sliderMinSnr_.setBounds(r1.removeFromLeft(90));
        r1.removeFromLeft(20);
        lblMaxThd_.setBounds(r1.removeFromLeft(110));
        sliderMaxThd_.setBounds(r1);

        inner.removeFromTop(6);
        auto r2 = inner.removeFromTop(24);
        lblF0Tol_.setBounds(r2.removeFromLeft(140));
        sliderF0Tol_.setBounds(r2.removeFromLeft(90));
        r2.removeFromLeft(20);
        lblCalPolicy_.setBounds(r2.removeFromLeft(90));
        comboCalPolicy_.setBounds(r2.removeFromLeft(120));
    }
}

} // namespace abdaudiolab::gui::recipes
