/**
 * @file DrawerFileSessionTab.cpp
 * @brief Implementation of the File/Session and export preview drawer tab.
 * @author ABDSynths
 * @date 2026
 */

#include "DrawerFileSessionTab.h"
#include <algorithm>

namespace abdaudiolab::gui
{

DrawerFileSessionTab::DrawerFileSessionTab()
{
    // ==========================================
    // 1. Session & Project Actions
    // ==========================================
    lblFileSection.setText("SESSION & PROJECT ACTIONS", juce::dontSendNotification);
    lblFileSection.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblFileSection.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(lblFileSection);

    btnFileNew.setTooltip("New Session - Clear current session and create a new profiling plan");
    btnFileNew.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnFileNew.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnFileNew.onClick = [this] { if (onNewSessionClicked) onNewSessionClicked(); };
    addAndMakeVisible(btnFileNew);

    btnFileOpen.setTooltip("Open Session - Load an existing session (.json) from disk");
    btnFileOpen.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnFileOpen.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnFileOpen.onClick = [this] { if (onOpenSessionClicked) onOpenSessionClicked(); };
    addAndMakeVisible(btnFileOpen);

    btnFileSave.setTooltip("Save Session - Save current profiling measurements and test definitions to session file");
    btnFileSave.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnFileSave.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnFileSave.onClick = [this] { if (onSaveSessionClicked) onSaveSessionClicked(); };
    addAndMakeVisible(btnFileSave);

    btnFileSaveAs.setTooltip("Save Session As - Save profiling session to a new file destination");
    btnFileSaveAs.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnFileSaveAs.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnFileSaveAs.onClick = [this] { if (onSaveSessionAsClicked) onSaveSessionAsClicked(); };
    addAndMakeVisible(btnFileSaveAs);

    btnFileReanalyze.setTooltip("Re-Analyze Session (Offline) - Recompute all statistical metrics from raw recorded audio without hardware connected");
    btnFileReanalyze.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnFileReanalyze.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentGreen);
    btnFileReanalyze.onClick = [this] { if (onReanalyzeSessionClicked) onReanalyzeSessionClicked(); };
    addAndMakeVisible(btnFileReanalyze);

    // ==========================================
    // 2. Laboratory Conditions & Notes (1.7.12)
    // ==========================================
    lblLabSection.setText("LABORATORY OBSERVATIONS & ENVIRONMENT", juce::dontSendNotification);
    lblLabSection.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblLabSection.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(lblLabSection);

    lblNotes.setText("Operator Notes / Remarks:", juce::dontSendNotification);
    lblNotes.setFont(juce::FontOptions(10.0f));
    lblNotes.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    addAndMakeVisible(lblNotes);

    txtOperatorNotes.setMultiLine(true);
    txtOperatorNotes.setReturnKeyStartsNewLine(true);
    txtOperatorNotes.setTextToShowWhenEmpty("Enter session observations, board rev, room remarks...", SoundIdTheme::textMuted);
    txtOperatorNotes.setFont(juce::FontOptions(10.5f));
    txtOperatorNotes.setColour(juce::TextEditor::backgroundColourId, SoundIdTheme::surfaceSubtle);
    txtOperatorNotes.setColour(juce::TextEditor::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(txtOperatorNotes);

    lblAmbientTemp.setText("Ambient Temp (\xc2\xb0\x43):", juce::dontSendNotification);
    lblAmbientTemp.setFont(juce::FontOptions(10.0f));
    lblAmbientTemp.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    addAndMakeVisible(lblAmbientTemp);

    txtAmbientTemp.setText("22.0");
    txtAmbientTemp.setFont(juce::FontOptions(11.0f));
    txtAmbientTemp.setInputRestrictions(5, "0123456789.");
    txtAmbientTemp.setColour(juce::TextEditor::backgroundColourId, SoundIdTheme::surfaceSubtle);
    txtAmbientTemp.setColour(juce::TextEditor::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(txtAmbientTemp);

    lblWarmupTime.setText("Warmup (min):", juce::dontSendNotification);
    lblWarmupTime.setFont(juce::FontOptions(10.0f));
    lblWarmupTime.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    addAndMakeVisible(lblWarmupTime);

    txtWarmupTime.setText("15");
    txtWarmupTime.setFont(juce::FontOptions(11.0f));
    txtWarmupTime.setInputRestrictions(4, "0123456789");
    txtWarmupTime.setColour(juce::TextEditor::backgroundColourId, SoundIdTheme::surfaceSubtle);
    txtWarmupTime.setColour(juce::TextEditor::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(txtWarmupTime);

    // ==========================================
    // 3. Target Export Directory
    // ==========================================
    lblFileExportSection.setText("TARGET EXPORT DIRECTORY", juce::dontSendNotification);
    lblFileExportSection.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblFileExportSection.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(lblFileExportSection);

    lblFileExportPathVal.setFont(juce::FontOptions(10.0f));
    lblFileExportPathVal.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    addAndMakeVisible(lblFileExportPathVal);

    btnFileChangeExport.setTooltip("Change Directory - Select output folder for C++ headers, JSON, and reports");
    btnFileChangeExport.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnFileChangeExport.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnFileChangeExport.onClick = [this] { if (onChangeExportFolderClicked) onChangeExportFolderClicked(); };
    addAndMakeVisible(btnFileChangeExport);

    btnFileRevealExport.setTooltip("Open in Explorer - Reveal output directory in Windows File Explorer");
    btnFileRevealExport.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnFileRevealExport.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnFileRevealExport.onClick = [this] { if (onRevealExportFolderClicked) onRevealExportFolderClicked(); };
    addAndMakeVisible(btnFileRevealExport);

    btnFileExportReport.setTooltip("Export Certification Report - Generate complete HTML/SVG audit report and NAM dataset");
    btnFileExportReport.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    btnFileExportReport.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    btnFileExportReport.onClick = [this] { if (onExportReportClicked) onExportReportClicked(); };
    addAndMakeVisible(btnFileExportReport);

    // ==========================================
    // 3. Preview Exported Code & Data
    // ==========================================
    lblFilePreviewSection.setText("PREVIEW EXPORTED CODE & DATA (.H / .JSON)", juce::dontSendNotification);
    lblFilePreviewSection.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    lblFilePreviewSection.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(lblFilePreviewSection);

    comboPreviewFiles.onChange = [this] {
        int selId = comboPreviewFiles.getSelectedId();
        if (selId >= 1)
        {
            juce::File expDir(currentExportFolderPath);
            auto files = expDir.findChildFiles(juce::File::findFiles, false, "*.h;*.json;*.abdlabtest");
            std::sort(files.begin(), files.end(), [](const juce::File& a, const juce::File& b) {
                return a.getLastModificationTime() > b.getLastModificationTime();
            });
            if (selId - 1 < static_cast<int>(files.size()))
            {
                auto targetFile = files[static_cast<size_t>(selId - 1)];
                txtCodePreview.setText(targetFile.loadFileAsString().substring(0, 8000));
            }
        }
    };
    addAndMakeVisible(comboPreviewFiles);

    txtCodePreview.setMultiLine(true);
    txtCodePreview.setReadOnly(true);
    txtCodePreview.setFont(juce::FontOptions("Consolas", 11.0f, juce::Font::plain));
    txtCodePreview.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1e1e1e));
    txtCodePreview.setColour(juce::TextEditor::textColourId, juce::Colour(0xffd4d4d4));
    addAndMakeVisible(txtCodePreview);

    btnCopyPreview.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnCopyPreview.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnCopyPreview.onClick = [this] {
        juce::SystemClipboard::copyTextToClipboard(txtCodePreview.getText());
    };
    addAndMakeVisible(btnCopyPreview);

    btnOpenFileInEditor.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnOpenFileInEditor.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnOpenFileInEditor.onClick = [this] {
        int selId = comboPreviewFiles.getSelectedId();
        if (selId >= 1)
        {
            juce::File expDir(currentExportFolderPath);
            auto files = expDir.findChildFiles(juce::File::findFiles, false, "*.h;*.json;*.abdlabtest");
            std::sort(files.begin(), files.end(), [](const juce::File& a, const juce::File& b) {
                return a.getLastModificationTime() > b.getLastModificationTime();
            });
            if (selId - 1 < static_cast<int>(files.size()))
            {
                files[static_cast<size_t>(selId - 1)].startAsProcess();
            }
        }
    };
    addAndMakeVisible(btnOpenFileInEditor);

    // ==========================================
    // 4. Updates & Exit
    // ==========================================
    btnCheckUpdates.setTooltip("Check for Updates - Check GitHub releases for newer software versions");
    btnCheckUpdates.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffe0e7ff));
    btnCheckUpdates.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff3730a3));
    btnCheckUpdates.onClick = [this] { if (onCheckUpdatesClicked) onCheckUpdatesClicked(); };
    addAndMakeVisible(btnCheckUpdates);

    btnFileExit.setTooltip("Exit - Close ABDAudioLab application");
    btnFileExit.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfffee2e2));
    btnFileExit.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentRed);
    btnFileExit.onClick = [this] { if (onExitAppClicked) onExitAppClicked(); };
    addAndMakeVisible(btnFileExit);
}

void DrawerFileSessionTab::setExportDirectory(const juce::String& path)
{
    currentExportFolderPath = path;
    lblFileExportPathVal.setText(path, juce::dontSendNotification);
    refreshFilePreviewList();
}

void DrawerFileSessionTab::refreshFilePreviewList()
{
    comboPreviewFiles.clear(juce::dontSendNotification);
    txtCodePreview.clear();

    juce::File expDir(currentExportFolderPath);
    if (!expDir.isDirectory()) return;

    auto files = expDir.findChildFiles(juce::File::findFiles, false, "*.h;*.json;*.abdlabtest");
    std::sort(files.begin(), files.end(), [](const juce::File& a, const juce::File& b) {
        return a.getLastModificationTime() > b.getLastModificationTime();
    });

    int id = 1;
    for (const auto& f : files)
    {
        comboPreviewFiles.addItem(f.getFileName(), id++);
    }

    if (!files.isEmpty())
    {
        comboPreviewFiles.setSelectedId(1, juce::sendNotification);
    }
}

void DrawerFileSessionTab::updateTheme()
{
    auto updateBtn = [](juce::TextButton& btn) {
        btn.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
        btn.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    };

    updateBtn(btnFileNew);
    updateBtn(btnFileOpen);
    updateBtn(btnFileSave);
    updateBtn(btnFileSaveAs);
    btnFileReanalyze.setColour(juce::TextButton::buttonColourId, SoundIdTheme::surfaceSubtle);
    btnFileReanalyze.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentGreen);
    updateBtn(btnFileChangeExport);
    updateBtn(btnFileRevealExport);
    updateBtn(btnCopyPreview);
    updateBtn(btnOpenFileInEditor);

    lblFileSection.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    lblLabSection.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    lblNotes.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    lblAmbientTemp.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    lblWarmupTime.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    txtOperatorNotes.setColour(juce::TextEditor::backgroundColourId, SoundIdTheme::surfaceSubtle);
    txtOperatorNotes.setColour(juce::TextEditor::textColourId, SoundIdTheme::textPrimary);
    txtAmbientTemp.setColour(juce::TextEditor::backgroundColourId, SoundIdTheme::surfaceSubtle);
    txtAmbientTemp.setColour(juce::TextEditor::textColourId, SoundIdTheme::textPrimary);
    txtWarmupTime.setColour(juce::TextEditor::backgroundColourId, SoundIdTheme::surfaceSubtle);
    txtWarmupTime.setColour(juce::TextEditor::textColourId, SoundIdTheme::textPrimary);

    lblFileExportSection.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    lblFileExportPathVal.setColour(juce::Label::textColourId, SoundIdTheme::textSecondary);
    lblFilePreviewSection.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);

    repaint();
}

int DrawerFileSessionTab::getPreferredHeight() const noexcept
{
    return 810;
}

void DrawerFileSessionTab::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

void DrawerFileSessionTab::resized()
{
    int w = getWidth();
    int y = 0;

    lblFileSection.setBounds(0, y, w, 16);
    y += 22;

    btnFileNew.setBounds(0, y, w, 30);
    y += 36;
    btnFileOpen.setBounds(0, y, w, 30);
    y += 36;
    btnFileSave.setBounds(0, y, w, 30);
    y += 36;
    btnFileSaveAs.setBounds(0, y, w, 30);
    y += 36;
    btnFileReanalyze.setBounds(0, y, w, 30);
    y += 42;

    // Laboratory Conditions & Notes (1.7.12)
    lblLabSection.setBounds(0, y, w, 16);
    y += 20;
    lblNotes.setBounds(0, y, w, 14);
    y += 16;
    txtOperatorNotes.setBounds(0, y, w, 54);
    y += 60;
    int halfColW = (w - 8) / 2;
    lblAmbientTemp.setBounds(0, y, halfColW, 14);
    lblWarmupTime.setBounds(halfColW + 8, y, w - halfColW - 8, 14);
    y += 16;
    txtAmbientTemp.setBounds(0, y, halfColW, 26);
    txtWarmupTime.setBounds(halfColW + 8, y, w - halfColW - 8, 26);
    y += 36;

    lblFileExportSection.setBounds(0, y, w, 16);
    y += 18;
    lblFileExportPathVal.setBounds(2, y, w - 2, 18);
    y += 22;

    btnFileChangeExport.setBounds(0, y, w, 30);
    y += 34;
    btnFileRevealExport.setBounds(0, y, w, 30);
    y += 34;
    btnFileExportReport.setBounds(0, y, w, 32);
    y += 40;

    lblFilePreviewSection.setBounds(0, y, w, 16);
    y += 18;
    comboPreviewFiles.setBounds(0, y, w, 28);
    y += 32;
    txtCodePreview.setBounds(0, y, w, 110);
    y += 114;
    int halfW = (w - 8) / 2;
    btnCopyPreview.setBounds(0, y, halfW, 28);
    btnOpenFileInEditor.setBounds(halfW + 8, y, w - halfW - 8, 28);
    y += 34;

    btnCheckUpdates.setBounds(0, y, w, 30);
    y += 36;

    btnFileExit.setBounds(0, y, w, 32);
}

} // namespace abdaudiolab::gui
