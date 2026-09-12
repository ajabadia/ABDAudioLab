/**
 * @file PluginScanDirectoriesModal.cpp
 * @brief Implementation of the plugin scan directories management modal.
 */

#include "PluginScanDirectoriesModal.h"

namespace abdaudiolab::gui
{

// ── DirectoryListModel ──────────────────────────────────────────────

int PluginScanDirectoriesModal::DirectoryListModel::getNumRows()
{
    return static_cast<int>(modal.scanDirectories.size());
}

void PluginScanDirectoriesModal::DirectoryListModel::paintListBoxItem(
    int /*rowNumber*/, juce::Graphics& /*g*/, int /*width*/, int /*height*/, bool /*rowIsSelected*/)
{
    // Custom row components used instead
}

void PluginScanDirectoriesModal::DirectoryListModel::listBoxItemClicked(int /*row*/, const juce::MouseEvent&)
{
}

juce::Component* PluginScanDirectoriesModal::DirectoryListModel::refreshComponentForRow(
    int rowNumber, bool isRowSelected, juce::Component* existingComponentToUpdate)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(modal.scanDirectories.size()))
    {
        delete existingComponentToUpdate;
        return nullptr;
    }

    auto* rowComp = dynamic_cast<DirectoryRowComponent*>(existingComponentToUpdate);
    if (rowComp == nullptr)
    {
        delete existingComponentToUpdate;
        rowComp = new DirectoryRowComponent(modal);
    }

    rowComp->setRowData(rowNumber, modal.scanDirectories[static_cast<size_t>(rowNumber)], isRowSelected);
    return rowComp;
}

// ── DirectoryRowComponent ───────────────────────────────────────────

PluginScanDirectoriesModal::DirectoryRowComponent::DirectoryRowComponent(PluginScanDirectoriesModal& owner)
    : modal(owner)
{
    btnRemove.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffcc3333));
    btnRemove.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnRemove.setTooltip("Quitar esta carpeta de la lista");
    btnRemove.onClick = [this] {
        if (row >= 0)
            modal.removeDirectory(row);
    };
    addAndMakeVisible(btnRemove);
}

void PluginScanDirectoriesModal::DirectoryRowComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    if (isSelected)
        g.setColour(SoundIdTheme::accentBlue.withAlpha(0.12f));
    else
        g.setColour(SoundIdTheme::bgCard);
    g.fillRoundedRectangle(bounds.reduced(1.0f), 4.0f);

    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(bounds.reduced(1.0f), 4.0f, 0.5f);

    auto textArea = bounds.reduced(10.0f, 4.0f);
    textArea.removeFromRight(40.0f); // Space for remove button

    g.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::plain));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText(directory.getFullPathName(), textArea.removeFromTop(textArea.getHeight() * 0.55f),
               juce::Justification::centredLeft, true);

    g.setFont(juce::FontOptions("Inter", 9.5f, juce::Font::italic));
    g.setColour(SoundIdTheme::textMuted);

    int numPlugins = 0;
    if (directory.isDirectory())
    {
        auto files = directory.findChildFiles(juce::File::findFilesAndDirectories, true, "*.vst3;*.component;*.lv2");
        numPlugins = files.size();
    }
    juce::String info = directory.exists()
        ? juce::String(numPlugins) + " archivos de plugin encontrados"
        : "Carpeta no encontrada";
    g.drawText(info, textArea, juce::Justification::centredLeft, true);
}

void PluginScanDirectoriesModal::DirectoryRowComponent::resized()
{
    auto b = getLocalBounds();
    btnRemove.setBounds(b.removeFromRight(34).reduced(4));
}

void PluginScanDirectoriesModal::DirectoryRowComponent::setRowData(int rowIndex, const juce::File& dir, bool selected)
{
    row = rowIndex;
    directory = dir;
    isSelected = selected;
    repaint();
}

// ── PluginScanDirectoriesModal ──────────────────────────────────────

PluginScanDirectoriesModal::PluginScanDirectoriesModal()
{
    // Title
    lblTitle.setText("Carpetas de Plugins (VST3 / AU / LV2)", juce::dontSendNotification);
    lblTitle.setFont(juce::FontOptions("Inter", 15.0f, juce::Font::bold));
    lblTitle.setColour(juce::Label::textColourId, SoundIdTheme::textPrimary);
    addAndMakeVisible(lblTitle);

    // Status
    lblStatus.setText("Configura las carpetas donde tienes tus plugins instalados.", juce::dontSendNotification);
    lblStatus.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::italic));
    lblStatus.setColour(juce::Label::textColourId, SoundIdTheme::textMuted);
    addAndMakeVisible(lblStatus);

    // Directory list
    directoryList.setModel(&listModel);
    directoryList.setRowHeight(52);
    directoryList.setColour(juce::ListBox::backgroundColourId, SoundIdTheme::bgLight);
    directoryList.setColour(juce::ListBox::outlineColourId, SoundIdTheme::borderSubtle);
    addAndMakeVisible(directoryList);

    // Add Folder button
    btnAddFolder.setButtonText("Agregar Carpeta...");
    btnAddFolder.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentBlue.withAlpha(0.2f));
    btnAddFolder.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentBlue);
    btnAddFolder.onClick = [this] {
        fileChooser = std::make_shared<juce::FileChooser>(
            juce::String::fromUTF8(u8"Seleccionar carpeta de plugins"),
            juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory),
            "");

        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this](const juce::FileChooser& fc) {
                auto result = fc.getResult();
                if (result.exists() && result.isDirectory())
                {
                    addDirectory(result);
                }
            });
    };
    addAndMakeVisible(btnAddFolder);

    // Scan All button
    btnScanAll.setButtonText("Escanear Todas");
    btnScanAll.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen.withAlpha(0.25f));
    btnScanAll.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentGreen);
    btnScanAll.onClick = [this] { startScan(); };
    addAndMakeVisible(btnScanAll);

    btnClose.setButtonText("Cerrar");
    btnClose.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCardHover);
    btnClose.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
    btnClose.onClick = [this] {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
        else
            setVisible(false);
    };
    addAndMakeVisible(btnClose);

    // Scan complete handler
    onScanComplete = [this](int pluginsFound) {
        btnScanAll.setEnabled(true);
        btnAddFolder.setEnabled(true);
        lblStatus.setText(juce::String::fromUTF8(u8"Escaneo finalizado: ")
            + juce::String(pluginsFound) + juce::String::fromUTF8(u8" plugins disponibles en catálogo."),
            juce::dontSendNotification);
        rebuildListContent();
    };

    setSize(560, 420);
}

juce::File PluginScanDirectoriesModal::getSettingsFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("ABDAudioLab")
        .getChildFile("PluginScanDirs.xml");
}

std::vector<juce::File> PluginScanDirectoriesModal::loadPersistedDirectories()
{
    scanDirectories.clear();

    auto settingsFile = getSettingsFile();
    if (settingsFile.existsAsFile())
    {
        if (auto xml = juce::parseXML(settingsFile))
        {
            for (auto* child : xml->getChildIterator())
            {
                if (child->hasTagName("Directory"))
                {
                    juce::String path = child->getStringAttribute("path");
                    if (path.isNotEmpty())
                        scanDirectories.push_back(juce::File(path));
                }
            }
        }
    }

    // Add default VST3 paths if list is empty (first run)
    if (scanDirectories.empty())
    {
#if JUCE_WINDOWS
        auto commonVst3 = juce::File("C:\\Program Files\\Common Files\\VST3");
        if (commonVst3.isDirectory())
            scanDirectories.push_back(commonVst3);
#elif JUCE_MAC
        auto userVst3 = juce::File("~/Library/Audio/Plug-Ins/VST3");
        auto sysVst3 = juce::File("/Library/Audio/Plug-Ins/VST3");
        if (userVst3.isDirectory()) scanDirectories.push_back(userVst3);
        if (sysVst3.isDirectory()) scanDirectories.push_back(sysVst3);
#endif
        savePersistedDirectories();
    }

    rebuildListContent();
    return scanDirectories;
}

void PluginScanDirectoriesModal::savePersistedDirectories()
{
    auto settingsFile = getSettingsFile();
    settingsFile.getParentDirectory().createDirectory();

    juce::XmlElement root("PluginScanDirectories");
    for (const auto& dir : scanDirectories)
    {
        auto* child = root.createNewChildElement("Directory");
        child->setAttribute("path", dir.getFullPathName());
    }

    root.writeTo(settingsFile);
}

void PluginScanDirectoriesModal::addDirectory(const juce::File& dir)
{
    // Avoid duplicates
    for (const auto& existing : scanDirectories)
    {
        if (existing.getFullPathName().equalsIgnoreCase(dir.getFullPathName()))
            return;
    }

    scanDirectories.push_back(dir);
    savePersistedDirectories();
    rebuildListContent();

    lblStatus.setText(juce::String::fromUTF8(u8"Carpeta añadida: ") + dir.getFileName(),
                      juce::dontSendNotification);
}

void PluginScanDirectoriesModal::removeDirectory(int index)
{
    if (index >= 0 && index < static_cast<int>(scanDirectories.size()))
    {
        juce::String name = scanDirectories[static_cast<size_t>(index)].getFileName();
        scanDirectories.erase(scanDirectories.begin() + index);
        savePersistedDirectories();
        rebuildListContent();

        lblStatus.setText(juce::String::fromUTF8(u8"Carpeta eliminada: ") + name,
                          juce::dontSendNotification);
    }
}

void PluginScanDirectoriesModal::rebuildListContent()
{
    directoryList.updateContent();
    directoryList.repaint();
}

void PluginScanDirectoriesModal::startScan()
{
    if (scanDirectories.empty())
    {
        lblStatus.setText("Carpeta no encontrada. Verifica la ruta.",
                          juce::dontSendNotification);
        return;
    }

    btnScanAll.setEnabled(false);
    btnAddFolder.setEnabled(false);
    lblStatus.setText("Escaneando plugins...", juce::dontSendNotification);

    juce::FileSearchPath searchPath;
    for (const auto& dir : scanDirectories)
    {
        if (dir.isDirectory())
            searchPath.add(dir);
    }

    if (onScanRequested)
    {
        onScanRequested(searchPath, [this](const juce::String& currentPlugin, float progress) {
            juce::MessageManager::callAsync([this, currentPlugin, progress]() {
                progressValue = static_cast<double>(progress);
                lblStatus.setText(currentPlugin
                    + " (" + juce::String(static_cast<int>(progress * 100)) + "%)",
                    juce::dontSendNotification);
            });
        });
    }
    else
    {
        btnScanAll.setEnabled(true);
        btnAddFolder.setEnabled(true);
    }
}

void PluginScanDirectoriesModal::showModal(juce::Component* parent)
{
    loadPersistedDirectories();

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = juce::String::fromUTF8(u8"Gestionar Carpetas de Plugins");
    options.dialogBackgroundColour = SoundIdTheme::bgLight;
    options.content.setNonOwned(this);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
    options.resizable = false;
    options.componentToCentreAround = parent;

    options.launchAsync();
}

void PluginScanDirectoriesModal::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(SoundIdTheme::bgLight);
    g.fillRoundedRectangle(b, 10.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(b.reduced(0.5f), 10.0f, 1.0f);
}

void PluginScanDirectoriesModal::resized()
{
    auto b = getLocalBounds().reduced(20);

    lblTitle.setBounds(b.removeFromTop(28));
    b.removeFromTop(6);
    lblStatus.setBounds(b.removeFromTop(20));
    b.removeFromTop(10);

    // Bottom buttons
    auto bottomBar = b.removeFromBottom(36);
    btnClose.setBounds(bottomBar.removeFromRight(100));
    bottomBar.removeFromRight(10);
    btnScanAll.setBounds(bottomBar.removeFromRight(160));
    bottomBar.removeFromRight(10);
    btnAddFolder.setBounds(bottomBar.removeFromRight(180));

    b.removeFromBottom(10);

    // Directory list fills the remaining space
    directoryList.setBounds(b);
}

} // namespace abdaudiolab::gui
