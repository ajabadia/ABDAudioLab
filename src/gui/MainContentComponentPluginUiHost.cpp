/**
 * @file MainContentComponentPluginUiHost.cpp
 * @brief IPluginUiHost implementation: VST3 hosting and presentation port.
 * @author ABDSynths
 * @date 2026
 */

#include "MainContentComponent.h"

namespace abdaudiolab
{

// ==============================================================================
// POR QUE ESTO ESTA EN UN FICHERO PROPIO Y NO EN MainContentComponent.cpp.
//
// Porque el fichero eran 3925 lineas con 80 metodos. En un fichero asi el numero
// de un metodo no dice nada: hay que recorrerlo entero para saber si esta en esta
// seccion o en la siguiente. Leer no es el problema, modificar si: cambiar diez
// lineas de un metodo obliga a recorrer 4000 lineas, y el metodo que toca acaba
// en el sitio menos probable de donde estabas mirando.
//
// No se ha movido ni una linea de codigo: los mismos cuerpos de funcion, en la
// misma clase, enlazados igual. Lo unico que cambia es el fichero donde viven, y
// eso se comprueba compilando.
// ==============================================================================

// ==============================================================================
// SECTION 2: IPluginUiHost IMPLEMENTATION (VST3 HOSTING & PRESENTATION PORT)
// ==============================================================================

void MainContentComponent::updatePluginLoadingState(bool isSuccess, const juce::String& message)
{
    juce::Logger::writeToLog("[PluginUiHost] LoadingState: " + juce::String(isSuccess ? "Success" : "Failed") + " - " + message);
    if (!isSuccess && message.isNotEmpty())
    {
        manualPromptLabel.setText(message, juce::dontSendNotification);
        manualPromptLabel.setVisible(true);
        hidePromptAfterDelay(5000);
    }
}

void MainContentComponent::updatePluginIdentity(const gui::PluginIdentityPresentation& identity,
                                                const juce::PluginDescription& description)
{
    drawer.setContracts(hardwareManager.getContractRegistry().getContracts());

    // Los retenidos van con los contratos: si se refresca uno, se refrescan
    // los dos, o el cajon se queda con una lista y un recuento que no coinciden.
    drawer.setQuarantinedProfiles(hardwareManager.getContractRegistry().getQuarantinedProfiles());
    drawer.setSelectedHardwareId(identity.legalTargetId);


    suiteList.setStandardTestAvailable(true);

    auto imgFile = gui::locateAssetFile(identity.modelAssetPath);
    juce::Image pluginImg;
    if (imgFile.existsAsFile())
        pluginImg = juce::ImageFileFormat::loadFrom(imgFile);

    mainHeader.setHardwareInfo(
        identity.titleBadge,
        identity.busDescription,
        pluginImg,
        gui::HardwareConnectionStatus::Connected
    );

    setupInfoTab.setTargetHardwareInfo(
        identity.titleBadge,
        identity.busDescription,
        "Internal Digital Bus (Zero Converter Coloration)",
        pluginImg,
        nullptr,
        identity.category
    );
    drawer.getSetupTab().setTargetHardwareInfo(
        identity.titleBadge,
        identity.busDescription,
        "Internal Digital Bus (Zero Converter Coloration)",
        pluginImg,
        nullptr,
        identity.category
    );
    updateSetupDrawerInfo();

    auto summary = sidebarStepper.getSessionSummary();
    summary.hardwareName = identity.titleBadge;
    summary.hardwareCategory = identity.category;
    sidebarStepper.setSessionSummary(summary);

    // Sincronizar TargetSelectionState con ProfilingSessionController y SoundIdTargetView
    gui::session::TargetSelectionState target;
    target.targetId = identity.legalTargetId.toStdString();
    target.targetName = identity.titleBadge.toStdString();
    target.manufacturer = description.manufacturerName.isNotEmpty() ? description.manufacturerName.toStdString() : "Digital Suburban";
    target.version = description.version.isNotEmpty() ? description.version.toStdString() : "1.0.0";
    target.kind = gui::session::TargetKind::PluginVST3;
    target.isConnected = true;
    target.isDeterministic = true;
    target.availableDomainDescription = identity.busDescription.toStdString();
    if (auto* plugin = pluginUiCoordinator.getActivePluginInstance())
        target.parameterCount = plugin->getNumParameters();
    else
        target.parameterCount = 0;

    profilingSessionController.selectTarget(target);
    profilingSessionController.updateAuditResult(
        synth::ApprovalStatus::Approved,
        "100% Determinista (Digital Host VST3)",
        "Reset de ciclo instantaneo",
        0.0,
        false,
        {},
        "Plugin cargado y validado en bus digital interno");

    targetView.updateFromSnapshot(profilingSessionController.getCurrentSnapshot());
}

void MainContentComponent::showPluginError(const juce::String& title, const juce::String& message)
{
    juce::Logger::writeToLog("[PluginUiHost ERROR] " + title + ": " + message);
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, title, message);
}

void MainContentComponent::notifyPluginUnloaded()
{
    suiteList.setStandardTestAvailable(false);
    mainHeader.setHardwareInfo(
        "Ninguno",
        "Sin hardware seleccionado",
        {},
        gui::HardwareConnectionStatus::Disconnected
    );
    auto summary = sidebarStepper.getSessionSummary();
    if (summary.hardwareCategory == "PLUGIN_VIRTUAL")
    {
        summary.hardwareName = "";
        summary.hardwareCategory = "";
        sidebarStepper.setSessionSummary(summary);
    }
}

} // namespace abdaudiolab
