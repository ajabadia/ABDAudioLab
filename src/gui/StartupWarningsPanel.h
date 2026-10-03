/**
 * @file StartupWarningsPanel.h
 * @brief Avisos de arranque que se quedan. Nada de texto que se autodestruye.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <optional>
#include <vector>

#include "core/HardwareContractQuarantine.h"

namespace abdaudiolab::gui
{

//==============================================================================
// POR QUE EXISTE ESTO Y NO ES "OTRO LABEL".
//
// Los avisos del arranque salen por `manualPromptLabel`, que se esconde solo a
// los seis segundos. Eso esta bien para un aviso de paso --"target
// verificado", "audio reconectado"-- y esta MUY mal para una retencion.
//
// Una retencion es lo unico de todo el arranque que NO se resuelve solo. El
// hardware sigue sin aparecer, y aparecera asi dentro de tres semanas, y
// dentro de tres semanas nadie recuerda un texto que se borro en 1999. Un
// aviso que se borra es peor que ningun aviso: ademas de no resolver nada,
// ocupa sitio. El que lo vio creera que ya lo miro.
//
// Asi que los retenidos no salen por ahi. Salen por aqui, que no se esconde.
//
// ----------------------------------------------------------------------------
// LO QUE ESTE PANEL ES, Y LO QUE NO ES.
//
// No es un registro de errores. Retener un contrato es una DECISION EDITORIAL
// sobre el contenido de un fichero, escrita en el propio fichero, y por eso
// cada linea lo dice. Un panel que mezclara las dos cosas enseña a ignorar el
// panel entero, que es lo que pasa cuando un aviso de "no he podido leer este
// JSON" y otro de "este JSON esta deliberadamente retenido" salen por el mismo
// camino sin distinguirse.
//
// ----------------------------------------------------------------------------
// POR QUE NO ES UN LISTBOX.
//
// Porque una lista de cadenas obliga a ir al indice para leer el motivo, y el
// motivo ES el contenido. Cada fila lleva nombre, motivo y fichero, y el alto
// sale de la fila: lo que hay que leer es lo que ocupa.
//
//==============================================================================

/**
 * @brief Panel de avisos que persisten, visible desde el arranque.
 */
class StartupWarningsPanel : public juce::Component
{
public:
    StartupWarningsPanel();
    ~StartupWarningsPanel() override;

    /** Una linea mas. Se respeta el orden de llegada. */
    void addNotice(const juce::String& texto);

    /**
     * Lo mismo, pero para un retenido: escribe el nombre y el motivo, y deja
     * el fichero a mano para poder abrirlo.
     */
    void addQuarantineNotice(const core::quarantine::Retenido& retenido);

    /** Sin avisos, el panel no se enseña. Es lo que evita un rectangulo vacio. */
    [[nodiscard]] bool hasNotices() const noexcept { return !lineas.empty(); }
    [[nodiscard]] int getNoticeCount() const noexcept { return static_cast<int>(lineas.size()); }
    [[nodiscard]] juce::String getNoticeText(int indice) const;

    /** Quita todos los avisos. Lo usa el reescaneo, que vuelve a empezar la lista. */
    void clearNotices();

    /**
     * El alto que hace falta para los avisos que hay ahora.
     *
     * Sale de los avisos, no de una constante. Un panel que reserva un hueco
     * fijo para N lineas y lo pinta vacio cuando hay menos es un rectangulo
     * vacio en la pantalla de arranque, que es justo el defecto que este panel
     * viene a evitar.
     */
    [[nodiscard]] int getPreferredHeight() const noexcept;

    /** Las lineas que se pintan sin recortarse, con el ancho que se les da. */
    [[nodiscard]] int getVisibleLineCount(int anchoTotal) const noexcept;

    /** El boton de "cerrar". */
    juce::TextButton btnCerrar { "Entendido" };

    /** Notificación cuando el usuario pulsa 'Entendido' para cerrar el panel. */
    std::function<void()> onDismissed;

    /** Se avisa para que quien quiera abrir un JSON desde aqui pueda engancharse. */
    std::function<void(const core::quarantine::Retenido&)> onOpenQuarantineJson;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

    [[nodiscard]] juce::Rectangle<float> getCardBounds() const noexcept;

private:
    struct Linea
    {
        juce::String texto;

        /** Si esta relleno, la linea es un retenido y se puede abrir su JSON. */
        std::optional<core::quarantine::Retenido> retenido;
    };

    class NoticeListContent : public juce::Component
    {
    public:
        void setLineas(const std::vector<Linea>& l);
        void paint(juce::Graphics& g) override;
        [[nodiscard]] int calculateTotalHeight(int width) const;

    private:
        std::vector<Linea> lineas;
        void paintLinea(juce::Graphics& g, juce::Rectangle<int> area, const Linea& linea) const;
    };

    juce::Viewport viewport;
    NoticeListContent noticeList;
    std::vector<Linea> lineas;
};

} // namespace abdaudiolab::gui