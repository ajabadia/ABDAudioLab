#include <catch2/catch_test_macros.hpp>
#include "gui/AppTheme.h"
#include "gui/SoundIdTheme.h"
#include "gui/soundid/SoundIdHardwareCatalogSelector.h"

TEST_CASE("SoundIdTheme - Light and Dark Mode Switching", "[Theme]")
{
    using namespace abdaudiolab::gui;

    SECTION("Apply Dark mode updates all surface, border and text tokens")
    {
        SoundIdTheme soundIdTheme;
        SoundIdTheme::applyThemeMode(AppTheme::ThemeMode::Dark, &soundIdTheme);

        REQUIRE(AppTheme::currentMode == AppTheme::ThemeMode::Dark);
        REQUIRE(AppTheme::BackgroundApp == juce::Colour(0xff121417));
        REQUIRE(AppTheme::SurfaceCard == juce::Colour(0xff1a1d20));
        REQUIRE(AppTheme::SurfaceSubtle == juce::Colour(0xff24282d));
        REQUIRE(AppTheme::SurfaceHover == juce::Colour(0xff2c3138));
        REQUIRE(AppTheme::TextPrimary == juce::Colour(0xfff1f3f5));
        REQUIRE(AppTheme::AccentActive == juce::Colour(0xff00e676));

        // Convenience aliases in SoundIdTheme
        REQUIRE(SoundIdTheme::bgLight == juce::Colour(0xff121417));
        REQUIRE(SoundIdTheme::bgCard == juce::Colour(0xff1a1d20));
        REQUIRE(SoundIdTheme::surfaceSubtle == juce::Colour(0xff24282d));
        REQUIRE(SoundIdTheme::bgCardHover == juce::Colour(0xff2c3138));
    }

    SECTION("Apply Light mode restores clean Scandinavian light tokens")
    {
        SoundIdTheme soundIdTheme;
        SoundIdTheme::applyThemeMode(AppTheme::ThemeMode::Light, &soundIdTheme);

        REQUIRE(AppTheme::currentMode == AppTheme::ThemeMode::Light);
        REQUIRE(AppTheme::BackgroundApp == juce::Colour(0xfff8f9fa));
        REQUIRE(AppTheme::SurfaceCard == juce::Colour(0xffffffff));
        REQUIRE(AppTheme::SurfaceSubtle == juce::Colour(0xfff1f3f5));
        REQUIRE(AppTheme::SurfaceHover == juce::Colour(0xffe9ecef));
        REQUIRE(AppTheme::TextPrimary == juce::Colour(0xff1a1d20));
        REQUIRE(AppTheme::AccentActive == juce::Colour(0xff00a86b));

        // Convenience aliases in SoundIdTheme
        REQUIRE(SoundIdTheme::bgLight == juce::Colour(0xfff8f9fa));
        REQUIRE(SoundIdTheme::bgCard == juce::Colour(0xffffffff));
        REQUIRE(SoundIdTheme::surfaceSubtle == juce::Colour(0xfff1f3f5));
        REQUIRE(SoundIdTheme::bgCardHover == juce::Colour(0xffe9ecef));
    }

    SECTION("SoundIdHardwareCatalogSelector updateTheme executes cleanly in Dark mode")
    {
        SoundIdTheme soundIdTheme;
        SoundIdTheme::applyThemeMode(AppTheme::ThemeMode::Dark, &soundIdTheme);

        SoundIdHardwareCatalogSelector selector;
        selector.updateTheme();

        REQUIRE_FALSE(selector.isCustomOrLibre());
        REQUIRE_FALSE(selector.isLocked());
    }

    SECTION("SoundIdHardwareCatalogSelector updateTheme executes cleanly in Light mode")
    {
        SoundIdTheme soundIdTheme;
        SoundIdTheme::applyThemeMode(AppTheme::ThemeMode::Light, &soundIdTheme);

        SoundIdHardwareCatalogSelector selector;
        selector.updateTheme();

        REQUIRE_FALSE(selector.isCustomOrLibre());
        REQUIRE_FALSE(selector.isLocked());
    }
}
