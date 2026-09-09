/**
 * @file DrawerFileSessionTab.h
 * @brief Drawer tab for Session operations and exported code/data inspection.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "../SoundIdTheme.h"

namespace abdaudiolab::gui
{

class DrawerFileSessionTab : public juce::Component
{
public:
    DrawerFileSessionTab();
    ~DrawerFileSessionTab() override = default;

    void setExportDirectory(const juce::String& path);
    void refreshFilePreviewList();
    void updateTheme();
    [[nodiscard]] int getPreferredHeight() const noexcept;

    std::function<void()> onNewSessionClicked;
    std::function<void()> onOpenSessionClicked;
    std::function<void()> onSaveSessionClicked;
    std::function<void()> onSaveSessionAsClicked;
    std::function<void()> onReanalyzeSessionClicked;
    std::function<void()> onChangeExportFolderClicked;
    std::function<void()> onRevealExportFolderClicked;
    std::function<void()> onExportReportClicked;
    std::function<void()> onCheckUpdatesClicked;
    std::function<void()> onExitAppClicked;

    // Accessors for Laboratory Conditions and Notes (1.7.12)
    [[nodiscard]] juce::String getOperatorNotes() const { return txtOperatorNotes.getText(); }
    void setOperatorNotes(const juce::String& notes) { txtOperatorNotes.setText(notes, false); }

    [[nodiscard]] float getAmbientTemperature() const { return txtAmbientTemp.getText().getFloatValue(); }
    void setAmbientTemperature(float degC) { txtAmbientTemp.setText(juce::String(degC, 1), false); }

    [[nodiscard]] int getWarmupTimeMinutes() const { return txtWarmupTime.getText().getIntValue(); }
    void setWarmupTimeMinutes(int minutes) { txtWarmupTime.setText(juce::String(minutes), false); }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // Session Actions
    juce::Label lblFileSection;
    juce::TextButton btnFileNew { "New Session (Reset Plan)" };
    juce::TextButton btnFileOpen { "Open Session Manifest (.json)..." };
    juce::TextButton btnFileSave { "Save Session" };
    juce::TextButton btnFileSaveAs { "Save Session As..." };
    juce::TextButton btnFileReanalyze { "Re-Analyze Session (Offline)" };

    // Laboratory Conditions & Notes (1.7.12)
    juce::Label lblLabSection;
    juce::Label lblNotes;
    juce::TextEditor txtOperatorNotes;
    juce::Label lblAmbientTemp;
    juce::TextEditor txtAmbientTemp;
    juce::Label lblWarmupTime;
    juce::TextEditor txtWarmupTime;

    // Export Directory
    juce::Label lblFileExportSection;
    juce::Label lblFileExportPathVal;
    juce::TextButton btnFileChangeExport { "Change Target Folder..." };
    juce::TextButton btnFileRevealExport { "Show Target Folder in Explorer" };
    juce::TextButton btnFileExportReport { "Generate / Export Certification Report (HTML/PDF)" };

    // Code & Data Previewer
    juce::Label lblFilePreviewSection;
    juce::ComboBox comboPreviewFiles;
    juce::TextEditor txtCodePreview;
    juce::TextButton btnCopyPreview { "Copy Code" };
    juce::TextButton btnOpenFileInEditor { "Open in Editor" };

    // Updates & Exit
    juce::TextButton btnCheckUpdates { "Check for Updates..." };
    juce::TextButton btnFileExit { "Exit ABDAudioLab" };

    juce::String currentExportFolderPath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrawerFileSessionTab)
};

} // namespace abdaudiolab::gui
