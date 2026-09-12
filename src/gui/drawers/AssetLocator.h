/**
 * @file AssetLocator.h
 * @brief Utilities to locate brand, model, and raster assets across portable directories.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_core/juce_core.h>

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

    // 2. Portable shared assets relative search
    juce::File exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    juce::File sharedAssetsDir = exeDir.getChildFile("../ABDSharedAssets");
    if (!sharedAssetsDir.isDirectory())
        sharedAssetsDir = exeDir.getChildFile("../../ABDSharedAssets");
    if (!sharedAssetsDir.isDirectory())
        sharedAssetsDir = exeDir.getChildFile("../../../ABDSharedAssets");

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

    // 3. Current Working Directory
    found = findExisting(juce::File::getCurrentWorkingDirectory().getChildFile(relPath));
    if (found.existsAsFile()) return found;

    // 4. Executable Directory relative path traversal
    found = findExisting(exeDir.getChildFile(relPath));
    if (found.existsAsFile()) return found;

    // 5. Project root and parent shared assets
    auto projectRoot = exeDir.getParentDirectory().getParentDirectory().getParentDirectory();
    found = findExisting(projectRoot.getChildFile(relPath));
    if (found.existsAsFile()) return found;

    auto sharedRel = projectRoot.getParentDirectory().getChildFile("ABDSharedAssets");
    if (sharedRel.isDirectory())
    {
        found = findExisting(sharedRel.getChildFile(relPath));
        if (found.existsAsFile()) return found;
    }

    return {};
}

} // namespace abdaudiolab::gui
