#include "HardwareDeviceDisplayCardComponent.h"
#include "SoundIdTheme.h"
#include "AppTheme.h"

namespace abdaudiolab::gui
{

static juce::File locateAssetFile(const juce::String& relPath)
{
    if (relPath.isEmpty()) return {};

    auto findExisting = [](const juce::File& file) -> juce::File {
        // ALWAYS prioritize .png because JUCE ImageFileFormat does not decode .webp natively!
        if (file.getFileExtension().equalsIgnoreCase(".webp"))
        {
            auto png = file.withFileExtension(".png");
            if (png.existsAsFile()) return png;
        }
        else if (file.getFileExtension().equalsIgnoreCase(".png"))
        {
            if (file.existsAsFile()) return file;
            auto webp = file.withFileExtension(".webp");
            if (webp.existsAsFile()) return webp;
        }
        if (file.existsAsFile()) return file;
        return {};
    };

    // 1. Ruta directa
    juce::File f(relPath);
    auto found = findExisting(f);
    if (found.existsAsFile()) return found;

    // Helper to search in a folder and standard subfolders
    auto checkDir = [&](const juce::File& dir) -> juce::File {
        if (!dir.isDirectory()) return {};

        auto res = findExisting(dir.getChildFile(relPath));
        if (res.existsAsFile()) return res;

        res = findExisting(dir.getChildFile("models").getChildFile(relPath));
        if (res.existsAsFile()) return res;

        res = findExisting(dir.getChildFile("brands").getChildFile(relPath));
        if (res.existsAsFile()) return res;

        res = findExisting(dir.getChildFile("models/logos").getChildFile(relPath));
        if (res.existsAsFile()) return res;

        // In case relPath already starts with models/ or brands/, try stripping it
        if (relPath.startsWith("models/") || relPath.startsWith("models\\"))
        {
            auto sub = relPath.substring(7);
            res = findExisting(dir.getChildFile("models").getChildFile(sub));
            if (res.existsAsFile()) return res;
        }
        if (relPath.startsWith("brands/") || relPath.startsWith("brands\\"))
        {
            auto sub = relPath.substring(7);
            res = findExisting(dir.getChildFile("brands").getChildFile(sub));
            if (res.existsAsFile()) return res;
        }
        return {};
    };

    // 2. Traversal portable hacia ABDSharedAssets subiendo hasta 7 niveles desde el ejecutable
    juce::File exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    juce::File curr = exeDir;
    for (int i = 0; i < 7; ++i)
    {
        auto sharedAssetsDir = curr.getChildFile("ABDSharedAssets");
        auto res = checkDir(sharedAssetsDir);
        if (res.existsAsFile()) return res;

        res = checkDir(curr);
        if (res.existsAsFile()) return res;

        curr = curr.getParentDirectory();
    }

    // 3. Traversal portable subiendo desde Current Working Directory
    curr = juce::File::getCurrentWorkingDirectory();
    for (int i = 0; i < 7; ++i)
    {
        auto sharedAssetsDir = curr.getChildFile("ABDSharedAssets");
        auto res = checkDir(sharedAssetsDir);
        if (res.existsAsFile()) return res;

        res = checkDir(curr);
        if (res.existsAsFile()) return res;

        curr = curr.getParentDirectory();
    }

    return {};
}

HardwareDeviceDisplayCardComponent::HardwareDeviceDisplayCardComponent()
{
}

void HardwareDeviceDisplayCardComponent::setDevice(const core::HardwareContract* contract)
{
    brandLogoDrawable.reset();
    modelSvgDrawable.reset();
    modelRasterImage = juce::Image();
    currentHwBrand.clear();
    currentHwDisplayName.clear();
    currentHwCategory.clear();

    if (contract != nullptr)
    {
        currentHwDisplayName = juce::String(contract->displayName);
        currentHwBrand = juce::String(contract->brand);
        currentHwCategory = juce::String(contract->deviceType);

        // 1. Cargar Brand Logo (soporta dark mode y color swaps idénticos a SlideInDrawer)
        if (!contract->brandLogo.empty())
        {
            juce::File brandFile;
            if (AppTheme::currentMode == AppTheme::ThemeMode::Dark)
            {
                if (currentHwBrand.containsIgnoreCase("yamaha"))
                {
                    brandFile = locateAssetFile(juce::String(contract->brandLogo).replace(".svg", "-dark.svg"));
                }
                else if (!currentHwBrand.containsIgnoreCase("roland"))
                {
                    brandFile = locateAssetFile(juce::String(contract->brandLogo).replace(".svg", "-white.svg"));
                }
            }
            if (!brandFile.existsAsFile())
            {
                brandFile = locateAssetFile(juce::String(contract->brandLogo));
            }

            if (brandFile.existsAsFile())
            {
                if (brandFile.getFileExtension().equalsIgnoreCase(".svg"))
                {
                    juce::String svgText = brandFile.loadFileAsString();
                    if (AppTheme::currentMode == AppTheme::ThemeMode::Dark)
                    {
                        if (currentHwBrand.containsIgnoreCase("roland"))
                        {
                            // Roland preserves its iconic orange (#FF5A00 / #E65100)
                        }
                        else if (currentHwBrand.containsIgnoreCase("yamaha"))
                        {
                            // Vibrant Yamaha violet for dark mode
                            svgText = svgText.replace("fill:#48217a", "fill:#A855F7", true)
                                             .replace("fill: #48217a", "fill:#A855F7", true)
                                             .replace("fill=\"#48217a\"", "fill=\"#A855F7\"", true);
                        }
                        else
                        {
                            // Convert dark fills to white for dark mode
                            svgText = svgText.replace("fill=\"#333\"", "fill=\"#FFFFFF\"", true)
                                             .replace("fill=\"#333333\"", "fill=\"#FFFFFF\"", true)
                                             .replace("fill=\"#000000\"", "fill=\"#FFFFFF\"", true)
                                             .replace("fill=\"#000\"", "fill=\"#FFFFFF\"", true)
                                             .replace("fill=\"black\"", "fill=\"#FFFFFF\"", true)
                                             .replace("fill=\"#111111\"", "fill=\"#FFFFFF\"", true)
                                             .replace("fill=\"#111827\"", "fill=\"#FFFFFF\"", true)
                                             .replace("fill=\"#222222\"", "fill=\"#FFFFFF\"", true)
                                             .replace("fill=\"#231f20\"", "fill=\"#FFFFFF\"", true)
                                             .replace("fill:#231f20", "fill=\"#FFFFFF\"", true)
                                             .replace("fill: #231f20", "fill:#FFFFFF", true)
                                             .replace("fill: #000000", "fill: #FFFFFF", true)
                                             .replace("fill:#000000", "fill:#FFFFFF", true)
                                             .replace("fill: black", "fill: #FFFFFF", true)
                                             .replace("fill:black", "fill:#FFFFFF", true)
                                             .replace("stroke=\"#000000\"", "stroke=\"#FFFFFF\"", true)
                                             .replace("stroke=\"#000\"", "stroke=\"#FFFFFF\"", true)
                                             .replace("stroke=\"black\"", "stroke=\"#FFFFFF\"", true);

                            if (!svgText.containsIgnoreCase("fill="))
                            {
                                svgText = svgText.replace("<svg ", "<svg fill=\"#FFFFFF\" ", true);
                            }
                        }
                    }

                    auto xml = juce::parseXML(svgText);
                    if (xml != nullptr)
                        brandLogoDrawable = juce::Drawable::createFromSVG(*xml);
                }
                else
                {
                    brandLogoDrawable = juce::Drawable::createFromImageDataStream(*brandFile.createInputStream());
                }
            }
        }

        // 2. Cargar Model Image (SVG o Raster PNG/WebP)
        if (!contract->modelImage.empty())
        {
            auto modelFile = locateAssetFile(juce::String(contract->modelImage));
            if (modelFile.existsAsFile())
            {
                if (modelFile.getFileExtension().equalsIgnoreCase(".svg"))
                {
                    modelSvgDrawable = juce::Drawable::createFromImageDataStream(*modelFile.createInputStream());
                }
                else
                {
                    modelRasterImage = juce::ImageFileFormat::loadFrom(modelFile);
                }
            }
        }
    }
    repaint();
}

void HardwareDeviceDisplayCardComponent::paint(juce::Graphics& g)
{
    auto deviceCard = getLocalBounds().toFloat();

    g.setColour(SoundIdTheme::bgCardHover);
    g.fillRoundedRectangle(deviceCard, 8.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(deviceCard.reduced(0.5f), 8.0f, 1.0f);

    auto dInner = deviceCard.reduced(14.0f, 12.0f);

    // Cabecera de la tarjeta con nombre de dispositivo y marca
    auto cardTitleRow = dInner.removeFromTop(24.0f);
    g.setFont(juce::FontOptions("Inter", 13.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText(currentHwDisplayName.isNotEmpty() ? currentHwDisplayName : juce::String::fromUTF8(u8"Dispositivo Analógico"),
               cardTitleRow.removeFromLeft(cardTitleRow.getWidth() - 90.0f), juce::Justification::centredLeft, true);

    if (brandLogoDrawable != nullptr)
    {
        auto logoBox = cardTitleRow.toFloat();
        brandLogoDrawable->drawWithin(g, logoBox, juce::RectanglePlacement::xRight | juce::RectanglePlacement::yMid | juce::RectanglePlacement::onlyReduceInSize, 1.0f);
    }
    else if (currentHwBrand.isNotEmpty())
    {
        g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::textMuted);
        g.drawText(currentHwBrand, cardTitleRow, juce::Justification::centredRight, true);
    }

    g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::plain));
    g.setColour(SoundIdTheme::accentGreen);
    g.drawText("Tipo: " + (currentHwCategory.isNotEmpty() ? currentHwCategory : "MANUAL_EURORACK"),
               dInner.removeFromTop(16.0f), juce::Justification::centredLeft, true);

    dInner.removeFromTop(8.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.fillRect(dInner.removeFromTop(1.0f));
    dInner.removeFromTop(10.0f);

    // Área de renderizado del sintetizador / pedal
    auto imageArea = dInner;
    if (modelSvgDrawable != nullptr)
    {
        modelSvgDrawable->drawWithin(g, imageArea, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize, 1.0f);
    }
    else if (modelRasterImage.isValid())
    {
        g.drawImage(modelRasterImage, imageArea, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
    }
    else
    {
        // Gráfico decorativo de fallback para hardware sin imagen
        g.setColour(SoundIdTheme::borderCard);
        g.drawRoundedRectangle(imageArea.reduced(20.0f, 20.0f), 6.0f, 1.0f);
        g.setFont(juce::FontOptions("Inter", 12.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::textMuted);
        g.drawText(juce::String::fromUTF8(u8"HARDWARE ANALÓGICO CONECTADO"), imageArea, juce::Justification::centred, true);
    }
}

} // namespace abdaudiolab::gui
