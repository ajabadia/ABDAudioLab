#include "gui/StartupWarningsPanel.h"

#include "gui/SoundIdTheme.h"

namespace abdaudiolab::gui
{

namespace
{

constexpr int padding        = 16;
constexpr int altoCabecera   = 28;
constexpr int altoBoton      = SoundIdTheme::ButtonTokens::height;
constexpr int anchoBoton     = 130;
constexpr int separacion     = 8;

[[nodiscard]] int calculateNoticeHeight(const juce::String& texto, int anchoDisponible) noexcept
{
    if (texto.isEmpty() || anchoDisponible <= 0)
        return 48;

    const float textW = static_cast<float>(juce::jmax(50, anchoDisponible - 28));

    juce::Font font(juce::FontOptions(13.5f));
    juce::AttributedString as;
    as.append(texto, font, juce::Colours::black);
    as.setWordWrap(juce::AttributedString::WordWrap::byWord);

    juce::TextLayout tl;
    tl.createLayout(as, textW);

    const int textH = static_cast<int>(std::ceil(tl.getHeight()));
    return juce::jmax(48, textH + 22);
}

[[nodiscard]] int lineasDeTexto(const juce::String& texto, int anchoDisponible) noexcept
{
    if (texto.isEmpty() || anchoDisponible <= 0)
        return 1;

    const float textW = static_cast<float>(juce::jmax(50, anchoDisponible));

    juce::Font font(juce::FontOptions(13.5f));
    juce::AttributedString as;
    as.append(texto, font, juce::Colours::black);
    as.setWordWrap(juce::AttributedString::WordWrap::byWord);

    juce::TextLayout tl;
    tl.createLayout(as, textW);

    return juce::jmax(1, tl.getNumLines());
}

} // namespace

// ==============================================================================
// NoticeListContent (Scrollable Child Component)
// ==============================================================================

void StartupWarningsPanel::NoticeListContent::setLineas(const std::vector<Linea>& l)
{
    lineas = l;
    repaint();
}

int StartupWarningsPanel::NoticeListContent::calculateTotalHeight(int width) const
{
    if (lineas.empty())
        return 0;

    int total = 0;
    for (size_t i = 0; i < lineas.size(); ++i)
    {
        total += calculateNoticeHeight(lineas[i].texto, width);
        if (i + 1 < lineas.size())
            total += separacion;
    }
    return total;
}

void StartupWarningsPanel::NoticeListContent::paintLinea(juce::Graphics& g,
                                                          juce::Rectangle<int> area,
                                                          const Linea& linea) const
{
    const bool retenido = linea.retenido.has_value();
    const juce::Colour tinte = retenido ? SoundIdTheme::accentAmber
                                        : SoundIdTheme::accentRed;

    auto fondo = area.toFloat().reduced(0.5f);

    g.setColour(tinte.withAlpha(0.08f));
    g.fillRoundedRectangle(fondo, 6.0f);

    g.setColour(tinte.withAlpha(0.35f));
    g.drawRoundedRectangle(fondo, 6.0f, 1.0f);

    g.setColour(tinte);
    g.fillRect(fondo.getX() + 2.0f, fondo.getY() + 6.0f, 3.0f, fondo.getHeight() - 12.0f);

    g.setColour(retenido ? SoundIdTheme::textSecondary : SoundIdTheme::textPrimary);
    g.setFont(juce::FontOptions(13.5f));
    g.drawFittedText(linea.texto, area.reduced(14, 8), juce::Justification::topLeft, 10);
}

void StartupWarningsPanel::NoticeListContent::paint(juce::Graphics& g)
{
    int y = 0;
    const int itemW = getWidth();

    for (const auto& linea : lineas)
    {
        const int itemH = calculateNoticeHeight(linea.texto, itemW);
        juce::Rectangle<int> itemArea(0, y, itemW, itemH);

        if (g.getClipBounds().intersects(itemArea))
            paintLinea(g, itemArea, linea);

        y += itemH + separacion;
    }
}

// ==============================================================================
// StartupWarningsPanel (Modal Overlay with Viewport)
// ==============================================================================

StartupWarningsPanel::StartupWarningsPanel()
{
    addAndMakeVisible(viewport);
    viewport.setViewedComponent(&noticeList, false);
    viewport.setScrollBarsShown(true, false);

    addAndMakeVisible(btnCerrar);
    btnCerrar.setColour(juce::TextButton::buttonColourId, SoundIdTheme::ButtonTokens::primaryBg());
    btnCerrar.setColour(juce::TextButton::textColourOffId, SoundIdTheme::ButtonTokens::primaryText());

    // Cerrar es ocultar, no vaciar. Vaciar pierde los avisos, y quien los
    // estaba leyendo todavia los necesita: un aviso que desaparece al pulsar
    // "entendido" ya no se puede volver a leer sin reescanear.
    btnCerrar.onClick = [this] {
        setVisible(false);
        if (onDismissed != nullptr)
            onDismissed();
    };

    // Empieza oculto. La campana en la cabecera es el punto de entrada
    // interactivo para que no bloquee ni altere la vista al arrancar.
    setVisible(false);
}

StartupWarningsPanel::~StartupWarningsPanel() = default;

void StartupWarningsPanel::addNotice(const juce::String& texto)
{
    if (texto.isEmpty())
        return;

    Linea linea;
    linea.texto = texto;
    lineas.push_back(linea);

    noticeList.setLineas(lineas);
    resized();
    repaint();
}

void StartupWarningsPanel::addQuarantineNotice(const core::quarantine::Retenido& retenido)
{
    if (retenido.nombre.isEmpty())
        return;

    Linea linea;
    linea.texto = core::quarantine::descripcionDeRetenido(retenido);
    linea.retenido = retenido;
    lineas.push_back(linea);

    noticeList.setLineas(lineas);
    resized();
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
    noticeList.setLineas(lineas);
    resized();
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
    if (lineas.empty())
        return 0;

    return noticeList.calculateTotalHeight(500) + padding * 2 + altoCabecera + altoBoton + 24;
}

juce::Rectangle<float> StartupWarningsPanel::getCardBounds() const noexcept
{
    auto fullBounds = getLocalBounds().toFloat();
    float cardW = juce::jlimit(420.0f, 680.0f, fullBounds.getWidth() * 0.60f);

    constexpr int chromeH = padding * 2 + altoCabecera + altoBoton + 24;
    int contentW = static_cast<int>(cardW) - padding * 2;
    int noticesH = noticeList.calculateTotalHeight(contentW);

    float idealH = static_cast<float>(chromeH + noticesH);
    float maxH = juce::jmin(540.0f, fullBounds.getHeight() * 0.80f);
    float cardH = juce::jlimit(200.0f, maxH, idealH);

    return fullBounds.withSizeKeepingCentre(cardW, cardH);
}

void StartupWarningsPanel::paint(juce::Graphics& g)
{
    if (lineas.empty())
        return;

    auto fullBounds = getLocalBounds().toFloat();

    // 1. Semi-transparent dark scrim (modal backdrop)
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRect(fullBounds);

    // 2. Centered opaque card
    auto card = getCardBounds();

    // Drop shadow
    g.setColour(juce::Colour(0x28000000));
    g.fillRoundedRectangle(card.expanded(5.0f), 14.0f);

    // Card background
    g.setColour(SoundIdTheme::bgCard);
    g.fillRoundedRectangle(card, 12.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(card, 12.0f, 1.0f);

    // 3. Header inside card
    auto area = card.reduced(static_cast<float>(padding)).toNearestInt();

    g.setColour(SoundIdTheme::accentAmber);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("SYSTEM NOTIFICATIONS", area.removeFromTop(14),
               juce::Justification::centredLeft);
    area.removeFromTop(4);

    g.setColour(SoundIdTheme::textPrimary);
    g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    g.drawText("Avisos del arranque (" + juce::String(static_cast<int>(lineas.size())) + ")",
               area.removeFromTop(altoCabecera),
               juce::Justification::centredLeft);
}

void StartupWarningsPanel::resized()
{
    auto card = getCardBounds();

    auto inner = card.reduced(static_cast<float>(padding)).toNearestInt();

    // Reserve top for header (14 + 4 + altoCabecera + 6)
    inner.removeFromTop(14 + 4 + altoCabecera + 6);

    // Reserve bottom for button
    auto bottomRow = inner.removeFromBottom(altoBoton + 8);
    btnCerrar.setBounds(bottomRow.removeFromRight(anchoBoton).withHeight(altoBoton));

    // Remaining area is for viewport
    viewport.setBounds(inner);

    int contentW = viewport.getWidth();
    int contentH = noticeList.calculateTotalHeight(contentW);

    if (contentH > viewport.getHeight())
    {
        contentW = viewport.getViewWidth();
        contentH = noticeList.calculateTotalHeight(contentW);
    }

    noticeList.setBounds(0, 0, contentW, contentH);
}

void StartupWarningsPanel::mouseDown(const juce::MouseEvent& e)
{
    // Clic fuera de la tarjeta (en el scrim oscuro) cierra la modal
    if (!getCardBounds().contains(e.position))
    {
        setVisible(false);
        if (onDismissed != nullptr)
            onDismissed();
    }
}

} // namespace abdaudiolab::gui