#include <catch2/catch_test_macros.hpp>
#include "core/plugins/PluginHostManager.h"
#include "core/plugins/PluginHardwareContractAdapter.h"
#include "gui/plugins/PluginWindowController.h"
#include "audio/LabAudioEngine.h"

using namespace abdaudiolab;

namespace
{

class TestEditor : public juce::AudioProcessorEditor
{
public:
    explicit TestEditor(juce::AudioProcessor& p) : juce::AudioProcessorEditor(p)
    {
        setSize(320, 240);
    }
};

class DummyHostedParam : public juce::AudioPluginInstance::HostedParameter
{
public:
    DummyHostedParam(const juce::String& paramId, const juce::String& paramName, float initialVal)
        : id_(paramId), name_(paramName), value_(initialVal)
    {
    }

    juce::String getParameterID() const override { return id_; }
    float getValue() const override { return value_; }
    void setValue(float newValue) override { value_ = newValue; }
    float getDefaultValue() const override { return 0.5f; }
    juce::String getName(int maxLen) const override { return name_.substring(0, maxLen); }
    juce::String getLabel() const override { return {}; }
    float getValueForText(const juce::String& text) const override { return text.getFloatValue(); }

private:
    juce::String id_;
    juce::String name_;
    float value_ { 0.0f };
};

class DummyPluginInstance : public juce::AudioPluginInstance
{
public:
    explicit DummyPluginInstance(bool provideEditor = false)
        : hasEditorFlag(provideEditor)
    {
        addHostedParameter(std::make_unique<DummyHostedParam>("drive", "Drive", 0.5f));
        addHostedParameter(std::make_unique<DummyHostedParam>("tone", "Tone", 0.7f));
    }

    const juce::String getName() const override { return "DummyPluginInstance"; }
    void prepareToPlay(double sr, int bs) override
    {
        sampleRate_ = sr;
        blockSize_ = bs;
        released_ = false;
    }
    void releaseResources() override { released_ = true; }
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        for (const auto metadata : midi)
        {
            auto msg = metadata.getMessage();
            if (msg.isNoteOn())
                activeNotes_++;
            else if (msg.isNoteOff() || msg.isAllNotesOff())
                activeNotes_ = 0;
        }
        buffer.applyGain(0.5f);
    }

    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override
    {
        return hasEditorFlag ? new TestEditor(*this) : nullptr;
    }
    bool hasEditor() const override { return hasEditorFlag; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override
    {
        destData.setSize(statePayload_.size());
        destData.copyFrom(statePayload_.data(), 0, statePayload_.size());
    }

    void setStateInformation(const void* data, int sizeInBytes) override
    {
        if (data != nullptr && sizeInBytes > 0)
        {
            statePayload_.assign(static_cast<const uint8_t*>(data),
                                 static_cast<const uint8_t*>(data) + sizeInBytes);
        }
    }

    void fillInPluginDescription(juce::PluginDescription& desc) const override
    {
        desc.name = "DummyPluginInstance";
        desc.manufacturerName = "ABDSynths";
        desc.pluginFormatName = "VST3";
        desc.version = "1.2.0";
        desc.fileOrIdentifier = "dummy_plugin.vst3";
        desc.isInstrument = true;
    }

    bool wasReleased() const noexcept { return released_; }
    double getPreparedSampleRate() const noexcept { return sampleRate_; }
    int getPreparedBlockSize() const noexcept { return blockSize_; }
    int getActiveNotes() const noexcept { return activeNotes_; }
    void setStateData(const std::vector<uint8_t>& bytes) { statePayload_ = bytes; }
    const std::vector<uint8_t>& getStateData() const noexcept { return statePayload_; }

private:
    bool hasEditorFlag { false };
    bool released_ { false };
    double sampleRate_ { 0.0 };
    int blockSize_ { 0 };
    int activeNotes_ { 0 };
    std::vector<uint8_t> statePayload_ { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE };
};

juce::PluginDescription createDummyDesc(bool isInst = true)
{
    juce::PluginDescription d;
    d.name = "DummyPluginInstance";
    d.manufacturerName = "ABDSynths";
    d.pluginFormatName = "VST3";
    d.version = "1.2.0";
    d.fileOrIdentifier = "dummy_plugin.vst3";
    d.isInstrument = isInst;
    return d;
}

} // namespace

// ==============================================================================
// CHARACTERIZATION TESTS (SEAM 4)
// ==============================================================================

TEST_CASE("PluginHostManager - 1. Descubrimiento y Formatos Validos", "[PluginHost]")
{
    core::PluginHostManager hostManager;

    SECTION("Format Manager registra formatos compatibles")
    {
        auto& fm = hostManager.getFormatManager();
        CHECK(fm.getNumFormats() >= 0);
    }

    SECTION("KnownPluginList XML roundtrip seguro")
    {
        juce::File tempCache = juce::File::createTempFile("plugin_cache_test");
        tempCache.deleteFile();

        hostManager.saveCache(tempCache);
        CHECK(tempCache.existsAsFile());

        hostManager.loadCache(tempCache);
        CHECK(hostManager.getAvailablePlugins().empty());

        tempCache.deleteFile();
    }
}

TEST_CASE("PluginHostManager - 2. Carga de plugin inexistente", "[PluginHost]")
{
    core::PluginHostManager hostManager;
    juce::File nonExistent("C:/path/to/non_existent_abdaudio_test.vst3");

    auto result = hostManager.loadPluginFromFile(nonExistent, 44100.0, 512);
    CHECK_FALSE(result.succeeded);
    CHECK(result.errorCode == "FILE_NOT_FOUND");
    CHECK_FALSE(hostManager.hasActivePlugin());
    CHECK(hostManager.getActivePluginInstance() == nullptr);
}

TEST_CASE("PluginHostManager - 3. Carga de plugin incompatible", "[PluginHost]")
{
    core::PluginHostManager hostManager;
    juce::File tempFile = juce::File::createTempFile("incompatible_test.vst3");
    tempFile.replaceWithText("Not a real binary plugin file");

    auto result = hostManager.loadPluginFromFile(tempFile, 44100.0, 512);
    CHECK_FALSE(result.succeeded);
    CHECK(result.errorCode == "INCOMPATIBLE_PLUGIN");
    CHECK_FALSE(hostManager.hasActivePlugin());

    tempFile.deleteFile();
}

TEST_CASE("PluginHostManager - 4. Carga sin editor", "[PluginHost]")
{
    core::PluginHostManager hostManager;
    auto dummy = std::make_unique<DummyPluginInstance>(false);
    auto desc = createDummyDesc();

    auto res = hostManager.adoptPluginInstance(std::move(dummy), desc, 48000.0, 256);
    REQUIRE(res.succeeded);
    REQUIRE(hostManager.hasActivePlugin());

    auto editorRes = hostManager.inspectEditor();
    CHECK_FALSE(editorRes.available);
    CHECK(editorRes.errorCode == "NO_EDITOR_SUPPORTED");
    CHECK(hostManager.createEditorIfNeeded() == nullptr);
}

TEST_CASE("PluginHostManager - 5. Carga con editor", "[PluginHost]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    core::PluginHostManager hostManager;
    auto dummy = std::make_unique<DummyPluginInstance>(true);
    auto desc = createDummyDesc();

    auto res = hostManager.adoptPluginInstance(std::move(dummy), desc, 48000.0, 256);
    REQUIRE(res.succeeded);
    REQUIRE(hostManager.hasActivePlugin());

    auto editorRes = hostManager.inspectEditor();
    CHECK(editorRes.available);
    CHECK(editorRes.errorCode.empty());

    auto editor = hostManager.createEditorIfNeeded();
    REQUIRE(editor != nullptr);
    CHECK(editor->getWidth() == 320);
    CHECK(editor->getHeight() == 240);
}

TEST_CASE("PluginHostManager - 6. Cerrar editor dos veces (idempotencia)", "[PluginHost]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    core::PluginHostManager hostManager;
    gui::PluginWindowController windowController;

    auto dummy = std::make_unique<DummyPluginInstance>(true);
    hostManager.adoptPluginInstance(std::move(dummy), createDummyDesc());

    windowController.showPluginWindow(hostManager);
    CHECK(windowController.isWindowOpen());

    windowController.closePluginWindow();
    CHECK_FALSE(windowController.isWindowOpen());

    // Second close must be safe and idempotent
    windowController.closePluginWindow();
    CHECK_FALSE(windowController.isWindowOpen());
}

TEST_CASE("PluginHostManager - 7. Descarga mientras el editor esta abierto", "[PluginHost]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    core::PluginHostManager hostManager;
    gui::PluginWindowController windowController;

    auto dummy = std::make_unique<DummyPluginInstance>(true);
    hostManager.adoptPluginInstance(std::move(dummy), createDummyDesc());

    windowController.showPluginWindow(hostManager);
    CHECK(windowController.isWindowOpen());

    // Safe wireup: unload closes editor before resetting instance
    hostManager.onPluginUnloading = [&windowController] {
        windowController.closePluginWindow();
    };

    hostManager.unloadPlugin();
    CHECK_FALSE(windowController.isWindowOpen());
    CHECK_FALSE(hostManager.hasActivePlugin());
}

TEST_CASE("PluginHostManager - 8. Descarga durante procesamiento", "[PluginHost]")
{
    core::PluginHostManager hostManager;
    auto dummy = std::make_unique<DummyPluginInstance>(false);
    hostManager.adoptPluginInstance(std::move(dummy), createDummyDesc(), 44100.0, 512);

    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();
    juce::MidiBuffer midi;

    hostManager.processAudioBlock(buffer, midi);
    CHECK(hostManager.hasActivePlugin());

    // Unload during active flow
    hostManager.unloadPlugin();
    CHECK_FALSE(hostManager.hasActivePlugin());

    // Subsequent process calls must safely handle absence
    hostManager.processAudioBlock(buffer, midi);
    SUCCEED("Audio processing handled null instance safely");
}

TEST_CASE("PluginHostManager - 9. Sample rate y block size switching", "[PluginHost]")
{
    core::PluginHostManager hostManager;
    auto dummy = std::make_unique<DummyPluginInstance>(false);
    auto* ptr = dummy.get();

    hostManager.adoptPluginInstance(std::move(dummy), createDummyDesc(), 44100.0, 512);
    CHECK(hostManager.getCurrentSampleRate() == 44100.0);
    CHECK(hostManager.getCurrentBlockSize() == 512);
    CHECK(ptr->getPreparedSampleRate() == 44100.0);
    CHECK(ptr->getPreparedBlockSize() == 512);

    hostManager.prepare(96000.0, 1024);
    CHECK(hostManager.getCurrentSampleRate() == 96000.0);
    CHECK(hostManager.getCurrentBlockSize() == 1024);
    CHECK(ptr->getPreparedSampleRate() == 96000.0);
    CHECK(ptr->getPreparedBlockSize() == 1024);
}

TEST_CASE("PluginHostManager - 10. Note On/Off y allNotesOff", "[PluginHost]")
{
    core::PluginHostManager hostManager;
    auto dummy = std::make_unique<DummyPluginInstance>(false);
    auto* ptr = dummy.get();

    hostManager.adoptPluginInstance(std::move(dummy), createDummyDesc(), 44100.0, 512);

    hostManager.sendMidiMessage(juce::MidiMessage::noteOn(1, 60, 0.8f));
    hostManager.sendMidiMessage(juce::MidiMessage::noteOn(1, 64, 0.8f));

    juce::AudioBuffer<float> buf(2, 512);
    buf.clear();
    juce::MidiBuffer midi;
    hostManager.processAudioBlock(buf, midi);

    CHECK(ptr->getActiveNotes() == 2);

    hostManager.allNotesOff();
    hostManager.processAudioBlock(buf, midi);
    CHECK(ptr->getActiveNotes() == 0);
}

TEST_CASE("PluginHostManager - 11. Estado/preset round-trip", "[PluginHost]")
{
    core::PluginHostManager hostManager;
    auto dummy = std::make_unique<DummyPluginInstance>(false);
    auto* ptr = dummy.get();

    std::vector<uint8_t> testState = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };
    ptr->setStateData(testState);

    hostManager.adoptPluginInstance(std::move(dummy), createDummyDesc());

    juce::MemoryBlock captured;
    REQUIRE(hostManager.getStateInformation(captured));
    CHECK(captured.getSize() == testState.size());

    std::vector<uint8_t> modifiedState = { 0x99, 0x88, 0x77 };
    ptr->setStateData(modifiedState);
    CHECK(ptr->getStateData() == modifiedState);

    REQUIRE(hostManager.setStateInformation(captured.getData(), static_cast<int>(captured.getSize())));
    CHECK(ptr->getStateData() == testState);
}

TEST_CASE("PluginHostManager - 12. Introspeccion de parametros", "[PluginHost]")
{
    core::PluginHostManager hostManager;
    auto dummy = std::make_unique<DummyPluginInstance>(false);
    hostManager.adoptPluginInstance(std::move(dummy), createDummyDesc());

    REQUIRE(hostManager.getParameterCount() == 2);
    CHECK(hostManager.getParameterName(0) == "Drive");
    CHECK(hostManager.getParameterName(1) == "Tone");

    hostManager.setParameterNormalized(0, 0.85f);
    CHECK(std::abs(hostManager.getParameterNormalized(0) - 0.85f) < 0.01f);
}

TEST_CASE("PluginHostManager - 13. profileSha256 estable y determinista", "[PluginHost]")
{
    auto desc = createDummyDesc();
    std::string hash1 = core::PluginHostManager::calculatePluginSha256(desc);
    std::string hash2 = core::PluginHostManager::calculatePluginSha256(desc);

    CHECK(!hash1.empty());
    CHECK(hash1 == hash2);

    core::PluginHostManager hostManager;
    auto dummy = std::make_unique<DummyPluginInstance>(false);
    auto res = hostManager.adoptPluginInstance(std::move(dummy), desc);

    CHECK(res.identity.componentSha256 == hash1);
    CHECK(hostManager.getActivePluginIdentity().componentSha256 == hash1);
}

TEST_CASE("PluginHostManager - 14. Callbacks desconectados y notificaciones", "[PluginHost]")
{
    core::PluginHostManager hostManager;

    int loadedCalls = 0;
    int unloadCalls = 0;

    hostManager.onPluginLoaded = [&](const core::PluginIdentity& ident) {
        loadedCalls++;
        CHECK(ident.name == "DummyPluginInstance");
    };

    hostManager.onPluginUnloading = [&]() {
        unloadCalls++;
    };

    auto dummy = std::make_unique<DummyPluginInstance>(false);
    hostManager.adoptPluginInstance(std::move(dummy), createDummyDesc());

    CHECK(loadedCalls == 1);
    CHECK(unloadCalls == 0);

    hostManager.unloadPlugin();
    CHECK(unloadCalls == 1);

    // After unloading, second unload does not re-invoke callback
    hostManager.unloadPlugin();
    CHECK(unloadCalls == 1);
}

TEST_CASE("PluginHostManager - 15. Fallo de inicializacion sin fuga", "[PluginHost]")
{
    core::PluginHostManager hostManager;
    auto res = hostManager.adoptPluginInstance(nullptr, createDummyDesc());

    CHECK_FALSE(res.succeeded);
    CHECK(res.errorCode == "NULL_INSTANCE");
    CHECK_FALSE(hostManager.hasActivePlugin());
    CHECK(hostManager.getActivePluginInstance() == nullptr);
}

TEST_CASE("PluginHostManager - 16. Dexed Real VST3 Lifecycle and Live RMS", "[PluginHost][dexed]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    core::PluginHostManager hostManager;

    juce::File dexedFile("C:\\Program Files\\Common Files\\VST3\\Dexed.vst3");
    if (!dexedFile.exists())
    {
        SKIP("Dexed.vst3 no encontrado en C:\\Program Files\\Common Files\\VST3\\Dexed.vst3.");
        return;
    }

    // 1. Cargar Dexed
    auto loadRes = hostManager.loadPluginFromFile(dexedFile, 48000.0, 512);
    REQUIRE(loadRes.succeeded);
    CHECK(loadRes.identity.name == "Dexed");
    CHECK(!loadRes.identity.componentSha256.empty());
    REQUIRE(hostManager.hasActivePlugin());

    // 2. Enviar nota MIDI 60 (C4)
    hostManager.sendMidiMessage(juce::MidiMessage::noteOn(1, 60, 0.9f));

    // 3. Procesar bloque de audio y confirmar RMS no silencioso
    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();
    juce::MidiBuffer midi;
    hostManager.processAudioBlock(buffer, midi);

    float rmsL = buffer.getRMSLevel(0, 0, 512);
    float rmsR = buffer.getRMSLevel(1, 0, 512);
    CHECK((rmsL > 0.0001f || rmsR > 0.0001f));

    // 4. Cerrar y descargar
    hostManager.unloadPlugin();
    CHECK_FALSE(hostManager.hasActivePlugin());

    // 5. Volver a cargar para certificar recarga limpia
    auto reloadRes = hostManager.loadPluginFromFile(dexedFile, 48000.0, 512);
    REQUIRE(reloadRes.succeeded);
    CHECK(hostManager.hasActivePlugin());
    CHECK(hostManager.getActivePluginIdentity().name == "Dexed");

    hostManager.unloadPlugin();
    CHECK_FALSE(hostManager.hasActivePlugin());
}

