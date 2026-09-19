/**
 * @file SessionStatusPresenter.h
 * @brief Pure presentation transformer for session execution states, badges, and banners.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../SessionExecutionCoordinator.h"

namespace abdaudiolab::gui::presentation
{

struct StatusPresentation
{
    juce::String badge;
    juce::String statusText;
    juce::String bannerText;

    juce::Colour badgeColour;
    juce::Colour bannerColour;

    bool primaryEnabled { false };
    bool pauseVisible { false };
    bool cancelVisible { false };
    bool bannerVisible { false };
    bool sessionRunning { false };
    unsigned progressPercent { 0 };

    [[nodiscard]] uint32_t getBadgeRgba() const noexcept
    {
        return (static_cast<uint32_t>(badgeColour.getRed()) << 24)
             | (static_cast<uint32_t>(badgeColour.getGreen()) << 16)
             | (static_cast<uint32_t>(badgeColour.getBlue()) << 8)
             | static_cast<uint32_t>(badgeColour.getAlpha());
    }

    [[nodiscard]] uint32_t getBannerRgba() const noexcept
    {
        return (static_cast<uint32_t>(bannerColour.getRed()) << 24)
             | (static_cast<uint32_t>(bannerColour.getGreen()) << 16)
             | (static_cast<uint32_t>(bannerColour.getBlue()) << 8)
             | static_cast<uint32_t>(bannerColour.getAlpha());
    }
};

class SessionStatusPresenter
{
public:
    [[nodiscard]]
    static StatusPresentation present(gui::SessionState state,
                                      unsigned progressPercent = 0,
                                      const juce::String& errorMessage = {});
};

} // namespace abdaudiolab::gui::presentation
