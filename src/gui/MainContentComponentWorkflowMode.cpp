/**
 * @file MainContentComponentWorkflowMode.cpp
 * @brief Workflow mode switching, fixture preload, and the startup warnings panel.
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
// SECTION 5 (AUXILIARY IMPLEMENTATION): WORKFLOW MODE SWITCHING & FIXTURE PRELOAD
// Owns switching visibility between 3-step Guided container and Lab Bench surfaces.
// Guided workflow state machine delegated to ProfilingSessionController.
// ==============================================================================
void MainContentComponent::setWorkflowMode(gui::session::UiWorkflowMode mode)
{
    if (currentWorkflowMode == mode)
        return;

    currentWorkflowMode = mode;
    profilingSessionController.setWorkflowMode(mode);
}

void MainContentComponent::setupGuidedWorkflowInitialData()
{
    gui::session::TargetSelectionState target;
    target.targetId = "synthetic_fixture_demo";
    target.targetName = "Sintetizador Virtual de Prueba (Demo Snapshot)";
    target.manufacturer = "ABDAudioLab";
    target.version = "1.0.0";
    target.kind = gui::session::TargetKind::SyntheticFixture;
    target.isConnected = true;
    target.isDeterministic = true;
    target.availableDomainDescription = "Notas MIDI C1-C6, Vel 1-127, Controles de Filtro y Modulación";
    target.parameterCount = 8;

    profilingSessionController.selectTarget(target);
    profilingSessionController.updateAuditResult(
        synth::ApprovalStatus::Approved,
        "100% Determinista (Fixture Digital)",
        "Reset de fase instantaneo (0 ms)",
        0.0,
        false,
        {},
        "Target de prueba sintetico precalificado para validacion acustica");

    // Precargar evaluacion de demostracion para que el usuario siempre tenga datos listos
    profilingSessionController.loadPredefinedFixture("fixture_approved.json");
    profilingSessionController.navigateToStage(gui::session::ProfilingWorkflowStage::ConfigureAndStart);

    profilingSessionController.setWorkflowMode(currentWorkflowMode);
}

std::optional<gui::session::TargetSelectionState> MainContentComponent::resolveCanonicalTarget() const
{
    // 1. Virtual plugin target hosted via PluginUiCoordinator
    if (pluginUiCoordinator.hasActivePlugin())
    {
        const auto desc = pluginUiCoordinator.getActivePluginDescription();
        gui::session::TargetSelectionState target;
        target.targetId = "plugin_" + juce::File::createLegalFileName(desc.fileOrIdentifier).toStdString();
        target.targetName = desc.name.isNotEmpty() ? desc.name.toStdString() : "Plugin Virtual";
        target.manufacturer = desc.manufacturerName.toStdString();
        target.version = desc.version.toStdString();
        target.kind = gui::session::TargetKind::PluginVST3;
        target.isConnected = true;
        target.isDeterministic = true;
        target.supportsMidiInput = desc.isInstrument;
        target.supportsParameterAutomation = true;
        target.availableDomainDescription = desc.isInstrument
            ? "Notas MIDI C1-C6, Vel 1-127, Parámetros VST3"
            : "Procesamiento de Audio, Parámetros VST3";
        target.parameterCount = 0;
        return target;
    }

    // 2. Hardware selected via SoundIdHardwareCatalogSelector (Official Step 1)
    const juce::String catHwId = catalogSelector.getSelectedHardwareId();
    if (catHwId.isNotEmpty())
    {
        const auto* c = hardwareManager.findContractById(catHwId.toStdString());
        gui::session::TargetSelectionState target;
        target.targetId = catHwId.toStdString();
        target.targetName = (c != nullptr) ? c->displayName : catHwId.toStdString();
        target.manufacturer = (c != nullptr) ? c->manufacturer : "";
        target.version = (c != nullptr) ? c->schemaVersion : "1.0";
        const bool isAnalogue = (c != nullptr && (c->deviceType == "MANUAL_EURORACK" || c->deviceType == "ANALOGUE_PEDAL"));
        target.kind = isAnalogue ? gui::session::TargetKind::HardwareAnalogue : gui::session::TargetKind::HardwareDigital;
        target.isConnected = true;
        target.isDeterministic = !isAnalogue;
        target.supportsMidiInput = (c != nullptr && (c->deviceType == "AUTOMATED_MIDI_CC" || c->deviceType == "AUTOMATED_SYSEX"));
        target.supportsMidiCc = (c != nullptr && c->deviceType == "AUTOMATED_MIDI_CC");
        target.supportsSysEx = (c != nullptr && c->deviceType == "AUTOMATED_SYSEX");
        target.supportsParameterAutomation = false;
        target.availableDomainDescription = isAnalogue ? "Controles analógicos manuales" : "Canal MIDI, Notas y CC";
        target.parameterCount = (c != nullptr) ? static_cast<int>(c->functions.size()) : 0;
        return target;
    }

    // 3. Hardware selected via SlideInDrawer (Advanced Inspection Drawer)
    const juce::String drawerHwId = drawer.getSelectedHardwareId();
    if (drawerHwId.isNotEmpty())
    {
        const auto* c = hardwareManager.findContractById(drawerHwId.toStdString());
        gui::session::TargetSelectionState target;
        target.targetId = drawerHwId.toStdString();
        target.targetName = (c != nullptr) ? c->displayName : drawerHwId.toStdString();
        target.manufacturer = (c != nullptr) ? c->manufacturer : "";
        target.version = (c != nullptr) ? c->schemaVersion : "1.0";
        const bool isAnalogue = (c != nullptr && (c->deviceType == "MANUAL_EURORACK" || c->deviceType == "ANALOGUE_PEDAL"));
        target.kind = isAnalogue ? gui::session::TargetKind::HardwareAnalogue : gui::session::TargetKind::HardwareDigital;
        target.isConnected = true;
        target.isDeterministic = !isAnalogue;
        target.supportsMidiInput = (c != nullptr && (c->deviceType == "AUTOMATED_MIDI_CC" || c->deviceType == "AUTOMATED_SYSEX"));
        target.supportsMidiCc = (c != nullptr && c->deviceType == "AUTOMATED_MIDI_CC");
        target.supportsSysEx = (c != nullptr && c->deviceType == "AUTOMATED_SYSEX");
        target.supportsParameterAutomation = false;
        target.availableDomainDescription = isAnalogue ? "Controles analógicos manuales" : "Canal MIDI, Notas y CC";
        target.parameterCount = (c != nullptr) ? static_cast<int>(c->functions.size()) : 0;
        return target;
    }

    // 4. Stable snapshot authority in ProfilingSessionController
    const auto snapshot = profilingSessionController.getCurrentSnapshot();
    if (!snapshot.target.targetId.empty())
    {
        return snapshot.target;
    }

    return std::nullopt;
}

bool MainContentComponent::cargarCatalogoDeContratos()
{
    // ──────────────────────────────────────────────────────────────────────────
    // POR QUE ESTO ES UN METODO Y NO UN BLOQUE DEL CONSTRUCTOR.
    //
    // Porque la cadena de busqueda tiene que poder VOLVER a correr. La accion de
    // levantar una retencion termina en editar un JSON fuera de la aplicacion, y
    // cuando se vuelve el catalogo esta en memoria de antes: el cajon sigue
    // diciendo que el contrato esta retenido aunque el fichero ya no lo diga, y lo
    // unico que queda es reiniciar. Un "reinicia la app" en un boton es una
    // forma de decir "no puedes", y quien lo lee se rinde y deja el contrato
    // retenido para siempre.
    //
    // Devuelve si hay contratos, y NO toca la interfaz. Volcar a la interfaz es
    // otro metodo, porque son dos pasos que se pueden fallar por separado: leer el
    // catalogo es de `core`, y llenarlo de `gui`.
    //
    // ──────────────────────────────────────────────────────────────────────────
    std::vector<juce::File> roots;

    if (const auto sharedAssets = core::sharedAssetsDir(); sharedAssets.isDirectory())
        roots.push_back(sharedAssets.getChildFile("contracts"));

    roots.push_back(core::optionalRepoResource("contracts/hardware").getParentDirectory().getParentDirectory());

    for (auto root : roots)
    {
        // Direct ABDSharedAssets/contracts directory
        if (root.isDirectory() && root.getFileName() == "contracts" && hardwareManager.getContractRegistry().loadContractsFromDirectory(root))
            break;

        for (int i = 0; i < 6; ++i)
        {
            // 1. Check ABDSharedAssets/contracts (sibling or child)
            auto shared = root.getChildFile("ABDSharedAssets").getChildFile("contracts");
            if (shared.isDirectory() && hardwareManager.getContractRegistry().loadContractsFromDirectory(shared))
                break;

            auto siblingShared = root.getParentDirectory().getChildFile("ABDSharedAssets").getChildFile("contracts");
            if (siblingShared.isDirectory() && hardwareManager.getContractRegistry().loadContractsFromDirectory(siblingShared))
                break;

            // 2. Fallback to local contracts/hardware
            auto direct = root.getChildFile("contracts").getChildFile("hardware");
            if (direct.isDirectory() && hardwareManager.getContractRegistry().loadContractsFromDirectory(direct))
                break;

            root = root.getParentDirectory();
        }
        if (hardwareManager.getContractRegistry().hasContracts())
            break;
    }

    // 3. Cargar perfiles canónicos adaptados (profiles/targets)
    for (auto root : roots)
    {
        for (int i = 0; i < 6; ++i)
        {
            auto targetsDir = root.getChildFile("profiles").getChildFile("targets");
            if (targetsDir.isDirectory())
            {
                hardwareManager.getContractRegistry().loadCanonicalTargetProfiles(targetsDir);
                break;
            }
            root = root.getParentDirectory();
        }
        if (!hardwareManager.getContractRegistry().getCanonicalAdaptedContracts().empty())
            break;
    }

    return hardwareManager.getContractRegistry().hasContracts();
}

void MainContentComponent::volcarCatalogoEnLaInterfaz(bool conservarSeleccion)
{
    // Llenar el cajon y el selector con lo que hay AHORA en el registro.
    //
    // `conservarSeleccion` es para el reescaneo: recargar todo devuelve el
    // catalogo al estado inicial, con el combo en "no selection", asi que sin
    // esto un reescaneo dejaria al usuario sin el Aparato que tenia abierto
    // justo despues de haber arreglado un JSON. Perder el contexto al pulsar un
    // boton es la forma mas rapida de que no se pulse dos veces.
    const juce::String seleccionada = conservarSeleccion
                                              ? drawer.getSelectedHardwareId()
                                              : juce::String();

    // Populate Hardware Selector from Contract Registry
    std::vector<gui::HardwareItem> hwItems;
    for (const auto& c : hardwareManager.getContractRegistry().getContracts())
    {
        gui::HardwareItem item;
        item.id = juce::String(c.id);
        item.displayName = juce::String(c.displayName);
        item.description = juce::String(c.description);
        item.category = juce::String(c.deviceType);
        item.brand = juce::String(c.brand);
        item.brandLogo = juce::String(c.brandLogo);
        item.modelImage = juce::String(c.modelImage);

        for (const auto& f : c.functions)
        {
            gui::FunctionItem fItem;
            fItem.id = juce::String(f.id);
            fItem.name = juce::String(f.name);
            fItem.blockType = juce::String(f.blockType);
            fItem.stimulusOutput = juce::String(f.routingGuide.stimulusOutput);
            fItem.responseInput = juce::String(f.routingGuide.responseInput);
            fItem.notes = juce::String(f.routingGuide.notes);
            fItem.captureMode = juce::String(f.captureMode);
            fItem.defaultBurstDurationSec = f.defaultBurstDurationSec;
            for (const auto& ctrl : f.controls)
            {
                gui::ControlItem cItem;
                cItem.name = juce::String(ctrl.name);
                cItem.type = juce::String(ctrl.type);
                fItem.controls.push_back(cItem);
            }
            item.functions.push_back(fItem);
        }
        hwItems.push_back(item);
    }
    drawer.setHardwareList(hwItems);
    drawer.setContracts(hardwareManager.getContractRegistry().getContracts());

    // Los retenidos tambien se pasan. Antes se callaban y no aparecian por
    // ningun lado de la interfaz, que es la forma mas rapida de que alguien
    // pregunte dentro de tres semanas por donde se fue un Aparato.
    drawer.setQuarantinedProfiles(hardwareManager.getContractRegistry().getQuarantinedProfiles());

    if (!seleccionada.isEmpty())
        drawer.setSelectedHardwareId(seleccionada);

    catalogSelector.setContracts(hardwareManager.getContractRegistry().getContracts());
}

// ==============================================================================
// EL PANEL DE AVISOS, Y POR QUE TIENE UN METODO PROPIO.
//
// Porque `resized()` no es el unico sitio que lo coloca. Los avisos llegan
// por `onProfileWarning`, que se dispara desde un `callAsync` del arranque,
// y en ese momento no hay ningun cambio de tamano que dispare un `resized()`:
// el panel se haria visible con alto cero, y un aviso invisible es peor que
// uno que no esta, porque el registro ya ha avisado y alguien lo ha creido.
//
// Y el tope de la mitad de la pantalla es a proposito. El alto sale del numero
// de avisos, y un catalogo con cien contratos retenidos no puede comerse la
// aplicacion entera. Un aviso que empuja la medicion fuera de la pantalla se
// resuelve con un scroll; uno que empuja el boton de INICIAR fuera de la
// pantalla no se resuelve con nada.
// ==============================================================================
int MainContentComponent::colocarPanelDeAvisos(juce::Rectangle<int>& /*bounds*/)
{
    // El panel de avisos ya no ocupa espacio en el layout principal.
    // Se muestra como overlay flotante al pulsar la campana de notificaciones
    // en la barra de cabecera. Ver mainHeader.onNotificationBellClicked.
    return 0;
}

void MainContentComponent::reescargarCatalogoDeContratos()
{
    // El boton de la ficha de cuarentena. Recorre la MISMA cadena de busqueda
    // que el arranque, y no una ruta fija: la ruta fija seria el
    // `contracts/hardware` del laboratorio, que no es el origen. Recargar solo ese
    // no arregla nada: volveria a resolver justo el problema que la cadena de
    // busqueda existe para evitar.
    const int retenidosAntes =
        static_cast<int>(hardwareManager.getContractRegistry().getQuarantinedProfiles().size());

    if (!cargarCatalogoDeContratos())
        juce::Logger::writeToLog("[MainContentComponent] el reescaneo no ha encontrado contratos cargables.");
    volcarCatalogoEnLaInterfaz(true);

    const int retenidosDespues =
        static_cast<int>(hardwareManager.getContractRegistry().getQuarantinedProfiles().size());

    juce::Logger::writeToLog("[MainContentComponent] catalogo reescanado: "
                             + juce::String(retenidosAntes) + " -> "
                             + juce::String(retenidosDespues) + " retenido(s)");

    // Y se DICE, que es lo que el usuario ha ido a comprobar. Sin esto el boton
    // recarga en silencio y quien lo ha pulsado se queda mirando el cajon para
    // ver si ha pasado algo.
    //
    // Va al pie de la seccion de cuarentena, y no a la barra de progreso del
    // arranque: ese `report` es un lambda local del constructor, y a los treinta
    // segundos de arranque no hay a quien enseñarle un 0.62.
    juce::String aviso;

    if (retenidosDespues < retenidosAntes)
        aviso = juce::String("Reescaneado: ") + juce::String(retenidosAntes - retenidosDespues)
                + juce::String(" contrato(s) ya no retenidos.");
    else if (retenidosDespues > retenidosAntes)
        aviso = juce::String("Reescaneado: ") + juce::String(retenidosDespues - retenidosAntes)
                + juce::String(" contrato(s) nuevo(s) retenidos.");
    else
        aviso = "Reescaneado: el catalogo no ha cambiado.";

    drawer.setQuarantineStatus(aviso);
}

} // namespace abdaudiolab
