#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <optional>
#include "IpcProtocol.h"
#include "Win32NamedPipe.h"

namespace abdaudiolab::ipc
{

enum class HostState
{
    Uninitialized,
    Listening,
    ConnectedHandshaking,
    Ready,
    ShuttingDown,
    Terminated
};

class WorkerProcessHost
{
public:
    WorkerProcessHost();
    ~WorkerProcessHost();

    WorkerProcessHost(const WorkerProcessHost&) = delete;
    WorkerProcessHost& operator=(const WorkerProcessHost&) = delete;

    bool spawnAndConnect(const std::string& workerExePath,
                         const std::string& sessionId,
                         uint64_t nonce,
                         int timeoutMs = 5000);

    bool sendPing(uint64_t sequence, int timeoutMs = 2000);

    // Cierre ordenado por protocolo: ShutdownRequest -> ShutdownResponse -> esperar salida -> TerminateProcess solo si timeout
    bool orderlyShutdown(int timeoutMs = 3000);

    // Fuerza terminación inmediata (para crashes o cuando el worker deja de responder)
    void forceTerminate();

    [[nodiscard]] bool isWorkerAlive() const noexcept;
    [[nodiscard]] uint32_t getWorkerPid() const noexcept { return workerPid_; }
    [[nodiscard]] HostState getState() const noexcept { return state_; }
    [[nodiscard]] const std::string& getLastError() const noexcept { return lastError_; }

    // Operaciones remotas del Vertical Slice 2 (Carga, Contrato y Lifecycle)
    bool loadPlugin(const LoadPluginRequestPayload& req,
                    LoadPluginResponsePayload& outResp,
                    int timeoutMs = 10000);

    bool queryContract(QueryContractResponsePayload& outResp,
                       int timeoutMs = 5000);

    bool resetPlugin(uint32_t trialIndex = 0,
                     int timeoutMs = 5000);

    bool releasePlugin(int timeoutMs = 5000);
    bool renderBlock(const RenderBlockRequestPayload& req,
                     RenderBlockResponsePayload& outResp,
                     int timeoutMs = 5000);

    // Detección y simulación para tests del Slice 1 y 2
    bool requestSimulatedCrash();
    bool requestSimulatedHang();

private:
    std::string pipeName_;
    std::string sessionId_;
    uint64_t sessionNonce_ { 0 };
    uint32_t workerPid_ { 0 };
    HostState state_ { HostState::Uninitialized };
    std::string lastError_;

    NamedPipeServer pipeServer_;

#if defined(_WIN32)
    HANDLE hProcess_ { INVALID_HANDLE_VALUE };
    HANDLE hThread_ { INVALID_HANDLE_VALUE };
#endif

    void cleanupProcessHandles();
};

} // namespace abdaudiolab::ipc
