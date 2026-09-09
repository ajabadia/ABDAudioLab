#include "PlotterModulationTableRenderer.h"
#include "SoundIdTheme.h"

namespace abdaudiolab::gui
{

PlotterModulationTableRenderer::PlotterModulationTableRenderer()
{
    setOpaque(false);
}

void PlotterModulationTableRenderer::setProfile(const math::ModulationMatrixProfile& profile)
{
    currentProfile = profile;
    repaint();
}

void PlotterModulationTableRenderer::updateNode(const math::ModulationNode& node)
{
    currentProfile.setNode(node.sourceID, node.destID, node);
    repaint();
}

void PlotterModulationTableRenderer::clear()
{
    currentProfile = math::ModulationMatrixProfile();
    repaint();
}

void PlotterModulationTableRenderer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto gridBounds = bounds.reduced(16.0f, 12.0f);

    // Card background
    g.setColour(SoundIdTheme::bgCard);
    g.fillRoundedRectangle(gridBounds, 6.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(gridBounds, 6.0f, 1.0f);

    auto allNodes = currentProfile.getAllNodes();

    if (allNodes.empty())
    {
        g.setColour(SoundIdTheme::textMuted);
        g.setFont(juce::FontOptions(13.0f));
        g.drawText("No modulation matrix nodes measured yet.\nConnect hardware and run a modulation probe session.",
                   gridBounds, juce::Justification::centred, true);
        return;
    }

    auto contentArea = gridBounds.reduced(12.0f, 10.0f);
    float rowHeight = 26.0f;
    float currentY = contentArea.getY();

    // Table Header
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);

    float colSrcW = 120.0f;
    float colDstW = 120.0f;
    float colGainW = 90.0f;
    float colOffsetW = 80.0f;
    float colLinW = 130.0f;
    float colBarW = 120.0f;

    float startX = contentArea.getX();
    g.drawText("Source ID", startX, currentY, colSrcW, rowHeight, juce::Justification::centredLeft, true);
    g.drawText("Dest ID", startX + colSrcW, currentY, colDstW, rowHeight, juce::Justification::centredLeft, true);
    g.drawText("Gain (K)", startX + colSrcW + colDstW, currentY, colGainW, rowHeight, juce::Justification::centredLeft, true);
    g.drawText("Offset (c)", startX + colSrcW + colDstW + colGainW, currentY, colOffsetW, rowHeight, juce::Justification::centredLeft, true);
    g.drawText("Linearity (R^2)", startX + colSrcW + colDstW + colGainW + colOffsetW, currentY, colLinW, rowHeight, juce::Justification::centredLeft, true);
    g.drawText("Fit Quality", startX + colSrcW + colDstW + colGainW + colOffsetW + colLinW, currentY, colBarW, rowHeight, juce::Justification::centredLeft, true);

    currentY += rowHeight + 2.0f;
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawHorizontalLine(static_cast<int>(currentY), contentArea.getX(), contentArea.getRight());
    currentY += 4.0f;

    g.setFont(juce::FontOptions(11.5f));

    int rowCounter = 0;
    for (const auto& node : allNodes)
    {
        if (currentY + rowHeight > contentArea.getBottom())
            break;

        auto rowRect = juce::Rectangle<float>(startX, currentY, contentArea.getWidth(), rowHeight);

        // Alternating row subtle background
        if ((rowCounter++) % 2 == 0)
        {
            g.setColour(SoundIdTheme::surfaceSubtle.withAlpha(0.2f));
            g.fillRoundedRectangle(rowRect, 3.0f);
        }

        g.setColour(SoundIdTheme::textPrimary);
        g.drawText("Src #" + juce::String(node.sourceID), startX, currentY, colSrcW, rowHeight, juce::Justification::centredLeft, true);
        g.drawText("Dst #" + juce::String(node.destID), startX + colSrcW, currentY, colDstW, rowHeight, juce::Justification::centredLeft, true);

        // K scalar
        g.setColour(juce::Colour(0xff06b6d4)); // Cyan
        g.drawText(juce::String(node.kScalar, 4), startX + colSrcW + colDstW, currentY, colGainW, rowHeight, juce::Justification::centredLeft, true);

        // Offset c
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText(juce::String(node.offsetC, 2), startX + colSrcW + colDstW + colGainW, currentY, colOffsetW, rowHeight, juce::Justification::centredLeft, true);

        // R^2 badge
        juce::Colour statusColor = (node.rSquared >= 0.95f) ? juce::Colour(0xff10b981) // Emerald Green
                                 : (node.rSquared >= 0.85f) ? juce::Colour(0xfff59e0b) // Amber
                                 : juce::Colour(0xffef4444);                          // Red

        g.setColour(statusColor);
        g.drawText(juce::String(node.rSquared, 4), startX + colSrcW + colDstW + colGainW + colOffsetW, currentY, colLinW, rowHeight, juce::Justification::centredLeft, true);

        // Progress-style Quality Bar
        float barX = startX + colSrcW + colDstW + colGainW + colOffsetW + colLinW;
        float barY = currentY + 6.0f;
        float barH = rowHeight - 12.0f;
        float barTotalW = 100.0f;
        float barFillW = juce::jlimit(0.0f, 1.0f, node.rSquared) * barTotalW;

        g.setColour(SoundIdTheme::borderSubtle);
        g.fillRoundedRectangle(barX, barY, barTotalW, barH, 2.0f);

        g.setColour(statusColor.withAlpha(0.85f));
        g.fillRoundedRectangle(barX, barY, barFillW, barH, 2.0f);

        currentY += rowHeight;
    }
}

} // namespace abdaudiolab::gui
