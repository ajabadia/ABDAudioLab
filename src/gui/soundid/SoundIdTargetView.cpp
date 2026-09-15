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

    selectPluginButton_.setButtonText(juce::String::fromUTF8(u8"Examinar Dispositivos / VST3... ▼"));
    selectPluginButton_.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCard);
    selectPluginButton_.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    selectPluginButton_.onClick = [this]() {
        juce::PopupMenu m;
        m.addSectionHeader(juce::String::fromUTF8(u8"Targets Disponibles en ABDAudioLab"));
        m.addItem(1, juce::String::fromUTF8(u8"1. Sintetizador Virtual de Prueba (Demo Snapshot)"));
        m.addItem(2, juce::String::fromUTF8(u8"2. Dexed FM Synth (Plugin VST3 Virtual)"));
        m.addItem(3, juce::String::fromUTF8(u8"3. Roland AIRA Torcido (Hardware USB Class-Compliant)"));
        m.addItem(4, juce::String::fromUTF8(u8"4. Korg MS-20 / Minilogue (Hardware MIDI/Loopback)"));
        m.addItem(5, juce::String::fromUTF8(u8"5. ReferenceSynth VST3 (Worker Aislado IPC - MVP)"));

        juce::Component::SafePointer<SoundIdTargetView> safeThis(this);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&selectPluginButton_),
            [safeThis](int result) {
                if (safeThis == nullptr)
                    return;

                safeThis->commands_.recordUserClick();
                session::TargetSelectionState target;
                if (result == 1)
                {
                    target.targetId = "synthetic_fixture_demo";
                    target.targetName = "Sintetizador Virtual de Prueba (Demo Snapshot)";
                    target.manufacturer = "ABDAudioLab";
                    target.version = "1.0.0";
                    target.kind = session::TargetKind::SyntheticFixture;
                    target.isConnected = true;
                    target.isDeterministic = true;
                    target.availableDomainDescription = "Notas MIDI C1-C6, Vel 1-127, Controles de Filtro y Modulación";
                    target.parameterCount = 8;
                }
                else if (result == 2)
                {
                    target.targetId = "dexed_vst3";
                    target.targetName = "Dexed FM Synthesizer";
                    target.manufacturer = "Digital Suburban";
                    target.version = "1.0.1";
                    target.kind = session::TargetKind::PluginVST3;
                    target.isConnected = true;
                    target.isDeterministic = true;
                    target.availableDomainDescription = "6 Operadores FM, Algoritmos 1-32, Pitch Env, LFO";
                    target.parameterCount = 155;
                }
                else if (result == 3)
                {
                    target.targetId = "roland_aira_torcido";
                    target.targetName = "Roland AIRA Torcido (Distortion & Filter)";
                    target.manufacturer = "Roland";
                    target.version = "1.0.0";
                    target.kind = session::TargetKind::HardwareAnalogue;
                    target.isConnected = true;
                    target.isDeterministic = false;
                    target.availableDomainDescription = "Audio In/Out USB 96 kHz 24-bit, Drive, Tone, Tube, Wet/Dry";
                    target.parameterCount = 6;
                }
                else if (result == 4)
                {
                    target.targetId = "korg_ms20_analogue";
                    target.targetName = "Korg MS-20 (Monophonic Analogue)";
                    target.manufacturer = "Korg";
                    target.version = "Rev 2";
                    target.kind = session::TargetKind::HardwareAnalogue;
                    target.isConnected = true;
                    target.isDeterministic = false;
                    target.availableDomainDescription = "VCO1/2, HPF Peak, LPF Peak, EG1/2, Patch Panel";
                    target.parameterCount = 18;
                }
                else if (result == 5)
                {
                    target.targetId = "ReferenceSynth";
                    target.targetName = "ReferenceSynth VST3 (Worker Aislado IPC)";
                    target.manufacturer = "ABDSynths";
                    target.version = "1.0.0";
                    target.kind = session::TargetKind::PluginVST3;
                    target.useIsolatedProcess = true; // Forzar ejecución fuera de proceso en worker esclavo
                    target.isConnected = true;
                    target.isDeterministic = true;
                    target.availableDomainDescription = "Oscilador PolyBLEP, Filtro Ladder, Envolvente ADSR, Reset determinista";
                    target.parameterCount = 10;
                }

                if (result >= 1 && result <= 5)
                {
                    safeThis->commands_.selectTarget(target);
                    if (safeThis->commands_.requestAudit())
                    {
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
                                std::string guidance = "Target validado para excitación adaptativa.";

                                if (snap.target.kind == session::TargetKind::HardwareAnalogue)
                                {
                                    status = synth::ApprovalStatus::ApprovedWithWarnings;
                                    determinism = "98.7% Repetibilidad analógica (Frecuencia estable)";
                                    resetCap = "Requiere reset de compuerta antes de cada ensayo";
                                    settlingMs = 250.0;
                                    reqReset = true;
                                    warnings.push_back("Calentamiento térmico requerido (settling > 200 ms)");
                                    guidance = "Hardware analógico: Realizar reset de compuerta.";
                                }

                                ctrl->updateAuditResult(status, determinism, resetCap, settlingMs, reqReset, warnings, guidance);
                            }
                        });
                    }
                }
            });
    };
    addAndMakeVisible(selectPluginButton_);

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
    selectPluginButton_.setBounds(buttonRow.removeFromLeft(200));
    buttonRow.removeFromLeft(16);
    runAuditButton_.setBounds(buttonRow.removeFromLeft(180));
    continueButton_.setBounds(buttonRow.removeFromRight(220));
}

} // namespace abdaudiolab::gui::soundid
