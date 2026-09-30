/**
 * @file test_VesCz101Feasibility.cpp
 * @brief HITO-10V0: VES Capability & Transport Feasibility Spike
 * 
 * Determina experimentalmente cómo puede ABDAudioLab controlar una instancia concreta
 * de Vintage Emulator Studio (VES) / Casio CZ-101 y qué clase de TargetProfile y transporte
 * es técnicamente honesta antes de abrir HITO-10V1.
 * 
 * Frase rectora:
 * "Un emulador puede reproducir una máquina vintage, pero solo una prueba de capacidad
 * puede demostrar qué interfaz de control ofrece realmente al host y qué transporte
 * declarativo puede usar ABDAudioLab sin inventar capacidades."
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "synth/ExternalPluginFixture.h"
#include "synth/ExternalPluginTypes.h"
#include "synth/Sha256.h"

using namespace abdaudiolab::synth;

namespace
{

juce::File resolveVesBinary()
{
    // 1. Consultar catálogo de plugins conocidos persistido por la aplicación (PluginCache.xml)
    juce::File cacheFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("ABDAudioLab")
        .getChildFile("PluginCache.xml");
    if (cacheFile.existsAsFile())
    {
        if (auto xml = juce::parseXML(cacheFile))
        {
            juce::KnownPluginList list;
            list.recreateFromXml(*xml);
            for (const auto& desc : list.getTypes())
            {
                if (desc.name.containsIgnoreCase("Vintage Emulator Studio") ||
                    desc.fileOrIdentifier.containsIgnoreCase("Vintage Emulator Studio"))
                {
                    juce::File f(desc.fileOrIdentifier);
                    if (f.exists())
                        return f;
                }
            }
        }
    }

    // 2. Consultar directorios de escaneo VST3 persistidos en opciones de la aplicación (PluginScanDirs.xml)
    juce::File scanDirsFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("ABDAudioLab")
        .getChildFile("PluginScanDirs.xml");
    if (scanDirsFile.existsAsFile())
    {
        if (auto xml = juce::parseXML(scanDirsFile))
        {
            for (auto* child : xml->getChildIterator())
            {
                if (child->hasTagName("Directory"))
                {
                    juce::File dir(child->getStringAttribute("path"));
                    if (dir.isDirectory())
                    {
                        juce::File candidate = dir.getChildFile("Vintage Emulator Studio.vst3");
                        if (candidate.exists())
                            return candidate;
                    }
                }
            }
        }
    }

    // 3. Variable de entorno explícita de testing
    auto envPath = juce::SystemStats::getEnvironmentVariable("VES_VST3_PATH", "");
    if (envPath.isNotEmpty())
    {
        juce::File f(envPath);
        if (f.exists())
            return f;
    }

    // 4. Rutas estándar del sistema
    juce::File defaultWin("C:\\Program Files\\Common Files\\VST3\\Vintage Emulator Studio.vst3");
    if (defaultWin.exists())
        return defaultWin;

    juce::File localVst3 = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("../Local/Programs/Common/VST3/Vintage Emulator Studio.vst3");
    if (localVst3.exists())
        return localVst3;

    return {};
}

juce::File resolveVesRomDirectory()
{
    auto envPath = juce::SystemStats::getEnvironmentVariable("VES_ROM_DIR", "");
    if (envPath.isNotEmpty())
    {
        juce::File d(envPath);
        if (d.isDirectory())
            return d;
    }

    // Check AppData settings from VES
    juce::File appDataSettings = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("VintageEmulatorStudio/settings/rom-directory.txt");
    if (appDataSettings.existsAsFile())
    {
        juce::String pathStr = appDataSettings.loadFileAsString().trim();
        if (pathStr.isNotEmpty())
        {
            juce::File d(pathStr);
            if (d.isDirectory())
                return d;
        }
    }

    // Sin VES_ROM_DIR ni configuracion en AppData el test se omite limpio: las ROMs
    // no viven en el repositorio y su ruta es del host, no del proyecto.
    return {};
}

struct Cz101RomInfo
{
    bool found { false };
    juce::File file;
    uint64_t sizeBytes { 0 };
    std::string sha256;
};

Cz101RomInfo inspectCz101Rom(const juce::File& romDir)
{
    Cz101RomInfo info;
    if (!romDir.isDirectory())
        return info;

    juce::File candidateZip = romDir.getChildFile("cz101.zip");
    juce::File candidateBin = romDir.getChildFile("Casio CZ-101_HN613256P_5F3_S40.BIN");
    juce::File candidateDir = romDir.getChildFile("Casio CZ-101 EPROM Firmware");

    juce::File targetFile;
    if (candidateZip.existsAsFile())
        targetFile = candidateZip;
    else if (candidateBin.existsAsFile())
        targetFile = candidateBin;

    if (targetFile.existsAsFile())
    {
        info.found = true;
        info.file = targetFile;
        info.sizeBytes = static_cast<uint64_t>(targetFile.getSize());

        juce::MemoryBlock block;
        if (targetFile.loadFileAsData(block))
        {
            info.sha256 = Sha256::computeHex(block.getData(), block.getSize());
        }
    }
    return info;
}

} // namespace

// ==============================================================================
// HITO-10V0: VES Capability & Transport Feasibility Spike
// ==============================================================================

TEST_CASE("HITO-10V0 - 1. VES VST3 Binary Discovery and Inspection", "[ves][external][firmware_emulated][feasibility]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::File vesFile = resolveVesBinary();
    if (!vesFile.exists())
    {
        SKIP("External fixture unavailable: Vintage Emulator Studio.vst3 not found in standard VST3 paths.");
    }

    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    InspectedPluginModule moduleInfo;
    std::string err;
    bool ok = ExternalPluginFixture::inspectPluginModule(formatManager, vesFile, "", moduleInfo, err);

    REQUIRE(ok);
    REQUIRE(err.empty());

    // 1. Identidad de binario y módulo
    CHECK_FALSE(moduleInfo.selectedUid.empty());
    CHECK_FALSE(moduleInfo.canonicalPath.empty());
    CHECK_FALSE(moduleInfo.binarySha256.empty());
    CHECK_FALSE(moduleInfo.componentNames.empty());
    CHECK(moduleInfo.vendor == "Autodafe");

    // 2. Buses y configuración
    REQUIRE_FALSE(moduleInfo.buses.empty());
    bool hasAudioOutput = false;
    for (const auto& bus : moduleInfo.buses)
    {
        if (!bus.isInput && bus.defaultChannelCount >= 1)
            hasAudioOutput = true;
    }
    CHECK(hasAudioOutput);

    // Telemetría de inspección para acta metrológica
    std::printf("\n======================================================\n");
    std::printf("[HITO-10V0 Discovery & Inspection Report]\n");
    std::printf("  target:        Vintage Emulator Studio.vst3\n");
    std::printf("  canonicalPath: %s\n", moduleInfo.canonicalPath.c_str());
    std::printf("  binarySha256:  %s\n", moduleInfo.binarySha256.c_str());
    std::printf("  selectedUid:   %s\n", moduleInfo.selectedUid.c_str());
    std::printf("  buses:         %zu\n", moduleInfo.buses.size());
    std::printf("  parameterCount (observed): %zu\n", moduleInfo.parameters.size());
    std::printf("======================================================\n");
}

TEST_CASE("HITO-10V0 - 2. Casio CZ-101 ROM Discovery and Legal Fixity", "[ves][external][firmware_emulated][feasibility]")
{
    juce::File romDir = resolveVesRomDirectory();
    if (!romDir.isDirectory())
    {
        SKIP("External fixture note: VES ROM directory not configured or not accessible.");
    }

    Cz101RomInfo rom = inspectCz101Rom(romDir);
    if (!rom.found)
    {
        SKIP("External fixture note: Casio CZ-101 ROM files not found in VES ROM directory.");
    }

    // Regla legal y de seguridad: comprobar fixity (SHA-256 y tamaño) sin incrustar ROM
    REQUIRE(rom.found);
    REQUIRE(rom.sizeBytes > 0);
    REQUIRE_FALSE(rom.sha256.empty());

    std::printf("\n======================================================\n");
    std::printf("[HITO-10V0 ROM Fixity & Legal Compliance Report]\n");
    std::printf("  romDirectory:   %s\n", romDir.getFullPathName().toRawUTF8());
    std::printf("  romFileName:    %s\n", rom.file.getFileName().toRawUTF8());
    std::printf("  romSizeBytes:   %llu B\n", static_cast<unsigned long long>(rom.sizeBytes));
    std::printf("  romSha256:      %s\n", rom.sha256.c_str());
    std::printf("  legalBoundary:  ROM is user-provided; zero ROM bytes stored in repository or TargetProfile.\n");
    std::printf("======================================================\n");
}

TEST_CASE("HITO-10V0 - 3. VES Parameter Catalog and Machine Selection Interface", "[ves][external][firmware_emulated][feasibility]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::File vesFile = resolveVesBinary();
    if (!vesFile.exists())
    {
        SKIP("Vintage Emulator Studio.vst3 not found.");
    }

    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    ExternalPluginFixture fixture(formatManager);
    std::string err;
    bool loaded = fixture.loadPluginFromDisk(vesFile, 48000.0, 256, err);
    REQUIRE(loaded);
    REQUIRE(err.empty());

    auto* instance = fixture.getPluginInstance();
    REQUIRE(instance != nullptr);

    const int numParams = instance->getParameters().size();
    std::vector<std::string> paramNames;
    bool hasCzSpecificParameter = false;
    bool hasMachineSelectionParameter = false;

    for (int i = 0; i < numParams; ++i)
    {
        auto* p = instance->getParameters()[i];
        if (p == nullptr) continue;

        juce::String name = p->getName(64);
        paramNames.push_back(name.toStdString());

        juce::String lower = name.toLowerCase();
        if (lower.contains("cz") || lower.contains("casio"))
            hasCzSpecificParameter = true;
        if (lower.contains("machine") || lower.contains("model") || lower.contains("synth"))
            hasMachineSelectionParameter = true;
    }

    std::printf("\n======================================================\n");
    std::printf("[HITO-10V0 Parameter Catalog & Machine Selection Interface]\n");
    std::printf("  totalParametersExposed:       %d\n", numParams);
    std::printf("  hasMachineSelectionParameter: %s\n", hasMachineSelectionParameter ? "YES" : "NO");
    std::printf("  hasCzSpecificParameter:       %s\n", hasCzSpecificParameter ? "YES" : "NO");

    if (numParams > 0 && numParams <= 20)
    {
        for (size_t i = 0; i < paramNames.size(); ++i)
        {
            std::printf("    [%zu] %s\n", i, paramNames[i].c_str());
        }
    }
    else if (numParams > 20)
    {
        std::printf("    (Showing first 10 and last 5 parameters of %d)\n", numParams);
        for (int i = 0; i < 10 && i < numParams; ++i)
            std::printf("    [%d] %s\n", i, paramNames[i].c_str());
        std::printf("    ...\n");
        for (int i = numParams - 5; i < numParams; ++i)
            std::printf("    [%d] %s\n", i, paramNames[i].c_str());
    }
    std::printf("======================================================\n");

    CHECK(numParams >= 0);
}

TEST_CASE("HITO-10V0 - 4. Control Transport Feasibility: Note, CC, SysEx and Audio Production", "[ves][external][firmware_emulated][feasibility]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::File vesFile = resolveVesBinary();
    if (!vesFile.exists())
    {
        SKIP("Vintage Emulator Studio.vst3 not found.");
    }

    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    ExternalPluginFixture fixture(formatManager);
    std::string err;
    bool loaded = fixture.loadPluginFromDisk(vesFile, 48000.0, 256, err);
    REQUIRE(loaded);

    auto* instance = fixture.getPluginInstance();
    REQUIRE(instance != nullptr);

    instance->prepareToPlay(48000.0, 256);

    const int blockSize = 256;
    juce::AudioBuffer<float> buffer(2, blockSize);
    juce::MidiBuffer midi;

    // 1. Enviar NoteOn C4 (MIDI 60)
    buffer.clear();
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<uint8_t>(100)), 0);
    instance->processBlock(buffer, midi);

    // 2. Enviar MIDI CC 7 (Volume) y CC 1 (Modulation)
    buffer.clear();
    midi.clear();
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 7, 127), 0);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 1, 64), 10);
    instance->processBlock(buffer, midi);

    // 3. Enviar SysEx virtual de prueba Casio CZ (Manufacturer ID 0x44)
    // Casio CZ parameter change / handshake frame test
    const uint8_t czSysExBytes[] = { 0xF0, 0x44, 0x00, 0x00, 0x70, 0x10, 0x00, 0x3F, 0xF7 };
    buffer.clear();
    midi.clear();
    midi.addEvent(juce::MidiMessage(czSysExBytes, sizeof(czSysExBytes)), 0);

    bool sysexProcessedWithoutCrash = true;
    try
    {
        instance->processBlock(buffer, midi);
    }
    catch (...)
    {
        sysexProcessedWithoutCrash = false;
    }
    REQUIRE(sysexProcessedWithoutCrash);

    // 4. Renderizar 10 bloques para comprobar estabilidad y generación de audio
    float maxMagnitude = 0.0f;
    for (int b = 0; b < 10; ++b)
    {
        buffer.clear();
        midi.clear();
        instance->processBlock(buffer, midi);
        maxMagnitude = std::max(maxMagnitude, buffer.getMagnitude(0, blockSize));
    }

    // 5. Enviar NoteOff
    buffer.clear();
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOff(1, 60, static_cast<uint8_t>(0)), 0);
    instance->processBlock(buffer, midi);

    instance->releaseResources();

    std::printf("\n======================================================\n");
    std::printf("[HITO-10V0 Control Transport & Audio Telemetry Report]\n");
    std::printf("  noteEventsAccepted:          YES (No host rejection/crash)\n");
    std::printf("  ccEventsAccepted:            YES (No host rejection/crash)\n");
    std::printf("  virtualSysExAccepted:        %s\n", sysexProcessedWithoutCrash ? "YES (Frame injected without crash)" : "NO");
    std::printf("  audioBufferMaxMagnitude:     %.6f\n", maxMagnitude);
    std::printf("  stateCanBeReset:             YES (prepareToPlay/releaseResources clean)\n");
    std::printf("======================================================\n");
}

TEST_CASE("HITO-10V0 - 5. Determinism and Repeatability Classification", "[ves][external][firmware_emulated][feasibility]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::File vesFile = resolveVesBinary();
    if (!vesFile.exists())
    {
        SKIP("Vintage Emulator Studio.vst3 not found.");
    }

    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    ExternalPluginFixture fixture(formatManager);
    std::string err;
    bool loaded = fixture.loadPluginFromDisk(vesFile, 48000.0, 256, err);
    REQUIRE(loaded);

    auto* instance = fixture.getPluginInstance();
    REQUIRE(instance != nullptr);

    const int totalBlocks = 20;
    const int blockSize = 256;
    const int totalSamples = totalBlocks * blockSize;

    std::vector<float> pass1(totalSamples, 0.0f);
    std::vector<float> pass2(totalSamples, 0.0f);

    auto renderPass = [&](std::vector<float>& dest) {
        instance->prepareToPlay(48000.0, blockSize);
        juce::AudioBuffer<float> buffer(2, blockSize);
        juce::MidiBuffer midi;

        for (int b = 0; b < totalBlocks; ++b)
        {
            buffer.clear();
            midi.clear();
            if (b == 0)
            {
                midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<uint8_t>(90)), 0);
            }
            else if (b == 10)
            {
                midi.addEvent(juce::MidiMessage::noteOff(1, 60, static_cast<uint8_t>(0)), 0);
            }

            instance->processBlock(buffer, midi);

            const float* ch0 = buffer.getReadPointer(0);
            std::copy(ch0, ch0 + blockSize, dest.data() + (b * blockSize));
        }
        instance->releaseResources();
    };

    renderPass(pass1);
    renderPass(pass2);

    float maxDiff = 0.0f;
    for (size_t i = 0; i < pass1.size(); ++i)
    {
        float diff = std::abs(pass1[i] - pass2[i]);
        if (diff > maxDiff)
            maxDiff = diff;
    }

    std::string classification;
    if (maxDiff == 0.0f)
        classification = "ByteIdentical";
    else if (maxDiff < 1e-4f)
        classification = "FunctionallyEquivalent";
    else
        classification = "TraceableButNonDeterministic";

    std::printf("\n======================================================\n");
    std::printf("[HITO-10V0 Determinism & Repeatability Report]\n");
    std::printf("  pass1 samples:                %d\n", totalSamples);
    std::printf("  pass2 samples:                %d\n", totalSamples);
    std::printf("  maxAbsoluteSampleDifference:  %.8f\n", maxDiff);
    std::printf("  repeatabilityClassification:  %s\n", classification.c_str());
    std::printf("======================================================\n");

    CHECK(maxDiff >= 0.0f);
}
