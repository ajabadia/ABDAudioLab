#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <memory>
#include "ipc/IpcProtocol.h"
#include "ipc/Win32NamedPipe.h"
#include "synth/ExternalPluginFixture.h"
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_processors/juce_audio_processors.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

using namespace abdaudiolab::ipc;
using namespace abdaudiolab::synth;

enum class WorkerState
{
    Idle,
    Loading,
    Loaded,
    Prepared,
    Resetting,
    Failed,
    Releasing
};

int main(int argc, char* argv[])
{
#if defined(_WIN32)
    // Deshabilitar ventanas emergentes modales de Windows Error Reporting (WER)
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif

    // Inicializador GUI / COM de JUCE para el hilo principal del worker (requerido por VST3)
    juce::ScopedJuceInitialiser_GUI juceInit;

    std::string pipeName;
    std::string expectedSessionId;
    uint64_t expectedNonce = 0;
    uint32_t expectedHostPid = 0;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--pipe" && i + 1 < argc)
        {
            pipeName = argv[++i];
        }
        else if (arg == "--session" && i + 1 < argc)
        {
            expectedSessionId = argv[++i];
        }
        else if (arg == "--nonce" && i + 1 < argc)
        {
            expectedNonce = std::stoull(argv[++i], nullptr, 16);
        }
        else if (arg == "--hostPid" && i + 1 < argc)
        {
            expectedHostPid = static_cast<uint32_t>(std::stoul(argv[++i]));
        }
    }

    if (pipeName.empty())
    {
        return 1;
    }

    NamedPipeClient pipeClient;
    if (!pipeClient.connectToServer(pipeName, 5000))
    {
        return 2;
    }

    // 1. Fase de Handshake
    MessageType reqType = MessageType::Invalid;
    std::vector<uint8_t> reqPayload;
    if (!pipeClient.receiveMessage(reqType, reqPayload, 5000))
    {
        return 3;
    }

    if (reqType != MessageType::HandshakeRequest)
    {
        return 4;
    }

    auto optHandshakeReq = HandshakePayload::deserialize(reqPayload.data(), reqPayload.size());
    if (!optHandshakeReq)
    {
        return 5;
    }

    // Validación estricta de seguridad: nonce y sesión
    if (optHandshakeReq->protocolMajor != kIpcProtocolMajor ||
        optHandshakeReq->nonce != expectedNonce ||
        optHandshakeReq->sessionId != expectedSessionId)
    {
        ErrorPayload err;
        err.errorCode = 403;
        err.errorMessage = "Handshake validation failed: session/nonce mismatch";
        pipeClient.sendMessage(MessageType::ErrorResponse, err.serialize());
        return 6;
    }

    // Responder HandshakeResponse
    HandshakePayload resp;
    resp.protocolMajor = kIpcProtocolMajor;
    resp.protocolMinor = kIpcProtocolMinor;
#if defined(_WIN32)
    resp.pid = GetCurrentProcessId();
#else
    resp.pid = 0;
#endif
    resp.nonce = expectedNonce;
    resp.sessionId = expectedSessionId;
    resp.architecture = (sizeof(void*) == 8) ? "x86_64" : "x86";
    resp.capabilities = 0x0003; // Base VST3 IPC + Slice 2 Lifecycle

    if (!pipeClient.sendMessage(MessageType::HandshakeResponse, resp.serialize()))
    {
        return 7;
    }

    // 2. Recursos de Hosting y Máquina de Estados del Worker
    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    std::unique_ptr<ExternalPluginFixture> currentFixture;
    WorkerState state = WorkerState::Idle;

    // 3. Bucle principal de control
    bool running = true;
    while (running && pipeClient.isConnected())
    {
        MessageType msgType = MessageType::Invalid;
        std::vector<uint8_t> msgPayload;

        if (!pipeClient.receiveMessage(msgType, msgPayload, 10000))
        {
            if (!pipeClient.isConnected())
            {
                break;
            }
            continue;
        }

        switch (msgType)
        {
            case MessageType::Ping:
            {
                auto optPing = PingPongPayload::deserialize(msgPayload.data(), msgPayload.size());
                if (optPing)
                {
                    PingPongPayload pong;
                    pong.sequence = optPing->sequence;
                    pong.timestampMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count());
                    pipeClient.sendMessage(MessageType::Pong, pong.serialize());
                }
                break;
            }

            case MessageType::LoadPluginRequest:
            {
                auto optReq = LoadPluginRequestPayload::deserialize(msgPayload.data(), msgPayload.size());
                if (!optReq)
                {
                    LoadPluginResponsePayload respLoad;
                    respLoad.status = 1;
                    respLoad.errorCode = 400;
                    respLoad.errorMessage = "Malformed LoadPluginRequest payload";
                    pipeClient.sendMessage(MessageType::LoadPluginResponse, respLoad.serialize());
                    break;
                }

                // Regla de estado: si ya hay un plugin cargado, exigir Release previo
                if (currentFixture != nullptr && state == WorkerState::Loaded)
                {
                    LoadPluginResponsePayload respLoad;
                    respLoad.status = 1;
                    respLoad.errorCode = 409;
                    respLoad.errorMessage = "A plugin is already loaded in worker; call ReleasePlugin first";
                    pipeClient.sendMessage(MessageType::LoadPluginResponse, respLoad.serialize());
                    break;
                }

                state = WorkerState::Loading;
                juce::File pluginFile(optReq->pluginPath);

                if (!pluginFile.exists())
                {
                    state = WorkerState::Failed;
                    LoadPluginResponsePayload respLoad;
                    respLoad.status = 1;
                    respLoad.errorCode = 404;
                    respLoad.errorMessage = "Plugin file not found: " + optReq->pluginPath;
                    pipeClient.sendMessage(MessageType::LoadPluginResponse, respLoad.serialize());
                    break;
                }

                auto fixture = std::make_unique<ExternalPluginFixture>(formatManager);
                std::string errStr;
                bool ok = fixture->loadPluginFromDisk(pluginFile, optReq->sampleRate, static_cast<int>(optReq->blockSize), errStr);

                if (!ok)
                {
                    state = WorkerState::Failed;
                    LoadPluginResponsePayload respLoad;
                    respLoad.status = 1;
                    respLoad.errorCode = 500;
                    respLoad.errorMessage = "Failed to load plugin: " + errStr;
                    pipeClient.sendMessage(MessageType::LoadPluginResponse, respLoad.serialize());
                    break;
                }

                const auto& id = fixture->getIdentity();

                // Validación estricta de hash SHA-256 binario si fue suministrado
                if (!optReq->expectedBinarySha256.empty() && !id.binaryHash.empty())
                {
                    if (id.binaryHash != optReq->expectedBinarySha256)
                    {
                        state = WorkerState::Failed;
                        LoadPluginResponsePayload respLoad;
                        respLoad.status = 1;
                        respLoad.errorCode = 422; // Integrity Mismatch / BinaryChanged
                        respLoad.errorMessage = "Binary SHA-256 mismatch: expected " + optReq->expectedBinarySha256
                                              + ", found " + id.binaryHash;
                        pipeClient.sendMessage(MessageType::LoadPluginResponse, respLoad.serialize());
                        break;
                    }
                }

                currentFixture = std::move(fixture);
                state = WorkerState::Loaded;

                LoadPluginResponsePayload respLoad;
                respLoad.status = 0;
                respLoad.errorCode = 0;
                respLoad.errorMessage = "";
                respLoad.pluginFormat = id.format;
                respLoad.pluginUid = id.pluginUid;
                respLoad.vendor = id.manufacturer;
                respLoad.name = id.pluginName;
                respLoad.version = id.version;
                respLoad.binarySha256 = id.binaryHash;
                respLoad.capabilities = 0x0001;
                respLoad.executionMode = "OutOfProcessVST3";

                pipeClient.sendMessage(MessageType::LoadPluginResponse, respLoad.serialize());
                break;
            }

            case MessageType::QueryContractRequest:
            {
                if (currentFixture == nullptr || state != WorkerState::Loaded)
                {
                    ErrorPayload err;
                    err.errorCode = 412;
                    err.errorMessage = "Cannot query contract: no plugin loaded in worker";
                    pipeClient.sendMessage(MessageType::ErrorResponse, err.serialize());
                    break;
                }

                const auto& id = currentFixture->getIdentity();
                QueryContractResponsePayload respContract;
                respContract.targetId = id.pluginUid.empty() ? id.pluginName : id.pluginUid;
                respContract.inputChannels = 0;  // Generador / Sintetizador
                respContract.outputChannels = 2; // Estéreo nominal
                respContract.numParameters = 10; // Nominal VST3
                respContract.supportsMidi = 1;
                respContract.supportsNativeGui = 1;
                respContract.requiresResetBetweenTrials = 1;
                respContract.settlingTimeMs = 50;
                respContract.determinism = "DeterministicAfterReset";
                respContract.latencySamples = 0;

                pipeClient.sendMessage(MessageType::QueryContractResponse, respContract.serialize());
                break;
            }

            case MessageType::ResetPluginRequest:
            {
                if (currentFixture == nullptr || state != WorkerState::Loaded)
                {
                    ErrorPayload err;
                    err.errorCode = 412;
                    err.errorMessage = "Cannot reset: no plugin loaded in worker";
                    pipeClient.sendMessage(MessageType::ErrorResponse, err.serialize());
                    break;
                }

                state = WorkerState::Resetting;
                currentFixture->resetState();
                state = WorkerState::Loaded;

                ResetPluginPayload respReset;
                respReset.trialIndex = 0;
                pipeClient.sendMessage(MessageType::ResetPluginResponse, respReset.serialize());
                break;
            }

            case MessageType::ReleasePluginRequest:
            {
                state = WorkerState::Releasing;
                if (currentFixture != nullptr)
                {
                    currentFixture.reset();
                }
                state = WorkerState::Idle;

                ReleasePluginPayload respRelease;
                respRelease.flags = 0;
                pipeClient.sendMessage(MessageType::ReleasePluginResponse, respRelease.serialize());
                break;
            }

            case MessageType::RenderBlockRequest:
            {
                auto optReq = RenderBlockRequestPayload::deserialize(msgPayload.data(), msgPayload.size());
                uint64_t reqSeq = optReq ? optReq->sequence : 0;

                if (currentFixture == nullptr || state != WorkerState::Loaded)
                {
                    RenderBlockResponsePayload respErr;
                    respErr.sequence = reqSeq;
                    respErr.status = 1;
                    respErr.errorCode = 412; // Precondition Failed: Plugin not loaded
                    respErr.errorMessage = "Cannot render block: no plugin loaded in worker";
                    pipeClient.sendMessage(MessageType::RenderBlockResponse, respErr.serialize());
                    break;
                }

                if (!optReq)
                {
                    RenderBlockResponsePayload respErr;
                    respErr.sequence = 0;
                    respErr.status = 1;
                    respErr.errorCode = 400; // Bad Request: Framing or parameter bounds illegal
                    respErr.errorMessage = "Malformed RenderBlockRequest payload";
                    pipeClient.sendMessage(MessageType::RenderBlockResponse, respErr.serialize());
                    break;
                }

                auto* instance = currentFixture->getPluginInstance();
                if (instance == nullptr)
                {
                    RenderBlockResponsePayload respErr;
                    respErr.sequence = optReq->sequence;
                    respErr.status = 1;
                    respErr.errorCode = 500;
                    respErr.errorMessage = "AudioProcessor instance is null";
                    pipeClient.sendMessage(MessageType::RenderBlockResponse, respErr.serialize());
                    break;
                }

                const int reqBlockSize = static_cast<int>(optReq->blockSize);
                const int reqChannels = static_cast<int>(optReq->numChannels);

                juce::AudioBuffer<float> blockBuf(reqChannels, reqBlockSize);
                blockBuf.clear();

                juce::MidiBuffer midiBuf;
                for (const auto& ev : optReq->midiEvents)
                {
                    juce::MidiMessage msg;
                    if (ev.type == 0) // NoteOn
                        msg = juce::MidiMessage::noteOn(ev.channel, static_cast<int>(ev.noteNumber), static_cast<float>(ev.velocity) / 127.0f);
                    else if (ev.type == 1) // NoteOff
                        msg = juce::MidiMessage::noteOff(ev.channel, static_cast<int>(ev.noteNumber), 0.0f);
                    else if (ev.type == 2) // AllNotesOff
                        msg = juce::MidiMessage::allNotesOff(ev.channel);

                    if (msg.getRawDataSize() > 0)
                    {
                        int offset = std::clamp(static_cast<int>(ev.sampleOffset), 0, reqBlockSize - 1);
                        midiBuf.addEvent(msg, offset);
                    }
                }

                auto tStart = std::chrono::steady_clock::now();
                try
                {
                    instance->processBlock(blockBuf, midiBuf);
                }
                catch (const std::exception& e)
                {
                    RenderBlockResponsePayload respErr;
                    respErr.sequence = optReq->sequence;
                    respErr.status = 1;
                    respErr.errorCode = 500;
                    respErr.errorMessage = "Exception in processBlock: " + std::string(e.what());
                    pipeClient.sendMessage(MessageType::RenderBlockResponse, respErr.serialize());
                    break;
                }
                catch (...)
                {
                    RenderBlockResponsePayload respErr;
                    respErr.sequence = optReq->sequence;
                    respErr.status = 1;
                    respErr.errorCode = 500;
                    respErr.errorMessage = "Unknown non-standard exception in processBlock";
                    pipeClient.sendMessage(MessageType::RenderBlockResponse, respErr.serialize());
                    break;
                }
                auto tEnd = std::chrono::steady_clock::now();
                uint64_t durationUs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(tEnd - tStart).count());

                RenderBlockResponsePayload respRender;
                respRender.sequence = optReq->sequence;
                respRender.status = 0;
                respRender.errorCode = 0;
                respRender.errorMessage = "";
                respRender.samplesRendered = optReq->blockSize;
                respRender.numChannels = optReq->numChannels;
                respRender.durationUs = durationUs;

                // Serializar audio (interleaved: sample0_ch0, sample0_ch1, ...)
                respRender.audioData.resize(static_cast<size_t>(reqBlockSize * reqChannels));
                for (int ch = 0; ch < reqChannels; ++ch)
                {
                    const float* rptr = blockBuf.getReadPointer(ch);
                    for (int s = 0; s < reqBlockSize; ++s)
                    {
                        respRender.audioData[static_cast<size_t>(s * reqChannels + ch)] = rptr[s];
                    }
                }

                pipeClient.sendMessage(MessageType::RenderBlockResponse, respRender.serialize());
                break;
            }

            case MessageType::ShutdownRequest:
            {
                ShutdownPayload respShutdown;
                respShutdown.reasonCode = 0;
                respShutdown.reasonText = "Worker clean shutdown ack";
                pipeClient.sendMessage(MessageType::ShutdownResponse, respShutdown.serialize());
                running = false;
                break;
            }

            case MessageType::SimulateCrashRequest:
            {
                // Provocar intencionalmente violación de acceso de memoria para tests adversariales
#if defined(_WIN32)
                RaiseException(EXCEPTION_ACCESS_VIOLATION, EXCEPTION_NONCONTINUABLE, 0, nullptr);
#else
                std::abort();
#endif
                break;
            }

            case MessageType::SimulateHangRequest:
            {
                // Bucle infinito sin cooperar para tests de watchdog/timeout
                while (true)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                break;
            }

            default:
            {
                ErrorPayload err;
                err.errorCode = 400;
                err.errorMessage = "Unsupported message type in Slice 2: " + std::to_string(static_cast<int>(msgType));
                pipeClient.sendMessage(MessageType::ErrorResponse, err.serialize());
                break;
            }
        }
    }

    if (currentFixture != nullptr)
    {
        currentFixture.reset();
    }

    pipeClient.disconnect();
    return 0;
}
