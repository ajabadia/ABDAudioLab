#include "SessionReportManager.h"

namespace abdaudiolab {
namespace gui {

void SessionReportManager::triggerSaveSessionAsync(const juce::File& initialLocation,
                                                   const juce::String& defaultName,
                                                   std::function<void(const juce::File& chosenFile)> onFileChosen)
{
    juce::File startDir = initialLocation.isDirectory() ? initialLocation : lastExportDirectory;
    if (!startDir.isDirectory())
        startDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    juce::File defaultFile = startDir.getChildFile(defaultName.isNotEmpty() ? defaultName : "session.abdlabtest");

    fileChooser = std::make_unique<juce::FileChooser>(
        "Save ABDAudioLab Session Package (.abdlabtest)...",
        defaultFile,
        "*.abdlabtest",
        true,
        true
    );

    auto dialogFlags = juce::FileBrowserComponent::saveMode |
                       juce::FileBrowserComponent::canSelectFiles |
                       juce::FileBrowserComponent::warnAboutOverwriting;

    fileChooser->launchAsync(dialogFlags, [this, onFileChosen](const juce::FileChooser& fc) {
        auto file = fc.getResult();
        if (file != juce::File())
        {
            if (file.getFileExtension() != ".abdlabtest")
                file = file.withFileExtension(".abdlabtest");

            lastExportDirectory = file.getParentDirectory();
            if (onFileChosen)
                onFileChosen(file);
        }
        else
        {
            if (onFileChosen)
                onFileChosen(juce::File());
        }
    });
}

void SessionReportManager::triggerLoadSessionAsync(const juce::File& initialLocation,
                                                   std::function<void(const juce::File& chosenFile)> onFileChosen)
{
    juce::File startDir = initialLocation.isDirectory() ? initialLocation : lastExportDirectory;
    if (!startDir.isDirectory())
        startDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    fileChooser = std::make_unique<juce::FileChooser>(
        "Open ABDAudioLab Session Package (.abdlabtest)...",
        startDir,
        "*.abdlabtest",
        true
    );

    auto dialogFlags = juce::FileBrowserComponent::openMode |
                       juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(dialogFlags, [this, onFileChosen](const juce::FileChooser& fc) {
        auto file = fc.getResult();
        if (file.existsAsFile())
        {
            lastExportDirectory = file.getParentDirectory();
            if (onFileChosen)
                onFileChosen(file);
        }
        else
        {
            if (onFileChosen)
                onFileChosen(juce::File());
        }
    });
}

void SessionReportManager::triggerSelectExportFolderAsync(const juce::File& initialFolder,
                                                          std::function<void(const juce::File& chosenDir)> onDirectoryChosen)
{
    juce::File startDir = initialFolder.isDirectory() ? initialFolder : lastExportDirectory;
    if (!startDir.isDirectory())
        startDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    fileChooser = std::make_unique<juce::FileChooser>(
        "Select Output Directory for C++ Look-Up Tables & Reports",
        startDir,
        "*"
    );

    auto dialogFlags = juce::FileBrowserComponent::openMode |
                       juce::FileBrowserComponent::canSelectDirectories;

    fileChooser->launchAsync(dialogFlags, [this, onDirectoryChosen](const juce::FileChooser& fc) {
        auto chosen = fc.getResult();
        if (chosen.isDirectory())
        {
            lastExportDirectory = chosen;
            if (onDirectoryChosen)
                onDirectoryChosen(chosen);
        }
        else
        {
            if (onDirectoryChosen)
                onDirectoryChosen(juce::File());
        }
    });
}

bool SessionReportManager::exportLutAndJsonReports(const juce::File& destinationDir,
                                                   const juce::String& baseName,
                                                   const core::ProfilingMetadata& metadata,
                                                   const std::vector<exporting::MeasuredPoint>& points)
{
    if (!destinationDir.exists())
        destinationDir.createDirectory();

    juce::String safeBaseName = juce::File::createLegalFileName(baseName).replaceCharacter(' ', '_');
    juce::File headerFile = destinationDir.getChildFile(safeBaseName + "_LUT.h");
    juce::File jsonFile = destinationDir.getChildFile(safeBaseName + "_Report.json");

    bool okCpp = exporting::LutExporter::exportToCppHeader(
        headerFile.getFullPathName().toStdString(),
        metadata,
        safeBaseName.toStdString(),
        points
    );

    bool okJson = exporting::LutExporter::exportToJsonReport(
        jsonFile.getFullPathName().toStdString(),
        metadata,
        points
    );

    return okCpp && okJson;
}

bool SessionReportManager::exportCertificationHtmlReport(const juce::File& destinationDir,
                                                         const juce::String& baseName,
                                                         const exporting::SessionManifestData& manifest,
                                                         const std::vector<exporting::MeasuredPoint>& points,
                                                         juce::File& outHtmlFile)
{
    if (!destinationDir.exists())
        destinationDir.createDirectory();

    juce::String safeBaseName = juce::File::createLegalFileName(baseName).replaceCharacter(' ', '_');
    outHtmlFile = destinationDir.getChildFile(safeBaseName + "_Certification_Report.html");

    return exporting::CertificationReportExporter::exportReportToHtml(
        outHtmlFile.getFullPathName().toStdString(),
        manifest,
        points
    );
}

bool SessionReportManager::launchHtmlReportInDefaultViewer(const juce::File& reportFile, juce::String& outError)
{
    if (!reportFile.existsAsFile())
    {
        outError = "El archivo de informe HTML no existe: " + reportFile.getFullPathName();
        return false;
    }

    // Usar startAsProcess() para invocar el visor HTML nativo del sistema
    if (reportFile.startAsProcess())
        return true;

    // Fallback secundario vía URL en caso de que startAsProcess() sea rechazado por el SO
    if (juce::URL(reportFile).launchInDefaultBrowser())
        return true;

    outError = "No se pudo iniciar el proceso del navegador predeterminado para: " + reportFile.getFullPathName();
    return false;
}

void SessionReportManager::triggerPeriodicAutoSaveCheckpoint(core::SessionManager& sessionManager,
                                                             const core::SessionManifest& currentManifest)
{
    sessionManager.triggerAutoSave(currentManifest);
}

juce::File SessionReportManager::checkRecoverableSession(const core::SessionManager& sessionManager) const
{
    return sessionManager.getRecoverableAutoSaveFile();
}

} // namespace gui
} // namespace abdaudiolab
