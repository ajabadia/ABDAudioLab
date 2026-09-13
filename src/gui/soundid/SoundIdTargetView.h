#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../session/ProfilingSessionContracts.h"

namespace abdaudiolab::gui::soundid
{

/**
 * @brief Vista limpia del Paso 1: Selección de Target, verificación de conexión y dominio.
 */
class SoundIdTargetView : public juce::Component
{
public:
    explicit SoundIdTargetView(session::IProfilingSessionCommands& commands);
    ~SoundIdTargetView() override = default;

    void updateFromSnapshot(const session::ProfilingSessionSnapshot& snapshot);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    session::IProfilingSessionCommands& commands_;

    juce::Label headerTitle_;
    juce::Label headerSubtitle_;

    // Tarjeta del target seleccionado
    juce::GroupComponent targetCard_;
    juce::Label targetNameLabel_;
    juce::Label targetKindLabel_;
    juce::Label connectionStatusLabel_;
    juce::Label domainDescriptionLabel_;
    juce::Label parametersCountLabel_;

    // Controles de acción
    juce::TextButton selectPluginButton_;
    juce::TextButton runAuditButton_;
    juce::TextButton continueButton_;

    bool isConnected_ { false };
    bool isAudited_ { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundIdTargetView)
};

} // namespace abdaudiolab::gui::soundid
