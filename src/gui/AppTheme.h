#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace abdaudiolab::gui
{

/**
 * @brief Unified design token system for ABDAudioLab & ABDSynths suite ("Técnica Refinada").
 * Inspired by Sonarworks SoundID Reference and Audio Precision ergonomics.
 */
struct AppTheme
{
    // 1. Surface & Background Tokens
    static inline const juce::Colour BackgroundApp     { 0xfff8f9fa }; // Soft technical grey base
    static inline const juce::Colour SurfaceCard        { 0xffffffff }; // Container / card pure white
    static inline const juce::Colour SurfaceSubtle      { 0xfff1f3f5 }; // Inactive fields, table alt, telemetry chip
    static inline const juce::Colour SurfaceHover       { 0xffe9ecef }; // Hover state for rows and chips
    static inline const juce::Colour SurfaceSelected    { 0xffe2e8f0 }; // Selection state

    // 2. Borders & Dividers
    static inline const juce::Colour BorderSubtle       { 0xffe2e8f0 }; // 1px clean separation lines
    static inline const juce::Colour BorderCard         { 0xffcbd5e1 }; // Card boundaries and active controls
    static inline const juce::Colour BorderFocus        { 0xff00a86b }; // Focus outline

    // 3. Typography Tokens
    static inline const juce::Colour TextPrimary        { 0xff1a1d20 }; // Primary reading / H1 / values
    static inline const juce::Colour TextSecondary      { 0xff6c757d }; // Parameter labels, units, muted
    static inline const juce::Colour TextMuted          { 0xff94a3b8 }; // Legends, footer, keyboard shortcuts

    // 4. Semantic Accents
    static inline const juce::Colour AccentActive       { 0xff00a86b }; // Nominal / Running / Emerald OK
    static inline const juce::Colour AccentActiveHover  { 0xff008f5a }; // Hover on primary actions
    static inline const juce::Colour AccentWarning      { 0xffe65100 }; // Deep warm amber / THD% / notices
    static inline const juce::Colour AccentError        { 0xffd32f2f }; // Critical clip / disconnect / saturation

    // 5. Hero / Pill Elements
    static inline const juce::Colour PillBlackBg        { 0xff1a1d20 };
    static inline const juce::Colour PillWhiteBg        { 0xffffffff };

    // 6. Geometry & Metrics
    static constexpr float cornerPill                   { 999.0f };
    static constexpr float cornerCard                   { 8.0f };
    static constexpr float cornerControl                { 6.0f };
    static constexpr float cornerModal                  { 12.0f };
    static constexpr float CardCornerRadius             { 12.0f };

    // 7. Typography Helpers
    static inline juce::Font fontRegular(float size) { return juce::FontOptions("Inter", size, juce::Font::plain); }
    static inline juce::Font fontBold(float size)    { return juce::FontOptions("Inter", size, juce::Font::bold); }
    static inline juce::Font fontMono(float size)    { return juce::FontOptions("Roboto Mono", size, juce::Font::plain); }
};

} // namespace abdaudiolab::gui
