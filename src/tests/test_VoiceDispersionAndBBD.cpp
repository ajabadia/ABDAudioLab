#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/JunoBBD.h"
#include "dsp/VoiceDispersionModel.h"
#include "dsp/VoiceAllocator.h"
#include <vector>
#include <cmath>

TEST_CASE("JunoBBD - Stereo Bucket Brigade Device Emulation", "[dsp][bbd][chorus]")
{
    using namespace abdaudiolab::dsp;

    JunoBBD bbd;
    const double sampleRate = 44100.0;
    bbd.prepare(sampleRate, 256);

    SECTION("Mode Off produces zero modification")
    {
        bbd.setMode(JunoBBD::Mode::Off);
        juce::AudioBuffer<float> buffer(2, 128);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* p = buffer.getWritePointer(ch);
            for (int i = 0; i < 128; ++i) p[i] = 0.5f;
        }

        bbd.processBlock(buffer);

        for (int ch = 0; ch < 2; ++ch)
        {
            auto* p = buffer.getReadPointer(ch);
            for (int i = 0; i < 128; ++i)
            {
                REQUIRE(p[i] == 0.5f);
            }
        }
    }

    SECTION("Mode I & II produce quadrature stereo modulation and stable gain")
    {
        bbd.setMode(JunoBBD::Mode::ModeI);
        juce::AudioBuffer<float> buffer(2, 512);
        
        // Input: 1.0f DC pulse
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* p = buffer.getWritePointer(ch);
            for (int i = 0; i < 512; ++i) p[i] = 1.0f;
        }

        // Process several blocks to fill BBD line (refilling input DC on each block)
        for (int b = 0; b < 10; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* p = buffer.getWritePointer(ch);
                for (int i = 0; i < 512; ++i) p[i] = 1.0f;
            }
            bbd.processBlock(buffer);
        }

        const float* left = buffer.getReadPointer(0);
        const float* right = buffer.getReadPointer(1);

        // After BBD fills with DC, output should stabilize around ~1.0 (dry 0.5 + wet 0.5)
        REQUIRE_THAT(left[511], Catch::Matchers::WithinAbs(1.0f, 0.15f));
        REQUIRE_THAT(right[511], Catch::Matchers::WithinAbs(1.0f, 0.15f));
    }
}

TEST_CASE("VoiceDispersionModel - Reproducible Analog Component Variance", "[dsp][dispersion][analog]")
{
    using namespace abdaudiolab::dsp;

    VoiceDispersionModel<6> model1;
    VoiceDispersionModel<6> model2;

    model1.reseed(106, 0.03f, 0.024f, 10.0f);
    model2.reseed(106, 0.03f, 0.024f, 10.0f);

    SECTION("Identical seeds produce identical voice offsets")
    {
        for (size_t v = 0; v < 6; ++v)
        {
            const auto& o1 = model1.getVoiceOffset(v);
            const auto& o2 = model2.getVoiceOffset(v);

            REQUIRE(o1.cutoffOffsetNorm == o2.cutoffOffsetNorm);
            REQUIRE(o1.vcaGainMultiplier == o2.vcaGainMultiplier);
            REQUIRE(o1.trackingOffsetCents == o2.trackingOffsetCents);
        }
    }

    SECTION("Different voices within same model have distinct variance")
    {
        const auto& v0 = model1.getVoiceOffset(0);
        const auto& v1 = model1.getVoiceOffset(1);

        // Not identical across voices
        REQUIRE(v0.cutoffOffsetNorm != v1.cutoffOffsetNorm);
        REQUIRE(v0.vcaGainMultiplier != v1.vcaGainMultiplier);

        // Within expected bounds
        for (size_t v = 0; v < 6; ++v)
        {
            float nominalCutoff = 0.5f;
            float dispersedCutoff = model1.applyCutoffDispersion(v, nominalCutoff);
            REQUIRE(dispersedCutoff >= 0.40f);
            REQUIRE(dispersedCutoff <= 0.60f);

            float nominalGain = 1.0f;
            float dispersedGain = model1.applyVcaDispersion(v, nominalGain);
            REQUIRE(dispersedGain >= 0.95f);
            REQUIRE(dispersedGain <= 1.05f);
        }
    }

    SECTION("Thermal drift steps smoothly without discontinuities")
    {
        float prevDrift = model1.getVoiceOffset(0).dynamicThermalPitchCents;
        for (int step = 0; step < 100; ++step)
        {
            model1.stepThermalDrift(2.5f, 0.005f);
            float currDrift = model1.getVoiceOffset(0).dynamicThermalPitchCents;
            // Smooth wandering
            REQUIRE(std::abs(currDrift - prevDrift) < 0.2f);
            // Stays bounded
            REQUIRE(std::abs(currDrift) <= 2.6f);
            prevDrift = currDrift;
        }
    }
}

TEST_CASE("VoiceAllocator - Poly1, Poly2 and Unison Stealing Logic", "[dsp][voices][allocator]")
{
    using namespace abdaudiolab::dsp;

    VoiceAllocator<6> allocator;

    SECTION("Poly1 mode rotates round-robin")
    {
        allocator.setPolyMode(PolyMode::Poly1);

        auto v0 = allocator.allocateNoteOn(60, 0.8f);
        auto v1 = allocator.allocateNoteOn(64, 0.8f);
        auto v2 = allocator.allocateNoteOn(67, 0.8f);

        REQUIRE(v0.size() == 1);
        REQUIRE(v1.size() == 1);
        REQUIRE(v2.size() == 1);

        REQUIRE(v0[0] == 0);
        REQUIRE(v1[0] == 1);
        REQUIRE(v2[0] == 2);
        REQUIRE(allocator.getNumActiveVoices() == 3);

        auto rel = allocator.allocateNoteOff(64);
        REQUIRE(rel.size() == 1);
        REQUIRE(rel[0] == 1);
        REQUIRE(allocator.getNumActiveVoices() == 2);
    }

    SECTION("Voice stealing reclaims oldest note when polyphony is exhausted")
    {
        allocator.setPolyMode(PolyMode::Poly1);

        // Fill all 6 voices
        for (int n = 0; n < 6; ++n)
        {
            allocator.allocateNoteOn(50 + n, 0.9f);
        }
        REQUIRE(allocator.getNumActiveVoices() == 6);

        // 7th note should steal voice 0 (oldest timestamp)
        auto vStolen = allocator.allocateNoteOn(70, 0.9f);
        REQUIRE(vStolen.size() == 1);
        REQUIRE(vStolen[0] == 0);
        REQUIRE(allocator.getVoiceState(0).midiNote == 70);
    }

    SECTION("Unison mode allocates all voices simultaneously")
    {
        allocator.setPolyMode(PolyMode::Unison);

        auto vAll = allocator.allocateNoteOn(60, 1.0f);
        REQUIRE(vAll.size() == 6);
        REQUIRE(allocator.getNumActiveVoices() == 6);

        for (size_t i = 0; i < 6; ++i)
        {
            REQUIRE(allocator.getVoiceState(i).midiNote == 60);
        }

        allocator.allNotesOff();
        REQUIRE(allocator.getNumActiveVoices() == 0);
    }
}
