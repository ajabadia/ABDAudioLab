/**
 * @file CenterSplitterBar.h
 * @brief Resizable horizontal splitter bar between CurvePlotter and SuiteList.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SoundIdTheme.h"
#include <functional>

namespace abdaudiolab::gui
{

/**
 * @class CenterSplitterBar
 * @brief Interactive horizontal handle allowing smooth proportional resizing between Graph and Queue.
 */
class CenterSplitterBar : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    std::function<void(int deltaY)> onDragged;
    std::function<void()> onResetToDefault;

    CenterSplitterBar()
    {
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        setTooltip("Drag up/down to resize Graph and Queue \u2022 Double-click to reset split");
    }

    void mouseEnter(const juce::MouseEvent&) override { isHovered = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override  { isHovered = false; repaint(); }

    void mouseDown(const juce::MouseEvent& e) override
    {
        dragStartPos = e.getEventRelativeTo(getParentComponent()).getPosition();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        auto currentPos = e.getEventRelativeTo(getParentComponent()).getPosition();
        int deltaY = currentPos.y - dragStartPos.y;
        dragStartPos = currentPos;
        if (onDragged) onDragged(deltaY);
    }

    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        if (onResetToDefault) onResetToDefault();
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        float cy = b.getCentreY();

        // Divider line
        g.setColour(isHovered ? gui::SoundIdTheme::accentGreen.withAlpha(0.7f) : gui::SoundIdTheme::borderSubtle);
        g.drawLine(b.getX(), cy, b.getRight(), cy, 1.0f);

        // Centered grip handle pill
        float gripW = 44.0f;
        float gripH = 4.0f;
        auto gripRect = juce::Rectangle<float>(b.getCentreX() - gripW * 0.5f, cy - gripH * 0.5f, gripW, gripH);
        g.setColour(isHovered ? gui::SoundIdTheme::accentGreen : gui::SoundIdTheme::textSecondary.withAlpha(0.5f));
        g.fillRoundedRectangle(gripRect, 2.0f);
    }

private:
    bool isHovered { false };
    juce::Point<int> dragStartPos;
};

} // namespace abdaudiolab::gui
