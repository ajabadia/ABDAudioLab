#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "DspEffects/JunoBBD.h"
#include "DspEffects/characters/BbdNoise.h"
#include "DspEffects/profiles/JunoBbdProfile.h"
#include "dsp/VoiceDispersionModel.h"
#include "dsp/VoiceAllocator.h"
#include <vector>
#include <cmath>

// El coro BBD que se prueba aqui es el motor COMPARTIDO
// (ABDSharedCode/DspEffects/JunoBBD.h), no el borrador que hubo en
// `dsp/JunoBBD.h`. Ese shim se borro: reexportaba un placeholder de LutDSP que
// no adopto nadie y que leia del sitio equivocado del modulo. El por que esta
// escrito en ABDSharedCode/_Deprecados/LutDSP-JunoBBD.h. Aqui lo que se prueba
// es el motor que se va a usar, con su perfil y su etapa de caracter reales.
using TestBbd = abd::dsp::JunoBBD<abd::dsp::JunoBbdJ106Profile, abd::dsp::BbdNoiseStage>;

TEST_CASE("JunoBBD - Stereo Bucket Brigade Device Emulation", "[dsp][bbd][chorus]")
{
    constexpr double sampleRate = 44100.0;

    TestBbd bbd;
    bbd.prepare(sampleRate);

    SECTION("Mode Off produces zero modification")
    {
        bbd.setMode(abd::dsp::JunoBbdMode::Off);

        for (int i = 0; i < 128; ++i)
        {
            float outL = 0.0f, outR = 0.0f;
            bbd.process(0.5f, 0.5f, outL, outR);

            REQUIRE(outL == 0.5f);
            REQUIRE(outR == 0.5f);
        }
    }

    SECTION("Mode I & II produce quadrature stereo modulation and stable gain")
    {
        bbd.setMode(abd::dsp::JunoBbdMode::ChorusI);

        // Input: 1.0f DC pulse, rellenando el bloque en cada pasada para que la
        // linea del BBD se llene (como hacia el test anterior con processBlock).
        float outL = 0.0f, outR = 0.0f, peak = 0.0f;
        for (int b = 0; b < 10; ++b)
            for (int i = 0; i < 512; ++i)
            {
                bbd.process(1.0f, 1.0f, outL, outR);
                peak = std::max(peak, std::max(std::abs(outL), std::abs(outR)));
            }

        // MEDIDO, no copiado del test anterior: con DC de 1.0 el motor asienta
        // en L=1.6064 y R=1.5082, y el pico llega a 1.6816. Que L y R NO sean
        // iguales es correcto y es de lo que va el motor: las dos lineas tienen
        // frecuencias de reloj distintas (la tolerancia de reloj de +/-1.5%), y
        // de ahi sale justamente el batido del coro BBD. El test viejo pedia
        // ~1.0 en los dos porque el borrador mezclaba a 0.5 + 0.5; este mezcla
        // con las ganancias ASIMETRICAS del IC6 del Juno (seco 0.863, mojado
        // 1.257). El margen cubre el rizado del LFO, no es un numero de adorno.
        REQUIRE_THAT(outL, Catch::Matchers::WithinAbs(1.60f, 0.15f));
        REQUIRE_THAT(outR, Catch::Matchers::WithinAbs(1.55f, 0.15f));

        // Y que la ganancia no se dispare: con DC y realimentacion, un motor
        // mal puesto en la realimentacion se va a infinito en un par de bloques.
        REQUIRE(peak < 2.0f);

        SECTION("and it is deterministic")
        {
            // Dos motores FRESCOS con el mismo guion dan lo mismo bit a bit. Los
            // tres generadores de ruido del motor son LCG, asi que esto no deberia
            // fallar nunca; si falla, alguien ha metido un reloj o un rand.
            //
            // Y "frescos" es la palabra que importa, y no es un detalle: comparar
            // el motor de arriba (que ya lleva 5120 muestras de DC dentro) con
            // uno recien construido NO mide determinismo, mide que el primero
            // tenga cola, y siempre darian distinto. Esta es la TERCERA vez que
            // esa trampa muerde en este repo (la primera en el `invertLeft` del
            // reverb, la segunda en el eco multi-cabezal). Por eso los dos
            // motores de esta comprobacion son nuevos.
            TestBbd first, second;
            first.prepare(sampleRate);
            second.prepare(sampleRate);
            first.setMode(abd::dsp::JunoBbdMode::ChorusI);
            second.setMode(abd::dsp::JunoBbdMode::ChorusI);

            float aL = 0.0f, aR = 0.0f, bL = 0.0f, bR = 0.0f;
            for (int i = 0; i < 4096; ++i)
            {
                const float in = 0.3f * std::sin(float(i) * 0.02f);
                first.process(in, in, aL, aR);
                second.process(in, in, bL, bR);
            }

            REQUIRE(aL == bL);
            REQUIRE(aR == bR);
        }
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
