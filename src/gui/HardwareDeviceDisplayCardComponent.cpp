#include "HardwareDeviceDisplayCardComponent.h"
#include "SoundIdTheme.h"
#include "AppTheme.h"
#include "core/LabResourcePaths.h"

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

    // 2. Assets compartidos: la busqueda de ABDSharedAssets la hace LabResourcePaths.
    //    Antes se repetia aqui, subiendo 7 niveles desde el ejecutable Y otras
    //    7 desde el directorio de trabajo, lo que hacia el resultado depender
    //    de donde se hubiera lanzado la aplicacion.
    if (const auto shared = abdaudiolab::core::sharedAssetsDir(); shared.isDirectory())
    {
        auto res = checkDir(shared);
        if (res.existsAsFile()) return res;
    }

    // 3. Raiz del repositorio. Antes se ascendia 7 niveles desde el ejecutable
    //    probando cada ancestro a ciegas. Esa era una REIMPLEMENTACION de la
    //    resolucion que ya hace LabResourcePaths, y por eso el resultado
    //    dependia de cuantos niveles separasen el .exe de la raiz: la misma
    //    consulta encontraba el asset en un checkout y no en otro.
    if (abdaudiolab::core::isSafeRepoRelativePath(relPath))
    {
        // resolveRepoRoot() no lanza: si el producto corre fuera del arbol del
        // repositorio devuelve una File invalida y checkDir la descarta.
        const auto root = abdaudiolab::core::resolveRepoRoot();
        if (root.isResolved)
        {
            if (auto res = checkDir(root.root); res.existsAsFile())
                return res;
        }
    }

    // 4. Directorio del ejecutable: distribucion portable con los assets al
    //    lado del binario. Es una coincidencia legitima (no es la raiz del repo)
    //    y por eso se consulta un solo nivel, sin ascenso.
    if (auto res = checkDir(juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                                .getParentDirectory()); res.existsAsFile())
        return res;

    return {};
}

HardwareDeviceDisplayCardComponent::HardwareDeviceDisplayCardComponent()
{
}

void HardwareDeviceDisplayCardComponent::clear()
{
    isPluginModeActive = false;
    brandLogoDrawable.reset();
    modelSvgDrawable.reset();
    modelRasterImage = juce::Image();
    currentHwBrand.clear();
    currentHwDisplayName.clear();
    currentHwCategory.clear();
    repaint();
}

void HardwareDeviceDisplayCardComponent::setPluginInfo(const juce::String& name, const juce::String& manufacturer, const juce::String& format, bool isInstrument)
{
    isPluginModeActive = true;
    brandLogoDrawable.reset();
    modelSvgDrawable.reset();

    juce::File defaultImgFile;
    if (isInstrument)
        defaultImgFile = locateAssetFile("models/generic-digital-keyboard.png");
    else
        defaultImgFile = locateAssetFile("models/generic-audio-rack.png");

    if (defaultImgFile.existsAsFile())
        modelRasterImage = juce::ImageFileFormat::loadFrom(defaultImgFile);
    else
        modelRasterImage = juce::Image();

    currentHwBrand = manufacturer.isNotEmpty() ? manufacturer : "Generic";
    currentHwDisplayName = name.isNotEmpty() ? name : "Plugin Virtual";
    currentHwCategory = isInstrument ? (format + " Virtual Instrument") : (format + " Virtual Effect");
    repaint();
}

void HardwareDeviceDisplayCardComponent::setDevice(const core::HardwareContract* contract)
{
    isPluginModeActive = false;
    brandLogoDrawable.reset();
    modelSvgDrawable.reset();
    modelRasterImage = juce::Image();
    currentHwBrand.clear();
    currentHwDisplayName.clear();
    currentHwCategory.clear();

    if (contract == nullptr)
    {
        repaint();
        return;
    }

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

        // Fallback genérico según categoría si no tiene imagen asignada
        if (modelSvgDrawable == nullptr && !modelRasterImage.isValid())
        {
            juce::String cat = currentHwCategory.toLowerCase();
            juce::File fallbackFile;
            if (cat.contains("pedal") || cat.contains("stompbox") || cat.contains("guitar"))
                fallbackFile = locateAssetFile("models/generic-guitar-pedal.png");
            else if (cat.contains("eurorack") || cat.contains("modular"))
                fallbackFile = locateAssetFile("models/generic-eurorack.png");
            else if (cat.contains("rack") || cat.contains("studio") || cat.contains("efecto") || cat.contains("effect"))
                fallbackFile = locateAssetFile("models/generic-audio-rack.png");
            else if (cat.contains("anal") || cat.contains("analog"))
                fallbackFile = locateAssetFile("models/generic-analog-keyboard.png");
            else
                fallbackFile = locateAssetFile("models/generic-digital-keyboard.png");

            if (fallbackFile.existsAsFile())
                modelRasterImage = juce::ImageFileFormat::loadFrom(fallbackFile);
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

    // 1. Caso: Sin selección (Estado Limpio / Nueva Sesión)
    if (!isPluginModeActive && currentHwDisplayName.isEmpty())
    {
        g.setFont(juce::FontOptions("Inter", 14.5f, juce::Font::bold));
        g.setColour(SoundIdTheme::textMuted);
        g.drawText(juce::String::fromUTF8(u8"SIN DISPOSITIVO SELECCIONADO"), dInner.removeFromTop(24.0f), juce::Justification::centred, true);

        g.setColour(SoundIdTheme::borderCard);
        g.drawRoundedRectangle(dInner.reduced(16.0f, 16.0f), 6.0f, 1.0f);

        g.setFont(juce::FontOptions("Inter", 12.5f, juce::Font::plain));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText(juce::String::fromUTF8(u8"Seleccione un sintetizador, pedal o plugin virtual en el menú superior."),
                   dInner, juce::Justification::centred, true);
        return;
    }

    // 2. TÍTULO / LOGO DE LA MARCA (Arriba del todo, centrado y limpio)
    auto logoArea = dInner.removeFromTop(32.0f);
    if (brandLogoDrawable != nullptr)
    {
        brandLogoDrawable->drawWithin(g, logoArea, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize, 1.0f);
    }
    else if (currentHwBrand.isNotEmpty())
    {
        g.setFont(juce::FontOptions("Inter", 16.0f, juce::Font::bold));
        g.setColour(isPluginModeActive ? SoundIdTheme::accentPurple : SoundIdTheme::textPrimary);
        g.drawText(currentHwBrand, logoArea, juce::Justification::centred, true);
    }

    dInner.removeFromTop(4.0f);

    // 3. Pie inferior reservado para NOMBRE y TIPO
    auto bottomTextCard = dInner.removeFromBottom(48.0f);
    auto nameArea = bottomTextCard.removeFromTop(26.0f);
    auto typeArea = bottomTextCard.removeFromTop(20.0f);

    g.setFont(juce::FontOptions("Inter", 16.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText(currentHwDisplayName, nameArea, juce::Justification::centred, true);

    g.setFont(juce::FontOptions("Inter", 12.5f, juce::Font::bold));
    g.setColour(isPluginModeActive ? SoundIdTheme::accentBlue : SoundIdTheme::accentGreen);
    g.drawText("Type: " + currentHwCategory, typeArea, juce::Justification::centred, true);

    dInner.removeFromBottom(6.0f);

    // 4. IMAGEN / VISTA CENTRAL
    auto imageArea = dInner;
    if (modelSvgDrawable != nullptr)
    {
        modelSvgDrawable->drawWithin(g, imageArea, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize, 1.0f);
    }
    else if (modelRasterImage.isValid())
    {
        g.drawImage(modelRasterImage, imageArea, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
    }
    else if (isPluginModeActive)
    {
        g.setColour(SoundIdTheme::accentPurple.withAlpha(0.2f));
        g.fillRoundedRectangle(imageArea.reduced(10.0f, 10.0f), 6.0f);
        g.setColour(SoundIdTheme::accentPurple.withAlpha(0.6f));
        g.drawRoundedRectangle(imageArea.reduced(10.0f, 10.0f), 6.0f, 1.5f);

        g.setFont(juce::FontOptions("Inter", 12.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentPurple);
        g.drawText(juce::String::fromUTF8(u8"PLUGIN VIRTUAL CARGADO\n(Lazo Digital Directo Sin Latencia)"),
                   imageArea, juce::Justification::centred, true);
    }
    else
    {
        g.setColour(SoundIdTheme::borderCard);
        g.drawRoundedRectangle(imageArea.reduced(16.0f, 16.0f), 6.0f, 1.0f);
        g.setFont(juce::FontOptions("Inter", 12.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::textMuted);
        g.drawText(juce::String::fromUTF8(u8"EQUIPO FÍSICO CONECTADO"), imageArea, juce::Justification::centred, true);
    }
}

} // namespace abdaudiolab::gui
