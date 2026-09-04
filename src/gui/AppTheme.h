#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace abdaudiolab::gui
{

/**
 * @brief Theme colour tokens structure supporting runtime Light / Dark switching.
 */
struct ThemeColours
{
    juce::Colour backgroundApp;
    juce::Colour surfaceCard;
    juce::Colour surfaceSubtle;
    juce::Colour surfaceHover;
    juce::Colour surfaceSelected;
    juce::Colour borderSubtle;
    juce::Colour borderCard;
    juce::Colour borderFocus;
    juce::Colour textPrimary;
    juce::Colour textSecondary;
    juce::Colour textMuted;
    juce::Colour accentActive;
    juce::Colour accentActiveHover;
    juce::Colour accentWarning;
    juce::Colour accentError;
    juce::Colour pillBlackBg;
    juce::Colour pillWhiteBg;
};

/**
 * @brief Unified design token system for ABDAudioLab & ABDSynths suite.
 * Inspired by Sonarworks SoundID Reference and Audio Precision ergonomics.
 */
struct AppTheme
{
    enum class ThemeMode { Light, Dark };

    // 1. Surface & Background Tokens
    static inline juce::Colour BackgroundApp     { 0xfff8f9fa }; // Soft technical grey base
    static inline juce::Colour SurfaceCard        { 0xffffffff }; // Container / card pure white
    static inline juce::Colour SurfaceSubtle      { 0xfff1f3f5 }; // Inactive fields, table alt, telemetry chip
    static inline juce::Colour SurfaceHover       { 0xffe9ecef }; // Hover state for rows and chips
    static inline juce::Colour SurfaceSelected    { 0xffe2e8f0 }; // Selection state

    // 2. Borders & Dividers
    static inline juce::Colour BorderSubtle       { 0xffe2e8f0 }; // 1px clean separation lines
    static inline juce::Colour BorderCard         { 0xffcbd5e1 }; // Card boundaries and active controls
    static inline juce::Colour BorderFocus        { 0xff00a86b }; // Focus outline

    // 3. Typography Tokens
    static inline juce::Colour TextPrimary        { 0xff1a1d20 }; // Primary reading / H1 / values
    static inline juce::Colour TextSecondary      { 0xff6c757d }; // Parameter labels, units, muted
    static inline juce::Colour TextMuted          { 0xff94a3b8 }; // Legends, footer, keyboard shortcuts

    // 4. Semantic Accents
    static inline juce::Colour AccentActive       { 0xff00a86b }; // Nominal / Running / Emerald OK
    static inline juce::Colour AccentActiveHover  { 0xff008f5a }; // Hover on primary actions
    static inline juce::Colour AccentWarning      { 0xffe65100 }; // Deep warm amber / THD% / notices
    static inline juce::Colour AccentError        { 0xffd32f2f }; // Critical clip / disconnect / saturation

    // 5. Hero / Pill Elements
    static inline juce::Colour PillBlackBg        { 0xff1a1d20 };
    static inline juce::Colour PillWhiteBg        { 0xffffffff };

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

    static inline ThemeMode currentMode { ThemeMode::Light };
    static inline ThemeColours current {
        juce::Colour(0xfff8f9fa), // BackgroundApp
        juce::Colour(0xffffffff), // SurfaceCard
        juce::Colour(0xfff1f3f5), // SurfaceSubtle
        juce::Colour(0xffe9ecef), // SurfaceHover
        juce::Colour(0xffe2e8f0), // SurfaceSelected
        juce::Colour(0xffe2e8f0), // BorderSubtle
        juce::Colour(0xffcbd5e1), // BorderCard
        juce::Colour(0xff00a86b), // BorderFocus
        juce::Colour(0xff1a1d20), // TextPrimary
        juce::Colour(0xff6c757d), // TextSecondary
        juce::Colour(0xff94a3b8), // TextMuted
        juce::Colour(0xff00a86b), // AccentActive
        juce::Colour(0xff008f5a), // AccentActiveHover
        juce::Colour(0xffe65100), // AccentWarning
        juce::Colour(0xffd32f2f), // AccentError
        juce::Colour(0xff1a1d20), // PillBlackBg
        juce::Colour(0xffffffff)  // PillWhiteBg
    };

    static void setMode(ThemeMode newMode)
    {
        currentMode = newMode;
        if (currentMode == ThemeMode::Dark)
        {
            current = {
                juce::Colour(0xff121417), // BackgroundApp (Dark graphite)
                juce::Colour(0xff1a1d20), // SurfaceCard (Anthracite card)
                juce::Colour(0xff24282d), // SurfaceSubtle (Subtle input / row)
                juce::Colour(0xff2c3138), // SurfaceHover
                juce::Colour(0xff333942), // SurfaceSelected
                juce::Colour(0xff2e343b), // BorderSubtle
                juce::Colour(0xff3d444e), // BorderCard
                juce::Colour(0xff00e676), // BorderFocus
                juce::Colour(0xfff1f3f5), // TextPrimary (Soft crisp white)
                juce::Colour(0xff94a3b8), // TextSecondary (Medium slate)
                juce::Colour(0xff64748b), // TextMuted (Muted grey)
                juce::Colour(0xff00e676), // AccentActive (Vibrant phosphor emerald)
                juce::Colour(0xff00c853), // AccentActiveHover
                juce::Colour(0xffff9100), // AccentWarning (Technical amber)
                juce::Colour(0xffff5252), // AccentError (Warning red)
                juce::Colour(0xff0a0c0e), // PillBlackBg
                juce::Colour(0xff24282d)  // PillWhiteBg (in dark mode subtle surface)
            };
        }
        else
        {
            current = {
                juce::Colour(0xfff8f9fa),
                juce::Colour(0xffffffff),
                juce::Colour(0xfff1f3f5),
                juce::Colour(0xffe9ecef),
                juce::Colour(0xffe2e8f0),
                juce::Colour(0xffe2e8f0),
                juce::Colour(0xffcbd5e1),
                juce::Colour(0xff00a86b),
                juce::Colour(0xff1a1d20),
                juce::Colour(0xff6c757d),
                juce::Colour(0xff94a3b8),
                juce::Colour(0xff00a86b),
                juce::Colour(0xff008f5a),
                juce::Colour(0xffe65100),
                juce::Colour(0xffd32f2f),
                juce::Colour(0xff1a1d20),
                juce::Colour(0xffffffff)
            };
        }

        BackgroundApp    = current.backgroundApp;
        SurfaceCard      = current.surfaceCard;
        SurfaceSubtle    = current.surfaceSubtle;
        SurfaceHover     = current.surfaceHover;
        SurfaceSelected  = current.surfaceSelected;
        BorderSubtle     = current.borderSubtle;
        BorderCard       = current.borderCard;
        BorderFocus      = current.borderFocus;
        TextPrimary      = current.textPrimary;
        TextSecondary    = current.textSecondary;
        TextMuted        = current.textMuted;
        AccentActive     = current.accentActive;
        AccentActiveHover= current.accentActiveHover;
        AccentWarning    = current.accentWarning;
        AccentError      = current.accentError;
        PillBlackBg      = current.pillBlackBg;
        PillWhiteBg      = current.pillWhiteBg;
    }
};

} // namespace abdaudiolab::gui
