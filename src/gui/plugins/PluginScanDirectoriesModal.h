/**
 * @file PluginScanDirectoriesModal.h
 * @brief Modal dialog for managing VST3/AU/LV2 plugin scan directories with persistence.
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <vector>
#include "../SoundIdTheme.h"

namespace abdaudiolab::gui
{

/**
 * @class PluginScanDirectoriesModal
 * @brief Persistent modal dialog to add/remove plugin scan directories and trigger scans.
 *
 * Features:
 * - List of configured plugin directories with plugin count per directory
 * - "Add Folder" button with native folder chooser
 * - "Remove" button per directory
 * - "Scan All" button to re-scan all configured directories
 * - Progress indicator during scanning
 * - Persistent storage via XML in user app data
 */
class PluginScanDirectoriesModal : public juce::Component
{
public:
    PluginScanDirectoriesModal();
    ~PluginScanDirectoriesModal() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** @brief Shows modal centered on parent, loading persisted directories. */
    void showModal(juce::Component* parent);

    /** @brief Refreshes colors based on active AppTheme. */
    void updateTheme();

    /** @brief Loads persisted scan directories from disk. */
    std::vector<juce::File> loadPersistedDirectories();

    /** @brief Saves current directories list to disk. */
    void savePersistedDirectories();

    /** @brief Returns the standard persistence file location. */
    static juce::File getSettingsFile();

    /** @brief Adds a directory to the list and persists. */
    void addDirectory(const juce::File& dir);

    /** @brief Removes directory at given index and persists. */
    void removeDirectory(int index);

    /** @brief Returns all currently configured directories. */
    const std::vector<juce::File>& getDirectories() const noexcept { return scanDirectories; }

    /** @brief Called when scan is complete with the number of plugins found. */
    std::function<void(int pluginsFound)> onScanComplete;

    /** @brief Called to request the owner to perform an actual scan. */
    std::function<void(const juce::FileSearchPath& paths,
                       std::function<void(const juce::String&, float)> progressCallback)> onScanRequested;

private:
    std::vector<juce::File> scanDirectories;

    juce::ListBox directoryList;
    juce::TextButton btnAddFolder;
    juce::TextButton btnScanAll;
    juce::TextButton btnClose;
    juce::Label lblTitle;
    juce::Label lblStatus;
    juce::ProgressBar* progressBar { nullptr };
    double progressValue { 0.0 };

    std::shared_ptr<juce::FileChooser> fileChooser;

    void rebuildListContent();
    void startScan();

    // ListBoxModel
    class DirectoryListModel : public juce::ListBoxModel
    {
    public:
        explicit DirectoryListModel(PluginScanDirectoriesModal& owner) : modal(owner) {}

        int getNumRows() override;
        void paintListBoxItem(int rowNumber, juce::Graphics& g,
                              int width, int height, bool rowIsSelected) override;
        void listBoxItemClicked(int row, const juce::MouseEvent&) override;
        juce::Component* refreshComponentForRow(int rowNumber, bool isRowSelected,
                                                 juce::Component* existingComponentToUpdate) override;

    private:
        PluginScanDirectoriesModal& modal;
    };

    // Row component with remove button
    class DirectoryRowComponent : public juce::Component
    {
    public:
        DirectoryRowComponent(PluginScanDirectoriesModal& owner);

        void paint(juce::Graphics& g) override;
        void resized() override;
        void setRowData(int rowIndex, const juce::File& dir, bool selected);

    private:
        PluginScanDirectoriesModal& modal;
        juce::TextButton btnRemove { "X" };
        juce::File directory;
        int row { -1 };
        bool isSelected { false };
    };

    DirectoryListModel listModel { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginScanDirectoriesModal)
};

} // namespace abdaudiolab::gui
