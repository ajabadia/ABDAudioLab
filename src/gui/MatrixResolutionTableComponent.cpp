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
    colAdvancedToggle = 4
};

bool isStandardStep(int step) noexcept
{
    return step == 1 || step == 3 || step == 5 || step == 8 || step == 16 || step == 32 || step == 64;
}
} // namespace

// ============================================================================
// AdvancedSettingsPanel Implementation
// ============================================================================
MatrixResolutionTableComponent::AdvancedSettingsPanel::AdvancedSettingsPanel()
{
    lblTitle.setFont(AppTheme::fontBold(11.0f));
    lblTitle.setColour(juce::Label::textColourId, SoundIdTheme::accentBlue);
    addAndMakeVisible(lblTitle);

    lblRange.setText("Range:", juce::dontSendNotification);
    lblRange.setFont(AppTheme::fontRegular(10.5f));
    lblRange.setColour(juce::Label::textColourId, AppTheme::TextSecondary);
    addAndMakeVisible(lblRange);

    txtMin.setInputRestrictions(5, "0123456789.");
    txtMin.setTooltip("Minimum Value (%) - Sweep start point");
    txtMin.setJustification(juce::Justification::centred);
    txtMin.setColour(juce::TextEditor::backgroundColourId, AppTheme::SurfaceCard);
    txtMin.setColour(juce::TextEditor::textColourId, AppTheme::TextPrimary);
    addAndMakeVisible(txtMin);

    lblRangeDash.setText("—", juce::dontSendNotification);
    lblRangeDash.setFont(AppTheme::fontRegular(10.5f));
    lblRangeDash.setJustificationType(juce::Justification::centred);
    lblRangeDash.setColour(juce::Label::textColourId, AppTheme::TextSecondary);
    addAndMakeVisible(lblRangeDash);

    txtMax.setInputRestrictions(5, "0123456789.");
    txtMax.setTooltip("Maximum Value (%) - Sweep end point");
    txtMax.setJustification(juce::Justification::centred);
    txtMax.setColour(juce::TextEditor::backgroundColourId, AppTheme::SurfaceCard);
    txtMax.setColour(juce::TextEditor::textColourId, AppTheme::TextPrimary);
    addAndMakeVisible(txtMax);

    lblUnitMin.setText("%", juce::dontSendNotification);
    lblUnitMin.setFont(AppTheme::fontRegular(10.0f));
    lblUnitMin.setColour(juce::Label::textColourId, AppTheme::TextSecondary);
    addAndMakeVisible(lblUnitMin);

    lblUnitMax.setText("%", juce::dontSendNotification);
    lblUnitMax.setFont(AppTheme::fontRegular(10.0f));
    lblUnitMax.setColour(juce::Label::textColourId, AppTheme::TextSecondary);
    addAndMakeVisible(lblUnitMax);

    lblOrder.setText("Order:", juce::dontSendNotification);
    lblOrder.setFont(AppTheme::fontRegular(10.5f));
    lblOrder.setColour(juce::Label::textColourId, AppTheme::TextSecondary);
    addAndMakeVisible(lblOrder);

    btnMoveEarlier.setTooltip("Move parameter earlier in sweep execution order");
    btnMoveEarlier.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnMoveEarlier.setColour(juce::TextButton::textColourOffId, AppTheme::TextPrimary);
    btnMoveEarlier.onClick = [this] { if (onMoveOrder) onMoveOrder(-1); };
    addAndMakeVisible(btnMoveEarlier);

    btnMoveLater.setTooltip("Move parameter later in sweep execution order");
    btnMoveLater.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnMoveLater.setColour(juce::TextButton::textColourOffId, AppTheme::TextPrimary);
    btnMoveLater.onClick = [this] { if (onMoveOrder) onMoveOrder(1); };
    addAndMakeVisible(btnMoveLater);

    lblCustomSteps.setText("Custom Points:", juce::dontSendNotification);
    lblCustomSteps.setFont(AppTheme::fontRegular(10.5f));
    lblCustomSteps.setColour(juce::Label::textColourId, SoundIdTheme::accentAmber);
    addChildComponent(lblCustomSteps);

    txtCustomSteps.setInputRestrictions(4, "0123456789");
    txtCustomSteps.setTooltip("Exact number of measurement evaluation points for this control");
    txtCustomSteps.setJustification(juce::Justification::centred);
    txtCustomSteps.setColour(juce::TextEditor::backgroundColourId, AppTheme::SurfaceCard);
    txtCustomSteps.setColour(juce::TextEditor::textColourId, AppTheme::TextPrimary);
    addChildComponent(txtCustomSteps);

    btnRemove.setTooltip("Remove this parameter from the measurement matrix");
    btnRemove.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    btnRemove.setColour(juce::TextButton::textColourOffId, AppTheme::TextSecondary);
    btnRemove.onClick = [this] { if (onRemove) onRemove(); };
    addAndMakeVisible(btnRemove);
}

void MatrixResolutionTableComponent::AdvancedSettingsPanel::setTargetControl(int index, ControlStepConfig* config, int totalControls)
{
    targetIndex = index;
    targetConfig = config;

    if (targetConfig == nullptr)
    {
        setVisible(false);
        return;
    }

    lblTitle.setText("Advanced settings \u2022 " + targetConfig->name, juce::dontSendNotification);

    // Range bindings with immediate cross-validation
    txtMin.onTextChange = nullptr;
    txtMax.onTextChange = nullptr;

    txtMin.setText(juce::String(targetConfig->minPct, 1), juce::dontSendNotification);
    txtMax.setText(juce::String(targetConfig->maxPct, 1), juce::dontSendNotification);

    txtMin.onTextChange = [this] {
        if (targetConfig == nullptr) return;
        float val = std::clamp(txtMin.getText().getFloatValue(), 0.0f, 100.0f);
        if (val > targetConfig->maxPct)
        {
            val = targetConfig->maxPct;
            txtMin.setText(juce::String(val, 1), juce::dontSendNotification);
        }
        targetConfig->minPct = val;
        if (onChanged) onChanged();
    };

    txtMax.onTextChange = [this] {
        if (targetConfig == nullptr) return;
        float val = std::clamp(txtMax.getText().getFloatValue(), 0.0f, 100.0f);
        if (val < targetConfig->minPct)
        {
            val = targetConfig->minPct;
            txtMax.setText(juce::String(val, 1), juce::dontSendNotification);
        }
        targetConfig->maxPct = val;
        if (onChanged) onChanged();
    };

    btnMoveEarlier.setEnabled(targetIndex > 0);
    btnMoveLater.setEnabled(targetIndex + 1 < totalControls);

    // Custom steps visibility: ONLY visible when isCustom is explicitly true
    bool showCustom = targetConfig->isCustom;
    lblCustomSteps.setVisible(showCustom);
    txtCustomSteps.setVisible(showCustom);

    if (showCustom)
    {
        txtCustomSteps.onTextChange = nullptr;
        txtCustomSteps.setText(juce::String(targetConfig->steps), juce::dontSendNotification);
        txtCustomSteps.onTextChange = [this] {
            if (targetConfig == nullptr) return;
            int pts = std::clamp(txtCustomSteps.getText().getIntValue(), 1, 512);
            targetConfig->steps = pts;
            if (onChanged) onChanged();
        };
    }

    setVisible(true);
    resized();
    repaint();
}

void MatrixResolutionTableComponent::AdvancedSettingsPanel::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(SoundIdTheme::surfaceSubtle.withAlpha(0.65f));
    g.fillRoundedRectangle(b, 6.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(b, 6.0f, 1.0f);
}

void MatrixResolutionTableComponent::AdvancedSettingsPanel::resized()
{
    auto b = getLocalBounds().reduced(10, 6);
    int rowH = 24;

    // Top Row: Title + Remove Button
    auto topRow = b.removeFromTop(rowH);
    btnRemove.setBounds(topRow.removeFromRight(105).reduced(0, 2));
    lblTitle.setBounds(topRow);

    b.removeFromTop(4);

    // Middle Row: Range and Order
    auto midRow = b.removeFromTop(rowH);

    lblRange.setBounds(midRow.removeFromLeft(46));
    txtMin.setBounds(midRow.removeFromLeft(40));
    lblUnitMin.setBounds(midRow.removeFromLeft(14));
    lblRangeDash.setBounds(midRow.removeFromLeft(16));
    txtMax.setBounds(midRow.removeFromLeft(40));
    lblUnitMax.setBounds(midRow.removeFromLeft(18));

    midRow.removeFromLeft(16); // spacing

    lblOrder.setBounds(midRow.removeFromLeft(42));
    btnMoveEarlier.setBounds(midRow.removeFromLeft(95).reduced(1, 1));
    midRow.removeFromLeft(4);
    btnMoveLater.setBounds(midRow.removeFromLeft(90).reduced(1, 1));

    // Bottom Row: Custom Points (if visible)
    if (lblCustomSteps.isVisible())
    {
        b.removeFromTop(4);
        auto customRow = b.removeFromTop(rowH);
        lblCustomSteps.setBounds(customRow.removeFromLeft(90));
        txtCustomSteps.setBounds(customRow.removeFromLeft(50));
    }
}

// ============================================================================
// MatrixResolutionTableComponent Implementation
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
    // 4 compact columns: Icon, Parameter, Resolution preset, and Advanced chevron toggle
    hdr.addColumn("ICON",            colIcon,            32,  28,  40,  juce::TableHeaderComponent::notSortable);
    hdr.addColumn("PARAMETER",       colParam,          160, 100, 300, juce::TableHeaderComponent::notSortable);
    hdr.addColumn("STEP RESOLUTION", colResolution,     150, 130, 220, juce::TableHeaderComponent::notSortable);
    hdr.addColumn("",                colAdvancedToggle,  34,  30,  44,  juce::TableHeaderComponent::notSortable);

    addAndMakeVisible(matrixTable);

    advancedPanel.setVisible(false);
    advancedPanel.onChanged = [this] {
        matrixTable.updateContent();
        matrixTable.repaint();
        if (onControlsChanged) onControlsChanged();
    };
    advancedPanel.onMoveOrder = [this](int dir) {
        if (activeAdvancedRow >= 0 && activeAdvancedRow < static_cast<int>(controlList.size()))
        {
            int dest = activeAdvancedRow + dir;
            moveRow(activeAdvancedRow, dest);
        }
    };
    advancedPanel.onRemove = [this] {
        if (activeAdvancedRow >= 0 && activeAdvancedRow < static_cast<int>(controlList.size()))
        {
            removeRow(activeAdvancedRow);
        }
    };
    addChildComponent(advancedPanel);

    btnAddParam.setTooltip("Add a plugin parameter to the measurement matrix");
    btnAddParam.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnAddParam.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentBlue);
    btnAddParam.onClick = [this] { showAddParamMenu(); };
    btnAddParam.setVisible(false);
    addAndMakeVisible(btnAddParam);
}

void MatrixResolutionTableComponent::setControls(const std::vector<ControlStepConfig>& controls)
{
    controlList = controls;
    if (activeAdvancedRow >= static_cast<int>(controlList.size()))
        activeAdvancedRow = -1;

    if (activeAdvancedRow >= 0)
    {
        advancedPanel.setTargetControl(activeAdvancedRow, &controlList[static_cast<size_t>(activeAdvancedRow)], static_cast<int>(controlList.size()));
    }
    else
    {
        advancedPanel.setVisible(false);
    }

    matrixTable.updateContent();
    matrixTable.repaint();
    resized();
}

void MatrixResolutionTableComponent::updateTheme()
{
    matrixTable.setColour(juce::ListBox::backgroundColourId, SoundIdTheme::bgCard);
    matrixTable.setColour(juce::ListBox::outlineColourId, SoundIdTheme::borderSubtle);
    matrixTable.updateContent();
    matrixTable.repaint();
}

void MatrixResolutionTableComponent::setAvailableParams(const std::vector<ControlStepConfig>& availableParams)
{
    availableParamsList = availableParams;
    btnAddParam.setVisible(!availableParamsList.empty());
    resized();
}

void MatrixResolutionTableComponent::clearAvailableParams()
{
    availableParamsList.clear();
    btnAddParam.setVisible(false);
    resized();
}

void MatrixResolutionTableComponent::toggleAdvancedRow(int rowIndex)
{
    if (activeAdvancedRow == rowIndex)
    {
        // Toggle close
        activeAdvancedRow = -1;
        advancedPanel.setVisible(false);
    }
    else if (rowIndex >= 0 && rowIndex < static_cast<int>(controlList.size()))
    {
        // Open/switch to this row
        activeAdvancedRow = rowIndex;
        advancedPanel.setTargetControl(activeAdvancedRow, &controlList[static_cast<size_t>(activeAdvancedRow)], static_cast<int>(controlList.size()));
    }
    else
    {
        activeAdvancedRow = -1;
        advancedPanel.setVisible(false);
    }

    matrixTable.updateContent();
    matrixTable.repaint();
    if (auto* parent = getParentComponent())
        parent->resized();
    else
        resized();
}

void MatrixResolutionTableComponent::showAddParamMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader("Plugin Parameters");

    int menuId = 1;
    std::vector<int> unmappedIndices;
    for (int i = 0; i < static_cast<int>(availableParamsList.size()); ++i)
    {
        const auto& ap = availableParamsList[static_cast<size_t>(i)];
        bool alreadyAdded = false;
        for (const auto& c : controlList)
        {
            if (c.id == ap.id || c.name == ap.name)
            {
                alreadyAdded = true;
                break;
            }
        }
        if (!alreadyAdded)
        {
            menu.addItem(menuId, ap.name);
            unmappedIndices.push_back(i);
            ++menuId;
        }
    }

    if (unmappedIndices.empty())
    {
        menu.addItem(1, "(All parameters already added)", false);
        menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(&btnAddParam));
        return;
    }

    menu.showMenuAsync(
        juce::PopupMenu::Options{}.withTargetComponent(&btnAddParam),
        [this, unmappedIndices](int result)
        {
            if (result <= 0) return;
            int idx = unmappedIndices[static_cast<size_t>(result - 1)];
            ControlStepConfig cs = availableParamsList[static_cast<size_t>(idx)];
            cs.steps = 5;
            cs.isCustom = false;
            cs.minPct = 0.0f;
            cs.maxPct = 100.0f;
            cs.sortOrder = static_cast<int>(controlList.size());
            controlList.push_back(cs);
            matrixTable.updateContent();
            matrixTable.repaint();
            if (auto* p = getParentComponent()) p->resized(); else resized();
            if (onControlsChanged) onControlsChanged();
        });
}

void MatrixResolutionTableComponent::removeRow(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= static_cast<int>(controlList.size())) return;
    controlList.erase(controlList.begin() + rowIndex);

    if (activeAdvancedRow == rowIndex)
    {
        activeAdvancedRow = -1;
        advancedPanel.setVisible(false);
    }
    else if (activeAdvancedRow > rowIndex)
    {
        activeAdvancedRow--;
        advancedPanel.setTargetControl(activeAdvancedRow, &controlList[static_cast<size_t>(activeAdvancedRow)], static_cast<int>(controlList.size()));
    }

    matrixTable.updateContent();
    matrixTable.repaint();
    if (auto* p = getParentComponent()) p->resized(); else resized();
    if (onControlsChanged) onControlsChanged();
}

void MatrixResolutionTableComponent::moveRow(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= static_cast<int>(controlList.size())) return;
    if (toIndex < 0 || toIndex >= static_cast<int>(controlList.size())) return;

    std::swap(controlList[static_cast<size_t>(fromIndex)], controlList[static_cast<size_t>(toIndex)]);
    activeAdvancedRow = toIndex;
    advancedPanel.setTargetControl(activeAdvancedRow, &controlList[static_cast<size_t>(activeAdvancedRow)], static_cast<int>(controlList.size()));

    matrixTable.updateContent();
    matrixTable.repaint();
    if (onControlsChanged) onControlsChanged();
}

int MatrixResolutionTableComponent::getPreferredHeight() const
{
    int numRows = static_cast<int>(controlList.size());
    int tableContentH = 26 + std::max(1, numRows) * 34 + 6;
    int tableH = std::clamp(tableContentH, 94, 220);

    int advH = 0;
    if (activeAdvancedRow >= 0 && activeAdvancedRow < static_cast<int>(controlList.size()))
    {
        advH = controlList[static_cast<size_t>(activeAdvancedRow)].isCustom ? 90 : 66;
    }

    return tableH + advH + (btnAddParam.isVisible() ? 30 : 0);
}

void MatrixResolutionTableComponent::resized()
{
    auto b = getLocalBounds();

    if (btnAddParam.isVisible())
    {
        btnAddParam.setBounds(b.removeFromBottom(26).reduced(0, 2));
    }

    if (activeAdvancedRow >= 0 && activeAdvancedRow < static_cast<int>(controlList.size()))
    {
        int advH = controlList[static_cast<size_t>(activeAdvancedRow)].isCustom ? 86 : 64;
        advancedPanel.setBounds(b.removeFromBottom(advH).reduced(0, 2));
    }
    else
    {
        advancedPanel.setVisible(false);
    }

    matrixTable.setBounds(b);
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
    if (rowNumber == activeAdvancedRow)
    {
        g.fillAll(SoundIdTheme::accentBlue.withAlpha(0.12f));
    }
    else if (rowIsSelected)
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
        g.setColour(rowNumber == activeAdvancedRow ? SoundIdTheme::accentBlue : AppTheme::TextPrimary);
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

    // 2. Column Parameter Name: painted directly in paintCell for high rendering performance
    if (columnId == colParam)
    {
        delete existingComponentToUpdate;
        return nullptr;
    }

    // 3. Column Step Resolution (ComboBox) with clean semantic presets
    if (columnId == colResolution)
    {
        juce::ComboBox* combo = nullptr;
        if (existingComponentToUpdate != nullptr)
            combo = dynamic_cast<juce::ComboBox*>(existingComponentToUpdate);
        else
        {
            combo = new juce::ComboBox();
            combo->setTooltip("Step Resolution - Preset number of points across sweep range");
            combo->addItem("Fixed (1 point)", 1);
            combo->addItem("Coarse (3 points)", 3);
            combo->addItem("Standard (5 points)", 5);
            combo->addItem("Detailed (8 points)", 8);
            combo->addItem("Fine (16 points)", 16);
            combo->addItem("Ultra-Fine (32 points)", 32);
            combo->addItem("Extreme (64 points)", 64);
            combo->addItem("Custom...", 99);
        }

        combo->onChange = nullptr;

        int selectedId = ctrl.isCustom ? 99 : (isStandardStep(ctrl.steps) ? ctrl.steps : 99);
        combo->setSelectedId(selectedId, juce::dontSendNotification);

        combo->onChange = [this, rowNumber, combo] {
            if (rowNumber >= 0 && rowNumber < static_cast<int>(controlList.size()))
            {
                auto& c = controlList[static_cast<size_t>(rowNumber)];
                int sId = combo->getSelectedId();
                if (sId == 99)
                {
                    c.isCustom = true;
                    // Keep existing steps or default to 10 if standard 1
                    if (c.steps <= 1) c.steps = 10;
                    // Auto-open advanced panel so user can immediately edit points
                    toggleAdvancedRow(rowNumber);
                }
                else
                {
                    c.isCustom = false;
                    c.steps = sId;
                    if (c.steps == 1) c.maxPct = c.minPct;
                    if (activeAdvancedRow == rowNumber)
                    {
                        advancedPanel.setTargetControl(rowNumber, &c, static_cast<int>(controlList.size()));
                        resized();
                    }
                }
                matrixTable.updateContent();
                matrixTable.repaint();
                if (onControlsChanged) onControlsChanged();
            }
        };

        return combo;
    }

    // 4. Column Advanced Toggle Button (Chevron ▾ / ▴)
    if (columnId == colAdvancedToggle)
    {
        AdvancedToggleComponent* toggleComp = nullptr;
        if (existingComponentToUpdate != nullptr)
            toggleComp = dynamic_cast<AdvancedToggleComponent*>(existingComponentToUpdate);
        else
            toggleComp = new AdvancedToggleComponent();

        toggleComp->setState(activeAdvancedRow == rowNumber);
        toggleComp->onToggle = [this, rowNumber] {
            toggleAdvancedRow(rowNumber);
        };

        return toggleComp;
    }

    delete existingComponentToUpdate;
    return nullptr;
}

} // namespace abdaudiolab::gui
