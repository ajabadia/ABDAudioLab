#include <catch2/catch_test_macros.hpp>
#include "hardware/MidiIdentityDetector.h"
#include "core/HardwareContractRegistry.h"
#include <juce_core/juce_core.h>

static std::vector<abdaudiolab::core::HardwareContract> loadTestContracts()
{
    abdaudiolab::core::HardwareContractRegistry registry;

    const juce::Array<juce::File> searchRoots {
        juce::File::getCurrentWorkingDirectory(),
        juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory(),
        juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getParentDirectory(),
        juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getParentDirectory().getParentDirectory(),
        juce::File("d:/desarrollos/ABDSynths/ABDAudioLab"),
        juce::File("D:/desarrollos/ABDSynths/ABDSharedAssets")
    };

    for (const auto& root : searchRoots)
    {
        auto c1 = root.getChildFile("contracts").getChildFile("hardware");
        if (c1.isDirectory() && registry.loadContractsFromDirectory(c1))
            break;

        auto c2 = root.getChildFile("contracts");
        if (c2.isDirectory() && registry.loadContractsFromDirectory(c2))
            break;
    }

    return registry.getContracts();
}

TEST_CASE("MidiIdentityDetector Request Generation", "[midi][sysex]")
{
    using namespace abdaudiolab::hardware;

    auto req = MidiIdentityDetector::makeIdentityRequest(0x7F);
    REQUIRE(req.isSysEx());

    const auto* data = req.getSysExData();
    const int size = req.getSysExDataSize();

    REQUIRE(size == 4);
    REQUIRE(data[0] == 0x7E); // Universal Non-Realtime
    REQUIRE(data[1] == 0x7F); // Broadcast
    REQUIRE(data[2] == 0x06); // General Information
    REQUIRE(data[3] == 0x01); // Identity Request
}

TEST_CASE("MidiIdentityDetector Contract-Driven Identity Reply Parsing", "[midi][sysex]")
{
    using namespace abdaudiolab::hardware;
    auto contracts = loadTestContracts();
    REQUIRE(!contracts.empty());

    SECTION("Roland AIRA Bitrazer Universal Reply")
    {
        // F0 7E 10 06 02 41 [family: 00 00] [model: 15 00] [rev: 01 00 00 00] F7
        const uint8_t sysex[] = { 0x7E, 0x10, 0x06, 0x02, 0x41, 0x00, 0x00, 0x15, 0x00, 0x01, 0x00, 0x00, 0x00 };
        auto msg = juce::MidiMessage::createSysExMessage(sysex, sizeof(sysex));

        DiscoveredDevice dev;
        REQUIRE(MidiIdentityDetector::parseIdentityReply(msg, dev, contracts));
        REQUIRE(dev.isSysExVerified);
        REQUIRE(dev.manufacturer == "Roland");
        REQUIRE(dev.hardwareId == "roland_aira_bitrazer");
        REQUIRE(dev.model == "Bitrazer");
    }

    SECTION("Roland AIRA Demora Universal Reply")
    {
        const uint8_t sysex[] = { 0x7E, 0x10, 0x06, 0x02, 0x41, 0x00, 0x00, 0x16, 0x00, 0x01, 0x00, 0x00, 0x00 };
        auto msg = juce::MidiMessage::createSysExMessage(sysex, sizeof(sysex));

        DiscoveredDevice dev;
        REQUIRE(MidiIdentityDetector::parseIdentityReply(msg, dev, contracts));
        REQUIRE(dev.hardwareId == "roland_aira_demora");
        REQUIRE(dev.model == "Demora");
    }

    SECTION("Casio CZ-101 Universal Reply")
    {
        // F0 7E 00 06 02 44 [family: 00 00] [model: 70 00] F7
        const uint8_t sysex[] = { 0x7E, 0x00, 0x06, 0x02, 0x44, 0x00, 0x00, 0x70, 0x00 };
        auto msg = juce::MidiMessage::createSysExMessage(sysex, sizeof(sysex));

        DiscoveredDevice dev;
        REQUIRE(MidiIdentityDetector::parseIdentityReply(msg, dev, contracts));
        REQUIRE(dev.manufacturer == "Casio");
        REQUIRE(dev.hardwareId == "casio_cz101");
    }

    SECTION("Roland Juno-106 Universal Reply")
    {
        // F0 7E 10 06 02 41 [family: 32 00] [model: 00 00] F7
        const uint8_t sysex[] = { 0x7E, 0x10, 0x06, 0x02, 0x41, 0x32, 0x00, 0x00, 0x00 };
        auto msg = juce::MidiMessage::createSysExMessage(sysex, sizeof(sysex));

        DiscoveredDevice dev;
        REQUIRE(MidiIdentityDetector::parseIdentityReply(msg, dev, contracts));
        REQUIRE(dev.manufacturer == "Roland");
        REQUIRE(dev.hardwareId == "roland_juno106");
    }

    SECTION("Korg MS2000 Universal Reply")
    {
        const uint8_t sysex[] = { 0x7E, 0x00, 0x06, 0x02, 0x42, 0x00, 0x00, 0x58, 0x00 };
        auto msg = juce::MidiMessage::createSysExMessage(sysex, sizeof(sysex));

        DiscoveredDevice dev;
        REQUIRE(MidiIdentityDetector::parseIdentityReply(msg, dev, contracts));
        REQUIRE(dev.manufacturer == "Korg");
        REQUIRE(dev.hardwareId == "korg_ms2000");
    }

    SECTION("Korg Prophecy Universal Reply")
    {
        const uint8_t sysex[] = { 0x7E, 0x00, 0x06, 0x02, 0x42, 0x00, 0x00, 0x5A, 0x00 };
        auto msg = juce::MidiMessage::createSysExMessage(sysex, sizeof(sysex));

        DiscoveredDevice dev;
        REQUIRE(MidiIdentityDetector::parseIdentityReply(msg, dev, contracts));
        REQUIRE(dev.manufacturer == "Korg");
        REQUIRE(dev.hardwareId == "korg_prophecy");
    }

    SECTION("Behringer PRO-800 Universal Reply")
    {
        // 00 20 32 is Behringer MMA ID, Model 2C is PRO-800
        const uint8_t sysex[] = { 0x7E, 0x00, 0x06, 0x02, 0x00, 0x20, 0x32, 0x00, 0x00, 0x2C, 0x00 };
        auto msg = juce::MidiMessage::createSysExMessage(sysex, sizeof(sysex));

        DiscoveredDevice dev;
        REQUIRE(MidiIdentityDetector::parseIdentityReply(msg, dev, contracts));
        REQUIRE(dev.manufacturer == "Behringer");
        REQUIRE(dev.hardwareId == "behringer_pro800");
    }

    SECTION("Behringer DeepMind 12 Universal Reply")
    {
        // 00 20 32 is Behringer MMA ID, Model 20 is DeepMind 12
        const uint8_t sysex[] = { 0x7E, 0x00, 0x06, 0x02, 0x00, 0x20, 0x32, 0x00, 0x00, 0x20, 0x00 };
        auto msg = juce::MidiMessage::createSysExMessage(sysex, sizeof(sysex));

        DiscoveredDevice dev;
        REQUIRE(MidiIdentityDetector::parseIdentityReply(msg, dev, contracts));
        REQUIRE(dev.manufacturer == "Behringer");
        REQUIRE(dev.hardwareId == "behringer_deepmind12");
    }

    SECTION("Yamaha DX7II Universal Reply")
    {
        const uint8_t sysex[] = { 0x7E, 0x00, 0x06, 0x02, 0x43, 0x00, 0x00, 0x01, 0x00 };
        auto msg = juce::MidiMessage::createSysExMessage(sysex, sizeof(sysex));

        DiscoveredDevice dev;
        REQUIRE(MidiIdentityDetector::parseIdentityReply(msg, dev, contracts));
        REQUIRE(dev.manufacturer == "Yamaha");
        REQUIRE(dev.hardwareId == "yamaha_dx7ii");
    }
}

TEST_CASE("MidiIdentityDetector Contract-Driven Port Name Heuristics", "[midi][sysex]")
{
    using namespace abdaudiolab::hardware;
    auto contracts = loadTestContracts();
    REQUIRE(!contracts.empty());

    SECTION("Roland AIRA Torcido")
    {
        juce::MidiDeviceInfo inDev { "Roland TORCIDO In", "dev_torcido_in" };
        juce::MidiDeviceInfo outDev { "Roland TORCIDO Out", "dev_torcido_out" };

        auto match = MidiIdentityDetector::matchFromPortNames(inDev, outDev, contracts);
        REQUIRE(match.has_value());
        REQUIRE(match->hardwareId == "roland_aira_torcido");
    }

    SECTION("Roland Juno (ABDJUNIO601)")
    {
        juce::MidiDeviceInfo inDev { "ABDJUNIO601 Synth In", "dev_juno_in" };
        juce::MidiDeviceInfo outDev { "ABDJUNIO601 Synth Out", "dev_juno_out" };

        auto match = MidiIdentityDetector::matchFromPortNames(inDev, outDev, contracts);
        REQUIRE(match.has_value());
        REQUIRE(match->hardwareId == "roland_juno106");
    }

    SECTION("Behringer PRO-800")
    {
        juce::MidiDeviceInfo inDev { "PRO-800 MIDI IN", "dev_pro800_in" };
        juce::MidiDeviceInfo outDev { "PRO-800 MIDI OUT", "dev_pro800_out" };

        auto match = MidiIdentityDetector::matchFromPortNames(inDev, outDev, contracts);
        REQUIRE(match.has_value());
        REQUIRE(match->hardwareId == "behringer_pro800");
    }

    SECTION("Behringer DeepMind 12")
    {
        juce::MidiDeviceInfo inDev { "DeepMind 12 MIDI", "dev_dm12_in" };
        juce::MidiDeviceInfo outDev { "DeepMind 12 MIDI", "dev_dm12_out" };

        auto match = MidiIdentityDetector::matchFromPortNames(inDev, outDev, contracts);
        REQUIRE(match.has_value());
        REQUIRE(match->hardwareId == "behringer_deepmind12");
    }

    SECTION("Behringer DeepMind 6")
    {
        juce::MidiDeviceInfo inDev { "DeepMind 6 MIDI", "dev_dm6_in" };
        juce::MidiDeviceInfo outDev { "DeepMind 6 MIDI", "dev_dm6_out" };

        auto match = MidiIdentityDetector::matchFromPortNames(inDev, outDev, contracts);
        REQUIRE(match.has_value());
        REQUIRE(match->hardwareId == "behringer_deepmind6");
    }

    SECTION("Casio CZ-101 (ABDCZ101)")
    {
        juce::MidiDeviceInfo inDev { "ABDCZ101 Port 1", "dev_cz_in" };
        juce::MidiDeviceInfo outDev { "ABDCZ101 Port 1", "dev_cz_out" };

        auto match = MidiIdentityDetector::matchFromPortNames(inDev, outDev, contracts);
        REQUIRE(match.has_value());
        REQUIRE(match->hardwareId == "casio_cz101");
    }

    SECTION("Korg MS2000 (ABDMS2000)")
    {
        juce::MidiDeviceInfo inDev { "ABDMS2000 USB-MIDI", "dev_ms_in" };
        juce::MidiDeviceInfo outDev { "ABDMS2000 USB-MIDI", "dev_ms_out" };

        auto match = MidiIdentityDetector::matchFromPortNames(inDev, outDev, contracts);
        REQUIRE(match.has_value());
        REQUIRE(match->hardwareId == "korg_ms2000");
    }

    SECTION("Korg Prophecy")
    {
        juce::MidiDeviceInfo inDev { "KORG Prophecy In", "dev_proph_in" };
        juce::MidiDeviceInfo outDev { "KORG Prophecy Out", "dev_proph_out" };

        auto match = MidiIdentityDetector::matchFromPortNames(inDev, outDev, contracts);
        REQUIRE(match.has_value());
        REQUIRE(match->hardwareId == "korg_prophecy");
    }

    SECTION("Yamaha DX7")
    {
        juce::MidiDeviceInfo inDev { "Yamaha DX7 MIDI", "dev_dx7_in" };
        juce::MidiDeviceInfo outDev { "Yamaha DX7 MIDI", "dev_dx7_out" };

        auto match = MidiIdentityDetector::matchFromPortNames(inDev, outDev, contracts);
        REQUIRE(match.has_value());
        REQUIRE(match->hardwareId == "yamaha_dx7");
    }

    SECTION("Yamaha DX7II")
    {
        juce::MidiDeviceInfo inDev { "Yamaha DX7II Port", "dev_dx7ii_in" };
        juce::MidiDeviceInfo outDev { "Yamaha DX7II Port", "dev_dx7ii_out" };

        auto match = MidiIdentityDetector::matchFromPortNames(inDev, outDev, contracts);
        REQUIRE(match.has_value());
        REQUIRE(match->hardwareId == "yamaha_dx7ii");
    }
}
