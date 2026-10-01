/**
 * @file DrawerHardwareTab.h
 * @brief Drawer tab for Hardware device inspection, submodule routing, and MIDI auto-detection.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "../SoundIdTheme.h"
#include "../../core/HardwareContractRegistry.h"
#include "AssetLocator.h"
#include "DrawerDataModels.h"
#include "HardwareMidiDetect/MidiHardwareBackend.h"

namespace abdaudiolab::gui
{

/**
 * Alto, en pixeles, que necesita la seccion de contratos retenidos por
 * cuarentena. Y CERO si no hay ninguno.
 *
 * El cero no es un adorno. `DrawerHardwareTab::getPreferredHeight()` lo suma al
 * alto del cajon y `resized()` lo usa para decidir si coloca la seccion, asi que
 * un alto minimo cuando la lista esta vacia abriria un hueco sin nada dentro
 * justo en las instalaciones limpias, que son las que mas se ven.
 *
 * Va como funcion suelta y no como metodo del componente para que un test pueda
 * fijarla sin enlazar el cajon entero: el `.cpp` del cajon no esta en el target
 * de tests, y un `Component` con `paint` fuera de linea exige su vtable.
 */
[[nodiscard]] inline int quarantineSectionHeight(size_t retenidos) noexcept
{
    if (retenidos == 0)
        return 0;

    // 10 de padding + 18 del titulo + 2 de respiro + una fila por contrato + 18
    // de la nota al pie + 10 de padding. `QuarantineListCard::paint` se gasta
    // exactamente los mismos.
    return 10 + 18 + 2 + static_cast<int>(retenidos) * 16 + 18 + 10;
}

/**
 * Alto de la FICHA de un retenido. Y CERO cuando no hay ninguno abierto.
 *
 * Mismo contrato que `quarantineSectionHeight`: cero significa "no se dibuja".
 * Y aqui el cero importa mas, porque la ficha nace de una seleccion y sin ella
 * no hay nada que pintar: dejarla con un alto minimo abriria un rectangulo
 * vacio justo cuando el usuario todavia no ha hecho clic en nada.
 *
 * No depende de la longitud del motivo, a proposito. Un motivo largo se
 * recorta en vez de crecer el cajon, porque un cajon cuyo alto depende de lo que
 * dice un texto de un JSON de otro repositorio es un cajon que se descuadra cada
 * vez que alguien corrige una palabra.
 */
[[nodiscard]] inline int quarantineDetailHeight(bool hayRetenidoSeleccionado) noexcept
{
    return hayRetenidoSeleccionado ? 132 : 0;
}

class DrawerHardwareTab : public juce::Component
{
public:
    DrawerHardwareTab();
    ~DrawerHardwareTab() override;

    void setHardwareList(const std::vector<HardwareItem>& list);
    void setContracts(std::vector<core::HardwareContract> contractsList);

    /**
     * Pinta los contratos que el registro retiene por cuarentena.
     *
     * Van en una seccion aparte y APARTE de la lista seleccionable, con su
     * motivo al lado. Retener un contrato en silencio no es lo mismo que
     * retenerlo con una linea que dice cual es: lo primero esconde hardware,
     * lo segundo se puede discutir y corregir.
     *
     * @param profiles  Tal cual los da `HardwareContractRegistry::
     *                  getQuarantinedProfiles()`: cada uno con su fichero, que
     *                  es lo que permite abrir el JSON a editar.
     */
    void setQuarantinedProfiles(const std::vector<core::quarantine::Retenido>& profiles);

    /**
     * Un aviso para el pie de la seccion de cuarentena.
     *
     * El pie ya existe y ya tiene su alto, asi que un aviso no descuadra nada.
     * Es donde va el resultado de un reescaneo: sin el, el boton recarga y no
     * dice nada, y quien lo ha pulsado se queda mirando el cajon para
     * comprobar si ha pasado algo.
     */
    void setQuarantineStatus(const juce::String& texto);

    /** La fila abierta en la ficha, o -1 si no hay ninguna. */
    [[nodiscard]] int getSelectedQuarantinedProfileIndex() const noexcept;

    /** El retenido abierto en la ficha, o `nullptr` si no hay ninguno. */
    [[nodiscard]] const core::quarantine::Retenido* getSelectedQuarantinedProfile() const noexcept;
    void setSelectedHardwareId(const juce::String& id);
    void clearSelectedHardware();
    void setHardwareLocked(bool locked);
    void triggerAutoDetect();
    void preWarmHardwarePicker();

    [[nodiscard]] bool getHardwareLocked() const noexcept { return isHardwareLocked; }
    [[nodiscard]] juce::String getSelectedHardwareId() const;
    [[nodiscard]] juce::String getSelectedFunctionId() const;
    [[nodiscard]] juce::String getActiveHardwareDisplayName() const;
    [[nodiscard]] juce::String getActiveFunctionDisplayName() const;
    [[nodiscard]] const juce::Image& getActiveModelRasterImage() const noexcept { return modelRasterImage; }
    [[nodiscard]] const juce::Drawable* getModelSvgDrawable() const noexcept { return modelSvgDrawable.get(); }
    [[nodiscard]] const juce::Drawable* getBrandLogoDrawable() const noexcept { return brandLogoDrawable.get(); }
    [[nodiscard]] const juce::String& getCurrentHwBrand() const noexcept { return currentHwBrand; }
    [[nodiscard]] float getBurstDurationSeconds() const;
    [[nodiscard]] bool isAdaptiveEnvelopeMode() const;
    [[nodiscard]] int getSelectedHardwareModeIndex() const { return hwModeCombo.getSelectedId(); }
    [[nodiscard]] int getPreferredHeight() const noexcept;

    void updateTheme();
    void updateBrandAndModelGraphics();
    void updateFunctionSelectionUI(const HardwareItem& item);

    // Callbacks
    std::function<void(const juce::String& hwId, const juce::String& funcId)> onHardwareSelected;
    std::function<void(const juce::String& displayName)> onDeviceDetected;
    std::function<void()> onNewFlowRequested;

    /**
     * Pedir que se vuelva a leer el catalogo de contratos.
     *
     * Lo que la hace falta es que la accion de levantar una retencion NO
     * termine en "reinicia la aplicacion". Editas el JSON, vuelves, y el
     * cajon sigue diciendo lo mismo: entonces retiras el contrato creyendo
     * que el motivo no era el, o simplemente reinicias sin saber por que.
     */
    std::function<void()> onContractsReloadRequested;

    void paint(juce::Graphics& g) override;
    void resized() override;

    class ImageDisplayComponent : public juce::Component
    {
    public:
        explicit ImageDisplayComponent(DrawerHardwareTab& ownerRef) : owner(ownerRef) {}
        void paint(juce::Graphics& g) override;
    private:
        DrawerHardwareTab& owner;
    };

    class WiringGuideCard : public juce::Component
    {
    public:
        void paint(juce::Graphics& g) override;
        juce::String stimulusText;
        juce::String responseText;
    };

    /**
     * Seccion de contratos retenidos por cuarentena: una fila por contrato con
     * su motivo al lado, y una nota que explica por que no son seleccionables.
     */
    class QuarantineListCard : public juce::Component
    {
    public:
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;

        [[nodiscard]] int computePreferredHeight() const noexcept
        {
            return quarantineSectionHeight(profiles.size());
        }

        /** La fila sobre la que esta el raton, o -1. */
        [[nodiscard]] int hoveredRow() const noexcept { return hovered; }

        /** La fila abierta en la ficha, o -1. */
        [[nodiscard]] int selectedRow() const noexcept { return selected; }

        /** Texto del pie. Vacio = la nota de siempre. */
        juce::String statusText;

        void selectRow(int fila);
        void deselectRow() { selectRow(-1); }

        std::vector<core::quarantine::Retenido> profiles;

        std::function<void(int)> onRowClicked;

    private:
        /** A que fila pertenece una coordenada vertical, o -1. */
        [[nodiscard]] int rowAt(int y) const noexcept;

        int hovered { -1 };
        int selected { -1 };
    };

    /**
     * La ficha del retenido abierto: su motivo entero, el JSON exacto que hay
     * que editar, y los dos botones que cierran el ciclo.
     */
    class QuarantineDetailCard : public juce::Component
    {
    public:
        void paint(juce::Graphics& g) override;

        // `has_value()` y no `!= nullptr`: la comparacion de `std::optional`
        // contra `nullptr_t` no la resuelve el operador de esta version de
        // MSVC, y `has_value()` dice lo mismo sin depender de eso.
        [[nodiscard]] const core::quarantine::Retenido* target() const noexcept
        {
            return retenido.has_value() ? &*retenido : nullptr;
        }

        void setTarget(const core::quarantine::Retenido* r);
        void updateTheme();

        juce::TextButton btnAbrir { "Abrir el JSON para editar" };
        juce::TextButton btnReescanear { "Reescanear contratos" };

    private:
        std::optional<core::quarantine::Retenido> retenido;
    };

private:
    void openHardwarePickerModal();

    /** Abre la ficha de la fila `fila`, o la cierra si ya estaba abierta. */
    void abrirFichaDeCuarentena(int fila);

    /** Pasa a la ficha lo que haya abierto, y ajusta el boton de abrir. */
    void actualizarFichaDeCuarentena();

    /** Abre el JSON del retenido abierto con el editor del sistema. */
    void abrirJsonDelRetenidoSeleccionado();

    std::vector<HardwareItem> hardwareList;
    std::vector<core::HardwareContract> availableContracts;

    std::unique_ptr<juce::Drawable> brandLogoDrawable;
    std::unique_ptr<juce::Drawable> modelSvgDrawable;
    juce::Image modelRasterImage;
    juce::String currentHwBrand;

    ImageDisplayComponent imgDisplay { *this };

    bool isHardwareLocked { false };
    juce::Label lblHardwareLockedBanner;
    juce::TextButton btnChangeHwOrNewFlow { "Cambiar Hardware / Iniciar Nuevo Flujo" };
    juce::Label lblHwTitle;
    juce::Label lblSelectHw;
    juce::ComboBox hwModeCombo;
    juce::Label lblSelectFunc;
    juce::ComboBox hwFunctionCombo;

    WiringGuideCard cardWiring;
    QuarantineListCard cardQuarantine;
    QuarantineDetailCard cardQuarantineDetail;
    juce::Label lblAutoDetectSection;
    juce::TextButton btnAutoDetect { "Auto-Detect Device (MIDI / USB)" };
    juce::Label lblOrSeparator;

    std::unique_ptr<abd::hwid::MidiHardwareBackend> midiBackend;
    std::unique_ptr<juce::DocumentWindow> pickerWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrawerHardwareTab)
};

} // namespace abdaudiolab::gui
