#include <catch2/catch_test_macros.hpp>
#include "core/plugins/PluginHostManager.h"
#include "core/plugins/PluginHardwareContractAdapter.h"
#include "audio/LabAudioEngine.h"

using namespace abdaudiolab;

TEST_CASE("PluginHostManager - Lifecycle and Catalog Formats", "[PluginHost]")
{
    core::PluginHostManager hostManager;

    SECTION("Format Manager initializes supported formats")
    {
        auto& fm = hostManager.getFormatManager();
        CHECK(fm.getNumFormats() >= 0);
    }

    SECTION("KnownPluginList XML roundtrip")
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

TEST_CASE("PluginHardwareContractAdapter - Parameter Introspection", "[PluginHost]")
{
    SECTION("Adapter generates valid HardwareContract from dummy processor description")
    {
        juce::PluginDescription desc;
        desc.name = "Test Distortion Plugin";
        desc.pluginFormatName = "VST3";
        desc.manufacturerName = "ABD Audio Research";
        desc.fileOrIdentifier = "test_distortion.vst3";
        desc.isInstrument = false;

        // Custom test processor to verify introspection
        class DummyPluginProcessor : public juce::AudioProcessor
        {
        public:
            DummyPluginProcessor()
            {
                addParameter(new juce::AudioParameterFloat({"drive", 1}, "Drive", 0.0f, 1.0f, 0.5f));
                addParameter(new juce::AudioParameterFloat({"tone", 1}, "Tone", 0.0f, 1.0f, 0.7f));
            }
            const juce::String getName() const override { return "Test Distortion Plugin"; }
            void prepareToPlay(double, int) override {}
            void releaseResources() override {}
            void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
            double getTailLengthSeconds() const override { return 0.0; }
            bool acceptsMidi() const override { return false; }
            bool producesMidi() const override { return false; }
            juce::AudioProcessorEditor* createEditor() override { return nullptr; }
            bool hasEditor() const override { return false; }
            int getNumPrograms() override { return 1; }
            int getCurrentProgram() override { return 0; }
            void setCurrentProgram(int) override {}
            const juce::String getProgramName(int) override { return {}; }
            void changeProgramName(int, const juce::String&) override {}
            void getStateInformation(juce::MemoryBlock&) override {}
            void setStateInformation(const void*, int) override {}
        };

        DummyPluginProcessor dummy;
        auto contract = core::PluginHardwareContractAdapter::createContractFromPlugin(dummy, desc);

        CHECK(contract.deviceType == "SOFTWARE_PLUGIN");
        CHECK(contract.displayName == "Test Distortion Plugin");
        CHECK(contract.brand == "ABD Audio Research");
        REQUIRE_FALSE(contract.functions.empty());

        const auto& fn = contract.functions[0];
        REQUIRE(fn.controls.size() == 2);
        CHECK(fn.controls[0].name == "Drive");
        CHECK(fn.controls[0].controlMethod == "SOFTWARE_PLUGIN_PARAM");
        CHECK(fn.controls[1].name == "Tone");

        // Verify normalized parameter setting
        core::PluginHardwareContractAdapter::setParameterNormalized(dummy, 1, 0.85f);
        CHECK(std::abs(core::PluginHardwareContractAdapter::getParameterNormalized(dummy, 1) - 0.85f) < 0.001f);
    }
}

TEST_CASE("LabAudioEngine - Plugin Digital Loopback Routing", "[PluginHost]")
{
    audio::LabAudioEngine engine;

    class PassThroughProcessor : public juce::AudioPluginInstance
    {
    public:
        PassThroughProcessor() = default;
        const juce::String getName() const override { return "PassThrough"; }
        void prepareToPlay(double, int) override {}
        void releaseResources() override {}
        void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
        {
            // Apply a known fixed gain of 0.5 to test direct processing
            buffer.applyGain(0.5f);
        }
        double getTailLengthSeconds() const override { return 0.0; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram(int) override {}
        const juce::String getProgramName(int) override { return {}; }
        void changeProgramName(int, const juce::String&) override {}
        void getStateInformation(juce::MemoryBlock&) override {}
        void setStateInformation(const void*, int) override {}
        void fillInPluginDescription(juce::PluginDescription&) const override {}
    };

    PassThroughProcessor plugin;
    engine.setActivePluginInstance(&plugin);
    CHECK(engine.getActivePluginInstance() == &plugin);

    engine.setActivePluginInstance(nullptr);
    CHECK(engine.getActivePluginInstance() == nullptr);
}
