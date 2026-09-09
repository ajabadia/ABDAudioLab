#include "SoundIdCurvePlotter.h"
#include "Waterfall3DComponent.h"
#include "PlotterModulationTableRenderer.h"
#include "PlotterHeatmapRenderer.h"
#include "PlotterFrequencyCurveRenderer.h"
#include <cmath>

namespace abdaudiolab::gui
{

SoundIdCurvePlotter::SoundIdCurvePlotter()
{
    btnCurve.setButtonText(juce::String::fromUTF8(u8"Curve (\u03bc \u00b1 \u03c3)"));
    btnHeatmap.setButtonText("2D Heatmap");
    btnSpectrum.setButtonText("Spectrum FFT");
    btnPhaseDelay.setButtonText("Phase / GD");
    btnWaterfall3D.setButtonText("3D Mountains");

    btnCurve.setTooltip(juce::String::fromUTF8(u8"Display statistical mean response curve (\u03bc) with shaded confidence band (\u00b1\u03c3)"));
    btnHeatmap.setTooltip("Display 2D parameter excitation grid heatmap with Viridis color scale");
    btnSpectrum.setTooltip("Live FFT spectrum analyzer (20 Hz - 20 kHz, logarithmic)");
    btnPhaseDelay.setTooltip("Display unwrapped phase response and group delay (Farina deconvolution)");
    btnWaterfall3D.setTooltip("Display 3D isometric spectral waterfall landscape (Retro/Mountains)");
    btnModMatrix.setTooltip("Display sparse modulation matrix parameters (Gain K, Offset c, Linearity R^2)");
    btnPaletteToggle.setTooltip("Toggle 3D visual palette: Retro Emerald vs Thermal Fire");

    addAndMakeVisible(btnCurve);
    addAndMakeVisible(btnHeatmap);
    addAndMakeVisible(btnSpectrum);
    addAndMakeVisible(btnPhaseDelay);
    addAndMakeVisible(btnWaterfall3D);
    addAndMakeVisible(btnModMatrix);
    addAndMakeVisible(btnPaletteToggle);
    addChildComponent(spectrumAnalyzer);

    btnToggleCollapse.setButtonText(juce::String::fromUTF8(u8"\u25bc")); // ▼ (pointing down to expand downwards)
    btnToggleCollapse.setTooltip("Maximize / Restore Graph Area - Expand graph area down to maximize or restore balanced split");
    btnToggleCollapse.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    btnToggleCollapse.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
    btnToggleCollapse.onClick = [this] { if (onToggleCollapse) onToggleCollapse(); };
    addAndMakeVisible(btnToggleCollapse);

    btnCurve.onClick = [this] { setViewMode(ViewMode::FrequencyCurve); };
    btnHeatmap.onClick = [this] { setViewMode(ViewMode::Heatmap2D); };
    btnSpectrum.onClick = [this] { setViewMode(ViewMode::SpectrumFFT); };
    btnPhaseDelay.onClick = [this] { setViewMode(ViewMode::PhaseGroupDelay); };
    btnWaterfall3D.onClick = [this] { setViewMode(ViewMode::Waterfall3D); };
    btnModMatrix.onClick = [this] { setViewMode(ViewMode::ModulationMatrix); };

    btnPaletteToggle.onClick = [this] {
        useThermalPalette = !useThermalPalette;
        btnPaletteToggle.setButtonText(useThermalPalette ? "Thermal Fire" : "Retro Emerald");
        if (waterfall3DView != nullptr)
            waterfall3DView->setPaletteMode(useThermalPalette);
        repaint();
    };

    waterfall3DView = std::make_unique<Waterfall3DComponent>();
    addChildComponent(*waterfall3DView);

    modulationTableView = std::make_unique<PlotterModulationTableRenderer>();
    addChildComponent(*modulationTableView);

    heatmapView = std::make_unique<PlotterHeatmapRenderer>();
    addChildComponent(*heatmapView);

    frequencyCurveView = std::make_unique<PlotterFrequencyCurveRenderer>();
    addChildComponent(*frequencyCurveView);

    // 1.7.10 Conmutable Legend Toggles
    toggleMean.setButtonText(juce::String::fromUTF8(u8"Mean (\u03bc)"));
    toggleMean.setTooltip("Conmutar visualización de la curva de respuesta media estimada");
    toggleMean.onClick = [this] { setShowMeanCurve(!showMeanCurve); };
    addAndMakeVisible(toggleMean);

    toggleSigma.setButtonText(juce::String::fromUTF8(u8"\u00b1\u03c3 Band"));
    toggleSigma.setTooltip(juce::String::fromUTF8(u8"Conmutar visualización de la banda de tolerancia/dispersión \u00b11\u03c3"));
    toggleSigma.onClick = [this] { setShowSigmaBand(!showSigmaBand); };
    addAndMakeVisible(toggleSigma);

    toggleThd.setButtonText("THD %");
    toggleThd.setTooltip("Conmutar visualización de los nodos de distorsión armónica total (THD)");
    toggleThd.onClick = [this] { setShowThdPoints(!showThdPoints); };
    addAndMakeVisible(toggleThd);

    togglePhase.setButtonText("Phase (rad)");
    togglePhase.setTooltip("Conmutar curva de respuesta de fase");
    togglePhase.onClick = [this] { setShowPhaseCurve(!showPhaseCurve); };
    addChildComponent(togglePhase);

    toggleGroupDelay.setButtonText("Group Delay");
    toggleGroupDelay.setTooltip("Conmutar curva de retardo de grupo (ms)");
    toggleGroupDelay.onClick = [this] { setShowGroupDelayCurve(!showGroupDelayCurve); };
    addChildComponent(toggleGroupDelay);

    updateLegendToggleStyles();

    setViewMode(ViewMode::FrequencyCurve);
}

SoundIdCurvePlotter::~SoundIdCurvePlotter() = default;

void SoundIdCurvePlotter::setMeasuringState(bool measuring, float progress)
{
    isMeasuring = measuring;
    measuringProgress = std::clamp(progress, 0.0f, 1.0f);
    if (frequencyCurveView != nullptr)
        frequencyCurveView->setMeasuringProgress(measuring, measuringProgress);
    repaint();
}

void SoundIdCurvePlotter::setShowMeanCurve(bool show) noexcept
{
    showMeanCurve = show;
    if (frequencyCurveView != nullptr)
        frequencyCurveView->setShowMeanCurve(show);
    updateLegendToggleStyles();
    repaint();
}

void SoundIdCurvePlotter::setShowSigmaBand(bool show) noexcept
{
    showSigmaBand = show;
    if (frequencyCurveView != nullptr)
        frequencyCurveView->setShowSigmaBand(show);
    updateLegendToggleStyles();
    repaint();
}

void SoundIdCurvePlotter::setShowThdPoints(bool show) noexcept
{
    showThdPoints = show;
    if (frequencyCurveView != nullptr)
        frequencyCurveView->setShowThdPoints(show);
    updateLegendToggleStyles();
    repaint();
}

void SoundIdCurvePlotter::setShowPhaseCurve(bool show) noexcept
{
    showPhaseCurve = show;
    updateLegendToggleStyles();
    repaint();
}

void SoundIdCurvePlotter::setShowGroupDelayCurve(bool show) noexcept
{
    showGroupDelayCurve = show;
    updateLegendToggleStyles();
    repaint();
}

void SoundIdCurvePlotter::setPhaseData(const std::vector<float>& freqsHz,
                                      const std::vector<float>& phaseRad,
                                      const std::vector<float>& groupDelaySamples)
{
    std::lock_guard<std::mutex> lock(pointsMutex);
    phaseFreqs = freqsHz;
    phaseDataRad = phaseRad;
    groupDelayDataSamples = groupDelaySamples;
    juce::MessageManager::callAsync([this] { repaint(); });
}

void SoundIdCurvePlotter::set3DIsometricOffsets(float xOffset, float yOffset) noexcept
{
    if (waterfall3DView != nullptr)
        waterfall3DView->setIsometricOffsets(xOffset, yOffset);
}

float SoundIdCurvePlotter::get3DXOffset() const noexcept
{
    return waterfall3DView != nullptr ? waterfall3DView->getXOffset() : 1.5f;
}

float SoundIdCurvePlotter::get3DYOffset() const noexcept
{
    return waterfall3DView != nullptr ? waterfall3DView->getYOffset() : 2.0f;
}

void SoundIdCurvePlotter::set3DZoomFactor(float zoom) noexcept
{
    if (waterfall3DView != nullptr)
        waterfall3DView->setZoomFactor(zoom);
}

float SoundIdCurvePlotter::get3DZoomFactor() const noexcept
{
    return waterfall3DView != nullptr ? waterfall3DView->getZoomFactor() : 1.0f;
}

void SoundIdCurvePlotter::reset3DCamera() noexcept
{
    if (waterfall3DView != nullptr)
        waterfall3DView->resetCamera();
}

void SoundIdCurvePlotter::updateLegendToggleStyles()
{
    auto applyPillStyle = [](juce::TextButton& btn, bool active, juce::Colour activeColour)
    {
        if (active)
        {
            btn.setColour(juce::TextButton::buttonColourId, activeColour.withAlpha(0.18f));
            btn.setColour(juce::TextButton::textColourOffId, activeColour);
        }
        else
        {
            btn.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
            btn.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textMuted);
        }
    };

    applyPillStyle(toggleMean, showMeanCurve, SoundIdTheme::accentGreen);
    applyPillStyle(toggleSigma, showSigmaBand, SoundIdTheme::accentPurple);
    applyPillStyle(toggleThd, showThdPoints, SoundIdTheme::accentAmber);
    applyPillStyle(togglePhase, showPhaseCurve, juce::Colour(0xff06b6d4)); // Cyan
    applyPillStyle(toggleGroupDelay, showGroupDelayCurve, juce::Colour(0xfff97316)); // Orange
}

void SoundIdCurvePlotter::setCollapsed(bool collapsed)
{
    isCollapsed = collapsed;
    btnToggleCollapse.setButtonText(isCollapsed ? juce::String::fromUTF8(u8"\u25bc") : juce::String::fromUTF8(u8"\u25b2"));
    btnCurve.setVisible(!isCollapsed);
    btnHeatmap.setVisible(!isCollapsed);
    btnSpectrum.setVisible(!isCollapsed);
    toggleMean.setVisible(!isCollapsed && currentView == ViewMode::FrequencyCurve);
    toggleSigma.setVisible(!isCollapsed && currentView == ViewMode::FrequencyCurve);
    toggleThd.setVisible(!isCollapsed && currentView == ViewMode::FrequencyCurve);

    if (isCollapsed && spectrumAnalyzer.isVisible())
        spectrumAnalyzer.setVisible(false);
    else if (!isCollapsed && currentView == ViewMode::SpectrumFFT)
        spectrumAnalyzer.setVisible(true);
    repaint();
    resized();
}

void SoundIdCurvePlotter::setChevronGlyph(const juce::String& glyph)
{
    btnToggleCollapse.setButtonText(glyph);
}

void SoundIdCurvePlotter::clear()
{
    std::lock_guard<std::mutex> lock(pointsMutex);
    points.clear();
    highlightedPointIndex = -1;
    if (frequencyCurveView != nullptr)
        frequencyCurveView->clear();
    if (heatmapView != nullptr)
        heatmapView->clear();
    spectrumAnalyzer.clearFrozenSpectrum();
    repaint();
}

void SoundIdCurvePlotter::addMeasuredPoint(const exporting::MeasuredPoint& point)
{
    {
        std::lock_guard<std::mutex> lock(pointsMutex);
        points.push_back(point);
        if (frequencyCurveView != nullptr)
            frequencyCurveView->setPoints(points);
        if (heatmapView != nullptr)
            heatmapView->setPoints(points);
    }
    juce::MessageManager::callAsync([this] { repaint(); });
}

void SoundIdCurvePlotter::setPoints(const std::vector<exporting::MeasuredPoint>& newPoints)
{
    std::lock_guard<std::mutex> lock(pointsMutex);
    points = newPoints;
    highlightedPointIndex = -1;
    if (frequencyCurveView != nullptr)
        frequencyCurveView->setPoints(points);
    if (heatmapView != nullptr)
        heatmapView->setPoints(points);
    juce::MessageManager::callAsync([this] { repaint(); });
}

void SoundIdCurvePlotter::patchPoint(int index, const exporting::MeasuredPoint& point)
{
    std::lock_guard<std::mutex> lock(pointsMutex);
    if (index >= 0 && index < static_cast<int>(points.size()))
    {
        points[static_cast<size_t>(index)] = point;
    }
    else
    {
        points.push_back(point);
    }
    if (frequencyCurveView != nullptr)
        frequencyCurveView->setPoints(points);
    if (heatmapView != nullptr)
        heatmapView->setPoints(points);
    juce::MessageManager::callAsync([this] { repaint(); });
}

void SoundIdCurvePlotter::removePoint(int index)
{
    std::lock_guard<std::mutex> lock(pointsMutex);
    if (index >= 0 && index < static_cast<int>(points.size()))
    {
        points.erase(points.begin() + index);
        highlightedPointIndex = -1;
    }
    if (frequencyCurveView != nullptr)
        frequencyCurveView->setPoints(points);
    if (heatmapView != nullptr)
        heatmapView->setPoints(points);
    juce::MessageManager::callAsync([this] { repaint(); });
}

void SoundIdCurvePlotter::setHighlightedPointIndex(int index)
{
    std::lock_guard<std::mutex> lock(pointsMutex);
    highlightedPointIndex = index;
    if (frequencyCurveView != nullptr)
        frequencyCurveView->setHighlightedPointIndex(index);
    juce::MessageManager::callAsync([this] { repaint(); });
}

void SoundIdCurvePlotter::setViewMode(ViewMode mode)
{
    currentView = mode;

    auto setTab = [](juce::TextButton& btn, bool active) {
        btn.setColour(juce::TextButton::buttonColourId, active ? SoundIdTheme::pillBlackBg : SoundIdTheme::pillWhiteBg);
        btn.setColour(juce::TextButton::textColourOffId, active ? juce::Colours::white : SoundIdTheme::textPrimary);
    };

    setTab(btnCurve, mode == ViewMode::FrequencyCurve);
    setTab(btnHeatmap, mode == ViewMode::Heatmap2D);
    setTab(btnSpectrum, mode == ViewMode::SpectrumFFT);
    setTab(btnPhaseDelay, mode == ViewMode::PhaseGroupDelay);
    setTab(btnWaterfall3D, mode == ViewMode::Waterfall3D);
    setTab(btnModMatrix, mode == ViewMode::ModulationMatrix);

    btnPaletteToggle.setVisible(!isCollapsed && (mode == ViewMode::Waterfall3D || mode == ViewMode::Heatmap2D));

    bool showFreqLegendToggles = (!isCollapsed && mode == ViewMode::FrequencyCurve);
    toggleMean.setVisible(showFreqLegendToggles);
    toggleSigma.setVisible(showFreqLegendToggles);
    toggleThd.setVisible(showFreqLegendToggles);

    bool showPhaseToggles = (!isCollapsed && mode == ViewMode::PhaseGroupDelay);
    togglePhase.setVisible(showPhaseToggles);
    toggleGroupDelay.setVisible(showPhaseToggles);

    spectrumAnalyzer.setVisible(mode == ViewMode::SpectrumFFT);
    if (waterfall3DView != nullptr)
        waterfall3DView->setVisible(mode == ViewMode::Waterfall3D);
    if (modulationTableView != nullptr)
        modulationTableView->setVisible(mode == ViewMode::ModulationMatrix);
    if (heatmapView != nullptr)
        heatmapView->setVisible(mode == ViewMode::Heatmap2D);
    if (frequencyCurveView != nullptr)
        frequencyCurveView->setVisible(mode == ViewMode::FrequencyCurve);

    repaint();
    resized();
}

void SoundIdCurvePlotter::setModulationProfile(const math::ModulationMatrixProfile& profile)
{
    currentModProfile = profile;
    if (modulationTableView != nullptr)
        modulationTableView->setProfile(profile);
    juce::MessageManager::callAsync([this] { repaint(); });
}

void SoundIdCurvePlotter::updateModulationNode(const math::ModulationNode& node)
{
    currentModProfile.setNode(node.sourceID, node.destID, node);
    if (modulationTableView != nullptr)
        modulationTableView->updateNode(node);
    juce::MessageManager::callAsync([this] { repaint(); });
}

void SoundIdCurvePlotter::setPreScanTrajectory(const math::PreScanResult& preScan)
{
    std::lock_guard<std::mutex> lock(pointsMutex);
    preScanTrajectory = preScan.trajectory;
    preScanRoadmapSteps = preScan.recommendedSteps;
    hasPreScanData = !preScanTrajectory.empty();
    if (waterfall3DView != nullptr)
        waterfall3DView->updateTrajectoryData(preScanTrajectory);
    if (frequencyCurveView != nullptr)
        frequencyCurveView->setPreScanData(preScanTrajectory, preScanRoadmapSteps, showPreScanGhost);
    juce::MessageManager::callAsync([this] { repaint(); });
}

void SoundIdCurvePlotter::clearPreScanData()
{
    std::lock_guard<std::mutex> lock(pointsMutex);
    preScanTrajectory.clear();
    preScanRoadmapSteps.clear();
    hasPreScanData = false;
    if (waterfall3DView != nullptr)
        waterfall3DView->clearData();
    if (frequencyCurveView != nullptr)
        frequencyCurveView->clearPreScanData();
    juce::MessageManager::callAsync([this] { repaint(); });
}

void SoundIdCurvePlotter::updateTheme()
{
    setViewMode(currentView);
    btnToggleCollapse.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
    updateLegendToggleStyles();
    spectrumAnalyzer.repaint();
    repaint();
}

void SoundIdCurvePlotter::resized()
{
    auto area = getLocalBounds();
    auto topBar = area.removeFromTop(32).reduced(4, 2);

    btnToggleCollapse.setBounds(topBar.removeFromRight(26).withSizeKeepingCentre(22, 22));
    topBar.removeFromRight(4);

    if (!isCollapsed)
    {
        if (currentView == ViewMode::Waterfall3D || currentView == ViewMode::Heatmap2D)
        {
            btnPaletteToggle.setBounds(topBar.removeFromRight(106));
            topBar.removeFromRight(4);
        }

        btnModMatrix.setBounds(topBar.removeFromRight(96));
        topBar.removeFromRight(4);
        btnWaterfall3D.setBounds(topBar.removeFromRight(106));
        topBar.removeFromRight(4);
        btnPhaseDelay.setBounds(topBar.removeFromRight(88));
        topBar.removeFromRight(4);
        btnSpectrum.setBounds(topBar.removeFromRight(102));
        topBar.removeFromRight(4);
        btnHeatmap.setBounds(topBar.removeFromRight(92));
        topBar.removeFromRight(4);
        btnCurve.setBounds(topBar.removeFromRight(102));
        topBar.removeFromRight(8);

        // Conmutable Legend Toggles layout
        if (currentView == ViewMode::FrequencyCurve)
        {
            toggleThd.setBounds(topBar.removeFromRight(56));
            topBar.removeFromRight(4);
            toggleSigma.setBounds(topBar.removeFromRight(76));
            topBar.removeFromRight(4);
            toggleMean.setBounds(topBar.removeFromRight(76));
        }
        else if (currentView == ViewMode::PhaseGroupDelay)
        {
            toggleGroupDelay.setBounds(topBar.removeFromRight(86));
            topBar.removeFromRight(4);
            togglePhase.setBounds(topBar.removeFromRight(82));
        }

        if (currentView == ViewMode::SpectrumFFT)
        {
            spectrumAnalyzer.setBounds(area.reduced(4, 0));
        }
        else if (currentView == ViewMode::Waterfall3D)
        {
            if (waterfall3DView != nullptr)
                waterfall3DView->setBounds(area.reduced(4, 2));
        }
        else if (currentView == ViewMode::ModulationMatrix)
        {
            if (modulationTableView != nullptr)
                modulationTableView->setBounds(area.reduced(4, 2));
        }
        else if (currentView == ViewMode::Heatmap2D)
        {
            if (heatmapView != nullptr)
                heatmapView->setBounds(area.reduced(4, 2));
        }
        else if (currentView == ViewMode::FrequencyCurve)
        {
            if (frequencyCurveView != nullptr)
                frequencyCurveView->setBounds(area.reduced(4, 2));
        }
    }
}

void SoundIdCurvePlotter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Theme Card Background with rounded corners & border
    g.setColour(SoundIdTheme::bgCard);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

    // Reserve right side for buttons so legend never overlaps tab buttons or toggle pills
    float reservedRight = isCollapsed ? 36.0f : (currentView == ViewMode::FrequencyCurve ? 650.0f : (currentView == ViewMode::PhaseGroupDelay ? 600.0f : 530.0f));
    auto headerArea = bounds.removeFromTop(32.0f).reduced(12.0f, 0.0f).withTrimmedRight(reservedRight);
    drawLegend(g, headerArea);

    // If collapsed, only header is rendered
    if (isCollapsed)
        return;

    // Skip painting plot content when spectrum is active (it's a child component)
    if (currentView == ViewMode::SpectrumFFT)
        return;

    auto plotArea = bounds.reduced(8.0f, 6.0f);

    if (currentView == ViewMode::PhaseGroupDelay)
    {
        drawPhaseGroupDelayPlot(g, plotArea);
    }
    else if (currentView == ViewMode::Waterfall3D)
    {
        // Rendered autonomously by child component waterfall3DView
    }
    else if (currentView == ViewMode::ModulationMatrix)
    {
        // Rendered autonomously by child component modulationTableView
    }
    else if (currentView == ViewMode::Heatmap2D)
    {
        // Rendered autonomously by child component heatmapView
    }
    else if (currentView == ViewMode::FrequencyCurve)
    {
        // Rendered autonomously by child component frequencyCurveView
    }
}

void SoundIdCurvePlotter::mouseDown(const juce::MouseEvent&)
{
}

void SoundIdCurvePlotter::mouseDrag(const juce::MouseEvent&)
{
}

void SoundIdCurvePlotter::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&)
{
}

void SoundIdCurvePlotter::mouseDoubleClick(const juce::MouseEvent&)
{
}

void SoundIdCurvePlotter::mouseMove(const juce::MouseEvent&)
{
}

void SoundIdCurvePlotter::mouseExit(const juce::MouseEvent&)
{
}

void SoundIdCurvePlotter::drawLegend(juce::Graphics& g, juce::Rectangle<float> legendArea)
{
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);

    juce::String title;
    if (currentView == ViewMode::FrequencyCurve)
        title = "Parameter & Response Curves";
    else if (currentView == ViewMode::Heatmap2D)
        title = "2D Parameter Excitation Heatmap";
    else if (currentView == ViewMode::PhaseGroupDelay)
        title = "Phase Response & Group Delay";
    else if (currentView == ViewMode::Waterfall3D)
        title = "3D Mountains (Drag: Rotate, Wheel: Zoom, Double-Click: Reset)";
    else if (currentView == ViewMode::ModulationMatrix)
        title = "Sparse Modulation Matrix Inspector (K, c, R^2)";
    else
        title = "Live FFT Spectrum Analyzer";

    // Compact title width
    g.drawText(title, legendArea, juce::Justification::centredLeft, true);
}

void SoundIdCurvePlotter::drawPhaseGroupDelayPlot(juce::Graphics& g, juce::Rectangle<float> plotArea)
{
    auto gridBounds = plotArea.withTrimmedRight(48.0f).withTrimmedBottom(18.0f).withTrimmedLeft(40.0f);
    lastGridBounds = gridBounds;

    // Background grid lines (Phase axis on left: -pi to +pi)
    float phaseVals[] = { 3.14159f, 1.57079f, 0.0f, -1.57079f, -3.14159f };
    const char* phaseLabels[] = { "+pi", "+pi/2", "0", "-pi/2", "-pi" };

    g.setFont(juce::FontOptions(10.0f));
    for (int i = 0; i < 5; ++i)
    {
        float normY = static_cast<float>(i) / 4.0f;
        float y = gridBounds.getY() + normY * gridBounds.getHeight();

        if (i == 2)
        {
            g.setColour(SoundIdTheme::textPrimary.withAlpha(0.6f));
            g.drawHorizontalLine(static_cast<int>(y), gridBounds.getX(), gridBounds.getRight());
        }
        else
        {
            g.setColour(SoundIdTheme::borderSubtle);
            g.drawHorizontalLine(static_cast<int>(y), gridBounds.getX(), gridBounds.getRight());
        }

        // Left label (Phase rad)
        g.setColour(juce::Colour(0xff06b6d4)); // Cyan
        g.drawText(phaseLabels[i], static_cast<int>(gridBounds.getX() - 36.0f), static_cast<int>(y - 6.0f), 32, 12, juce::Justification::centredRight, false);
    }

    // Right labels (Group Delay ms: 0ms to 20ms)
    float gdVals[] = { 20.0f, 15.0f, 10.0f, 5.0f, 0.0f };
    for (int i = 0; i < 5; ++i)
    {
        float normY = static_cast<float>(i) / 4.0f;
        float y = gridBounds.getY() + normY * gridBounds.getHeight();
        g.setColour(juce::Colour(0xfff97316)); // Orange
        g.drawText(juce::String(gdVals[i], 0) + "ms", static_cast<int>(gridBounds.getRight() + 6.0f), static_cast<int>(y - 6.0f), 38, 12, juce::Justification::centredLeft, false);
    }

    // Vertical frequency grid lines
    auto drawFreqLine = [&](float freqHz, const juce::String& label)
    {
        float minF = 20.0f, maxF = 20000.0f;
        float normX = (std::log10(freqHz) - std::log10(minF)) / (std::log10(maxF) - std::log10(minF));
        float x = gridBounds.getX() + normX * gridBounds.getWidth();

        g.setColour(SoundIdTheme::borderSubtle);
        g.drawVerticalLine(static_cast<int>(x), gridBounds.getY(), gridBounds.getBottom());

        g.setColour(SoundIdTheme::textMuted);
        g.drawText(label, static_cast<int>(x - 20.0f), static_cast<int>(gridBounds.getBottom() + 4.0f), 40, 14, juce::Justification::centred, false);
    };

    drawFreqLine(100.0f, "100 Hz");
    drawFreqLine(1000.0f, "1 kHz");
    drawFreqLine(10000.0f, "10 kHz");

    std::lock_guard<std::mutex> lock(pointsMutex);
    if (phaseFreqs.empty() || (phaseDataRad.empty() && groupDelayDataSamples.empty()))
    {
        g.setColour(SoundIdTheme::textMuted);
        g.setFont(juce::FontOptions(13.0f));
        g.drawText("No Farina impulse response deconvolution available for Phase/GD analysis.", gridBounds, juce::Justification::centred, true);
        return;
    }

    float minF = 20.0f, maxF = 20000.0f;
    float logMinF = std::log10(minF);
    float logMaxF = std::log10(maxF);

    juce::Graphics::ScopedSaveState clipSave(g);
    g.reduceClipRegion(gridBounds.toNearestInt());

    // Render Phase Curve
    if (showPhaseCurve && !phaseDataRad.empty())
    {
        juce::Path phasePath;
        bool started = false;

        for (size_t i = 0; i < phaseFreqs.size() && i < phaseDataRad.size(); ++i)
        {
            float f = phaseFreqs[i];
            if (f < minF || f > maxF)
                continue;

            float normX = (std::log10(f) - logMinF) / (logMaxF - logMinF);
            float px = gridBounds.getX() + normX * gridBounds.getWidth();

            // Wrap phase to [-pi, pi] for display
            constexpr float kPi = 3.14159265f;
            constexpr float kTwoPi = 6.2831853f;
            float wrappedPhase = std::remainder(phaseDataRad[i], kTwoPi);
            float normY = (kPi - wrappedPhase) / kTwoPi;
            normY = std::clamp(normY, 0.0f, 1.0f);
            float py = gridBounds.getY() + normY * gridBounds.getHeight();

            if (!started)
            {
                phasePath.startNewSubPath(px, py);
                started = true;
            }
            else
            {
                phasePath.lineTo(px, py);
            }
        }

        g.setColour(juce::Colour(0xff06b6d4)); // Cyan
        g.strokePath(phasePath, juce::PathStrokeType(2.0f));
    }

    // Render Group Delay Curve
    if (showGroupDelayCurve && !groupDelayDataSamples.empty())
    {
        juce::Path gdPath;
        bool started = false;
        double sampleRate = 96000.0; // Standard reference

        for (size_t i = 0; i < phaseFreqs.size() && i < groupDelayDataSamples.size(); ++i)
        {
            float f = phaseFreqs[i];
            if (f < minF || f > maxF)
                continue;

            float normX = (std::log10(f) - logMinF) / (logMaxF - logMinF);
            float px = gridBounds.getX() + normX * gridBounds.getWidth();

            float gdMs = (groupDelayDataSamples[i] / static_cast<float>(sampleRate)) * 1000.0f;
            float normY = std::clamp((20.0f - gdMs) / 20.0f, 0.0f, 1.0f);
            float py = gridBounds.getY() + normY * gridBounds.getHeight();

            if (!started)
            {
                gdPath.startNewSubPath(px, py);
                started = true;
            }
            else
            {
                gdPath.lineTo(px, py);
            }
        }

        g.setColour(juce::Colour(0xfff97316)); // Orange
        g.strokePath(gdPath, juce::PathStrokeType(1.8f));
    }
}

} // namespace abdaudiolab::gui
