#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include "IpcProtocol.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace abdaudiolab::ipc
{

/**
 * @brief Generador de nombres seguros no predecibles para pipes y objetos IPC.
 */
[[nodiscard]] std::string generateSecurePipeName(const std::string& sessionId, uint64_t nonce, uint32_t pid);

/**
 * @brief Crea un SECURITY_ATTRIBUTES Win32 con ACL restrictiva al usuario y logon actual.
 */
class [[nodiscard]] RestrictedSecurityDescriptor
{
public:
    RestrictedSecurityDescriptor();
    ~RestrictedSecurityDescriptor();

    RestrictedSecurityDescriptor(const RestrictedSecurityDescriptor&) = delete;
    RestrictedSecurityDescriptor& operator=(const RestrictedSecurityDescriptor&) = delete;

#if defined(_WIN32)
    [[nodiscard]] SECURITY_ATTRIBUTES* getSecurityAttributes() noexcept;
#endif

    [[nodiscard]] bool isValid() const noexcept { return isValid_; }

private:
    bool isValid_ { false };
#if defined(_WIN32)
    SECURITY_ATTRIBUTES sa_;
    PSECURITY_DESCRIPTOR pSd_ { nullptr };
    PACL pAcl_ { nullptr };
#endif
};

/**
 * @brief Extremo Servidor (Host) del Named Pipe Win32 con framing estricto de 8 bytes.
 */
class NamedPipeServer
{
public:
    NamedPipeServer();
    ~NamedPipeServer();

    NamedPipeServer(const NamedPipeServer&) = delete;
    NamedPipeServer& operator=(const NamedPipeServer&) = delete;

    bool create(const std::string& pipeName, int timeoutMs = 5000);
    bool waitForClientConnection(int timeoutMs = 5000);
    void disconnect();
    [[nodiscard]] bool isConnected() const noexcept;

    bool sendMessage(MessageType type, const std::vector<uint8_t>& payload);
    bool receiveMessage(MessageType& outType, std::vector<uint8_t>& outPayload, int timeoutMs = 5000);

private:
#if defined(_WIN32)
    HANDLE hPipe_ { INVALID_HANDLE_VALUE };
    HANDLE hConnectEvent_ { INVALID_HANDLE_VALUE };
    OVERLAPPED ovConnect_ {};
#endif
    std::string pipeName_;
    bool isConnected_ { false };
};

/**
 * @brief Extremo Cliente (Worker) del Named Pipe Win32.
 */
class NamedPipeClient
{
public:
    NamedPipeClient();
    ~NamedPipeClient();

    NamedPipeClient(const NamedPipeClient&) = delete;
    NamedPipeClient& operator=(const NamedPipeClient&) = delete;

    bool connectToServer(const std::string& pipeName, int timeoutMs = 5000);
    void disconnect();
    [[nodiscard]] bool isConnected() const noexcept;

    bool sendMessage(MessageType type, const std::vector<uint8_t>& payload);
    bool receiveMessage(MessageType& outType, std::vector<uint8_t>& outPayload, int timeoutMs = 5000);

private:
#if defined(_WIN32)
    HANDLE hPipe_ { INVALID_HANDLE_VALUE };
#endif
    std::string pipeName_;
    bool isConnected_ { false };
};

} // namespace abdaudiolab::ipc
