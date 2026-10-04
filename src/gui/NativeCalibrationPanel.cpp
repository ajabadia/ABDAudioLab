#include "NativeCalibrationPanel.h"
#include "SoundIdTheme.h"
#include "../BuildVersion.h"
#include <cmath>

namespace abdaudiolab::gui
{

NativeCalibrationPanel::NativeCalibrationPanel(audio::LabAudioEngine& engine)
    : audioEngine(engine), progressBar(progressValue)
{
    btnStartMeasure.setButtonText(juce::String::fromUTF8(u8"Iniciar Calibración Loopback"));
    btnStartMeasure.setTooltip(juce::String::fromUTF8(u8"Reproduce un barrido Farina de 1.0s para medir latencia de ida y vuelta, ganancia y compensación H(f)"));
    btnStartMeasure.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    btnStartMeasure.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnStartMeasure.onClick = [this] { startCalibrationSweep(); };
    addAndMakeVisible(btnStartMeasure);

    btnReuseCalibration.setButtonText(juce::String::fromUTF8(u8"Reutilizar calibración guardada"));
    btnReuseCalibration.setTooltip(juce::String::fromUTF8(u8"Aplica la calibración coincidente encontrada para la configuración actual"));
    btnReuseCalibration.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen.withAlpha(0.25f));
    btnReuseCalibration.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentGreen);
    btnReuseCalibration.onClick = [this] { reuseMatchingProfile(); };
    addChildComponent(btnReuseCalibration);

    btnSkip.setButtonText(juce::String::fromUTF8(u8"Continuar sin calibrar (Bypass)"));
    btnSkip.setTooltip(juce::String::fromUTF8(u8"Continúa sin compensación de latencia ni nivel de la interfaz de audio. Restablece ganancia neutral."));
    btnSkip.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCardHover);
    btnSkip.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentAmber);
    btnSkip.onClick = [this] { skipCalibration(); };
    addAndMakeVisible(btnSkip);

    btnContinue.setButtonText(juce::String::fromUTF8(u8"Continuar a Run Session (Paso 3) ➔"));
    btnContinue.setTooltip(juce::String::fromUTF8(u8"Avanza al Paso 3: excitación y perfilado de la sesión"));
    btnContinue.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentGreen);
    btnContinue.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    btnContinue.onClick = [this] {
        if (onContinueToSession)
            onContinueToSession();
    };
    addChildComponent(btnContinue);

    btnVerifyDigital.setButtonText(juce::String::fromUTF8(u8"Verificar Latencia Digital"));
    btnVerifyDigital.setTooltip(juce::String::fromUTF8(u8"Verifica la preparación del bus digital y latencia del plugin"));
    btnVerifyDigital.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentBlue.withAlpha(0.25f));
    btnVerifyDigital.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textPrimary);
    btnVerifyDigital.onClick = [this] {
        if (onVerifyDigitalRequested)
            onVerifyDigitalRequested();
    };
    addChildComponent(btnVerifyDigital);

    btnRetry.setButtonText(juce::String::fromUTF8(u8"Repetir Calibración"));
    btnRetry.setTooltip(juce::String::fromUTF8(u8"Vuelve a ejecutar la calibración tras comprobar conexiones y niveles"));
    btnRetry.setColour(juce::TextButton::buttonColourId, SoundIdTheme::accentAmber);
    btnRetry.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    btnRetry.onClick = [this] { startCalibrationSweep(); };
    addChildComponent(btnRetry);

    btnSaveCalibration.setButtonText(juce::String::fromUTF8(u8"Guardar Calibración"));
    btnSaveCalibration.setTooltip(juce::String::fromUTF8(u8"Guarda este resultado de calibración en AppData para conservarlo"));
    btnSaveCalibration.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCardHover);
    btnSaveCalibration.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentGreen);
    btnSaveCalibration.onClick = [this] { saveCurrentCalibrationProfile(); };
    addChildComponent(btnSaveCalibration);

    btnToggleSavedProfiles.setButtonText(juce::String::fromUTF8(u8"Calibraciones Guardadas"));
    btnToggleSavedProfiles.setTooltip(juce::String::fromUTF8(u8"Muestra o repliega el listado de calibraciones guardadas en disco"));
    btnToggleSavedProfiles.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCardHover);
    btnToggleSavedProfiles.setColour(juce::TextButton::textColourOffId, SoundIdTheme::textSecondary);
    btnToggleSavedProfiles.onClick = [this] {
        showSavedProfilesSection_ = !showSavedProfilesSection_;
        refreshSavedProfiles();
        resized();
        repaint();
    };
    addAndMakeVisible(btnToggleSavedProfiles);

    btnDeleteProfile.setButtonText(juce::String::fromUTF8(u8"Eliminar"));
    btnDeleteProfile.setTooltip(juce::String::fromUTF8(u8"Elimina la calibración guardada seleccionada"));
    btnDeleteProfile.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCardHover);
    btnDeleteProfile.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentRed);
    btnDeleteProfile.onClick = [this] { deleteSelectedProfile(); };
    addChildComponent(btnDeleteProfile);

    btnViewProfileDetails.setButtonText(juce::String::fromUTF8(u8"Ver Detalles"));
    btnViewProfileDetails.setTooltip(juce::String::fromUTF8(u8"Muestra u oculta los detalles técnicos de la calibración guardada"));
    btnViewProfileDetails.setColour(juce::TextButton::buttonColourId, SoundIdTheme::bgCardHover);
    btnViewProfileDetails.setColour(juce::TextButton::textColourOffId, SoundIdTheme::accentBlue);
    btnViewProfileDetails.onClick = [this] {
        showProfileDetails_ = !showProfileDetails_;
        repaint();
    };
    addChildComponent(btnViewProfileDetails);

    progressBar.setColour(juce::ProgressBar::foregroundColourId, SoundIdTheme::accentGreen);
    progressBar.setColour(juce::ProgressBar::backgroundColourId, SoundIdTheme::borderSubtle);
    addChildComponent(progressBar);

    refreshSavedProfiles();
    startTimerHz(30);
}

NativeCalibrationPanel::~NativeCalibrationPanel()
{
    stopTimer();
}

void NativeCalibrationPanel::neutralizeActiveTrim()
{
    audioEngine.setInputAutoTrim(1.0f);
}

void NativeCalibrationPanel::evaluateProfilesMatching()
{
    auto* dev = audioEngine.getDeviceManager().getCurrentAudioDevice();
    auto currentSnap = calibration::CurrentAudioConfigurationSnapshot::captureFrom(
        dev, 0, calibrationInputChannelName, 0, calibrationOutputChannelName);

    matchingProfile_ = calibration::CalibrationMatchEvaluator::findBestMatchingProfile(
        savedProfiles, currentSnap, &matchEvaluation_);

    if (currentState == State::ReadyToMeasure && matchingProfile_.has_value() && matchEvaluation_.isActionableMatch)
    {
        btnReuseCalibration.setVisible(true);
        btnReuseCalibration.setEnabled(true);
    }
    else
    {
        btnReuseCalibration.setVisible(false);
    }
    resized();
}

void NativeCalibrationPanel::reuseMatchingProfile()
{
    if (!matchingProfile_.has_value() || !matchEvaluation_.isActionableMatch)
        return;

    stopTimer();
    calibrationData = matchingProfile_->calibrationResult;
    activeCalibrationRecord_ = *matchingProfile_;
    activeAlignment = calibration::ActiveCalibrationAlignment::AlignedAndActive;
    currentState = State::Success;

    audioEngine.setInputAutoTrim(calibrationData.recommendedTrimGain);

    btnStartMeasure.setVisible(false);
    btnReuseCalibration.setVisible(false);
    btnSkip.setVisible(false);
    btnRetry.setVisible(true);
    btnContinue.setVisible(true);
    btnContinue.setEnabled(true);
    btnSaveCalibration.setVisible(false);

    saveFeedbackText_ = juce::String::fromUTF8(u8"Calibración guardada reutilizada correctamente.");

    if (onCalibrationApplied)
        onCalibrationApplied(calibrationData);

    startTimerHz(10);
    resized();
    repaint();
}

void NativeCalibrationPanel::resetToInitialState()
{
    currentState = State::ReadyToMeasure;
    activeAlignment = calibration::ActiveCalibrationAlignment::None;
    activeCalibrationRecord_ = {};
    neutralizeActiveTrim();
    measurementStep = 0;
    progressValue = 0.0;
    liveInputPeak = 0.0f;
    saveFeedbackText_ = {};
    showSavedProfilesSection_ = false;
    showProfileDetails_ = false;
    btnStartMeasure.setVisible(true);
    btnStartMeasure.setEnabled(true);
    btnSkip.setVisible(true);
    btnSkip.setEnabled(true);
    btnContinue.setVisible(false);
    btnRetry.setVisible(false);
    btnSaveCalibration.setVisible(false);
    btnDeleteProfile.setVisible(false);
    btnViewProfileDetails.setVisible(false);
    progressBar.setVisible(false);

    evaluateProfilesMatching();

    startTimerHz(30);
    repaint();
}

void NativeCalibrationPanel::refreshSavedProfiles()
{
    savedProfiles = profileStore.list();
    btnToggleSavedProfiles.setButtonText(juce::String::fromUTF8(u8"Calibraciones Guardadas (") +
                                         juce::String((int)savedProfiles.size()) + ")");

    if (savedProfiles.empty())
    {
        selectedProfileIndex_ = 0;
        btnDeleteProfile.setVisible(false);
        btnViewProfileDetails.setVisible(false);
    }
    else
    {
        if (selectedProfileIndex_ >= (int)savedProfiles.size())
            selectedProfileIndex_ = (int)savedProfiles.size() - 1;
        if (selectedProfileIndex_ < 0)
            selectedProfileIndex_ = 0;

        btnDeleteProfile.setVisible(showSavedProfilesSection_);
        btnViewProfileDetails.setVisible(showSavedProfilesSection_);
    }

    evaluateProfilesMatching();
}

void NativeCalibrationPanel::saveCurrentCalibrationProfile()
{
    if (!calibrationData.isCalibrated || calibrationData.clippingDetected)
    {
        saveFeedbackText_ = juce::String::fromUTF8(u8"No se puede guardar una calibración no válida.");
        repaint();
        return;
    }

    calibration::CalibrationRecord rec;
    rec.schemaVersion = 1;
    rec.createdAt = calibration::CalibrationProfileStore::getCurrentUtcIsoTimestamp();

    auto* dev = audioEngine.getDeviceManager().getCurrentAudioDevice();
    rec.deviceSnapshot.deviceName = dev != nullptr ? dev->getName().toStdString() : "Audio Device";
    rec.deviceSnapshot.driverType = dev != nullptr ? dev->getTypeName().toStdString() : "Unknown";
    rec.deviceSnapshot.sampleRate = dev != nullptr ? dev->getCurrentSampleRate() : audioEngine.getSampleRate();
    rec.deviceSnapshot.bufferSizeSamples = dev != nullptr ? dev->getCurrentBufferSizeSamples() : 0;

    rec.routingSnapshot.inputChannelIndex = 0;
    rec.routingSnapshot.inputChannelLabel = calibrationInputChannelName.toStdString();
    rec.routingSnapshot.outputChannelIndex = 0;
    rec.routingSnapshot.outputChannelLabel = calibrationOutputChannelName.toStdString();

    rec.calibrationResult = calibrationData;
    rec.provenance.applicationVersion = version::kAppVersion;
    rec.provenance.calibrationAlgorithmVersion = 1;

    rec.profileId = calibration::CalibrationProfileStore::generateDefaultProfileId(
        rec.deviceSnapshot.deviceName, rec.createdAt);

    auto saveRes = profileStore.save(rec, false);
    if (saveRes.success)
    {
        saveFeedbackText_ = juce::String::fromUTF8(u8"Calibración guardada. La verificación automática de compatibilidad con la interfaz actual se añadirá posteriormente.");
        btnSaveCalibration.setEnabled(false);
        refreshSavedProfiles();
    }
    else
    {
        saveFeedbackText_ = juce::String::fromUTF8(u8"Error al guardar: ") + juce::String(saveRes.errorMessage);
    }
    repaint();
}

void NativeCalibrationPanel::deleteSelectedProfile()
{
    if (selectedProfileIndex_ >= 0 && selectedProfileIndex_ < (int)savedProfiles.size())
    {
        std::string id = savedProfiles[static_cast<size_t>(selectedProfileIndex_)].profileId;
        profileStore.remove(id);
        refreshSavedProfiles();
        saveFeedbackText_ = juce::String::fromUTF8(u8"Perfil eliminado correctamente.");
        resized();
        repaint();
    }
}

void NativeCalibrationPanel::startCalibrationSweep()
{
    currentState = State::Measuring;
    measurementStep = 0;
    progressValue = 0.0;
    btnStartMeasure.setEnabled(false);
    btnSkip.setEnabled(false);
    btnContinue.setVisible(false);
    btnRetry.setVisible(false);
    progressBar.setVisible(true);
    repaint();

    // 1. Arm receiver for 1.25s capture
    double sr = audioEngine.getSampleRate();
    int captureSamples = static_cast<int>(sr * 1.25);
    audioEngine.getResponseReceiver().armCapture(captureSamples, 0.005f);

    // 2. Play 1.0s Farina sweep at full band
    audioEngine.getStimulusGenerator().setStimulus(audio::StimulusType::LogFarinaSweep, 1.0, 20.0f, 40000.0f);

    startTimer(50); // 50ms tick during sweep
}

void NativeCalibrationPanel::timerCallback()
{
    if (currentState == State::Measuring)
    {
        measurementStep++;
        progressValue = std::min(1.0, measurementStep * 0.05 / 1.25);

        if (measurementStep > 28) // ~1.4s
        {
            stopTimer();
            processCalibrationResult();
        }
    }
    else
    {
        // Live loopback signal detection
        float inL = audioEngine.getInputPeakL();
        float inR = audioEngine.getInputPeakR();
        liveInputPeak = std::max(liveInputPeak * 0.88f, std::max(inL, inR));

        // Periodic runtime alignment check
        if (activeAlignment == calibration::ActiveCalibrationAlignment::AlignedAndActive)
        {
            auto* dev = audioEngine.getDeviceManager().getCurrentAudioDevice();
            auto currentSnap = calibration::CurrentAudioConfigurationSnapshot::captureFrom(
                dev, 0, calibrationInputChannelName, 0, calibrationOutputChannelName);

            if (!calibration::CalibrationMatchEvaluator::isStillAligned(activeCalibrationRecord_, currentSnap))
            {
                activeAlignment = calibration::ActiveCalibrationAlignment::Misaligned;
                neutralizeActiveTrim();
                saveFeedbackText_ = juce::String::fromUTF8(u8"La configuración de audio cambió desde la última calibración. Calibración previa desactivada.");
                btnContinue.setEnabled(false);
                btnRetry.setVisible(true);
                btnSkip.setVisible(true);
                btnSkip.setEnabled(true);
                repaint();
            }
        }
    }
    repaint();
}

void NativeCalibrationPanel::processCalibrationResult()
{
    progressBar.setVisible(false);
    double sr = audioEngine.getSampleRate();

    std::vector<float> captured;
    audioEngine.getResponseReceiver().retrieveRecordedData(captured);
    calibrationData = math::LoopbackCalibrator::analyzeLoopback(captured, sr, 1.0, 20.0f, 40000.0f, -3.0f);

    if (calibrationData.isCalibrated && !calibrationData.clippingDetected)
    {
        currentState = State::Success;
        activeAlignment = calibration::ActiveCalibrationAlignment::AlignedAndActive;

        activeCalibrationRecord_.schemaVersion = 1;
        activeCalibrationRecord_.calibrationResult = calibrationData;
        auto* dev = audioEngine.getDeviceManager().getCurrentAudioDevice();
        activeCalibrationRecord_.deviceSnapshot.deviceName = dev != nullptr ? dev->getName().toStdString() : "Audio Device";
        activeCalibrationRecord_.deviceSnapshot.driverType = dev != nullptr ? dev->getTypeName().toStdString() : "Unknown";
        activeCalibrationRecord_.deviceSnapshot.sampleRate = dev != nullptr ? dev->getCurrentSampleRate() : audioEngine.getSampleRate();
        activeCalibrationRecord_.deviceSnapshot.bufferSizeSamples = dev != nullptr ? dev->getCurrentBufferSizeSamples() : 0;
        activeCalibrationRecord_.routingSnapshot.inputChannelIndex = 0;
        activeCalibrationRecord_.routingSnapshot.inputChannelLabel = calibrationInputChannelName.toStdString();
        activeCalibrationRecord_.routingSnapshot.outputChannelIndex = 0;
        activeCalibrationRecord_.routingSnapshot.outputChannelLabel = calibrationOutputChannelName.toStdString();

        audioEngine.setInputAutoTrim(calibrationData.recommendedTrimGain);
        btnStartMeasure.setVisible(false);
        btnReuseCalibration.setVisible(false);
        btnSkip.setVisible(false);
        btnRetry.setVisible(true);
        btnContinue.setVisible(true);
        btnContinue.setEnabled(true);
        btnSaveCalibration.setVisible(true);
        btnSaveCalibration.setEnabled(true);
        saveFeedbackText_ = {};
        refreshSavedProfiles();

        if (onCalibrationApplied)
            onCalibrationApplied(calibrationData);
    }
    else
    {
        currentState = State::Failed;
        btnStartMeasure.setVisible(false);
        btnSkip.setVisible(true);
        btnSkip.setEnabled(true);
        btnRetry.setVisible(true);
        btnContinue.setVisible(false);
        btnSaveCalibration.setVisible(false);
        startTimerHz(30);
    }
    repaint();
}

void NativeCalibrationPanel::skipCalibration()
{
    stopTimer();
    audioEngine.getResponseReceiver().reset();

    calibrationData = {};
    calibrationData.isCalibrated = false;
    calibrationData.sampleRate = audioEngine.getSampleRate();
    calibrationData.recommendedTrimGain = 1.0f; // 0 dB unity gain
    calibrationData.targetHeadroomDbfs = -3.0f;
    calibrationData.roundTripLatencyMs = 0.0f;
    calibrationData.latencySamples = 0;
    calibrationData.frequencyFlatnessDb = 0.0f;
    calibrationData.deviceName = "Bypassed / Nominal (0 dB)";

    neutralizeActiveTrim();
    currentState = State::Skipped;
    activeAlignment = calibration::ActiveCalibrationAlignment::Bypassed;
    activeCalibrationRecord_ = {};

    btnStartMeasure.setVisible(false);
    btnReuseCalibration.setVisible(false);
    btnSkip.setVisible(false);
    btnRetry.setVisible(true);
    btnContinue.setVisible(true);
    btnContinue.setEnabled(true);
    btnSaveCalibration.setVisible(false);
    saveFeedbackText_ = {};

    if (onCalibrationSkipped)
        onCalibrationSkipped();
    else if (onCalibrationApplied)
        onCalibrationApplied(calibrationData);

    repaint();
}

void NativeCalibrationPanel::updateFromSnapshot(const session::ProfilingSessionSnapshot& snapshot)
{
    isDigitalMode_ = (snapshot.calibration.audio.requirement == session::CalibrationRequirement::NotApplicable);
    isDigitalVerified_ = snapshot.calibration.digital.verified;

    if (isDigitalMode_)
    {
        btnStartMeasure.setVisible(false);
        btnSkip.setVisible(false);
        btnRetry.setVisible(false);
        progressBar.setVisible(false);
        btnVerifyDigital.setVisible(true);
        btnContinue.setVisible(true);
        btnContinue.setEnabled(isDigitalVerified_);

        if (isDigitalVerified_)
        {
            digitalStatusText_ = juce::String::fromUTF8(u8"Ruta digital verificada: Buffer 0 dBFS, 0 ms latencia física.\nListo para proceder al perfilado.");
            btnVerifyDigital.setButtonText(juce::String::fromUTF8(u8"✓ Verificación Completa"));
            btnVerifyDigital.setEnabled(false);
        }
        else
        {
            digitalStatusText_ = juce::String::fromUTF8(u8"Modo digital activo (Plugin VST3 / Sintetizador Virtual)\n"
                                                       u8"No necesitas conectar un cable de loopback.\n"
                                                       u8"La calibración analógica de la interfaz no se aplica a plugins ni a sintetizadores virtuales.\n"
                                                       u8"La aplicación usará la ruta digital sin compensación de conversión DAC/ADC.");
            btnVerifyDigital.setButtonText(juce::String::fromUTF8(u8"Verificar Latencia Digital"));
            btnVerifyDigital.setEnabled(true);
        }
    }
    else
    {
        btnVerifyDigital.setVisible(false);
        if (currentState == State::ReadyToMeasure)
        {
            btnStartMeasure.setVisible(true);
            btnSkip.setVisible(true);
        }
    }
    repaint();
}

void NativeCalibrationPanel::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    // Fondo del panel central
    g.fillAll(SoundIdTheme::bgLight);

    // Tarjeta central principal
    float maxCardW = juce::jmin(840.0f, area.getWidth() - 40.0f);
    float maxCardH = juce::jmin(540.0f, area.getHeight() - 30.0f);
    auto cardBounds = juce::Rectangle<float>((area.getWidth() - maxCardW) * 0.5f,
                                            (area.getHeight() - maxCardH) * 0.5f,
                                            maxCardW, maxCardH);

    g.setColour(SoundIdTheme::bgCard);
    g.fillRoundedRectangle(cardBounds, 12.0f);

    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(cardBounds.reduced(0.5f), 12.0f, 1.0f);

    auto content = cardBounds.reduced(28.0f, 24.0f);

    // 1. Header Row
    auto headerRow = content.removeFromTop(32.0f);
    g.setFont(juce::FontOptions("Inter", 18.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText(juce::String::fromUTF8(u8"2. Calibración de Interfaz de Audio"), headerRow.removeFromLeft(460.0f), juce::Justification::centredLeft, true);

    // Estado Badge
    auto badgeRect = headerRow.removeFromRight(210.0f).reduced(0.0f, 3.0f);
    if (isDigitalMode_)
    {
        if (isDigitalVerified_)
        {
            g.setColour(juce::Colour(0xffd1fae5));
            g.fillRoundedRectangle(badgeRect, 6.0f);
            g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::bold));
            g.setColour(juce::Colour(0xff065f46));
            g.drawText(juce::String::fromUTF8(u8"● MODO DIGITAL ACTIVO"), badgeRect, juce::Justification::centred, true);
        }
        else
        {
            g.setColour(juce::Colour(0xffe0e7ff));
            g.fillRoundedRectangle(badgeRect, 6.0f);
            g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::bold));
            g.setColour(juce::Colour(0xff3730a3));
            g.drawText(juce::String::fromUTF8(u8"⟳ VERIFICACIÓN PENDIENTE"), badgeRect, juce::Justification::centred, true);
        }
    }
    else if (currentState == State::Success)
    {
        g.setColour(juce::Colour(0xffd1fae5));
        g.fillRoundedRectangle(badgeRect, 6.0f);
        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::bold));
        g.setColour(juce::Colour(0xff065f46));
        g.drawText(juce::String::fromUTF8(u8"● CALIBRACIÓN COMPLETADA"), badgeRect, juce::Justification::centred, true);
    }
    else if (currentState == State::Skipped)
    {
        g.setColour(juce::Colour(0xfffef3c7));
        g.fillRoundedRectangle(badgeRect, 6.0f);
        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::bold));
        g.setColour(juce::Colour(0xff92400e));
        g.drawText(juce::String::fromUTF8(u8"⏭ CALIBRACIÓN OMITIDA"), badgeRect, juce::Justification::centred, true);
    }
    else if (currentState == State::Measuring)
    {
        g.setColour(juce::Colour(0xffe0e7ff));
        g.fillRoundedRectangle(badgeRect, 6.0f);
        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::bold));
        g.setColour(juce::Colour(0xff3730a3));
        g.drawText(juce::String::fromUTF8(u8"⟳ MIDIENDO..."), badgeRect, juce::Justification::centred, true);
    }
    else if (currentState == State::Failed)
    {
        g.setColour(juce::Colour(0xfffee2e2));
        g.fillRoundedRectangle(badgeRect, 6.0f);
        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentRed);
        g.drawText(juce::String::fromUTF8(u8"✕ REVISAR RETORNO"), badgeRect, juce::Justification::centred, true);
    }
    else
    {
        g.setColour(SoundIdTheme::bgCardHover);
        g.fillRoundedRectangle(badgeRect, 6.0f);
        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::bold));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText(juce::String::fromUTF8(u8"CALIBRACIÓN PENDIENTE"), badgeRect, juce::Justification::centred, true);
    }

    content.removeFromTop(10.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.fillRect(content.removeFromTop(1.0f));
    content.removeFromTop(10.0f);

    // Subtítulo explicativo
    g.setFont(juce::FontOptions("Inter", 12.5f, juce::Font::bold));
    g.setColour(SoundIdTheme::textPrimary);
    g.drawText(juce::String::fromUTF8(u8"Comprueba la latencia y el nivel de tu interfaz de audio antes de medir el instrumento."),
               content.removeFromTop(18.0f), juce::Justification::topLeft, true);

    content.removeFromTop(2.0f);
    g.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::plain));
    g.setColour(SoundIdTheme::textSecondary);
    g.drawText(juce::String::fromUTF8(u8"Esta calibración mide la ruta de audio de tu interfaz, no el sintetizador que vas a perfilar.\n"
                                      u8"Conecta la salida de calibración indicada a la entrada de retorno indicada mediante un cable directo.\n"
                                      u8"La aplicación enviará una señal de prueba y medirá el retardo y el nivel del sistema de captura."),
               content.removeFromTop(44.0f), juce::Justification::topLeft, true);

    content.removeFromTop(10.0f);

    // 2 Columnas de contenido
    float leftW = content.getWidth() * 0.52f;
    auto leftCol = content.removeFromLeft(leftW);
    content.removeFromLeft(20.0f);
    auto rightCol = content;

    if (isDigitalMode_)
    {
        auto drawDigitalItem = [&](int num, const juce::String& title, const juce::String& desc) {
            auto stepRow = leftCol.removeFromTop(62.0f);
            auto circleBounds = stepRow.removeFromLeft(28.0f).withSizeKeepingCentre(24.0f, 24.0f);

            g.setColour(SoundIdTheme::accentBlue.withAlpha(0.15f));
            g.fillEllipse(circleBounds);
            g.setColour(SoundIdTheme::accentBlue);
            g.drawEllipse(circleBounds, 1.5f);

            g.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::bold));
            g.drawText(juce::String(num), circleBounds, juce::Justification::centred, false);

            stepRow.removeFromLeft(10.0f);
            g.setFont(juce::FontOptions("Inter", 12.0f, juce::Font::bold));
            g.setColour(SoundIdTheme::textPrimary);
            g.drawText(title, stepRow.removeFromTop(18.0f), juce::Justification::centredLeft, true);

            g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::plain));
            g.setColour(SoundIdTheme::textSecondary);
            g.drawText(desc, stepRow, juce::Justification::topLeft, true);

            leftCol.removeFromTop(6.0f);
        };

        drawDigitalItem(1, juce::String::fromUTF8(u8"1. Modo Digital Activo"),
                        juce::String::fromUTF8(u8"No necesitas conectar un cable de loopback. La calibración analógica de la interfaz no se aplica a plugins ni a sintetizadores virtuales."));
        drawDigitalItem(2, juce::String::fromUTF8(u8"2. Ruta Digital Directa"),
                        juce::String::fromUTF8(u8"La aplicación usará la ruta digital sin compensación de conversión DAC/ADC."));
        drawDigitalItem(3, juce::String::fromUTF8(u8"3. Verificación de Buffer"),
                        juce::String::fromUTF8(u8"Comprueba que el host y el plugin responden a la tasa de muestreo y tamaño de bloque."));

        // Columna derecha Digital
        g.setColour(SoundIdTheme::bgCardHover);
        g.fillRoundedRectangle(rightCol.withHeight(220.0f), 8.0f);
        g.setColour(SoundIdTheme::borderSubtle);
        g.drawRoundedRectangle(rightCol.withHeight(220.0f).reduced(0.5f), 8.0f, 1.0f);

        auto meterArea = rightCol.withHeight(220.0f).reduced(14.0f, 12.0f);
        g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::textMuted);
        g.drawText(juce::String::fromUTF8(u8"ESTADO DE RUTA DIGITAL"), meterArea.removeFromTop(16.0f), juce::Justification::centredLeft, true);
        meterArea.removeFromTop(8.0f);

        g.setFont(juce::FontOptions("Inter", 12.0f, juce::Font::bold));
        g.setColour(isDigitalVerified_ ? SoundIdTheme::accentGreen : SoundIdTheme::accentAmber);
        g.drawText(isDigitalVerified_ ? juce::String::fromUTF8(u8"✓ Ruta digital verificada") : juce::String::fromUTF8(u8"⟳ Pendiente de verificación digital"),
                   meterArea.removeFromTop(20.0f), juce::Justification::centredLeft, true);

        meterArea.removeFromTop(8.0f);
        g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::plain));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText(juce::String::fromUTF8(u8"Modo: Digital (Plugin VST3 / Sintetizador Virtual)\n"
                                          u8"Latencia analógica: 0 ms\n"
                                          u8"Nivel nominal: 0 dBFS\n"
                                          u8"Conversión DAC/ADC: No requerida"),
                   meterArea, juce::Justification::topLeft, true);
        return;
    }

    // --- Columna Izquierda: 3 Pasos del protocolo ---
    // Paso 1: Conecta el cable de loopback
    {
        auto stepRow = leftCol.removeFromTop(74.0f);
        auto circleBounds = stepRow.removeFromLeft(28.0f).withSizeKeepingCentre(24.0f, 24.0f);

        g.setColour(SoundIdTheme::accentGreen.withAlpha(0.15f));
        g.fillEllipse(circleBounds);
        g.setColour(SoundIdTheme::accentGreen);
        g.drawEllipse(circleBounds, 1.5f);

        g.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::bold));
        g.drawText("1", circleBounds, juce::Justification::centred, false);

        stepRow.removeFromLeft(10.0f);
        g.setFont(juce::FontOptions("Inter", 12.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::textPrimary);
        g.drawText(juce::String::fromUTF8(u8"1. Conecta el cable de loopback"), stepRow.removeFromTop(18.0f), juce::Justification::centredLeft, true);

        auto channelBox = stepRow.removeFromTop(18.0f);
        g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentBlue);
        g.drawText(juce::String::fromUTF8(u8"Salida de calibración: ") + calibrationOutputChannelName +
                   juce::String::fromUTF8(u8"  ➔  Entrada de retorno: ") + calibrationInputChannelName,
                   channelBox, juce::Justification::centredLeft, true);

        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::plain));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText(juce::String::fromUTF8(u8"Usa un cable directo de nivel de línea. No conectes todavía el sintetizador que quieres medir."),
                   stepRow, juce::Justification::topLeft, true);

        leftCol.removeFromTop(6.0f);
    }

    // Paso 2: Comprueba el nivel de retorno
    {
        auto stepRow = leftCol.removeFromTop(60.0f);
        auto circleBounds = stepRow.removeFromLeft(28.0f).withSizeKeepingCentre(24.0f, 24.0f);

        g.setColour(SoundIdTheme::accentGreen.withAlpha(0.15f));
        g.fillEllipse(circleBounds);
        g.setColour(SoundIdTheme::accentGreen);
        g.drawEllipse(circleBounds, 1.5f);

        g.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::bold));
        g.drawText("2", circleBounds, juce::Justification::centred, false);

        stepRow.removeFromLeft(10.0f);
        g.setFont(juce::FontOptions("Inter", 12.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::textPrimary);
        g.drawText(juce::String::fromUTF8(u8"2. Comprueba el nivel de retorno"), stepRow.removeFromTop(18.0f), juce::Justification::centredLeft, true);

        g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::plain));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText(juce::String::fromUTF8(u8"Asegúrate de que la señal no esté silenciada ni saturada. La aplicación verificará el nivel y aplicará la compensación necesaria."),
                   stepRow, juce::Justification::topLeft, true);

        leftCol.removeFromTop(6.0f);
    }

    // Paso 3: Ejecuta la calibración
    {
        auto stepRow = leftCol.removeFromTop(60.0f);
        auto circleBounds = stepRow.removeFromLeft(28.0f).withSizeKeepingCentre(24.0f, 24.0f);

        g.setColour(SoundIdTheme::accentGreen.withAlpha(0.15f));
        g.fillEllipse(circleBounds);
        g.setColour(SoundIdTheme::accentGreen);
        g.drawEllipse(circleBounds, 1.5f);

        g.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::bold));
        g.drawText("3", circleBounds, juce::Justification::centred, false);

        stepRow.removeFromLeft(10.0f);
        g.setFont(juce::FontOptions("Inter", 12.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::textPrimary);
        g.drawText(juce::String::fromUTF8(u8"3. Ejecuta la calibración"), stepRow.removeFromTop(18.0f), juce::Justification::centredLeft, true);

        g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::plain));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText(juce::String::fromUTF8(u8"La aplicación reproducirá un barrido breve y calculará la latencia y el ajuste de nivel de esta ruta de audio."),
                   stepRow, juce::Justification::topLeft, true);
    }

    // --- Columna Derecha: Monitor Balístico de Nivel & Métricas ---
    g.setColour(SoundIdTheme::bgCardHover);
    g.fillRoundedRectangle(rightCol.withHeight(220.0f), 8.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.drawRoundedRectangle(rightCol.withHeight(220.0f).reduced(0.5f), 8.0f, 1.0f);

    auto meterArea = rightCol.withHeight(220.0f).reduced(14.0f, 12.0f);

    g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
    g.setColour(SoundIdTheme::textMuted);
    g.drawText(juce::String::fromUTF8(u8"MONITOR DE SEÑAL EN TIEMPO REAL"), meterArea.removeFromTop(16.0f), juce::Justification::centredLeft, true);
    meterArea.removeFromTop(6.0f);

    // Vúmetro horizontal
    auto vumeterBar = meterArea.removeFromTop(14.0f);
    g.setColour(SoundIdTheme::borderCard);
    g.fillRoundedRectangle(vumeterBar, 4.0f);

    float peakNorm = juce::jlimit(0.0f, 1.0f, liveInputPeak);
    auto fillBar = vumeterBar.withWidth(vumeterBar.getWidth() * peakNorm);

    if (peakNorm > 0.98f)
        g.setColour(SoundIdTheme::accentRed);
    else if (peakNorm > 0.60f)
        g.setColour(SoundIdTheme::accentGreen);
    else
        g.setColour(SoundIdTheme::accentGreen.withAlpha(0.6f));

    g.fillRoundedRectangle(fillBar, 4.0f);

    meterArea.removeFromTop(4.0f);
    float liveDb = 20.0f * std::log10(std::max(liveInputPeak, 1e-4f));
    juce::String dbText = (liveDb < -70.0f) ? "-inf dBFS" : juce::String(liveDb, 1) + " dBFS";
    g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::plain));
    g.setColour(SoundIdTheme::textSecondary);
    g.drawText(juce::String::fromUTF8(u8"Nivel de entrada: ") + dbText, meterArea.removeFromTop(15.0f), juce::Justification::centredLeft, true);

    meterArea.removeFromTop(6.0f);
    g.setColour(SoundIdTheme::borderSubtle);
    g.fillRect(meterArea.removeFromTop(1.0f));
    meterArea.removeFromTop(6.0f);

    // Métricas y mensajes de calibración
    if (currentState == State::Success)
    {
        g.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentGreen);
        g.drawText(juce::String::fromUTF8(u8"Calibración completada"), meterArea.removeFromTop(16.0f), juce::Justification::centredLeft, true);

        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::plain));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText(juce::String::fromUTF8(u8"La ruta de audio está lista para medir."), meterArea.removeFromTop(14.0f), juce::Justification::centredLeft, true);

        float trimDb = 20.0f * std::log10(std::max(calibrationData.recommendedTrimGain, 1e-4f));
        juce::String sign = (trimDb >= 0.0f) ? "+" : "";

        g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::plain));
        g.setColour(SoundIdTheme::textPrimary);
        g.drawText(juce::String::fromUTF8(u8"Latencia de ida y vuelta: ") + juce::String(calibrationData.roundTripLatencyMs, 2) + " ms (" +
                   juce::String(calibrationData.latencySamples) + " samples)",
                   meterArea.removeFromTop(15.0f), juce::Justification::centredLeft, true);

        g.drawText(juce::String::fromUTF8(u8"Ajuste de nivel: ") + sign + juce::String(trimDb, 2) + " dB",
                   meterArea.removeFromTop(15.0f), juce::Justification::centredLeft, true);

        juce::String polarityStr = calibrationData.phaseInversionDetected ? juce::String::fromUTF8(u8"Invertida (\u26A0)") : juce::String::fromUTF8(u8"Correcta");
        g.drawText(juce::String::fromUTF8(u8"Polaridad: ") + polarityStr,
                   meterArea.removeFromTop(15.0f), juce::Justification::centredLeft, true);

        if (calibrationData.clippingDetected)
        {
            g.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::bold));
            g.setColour(SoundIdTheme::accentAmber);
            g.drawText(juce::String::fromUTF8(u8"Aviso: Se detectó saturación durante la calibración."),
                       meterArea.removeFromTop(14.0f), juce::Justification::centredLeft, true);
        }

        if (!saveFeedbackText_.isEmpty())
        {
            g.setFont(juce::FontOptions("Inter", 9.5f, juce::Font::bold));
            g.setColour(SoundIdTheme::accentGreen);
            g.drawText(saveFeedbackText_, meterArea.removeFromTop(28.0f), juce::Justification::topLeft, true);
        }
    }
    else if (currentState == State::Failed)
    {
        if (calibrationData.clippingDetected)
        {
            g.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::bold));
            g.setColour(SoundIdTheme::accentRed);
            g.drawText(juce::String::fromUTF8(u8"La señal de retorno está saturando."), meterArea.removeFromTop(18.0f), juce::Justification::centredLeft, true);

            g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::plain));
            g.setColour(SoundIdTheme::textSecondary);
            g.drawText(juce::String::fromUTF8(u8"Reduce la ganancia de entrada o el nivel de salida y vuelve a ejecutar la calibración."),
                       meterArea, juce::Justification::topLeft, true);
        }
        else
        {
            g.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::bold));
            g.setColour(SoundIdTheme::accentRed);
            g.drawText(juce::String::fromUTF8(u8"No se detecta una señal de retorno suficiente."), meterArea.removeFromTop(18.0f), juce::Justification::centredLeft, true);

            g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::plain));
            g.setColour(SoundIdTheme::textSecondary);
            g.drawText(juce::String::fromUTF8(u8"Comprueba que el cable conecta la salida indicada con la entrada indicada y que la entrada no está silenciada."),
                       meterArea, juce::Justification::topLeft, true);
        }
    }
    else if (currentState == State::Skipped)
    {
        g.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentAmber);
        g.drawText(juce::String::fromUTF8(u8"Calibración omitida"), meterArea.removeFromTop(18.0f), juce::Justification::centredLeft, true);

        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::plain));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText(juce::String::fromUTF8(u8"Modo: Omitido (sin compensación de latencia)\n"
                                          u8"Ajuste de ganancia: 0.0 dB nominal\n"
                                          u8"Latencia asumida: 0 muestras (0.0 ms)"),
                   meterArea, juce::Justification::topLeft, true);
    }
    else if (currentState == State::Measuring)
    {
        g.setFont(juce::FontOptions("Inter", 11.5f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentBlue);
        g.drawText(juce::String::fromUTF8(u8"Midiendo respuesta de interfaz..."), meterArea.removeFromTop(18.0f), juce::Justification::centredLeft, true);

        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::plain));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText(juce::String::fromUTF8(u8"Reproduciendo barrido logarítmico Farina.\n"
                                          u8"Calculando latencia, auto-trim y polaridad..."),
                   meterArea, juce::Justification::topLeft, true);
    }
    else if (activeAlignment == calibration::ActiveCalibrationAlignment::Misaligned)
    {
        g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentAmber);
        g.drawText(juce::String::fromUTF8(u8"La configuración cambió desde la última calibración"), meterArea.removeFromTop(18.0f), juce::Justification::centredLeft, true);

        g.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::plain));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText(juce::String::fromUTF8(u8"La compensación de latencia y nivel anterior no está activa.\n"
                                          u8"Por favor, recalibra o continúa sin calibrar (Bypass)."),
                   meterArea, juce::Justification::topLeft, true);
    }
    else if (matchingProfile_.has_value() && matchEvaluation_.isActionableMatch)
    {
        g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentGreen);
        g.drawText(juce::String::fromUTF8(u8"Calibración compatible encontrada"), meterArea.removeFromTop(16.0f), juce::Justification::centredLeft, true);

        g.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::plain));
        g.setColour(SoundIdTheme::textPrimary);
        juce::String devStr = juce::String(matchingProfile_->deviceSnapshot.deviceName) + " \u00B7 " +
                              juce::String(matchingProfile_->deviceSnapshot.sampleRate / 1000.0, 1) + " kHz \u00B7 Buffer " +
                              juce::String(matchingProfile_->deviceSnapshot.bufferSizeSamples);
        g.drawText(devStr, meterArea.removeFromTop(15.0f), juce::Justification::centredLeft, true);

        g.setFont(juce::FontOptions("Inter", 9.0f, juce::Font::plain));
        g.setColour(SoundIdTheme::accentAmber);
        g.drawText(juce::String::fromUTF8(u8"Aviso: La interfaz y la configuración actual coinciden con los datos guardados.\n"
                                          u8"No se pueden detectar cambios físicos en cables, ganancia analógica o una segunda unidad idéntica."),
                   meterArea, juce::Justification::topLeft, true);
    }
    else if (!savedProfiles.empty() && matchEvaluation_.status == calibration::CalibrationMatchStatus::ConfigurationMismatch)
    {
        g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::accentAmber);
        g.drawText(juce::String::fromUTF8(u8"Configuración distinta a la guardada"), meterArea.removeFromTop(16.0f), juce::Justification::centredLeft, true);

        g.setFont(juce::FontOptions("Inter", 9.5f, juce::Font::plain));
        g.setColour(SoundIdTheme::textSecondary);
        juce::String diffMsg = juce::String::fromUTF8(u8"La calibración guardada no coincide con la configuración actual.\n");
        for (const auto& d : matchEvaluation_.differences)
        {
            diffMsg += juce::String(d.fieldName) + ": " + juce::String(d.profileValue) + " \u2192 " + juce::String(d.currentValue) + "\n";
        }
        g.drawText(diffMsg, meterArea, juce::Justification::topLeft, true);
    }
    else
    {
        g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::textPrimary);
        g.drawText(juce::String::fromUTF8(u8"A la espera de calibración"), meterArea.removeFromTop(18.0f), juce::Justification::centredLeft, true);

        g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::plain));
        g.setColour(SoundIdTheme::textSecondary);
        g.drawText(juce::String::fromUTF8(u8"Comprueba el cable y pulsa 'Iniciar Calibración Loopback'.\n"
                                          u8"La aplicación comprobará el nivel y calculará la compensación necesaria."),
                   meterArea, juce::Justification::topLeft, true);
    }

    // Sección compacta de perfiles guardados
    if (showSavedProfilesSection_)
    {
        auto savedArea = rightCol;
        savedArea.removeFromTop(226.0f);
        auto savedBox = savedArea.removeFromTop(juce::jmin(140.0f, savedArea.getHeight() - 65.0f));

        g.setColour(SoundIdTheme::bgCardHover);
        g.fillRoundedRectangle(savedBox, 8.0f);
        g.setColour(SoundIdTheme::borderSubtle);
        g.drawRoundedRectangle(savedBox.reduced(0.5f), 8.0f, 1.0f);

        auto inner = savedBox.reduced(10.0f, 8.0f);
        auto titleRow = inner.removeFromTop(16.0f);
        g.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::bold));
        g.setColour(SoundIdTheme::textMuted);
        g.drawText(juce::String::fromUTF8(u8"CALIBRACIONES GUARDADAS EN APPDATA"), titleRow, juce::Justification::centredLeft, true);

        inner.removeFromTop(4.0f);

        if (savedProfiles.empty())
        {
            g.setFont(juce::FontOptions("Inter", 10.5f, juce::Font::plain));
            g.setColour(SoundIdTheme::textSecondary);
            g.drawText(juce::String::fromUTF8(u8"No hay calibraciones guardadas en disco aún."), inner, juce::Justification::centredLeft, true);
        }
        else
        {
            const auto& p = savedProfiles[static_cast<size_t>(selectedProfileIndex_)];

            g.setFont(juce::FontOptions("Inter", 11.0f, juce::Font::bold));
            g.setColour(SoundIdTheme::textPrimary);
            g.drawText(juce::String(p.deviceSnapshot.deviceName) + " (" + juce::String(p.deviceSnapshot.driverType) + ")",
                       inner.removeFromTop(15.0f), juce::Justification::centredLeft, true);

            g.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::plain));
            g.setColour(SoundIdTheme::textSecondary);
            juce::String srKhz = juce::String(p.deviceSnapshot.sampleRate / 1000.0, 1) + " kHz";
            juce::String bufSpl = "Buffer " + juce::String(p.deviceSnapshot.bufferSizeSamples);
            juce::String routeStr = juce::String(p.routingSnapshot.outputChannelLabel) + " \u2794 " + juce::String(p.routingSnapshot.inputChannelLabel);
            g.drawText(srKhz + " \u00B7 " + bufSpl + " \u00B7 " + routeStr,
                       inner.removeFromTop(14.0f), juce::Justification::centredLeft, true);

            g.setFont(juce::FontOptions("Inter", 9.5f, juce::Font::plain));
            g.setColour(SoundIdTheme::textMuted);
            g.drawText(juce::String::fromUTF8(u8"Fecha: ") + juce::String(p.createdAt),
                       inner.removeFromTop(13.0f), juce::Justification::centredLeft, true);

            if (showProfileDetails_)
            {
                g.setFont(juce::FontOptions("Inter", 9.5f, juce::Font::bold));
                g.setColour(SoundIdTheme::accentGreen);
                g.drawText("Lat: " + juce::String(p.calibrationResult.roundTripLatencyMs, 1) + " ms (" +
                           juce::String(p.calibrationResult.latencySamples) + " spls) \u00B7 SNR: " +
                           juce::String(p.calibrationResult.snrDb, 1) + " dB \u00B7 Flat: " +
                           juce::String(p.calibrationResult.frequencyFlatnessDb, 1) + " dB",
                           inner.removeFromTop(14.0f), juce::Justification::centredLeft, true);
            }
        }
    }
}

void NativeCalibrationPanel::resized()
{
    auto area = getLocalBounds();
    int maxCardW = juce::jmin(840, area.getWidth() - 40);
    int maxCardH = juce::jmin(540, area.getHeight() - 30);
    auto cardBounds = juce::Rectangle<int>((area.getWidth() - maxCardW) / 2,
                                          (area.getHeight() - maxCardH) / 2,
                                          maxCardW, maxCardH);

    int bottomY = cardBounds.getBottom() - 56;
    int leftX = cardBounds.getX() + 28;
    int cardRight = cardBounds.getRight() - 28;

    progressBar.setBounds(leftX, bottomY - 24, cardRight - leftX, 12);

    if (isDigitalMode_)
    {
        btnVerifyDigital.setBounds(leftX, bottomY, 240, 36);
        btnContinue.setBounds(cardRight - 280, bottomY, 280, 36);
        btnToggleSavedProfiles.setVisible(false);
        btnSaveCalibration.setVisible(false);
        btnDeleteProfile.setVisible(false);
        btnViewProfileDetails.setVisible(false);
    }
    else
    {
        btnToggleSavedProfiles.setVisible(true);

        if (currentState == State::Success)
        {
            btnRetry.setBounds(leftX, bottomY, 130, 36);
            btnToggleSavedProfiles.setBounds(leftX + 138, bottomY, 190, 36);

            btnSaveCalibration.setBounds(cardRight - 420, bottomY, 170, 36);
            btnContinue.setBounds(cardRight - 240, bottomY, 240, 36);
        }
        else
        {
            btnSaveCalibration.setVisible(false);

            if (currentState == State::ReadyToMeasure)
            {
                if (matchingProfile_.has_value() && matchEvaluation_.isActionableMatch)
                {
                    btnReuseCalibration.setVisible(true);
                    btnSkip.setBounds(leftX, bottomY, 190, 36);
                    btnReuseCalibration.setBounds(leftX + 196, bottomY, 220, 36);
                    btnToggleSavedProfiles.setBounds(leftX + 422, bottomY, 170, 36);
                    btnStartMeasure.setBounds(cardRight - 210, bottomY, 210, 36);
                }
                else
                {
                    btnReuseCalibration.setVisible(false);
                    btnSkip.setBounds(leftX, bottomY, 200, 36);
                    btnToggleSavedProfiles.setBounds(leftX + 208, bottomY, 190, 36);
                    btnStartMeasure.setBounds(cardRight - 280, bottomY, 280, 36);
                }
            }
            else
            {
                btnReuseCalibration.setVisible(false);
                btnRetry.setBounds(leftX, bottomY, 160, 36);
                btnSkip.setBounds(leftX + 168, bottomY, 180, 36);
                btnToggleSavedProfiles.setBounds(leftX + 356, bottomY, 190, 36);
                btnContinue.setBounds(cardRight - 280, bottomY, 280, 36);
            }
        }

        // Botones dentro de la sección de perfiles guardados
        if (showSavedProfilesSection_ && !savedProfiles.empty())
        {
            int sectionX = cardBounds.getX() + static_cast<int>(maxCardW * 0.52f) + 48;
            int sectionW = cardRight - sectionX;
            int sectionBottom = cardBounds.getY() + 24 + 32 + 44 + 10 + 226 + 140;

            btnViewProfileDetails.setVisible(true);
            btnDeleteProfile.setVisible(true);
            btnViewProfileDetails.setBounds(sectionX + sectionW - 180, sectionBottom - 30, 95, 24);
            btnDeleteProfile.setBounds(sectionX + sectionW - 80, sectionBottom - 30, 75, 24);
        }
        else
        {
            btnViewProfileDetails.setVisible(false);
            btnDeleteProfile.setVisible(false);
        }
    }
}

} // namespace abdaudiolab::gui
