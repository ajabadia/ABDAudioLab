#include "Win32NamedPipe.h"
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>

#if defined(_WIN32)
#include <sddl.h>
#endif

namespace abdaudiolab::ipc
{

std::string generateSecurePipeName(const std::string& sessionId, uint64_t nonce, uint32_t pid)
{
    std::ostringstream ss;
    ss << "\\\\.\\pipe\\abdaudiolab_" << sessionId << "_" << std::hex << nonce << "_" << std::dec << pid;
    return ss.str();
}

// ==============================================================================
// RestrictedSecurityDescriptor Implementation
// ==============================================================================
RestrictedSecurityDescriptor::RestrictedSecurityDescriptor()
{
#if defined(_WIN32)
    sa_.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa_.bInheritHandle = FALSE;
    sa_.lpSecurityDescriptor = nullptr;

    pSd_ = static_cast<PSECURITY_DESCRIPTOR>(LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH));
    if (!pSd_) return;

    if (!InitializeSecurityDescriptor(pSd_, SECURITY_DESCRIPTOR_REVISION))
    {
        LocalFree(pSd_);
        pSd_ = nullptr;
        return;
    }

    HANDLE hToken = INVALID_HANDLE_VALUE;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    {
        LocalFree(pSd_);
        pSd_ = nullptr;
        return;
    }

    DWORD tokenInfoLen = 0;
    GetTokenInformation(hToken, TokenUser, nullptr, 0, &tokenInfoLen);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || tokenInfoLen == 0)
    {
        CloseHandle(hToken);
        LocalFree(pSd_);
        pSd_ = nullptr;
        return;
    }

    std::vector<uint8_t> userBuf(tokenInfoLen);
    auto* pTokenUser = reinterpret_cast<PTOKEN_USER>(userBuf.data());

    if (!GetTokenInformation(hToken, TokenUser, pTokenUser, tokenInfoLen, &tokenInfoLen))
    {
        CloseHandle(hToken);
        LocalFree(pSd_);
        pSd_ = nullptr;
        return;
    }
    CloseHandle(hToken);

    DWORD aclSize = sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD) + GetLengthSid(pTokenUser->User.Sid);
    pAcl_ = static_cast<PACL>(LocalAlloc(LPTR, aclSize));
    if (!pAcl_)
    {
        LocalFree(pSd_);
        pSd_ = nullptr;
        return;
    }

    if (!InitializeAcl(pAcl_, aclSize, ACL_REVISION))
    {
        LocalFree(pAcl_);
        LocalFree(pSd_);
        pAcl_ = nullptr;
        pSd_ = nullptr;
        return;
    }

    // Permitir acceso total únicamente al usuario actual del proceso
    if (!AddAccessAllowedAce(pAcl_, ACL_REVISION, GENERIC_ALL, pTokenUser->User.Sid))
    {
        LocalFree(pAcl_);
        LocalFree(pSd_);
        pAcl_ = nullptr;
        pSd_ = nullptr;
        return;
    }

    if (!SetSecurityDescriptorDacl(pSd_, TRUE, pAcl_, FALSE))
    {
        LocalFree(pAcl_);
        LocalFree(pSd_);
        pAcl_ = nullptr;
        pSd_ = nullptr;
        return;
    }

    sa_.lpSecurityDescriptor = pSd_;
    isValid_ = true;
#else
    isValid_ = true;
#endif
}

RestrictedSecurityDescriptor::~RestrictedSecurityDescriptor()
{
#if defined(_WIN32)
    if (pAcl_) LocalFree(pAcl_);
    if (pSd_) LocalFree(pSd_);
#endif
}

#if defined(_WIN32)
SECURITY_ATTRIBUTES* RestrictedSecurityDescriptor::getSecurityAttributes() noexcept
{
    return isValid_ ? &sa_ : nullptr;
}
#endif

// ==============================================================================
// Helper de lectura exacta con timeout
// ==============================================================================
#if defined(_WIN32)
static bool readExactBytes(HANDLE hPipe, uint8_t* dest, size_t count, int timeoutMs)
{
    size_t totalRead = 0;
    auto start = std::chrono::steady_clock::now();

    while (totalRead < count)
    {
        DWORD bytesAvailable = 0;
        if (!PeekNamedPipe(hPipe, nullptr, 0, nullptr, &bytesAvailable, nullptr))
        {
            return false; // Error o pipe cerrado
        }

        if (bytesAvailable > 0)
        {
            DWORD bytesToRead = static_cast<DWORD>(std::min<size_t>(count - totalRead, bytesAvailable));
            DWORD readNow = 0;
            if (!ReadFile(hPipe, dest + totalRead, bytesToRead, &readNow, nullptr) || readNow == 0)
            {
                return false;
            }
            totalRead += readNow;
        }
        else
        {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
            if (elapsed >= timeoutMs)
            {
                return false; // Timeout
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
    return true;
}

static bool writeExactBytes(HANDLE hPipe, const uint8_t* src, size_t count)
{
    size_t totalWritten = 0;
    while (totalWritten < count)
    {
        DWORD toWrite = static_cast<DWORD>(count - totalWritten);
        DWORD written = 0;
        if (!WriteFile(hPipe, src + totalWritten, toWrite, &written, nullptr) || written == 0)
        {
            return false;
        }
        totalWritten += written;
    }
    return true;
}
#endif

// ==============================================================================
// NamedPipeServer Implementation
// ==============================================================================
NamedPipeServer::NamedPipeServer() = default;

NamedPipeServer::~NamedPipeServer()
{
    disconnect();
}

bool NamedPipeServer::create(const std::string& pipeName, int timeoutMs)
{
#if defined(_WIN32)
    disconnect();
    pipeName_ = pipeName;

    RestrictedSecurityDescriptor sec;
    SECURITY_ATTRIBUTES* pSa = sec.getSecurityAttributes();

    hPipe_ = CreateNamedPipeA(
        pipeName.c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,              // 1 sola instancia por worker
        65536,          // Out buffer
        65536,          // In buffer
        static_cast<DWORD>(timeoutMs),
        pSa
    );

    if (hPipe_ == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    hConnectEvent_ = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (hConnectEvent_ == INVALID_HANDLE_VALUE)
    {
        CloseHandle(hPipe_);
        hPipe_ = INVALID_HANDLE_VALUE;
        return false;
    }

    memset(&ovConnect_, 0, sizeof(ovConnect_));
    ovConnect_.hEvent = hConnectEvent_;
    BOOL connected = ConnectNamedPipe(hPipe_, &ovConnect_);
    DWORD err = GetLastError();

    if (!connected && err != ERROR_IO_PENDING && err != ERROR_PIPE_CONNECTED)
    {
        CloseHandle(hConnectEvent_);
        hConnectEvent_ = INVALID_HANDLE_VALUE;
        CloseHandle(hPipe_);
        hPipe_ = INVALID_HANDLE_VALUE;
        return false;
    }

    if (connected || err == ERROR_PIPE_CONNECTED)
    {
        isConnected_ = true;
    }

    return true;
#else
    return false;
#endif
}

bool NamedPipeServer::waitForClientConnection(int timeoutMs)
{
#if defined(_WIN32)
    if (isConnected_) return true;
    if (hPipe_ == INVALID_HANDLE_VALUE || hConnectEvent_ == INVALID_HANDLE_VALUE) return false;

    DWORD waitRes = WaitForSingleObject(hConnectEvent_, static_cast<DWORD>(timeoutMs));
    if (waitRes == WAIT_OBJECT_0)
    {
        DWORD bytesTransferred = 0;
        if (GetOverlappedResult(hPipe_, &ovConnect_, &bytesTransferred, FALSE))
        {
            isConnected_ = true;
            return true;
        }
        else
        {
            DWORD err = GetLastError();
            if (err == ERROR_PIPE_CONNECTED)
            {
                isConnected_ = true;
                return true;
            }
        }
    }
    else
    {
        // Si hay timeout o error, cancelar la operación overlapped pendiente para no dejar hPipe_ apuntando a memoria inválida
        CancelIoEx(hPipe_, &ovConnect_);
    }
    return false;
#else
    return false;
#endif
}

void NamedPipeServer::disconnect()
{
#if defined(_WIN32)
    if (hPipe_ != INVALID_HANDLE_VALUE)
    {
        CancelIoEx(hPipe_, nullptr);
        if (isConnected_)
        {
            FlushFileBuffers(hPipe_);
            DisconnectNamedPipe(hPipe_);
        }
        CloseHandle(hPipe_);
        hPipe_ = INVALID_HANDLE_VALUE;
    }
    if (hConnectEvent_ != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hConnectEvent_);
        hConnectEvent_ = INVALID_HANDLE_VALUE;
    }
    memset(&ovConnect_, 0, sizeof(ovConnect_));
    isConnected_ = false;
#endif
}

bool NamedPipeServer::isConnected() const noexcept
{
    return isConnected_;
}

bool NamedPipeServer::sendMessage(MessageType type, const std::vector<uint8_t>& payload)
{
#if defined(_WIN32)
    if (!isConnected_ || hPipe_ == INVALID_HANDLE_VALUE) return false;
    if (payload.size() > kIpcMaxPayloadSize) return false;

    MessageHeader h;
    h.magic = kIpcMagicHeader;
    h.type = type;
    h.payloadSize = static_cast<uint16_t>(payload.size());

    auto headerBytes = h.serialize();
    if (!writeExactBytes(hPipe_, headerBytes.data(), headerBytes.size()))
    {
        isConnected_ = false;
        return false;
    }

    if (!payload.empty())
    {
        if (!writeExactBytes(hPipe_, payload.data(), payload.size()))
        {
            isConnected_ = false;
            return false;
        }
    }
    return true;
#else
    return false;
#endif
}

bool NamedPipeServer::receiveMessage(MessageType& outType, std::vector<uint8_t>& outPayload, int timeoutMs)
{
#if defined(_WIN32)
    if (!isConnected_ || hPipe_ == INVALID_HANDLE_VALUE) return false;

    std::array<uint8_t, kIpcHeaderSize> headerBuf;
    if (!readExactBytes(hPipe_, headerBuf.data(), kIpcHeaderSize, timeoutMs))
    {
        return false;
    }

    auto optHeader = MessageHeader::deserialize(headerBuf.data(), kIpcHeaderSize);
    if (!optHeader)
    {
        // Corrupción de framing o magic incorrecto
        isConnected_ = false;
        return false;
    }

    outType = optHeader->type;
    outPayload.clear();

    if (optHeader->payloadSize > 0)
    {
        if (optHeader->payloadSize > kIpcMaxPayloadSize)
        {
            // Tamaño sobredimensionado ilegal
            isConnected_ = false;
            return false;
        }

        outPayload.resize(optHeader->payloadSize);
        if (!readExactBytes(hPipe_, outPayload.data(), optHeader->payloadSize, timeoutMs))
        {
            isConnected_ = false;
            return false;
        }
    }
    return true;
#else
    return false;
#endif
}

// ==============================================================================
// NamedPipeClient Implementation
// ==============================================================================
NamedPipeClient::NamedPipeClient() = default;

NamedPipeClient::~NamedPipeClient()
{
    disconnect();
}

bool NamedPipeClient::connectToServer(const std::string& pipeName, int timeoutMs)
{
#if defined(_WIN32)
    disconnect();
    pipeName_ = pipeName;

    auto start = std::chrono::steady_clock::now();
    while (true)
    {
        hPipe_ = CreateFileA(
            pipeName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );

        if (hPipe_ != INVALID_HANDLE_VALUE)
        {
            isConnected_ = true;
            return true;
        }

        if (GetLastError() != ERROR_PIPE_BUSY)
        {
            // El pipe no existe aún o acceso denegado
        }
        else
        {
            if (WaitNamedPipeA(pipeName.c_str(), 100))
            {
                continue;
            }
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeoutMs)
        {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
#else
    return false;
#endif
}

void NamedPipeClient::disconnect()
{
#if defined(_WIN32)
    if (hPipe_ != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hPipe_);
        hPipe_ = INVALID_HANDLE_VALUE;
    }
    isConnected_ = false;
#endif
}

bool NamedPipeClient::isConnected() const noexcept
{
    return isConnected_;
}

bool NamedPipeClient::sendMessage(MessageType type, const std::vector<uint8_t>& payload)
{
#if defined(_WIN32)
    if (!isConnected_ || hPipe_ == INVALID_HANDLE_VALUE) return false;
    if (payload.size() > kIpcMaxPayloadSize) return false;

    MessageHeader h;
    h.magic = kIpcMagicHeader;
    h.type = type;
    h.payloadSize = static_cast<uint16_t>(payload.size());

    auto headerBytes = h.serialize();
    if (!writeExactBytes(hPipe_, headerBytes.data(), headerBytes.size()))
    {
        isConnected_ = false;
        return false;
    }

    if (!payload.empty())
    {
        if (!writeExactBytes(hPipe_, payload.data(), payload.size()))
        {
            isConnected_ = false;
            return false;
        }
    }
    return true;
#else
    return false;
#endif
}

bool NamedPipeClient::receiveMessage(MessageType& outType, std::vector<uint8_t>& outPayload, int timeoutMs)
{
#if defined(_WIN32)
    if (!isConnected_ || hPipe_ == INVALID_HANDLE_VALUE) return false;

    std::array<uint8_t, kIpcHeaderSize> headerBuf;
    if (!readExactBytes(hPipe_, headerBuf.data(), kIpcHeaderSize, timeoutMs))
    {
        return false;
    }

    auto optHeader = MessageHeader::deserialize(headerBuf.data(), kIpcHeaderSize);
    if (!optHeader)
    {
        isConnected_ = false;
        return false;
    }

    outType = optHeader->type;
    outPayload.clear();

    if (optHeader->payloadSize > 0)
    {
        if (optHeader->payloadSize > kIpcMaxPayloadSize)
        {
            isConnected_ = false;
            return false;
        }

        outPayload.resize(optHeader->payloadSize);
        if (!readExactBytes(hPipe_, outPayload.data(), optHeader->payloadSize, timeoutMs))
        {
            isConnected_ = false;
            return false;
        }
    }
    return true;
#else
    return false;
#endif
}

} // namespace abdaudiolab::ipc
