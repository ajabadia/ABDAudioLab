#include "gui/StartupWarningsPanel.h"

#include "gui/SoundIdTheme.h"

namespace abdaudiolab::gui
{

namespace
{

// El alto de una linea, su separacion, y el marco del panel. Son tres numeros
// y estan aqui, y no repartidos por `paint` y `resized`, porque los dos los
// necesitan y porque si los dos escriben el 10 a mano un dia uno se queda
// medio pixel descolgado y no se sabe cual de los dos va bien.
constexpr int altoLinea      = 44;
constexpr int separacion     = 2;
constexpr int padding        = 10;
constexpr int altoCabecera   = 20;
constexpr int altoBoton      = 26;
constexpr int anchoBoton     = 110;

// Cuanto ancho consume aproximadamente un caracter del cuerpo que se usa.
// No es una medida exacta a proposito: solo hace falta para saber cuantas
// lineas de texto caben en una fila, y quedarse corto es mejor que quedarse
// largo, porque un texto que se sale no se lee y uno que se parte de mas
// ocupa sitio de mas.
constexpr int anchoPorCaracter = 7;

/**
 * Cuantas lineas de texto ocupa `texto` a un ancho dado.
 *
 * Es una estimacion por palabras, no un layout real, porque no hace falta un
 * layout real: lo que se quiere saber es cuanto alto hace falta para que el
 * motivo NO se corte, y `drawFittedText` se encarga del ajuste fino dentro de
 * ese alto.
 */
[[nodiscard]] int lineasDeTexto(const juce::String& texto, int anchoDisponible) noexcept
{
    if (texto.isEmpty() || anchoDisponible <= 0)
        return 1;

    const int caben = anchoDisponible / anchoPorCaracter;

    if (caben <= 0)
        return 1;

    int necesarias = 1;

    for (const auto& palabra : juce::StringArray::fromTokens(texto, " ", "\""))
    {
        const int anchoPalabra = juce::jmax(1, static_cast<int>(palabra.length())) * anchoPorCaracter;

        // Una palabra mas larga que la linea necesita su propia linea, y a
        // partir de ahi cada `caben` pixeles de ancho es una linea mas.
        necesarias += juce::jmax(1, (anchoPalabra + caben - 1) / caben);
    }

    return juce::jmax(1, necesarias);
}

} // namespace

StartupWarningsPanel::StartupWarningsPanel()
{
    addAndMakeVisible(btnCerrar);
    btnCerrar.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);

    // Cerrar es ocultar, no vaciar. Vaciar pierde los avisos, y quien los
    // estaba leyendo todavia los necesita: un aviso que desaparece al pulsar
    // "entendido" ya no se puede volver a leer sin reescanear, que es un
    // viaje entero para volver a ver una frase que estaba a un clic.
    btnCerrar.onClick = [this] { setVisible(false); };
}

StartupWarningsPanel::~StartupWarningsPanel() = default;

void StartupWarningsPanel::addNotice(const juce::String& texto)
{
    if (texto.isEmpty())
        return;

    Linea linea;
    linea.texto = texto;
    lineas.push_back(linea);

    setVisible(true);
    repaint();
}

void StartupWarningsPanel::addQuarantineNotice(const core::quarantine::Retenido& retenido)
{
    if (retenido.nombre.isEmpty())
        return;

    // El texto NO se redacta aqui. Lo pone `quarantine::descripcionDeRetenido`,
    // que es la misma frase que escriben el registro y el adapter en el log,
    // y esa es la unica forma de que el log y la pantalla digan lo mismo: un
    // texto distinto en cada sitio acaba siendo el equivocado en uno de los
    // dos, y el equivocado es el que se lee el primero.
    Linea linea;
    linea.texto = core::quarantine::descripcionDeRetenido(retenido);
    linea.retenido = retenido;
    lineas.push_back(linea);

    setVisible(true);
    repaint();
}

juce::String StartupWarningsPanel::getNoticeText(int indice) const
{
    if (indice < 0 || indice >= static_cast<int>(lineas.size()))
        return {};

    return lineas[static_cast<size_t>(indice)].texto;
}

void StartupWarningsPanel::clearNotices()
{
    lineas.clear();
    repaint();
}

int StartupWarningsPanel::getVisibleLineCount(int anchoTotal) const noexcept
{
    int total = 0;

    for (const auto& linea : lineas)
        total += lineasDeTexto(linea.texto, anchoTotal - padding * 2);

    return total;
}

int StartupWarningsPanel::getPreferredHeight() const noexcept
{
    // Sin avisos no hay alto. Es la misma regla que la seccion de cuarentena del
    // cajon, y por el mismo motivo: un hueco reservado que no pinta nada parece
    // algo roto, y en la pantalla de arranque eso se lee como "el programa
    // fallo al arrancar" cuando lo que pasa es que no hay nada que decir.
    if (lineas.empty())
        return 0;

    const int cuerpo = static_cast<int>(lineas.size()) * altoLinea + padding;
    return padding + altoCabecera + cuerpo + padding + altoBoton + padding;
}

void StartupWarningsPanel::paintLinea(juce::Graphics& g,
                                      juce::Rectangle<int> area,
                                      const Linea& linea) const
{
    // El color lo dice casi todo: ambar para una decision editorial, que no es
    // un error pero tampoco es un ok, y rojo para lo que si es un fallo. Un rojo
    // en una retencion haria que alguien fuera a buscar un JSON roto que no
    // existe, y ese es el fallo que cuesta una tarde.
    const bool retenido = linea.retenido.has_value();
    const juce::Colour tinte = retenido ? SoundIdTheme::accentAmber
                                        : SoundIdTheme::accentRed;

    auto fondo = area.toFloat().reduced(0.5f);

    g.setColour(tinte.withAlpha(0.10f));
    g.fillRoundedRectangle(fondo, 4.0f);

    g.setColour(tinte.withAlpha(0.45f));
    g.drawRoundedRectangle(fondo, 4.0f, 1.0f);

    g.setColour(tinte);
    g.fillRect(fondo.getX() + 1.0f, fondo.getY() + 4.0f, 2.0f, fondo.getHeight() - 8.0f);

    g.setColour(retenido ? SoundIdTheme::textSecondary : SoundIdTheme::textPrimary);
    g.setFont(juce::FontOptions(12.0f));
    g.drawFittedText(linea.texto, area.reduced(10, 4), juce::Justification::topLeft, 3);
}

void StartupWarningsPanel::paint(juce::Graphics& g)
{
    if (lineas.empty())
        return;

    auto area = getLocalBounds().reduced(padding, padding);

    g.setColour(SoundIdTheme::textMuted);
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText("Avisos del arranque (" + juce::String(static_cast<int>(lineas.size())) + ")",
               area.removeFromTop(altoCabecera).toNearestInt(),
               juce::Justification::centredLeft);

    auto cuerpo = getLocalBounds()
                      .reduced(padding, padding + altoCabecera)
                      .withTrimmedBottom(altoBoton + padding);

    for (size_t i = 0; i < lineas.size(); ++i)
    {
        auto fila = cuerpo.removeFromTop(altoLinea);
        paintLinea(g, fila, lineas[i]);
        cuerpo.removeFromTop(separacion);
    }
}

void StartupWarningsPanel::resized()
{
    auto boton = getLocalBounds()
                     .reduced(padding, padding)
                     .removeFromBottom(altoBoton)
                     .removeFromRight(anchoBoton)
                     .toNearestInt();

    btnCerrar.setBounds(boton);
}

} // namespace abdaudiolab::gui