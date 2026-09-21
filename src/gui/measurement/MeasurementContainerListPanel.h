/**
 * @file MeasurementContainerListPanel.h
 * @brief UI panel listing loaded FAIR containers with domain filters, integrity badges,
 *        accessible keyboard focus navigation (WCAG 2.4.7), and safe virtualized row recycling.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "MeasurementComparisonSession.h"

namespace abdaudiolab::gui::measurement
{

class MeasurementContainerListPanel : public juce::Component,
                                      public MeasurementComparisonSession::Listener,
                                      public juce::ListBoxModel
{
public:
    explicit MeasurementContainerListPanel(MeasurementComparisonSession& session);
    ~MeasurementContainerListPanel() override;

    void updateTheme();
    void paint(juce::Graphics& g) override;
    void resized() override;

    // ListBoxModel interface
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForRow(int rowNumber, bool isRowSelected, juce::Component* existingComponentToUpdate) override;
    void selectedRowsChanged(int lastRowSelected) override;

    // Keyboard navigation & accessibility (WCAG 2.4.7)
    bool keyPressed(const juce::KeyPress& key) override;

    // MeasurementComparisonSession::Listener interface
    void containerStateChanged(int containerId, ContainerLoadState newState) override;
    void containerListChanged() override;
    void activeAudioSourceChanged(int activeContainerId) override;
    void domainFilterChanged() override;

    // Selection & focus inspection for tests and UI coordination
    [[nodiscard]] int getSelectedRow() const;
    void selectRow(int rowNumber);
    [[nodiscard]] juce::ListBox& getListBox() noexcept { return listBox_; }

    std::function<void(int containerId)> onContainerSelected;

private:
    void promptAddContainer();
    void updateDomainFilterButtons();

    MeasurementComparisonSession& session_;
    std::vector<LoadedContainerEntry> cachedEntries_;

    juce::Label lblTitle_ { {}, "Contenedores FAIR / LNL" };
    juce::TextButton btnAdd_ { "+ Cargar Contenedor..." };
    juce::TextButton btnClear_ { "Limpiar" };

    // Domain filters
    juce::TextButton btnFilterAll_ { "Todos" };
    juce::TextButton btnFilterOffline_ { "Offline Digital" };
    juce::TextButton btnFilterRealtime_ { "Tiempo Real" };
    juce::TextButton btnFilterHardware_ { "Loopback" };

    juce::ListBox listBox_;
    std::unique_ptr<juce::FileChooser> fileChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeasurementContainerListPanel)
};

} // namespace abdaudiolab::gui::measurement
