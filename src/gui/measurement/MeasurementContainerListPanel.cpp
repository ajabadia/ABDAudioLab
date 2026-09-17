/**
 * @file MeasurementContainerListPanel.cpp
 * @brief Implementation of MeasurementContainerListPanel with accessible keyboard navigation (WCAG 2.4.7),
 *        focused item viewport tracking, and robust virtualized row recycling.
 * @author ABDSynths
 * @date 2026
 */

#include "MeasurementContainerListPanel.h"

namespace abdaudiolab::gui::measurement
{

namespace
{
class ContainerRowComponent : public juce::Component
{
public:
    ContainerRowComponent(MeasurementComparisonSession& session, std::function<void(int)> onSelect)
        : session_(session), onSelectCallback_(std::move(onSelect))
    {
        addAndMakeVisible(chkCompare_);
        addAndMakeVisible(btnAudioSource_);
        addAndMakeVisible(btnDelete_);
    }

    /**
     * @brief Completely overwrites the visual state and listeners for this row component,
     *        ensuring clean virtualization without retaining state from previously represented rows.
     */
    void update(const LoadedContainerEntry& entry, bool isActiveAudio, bool isRowSelected)
    {
        containerId_ = entry.id;
        entry_ = entry;
        isRowSelected_ = isRowSelected;

        const bool isVerified = (entry.loadState == ContainerLoadState::Verified);
        const bool isCorrupt = (entry.loadState == ContainerLoadState::Corrupt);

        // 1. Rebind callbacks with exact captured containerId, completely replacing any prior listeners
        const int capturedId = entry.id;
        chkCompare_.onClick = [this, capturedId]()
        {
            session_.setContainerSelectedForComparison(capturedId, chkCompare_.getToggleState());
        };

        btnAudioSource_.onClick = [this, capturedId]()
        {
            session_.setActiveAudioContainerId(capturedId);
        };

        btnDelete_.onClick = [this, capturedId]()
        {
            session_.removeContainer(capturedId);
        };

        // 2. Refresh UI controls
        chkCompare_.setEnabled(isVerified);
        chkCompare_.setToggleState(entry.selectedForComparison && isVerified, juce::dontSendNotification);

        btnAudioSource_.setEnabled(entry.isPlayable());
        btnAudioSource_.setButtonText(isActiveAudio ? "[ACTIVO]" : "[Audio]");

        if (isCorrupt)
        {
            chkCompare_.setTooltip("Contenedor corrupto: excluido de comparaciones (" + entry.diagnosticReason + ")");
            btnAudioSource_.setTooltip("Audio bloqueado: fallo de integridad criptográfica");
        }
        else
        {
            chkCompare_.setTooltip("Superponer en curvas comparativas");
            btnAudioSource_.setTooltip("Seleccionar como fuente de audio");
        }

        btnDelete_.setTooltip("Eliminar contenedor de la sesión");

        repaint();
    }

    void resetToEmpty()
    {
        containerId_ = -1;
        entry_ = LoadedContainerEntry();
        isRowSelected_ = false;

        chkCompare_.onClick = nullptr;
        btnAudioSource_.onClick = nullptr;
        btnDelete_.onClick = nullptr;

        chkCompare_.setEnabled(false);
        chkCompare_.setToggleState(false, juce::dontSendNotification);
        btnAudioSource_.setEnabled(false);
        btnAudioSource_.setButtonText("[Audio]");

        repaint();
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        if (onSelectCallback_ && containerId_ != -1)
            onSelectCallback_(containerId_);
    }

    void paint(juce::Graphics& g) override
    {
        if (containerId_ == -1)
            return;

        auto bounds = getLocalBounds().toFloat();

        // Background with accessible focus highlight
        if (isRowSelected_)
        {
            g.setColour(juce::Colour(0xff242430));
            g.fillRoundedRectangle(bounds.reduced(2.0f, 1.0f), 4.0f);
            // High-contrast WCAG focus indicator halo
            g.setColour(juce::Colour(0xff00e5ff));
            g.drawRoundedRectangle(bounds.reduced(2.0f, 1.0f), 4.0f, 2.0f);
        }
        else
        {
            g.setColour(juce::Colour(0xff1e1e24));
            g.fillRoundedRectangle(bounds.reduced(2.0f, 1.0f), 4.0f);
        }

        // Visual trace swatch (accessible pattern & marker)
        const float swatchX = 35.0f;
        const float swatchY = bounds.getCentreY();
        const float swatchW = 28.0f;

        g.setColour(entry_.traceColour);
        if (entry_.dashPatternIndex == 1) // dashed
        {
            juce::Line<float> line(swatchX, swatchY, swatchX + swatchW, swatchY);
            float dashes[] = { 4.0f, 2.0f };
            g.drawDashedLine(line, dashes, 2, 2.5f);
        }
        else if (entry_.dashPatternIndex == 2) // dot-dash
        {
            juce::Line<float> line(swatchX, swatchY, swatchX + swatchW, swatchY);
            float dashes[] = { 6.0f, 2.0f, 2.0f, 2.0f };
            g.drawDashedLine(line, dashes, 4, 2.5f);
        }
        else if (entry_.dashPatternIndex == 3) // dotted
        {
            juce::Line<float> line(swatchX, swatchY, swatchX + swatchW, swatchY);
            float dashes[] = { 1.5f, 2.0f };
            g.drawDashedLine(line, dashes, 2, 2.5f);
        }
        else // solid
        {
            g.drawLine(swatchX, swatchY, swatchX + swatchW, swatchY, 2.5f);
        }

        // Marker symbol in center of swatch
        const float markerX = swatchX + swatchW * 0.5f;
        if (entry_.markerShapeIndex == 1) // Square
            g.fillRect(markerX - 3.0f, swatchY - 3.0f, 6.0f, 6.0f);
        else if (entry_.markerShapeIndex == 2) // Triangle
        {
            juce::Path tri;
            tri.addTriangle(markerX, swatchY - 4.0f, markerX - 3.5f, swatchY + 3.0f, markerX + 3.5f, swatchY + 3.0f);
            g.fillPath(tri);
        }
        else if (entry_.markerShapeIndex == 3) // Diamond
        {
            juce::Path dia;
            dia.startNewSubPath(markerX, swatchY - 4.0f);
            dia.lineTo(markerX + 3.5f, swatchY);
            dia.lineTo(markerX, swatchY + 4.0f);
            dia.lineTo(markerX - 3.5f, swatchY);
            dia.closeSubPath();
            g.fillPath(dia);
        }
        else // Circle
            g.fillEllipse(markerX - 3.5f, swatchY - 3.5f, 7.0f, 7.0f);

        // Name & Preset label
        juce::String labelText = entry_.containerDir.getFileName();
        if (entry_.viewModel != nullptr && entry_.viewModel->dutName.isNotEmpty())
            labelText = entry_.viewModel->dutName + " (" + entry_.viewModel->executionDomainText + ")";

        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(13.0f));
        g.drawText(labelText, 70, 4, getWidth() - 250, 18, juce::Justification::centredLeft, true);

        // Subtitle / diagnostic
        g.setFont(juce::FontOptions(11.0f));
        if (entry_.loadState == ContainerLoadState::Corrupt)
        {
            g.setColour(juce::Colour(0xffff5252));
            g.drawText("CORRUPT: " + entry_.diagnosticReason, 70, 22, getWidth() - 250, 14, juce::Justification::centredLeft, true);
        }
        else if (entry_.loadState == ContainerLoadState::Rejected)
        {
            g.setColour(juce::Colour(0xffffab00));
            g.drawText("REJECTED: " + entry_.diagnosticReason, 70, 22, getWidth() - 250, 14, juce::Justification::centredLeft, true);
        }
        else if (entry_.viewModel != nullptr)
        {
            g.setColour(juce::Colour(0xffa0a0b0));
            g.drawText("Métricas: " + juce::String(static_cast<int>(entry_.viewModel->metrics.size())) +
                       " | Puntos: " + juce::String(static_cast<int>(entry_.viewModel->curve.x.size())),
                       70, 22, getWidth() - 250, 14, juce::Justification::centredLeft, true);
        }

        // Status badge
        const float badgeW = 75.0f;
        const float badgeH = 20.0f;
        const float badgeX = getWidth() - 150.0f;
        const float badgeY = (getHeight() - badgeH) * 0.5f;

        juce::Colour badgeCol = juce::Colour(0xff505060);
        if (entry_.loadState == ContainerLoadState::Verified)
            badgeCol = juce::Colour(0xff00c853); // Green
        else if (entry_.loadState == ContainerLoadState::Corrupt)
            badgeCol = juce::Colour(0xffd50000); // Red
        else if (entry_.loadState == ContainerLoadState::Rejected)
            badgeCol = juce::Colour(0xffff6d00); // Amber
        else if (entry_.loadState == ContainerLoadState::Loading)
            badgeCol = juce::Colour(0xff0091ea); // Blue

        g.setColour(badgeCol);
        g.fillRoundedRectangle(badgeX, badgeY, badgeW, badgeH, 3.0f);

        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(10.0f));
        g.drawText(containerLoadStateToString(entry_.loadState),
                   static_cast<int>(badgeX), static_cast<int>(badgeY),
                   static_cast<int>(badgeW), static_cast<int>(badgeH),
                   juce::Justification::centred);
    }

    void resized() override
    {
        chkCompare_.setBounds(8, (getHeight() - 20) / 2, 22, 20);
        btnAudioSource_.setBounds(getWidth() - 70, (getHeight() - 22) / 2, 42, 22);
        btnDelete_.setBounds(getWidth() - 24, (getHeight() - 22) / 2, 20, 22);
    }

private:
    MeasurementComparisonSession& session_;
    std::function<void(int)> onSelectCallback_;
    int containerId_ { -1 };
    LoadedContainerEntry entry_;
    bool isRowSelected_ { false };

    juce::ToggleButton chkCompare_;
    juce::TextButton btnAudioSource_;
    juce::TextButton btnDelete_ { "X" };
};
} // namespace

MeasurementContainerListPanel::MeasurementContainerListPanel(MeasurementComparisonSession& session)
    : session_(session)
{
    session_.addListener(this);

    setWantsKeyboardFocus(true);

    addAndMakeVisible(lblTitle_);
    lblTitle_.setFont(juce::FontOptions(15.0f));
    lblTitle_.setColour(juce::Label::textColourId, juce::Colour(0xff00d4ff));

    addAndMakeVisible(btnAdd_);
    btnAdd_.onClick = [this]() { promptAddContainer(); };

    addAndMakeVisible(btnClear_);
    btnClear_.onClick = [this]() { session_.clear(); };

    // Domain filters
    addAndMakeVisible(btnFilterAll_);
    addAndMakeVisible(btnFilterOffline_);
    addAndMakeVisible(btnFilterRealtime_);
    addAndMakeVisible(btnFilterHardware_);

    btnFilterAll_.onClick = [this]() { session_.setDomainFilter(std::nullopt); };
    btnFilterOffline_.onClick = [this]() { session_.setDomainFilter(abdaudiolab::measurement::MeasurementExecutionDomain::Vst3OfflineDigital); };
    btnFilterRealtime_.onClick = [this]() { session_.setDomainFilter(abdaudiolab::measurement::MeasurementExecutionDomain::Vst3Realtime); };
    btnFilterHardware_.onClick = [this]() { session_.setDomainFilter(abdaudiolab::measurement::MeasurementExecutionDomain::DigitalHardwareRoundtrip); };

    listBox_.setModel(this);
    listBox_.setRowHeight(42);
    listBox_.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff141418));
    listBox_.setWantsKeyboardFocus(true);
    addAndMakeVisible(listBox_);

    updateDomainFilterButtons();
    containerListChanged();
}

MeasurementContainerListPanel::~MeasurementContainerListPanel()
{
    session_.removeListener(this);
}

void MeasurementContainerListPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff18181f));
    g.setColour(juce::Colour(0xff2a2a35));
    g.drawRect(getLocalBounds(), 1);
}

void MeasurementContainerListPanel::resized()
{
    auto area = getLocalBounds().reduced(8);
    auto topArea = area.removeFromTop(28);

    lblTitle_.setBounds(topArea.removeFromLeft(180));
    btnClear_.setBounds(topArea.removeFromRight(65));
    topArea.removeFromRight(6);
    btnAdd_.setBounds(topArea.removeFromRight(150));

    area.removeFromTop(6);
    auto filterArea = area.removeFromTop(24);
    const int filterBtnW = 90;
    btnFilterAll_.setBounds(filterArea.removeFromLeft(filterBtnW));
    filterArea.removeFromLeft(4);
    btnFilterOffline_.setBounds(filterArea.removeFromLeft(filterBtnW));
    filterArea.removeFromLeft(4);
    btnFilterRealtime_.setBounds(filterArea.removeFromLeft(filterBtnW));
    filterArea.removeFromLeft(4);
    btnFilterHardware_.setBounds(filterArea.removeFromLeft(filterBtnW));

    area.removeFromTop(6);
    listBox_.setBounds(area);
}

int MeasurementContainerListPanel::getNumRows()
{
    return static_cast<int>(cachedEntries_.size());
}

void MeasurementContainerListPanel::paintListBoxItem(int, juce::Graphics&, int, int, bool)
{
}

juce::Component* MeasurementContainerListPanel::refreshComponentForRow(int rowNumber,
                                                                      bool isRowSelected,
                                                                      juce::Component* existingComponentToUpdate)
{
    auto* rowComp = dynamic_cast<ContainerRowComponent*>(existingComponentToUpdate);
    if (rowComp == nullptr)
    {
        rowComp = new ContainerRowComponent(session_, [this](int containerId)
        {
            for (size_t i = 0; i < cachedEntries_.size(); ++i)
            {
                if (cachedEntries_[i].id == containerId)
                {
                    listBox_.selectRow(static_cast<int>(i));
                    break;
                }
            }
            if (onContainerSelected)
                onContainerSelected(containerId);
        });
    }

    if (rowNumber >= 0 && rowNumber < static_cast<int>(cachedEntries_.size()))
    {
        const auto& entry = cachedEntries_[static_cast<size_t>(rowNumber)];
        const bool isActiveAudio = (entry.id == session_.getActiveAudioContainerId());
        rowComp->update(entry, isActiveAudio, isRowSelected);
    }
    else
    {
        rowComp->resetToEmpty();
    }
    return rowComp;
}

void MeasurementContainerListPanel::selectedRowsChanged(int lastRowSelected)
{
    if (lastRowSelected >= 0 && lastRowSelected < static_cast<int>(cachedEntries_.size()))
    {
        if (onContainerSelected)
            onContainerSelected(cachedEntries_[static_cast<size_t>(lastRowSelected)].id);
    }
    listBox_.repaint();
}

bool MeasurementContainerListPanel::keyPressed(const juce::KeyPress& key)
{
    const int totalRows = getNumRows();
    const int currentSel = listBox_.getSelectedRow();

    // 1. Up arrow: Navigate up
    if (key.isKeyCode(juce::KeyPress::upKey))
    {
        if (currentSel > 0)
        {
            const int target = currentSel - 1;
            listBox_.selectRow(target);
            listBox_.scrollToEnsureRowIsOnscreen(target);
            selectedRowsChanged(target);
            return true;
        }
        else if (currentSel == -1 && totalRows > 0)
        {
            listBox_.selectRow(0);
            listBox_.scrollToEnsureRowIsOnscreen(0);
            selectedRowsChanged(0);
            return true;
        }
        return true;
    }

    // 2. Down arrow: Navigate down
    if (key.isKeyCode(juce::KeyPress::downKey))
    {
        if (currentSel < totalRows - 1)
        {
            const int target = (currentSel == -1) ? 0 : currentSel + 1;
            listBox_.selectRow(target);
            listBox_.scrollToEnsureRowIsOnscreen(target);
            selectedRowsChanged(target);
            return true;
        }
        return true;
    }

    // 3. Space: Toggle comparison for selected container (only if Verified)
    if (key.isKeyCode(juce::KeyPress::spaceKey))
    {
        if (currentSel >= 0 && currentSel < static_cast<int>(cachedEntries_.size()))
        {
            const auto& entry = cachedEntries_[static_cast<size_t>(currentSel)];
            if (entry.loadState == ContainerLoadState::Verified)
            {
                session_.setContainerSelectedForComparison(entry.id, !entry.selectedForComparison);
                return true;
            }
        }
        return true;
    }

    // 4. Return / Enter: Activate verified audio for selected container
    if (key.isKeyCode(juce::KeyPress::returnKey))
    {
        if (currentSel >= 0 && currentSel < static_cast<int>(cachedEntries_.size()))
        {
            const auto& entry = cachedEntries_[static_cast<size_t>(currentSel)];
            if (entry.isPlayable())
            {
                session_.setActiveAudioContainerId(entry.id);
                return true;
            }
        }
        return true;
    }

    // 5. Delete: Delete SELECTED container (strictly NOT active audio if different, no-op if no selection)
    if (key.isKeyCode(juce::KeyPress::deleteKey) || key.isKeyCode(juce::KeyPress::backspaceKey))
    {
        if (currentSel >= 0 && currentSel < static_cast<int>(cachedEntries_.size()))
        {
            const int targetId = cachedEntries_[static_cast<size_t>(currentSel)].id;
            session_.removeContainer(targetId);

            // Maintain safe focus on remaining items
            const int remainingRows = getNumRows();
            if (remainingRows > 0)
            {
                const int nextSel = std::min(currentSel, remainingRows - 1);
                listBox_.selectRow(nextSel);
                listBox_.scrollToEnsureRowIsOnscreen(nextSel);
                selectedRowsChanged(nextSel);
            }
            return true;
        }
        // If nothing is selected, Delete does nothing!
        return true;
    }

    return Component::keyPressed(key);
}

int MeasurementContainerListPanel::getSelectedRow() const
{
    return listBox_.getSelectedRow();
}

void MeasurementContainerListPanel::selectRow(int rowNumber)
{
    listBox_.selectRow(rowNumber);
    listBox_.scrollToEnsureRowIsOnscreen(rowNumber);
    selectedRowsChanged(rowNumber);
}

void MeasurementContainerListPanel::containerStateChanged(int, ContainerLoadState)
{
    containerListChanged();
}

void MeasurementContainerListPanel::containerListChanged()
{
    const int prevSel = listBox_.getSelectedRow();
    cachedEntries_ = session_.getFilteredContainers();
    listBox_.updateContent();

    if (prevSel >= 0 && prevSel < static_cast<int>(cachedEntries_.size()))
        listBox_.selectRow(prevSel);
    else if (!cachedEntries_.empty() && prevSel >= static_cast<int>(cachedEntries_.size()))
        listBox_.selectRow(static_cast<int>(cachedEntries_.size()) - 1);

    listBox_.repaint();
}

void MeasurementContainerListPanel::activeAudioSourceChanged(int)
{
    listBox_.repaint();
}

void MeasurementContainerListPanel::domainFilterChanged()
{
    updateDomainFilterButtons();
    containerListChanged();
}

void MeasurementContainerListPanel::updateDomainFilterButtons()
{
    const auto filter = session_.getDomainFilter();
    btnFilterAll_.setToggleState(!filter.has_value(), juce::dontSendNotification);
    btnFilterOffline_.setToggleState(filter == abdaudiolab::measurement::MeasurementExecutionDomain::Vst3OfflineDigital, juce::dontSendNotification);
    btnFilterRealtime_.setToggleState(filter == abdaudiolab::measurement::MeasurementExecutionDomain::Vst3Realtime, juce::dontSendNotification);
    btnFilterHardware_.setToggleState(filter == abdaudiolab::measurement::MeasurementExecutionDomain::DigitalHardwareRoundtrip, juce::dontSendNotification);
}

void MeasurementContainerListPanel::promptAddContainer()
{
    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Seleccionar Carpeta de Contenedor FAIR/LNL...",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*",
        true);

    fileChooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [this](const juce::FileChooser& chooser)
    {
        auto result = chooser.getResult();
        if (result.exists() && result.isDirectory())
        {
            session_.addContainerAsync(result);
        }
    });
}

} // namespace abdaudiolab::gui::measurement
