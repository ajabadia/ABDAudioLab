/**
 * @file SessionStatusPresenter.cpp
 * @brief Implementation of pure session status presentation mapping.
 * @author ABDSynths
 * @date 2026
 */

#include "SessionStatusPresenter.h"

namespace abdaudiolab::gui::presentation
{

StatusPresentation SessionStatusPresenter::present(gui::SessionState state,
                                                   unsigned progressPercent,
                                                   const juce::String& errorMessage)
{
    StatusPresentation out;

    if (progressPercent > 100)
        progressPercent = 100;

    out.progressPercent = progressPercent;

    switch (state)
    {
    case gui::SessionState::Idle:
        out.badge = "IDLE";
        out.statusText = "Ready";
        out.primaryEnabled = true;
        out.badgeColour = juce::Colour::fromRGBA(0x80, 0x80, 0x80, 0xFF);
        break;

    case gui::SessionState::Starting:
        out.badge = "STARTING";
        out.statusText = "Starting session";
        out.sessionRunning = true;
        out.primaryEnabled = false;
        out.badgeColour = juce::Colour::fromRGBA(0x46, 0x82, 0xB4, 0xFF);
        break;

    case gui::SessionState::Running:
        out.badge = "RUNNING";
        out.statusText = "Session running";
        out.sessionRunning = true;
        out.pauseVisible = true;
        out.cancelVisible = true;
        out.primaryEnabled = true;
        out.badgeColour = juce::Colour::fromRGBA(0x2E, 0x8B, 0x57, 0xFF);
        break;

    case gui::SessionState::Paused:
        out.badge = "PAUSED";
        out.statusText = "Session paused";
        out.sessionRunning = true;
        out.pauseVisible = true;
        out.cancelVisible = true;
        out.primaryEnabled = true;
        out.badgeColour = juce::Colour::fromRGBA(0xD3, 0x9E, 0x00, 0xFF);
        break;

    case gui::SessionState::Capturing:
        out.badge = "CAPTURING";
        out.statusText = "Capturing measurement";
        out.sessionRunning = true;
        out.cancelVisible = true;
        out.primaryEnabled = false;
        out.badgeColour = juce::Colour::fromRGBA(0x41, 0x69, 0xE1, 0xFF);
        break;

    case gui::SessionState::CancelRequested:
        out.badge = "STOPPING";
        out.statusText = "Stopping session";
        out.sessionRunning = true;
        out.cancelVisible = false;
        out.primaryEnabled = false;
        out.badgeColour = juce::Colour::fromRGBA(0xCC, 0x84, 0x00, 0xFF);
        break;

    case gui::SessionState::Aborted:
        out.badge = "ABORTED";
        out.statusText = "Session aborted";
        out.primaryEnabled = true;
        out.badgeColour = juce::Colour::fromRGBA(0xB2, 0x22, 0x22, 0xFF);
        break;

    case gui::SessionState::Completed:
        out.badge = "COMPLETED";
        out.statusText = "Session completed";
        out.primaryEnabled = true;
        out.badgeColour = juce::Colour::fromRGBA(0x22, 0x8B, 0x22, 0xFF);
        break;

    case gui::SessionState::Failed:
        out.badge = "FAILED";
        out.statusText = "Session failed";
        out.bannerText = errorMessage.isNotEmpty() ? errorMessage : "Unknown error";
        out.bannerVisible = true;
        out.primaryEnabled = true;
        out.badgeColour = juce::Colour::fromRGBA(0x8B, 0x00, 0x00, 0xFF);
        out.bannerColour = juce::Colour::fromRGBA(0xFF, 0xCC, 0xCC, 0xFF);
        break;
    }

    return out;
}

} // namespace abdaudiolab::gui::presentation
