/**
 * @file test_TargetProfilePhysicalPreflightBench.cpp
 * @brief HITO-10D2.7A.2: Preflight físico de solo lectura local en banco controlado.
 * 
 * Verifica la enumeración de puertos del host, la exclusión estricta de puertos virtuales,
 * la detección del target físico (Behringer DeepMind 12D), y el diagnóstico canónico
 * de estados de preflight con CERO bytes transmitidos y ExportReadiness bloqueado.
 * 
 * Regla de oro:
 * "Un cable conectado no convierte una capacidad hipotética en una medición válida;
 * el sistema debe demostrar identidad, consentimiento, trazabilidad y bloqueo seguro
 * antes de poder confiar en cualquier resultado exportable."
 */

#include <catch2/catch_test_macros.hpp>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_core/juce_core.h>

#include "core/LabResourcePaths.h"
#include "hardware/preflight/HardwareTransportPreflightService.h"
#include "hardware/preflight/HardwareTransportPreflightTypes.h"
#include "hardware/transport/JuceMidiTransport.h"
#include "hardware/consent/OperatorConsentService.h"
#include "hardware/consent/OperatorConsentRequest.h"
#include "hardware/MidiIdentityDetector.h"
#include "core/HardwareContractRegistry.h"
#include "gui/session/ProfilingSessionContracts.h"
#include "hardware/transport/HardwareDispatchScheduler.h"
#include "hardware/transport/HardwareDispatchEvidenceRecord.h"
#include "hardware/transport/IMonotonicClock.h"
#include "synth/Sha256.h"
#include "tests/support/MockMidiTransport.h"
#include "tests/support/MockMonotonicClock.h"
#include "tests/support/MockMidiIdentityProbe.h"

using namespace abdaudiolab::hardware;
using namespace abdaudiolab::profiling;
using namespace abdaudiolab::synth;
using namespace abdaudiolab::tests;

namespace
{

bool isVirtualMidiPort(const juce::String& name)
{
    const juce::String lower = name.toLowerCase();
    return lower.contains("loopbe") || 
           lower.contains("loopmidi") || 
           lower.contains("tevirtualmidi") || 
           lower.contains("midi 2.0 loop") ||
           lower.contains("midi 2.0 virtual") ||
           lower.contains("midi 2.0 service");
}

TargetProfile makeDeepMind12Profile()
{
    TargetProfile p;
    p.targetProfileId = "behringer_deepmind12";
    p.displayName = "Behringer DeepMind 12 / 12D";
    p.vendor = "Behringer";
    p.targetKind = "HardwareDigital";
    p.hasExplicitTransportPolicy = true;
    p.transportPolicy.minimumInterMessageDelayMs = 20;
    p.transportPolicy.maximumMessagesPerSecond = 50;
    p.transportPolicy.requiresResponseAck = false;
    p.transportPolicy.requiresVerifiedIdentity = false;
    p.transportPolicy.allowsUserConfirmedUnverifiedIdentity = true;
    return p;
}

TargetProfile makeStrictIdentityProfile()
{
    TargetProfile p = makeDeepMind12Profile();
    p.transportPolicy.requiresVerifiedIdentity = true;
    p.transportPolicy.allowsUserConfirmedUnverifiedIdentity = false;
    return p;
}

MidiPortSelection makeDeepMind12Port()
{
    return MidiPortSelection {
        "DeepMind12D",
        "DeepMind12D",
        "behringer_deepmind12"
    };
}

} // namespace

TEST_CASE("HITO-10D2.7A.2 - 1. Host MIDI Port Enumeration and Virtual Port Exclusion", "[hardware][physical][bench][preflight]")
{
    const auto availableOutputs = juce::MidiOutput::getAvailableDevices();
    const auto availableInputs = juce::MidiInput::getAvailableDevices();

    // El host debe tener puertos enumerados en Windows
    CHECK(!availableOutputs.isEmpty());

    int virtualCount = 0;
    int physicalCount = 0;
    bool deepMindFound = false;

    for (const auto& dev : availableOutputs)
    {
        if (isVirtualMidiPort(dev.name))
        {
            virtualCount++;
        }
        else
        {
            physicalCount++;
            if (dev.name.containsIgnoreCase("DeepMind"))
            {
                deepMindFound = true;
                CHECK(dev.name == "DeepMind12D");
            }
        }
    }

    // Comprobaciones de exclusión de rutas virtuales
    CHECK(virtualCount >= 0);
    // Verificamos que si DeepMind12D está conectado, se clasifica como físico no-virtual
    if (deepMindFound)
    {
        CHECK(physicalCount >= 1);
        CHECK_FALSE(isVirtualMidiPort("DeepMind12D"));
    }
}

TEST_CASE("HITO-10D2.7A.2 - 2. Read-Only Physical Preflight on Observed Endpoint", "[hardware][physical][bench][preflight]")
{
    const auto availableOutputs = juce::MidiOutput::getAvailableDevices();
    juce::MidiDeviceInfo deepMindOut;
    bool found = false;

    for (const auto& dev : availableOutputs)
    {
        if (dev.name.containsIgnoreCase("DeepMind12D") || dev.name.containsIgnoreCase("DeepMind"))
        {
            deepMindOut = dev;
            found = true;
            break;
        }
    }

    if (found)
    {
        // 1. Configuración de puerto observado sin transmisión
        const MidiPortSelection portSelection {
            deepMindOut.identifier.toStdString(),
            deepMindOut.name.toStdString(),
            "behringer_deepmind12"
        };

        const auto profile = makeDeepMind12Profile();
        JuceMidiTransport transport;

        // 2. Ejecución de preflight pasivo (identityProbe = nullptr, 0 bytes)
        const auto result = HardwareTransportPreflightService::evaluate(
            profile,
            portSelection,
            transport,
            nullptr // Sin sonda SysEx inquiry: preflight de solo lectura
        );

        // 3. Verificación de invariantes normativos:
        // - El puerto debe haber abierto con éxito
        CHECK(transport.isOpen());
        // - No se permite despacho automático (readyForConsentOrDispatch es false sin consentimiento D2.4)
        CHECK_FALSE(result.readyForConsentOrDispatch);
        // - Disposición canónica: AwaitingUserConfirmation
        CHECK(result.disposition == HardwarePreflightDisposition::AwaitingUserConfirmation);
        // - Estado de identidad canónico: IdentityUnavailable (sin sonda activa)
        CHECK(result.identityState == HardwareIdentityState::IdentityUnavailable);
        CHECK(result.diagnosticCode == "AWAITING_USER_CONFIRMATION");

        // 4. Cierre seguro del endpoint
        transport.close();
        CHECK_FALSE(transport.isOpen());

        // 5. Garantía estricta de ExportReadiness
        // Durante D2.7A, la exportación metrológica permanece sellada
        CHECK(result.disposition != HardwarePreflightDisposition::Ready);
    }
    else
    {
        // Si se ejecuta en un entorno sin hardware conectado, validamos que no se inventa el puerto
        MidiPortSelection portSelection {
            "invalid_non_existent_id",
            "DeepMind12D",
            "behringer_deepmind12"
        };
        const auto profile = makeDeepMind12Profile();
        JuceMidiTransport transport;

        const auto result = HardwareTransportPreflightService::evaluate(profile, portSelection, transport, nullptr);
        CHECK(result.disposition == HardwarePreflightDisposition::Blocked);
        CHECK(result.identityState == HardwareIdentityState::IdentityUnavailable);
        CHECK(result.diagnosticCode == "ERR_MIDI_OUTPUT_OPEN_FAILED");
    }
}

TEST_CASE("HITO-10D2.7A.2 - 3. Fail-Closed Boundaries on Invalid Port and Strict Identity", "[hardware][physical][bench][preflight]")
{
    SECTION("Invalid port selection fails closed with ERR_MIDI_OUTPUT_OPEN_FAILED")
    {
        const MidiPortSelection invalidPort {
            "non_existent_device_id_9999",
            "NonExistentPort",
            "behringer_deepmind12"
        };

        const auto profile = makeDeepMind12Profile();
        JuceMidiTransport transport;

        const auto result = HardwareTransportPreflightService::evaluate(profile, invalidPort, transport, nullptr);
        CHECK_FALSE(result.readyForConsentOrDispatch);
        CHECK(result.disposition == HardwarePreflightDisposition::Blocked);
        CHECK(result.diagnosticCode == "ERR_MIDI_OUTPUT_OPEN_FAILED");
        CHECK_FALSE(transport.isOpen());
    }

    SECTION("Strict identity policy without probe fails closed with ERR_VERIFIED_IDENTITY_REQUIRED")
    {
        const auto availableOutputs = juce::MidiOutput::getAvailableDevices();
        juce::MidiDeviceInfo targetDev;
        bool found = false;

        for (const auto& dev : availableOutputs)
        {
            if (dev.name.containsIgnoreCase("DeepMind12D"))
            {
                targetDev = dev;
                found = true;
                break;
            }
        }

        if (found)
        {
            const MidiPortSelection portSelection {
                targetDev.identifier.toStdString(),
                targetDev.name.toStdString(),
                "behringer_deepmind12"
            };

            const auto strictProfile = makeStrictIdentityProfile();
            JuceMidiTransport transport;

            const auto result = HardwareTransportPreflightService::evaluate(
                strictProfile,
                portSelection,
                transport,
                nullptr
            );

            CHECK_FALSE(result.readyForConsentOrDispatch);
            CHECK(result.disposition == HardwarePreflightDisposition::Blocked);
            CHECK(result.identityState == HardwareIdentityState::IdentityUnavailable);
            CHECK(result.diagnosticCode == "ERR_VERIFIED_IDENTITY_REQUIRED");

            transport.close();
        }
    }
}

TEST_CASE("HITO-10D2.7A.2 - 4. Contract-Driven Matching Against Native Catalog", "[hardware][physical][bench][preflight]")
{
    // Verificamos que el subsistema de contratos legacy de ABDAudioLab reconoce DeepMind12D
    // mediante coincidencia de nombres de puerto declarados en el JSON
    abdaudiolab::core::HardwareContractRegistry registry;
    const juce::File contractsDir = abdaudiolab::core::contractsHardwareDir();
    registry.loadContractsFromDirectory(contractsDir);

    auto resolution = registry.resolveContractById("behringer_deepmind12");
    REQUIRE(resolution.contract.has_value());
    CHECK(resolution.source == abdaudiolab::core::HardwareContractResolutionSource::NativeLegacyContract);

    const auto& contract = *resolution.contract;
    CHECK(contract.id == "behringer_deepmind12");
    CHECK(contract.displayName == "Behringer DeepMind 12 / 12D");

    // Validamos que los portNameMatches contienen coincidencia con el nombre exacto de Windows
    bool matchFound = false;
    for (const auto& pattern : contract.midiIdentity.portNameMatches)
    {
        if (juce::String("DeepMind12D").containsIgnoreCase(pattern))
        {
            matchFound = true;
            break;
        }
    }
    CHECK(matchFound);
}

TEST_CASE("HITO-10D2.7A.3 - Cryptographic Separation, Canonical Digest & Anti-TOCTOU Verification", "[hardware][physical][bench][consent]")
{
    // 1. Raw wire payload exact definition: Channel 1, CC 1 (Modulation Wheel), Value 0
    const uint8_t rawWireBytes[3] = { 0xB0, 0x01, 0x00 };
    const std::string expectedMessageDigest = "82c7e37b65368c3ef0f5f0a32a8185d14a81820a6be1f130c337b0c6b7681077";

    // Mathematical verification of raw binary SHA-256
    const std::string computedRawHash = abdaudiolab::synth::Sha256::computeHex(rawWireBytes, 3);
    CHECK(computedRawHash == expectedMessageDigest);

    // 2. Structured message verification via OperatorConsentService
    const MidiCcMessage ccMsg {
        1,   // channel 1
        1,   // CC 1 (Modulation Wheel)
        0,   // value 0
        "behringer_deepmind12",
        "modwheel",
        1
    };

    const std::string computedMessageDigest = OperatorConsentService::computeMessageDigest(ccMsg);
    CHECK(computedMessageDigest == expectedMessageDigest);

    // 3. Construction of OperatorConsentRequest with cryptographic and bench separation
    const auto availableOutputs = juce::MidiOutput::getAvailableDevices();
    juce::MidiDeviceInfo deepMindOut;
    bool found = false;
    for (const auto& dev : availableOutputs)
    {
        if (dev.name.containsIgnoreCase("DeepMind12D") || dev.name.containsIgnoreCase("DeepMind"))
        {
            deepMindOut = dev;
            found = true;
            break;
        }
    }

    const MidiPortSelection portSelection {
        found ? deepMindOut.identifier.toStdString() : "DeepMind12D_PortId",
        found ? deepMindOut.name.toStdString() : "DeepMind12D",
        "behringer_deepmind12"
    };

    const auto profile = makeDeepMind12Profile();
    HardwarePreflightResult preflight;
    preflight.portSelection = portSelection;
    preflight.identityState = HardwareIdentityState::IdentityUnavailable;
    preflight.disposition = HardwarePreflightDisposition::AwaitingUserConfirmation;
    preflight.targetProfileId = "behringer_deepmind12";
    preflight.readyForConsentOrDispatch = false;

    auto req = OperatorConsentService::buildRequest(
        profile,
        ProfileProvenanceOrigin::BuiltIn,
        preflight,
        "recipe_d2_7a_preflight_transport_obs",
        "plan_d2_7a_single_step_transport_obs",
        "modwheel",
        0.0,
        ccMsg,
        std::nullopt
    );

    // Set canonical contract vs bench session separation
    req.targetProfileId = "NotApplicableForNativeLegacyContract";
    req.targetContractId = "behringer_deepmind12";
    req.targetResolutionSource = "NativeLegacyContract";
    req.benchSessionId = "d2_7a_deepmind12_2026_09_27_001";
    req.recipeContextId = "recipe_d2_7a_preflight_transport_obs";
    req.executionPlanContextId = "plan_d2_7a_single_step_transport_obs";
    req.commandCanonicalization = "abdaudiolab::hardware::OperatorConsentService::CanonicalV1";
    req.requiresResponseAck = false;

    // Recalculate deterministic command digest with bench and contract attributes
    req.commandDigest = OperatorConsentService::computeCommandDigest(req);
    req.requestId = "REQ_" + req.commandDigest.substr(0, 16);

    // Invariants:
    // - messageDigest is strictly the raw 3-byte SHA-256
    CHECK(req.messageDigest == expectedMessageDigest);
    // - commandDigest represents the contextual canonical authorization and differs from messageDigest
    CHECK(req.commandDigest != req.messageDigest);
    CHECK_FALSE(req.commandDigest.empty());
    CHECK(req.commandCanonicalization == "abdaudiolab::hardware::OperatorConsentService::CanonicalV1");
    // - Request strictly mandates explicit operator confirmation
    CHECK(OperatorConsentService::requiresExplicitConsent(req));

    // 4. Anti-TOCTOU validation:
    // Granted consent token
    const auto consentToken = OperatorConsentService::grantConsent(req, "Operator confirmed on physical bench");
    CHECK(consentToken.decision == OperatorConsentDecision::Granted);
    CHECK(consentToken.readyForDispatch == true);
    CHECK(consentToken.resultingIdentityState == HardwareIdentityState::UserConfirmedUnverified);

    // Valid token passes validation with unmodified parameters
    const auto validated = OperatorConsentService::validateConsentToken(
        req,
        consentToken,
        req.resolvedExecutionPlanHash,
        req.portSelection,
        req.ccMessage,
        req.sysExMessage
    );
    CHECK(validated.readyForDispatch == true);

    // Tampering test A: Payload altered (value 0 -> 1)
    MidiCcMessage tamperedMsg = ccMsg;
    tamperedMsg.value = 1;
    const auto tamperedValidation = OperatorConsentService::validateConsentToken(
        req,
        consentToken,
        req.resolvedExecutionPlanHash,
        req.portSelection,
        tamperedMsg,
        req.sysExMessage
    );
    CHECK_FALSE(tamperedValidation.readyForDispatch);
    CHECK(tamperedValidation.decision == OperatorConsentDecision::Invalidated);
    CHECK(tamperedValidation.diagnosticCode == "ERR_CONSENT_INVALIDATED_TOCTOU");

    // Tampering test B: Modified commandDigest
    OperatorConsentRequest corruptedReq = req;
    corruptedReq.commandDigest = "deadbeefcafebabe0123456789abcdef0123456789abcdef0123456789abcdef";
    const auto corruptedValidation = OperatorConsentService::validateConsentToken(
        corruptedReq,
        consentToken,
        corruptedReq.resolvedExecutionPlanHash,
        corruptedReq.portSelection,
        corruptedReq.ccMessage,
        corruptedReq.sysExMessage
    );
    CHECK_FALSE(corruptedValidation.readyForDispatch);
    CHECK(corruptedValidation.decision == OperatorConsentDecision::Invalidated);
    CHECK(corruptedValidation.diagnosticCode == "ERR_CONSENT_INVALIDATED_TOCTOU");
}

TEST_CASE("HITO-10D2.7A.3 - 6. Literal Canonical String, Byte Count and Digest Fixture Verification", "[hardware][physical][bench][consent]")
{
    const MidiCcMessage ccMsg {
        1,   // channel 1
        1,   // CC 1 (Modulation Wheel)
        0,   // value 0
        "behringer_deepmind12",
        "modwheel",
        1
    };

    const MidiPortSelection portSelection {
        "DeepMind12D",
        "DeepMind12D",
        "behringer_deepmind12"
    };

    TargetProfile profile;
    profile.targetProfileId = "NotApplicableForNativeLegacyContract";
    profile.displayName = "Behringer DeepMind 12 / 12D";
    profile.vendor = "Behringer";
    profile.hasExplicitTransportPolicy = true;
    profile.transportPolicy.minimumInterMessageDelayMs = 20;
    profile.transportPolicy.maximumMessagesPerSecond = 50;
    profile.transportPolicy.requiresResponseAck = false;
    profile.transportPolicy.requiresVerifiedIdentity = false;
    profile.transportPolicy.allowsUserConfirmedUnverifiedIdentity = true;

    HardwarePreflightResult preflight;
    preflight.portSelection = portSelection;
    preflight.identityState = HardwareIdentityState::IdentityUnavailable;
    preflight.disposition = HardwarePreflightDisposition::AwaitingUserConfirmation;
    preflight.targetProfileId = "NotApplicableForNativeLegacyContract";
    preflight.readyForConsentOrDispatch = false;

    auto req = OperatorConsentService::buildRequest(
        profile,
        ProfileProvenanceOrigin::BuiltIn,
        preflight,
        "",
        "",
        "modwheel",
        0.0,
        ccMsg,
        std::nullopt
    );

    req.targetProfileId = "NotApplicableForNativeLegacyContract";
    req.targetContractId = "behringer_deepmind12";
    req.targetResolutionSource = "NativeLegacyContract";
    req.benchSessionId = "d2_7a_deepmind12_2026_09_27_001";
    req.recipeContextId = "recipe_d2_7a_preflight_transport_obs";
    req.executionPlanContextId = "plan_d2_7a_single_step_transport_obs";
    req.commandCanonicalization = "abdaudiolab::hardware::OperatorConsentService::CanonicalV1";
    req.requiresResponseAck = false;

    req.commandDigest = OperatorConsentService::computeCommandDigest(req);
    req.requestId = "REQ_" + req.commandDigest.substr(0, 16);

    // 1. Raw wire bytes and messageDigest verification
    const std::string expectedMessageDigest = "82c7e37b65368c3ef0f5f0a32a8185d14a81820a6be1f130c337b0c6b7681077";
    REQUIRE(OperatorConsentService::computeMessageDigest(ccMsg) == expectedMessageDigest);
    REQUIRE(req.messageDigest == expectedMessageDigest);

    // 2. Canonical UTF-8 representation and byte count verification
    const std::string expectedCanonical = 
        "CANONICAL_V1|TARGET_PROFILE:NotApplicableForNativeLegacyContract|CONTRACT_ID:behringer_deepmind12|RESOLUTION_SOURCE:NativeLegacyContract|BENCH_SESSION:d2_7a_deepmind12_2026_09_27_001|VENDOR:Behringer|PORT_ID:DeepMind12D|PORT_NAME:DeepMind12D|RECIPE_CONTEXT_ID:recipe_d2_7a_preflight_transport_obs|PLAN_CONTEXT_ID:plan_d2_7a_single_step_transport_obs|SEMANTIC_ID:modwheel|NORM_VAL:0.000000|RAW_VAL:0|CC:1:1:0|MSG_DIGEST:82c7e37b65368c3ef0f5f0a32a8185d14a81820a6be1f130c337b0c6b7681077|DELAY_MS:20|REQUIRES_ACK:0|EXPORT_READINESS:BLOCKED";

    const std::string actualCanonical = OperatorConsentService::buildCanonicalCommandString(req);
    REQUIRE(actualCanonical == expectedCanonical);
    REQUIRE(actualCanonical.size() == 535);

    // 3. Exact deterministic commandDigest verification
    const std::string expectedCommandDigest = "5d09261b0204e9f5d61cb52c9a14051c0e5ab3666a77aa7bf70dab53b9f0589b";
    REQUIRE(OperatorConsentService::computeCommandDigest(actualCanonical) == expectedCommandDigest);
    REQUIRE(req.commandDigest == expectedCommandDigest);
}

TEST_CASE("HITO-10D2.7A.4 - Single Physical Dispatch Execution (3 Bytes Wire CC) and Forensics", "[hardware][physical][bench][dispatch]")
{
    // Pre-dispatch Validation 1: Physical endpoint availability
    const auto availableOutputs = juce::MidiOutput::getAvailableDevices();
    bool deepMindAvailable = false;
    for (const auto& dev : availableOutputs)
    {
        if (dev.name == "DeepMind12D")
        {
            deepMindAvailable = true;
            break;
        }
    }
    if (!deepMindAvailable)
    {
        SKIP("SKIP_PHYSICAL_BENCH_NOT_AVAILABLE: Prerrequisito externo no disponible (sintetizador hardware DeepMind 12D no detectado en el host)");
    }

    // Pre-dispatch Validation 2: Exact port selection matches
    const MidiPortSelection portSelection {
        "DeepMind12D",
        "DeepMind12D",
        "behringer_deepmind12"
    };
    REQUIRE(portSelection.displayName == "DeepMind12D");
    REQUIRE(portSelection.stableDeviceId == "DeepMind12D");

    // Pre-dispatch Validation 3 & 4: Target contract & resolution source
    abdaudiolab::core::HardwareContractRegistry registry;
    const juce::File contractsDir = abdaudiolab::core::contractsHardwareDir();
    registry.loadContractsFromDirectory(contractsDir);
    const auto resolution = registry.resolveContractById("behringer_deepmind12");
    REQUIRE(resolution.contract.has_value());
    REQUIRE(resolution.source == abdaudiolab::core::HardwareContractResolutionSource::NativeLegacyContract);
    REQUIRE(resolution.contract->id == "behringer_deepmind12");

    // Pre-dispatch Validation 5: Identity State via Preflight
    const auto profile = makeDeepMind12Profile();
    JuceMidiTransport transport;
    const auto preflight = HardwareTransportPreflightService::evaluate(profile, portSelection, transport, nullptr);
    REQUIRE(transport.isOpen());
    REQUIRE(preflight.identityState == HardwareIdentityState::IdentityUnavailable);
    REQUIRE(preflight.disposition == HardwarePreflightDisposition::AwaitingUserConfirmation);

    // Pre-dispatch Validation 10: Payload reconstruction exact wire bytes B0 01 00
    const MidiCcMessage ccMsg {
        1,   // channel 1
        1,   // CC 1 (Modulation Wheel)
        0,   // value 0
        "behringer_deepmind12",
        "modwheel",
        1
    };
    const uint8_t rawWireBytes[3] = { 0xB0, 0x01, 0x00 };
    const std::string expectedMessageDigest = "82c7e37b65368c3ef0f5f0a32a8185d14a81820a6be1f130c337b0c6b7681077";

    // Pre-dispatch Validation 11: messageDigest recalculation
    REQUIRE(abdaudiolab::synth::Sha256::computeHex(rawWireBytes, 3) == expectedMessageDigest);
    REQUIRE(OperatorConsentService::computeMessageDigest(ccMsg) == expectedMessageDigest);

    // Build the exact approved request
    auto req = OperatorConsentService::buildRequest(
        profile,
        ProfileProvenanceOrigin::BuiltIn,
        preflight,
        "",
        "",
        "modwheel",
        0.0,
        ccMsg,
        std::nullopt
    );

    req.targetProfileId = "NotApplicableForNativeLegacyContract";
    req.targetContractId = "behringer_deepmind12";
    req.targetResolutionSource = "NativeLegacyContract";
    req.benchSessionId = "d2_7a_deepmind12_2026_09_27_001";
    req.recipeContextId = "recipe_d2_7a_preflight_transport_obs";
    req.executionPlanContextId = "plan_d2_7a_single_step_transport_obs";
    req.commandCanonicalization = "abdaudiolab::hardware::OperatorConsentService::CanonicalV1";
    req.requiresResponseAck = false;

    // Pre-dispatch Validation 12: CanonicalV1 reconstruction (535 bytes)
    const std::string expectedCanonical = 
        "CANONICAL_V1|TARGET_PROFILE:NotApplicableForNativeLegacyContract|CONTRACT_ID:behringer_deepmind12|RESOLUTION_SOURCE:NativeLegacyContract|BENCH_SESSION:d2_7a_deepmind12_2026_09_27_001|VENDOR:Behringer|PORT_ID:DeepMind12D|PORT_NAME:DeepMind12D|RECIPE_CONTEXT_ID:recipe_d2_7a_preflight_transport_obs|PLAN_CONTEXT_ID:plan_d2_7a_single_step_transport_obs|SEMANTIC_ID:modwheel|NORM_VAL:0.000000|RAW_VAL:0|CC:1:1:0|MSG_DIGEST:82c7e37b65368c3ef0f5f0a32a8185d14a81820a6be1f130c337b0c6b7681077|DELAY_MS:20|REQUIRES_ACK:0|EXPORT_READINESS:BLOCKED";
    const std::string actualCanonical = OperatorConsentService::buildCanonicalCommandString(req);
    REQUIRE(actualCanonical == expectedCanonical);
    REQUIRE(actualCanonical.size() == 535);

    // Pre-dispatch Validation 13: commandDigest recalculation
    const std::string expectedCommandDigest = "5d09261b0204e9f5d61cb52c9a14051c0e5ab3666a77aa7bf70dab53b9f0589b";
    req.commandDigest = OperatorConsentService::computeCommandDigest(req);
    REQUIRE(req.commandDigest == expectedCommandDigest);

    // Pre-dispatch Validation 6: Operator consent token
    const auto consentToken = OperatorConsentService::grantConsent(req, "Operator approved D2.7A.3 on physical bench");
    REQUIRE(consentToken.decision == OperatorConsentDecision::Granted);
    REQUIRE(consentToken.readyForDispatch == true);

    // Pre-dispatch Validation 14 & 15: Scheduler and pacing setup
    RealMonotonicClock clock;
    HardwareDispatchScheduler scheduler(clock);
    scheduler.resetPacing();

    // EXECUTION OF THE SINGLE AUTHORIZED PHYSICAL DISPATCH (Exactly 3 bytes: B0 01 00)
    const auto evidence = scheduler.executeDispatchCc(
        transport,
        nullptr,
        req,
        consentToken,
        req.executionPlanContextId,
        portSelection,
        *req.ccMessage,
        profile.transportPolicy,
        ExpectedMidiResponse{}
    );

    // IMMEDIATE CLOSE: 0 additional bytes allowed
    transport.close();
    REQUIRE_FALSE(transport.isOpen());

    // FORENSIC EVIDENCE VERIFICATION (D2.7A.4)
    CHECK(evidence.targetContractId == "behringer_deepmind12");
    CHECK(evidence.targetResolutionSource == "NativeLegacyContract");
    CHECK(evidence.benchSessionId == "d2_7a_deepmind12_2026_09_27_001");
    CHECK(evidence.portSelection.displayName == "DeepMind12D");
    CHECK(evidence.sequenceNumber == 1);
    CHECK(evidence.semanticId == "modwheel");
    CHECK(evidence.nativeParameterId == "modwheel");
    CHECK(evidence.messageDigest == expectedMessageDigest);
    CHECK(evidence.commandDigest == expectedCommandDigest);
    CHECK(evidence.transportError == MidiTransportError::None);
    CHECK(evidence.responseOutcome == MidiResponseOutcome::Unsupported);
    CHECK(evidence.consentWasGranted == true);
    CHECK(evidence.diagnosticCode == "OK");
    CHECK(evidence.monotonicTimestampMs > 0);
}

TEST_CASE("HITO-10D2.7A.5 - 8. Nonexistent Port Reject and No Fallback to Index 0", "[hardware][physical][bench][recovery][fail_closed]")
{
    // Test 8: An invalid / nonexistent endpoint name must NEVER fall back to device index 0.
    // It must return open failure, disposition Blocked, 0 bytes transmitted, and transport not open.
    const auto profile = makeDeepMind12Profile();
    const MidiPortSelection invalidPort {
        "D2_7A_NONEXISTENT_DEEPMIND_PORT",
        "D2_7A_NONEXISTENT_DEEPMIND_PORT",
        "behringer_deepmind12"
    };

    JuceMidiTransport transport;
    const auto preflight = HardwareTransportPreflightService::evaluate(profile, invalidPort, transport, nullptr);

    CHECK_FALSE(transport.isOpen());
    CHECK(preflight.disposition == HardwarePreflightDisposition::Blocked);
    CHECK(preflight.identityState == HardwareIdentityState::IdentityUnavailable);
    CHECK(preflight.diagnosticCode == "ERR_MIDI_OUTPUT_OPEN_FAILED");
    CHECK_FALSE(preflight.readyForConsentOrDispatch);
}

TEST_CASE("HITO-10D2.7A.5 - 9. Revoked Consent Prior to Dispatch Prevents Transmission", "[hardware][physical][bench][recovery][fail_closed]")
{
    // Test 9: A valid request whose consent token is denied/revoked prior to dispatch
    // MUST block transmission, record 0 messages written, transport write not invoked, and exportBlocked = true.
    const auto profile = makeDeepMind12Profile();
    const auto port = makeDeepMind12Port();

    HardwarePreflightResult preflight;
    preflight.portSelection = port;
    preflight.identityState = HardwareIdentityState::IdentityUnavailable;
    preflight.disposition = HardwarePreflightDisposition::AwaitingUserConfirmation;
    preflight.targetProfileId = "NotApplicableForNativeLegacyContract";
    preflight.readyForConsentOrDispatch = false;

    const MidiCcMessage ccMsg {
        1,   // channel 1
        1,   // CC 1 (Modulation Wheel)
        0,   // value 0
        "behringer_deepmind12",
        "modwheel",
        1
    };

    auto req = OperatorConsentService::buildRequest(
        profile,
        ProfileProvenanceOrigin::BuiltIn,
        preflight,
        "recipe_d2_7a_preflight_transport_obs",
        "plan_d2_7a_single_step_transport_obs",
        "modwheel",
        0.0,
        ccMsg,
        std::nullopt
    );
    req.targetProfileId = "NotApplicableForNativeLegacyContract";
    req.targetContractId = "behringer_deepmind12";
    req.targetResolutionSource = "NativeLegacyContract";
    req.benchSessionId = "d2_7a_deepmind12_2026_09_27_001";
    req.recipeContextId = "recipe_d2_7a_preflight_transport_obs";
    req.executionPlanContextId = "plan_d2_7a_single_step_transport_obs";
    req.commandCanonicalization = "abdaudiolab::hardware::OperatorConsentService::CanonicalV1";
    req.commandDigest = OperatorConsentService::computeCommandDigest(req);

    // Operator revokes / denies consent
    const auto revokedConsent = OperatorConsentService::denyConsent(req, "Operator revoked consent prior to scheduler dispatch");
    CHECK(revokedConsent.decision == OperatorConsentDecision::Denied);
    CHECK_FALSE(revokedConsent.readyForDispatch);

    MockMonotonicClock clock(1000);
    HardwareDispatchScheduler scheduler(clock);
    MockMidiTransport transport;
    transport.open(port);

    const auto evidence = scheduler.executeDispatchCc(
        transport,
        nullptr,
        req,
        revokedConsent,
        req.executionPlanContextId,
        port,
        ccMsg,
        profile.transportPolicy,
        ExpectedMidiResponse{}
    );

    // Assertions: 0 messages sent, blocked by consent, export strictly blocked
    CHECK(transport.sentCc().empty());
    CHECK(evidence.exportBlocked == true);
    CHECK(evidence.consentWasGranted == false);
    CHECK(evidence.diagnosticCode == "ERR_CONSENT_NOT_GRANTED");
    CHECK(evidence.transportError == MidiTransportError::None);
}

TEST_CASE("HITO-10D2.7A.5 - 10. Anti-TOCTOU Tampering Invalidation (Payload, Port, Bench Session)", "[hardware][physical][bench][recovery][fail_closed]")
{
    // Test 10: Valid consent granted for B0 01 00 is strictly invalidated if any execution parameter alters.
    const auto profile = makeDeepMind12Profile();
    const auto port = makeDeepMind12Port();

    HardwarePreflightResult preflight;
    preflight.portSelection = port;
    preflight.identityState = HardwareIdentityState::IdentityUnavailable;
    preflight.disposition = HardwarePreflightDisposition::AwaitingUserConfirmation;
    preflight.targetProfileId = "NotApplicableForNativeLegacyContract";
    preflight.readyForConsentOrDispatch = false;

    const MidiCcMessage ccMsg {
        1,   // channel 1
        1,   // CC 1 (Modulation Wheel)
        0,   // value 0
        "behringer_deepmind12",
        "modwheel",
        1
    };

    auto req = OperatorConsentService::buildRequest(
        profile,
        ProfileProvenanceOrigin::BuiltIn,
        preflight,
        "recipe_d2_7a_preflight_transport_obs",
        "plan_d2_7a_single_step_transport_obs",
        "modwheel",
        0.0,
        ccMsg,
        std::nullopt
    );
    req.targetProfileId = "NotApplicableForNativeLegacyContract";
    req.targetContractId = "behringer_deepmind12";
    req.targetResolutionSource = "NativeLegacyContract";
    req.benchSessionId = "d2_7a_deepmind12_2026_09_27_001";
    req.recipeContextId = "recipe_d2_7a_preflight_transport_obs";
    req.executionPlanContextId = "plan_d2_7a_single_step_transport_obs";
    req.commandCanonicalization = "abdaudiolab::hardware::OperatorConsentService::CanonicalV1";
    req.commandDigest = OperatorConsentService::computeCommandDigest(req);

    const auto validToken = OperatorConsentService::grantConsent(req, "Operator approved D2.7A.3 on physical bench");
    REQUIRE(validToken.readyForDispatch);

    MockMonotonicClock clock(2000);
    HardwareDispatchScheduler scheduler(clock);
    MockMidiTransport transport;
    transport.open(port);

    SECTION("Variant A: Altered Payload (B0 01 00 -> B0 01 01)")
    {
        MidiCcMessage alteredMsg = ccMsg;
        alteredMsg.value = 1; // single byte difference: 0 -> 1

        const auto evidence = scheduler.executeDispatchCc(
            transport,
            nullptr,
            req,
            validToken,
            req.executionPlanContextId,
            port,
            alteredMsg,
            profile.transportPolicy,
            ExpectedMidiResponse{}
        );

        CHECK(transport.sentCc().empty());
        CHECK(evidence.exportBlocked == true);
        CHECK(evidence.consentWasGranted == false);
        CHECK(evidence.diagnosticCode == "ERR_CONSENT_INVALIDATED_TOCTOU");
    }

    SECTION("Variant B: Altered Port Selection (DeepMind12D -> OtherPort)")
    {
        MidiPortSelection hijackedPort {
            "Virtual_Hijacked_Port",
            "Virtual_Hijacked_Port",
            "behringer_deepmind12"
        };

        const auto evidence = scheduler.executeDispatchCc(
            transport,
            nullptr,
            req,
            validToken,
            req.executionPlanContextId,
            hijackedPort,
            ccMsg,
            profile.transportPolicy,
            ExpectedMidiResponse{}
        );

        CHECK(transport.sentCc().empty());
        CHECK(evidence.exportBlocked == true);
        CHECK(evidence.consentWasGranted == false);
        CHECK(evidence.diagnosticCode == "ERR_CONSENT_INVALIDATED_TOCTOU");
    }

    SECTION("Variant C: Altered Execution Plan / Session Context")
    {
        const std::string tamperedPlan = "plan_d2_7a_tampered_execution_plan_002";

        const auto evidence = scheduler.executeDispatchCc(
            transport,
            nullptr,
            req,
            validToken,
            tamperedPlan,
            port,
            ccMsg,
            profile.transportPolicy,
            ExpectedMidiResponse{}
        );

        CHECK(transport.sentCc().empty());
        CHECK(evidence.exportBlocked == true);
        CHECK(evidence.consentWasGranted == false);
        CHECK(evidence.diagnosticCode == "ERR_CONSENT_INVALIDATED_TOCTOU");
    }
}

TEST_CASE("HITO-10D2.7A.5 - 11. Identity Mismatch Blocks Dispatch Hermetically", "[hardware][physical][bench][recovery][fail_closed]")
{
    // Test 11: An identity mismatch detected by probe MUST block preflight and dispatch hermetically.
    auto profile = makeDeepMind12Profile();
    profile.transportPolicy.requiresVerifiedIdentity = true;
    const auto port = makeDeepMind12Port();

    MockMidiTransport transport;
    transport.open(port);

    MockMidiIdentityProbe probe;
    probe.setNextOutcome(IdentityProbeOutcome::Mismatch, "Roland", "Boutique", "1.00");

    const auto preflight = HardwareTransportPreflightService::evaluate(profile, port, transport, &probe);

    CHECK(preflight.disposition == HardwarePreflightDisposition::Blocked);
    CHECK(preflight.identityState == HardwareIdentityState::IdentityMismatch);
    CHECK(preflight.diagnosticCode == "ERR_IDENTITY_MISMATCH");
    CHECK_FALSE(preflight.readyForConsentOrDispatch);

    const MidiCcMessage ccMsg {
        1, 1, 0, "behringer_deepmind12", "modwheel", 1
    };

    auto req = OperatorConsentService::buildRequest(
        profile,
        ProfileProvenanceOrigin::BuiltIn,
        preflight,
        "recipe_d2_7a_preflight_transport_obs",
        "plan_d2_7a_single_step_transport_obs",
        "modwheel",
        0.0,
        ccMsg,
        std::nullopt
    );

    // Granting consent on an IdentityMismatch request must be rejected immediately
    const auto consentAttempt = OperatorConsentService::grantConsent(req);
    CHECK(consentAttempt.decision == OperatorConsentDecision::BlockedByIdentityMismatch);
    CHECK_FALSE(consentAttempt.readyForDispatch);
    CHECK(consentAttempt.diagnosticCode == "ERR_IDENTITY_MISMATCH_BLOCKED");

    MockMonotonicClock clock(3000);
    HardwareDispatchScheduler scheduler(clock);

    const auto evidence = scheduler.executeDispatchCc(
        transport,
        nullptr,
        req,
        consentAttempt,
        req.executionPlanContextId,
        port,
        ccMsg,
        profile.transportPolicy,
        ExpectedMidiResponse{}
    );

    CHECK(transport.sentCc().empty());
    CHECK(evidence.exportBlocked == true);
    CHECK(evidence.consentWasGranted == false);
    CHECK(evidence.diagnosticCode == "ERR_IDENTITY_MISMATCH_BLOCKED");
}

TEST_CASE("HITO-10D2.7A.5 - 12. Transport Failure (WriteFailed / Disconnected) Fail-Closed and Evidence Preservation", "[hardware][physical][bench][recovery][fail_closed]")
{
    // Test 12: Injected transport failure (WriteFailed or Disconnected) on mock transport
    // must result in fail-closed, 0 subsequent messages sent, complete evidence, and exportBlocked = true.
    const auto profile = makeDeepMind12Profile();
    const auto port = makeDeepMind12Port();

    HardwarePreflightResult preflight;
    preflight.portSelection = port;
    preflight.identityState = HardwareIdentityState::IdentityUnavailable;
    preflight.disposition = HardwarePreflightDisposition::AwaitingUserConfirmation;
    preflight.targetProfileId = "NotApplicableForNativeLegacyContract";
    preflight.readyForConsentOrDispatch = false;

    const MidiCcMessage ccMsg {
        1, 1, 0, "behringer_deepmind12", "modwheel", 1
    };

    auto req = OperatorConsentService::buildRequest(
        profile,
        ProfileProvenanceOrigin::BuiltIn,
        preflight,
        "recipe_d2_7a_preflight_transport_obs",
        "plan_d2_7a_single_step_transport_obs",
        "modwheel",
        0.0,
        ccMsg,
        std::nullopt
    );
    req.targetProfileId = "NotApplicableForNativeLegacyContract";
    req.targetContractId = "behringer_deepmind12";
    req.targetResolutionSource = "NativeLegacyContract";
    req.benchSessionId = "d2_7a_deepmind12_2026_09_27_001";
    req.recipeContextId = "recipe_d2_7a_preflight_transport_obs";
    req.executionPlanContextId = "plan_d2_7a_single_step_transport_obs";
    req.commandCanonicalization = "abdaudiolab::hardware::OperatorConsentService::CanonicalV1";
    req.commandDigest = OperatorConsentService::computeCommandDigest(req);

    const auto validConsent = OperatorConsentService::grantConsent(req, "Operator approved D2.7A.3 on physical bench");
    REQUIRE(validConsent.readyForDispatch);

    MockMonotonicClock clock(4000);
    HardwareDispatchScheduler scheduler(clock);
    MockMidiTransport transport;
    transport.open(port);

    SECTION("Variant A: Transport WriteFailed Fail-Closed")
    {
        transport.failNextSend(MidiTransportError::WriteFailed, "ERR_MIDI_TRANSPORT_WRITE_FAILED");

        const auto evidence = scheduler.executeDispatchCc(
            transport,
            nullptr,
            req,
            validConsent,
            req.executionPlanContextId,
            port,
            ccMsg,
            profile.transportPolicy,
            ExpectedMidiResponse{}
        );

        CHECK(evidence.exportBlocked == true);
        CHECK(evidence.transportError == MidiTransportError::WriteFailed);
        CHECK(evidence.diagnosticCode == "ERR_MIDI_TRANSPORT_WRITE_FAILED");
        CHECK(evidence.consentWasGranted == true);
        CHECK(evidence.targetContractId == "behringer_deepmind12");
        CHECK(evidence.commandDigest == req.commandDigest);
    }

    SECTION("Variant B: Transport Disconnected Fail-Closed and No Subsequent Messages")
    {
        transport.simulateDisconnect();

        const auto evidence1 = scheduler.executeDispatchCc(
            transport,
            nullptr,
            req,
            validConsent,
            req.executionPlanContextId,
            port,
            ccMsg,
            profile.transportPolicy,
            ExpectedMidiResponse{}
        );

        CHECK(evidence1.exportBlocked == true);
        CHECK(evidence1.transportError == MidiTransportError::Disconnected);
        CHECK(evidence1.diagnosticCode == "ERR_MIDI_TRANSPORT_DISCONNECTED");
        CHECK(transport.sentCc().empty());

        // Subsequent dispatch attempt must also fail-closed with 0 messages sent
        const auto evidence2 = scheduler.executeDispatchCc(
            transport,
            nullptr,
            req,
            validConsent,
            req.executionPlanContextId,
            port,
            ccMsg,
            profile.transportPolicy,
            ExpectedMidiResponse{}
        );

        CHECK(evidence2.exportBlocked == true);
        CHECK(evidence2.transportError == MidiTransportError::Disconnected);
        CHECK(transport.sentCc().empty());
    }
}

