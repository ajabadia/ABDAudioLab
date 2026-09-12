#include <catch2/catch_test_macros.hpp>
#include "hardware/AudioMidiInterfaceDetector.h"

using namespace abdaudiolab::hardware;

TEST_CASE("AudioMidiInterfaceDetector - Structural & Matching Logic", "[Hardware]")
{
    juce::AudioDeviceManager deviceManager;

    SECTION("Runs cleanly when no audio hardware is connected or configured")
    {
        auto detected = AudioMidiInterfaceDetector::detectInterfaces(deviceManager);
        // May be empty or may detect real devices on the host running the tests
        for (const auto& dev : detected)
        {
            CHECK(dev.id.isNotEmpty());
            CHECK(dev.displayName.isNotEmpty());
            CHECK(dev.brand.isNotEmpty());
            CHECK(dev.imageRelPath.isNotEmpty());
            CHECK((dev.isPresentInSystem || dev.isAnyActiveInSoftware()));
        }
    }
}
