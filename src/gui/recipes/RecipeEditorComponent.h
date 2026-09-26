#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include "RecipeAssistanceLevelSelectorComponent.h"
#include "RecipeExecutionController.h"

namespace abdaudiolab::gui::recipes
{

/**
 * @class RecipeEditorComponent
 * @brief Editor delimitado de recetas según el nivel de asistencia activo (Quick, Configurable, Advanced).
 * Aplica ediciones exclusivamente sobre WorkingRecipe en memoria, preservando RecipeBase intacta.
 */
class RecipeEditorComponent : public juce::Component
{
public:
    RecipeEditorComponent();
    ~RecipeEditorComponent() override = default;

    void setController(RecipeExecutionController* controller);
    void updateFromState();

    [[nodiscard]] RecipeExecutionController* getController() const noexcept { return controller_; }
    [[nodiscard]] bool showsWorkingRecipe() const noexcept
    {
        return controller_ != nullptr && controller_->isRecipeSelected();
    }
    [[nodiscard]] RecipeEditorView editorView() const noexcept
    {
        return controller_ != nullptr ? controller_->getEditorView() : RecipeEditorView::Quick;
    }

    void paint(juce::Graphics& g) override;
    void resized() override;

    std::function<void()> onRecipeModified;

private:
    void setupControls();
    void commitConfigurableEdits();
    void commitAdvancedEdits();

    RecipeExecutionController* controller_ { nullptr };

    // Cabecera: Selector de Nivel y Estado
    RecipeAssistanceLevelSelectorComponent levelSelector_;
    juce::Label lblStatusBadge_;
    juce::TextButton btnResetPreset_ { "Restablecer preset" };

    // Controles de Modo Configurable
    juce::GroupComponent groupConfigurable_ { "groupConfig", "Ajustes Operativos Seguros (Configurable)" };
    juce::Label lblRepetitions_ { {}, "Repeticiones:" };
    juce::Slider sliderRepetitions_;

    juce::Label lblMidiNote_ { {}, "Nota MIDI:" };
    juce::Slider sliderMidiNote_;

    juce::Label lblGateMs_ { {}, "Compuerta (gate ms):" };
    juce::Slider sliderGateMs_;

    juce::Label lblSettlingMs_ { {}, "Reposo (settling ms):" };
    juce::Slider sliderSettlingMs_;

    juce::Label lblPointSet_ { {}, "Conjunto de Puntos:" };
    juce::ComboBox comboPointSet_;

    // Controles de Modo Avanzado
    juce::GroupComponent groupAdvanced_ { "groupAdv", "Políticas Científicas y Tolerancias (Avanzado)" };
    juce::Label lblMinSnr_ { {}, "SNR Mínimo (dB):" };
    juce::Slider sliderMinSnr_;

    juce::Label lblMaxThd_ { {}, "THD Máximo (%):" };
    juce::Slider sliderMaxThd_;

    juce::Label lblF0Tol_ { {}, "Tolerancia f0 (cents):" };
    juce::Slider sliderF0Tol_;

    juce::Label lblCalPolicy_ { {}, "Calibración:" };
    juce::ComboBox comboCalPolicy_;

    // Notificación de diagnósticos locales
    juce::Label lblDiagnosticError_;

    bool isUpdatingControls_ { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecipeEditorComponent)
};

} // namespace abdaudiolab::gui::recipes
