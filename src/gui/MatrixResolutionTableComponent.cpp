#include "MatrixResolutionTableComponent.h"
#include <algorithm>
#include <cmath>

namespace abdaudiolab::gui
{

namespace
{
enum ColumnId
{
    colIcon = 1,
    colParam = 2,
    colResolution = 3,
    colSteps = 4,
    colMin = 5,
    colMax = 6,
    colOrder = 7
};

bool isStandardStep(int step) noexcept
{
    return step == 1 || step == 3 || step == 5 || step == 8 || step == 16 || step == 32 || step == 64;
}
} // namespace

// ============================================================================
// OrderButtonsComponent
// ============================================================================
MatrixResolutionTableComponent::OrderButtonsComponent::OrderButtonsComponent()
{
    btnUp.setTooltip("Move parameter up in sweep execution order");
    btnUp.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    btnUp.setColour(juce::TextButton::textColourOffId, AppTheme::TextSecondary);
    addAndMakeVisible(btnUp);

    btnDown.setTooltip("Move parameter down in sweep execution order");
    btnDown.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    btnDown.setColour(juce::TextButton::textColourOffId, AppTheme::TextSecondary);
    addAndMakeVisible(btnDown);
}

void MatrixResolutionTableComponent::OrderButtonsComponent::resized()
{
    auto b = getLocalBounds();
    int h = b.getHeight() / 2;
    btnUp.setBounds(b.removeFromTop(h));
    btnDown.setBounds(b);
}

// ============================================================================
// MatrixResolutionTableComponent
// ============================================================================
MatrixResolutionTableComponent::MatrixResolutionTableComponent()
{
    matrixTable.setModel(this);
    matrixTable.setRowHeight(34);
    matrixTable.setHeaderHeight(26);
    matrixTable.setColour(juce::ListBox::backgroundColourId, SoundIdTheme::bgCard);
    matrixTable.setColour(juce::ListBox::outlineColourId, SoundIdTheme::borderSubtle);
    matrixTable.setOutlineThickness(1);

    auto& hdr = matrixTable.getHeader();
    hdr.addColumn("ICON", colIcon, 32, 28, 40, juce::TableHeaderComponent::notSortable);
    hdr.addColumn("PARAMETER", colParam, 170, 120, 260, juce::TableHeaderComponent::notSortable);
    hdr.addColumn("STEP RESOLUTION", colResolution, 150, 130, 220, juce::TableHeaderComponent::notSortable);
    hdr.addColumn("STEPS", colSteps, 55, 45, 75, juce::TableHeaderComponent::notSortable);
    hdr.addColumn("MIN %", colMin, 52, 45, 75, juce::TableHeaderComponent::notSortable);
    hdr.addColumn("MAX %", colMax, 52, 45, 75, juce::TableHeaderComponent::notSortable);
    hdr.addColumn("ORDER", colOrder, 46, 40, 60, juce::TableHeaderComponent::notSortable);

    addAndMakeVisible(matrixTable);
}

void MatrixResolutionTableComponent::setControls(const std::vector<ControlStepConfig>& controls)
{
    controlList = controls;
    matrixTable.updateContent();
    matrixTable.repaint();
}

void MatrixResolutionTableComponent::updateTheme()
{
    matrixTable.setColour(juce::ListBox::backgroundColourId, SoundIdTheme::bgCard);
    matrixTable.setColour(juce::ListBox::outlineColourId, SoundIdTheme::borderSubtle);
    matrixTable.updateContent();
    matrixTable.repaint();
}

int MatrixResolutionTableComponent::getPreferredHeight() const
{
    int numRows = static_cast<int>(controlList.size());
    int tableContentH = 26 + std::max(1, numRows) * 34 + 6;
    return std::clamp(tableContentH, 94, 260);
}

void MatrixResolutionTableComponent::resized()
{
    matrixTable.setBounds(getLocalBounds());
}

// ============================================================================
// TableListBoxModel Implementation
// ============================================================================
int MatrixResolutionTableComponent::getNumRows()
{
    return static_cast<int>(controlList.size());
}

void MatrixResolutionTableComponent::paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected)
{
    if (rowIsSelected)
    {
        g.fillAll(AppTheme::SurfaceHover);
    }
    else if (rowNumber % 2 == 1)
    {
        g.fillAll(AppTheme::BackgroundApp.withAlpha(0.60f));
    }
    else
    {
        g.fillAll(AppTheme::SurfaceCard);
    }

    g.setColour(AppTheme::BorderSubtle);
    g.drawHorizontalLine(height - 1, 0.0f, static_cast<float>(width));
}

void MatrixResolutionTableComponent::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool /*rowIsSelected*/)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(controlList.size()))
        return;

    const auto& ctrl = controlList[static_cast<size_t>(rowNumber)];

    if (columnId == colParam)
    {
        g.setFont(AppTheme::fontBold(11.0f));
        g.setColour(AppTheme::TextPrimary);
        g.drawText(ctrl.name, 6, 0, width - 8, height, juce::Justification::centredLeft, true);
    }
}

juce::Component* MatrixResolutionTableComponent::refreshComponentForCell(int rowNumber, int columnId, bool /*isRowSelected*/,
                                                                         juce::Component* existingComponentToUpdate)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(controlList.size()))
    {
        delete existingComponentToUpdate;
        return nullptr;
    }

    const auto& ctrl = controlList[static_cast<size_t>(rowNumber)];

    // 1. Column Icon
    if (columnId == colIcon)
    {
        ControlIconComponent* iconComp = nullptr;
        if (existingComponentToUpdate != nullptr)
            iconComp = dynamic_cast<ControlIconComponent*>(existingComponentToUpdate);
        else
            iconComp = new ControlIconComponent(ctrl.type);

        if (iconComp != nullptr)
            iconComp->setControlType(ctrl.type);

        return iconComp;
    }

    // 2. Column Parameter Name: painted directly in paintCell for zero widget overhead
    if (columnId == colParam)
    {
        delete existingComponentToUpdate;
        return nullptr;
    }

    // 3. Column Step Resolution (ComboBox)
    if (columnId == colResolution)
    {
        juce::ComboBox* combo = nullptr;
        if (existingComponentToUpdate != nullptr)
            combo = dynamic_cast<juce::ComboBox*>(existingComponentToUpdate);
        else
        {
            combo = new juce::ComboBox();
            combo->setTooltip("Step Resolution - Preset number of points across sweep range");
            combo->addItem("Fixed (1 step - single reference)", 1);
            combo->addItem("3 Steps (0%, 50%, 100%)", 3);
            combo->addItem("5 Steps (Standard: 0, 25, 50, 75, 100%)", 5);
            combo->addItem("8 Steps (Detailed: 8 steps)", 8);
            combo->addItem("16 Steps (High-Res: 16 steps)", 16);
            combo->addItem("32 Steps (Ultra High-Res: 32 steps)", 32);
            combo->addItem("64 Steps (Extreme: 64 steps)", 64);
            combo->addItem("Custom Steps (Manual)...", 99);
        }

        // CRITICAL JUCE 8: Clear callback before mutating state to avoid firing on recycled row
        combo->onChange = nullptr;

        int step = (ctrl.steps > 0) ? ctrl.steps : 1;
        combo->setSelectedId(isStandardStep(step) ? step : 99, juce::dontSendNotification);

        // Rebind callback capturing the EXACT rowNumber
        combo->onChange = [this, rowNumber, combo] {
            if (rowNumber >= 0 && rowNumber < static_cast<int>(controlList.size()))
            {
                auto& c = controlList[static_cast<size_t>(rowNumber)];
                int sId = combo->getSelectedId();
                if (sId != 99)
                {
                    c.steps = sId;
                    if (c.steps == 1) c.maxPct = c.minPct;
                }
                matrixTable.updateContent();
                if (onControlsChanged) onControlsChanged();
            }
        };

        return combo;
    }

    // 4. Column Custom Steps (TextEditor)
    if (columnId == colSteps)
    {
        juce::TextEditor* editor = nullptr;
        if (existingComponentToUpdate != nullptr)
            editor = dynamic_cast<juce::TextEditor*>(existingComponentToUpdate);
        else
        {
            editor = new juce::TextEditor();
            editor->setInputRestrictions(3, "0123456789");
            editor->setTooltip("Custom Steps - Exact evaluation points count");
            editor->setJustification(juce::Justification::centred);
        }

        // CRITICAL JUCE 8: Clear callback before mutating text
        editor->onTextChange = nullptr;
        editor->setText(juce::String(ctrl.steps), juce::dontSendNotification);

        // Rebind callback capturing EXACT rowNumber
        editor->onTextChange = [this, rowNumber, editor] {
            if (rowNumber >= 0 && rowNumber < static_cast<int>(controlList.size()))
            {
                int val = std::max(1, editor->getText().getIntValue());
                controlList[static_cast<size_t>(rowNumber)].steps = val;
                if (onControlsChanged) onControlsChanged();
            }
        };

        return editor;
    }

    // 5. Column Min % (TextEditor)
    if (columnId == colMin)
    {
        juce::TextEditor* editor = nullptr;
        if (existingComponentToUpdate != nullptr)
            editor = dynamic_cast<juce::TextEditor*>(existingComponentToUpdate);
        else
        {
            editor = new juce::TextEditor();
            editor->setInputRestrictions(5, "0123456789.");
            editor->setTooltip("Minimum Value (%) - Sweep start point");
            editor->setJustification(juce::Justification::centred);
        }

        // CRITICAL JUCE 8: Clear callback before mutating text
        editor->onTextChange = nullptr;
        editor->setText(juce::String(ctrl.minPct, 1), juce::dontSendNotification);

        // Rebind callback capturing EXACT rowNumber
        editor->onTextChange = [this, rowNumber, editor] {
            if (rowNumber >= 0 && rowNumber < static_cast<int>(controlList.size()))
            {
                float val = std::clamp(editor->getText().getFloatValue(), 0.0f, 100.0f);
                auto& c = controlList[static_cast<size_t>(rowNumber)];
                c.minPct = val;
                if (c.steps == 1)
                {
                    c.maxPct = val;
                    matrixTable.updateContent();
                }
                if (onControlsChanged) onControlsChanged();
            }
        };

        return editor;
    }

    // 6. Column Max % (TextEditor)
    if (columnId == colMax)
    {
        juce::TextEditor* editor = nullptr;
        if (existingComponentToUpdate != nullptr)
            editor = dynamic_cast<juce::TextEditor*>(existingComponentToUpdate);
        else
        {
            editor = new juce::TextEditor();
            editor->setInputRestrictions(5, "0123456789.");
            editor->setTooltip("Maximum Value (%) - Sweep end point");
            editor->setJustification(juce::Justification::centred);
        }

        // CRITICAL JUCE 8: Clear callback before mutating text
        editor->onTextChange = nullptr;
        editor->setText(juce::String(ctrl.maxPct, 1), juce::dontSendNotification);

        bool enabled = (ctrl.steps > 1);
        editor->setEnabled(enabled);
        editor->setColour(juce::TextEditor::backgroundColourId, enabled ? AppTheme::SurfaceCard : AppTheme::SurfaceHover);
        editor->setColour(juce::TextEditor::textColourId, enabled ? AppTheme::TextPrimary : AppTheme::TextSecondary);

        // Rebind callback capturing EXACT rowNumber
        editor->onTextChange = [this, rowNumber, editor] {
            if (rowNumber >= 0 && rowNumber < static_cast<int>(controlList.size()))
            {
                auto& c = controlList[static_cast<size_t>(rowNumber)];
                float val = std::clamp(editor->getText().getFloatValue(), c.minPct, 100.0f);
                c.maxPct = val;
                if (onControlsChanged) onControlsChanged();
            }
        };

        return editor;
    }

    // 7. Column Order (OrderButtonsComponent)
    if (columnId == colOrder)
    {
        OrderButtonsComponent* order = nullptr;
        if (existingComponentToUpdate != nullptr)
            order = dynamic_cast<OrderButtonsComponent*>(existingComponentToUpdate);
        else
            order = new OrderButtonsComponent();

        // CRITICAL JUCE 8: Clear callbacks before mutating state
        order->btnUp.onClick = nullptr;
        order->btnDown.onClick = nullptr;

        order->btnUp.setEnabled(rowNumber > 0);
        order->btnDown.setEnabled(rowNumber + 1 < static_cast<int>(controlList.size()));

        // Rebind callbacks capturing EXACT rowNumber
        order->btnUp.onClick = [this, rowNumber] {
            if (rowNumber > 0 && rowNumber < static_cast<int>(controlList.size()))
            {
                std::swap(controlList[static_cast<size_t>(rowNumber)],
                          controlList[static_cast<size_t>(rowNumber - 1)]);
                matrixTable.updateContent();
                if (onControlsChanged) onControlsChanged();
            }
        };

        order->btnDown.onClick = [this, rowNumber] {
            if (rowNumber >= 0 && rowNumber + 1 < static_cast<int>(controlList.size()))
            {
                std::swap(controlList[static_cast<size_t>(rowNumber)],
                          controlList[static_cast<size_t>(rowNumber + 1)]);
                matrixTable.updateContent();
                if (onControlsChanged) onControlsChanged();
            }
        };

        return order;
    }

    delete existingComponentToUpdate;
    return nullptr;
}

} // namespace abdaudiolab::gui
