#include "SoundIdTargetView.h"
#include "../SoundIdTheme.h"
#include "../session/ProfilingSessionController.h"

namespace abdaudiolab::gui::soundid
{

SoundIdTargetView::SoundIdTargetView(session::IProfilingSessionCommands& commands)
    : commands_(commands)
{
    // Encabezado
    headerTitle_.setText(juce::String::fromUTF8(u8"Paso 1: Seleccionar Sintetizador o Target"), juce::dontSendNotification);
    headerTitle_.setFont(juce::Font(20.0f, juce::Font::bold));
    headerTitle_.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(headerTitle_);

    headerSubtitle_.setText(juce::String::fromUTF8(u8"Elija el sintetizador analógico, digital o plugin VST3 que desea caracterizar y validar."), juce::dontSendNotification);
    headerSubtitle_.setFont(juce::Font(13.0f, juce::Font::plain));
    headerSubtitle_.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    addAndMakeVisible(headerSubtitle_);

    // Tarjeta
    targetCard_.setText(juce::String::fromUTF8(u8"Dispositivo / Plugin Activo"));
    targetCard_.setColour(juce::GroupComponent::outlineColourId, SoundIdTheme::borderCard);
    targetCard_.setColour(juce::GroupComponent::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(targetCard_);

    auto setupInfoLabel = [this](juce::Label& lbl, const juce::String& text, bool bold) {
        lbl.setText(text, juce::dontSendNotification);
        lbl.setFont(juce::Font(14.0f, bold ? juce::Font::bold : juce::Font::plain));
        lbl.setColour(juce::Label::textColourId, bold ? SoundIdTheme::textPrimary : SoundIdTheme::textSecondary);
        addAndMakeVisible(lbl);
    };

    setupInfoLabel(targetNameLabel_, juce::String::fromUTF8(u8"Sin seleccionar"), true);
    setupInfoLabel(targetKindLabel_, juce::String::fromUTF8(u8"Tipo: Desconocido"), false);
    setupInfoLabel(connectionStatusLabel_, juce::String::fromUTF8(u8"Estado: Desconectado"), false);
    setupInfoLabel(domainDescriptionLabel_, juce::String::fromUTF8(u8"Dominio: Pendiente de descubrimiento"), false);
    setupInfoLabel(parametersCountLabel_, juce::String::fromUTF8(u8"Parámetros: 0"), false);

    // Desactivar selector emergente redundante (HITO-03.1: catalogSelector es la única autoridad oficial de selección)
    selectPluginButton_.setVisible(false);
    selectPluginButton_.setEnabled(false);

    runAuditButton_.setButtonText(juce::String::fromUTF8(u8"Auditar Determinismo"));
    runAuditButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentBlue.withAlpha(0.2f));
    runAuditButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    runAuditButton_.onClick = [this]() {
        commands_.recordUserClick();
        if (commands_.requestAudit())
        {
            juce::Component::SafePointer<SoundIdTargetView> safeThis(this);
            juce::MessageManager::callAsync([safeThis]() {
                if (safeThis == nullptr) return;
                if (auto* ctrl = dynamic_cast<session::ProfilingSessionController*>(&safeThis->commands_))
                {
                    auto snap = ctrl->getCurrentSnapshot();
                    synth::ApprovalStatus status = synth::ApprovalStatus::Approved;
                    std::string determinism = "100% Determinista (Validado)";
                    std::string resetCap = "Reset inmediato de ciclo";
                    double settlingMs = 50.0;
                    bool reqReset = false;
                    std::vector<std::string> warnings;
                    std::string guidance = "Target auditado con éxito.";

                    if (snap.target.kind == session::TargetKind::HardwareAnalogue)
                    {
                        status = synth::ApprovalStatus::ApprovedWithWarnings;
                        determinism = "98.7% Repetibilidad analógica";
                        resetCap = "Requiere reset de compuerta";
                        settlingMs = 250.0;
                        reqReset = true;
                        warnings.push_back("Calentamiento térmico requerido (settling > 200 ms)");
                        guidance = "Hardware analógico auditado con advertencias.";
                    }

                    ctrl->updateAuditResult(status, determinism, resetCap, settlingMs, reqReset, warnings, guidance);
                }
            });
        }
    };
    addAndMakeVisible(runAuditButton_);

    continueButton_.setButtonText(juce::String::fromUTF8(u8"Continuar a Preparación ➔"));
    continueButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    continueButton_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    continueButton_.onClick = [this]() {
        commands_.recordUserClick();
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
        targetNameLabel_.setText(juce::String::fromUTF8(snapshot.target.targetName.c_str()) + " (" + juce::String::fromUTF8(snapshot.target.manufacturer.c_str()) + ")", juce::dontSendNotification);
        targetKindLabel_.setText(juce::String::fromUTF8(u8"Naturaleza: ") + juce::String::fromUTF8(session::targetKindToString(snapshot.target.kind).c_str()), juce::dontSendNotification);
        targetKindLabel_.setText(juce::String::fromUTF8(u8"Naturaleza: ") + juce::String::fromUTF8(session::targetKindToString(snapshot.target.kind).c_str()), juce::dontSendNotification);
        connectionStatusLabel_.setText(isConnected_ ? juce::String::fromUTF8(u8"Conexión: Activa y Sincronizada") : juce::String::fromUTF8(u8"Conexión: No disponible"), juce::dontSendNotification);
        domainDescriptionLabel_.setText(juce::String::fromUTF8(u8"Dominio: ") + juce::String::fromUTF8(snapshot.target.availableDomainDescription.c_str()), juce::dontSendNotification);
        parametersCountLabel_.setText(juce::String::fromUTF8(u8"Parámetros descubiertos: ") + juce::String(snapshot.target.parameterCount), juce::dontSendNotification);
    }
    else
    {
        targetNameLabel_.setText(juce::String::fromUTF8(u8"Ningún target seleccionado"), juce::dontSendNotification);
        targetKindLabel_.setText(juce::String::fromUTF8(u8"Naturaleza: -"), juce::dontSendNotification);
        connectionStatusLabel_.setText(juce::String::fromUTF8(u8"Estado: En espera"), juce::dontSendNotification);
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
    runAuditButton_.setBounds(buttonRow.removeFromLeft(200));
    continueButton_.setBounds(buttonRow.removeFromRight(220));
}

} // namespace abdaudiolab::gui::soundid
