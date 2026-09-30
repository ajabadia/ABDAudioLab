/**
 * @file AssetLocator.h
 * @brief Utilities to locate brand, model, and raster assets across portable directories.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_core/juce_core.h>
#include "core/LabResourcePaths.h"

namespace abdaudiolab::gui
{

inline juce::File locateAssetFile(const juce::String& relPath)
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

    // 1. Direct path
    juce::File f(relPath);
    auto found = findExisting(f);
    if (found.existsAsFile()) return found;

    // 2. Assets compartidos (ABDSharedAssets). La busqueda la hace LabResourcePaths:
    //    antes se repetia aqui con tres niveles fijos desde el ejecutable.
    const auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                            .getParentDirectory();
    const auto sharedAssetsDir = abdaudiolab::core::sharedAssetsDir();

    if (sharedAssetsDir.isDirectory())
    {
        found = findExisting(sharedAssetsDir.getChildFile(relPath));
        if (found.existsAsFile()) return found;

        found = findExisting(sharedAssetsDir.getChildFile("models").getChildFile(relPath));
        if (found.existsAsFile()) return found;
        found = findExisting(sharedAssetsDir.getChildFile("interfaces").getChildFile(relPath));
        if (found.existsAsFile()) return found;
        found = findExisting(sharedAssetsDir.getChildFile("brands").getChildFile(relPath));
        if (found.existsAsFile()) return found;
        found = findExisting(sharedAssetsDir.getChildFile("icons").getChildFile(relPath));
        if (found.existsAsFile()) return found;
        found = findExisting(sharedAssetsDir.getChildFile("models/logos").getChildFile(relPath));
        if (found.existsAsFile()) return found;
    }

    // 3. Raiz del repositorio. Antes se probaba el directorio de trabajo, lo que
    //    hacia que el resultado dependiera de donde se hubiera lanzado la app.
    //    Se usa optionalRepoResource() y NO repoResource(): esta ultima lanza
    //    excepcion si no encuentra la raiz, y un producto ejecutado fuera del
    //    arbol del repositorio abortaria en vez de degradar con elegancia.
    if (abdaudiolab::core::isSafeRepoRelativePath(relPath))
    {
        found = findExisting(abdaudiolab::core::optionalRepoResource(relPath));
        if (found.existsAsFile()) return found;
    }

    // 4. Executable Directory relative path traversal
    found = findExisting(exeDir.getChildFile(relPath));
    if (found.existsAsFile()) return found;

    return {};
}

} // namespace abdaudiolab::gui
