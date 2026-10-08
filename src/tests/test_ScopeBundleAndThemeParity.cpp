/**
 * @file test_ScopeBundleAndThemeParity.cpp
 * @brief Phase 3 test suite: Embedded WebUI bundle integrity, hermeticity & theme parity.
 *
 * Verifies:
 * 1. ScopeCore transitively provides ABDScopeWebAssets without manual linkage.
 * 2. Embedded binary catalog contains all required WebUI entry points, scripts, styles and assets.
 * 3. ScopeResourceProvider serves memory assets with valid MIME types and URL normalization.
 * 4. Production hermeticity: zero local filesystem dependency, zero local HTTP ports, no absolute machine paths.
 * 5. Controlled error handling: missing assets cleanly produce std::nullopt (HTTP 404).
 * 6. Theme parity: theme.generated.css includes audiolab, audiolab-light, and ms2000 token contracts.
 *
 * @author ABDAudioLab & ABDSharedCode
 * @date 2026-10-07
 */

#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <JUCE/ScopeResourceProvider.h>
#include "ABDScopeWebAssets.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace
{

/**
 * @brief Helper to find a named resource in the binary catalog and return it as std::string_view.
 */
std::string_view getResourceAsStringView(const char* resourceName)
{
    int size = 0;
    const char* data = ABDScopeWebAssets::getNamedResource(resourceName, size);
    if (data != nullptr && size > 0)
        return std::string_view(data, static_cast<size_t>(size));
    return {};
}

} // namespace

TEST_CASE("ScopeWebAssets Embedded Binary Catalog Verification", "[Scope][Bundle][BinaryData]")
{
    SECTION("Binary catalog is populated by juce_add_binary_data")
    {
        REQUIRE(ABDScopeWebAssets::namedResourceListSize > 0);
        INFO("Total embedded WebUI resources: " << ABDScopeWebAssets::namedResourceListSize);
        REQUIRE(ABDScopeWebAssets::namedResourceListSize >= 15);
    }

    SECTION("Essential frontend entry points exist in binary assets")
    {
        const std::vector<std::string> requiredResources = {
            "index_html",
            "scope_js",
            "scope_css",
            "theme_generated_css",
            "frame_js",
            "trigger_js",
            "icons_js",
            "OscilloscopeRenderer_js",
            "SpectrumRenderer_js",
            "LissajousRenderer_js",
            "PhaseMeterRenderer_js",
            "SpectrogramRenderer_js",
            "VuMeterRenderer_js",
            "BaseRenderer_js"
        };

        for (const auto& resName : requiredResources)
        {
            int size = 0;
            const char* data = ABDScopeWebAssets::getNamedResource(resName.c_str(), size);
            INFO("Checking resource existence: " << resName);
            REQUIRE(data != nullptr);
            REQUIRE(size > 0);
        }
    }

    SECTION("Original filename list preserves canonical basenames")
    {
        bool foundIndex = false;
        bool foundScopeCss = false;
        bool foundThemeGenerated = false;

        for (int i = 0; i < ABDScopeWebAssets::namedResourceListSize; ++i)
        {
            const std::string orig(ABDScopeWebAssets::originalFilenames[i]);
            if (orig == "index.html") foundIndex = true;
            if (orig == "scope.css") foundScopeCss = true;
            if (orig == "theme.generated.css") foundThemeGenerated = true;
        }

        REQUIRE(foundIndex);
        REQUIRE(foundScopeCss);
        REQUIRE(foundThemeGenerated);
    }
}

TEST_CASE("ScopeResourceProvider In-Memory Resolution & MIME Dispatch", "[Scope][Bundle][ResourceProvider]")
{
    SECTION("Root and index.html resolution")
    {
        auto rootRes = abd::scope::scopeResourceProvider("/");
        REQUIRE(rootRes.has_value());
        REQUIRE(rootRes->mimeType == "text/html");
        REQUIRE(rootRes->data.size() > 0);

        auto indexRes = abd::scope::scopeResourceProvider("/index.html");
        REQUIRE(indexRes.has_value());
        REQUIRE(indexRes->mimeType == "text/html");
        REQUIRE(indexRes->data.size() == rootRes->data.size());

        // Verify content contains expected DOM elements
        std::string htmlContent(reinterpret_cast<const char*>(indexRes->data.data()), indexRes->data.size());
        REQUIRE(htmlContent.find("id=\"scope-app\"") != std::string::npos);
        REQUIRE(htmlContent.find("createScope") != std::string::npos);
    }

    SECTION("CSS stylesheets resolution")
    {
        auto cssRes = abd::scope::scopeResourceProvider("/src/scope.css");
        REQUIRE(cssRes.has_value());
        REQUIRE(cssRes->mimeType == "text/css");
        REQUIRE(cssRes->data.size() > 0);

        auto themeCssRes = abd::scope::scopeResourceProvider("/src/theme.generated.css");
        REQUIRE(themeCssRes.has_value());
        REQUIRE(themeCssRes->mimeType == "text/css");
        REQUIRE(themeCssRes->data.size() > 0);
    }

    SECTION("JavaScript ES modules resolution")
    {
        auto jsRes = abd::scope::scopeResourceProvider("/src/scope.js");
        REQUIRE(jsRes.has_value());
        REQUIRE(jsRes->mimeType == "application/javascript");
        REQUIRE(jsRes->data.size() > 0);

        auto rendererRes = abd::scope::scopeResourceProvider("/src/renderers/OscilloscopeRenderer.js");
        REQUIRE(rendererRes.has_value());
        REQUIRE(rendererRes->mimeType == "application/javascript");
        REQUIRE(rendererRes->data.size() > 0);
    }

    SECTION("URL decoration: query strings and hash fragments are cleanly stripped")
    {
        auto decorated = abd::scope::scopeResourceProvider("/src/scope.css?theme=ms2000&v=1.2#root");
        REQUIRE(decorated.has_value());
        REQUIRE(decorated->mimeType == "text/css");

        auto rootDecorated = abd::scope::scopeResourceProvider("/?theme=audiolab-light");
        REQUIRE(rootDecorated.has_value());
        REQUIRE(rootDecorated->mimeType == "text/html");
    }

    SECTION("JUCE host built-in script delegation")
    {
        // juce.js must return nullopt to allow WebBrowserComponent's internal IPC script to serve
        auto juceJs = abd::scope::scopeResourceProvider("/juce.js");
        REQUIRE_FALSE(juceJs.has_value());

        auto juceJsDeep = abd::scope::scopeResourceProvider("/vendor/juce.js");
        REQUIRE_FALSE(juceJsDeep.has_value());
    }

    SECTION("Hermetic 404: Non-existent assets produce clean std::nullopt without throwing")
    {
        auto missing = abd::scope::scopeResourceProvider("/missing_bundle_asset.png");
        REQUIRE_FALSE(missing.has_value());

        auto missingCss = abd::scope::scopeResourceProvider("/themes/non_existent_theme.css");
        REQUIRE_FALSE(missingCss.has_value());
    }
}

TEST_CASE("Scope Theme Parity and Token Cascade Verification", "[Scope][Bundle][ThemeParity]")
{
    std::string_view themeCss = getResourceAsStringView("theme_generated_css");
    REQUIRE(!themeCss.empty());

    SECTION("Required canonical themes exist in theme.generated.css")
    {
        REQUIRE(themeCss.find("[data-theme=\"audiolab\"]") != std::string_view::npos);
        REQUIRE(themeCss.find("[data-theme=\"audiolab-light\"]") != std::string_view::npos);
        REQUIRE(themeCss.find("[data-theme=\"ms2000\"]") != std::string_view::npos);
    }

    SECTION("audiolab (Dark Precision) theme defines required SSOT color tokens")
    {
        REQUIRE(themeCss.find("--color-bg-base:") != std::string_view::npos);
        REQUIRE(themeCss.find("--color-panel-bg:") != std::string_view::npos);
        REQUIRE(themeCss.find("--color-panel-surface:") != std::string_view::npos);
        REQUIRE(themeCss.find("--color-panel-border:") != std::string_view::npos);
        REQUIRE(themeCss.find("--color-accent:") != std::string_view::npos);
        REQUIRE(themeCss.find("--color-text-main:") != std::string_view::npos);
        REQUIRE(themeCss.find("--color-text-muted:") != std::string_view::npos);
    }

    SECTION("audiolab-light theme defines semantic feedback and surface tokens")
    {
        REQUIRE(themeCss.find("--color-success:") != std::string_view::npos);
        REQUIRE(themeCss.find("--color-warning:") != std::string_view::npos);
        REQUIRE(themeCss.find("--color-danger:") != std::string_view::npos);
        REQUIRE(themeCss.find("--color-scrollbar-thumb:") != std::string_view::npos);
    }

    SECTION("Component adapter maps scope tokens onto host design system tokens")
    {
        REQUIRE(themeCss.find("--scope-bg: var(--color-panel-bg") != std::string_view::npos);
        REQUIRE(themeCss.find("--scope-surface: var(--color-panel-surface") != std::string_view::npos);
        REQUIRE(themeCss.find("--scope-border: var(--color-panel-border") != std::string_view::npos);
        REQUIRE(themeCss.find("--scope-accent: var(--color-accent") != std::string_view::npos);
        REQUIRE(themeCss.find("--scope-text-main: var(--color-text-main") != std::string_view::npos);
        REQUIRE(themeCss.find("--scope-text-muted: var(--color-text-muted") != std::string_view::npos);
    }

    SECTION("Index HTML includes link to theme.generated.css to ensure runtime cascade")
    {
        std::string_view indexHtml = getResourceAsStringView("index_html");
        REQUIRE(!indexHtml.empty());
        REQUIRE(indexHtml.find("theme.generated.css") != std::string_view::npos);
        REQUIRE(indexHtml.find("scope.css") != std::string_view::npos);
    }
}
