#include "SynthTargetLifecycleAdapters.h"
#include <thread>
#include <chrono>

namespace abdaudiolab::synth
{

// ==============================================================================
// SyntheticTargetLifecycleAdapter
// ==============================================================================

SyntheticTargetLifecycleAdapter::SyntheticTargetLifecycleAdapter(double defaultSampleRate, uint32_t seed)
    : fixture_(defaultSampleRate, seed),
      synthTarget_(fixture_),
      sampleRate_(defaultSampleRate)
{
}

bool SyntheticTargetLifecycleAdapter::initializeTarget(const gui::session::TargetSelectionState& targetState,
                                                       double sampleRate,
                                                       int blockSize,
                                                       std::string& /*outErrorMessage*/)
{
    sampleRate_ = sampleRate;
    blockSize_ = blockSize;

    fixture_.setSampleRate(sampleRate);
    synthTarget_.prepare({ sampleRate, blockSize, 2 });
    synthTarget_.resetState();

    fingerprint_.pluginPath = "internal://SyntheticSynthFixture_V1";
    fingerprint_.pluginFormatVersion = "SyntheticEngine 1.0.0";
    fingerprint_.vendor = targetState.manufacturer.empty() ? "ABDAudioLab" : targetState.manufacturer;
    fingerprint_.pluginUid = targetState.targetId.empty() ? "synthetic_fixture_demo" : targetState.targetId;
    fingerprint_.fileSizeBytes = sizeof(SyntheticSynthFixture);
    fingerprint_.binarySha256 = Sha256::computeHex("SyntheticSynthFixture_V1_Internal_Binary");
    fingerprint_.buildConfiguration = "Internal-DSP";
    fingerprint_.hostSampleRate = sampleRate;
    fingerprint_.hostBlockSize = blockSize;
    fingerprint_.osArchitecture = (sizeof(void*) == 8) ? "x86_64" : "x86";
    fingerprint_.normalizedFingerprint = fingerprint_.computeNormalizedFingerprint();

    isReady_ = true;
    return true;
}

ISynthTarget* SyntheticTargetLifecycleAdapter::getTarget() noexcept
{
    return isReady_ ? &synthTarget_ : nullptr;
}

const ISynthTarget* SyntheticTargetLifecycleAdapter::getTarget() const noexcept
{
    return isReady_ ? &synthTarget_ : nullptr;
}

void SyntheticTargetLifecycleAdapter::resetForTrial()
{
    if (isReady_)
    {
        synthTarget_.resetState();
    }
}

void SyntheticTargetLifecycleAdapter::releaseTarget()
{
    isReady_ = false;
}

bool SyntheticTargetLifecycleAdapter::isReady() const noexcept
{
    return isReady_;
}

EvaluationOrigin SyntheticTargetLifecycleAdapter::getEvaluationOrigin() const noexcept
{
    return EvaluationOrigin::SyntheticDemo;
}

std::string SyntheticTargetLifecycleAdapter::getExecutionMode() const
{
    return "DemoMode";
}

TargetFingerprint SyntheticTargetLifecycleAdapter::getFingerprint() const
{
    return fingerprint_;
}

// ==============================================================================
// InProcessVst3LifecycleAdapter
// ==============================================================================

InProcessVst3LifecycleAdapter::InProcessVst3LifecycleAdapter(double watchdogMaxBlockMs)
    : watchdogMaxBlockMs_(watchdogMaxBlockMs),
      creationThreadId_(std::this_thread::get_id())
{
    formatManager_.addDefaultFormats();
}

InProcessVst3LifecycleAdapter::~InProcessVst3LifecycleAdapter()
{
    releaseTarget();
}

juce::File InProcessVst3LifecycleAdapter::resolveVst3File(const gui::session::TargetSelectionState& targetState)
{
    // 1. Variable de entorno explícita para CI o rutas personalizadas
    auto envPath = juce::SystemStats::getEnvironmentVariable("REFERENCE_SYNTH_VST3_PATH", "");
    if (envPath.isNotEmpty())
    {
        juce::File f(envPath);
        if (f.exists())
            return f;
    }

    auto genPath = juce::SystemStats::getEnvironmentVariable("ABDAUDIOLAB_VST3_PATH", "");
    if (genPath.isNotEmpty())
    {
        juce::File f(genPath);
        if (f.exists())
            return f;
    }

    // 2. Si targetId contiene una ruta con extensión o separadores de ruta, respetarla directamente
    if (!targetState.targetId.empty())
    {
        bool looksLikePath = targetState.targetId.find('/') != std::string::npos
                          || targetState.targetId.find('\\') != std::string::npos
                          || targetState.targetId.find(".vst3") != std::string::npos;

        juce::File direct(targetState.targetId);
        if (direct.exists() || looksLikePath)
            return direct;
    }

    juce::String tid(targetState.targetId);
    bool isReference = tid.isEmpty() || tid.containsIgnoreCase("reference");
    bool isDexed = tid.containsIgnoreCase("dexed");

    if (isDexed)
    {
        juce::File dexedWin("C:\\Program Files\\Common Files\\VST3\\Dexed.vst3");
        return dexedWin;
    }

    bool isDemo = tid.containsIgnoreCase("demosynth");
    if (isDemo)
    {
        juce::File demoWin("C:\\Program Files\\Common Files\\VST3\\DemoSynth.vst3");
        if (demoWin.exists())
            return demoWin;
        juce::File demoBuild("D:\\desarrollos\\ABDSynths\\_RESOURCES\\DemoSynthPlugin-main\\build\\DemoSynth_artefacts\\Release\\VST3\\DemoSynth.vst3");
        if (demoBuild.exists())
            return demoBuild;
    }

    // Comprobar si el plugin existe por nombre en C:\Program Files\Common Files\VST3\<name>.vst3
    juce::File commonDir("C:\\Program Files\\Common Files\\VST3");
    juce::File commonNamed = commonDir.getChildFile(tid.endsWithIgnoreCase(".vst3") ? tid : (tid + ".vst3"));
    if (commonNamed.exists())
        return commonNamed;

    if (isReference)
    {
        juce::File exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();

        // 1. En el mismo directorio del ejecutable actual (distribución portable / artefacts)
        juce::File siblingVst3 = exeDir.getChildFile("ReferenceSynth.vst3");
        if (siblingVst3.exists())
            return siblingVst3;

        // 2. Subiendo 2 niveles hasta build/ReferenceSynth_artefacts
        juce::File buildVst3Rel2 = exeDir.getParentDirectory().getParentDirectory()
            .getChildFile("ReferenceSynth_artefacts/Release/VST3/ReferenceSynth.vst3");
        if (buildVst3Rel2.exists())
            return buildVst3Rel2;

        // 3. Subiendo 1 nivel por si la jerarquía es build/Release
        juce::File exeRelative = exeDir.getChildFile("../ReferenceSynth_artefacts/Release/VST3/ReferenceSynth.vst3");
        if (exeRelative.exists())
            return exeRelative;

        // 4. Artefacto construido localmente por CMake (Release) desde CWD
        juce::File buildVst3 = juce::File::getCurrentWorkingDirectory()
            .getChildFile("build/ReferenceSynth_artefacts/Release/VST3/ReferenceSynth.vst3");
        if (buildVst3.exists())
            return buildVst3;

        juce::File buildVst3Lab = juce::File::getCurrentWorkingDirectory()
            .getChildFile("build/ABDAudioLab_artefacts/Release/ReferenceSynth.vst3");
        if (buildVst3Lab.exists())
            return buildVst3Lab;

        juce::File exeLab = exeDir.getParentDirectory()
            .getChildFile("ABDAudioLab_artefacts/Release/ReferenceSynth.vst3");
        if (exeLab.exists())
            return exeLab;

        // 5. Raíz del proyecto si CWD es subdirectorio
        juce::File projectRoot = juce::File::getCurrentWorkingDirectory()
            .getChildFile("ReferenceSynth_artefacts/Release/VST3/ReferenceSynth.vst3");
        if (projectRoot.exists())
            return projectRoot;

        // 6. Ubicación estándar de plugins del sistema
        juce::File winCommon("C:\\Program Files\\Common Files\\VST3\\ReferenceSynth.vst3");
        if (winCommon.exists())
            return winCommon;

        return siblingVst3;
    }

    // Para cualquier otro targetId no reconocido, retornar un archivo con ese nombre (no existirá)
    return juce::File(targetState.targetId);
}

bool InProcessVst3LifecycleAdapter::requiresUnblockedMessageThread() const noexcept
{
    if (const_cast<juce::AudioPluginFormatManager&>(formatManager_).getNumFormats() == 0)
    {
        const_cast<juce::AudioPluginFormatManager&>(formatManager_).addDefaultFormats();
    }

    for (int i = 0; i < formatManager_.getNumFormats(); ++i)
    {
        auto* fmt = formatManager_.getFormat(i);
        if (fmt != nullptr && fmt->getName().containsIgnoreCase("VST3"))
        {
            juce::PluginDescription dummyDesc;
            dummyDesc.pluginFormatName = "VST3";
            return fmt->requiresUnblockedMessageThreadDuringCreation(dummyDesc);
        }
    }
    return false;
}

void InProcessVst3LifecycleAdapter::setWatchdogMaxBlockDurationMs(double maxMs) noexcept
{
    watchdogMaxBlockMs_ = maxMs;
    if (fixture_ != nullptr)
    {
        fixture_->setWatchdogMaxBlockDurationMs(maxMs);
    }
}

void InProcessVst3LifecycleAdapter::markCorrupted(const std::string& reason) noexcept
{
    isCorrupted_ = true;
    isReady_ = false;
    corruptionReason_ = reason;
}

bool InProcessVst3LifecycleAdapter::initializeTarget(const gui::session::TargetSelectionState& targetState,
                                                    double sampleRate,
                                                    int blockSize,
                                                    std::string& outErrorMessage)
{
    if (isCorrupted_)
    {
        outErrorMessage = "InProcessVst3LifecycleAdapter is corrupted from previous fault: " + corruptionReason_;
        return false;
    }

    sampleRate_ = sampleRate;
    blockSize_ = blockSize;

    // 1. Localizar binario VST3
    juce::File pluginFile = resolveVst3File(targetState);
    if (!pluginFile.exists())
    {
        outErrorMessage = "Plugin binary does not exist on disk: " + pluginFile.getFullPathName().toStdString();
        isReady_ = false;
        return false;
    }

    // 2. Instanciación controlada fuera del hilo de audio
    try
    {
        if (formatManager_.getNumFormats() == 0)
        {
            formatManager_.addDefaultFormats();
        }

        fixture_ = std::make_unique<ExternalPluginFixture>(formatManager_);
        // Durante la inicialización no activamos watchdog ultrabajo para permitir el calentamiento/settling inicial
        fixture_->setWatchdogMaxBlockDurationMs(0.0);

        std::string loadErr;
        bool loaded = fixture_->loadPluginFromDisk(pluginFile, sampleRate, blockSize, loadErr);
        if (!loaded)
        {
            outErrorMessage = "Failed to load VST3 plugin: " + loadErr;
            fixture_.reset();
            isReady_ = false;
            return false;
        }

        // 3. Secuencia de preparación completa y warming antes del primer bloque de medición
        completePreparationSequence(sampleRate, blockSize);

        // 3b. Activar el watchdog para bloques de medición subsiguientes
        fixture_->setWatchdogMaxBlockDurationMs(watchdogMaxBlockMs_);

        // 4. Extraer huella metrológica completa del binario e integrarla en la procedencia
        const auto& id = fixture_->getIdentity();
        fingerprint_.pluginPath = id.absolutePath.empty() ? pluginFile.getFullPathName().toStdString() : id.absolutePath;
        fingerprint_.pluginFormatVersion = "VST 3.7.x (In-process VST3 test target)";
        fingerprint_.vendor = id.manufacturer.empty() ? "ABDSynths" : id.manufacturer;
        fingerprint_.pluginUid = id.pluginUid.empty() ? targetState.targetId : id.pluginUid;

        uint64_t totalBytes = 0;
        for (const auto& b : id.bundleFiles)
            totalBytes += b.fileSize;
        if (totalBytes == 0 && pluginFile.existsAsFile())
            totalBytes = static_cast<uint64_t>(pluginFile.getSize());

        fingerprint_.fileSizeBytes = totalBytes;
        fingerprint_.binarySha256 = id.binaryHash.empty() ? id.bundleHash : id.binaryHash;
        if (fingerprint_.binarySha256.empty())
        {
            fingerprint_.binarySha256 = Sha256::computeHex(fingerprint_.pluginPath);
        }
        fingerprint_.buildConfiguration = "Release-x64";
        fingerprint_.hostSampleRate = sampleRate;
        fingerprint_.hostBlockSize = blockSize;
        fingerprint_.osArchitecture = id.architecture.empty() ? "x86_64-windows" : id.architecture;
        fingerprint_.normalizedFingerprint = fingerprint_.computeNormalizedFingerprint();

        isReady_ = true;
        isCorrupted_ = false;
        corruptionReason_.clear();
        return true;
    }
    catch (const std::exception& e)
    {
        markCorrupted(e.what());
        outErrorMessage = "Exception during VST3 target initialization: " + std::string(e.what());
        fixture_.reset();
        return false;
    }
    catch (...)
    {
        markCorrupted("Unknown non-standard exception during initialization");
        outErrorMessage = "Fatal non-standard exception during VST3 target initialization";
        fixture_.reset();
        return false;
    }
}

void InProcessVst3LifecycleAdapter::completePreparationSequence(double sampleRate, int blockSize)
{
    if (fixture_ == nullptr)
        return;

    // a) Configuración de buses y prepareToPlay
    fixture_->prepare({ sampleRate, blockSize, 2 });

    // b) Activación de buses
    if (auto* inst = fixture_->getPluginInstance())
    {
        inst->enableAllBuses();
    }

    // c) Reset de voces y estado inicial
    fixture_->resetState();

    // d) Settling / Warmup: ejecutar 1 bloque silencioso de preparación para permitir
    // inicializaciones de TLS o cachés internas del plugin sin emitir telemetría de medición
    MidiExcitationSequence warmupSeq;
    warmupSeq.totalDurationSec = static_cast<double>(blockSize) / sampleRate;
    std::vector<float> warmupAudio;
    fixture_->render(warmupSeq, warmupAudio, 0);

    // e) Verificación de estado
    (void)fixture_->verifyState();
}

ISynthTarget* InProcessVst3LifecycleAdapter::getTarget() noexcept
{
    if (!isReady_ || isCorrupted_ || fixture_ == nullptr)
        return nullptr;
    return fixture_.get();
}

const ISynthTarget* InProcessVst3LifecycleAdapter::getTarget() const noexcept
{
    if (!isReady_ || isCorrupted_ || fixture_ == nullptr)
        return nullptr;
    return fixture_.get();
}

void InProcessVst3LifecycleAdapter::resetForTrial()
{
    if (isReady_ && !isCorrupted_ && fixture_ != nullptr)
    {
        fixture_->resetState();
    }
}

void InProcessVst3LifecycleAdapter::releaseTarget()
{
    isReady_ = false;
    if (fixture_ != nullptr)
    {
        fixture_->resetState();
        fixture_.reset(); // Libera instance_->releaseResources() y el descriptor
    }
}

bool InProcessVst3LifecycleAdapter::isReady() const noexcept
{
    return isReady_ && !isCorrupted_ && (fixture_ != nullptr);
}

EvaluationOrigin InProcessVst3LifecycleAdapter::getEvaluationOrigin() const noexcept
{
    return EvaluationOrigin::MeasuredExternalPlugin;
}

std::string InProcessVst3LifecycleAdapter::getExecutionMode() const
{
    return "InProcessVST3";
}

TargetFingerprint InProcessVst3LifecycleAdapter::getFingerprint() const
{
    return fingerprint_;
}

} // namespace abdaudiolab::synth
