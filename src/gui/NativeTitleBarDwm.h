#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#if JUCE_WINDOWS
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#endif

namespace abdaudiolab::gui
{

/**
 * @brief Helper utility to configure Windows DWM Immersive Dark Mode for native title bars.
 */
inline void applyNativeTitleBarDarkMode(juce::TopLevelWindow* window, bool isDark)
{
    if (window == nullptr)
        return;

#if JUCE_WINDOWS
    if (auto* peer = window->getPeer())
    {
        HWND hwnd = static_cast<HWND>(peer->getNativeHandle());
        if (hwnd != nullptr)
        {
            BOOL dark = isDark ? TRUE : FALSE;
            // 20 is DWMWA_USE_IMMERSIVE_DARK_MODE in Win10 20H1+ & Win11; 19 is older Win10
            if (FAILED(::DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark))))
            {
                ::DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
            }
            // Force redraw of the non-client title bar frame
            ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                           SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }
    }
#else
    juce::ignoreUnused(window, isDark);
#endif
}

} // namespace abdaudiolab::gui
