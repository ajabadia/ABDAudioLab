#include "SoundIdTargetView.h"
#include "../SoundIdTheme.h"

namespace abdaudiolab::gui::soundid
{

SoundIdTargetView::SoundIdTargetView(session::IProfilingSessionCommands& commands)
    : commands_(commands)
{
    // Encabezado
    headerTitle_.setText("Paso 1: Seleccionar Sintetizador o Target", juce::dontSendNotification);
    headerTitle_.setFont(juce::Font(20.0f, juce::Font::bold));
    headerTitle_.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(headerTitle_);

    headerSubtitle_.setText("Elija el sintetizador analógico, digital o plugin VST3 que desea caracterizar y validar.", juce::dontSendNotification);
    headerSubtitle_.setFont(juce::Font(13.0f, juce::Font::plain));
    headerSubtitle_.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    addAndMakeVisible(headerSubtitle_);

    // Tarjeta
    targetCard_.setText("Dispositivo / Plugin Activo");
    targetCard_.setColour(juce::GroupComponent::outlineColourId, SoundIdTheme::borderCard);
    targetCard_.setColour(juce::GroupComponent::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(targetCard_);

    auto setupInfoLabel = [this](juce::Label& lbl, const std::string& text, bool bold) {
        lbl.setText(text, juce::dontSendNotification);
        lbl.setFont(juce::Font(14.0f, bold ? juce::Font::bold : juce::Font::plain));
        lbl.setColour(juce::Label::textColourId, bold ? SoundIdTheme::textPrimary : SoundIdTheme::textSecondary);
        addAndMakeVisible(lbl);
    };

    setupInfoLabel(targetNameLabel_, "Sin seleccionar", true);
    setupInfoLabel(targetKindLabel_, "Tipo: Desconocido", false);
    setupInfoLabel(connectionStatusLabel_, "Estado: Desconectado", false);
    setupInfoLabel(domainDescriptionLabel_, "Dominio: Pendiente de descubrimiento", false);
    setupInfoLabel(parametersCountLabel_, "Parámetros: 0", false);

    // Botones
    selectPluginButton_.setButtonText("Examinar Plugins VST3...");
    selectPluginButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCard);
    selectPluginButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    addAndMakeVisible(selectPluginButton_);

    runAuditButton_.setButtonText("Auditar Determinismo");
    runAuditButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentBlue.withAlpha(0.2f));
    runAuditButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    runAuditButton_.onClick = [this]() {
        commands_.requestAudit();
    };
    addAndMakeVisible(runAuditButton_);

    continueButton_.setButtonText("Continuar a Preparación ->");
    continueButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    continueButton_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    continueButton_.onClick = [this]() {
        commands_.navigateToStage(session::ProfilingWorkflowStage::ConfigureAndStart);
    };
    addAndMakeVisible(continueButton_);
}

void SoundIdTargetView::updateFromSnapshot(const session::ProfilingSessionSnapshot& snapshot)
{
    isConnected_ = snapshot.target.isConnected;
    isAudited_ = snapshot.audit.isAudited;

    if (!snapshot.target.targetName.empty())
    {
        targetNameLabel_.setText(snapshot.target.targetName + " (" + snapshot.target.manufacturer + ")", juce::dontSendNotification);
        targetKindLabel_.setText("Naturaleza: " + session::targetKindToString(snapshot.target.kind), juce::dontSendNotification);
        connectionStatusLabel_.setText(isConnected_ ? "Conexión: Activa y Sincronizada" : "Conexión: No disponible", juce::dontSendNotification);
        domainDescriptionLabel_.setText("Dominio: " + snapshot.target.availableDomainDescription, juce::dontSendNotification);
        parametersCountLabel_.setText("Parámetros descubiertos: " + std::to_string(snapshot.target.parameterCount), juce::dontSendNotification);
    }
    else
    {
        targetNameLabel_.setText("Ningún target seleccionado", juce::dontSendNotification);
        targetKindLabel_.setText("Naturaleza: -", juce::dontSendNotification);
        connectionStatusLabel_.setText("Estado: En espera", juce::dontSendNotification);
    }

    runAuditButton_.setEnabled(isConnected_);
    continueButton_.setEnabled(isConnected_ && isAudited_);

    repaint();
}

void SoundIdTargetView::paint(juce::Graphics& g)
{
    g.fillAll(SoundIdTheme::bgLight);
}

void SoundIdTargetView::resized()
{
    auto area = getLocalBounds().reduced(24);

    headerTitle_.setBounds(area.removeFromTop(28));
    headerSubtitle_.setBounds(area.removeFromTop(22));
    area.removeFromTop(16);

    // Tarjeta central
    auto cardArea = area.removeFromTop(200);
    targetCard_.setBounds(cardArea);

    auto cardInner = cardArea.reduced(16, 24);
    targetNameLabel_.setBounds(cardInner.removeFromTop(24));
    targetKindLabel_.setBounds(cardInner.removeFromTop(20));
    connectionStatusLabel_.setBounds(cardInner.removeFromTop(20));
    domainDescriptionLabel_.setBounds(cardInner.removeFromTop(20));
    parametersCountLabel_.setBounds(cardInner.removeFromTop(20));

    area.removeFromTop(24);

    // Fila de botones de acción
    auto buttonRow = area.removeFromTop(40);
    selectPluginButton_.setBounds(buttonRow.removeFromLeft(200));
    buttonRow.removeFromLeft(16);
    runAuditButton_.setBounds(buttonRow.removeFromLeft(180));
    continueButton_.setBounds(buttonRow.removeFromRight(220));
}

} // namespace abdaudiolab::gui::soundid
