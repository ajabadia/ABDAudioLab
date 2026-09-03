#pragma once

#include <AutoUpdater/AutoUpdaterConfig.h>
#include <juce_core/juce_core.h>

namespace abdaudiolab::config
{

/**
 * @brief Returns the default configuration for the ABDShared AutoUpdater.
 */
inline ABDShared::AutoUpdaterConfig getAutoUpdaterConfig()
{
    ABDShared::AutoUpdaterConfig cfg;
    cfg.currentVersion = "1.1.0";
    cfg.repoOwner = "ajabadia";
    cfg.repoName = "ABDAudioLab";
    cfg.appName = "ABDAudioLab";
    cfg.userAgent = "ABDAudioLab-AutoUpdater/1.1";
    cfg.checkIntervalHours = 24;
    cfg.checkOnStartup = true;
    cfg.allowPrerelease = false;

    cfg.assetNames.windows = "ABDAudioLab_Setup_x64.exe";
    cfg.assetNames.macos = "ABDAudioLab_macos_universal.dmg";
    cfg.assetNames.linux = "ABDAudioLab_linux_x86_64.AppImage";

    cfg.logCallback = [](const juce::String& msg) {
        juce::Logger::writeToLog("[AutoUpdater] " + msg);
    };

    return cfg;
}

} // namespace abdaudiolab::config
