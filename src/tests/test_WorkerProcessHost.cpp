#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "ipc/IpcProtocol.h"
#include "ipc/Win32NamedPipe.h"
#include "ipc/WorkerProcessHost.h"

#include <juce_core/juce_core.h>
#include <chrono>
#include <thread>

using namespace abdaudiolab::ipc;

namespace
{

juce::File getWorkerExecutableFile()
{
    // 1. Relativo al directorio de build
    juce::File buildWorker = juce::File::getCurrentWorkingDirectory()
        .getChildFile("build/Release/ABDAudioLab_PluginWorker.exe");
    if (buildWorker.existsAsFile())
        return buildWorker;

    // 2. Mismo directorio que el ejecutable de test
    juce::File exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    juce::File siblingWorker = exeDir.getChildFile("ABDAudioLab_PluginWorker.exe");
    if (siblingWorker.existsAsFile())
        return siblingWorker;

    return buildWorker;
}

} // namespace

// ==============================================================================
// Pruebas Unitarias de Protocolo Binario y Framing
// ==============================================================================
TEST_CASE("IPC Protocol: Encoding, Decoding y Seguridad de Framing Little-Endian", "[worker][ipc][protocol]")
{
    SECTION("Cabecera válida de 8 bytes")
    {
        MessageHeader h;
        h.magic = kIpcMagicHeader;
        h.type = MessageType::HandshakeRequest;
        h.payloadSize = 128;

        auto bytes = h.serialize();
        REQUIRE(bytes.size() == kIpcHeaderSize);

        auto opt = MessageHeader::deserialize(bytes.data(), bytes.size());
        REQUIRE(opt.has_value());
        CHECK(opt->magic == kIpcMagicHeader);
        CHECK(opt->type == MessageType::HandshakeRequest);
        CHECK(opt->payloadSize == 128);
    }

    SECTION("Rechazo de Magic Header corrupto o ajeno")
    {
        MessageHeader h;
        h.magic = 0xDEADBEEF; // Magic corrupto
        h.type = MessageType::Ping;
        h.payloadSize = 16;

        auto bytes = h.serialize();
        auto opt = MessageHeader::deserialize(bytes.data(), bytes.size());
        CHECK_FALSE(opt.has_value());
    }

    SECTION("HandshakePayload round-trip con Nonce de 64 bits")
    {
        HandshakePayload req;
        req.protocolMajor = 1;
        req.protocolMinor = 0;
        req.pid = 12345;
        req.nonce = 0xFEEDFACECAFEBABEULL;
        req.sessionId = "session_test_abc123";
        req.architecture = "x86_64";
        req.capabilities = 0x0001;

        auto bytes = req.serialize();
        auto opt = HandshakePayload::deserialize(bytes.data(), bytes.size());
        REQUIRE(opt.has_value());
        CHECK(opt->protocolMajor == 1);
        CHECK(opt->protocolMinor == 0);
        CHECK(opt->pid == 12345);
        CHECK(opt->nonce == 0xFEEDFACECAFEBABEULL);
        CHECK(opt->sessionId == "session_test_abc123");
        CHECK(opt->architecture == "x86_64");
        CHECK(opt->capabilities == 0x0001);
    }

    SECTION("Ping / Pong Payload round-trip")
    {
        PingPongPayload p;
        p.sequence = 42;
        p.timestampMs = 987654321ULL;

        auto bytes = p.serialize();
        auto opt = PingPongPayload::deserialize(bytes.data(), bytes.size());
        REQUIRE(opt.has_value());
        CHECK(opt->sequence == 42);
        CHECK(opt->timestampMs == 987654321ULL);
    }
}

// ==============================================================================
// Pruebas Adversariales de Resiliencia del Host
// ==============================================================================
TEST_CASE("WorkerProcessHost: Resiliencia ante worker inexistente o timeout", "[worker][ipc][safety]")
{
    WorkerProcessHost host;

    SECTION("Worker inexistente falla limpiamente sin colgar ni crashear")
    {
        bool ok = host.spawnAndConnect("C:/Ruta/Ficticia/Inexistente.exe", "session_fake", 0x1234, 1000);
        CHECK_FALSE(ok);
        CHECK_FALSE(host.isWorkerAlive());
        CHECK(host.getState() == HostState::Uninitialized);
        CHECK_FALSE(host.getLastError().empty());
    }
}

// ==============================================================================
// Pruebas de Integración con el Proceso Esclavo Real (Slice 1)
// ==============================================================================
TEST_CASE("WorkerProcessHost: Spawn, Handshake, Ping/Pong y Cierre Ordenado", "[worker][ipc][e2e]")
{
    auto workerFile = getWorkerExecutableFile();
    if (!workerFile.existsAsFile())
    {
        SKIP("ABDAudioLab_PluginWorker.exe no compilado aún; compile primero.");
    }

    WorkerProcessHost host;
    std::string sessionId = "test_slice1_" + std::to_string(juce::Time::currentTimeMillis());
    uint64_t nonce = 0xABCD1234DEADBEEFULL;

    // 1. Spawn y Handshake mutuo
    bool connected = host.spawnAndConnect(workerFile.getFullPathName().toStdString(), sessionId, nonce, 5000);
    REQUIRE(connected);
    REQUIRE(host.isWorkerAlive());
    REQUIRE(host.getState() == HostState::Ready);
    REQUIRE(host.getWorkerPid() != 0);

    // 2. Ping / Pong secuencial (Liveness)
    CHECK(host.sendPing(1, 2000) == true);
    CHECK(host.sendPing(2, 2000) == true);
    CHECK(host.sendPing(3, 2000) == true);

    // 3. Cierre ordenado por protocolo (ShutdownRequest -> ShutdownResponse -> exit(0))
    bool cleanShutdown = host.orderlyShutdown(3000);
    CHECK(cleanShutdown == true);
    CHECK_FALSE(host.isWorkerAlive());
    CHECK(host.getState() == HostState::Terminated);
}

TEST_CASE("WorkerProcessHost: Detección de Crash del Worker sin Afectar al Host", "[worker][ipc][safety]")
{
    auto workerFile = getWorkerExecutableFile();
    if (!workerFile.existsAsFile())
    {
        SKIP("ABDAudioLab_PluginWorker.exe no compilado aún; compile primero.");
    }

    WorkerProcessHost host;
    std::string sessionId = "test_crash_" + std::to_string(juce::Time::currentTimeMillis());
    uint64_t nonce = 0x9988776655443322ULL;

    REQUIRE(host.spawnAndConnect(workerFile.getFullPathName().toStdString(), sessionId, nonce, 5000));
    REQUIRE(host.isWorkerAlive());

    // Inyectar fallo crítico de memoria (Access Violation) en el worker
    CHECK(host.requestSimulatedCrash() == true);

    // Esperar hasta 4 segundos a que el sistema operativo limpie el proceso worker caído
    auto tStart = std::chrono::steady_clock::now();
    while (host.isWorkerAlive())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - tStart).count();
        if (elapsed > 4000) break;
    }

    // EL HOST DEBE SEGUIR 100% VIVO, mientras que el worker debe haber muerto
    CHECK_FALSE(host.isWorkerAlive());

    // Llamadas posteriores deben ser rechazadas limpiamente
    CHECK_FALSE(host.sendPing(10, 500));
}
