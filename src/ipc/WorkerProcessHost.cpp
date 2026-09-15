#include "WorkerProcessHost.h"
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>

namespace abdaudiolab::ipc
{

WorkerProcessHost::WorkerProcessHost() = default;

WorkerProcessHost::~WorkerProcessHost()
{
    if (isWorkerAlive())
    {
        orderlyShutdown(1000);
    }
    cleanupProcessHandles();
}

void WorkerProcessHost::cleanupProcessHandles()
{
#if defined(_WIN32)
    if (hThread_ != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hThread_);
        hThread_ = INVALID_HANDLE_VALUE;
    }
    if (hProcess_ != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hProcess_);
        hProcess_ = INVALID_HANDLE_VALUE;
    }
#endif
    state_ = HostState::Terminated;
}

bool WorkerProcessHost::isWorkerAlive() const noexcept
{
#if defined(_WIN32)
    if (hProcess_ == INVALID_HANDLE_VALUE) return false;
    DWORD exitCode = 0;
    if (GetExitCodeProcess(hProcess_, &exitCode))
    {
        return exitCode == STILL_ACTIVE;
    }
    return false;
#else
    return false;
#endif
}

void WorkerProcessHost::forceTerminate()
{
#if defined(_WIN32)
    if (isWorkerAlive())
    {
        TerminateProcess(hProcess_, 1);
        WaitForSingleObject(hProcess_, 1000);
    }
    pipeServer_.disconnect();
    cleanupProcessHandles();
#endif
}

bool WorkerProcessHost::spawnAndConnect(const std::string& workerExePath,
                                       const std::string& sessionId,
                                       uint64_t nonce,
                                       int timeoutMs)
{
    lastError_.clear();
    sessionId_ = sessionId;
    sessionNonce_ = nonce;

#if defined(_WIN32)
    uint32_t hostPid = GetCurrentProcessId();
    pipeName_ = generateSecurePipeName(sessionId, nonce, hostPid);

    // 1. Crear el Named Pipe Server antes de lanzar el worker
    if (!pipeServer_.create(pipeName_, timeoutMs))
    {
        lastError_ = "Error al crear Named Pipe en el Host (Win32 err: " + std::to_string(GetLastError()) + ")";
        return false;
    }

    // 2. Construir línea de comandos y lanzar el worker
    std::ostringstream cmd;
    cmd << "\"" << workerExePath << "\" --pipe \"" << pipeName_ << "\" --session \"" << sessionId
        << "\" --nonce " << std::hex << nonce << " --hostPid " << std::dec << hostPid;

    std::string cmdStr = cmd.str();
    std::vector<char> cmdBuf(cmdStr.begin(), cmdStr.end());
    cmdBuf.push_back('\0');

    STARTUPINFOA si = {};
    si.cb = sizeof(STARTUPINFOA);
    PROCESS_INFORMATION pi = {};

    BOOL created = CreateProcessA(
        nullptr,
        cmdBuf.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi
    );

    if (!created)
    {
        pipeServer_.disconnect();
        lastError_ = "No se pudo spawnear el proceso worker: " + workerExePath + " (Win32 err: " + std::to_string(GetLastError()) + ")";
        return false;
    }

    hProcess_ = pi.hProcess;
    hThread_ = pi.hThread;
    workerPid_ = pi.dwProcessId;
    state_ = HostState::Listening;

    // 3. Esperar que el worker se conecte al pipe
    if (!pipeServer_.waitForClientConnection(timeoutMs))
    {
        lastError_ = "Timeout esperando conexión del worker al Named Pipe (" + std::to_string(timeoutMs) + " ms)";
        forceTerminate();
        return false;
    }

    state_ = HostState::ConnectedHandshaking;

    // 4. Realizar handshake mutuo con validación estricta de nonce, sessionId y protocolo
    HandshakePayload req;
    req.protocolMajor = kIpcProtocolMajor;
    req.protocolMinor = kIpcProtocolMinor;
    req.pid = hostPid;
    req.nonce = nonce;
    req.sessionId = sessionId;
    req.architecture = (sizeof(void*) == 8) ? "x86_64" : "x86";
    req.capabilities = 0x0001; // Capacidad base VST3 IPC

    if (!pipeServer_.sendMessage(MessageType::HandshakeRequest, req.serialize()))
    {
        lastError_ = "Error al enviar HandshakeRequest al worker";
        forceTerminate();
        return false;
    }

    MessageType respType = MessageType::Invalid;
    std::vector<uint8_t> respPayload;
    if (!pipeServer_.receiveMessage(respType, respPayload, timeoutMs))
    {
        lastError_ = "Timeout esperando HandshakeResponse del worker";
        forceTerminate();
        return false;
    }

    if (respType != MessageType::HandshakeResponse)
    {
        lastError_ = "Respuesta de handshake inesperada: tipo " + std::to_string(static_cast<int>(respType));
        forceTerminate();
        return false;
    }

    auto optResp = HandshakePayload::deserialize(respPayload.data(), respPayload.size());
    if (!optResp)
    {
        lastError_ = "Payload de HandshakeResponse malformado o corrupto";
        forceTerminate();
        return false;
    }

    if (optResp->protocolMajor != kIpcProtocolMajor)
    {
        lastError_ = "Incompatibilidad de protocolo mayor: esperado " + std::to_string(kIpcProtocolMajor)
                   + ", recibido " + std::to_string(optResp->protocolMajor);
        forceTerminate();
        return false;
    }

    if (optResp->nonce != nonce)
    {
        lastError_ = "Rechazo de seguridad: nonce no coincide con la sesión generada";
        forceTerminate();
        return false;
    }

    if (optResp->sessionId != sessionId)
    {
        lastError_ = "Rechazo de seguridad: sessionId devuelto no coincide";
        forceTerminate();
        return false;
    }

    workerPid_ = optResp->pid;
    state_ = HostState::Ready;
    return true;
#else
    lastError_ = "IPC de proceso no soportado en plataforma no-Windows";
    return false;
#endif
}

bool WorkerProcessHost::sendPing(uint64_t sequence, int timeoutMs)
{
    if (!isWorkerAlive() || state_ != HostState::Ready) return false;

    PingPongPayload ping;
    ping.sequence = sequence;
    ping.timestampMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());

    if (!pipeServer_.sendMessage(MessageType::Ping, ping.serialize()))
    {
        lastError_ = "Fallo al enviar Ping al worker";
        return false;
    }

    MessageType respType = MessageType::Invalid;
    std::vector<uint8_t> respPayload;
    if (!pipeServer_.receiveMessage(respType, respPayload, timeoutMs))
    {
        lastError_ = "Timeout o error esperando Pong del worker";
        return false;
    }

    if (respType != MessageType::Pong)
    {
        lastError_ = "Respuesta inesperada al Ping: tipo " + std::to_string(static_cast<int>(respType));
        return false;
    }

    auto optPong = PingPongPayload::deserialize(respPayload.data(), respPayload.size());
    if (!optPong || optPong->sequence != sequence)
    {
        lastError_ = "Pong inválido o número de secuencia desalineado";
        return false;
    }

    return true;
}

bool WorkerProcessHost::orderlyShutdown(int timeoutMs)
{
    if (!isWorkerAlive())
    {
        state_ = HostState::Terminated;
        cleanupProcessHandles();
        return true;
    }

    state_ = HostState::ShuttingDown;

    ShutdownPayload shutdown;
    shutdown.reasonCode = 0;
    shutdown.reasonText = "Host normal shutdown request";

    if (pipeServer_.isConnected())
    {
        pipeServer_.sendMessage(MessageType::ShutdownRequest, shutdown.serialize());

        MessageType respType = MessageType::Invalid;
        std::vector<uint8_t> respPayload;
        pipeServer_.receiveMessage(respType, respPayload, 1000);
    }

    pipeServer_.disconnect();

#if defined(_WIN32)
    if (hProcess_ != INVALID_HANDLE_VALUE)
    {
        DWORD waitRes = WaitForSingleObject(hProcess_, static_cast<DWORD>(timeoutMs));
        if (waitRes != WAIT_OBJECT_0)
        {
            // Worker no salió en el tiempo límite: aplicar TerminateProcess como último recurso
            forceTerminate();
            return false;
        }
    }
#endif

    cleanupProcessHandles();
    return true;
}

bool WorkerProcessHost::requestSimulatedCrash()
{
    if (!isWorkerAlive()) return false;
    return pipeServer_.sendMessage(MessageType::SimulateCrashRequest, {});
}

bool WorkerProcessHost::requestSimulatedHang()
{
    if (!isWorkerAlive()) return false;
    return pipeServer_.sendMessage(MessageType::SimulateHangRequest, {});
}

bool WorkerProcessHost::loadPlugin(const LoadPluginRequestPayload& req,
                                  LoadPluginResponsePayload& outResp,
                                  int timeoutMs)
{
    if (!isWorkerAlive() || state_ != HostState::Ready)
    {
        lastError_ = "Worker no está vivo o no está en estado Ready";
        return false;
    }

    if (!pipeServer_.sendMessage(MessageType::LoadPluginRequest, req.serialize()))
    {
        lastError_ = "Fallo al enviar LoadPluginRequest al worker";
        return false;
    }

    MessageType respType = MessageType::Invalid;
    std::vector<uint8_t> respPayload;
    if (!pipeServer_.receiveMessage(respType, respPayload, timeoutMs))
    {
        lastError_ = "Timeout esperando LoadPluginResponse del worker";
        return false;
    }

    if (respType != MessageType::LoadPluginResponse)
    {
        lastError_ = "Respuesta inesperada a LoadPlugin: tipo " + std::to_string(static_cast<int>(respType));
        return false;
    }

    auto optResp = LoadPluginResponsePayload::deserialize(respPayload.data(), respPayload.size());
    if (!optResp)
    {
        lastError_ = "Payload de LoadPluginResponse malformado";
        return false;
    }

    outResp = *optResp;
    if (outResp.status != 0)
    {
        lastError_ = outResp.errorMessage.empty() ? "Error desconocido devuelto por worker al cargar plugin" : outResp.errorMessage;
        return false;
    }

    return true;
}

bool WorkerProcessHost::queryContract(QueryContractResponsePayload& outResp,
                                      int timeoutMs)
{
    if (!isWorkerAlive() || state_ != HostState::Ready)
    {
        lastError_ = "Worker no está vivo o no está en estado Ready";
        return false;
    }

    QueryContractRequestPayload req;
    if (!pipeServer_.sendMessage(MessageType::QueryContractRequest, req.serialize()))
    {
        lastError_ = "Fallo al enviar QueryContractRequest al worker";
        return false;
    }

    MessageType respType = MessageType::Invalid;
    std::vector<uint8_t> respPayload;
    if (!pipeServer_.receiveMessage(respType, respPayload, timeoutMs))
    {
        lastError_ = "Timeout esperando QueryContractResponse del worker";
        return false;
    }

    if (respType == MessageType::ErrorResponse)
    {
        auto optErr = ErrorPayload::deserialize(respPayload.data(), respPayload.size());
        lastError_ = optErr ? optErr->errorMessage : "Error devuelto por worker en QueryContract";
        return false;
    }

    if (respType != MessageType::QueryContractResponse)
    {
        lastError_ = "Respuesta inesperada a QueryContract: tipo " + std::to_string(static_cast<int>(respType));
        return false;
    }

    auto optResp = QueryContractResponsePayload::deserialize(respPayload.data(), respPayload.size());
    if (!optResp)
    {
        lastError_ = "Payload de QueryContractResponse malformado";
        return false;
    }

    outResp = *optResp;
    return true;
}

bool WorkerProcessHost::resetPlugin(uint32_t trialIndex, int timeoutMs)
{
    if (!isWorkerAlive() || state_ != HostState::Ready)
    {
        lastError_ = "Worker no está vivo o no está en estado Ready";
        return false;
    }

    ResetPluginPayload req;
    req.trialIndex = trialIndex;

    if (!pipeServer_.sendMessage(MessageType::ResetPluginRequest, req.serialize()))
    {
        lastError_ = "Fallo al enviar ResetPluginRequest al worker";
        return false;
    }

    MessageType respType = MessageType::Invalid;
    std::vector<uint8_t> respPayload;
    if (!pipeServer_.receiveMessage(respType, respPayload, timeoutMs))
    {
        lastError_ = "Timeout esperando ResetPluginResponse del worker";
        return false;
    }

    if (respType == MessageType::ErrorResponse)
    {
        auto optErr = ErrorPayload::deserialize(respPayload.data(), respPayload.size());
        lastError_ = optErr ? optErr->errorMessage : "Error devuelto por worker en ResetPlugin";
        return false;
    }

    if (respType != MessageType::ResetPluginResponse)
    {
        lastError_ = "Respuesta inesperada a ResetPlugin: tipo " + std::to_string(static_cast<int>(respType));
        return false;
    }

    return true;
}

bool WorkerProcessHost::releasePlugin(int timeoutMs)
{
    if (!isWorkerAlive() || state_ != HostState::Ready)
    {
        return true; // Idempotente si el worker ya no está activo
    }

    ReleasePluginPayload req;
    if (!pipeServer_.sendMessage(MessageType::ReleasePluginRequest, req.serialize()))
    {
        lastError_ = "Fallo al enviar ReleasePluginRequest al worker";
        return false;
    }

    MessageType respType = MessageType::Invalid;
    std::vector<uint8_t> respPayload;
    if (!pipeServer_.receiveMessage(respType, respPayload, timeoutMs))
    {
        lastError_ = "Timeout esperando ReleasePluginResponse del worker";
        return false;
    }

    return respType == MessageType::ReleasePluginResponse;
}

bool WorkerProcessHost::renderBlock(const RenderBlockRequestPayload& req,
                                    RenderBlockResponsePayload& outResp,
                                    int timeoutMs)
{
    if (!isWorkerAlive() || state_ != HostState::Ready)
    {
        lastError_ = "Worker no conectado o no listo para renderizar bloque";
        return false;
    }

    if (!pipeServer_.sendMessage(MessageType::RenderBlockRequest, req.serialize()))
    {
        lastError_ = "Fallo al enviar RenderBlockRequest al worker";
        return false;
    }

    MessageType respType = MessageType::Invalid;
    std::vector<uint8_t> respPayload;
    if (!pipeServer_.receiveMessage(respType, respPayload, timeoutMs))
    {
        lastError_ = "Timeout (" + std::to_string(timeoutMs) + " ms) esperando RenderBlockResponse del worker";
        return false;
    }

    if (respType == MessageType::ErrorResponse)
    {
        auto optErr = ErrorPayload::deserialize(respPayload.data(), respPayload.size());
        lastError_ = optErr ? optErr->errorMessage : "Error estructurado devuelto por worker en RenderBlock";
        return false;
    }

    if (respType != MessageType::RenderBlockResponse)
    {
        lastError_ = "Respuesta inesperada a RenderBlock: tipo " + std::to_string(static_cast<int>(respType));
        return false;
    }

    auto optResp = RenderBlockResponsePayload::deserialize(respPayload.data(), respPayload.size());
    if (!optResp)
    {
        lastError_ = "Payload RenderBlockResponse malformado o límites de audio excedidos";
        return false;
    }

    outResp = *optResp;

    if (outResp.sequence != req.sequence)
    {
        lastError_ = "Secuencia de RenderBlock inconsistente: enviada " + std::to_string(req.sequence)
                   + ", recibida " + std::to_string(outResp.sequence);
        return false;
    }

    if (outResp.status != 0)
    {
        lastError_ = outResp.errorMessage.empty()
            ? ("Error de render reportado por worker: código " + std::to_string(outResp.errorCode))
            : outResp.errorMessage;
        return false;
    }

    return true;
}

} // namespace abdaudiolab::ipc
