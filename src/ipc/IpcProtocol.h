#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <cstring>
#include <optional>

namespace abdaudiolab::ipc
{

// ==============================================================================
// Constantes de Protocolo y Seguridad
// ==============================================================================
inline constexpr uint32_t kIpcMagicHeader       = 0x41424457; // "ABDW" en Little Endian
inline constexpr uint16_t kIpcProtocolMajor     = 1;
inline constexpr uint16_t kIpcProtocolMinor     = 0;
inline constexpr uint16_t kIpcMaxPayloadSize    = 65535; // 64 KiB límite máximo de framing
inline constexpr uint16_t kIpcHeaderSize        = 8;     // magic (4) + type (2) + size (2)

// ==============================================================================
// Tipos de Mensajes de Control
// ==============================================================================
enum class MessageType : uint16_t
{
    Invalid                 = 0x0000,
    HandshakeRequest        = 0x0001,
    HandshakeResponse       = 0x0002,
    Ping                    = 0x0003,
    Pong                    = 0x0004,
    ShutdownRequest         = 0x0005,
    ShutdownResponse        = 0x0006,
    LoadPluginRequest       = 0x0010,
    LoadPluginResponse      = 0x0011,
    QueryContractRequest    = 0x0012,
    QueryContractResponse   = 0x0013,
    ResetPluginRequest      = 0x0014,
    ResetPluginResponse     = 0x0015,
    ReleasePluginRequest    = 0x0016,
    ReleasePluginResponse   = 0x0017,
    RenderBlockRequest      = 0x0020,
    RenderBlockResponse     = 0x0021,
    SimulateCrashRequest    = 0x00F0,
    SimulateHangRequest     = 0x00F1,
    ErrorResponse           = 0xFFFF
};

// ==============================================================================
// Helpers de Codificación / Decodificación Little-Endian (Libre de Padding ABI)
// ==============================================================================
inline void writeUint16LE(std::vector<uint8_t>& buffer, uint16_t value)
{
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

inline void writeUint32LE(std::vector<uint8_t>& buffer, uint32_t value)
{
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

inline void writeUint64LE(std::vector<uint8_t>& buffer, uint64_t value)
{
    for (int i = 0; i < 8; ++i)
    {
        buffer.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }
}

inline void writeString(std::vector<uint8_t>& buffer, const std::string& str)
{
    uint16_t len = static_cast<uint16_t>(std::min<size_t>(str.size(), 65535));
    writeUint16LE(buffer, len);
    buffer.insert(buffer.end(), str.begin(), str.begin() + len);
}

inline bool readUint16LE(const uint8_t*& data, size_t& remaining, uint16_t& outValue)
{
    if (remaining < 2) return false;
    outValue = static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
    data += 2;
    remaining -= 2;
    return true;
}

inline bool readUint32LE(const uint8_t*& data, size_t& remaining, uint32_t& outValue)
{
    if (remaining < 4) return false;
    outValue = static_cast<uint32_t>(data[0]) |
              (static_cast<uint32_t>(data[1]) << 8) |
              (static_cast<uint32_t>(data[2]) << 16) |
              (static_cast<uint32_t>(data[3]) << 24);
    data += 4;
    remaining -= 4;
    return true;
}

inline bool readUint64LE(const uint8_t*& data, size_t& remaining, uint64_t& outValue)
{
    if (remaining < 8) return false;
    outValue = 0;
    for (int i = 0; i < 8; ++i)
    {
        outValue |= (static_cast<uint64_t>(data[i]) << (i * 8));
    }
    data += 8;
    remaining -= 8;
    return true;
}

inline bool readString(const uint8_t*& data, size_t& remaining, std::string& outStr)
{
    uint16_t len = 0;
    if (!readUint16LE(data, remaining, len)) return false;
    if (remaining < len) return false;
    outStr.assign(reinterpret_cast<const char*>(data), len);
    data += len;
    remaining -= len;
    return true;
}

// ==============================================================================
// Cabecera de Framing Binario
// ==============================================================================
struct MessageHeader
{
    uint32_t magic { kIpcMagicHeader };
    MessageType type { MessageType::Invalid };
    uint16_t payloadSize { 0 };

    [[nodiscard]] std::vector<uint8_t> serialize() const
    {
        std::vector<uint8_t> buf;
        buf.reserve(kIpcHeaderSize);
        writeUint32LE(buf, magic);
        writeUint16LE(buf, static_cast<uint16_t>(type));
        writeUint16LE(buf, payloadSize);
        return buf;
    }

    [[nodiscard]] static std::optional<MessageHeader> deserialize(const uint8_t* data, size_t size)
    {
        if (size < kIpcHeaderSize) return std::nullopt;
        const uint8_t* ptr = data;
        size_t rem = size;

        MessageHeader h;
        if (!readUint32LE(ptr, rem, h.magic)) return std::nullopt;
        uint16_t rawType = 0;
        if (!readUint16LE(ptr, rem, rawType)) return std::nullopt;
        h.type = static_cast<MessageType>(rawType);
        if (!readUint16LE(ptr, rem, h.payloadSize)) return std::nullopt;

        if (h.magic != kIpcMagicHeader) return std::nullopt;
        return h;
    }
};

// ==============================================================================
// Estructuras de Mensajes del Slice 1
// ==============================================================================
struct HandshakePayload
{
    uint16_t protocolMajor { kIpcProtocolMajor };
    uint16_t protocolMinor { kIpcProtocolMinor };
    uint32_t pid { 0 };
    uint64_t nonce { 0 };
    std::string sessionId;
    std::string architecture;
    uint32_t capabilities { 0 };

    [[nodiscard]] std::vector<uint8_t> serialize() const
    {
        std::vector<uint8_t> buf;
        writeUint16LE(buf, protocolMajor);
        writeUint16LE(buf, protocolMinor);
        writeUint32LE(buf, pid);
        writeUint64LE(buf, nonce);
        writeString(buf, sessionId);
        writeString(buf, architecture);
        writeUint32LE(buf, capabilities);
        return buf;
    }

    [[nodiscard]] static std::optional<HandshakePayload> deserialize(const uint8_t* data, size_t size)
    {
        const uint8_t* ptr = data;
        size_t rem = size;

        HandshakePayload p;
        if (!readUint16LE(ptr, rem, p.protocolMajor)) return std::nullopt;
        if (!readUint16LE(ptr, rem, p.protocolMinor)) return std::nullopt;
        if (!readUint32LE(ptr, rem, p.pid)) return std::nullopt;
        if (!readUint64LE(ptr, rem, p.nonce)) return std::nullopt;
        if (!readString(ptr, rem, p.sessionId)) return std::nullopt;
        if (!readString(ptr, rem, p.architecture)) return std::nullopt;
        if (!readUint32LE(ptr, rem, p.capabilities)) return std::nullopt;
        return p;
    }
};

struct PingPongPayload
{
    uint64_t sequence { 0 };
    uint64_t timestampMs { 0 };

    [[nodiscard]] std::vector<uint8_t> serialize() const
    {
        std::vector<uint8_t> buf;
        writeUint64LE(buf, sequence);
        writeUint64LE(buf, timestampMs);
        return buf;
    }

    [[nodiscard]] static std::optional<PingPongPayload> deserialize(const uint8_t* data, size_t size)
    {
        const uint8_t* ptr = data;
        size_t rem = size;

        PingPongPayload p;
        if (!readUint64LE(ptr, rem, p.sequence)) return std::nullopt;
        if (!readUint64LE(ptr, rem, p.timestampMs)) return std::nullopt;
        return p;
    }
};

struct ShutdownPayload
{
    uint32_t reasonCode { 0 }; // 0 = Cierre normal
    std::string reasonText;

    [[nodiscard]] std::vector<uint8_t> serialize() const
    {
        std::vector<uint8_t> buf;
        writeUint32LE(buf, reasonCode);
        writeString(buf, reasonText);
        return buf;
    }

    [[nodiscard]] static std::optional<ShutdownPayload> deserialize(const uint8_t* data, size_t size)
    {
        const uint8_t* ptr = data;
        size_t rem = size;

        ShutdownPayload p;
        if (!readUint32LE(ptr, rem, p.reasonCode)) return std::nullopt;
        if (!readString(ptr, rem, p.reasonText)) return std::nullopt;
        return p;
    }
};

struct ErrorPayload
{
    uint32_t errorCode { 0 };
    std::string errorMessage;

    [[nodiscard]] std::vector<uint8_t> serialize() const
    {
        std::vector<uint8_t> buf;
        writeUint32LE(buf, errorCode);
        writeString(buf, errorMessage);
        return buf;
    }

    [[nodiscard]] static std::optional<ErrorPayload> deserialize(const uint8_t* data, size_t size)
    {
        const uint8_t* ptr = data;
        size_t rem = size;

        ErrorPayload p;
        if (!readUint32LE(ptr, rem, p.errorCode)) return std::nullopt;
        if (!readString(ptr, rem, p.errorMessage)) return std::nullopt;
        return p;
    }
};

// ==============================================================================
// Payloads para el Vertical Slice 2 (Carga, Contrato y Lifecycle Remoto)
// ==============================================================================
struct LoadPluginRequestPayload
{
    std::string runId;
    std::string sessionId;
    std::string pluginPath;
    std::string expectedBinarySha256;
    double sampleRate { 48000.0 };
    uint32_t blockSize { 256 };

    [[nodiscard]] std::vector<uint8_t> serialize() const
    {
        std::vector<uint8_t> buf;
        writeString(buf, runId);
        writeString(buf, sessionId);
        writeString(buf, pluginPath);
        writeString(buf, expectedBinarySha256);
        uint64_t srRaw = 0;
        std::memcpy(&srRaw, &sampleRate, sizeof(double));
        writeUint64LE(buf, srRaw);
        writeUint32LE(buf, blockSize);
        return buf;
    }

    [[nodiscard]] static std::optional<LoadPluginRequestPayload> deserialize(const uint8_t* data, size_t size)
    {
        const uint8_t* ptr = data;
        size_t rem = size;

        LoadPluginRequestPayload p;
        if (!readString(ptr, rem, p.runId)) return std::nullopt;
        if (!readString(ptr, rem, p.sessionId)) return std::nullopt;
        if (!readString(ptr, rem, p.pluginPath)) return std::nullopt;
        if (!readString(ptr, rem, p.expectedBinarySha256)) return std::nullopt;
        uint64_t srRaw = 0;
        if (!readUint64LE(ptr, rem, srRaw)) return std::nullopt;
        std::memcpy(&p.sampleRate, &srRaw, sizeof(double));
        if (!readUint32LE(ptr, rem, p.blockSize)) return std::nullopt;
        return p;
    }
};

struct LoadPluginResponsePayload
{
    uint32_t status { 0 }; // 0 = Exito, != 0 = Error
    uint32_t errorCode { 0 };
    std::string errorMessage;
    std::string pluginFormat;
    std::string pluginUid;
    std::string vendor;
    std::string name;
    std::string version;
    std::string binarySha256;
    uint32_t capabilities { 0 };
    std::string executionMode;

    [[nodiscard]] std::vector<uint8_t> serialize() const
    {
        std::vector<uint8_t> buf;
        writeUint32LE(buf, status);
        writeUint32LE(buf, errorCode);
        writeString(buf, errorMessage);
        writeString(buf, pluginFormat);
        writeString(buf, pluginUid);
        writeString(buf, vendor);
        writeString(buf, name);
        writeString(buf, version);
        writeString(buf, binarySha256);
        writeUint32LE(buf, capabilities);
        writeString(buf, executionMode);
        return buf;
    }

    [[nodiscard]] static std::optional<LoadPluginResponsePayload> deserialize(const uint8_t* data, size_t size)
    {
        const uint8_t* ptr = data;
        size_t rem = size;

        LoadPluginResponsePayload p;
        if (!readUint32LE(ptr, rem, p.status)) return std::nullopt;
        if (!readUint32LE(ptr, rem, p.errorCode)) return std::nullopt;
        if (!readString(ptr, rem, p.errorMessage)) return std::nullopt;
        if (!readString(ptr, rem, p.pluginFormat)) return std::nullopt;
        if (!readString(ptr, rem, p.pluginUid)) return std::nullopt;
        if (!readString(ptr, rem, p.vendor)) return std::nullopt;
        if (!readString(ptr, rem, p.name)) return std::nullopt;
        if (!readString(ptr, rem, p.version)) return std::nullopt;
        if (!readString(ptr, rem, p.binarySha256)) return std::nullopt;
        if (!readUint32LE(ptr, rem, p.capabilities)) return std::nullopt;
        if (!readString(ptr, rem, p.executionMode)) return std::nullopt;
        return p;
    }
};

struct QueryContractRequestPayload
{
    uint32_t dummy { 0 };

    [[nodiscard]] std::vector<uint8_t> serialize() const
    {
        std::vector<uint8_t> buf;
        writeUint32LE(buf, dummy);
        return buf;
    }

    [[nodiscard]] static std::optional<QueryContractRequestPayload> deserialize(const uint8_t* data, size_t size)
    {
        const uint8_t* ptr = data;
        size_t rem = size;
        QueryContractRequestPayload p;
        if (!readUint32LE(ptr, rem, p.dummy)) return std::nullopt;
        return p;
    }
};

struct QueryContractResponsePayload
{
    std::string targetId;
    uint32_t inputChannels { 0 };
    uint32_t outputChannels { 0 };
    uint32_t numParameters { 0 };
    uint32_t supportsMidi { 0 };    // 1 = Si
    uint32_t supportsNativeGui { 0 }; // 1 = Si
    uint32_t requiresResetBetweenTrials { 0 }; // 1 = Si
    uint32_t settlingTimeMs { 50 };
    std::string determinism;
    uint32_t latencySamples { 0 };

    [[nodiscard]] std::vector<uint8_t> serialize() const
    {
        std::vector<uint8_t> buf;
        writeString(buf, targetId);
        writeUint32LE(buf, inputChannels);
        writeUint32LE(buf, outputChannels);
        writeUint32LE(buf, numParameters);
        writeUint32LE(buf, supportsMidi);
        writeUint32LE(buf, supportsNativeGui);
        writeUint32LE(buf, requiresResetBetweenTrials);
        writeUint32LE(buf, settlingTimeMs);
        writeString(buf, determinism);
        writeUint32LE(buf, latencySamples);
        return buf;
    }

    [[nodiscard]] static std::optional<QueryContractResponsePayload> deserialize(const uint8_t* data, size_t size)
    {
        const uint8_t* ptr = data;
        size_t rem = size;

        QueryContractResponsePayload p;
        if (!readString(ptr, rem, p.targetId)) return std::nullopt;
        if (!readUint32LE(ptr, rem, p.inputChannels)) return std::nullopt;
        if (!readUint32LE(ptr, rem, p.outputChannels)) return std::nullopt;
        if (!readUint32LE(ptr, rem, p.numParameters)) return std::nullopt;
        if (!readUint32LE(ptr, rem, p.supportsMidi)) return std::nullopt;
        if (!readUint32LE(ptr, rem, p.supportsNativeGui)) return std::nullopt;
        if (!readUint32LE(ptr, rem, p.requiresResetBetweenTrials)) return std::nullopt;
        if (!readUint32LE(ptr, rem, p.settlingTimeMs)) return std::nullopt;
        if (!readString(ptr, rem, p.determinism)) return std::nullopt;
        if (!readUint32LE(ptr, rem, p.latencySamples)) return std::nullopt;
        return p;
    }
};

struct ResetPluginPayload
{
    uint32_t trialIndex { 0 };

    [[nodiscard]] std::vector<uint8_t> serialize() const
    {
        std::vector<uint8_t> buf;
        writeUint32LE(buf, trialIndex);
        return buf;
    }

    [[nodiscard]] static std::optional<ResetPluginPayload> deserialize(const uint8_t* data, size_t size)
    {
        const uint8_t* ptr = data;
        size_t rem = size;
        ResetPluginPayload p;
        if (!readUint32LE(ptr, rem, p.trialIndex)) return std::nullopt;
        return p;
    }
};

struct ReleasePluginPayload
{
    uint32_t flags { 0 };

    [[nodiscard]] std::vector<uint8_t> serialize() const
    {
        std::vector<uint8_t> buf;
        writeUint32LE(buf, flags);
        return buf;
    }

    [[nodiscard]] static std::optional<ReleasePluginPayload> deserialize(const uint8_t* data, size_t size)
    {
        const uint8_t* ptr = data;
        size_t rem = size;
        ReleasePluginPayload p;
        if (!readUint32LE(ptr, rem, p.flags)) return std::nullopt;
        return p;
    }
};

// ==============================================================================
// Slice 3: RenderBlock Payload Structures & Explicit LE Serialization
// ==============================================================================
inline constexpr uint32_t kIpcMaxBlockSamples  = 4096;
inline constexpr uint16_t kIpcMaxAudioChannels = 8;
inline constexpr uint32_t kIpcMaxMidiEvents    = 512;

struct TimedMidiWireEvent
{
    uint16_t sampleOffset { 0 };
    uint8_t  type { 0 }; // 0=NoteOn, 1=NoteOff, 2=AllNotesOff
    uint8_t  channel { 1 };
    uint8_t  noteNumber { 60 };
    uint8_t  velocity { 64 };
};

struct RenderBlockRequestPayload
{
    uint64_t sequence { 0 };
    uint32_t blockSize { 256 };
    uint16_t numChannels { 2 };
    std::vector<TimedMidiWireEvent> midiEvents;

    [[nodiscard]] std::vector<uint8_t> serialize() const
    {
        std::vector<uint8_t> buf;
        writeUint64LE(buf, sequence);
        writeUint32LE(buf, blockSize);
        writeUint16LE(buf, numChannels);

        uint32_t eventCount = static_cast<uint32_t>(std::min<size_t>(midiEvents.size(), kIpcMaxMidiEvents));
        writeUint32LE(buf, eventCount);
        for (uint32_t i = 0; i < eventCount; ++i)
        {
            const auto& ev = midiEvents[i];
            writeUint16LE(buf, ev.sampleOffset);
            buf.push_back(ev.type);
            buf.push_back(ev.channel);
            buf.push_back(ev.noteNumber);
            buf.push_back(ev.velocity);
        }
        return buf;
    }

    [[nodiscard]] static std::optional<RenderBlockRequestPayload> deserialize(const uint8_t* data, size_t size)
    {
        const uint8_t* ptr = data;
        size_t rem = size;
        RenderBlockRequestPayload p;

        if (!readUint64LE(ptr, rem, p.sequence)) return std::nullopt;
        if (!readUint32LE(ptr, rem, p.blockSize)) return std::nullopt;
        if (!readUint16LE(ptr, rem, p.numChannels)) return std::nullopt;

        // Validación defensiva estricta de límites
        if (p.blockSize == 0 || p.blockSize > kIpcMaxBlockSamples) return std::nullopt;
        if (p.numChannels == 0 || p.numChannels > kIpcMaxAudioChannels) return std::nullopt;

        uint32_t eventCount = 0;
        if (!readUint32LE(ptr, rem, eventCount)) return std::nullopt;
        if (eventCount > kIpcMaxMidiEvents) return std::nullopt;

        // Cada evento serializado ocupa 6 bytes (sampleOffset 2 + type 1 + chan 1 + note 1 + vel 1)
        if (rem < static_cast<size_t>(eventCount) * 6) return std::nullopt;

        p.midiEvents.reserve(eventCount);
        for (uint32_t i = 0; i < eventCount; ++i)
        {
            TimedMidiWireEvent ev;
            if (!readUint16LE(ptr, rem, ev.sampleOffset)) return std::nullopt;
            if (rem < 4) return std::nullopt;
            ev.type = ptr[0];
            ev.channel = ptr[1];
            ev.noteNumber = ptr[2];
            ev.velocity = ptr[3];
            ptr += 4;
            rem -= 4;
            p.midiEvents.push_back(ev);
        }

        return p;
    }
};

struct RenderBlockResponsePayload
{
    uint64_t sequence { 0 };
    uint32_t status { 0 }; // 0 = OK, != 0 = Error
    uint32_t errorCode { 0 };
    std::string errorMessage;
    uint32_t samplesRendered { 0 };
    uint16_t numChannels { 0 };
    uint64_t durationUs { 0 };
    std::vector<float> audioData; // Samples interleaved

    [[nodiscard]] std::vector<uint8_t> serialize() const
    {
        std::vector<uint8_t> buf;
        writeUint64LE(buf, sequence);
        writeUint32LE(buf, status);
        writeUint32LE(buf, errorCode);
        writeString(buf, errorMessage);
        writeUint32LE(buf, samplesRendered);
        writeUint16LE(buf, numChannels);
        writeUint64LE(buf, durationUs);

        uint32_t floatCount = static_cast<uint32_t>(audioData.size());
        writeUint32LE(buf, floatCount);
        if (floatCount > 0)
        {
            const size_t byteLen = floatCount * sizeof(float);
            const uint8_t* rawFloats = reinterpret_cast<const uint8_t*>(audioData.data());
            buf.insert(buf.end(), rawFloats, rawFloats + byteLen);
        }
        return buf;
    }

    [[nodiscard]] static std::optional<RenderBlockResponsePayload> deserialize(const uint8_t* data, size_t size)
    {
        const uint8_t* ptr = data;
        size_t rem = size;
        RenderBlockResponsePayload p;

        if (!readUint64LE(ptr, rem, p.sequence)) return std::nullopt;
        if (!readUint32LE(ptr, rem, p.status)) return std::nullopt;
        if (!readUint32LE(ptr, rem, p.errorCode)) return std::nullopt;
        if (!readString(ptr, rem, p.errorMessage)) return std::nullopt;
        if (!readUint32LE(ptr, rem, p.samplesRendered)) return std::nullopt;
        if (!readUint16LE(ptr, rem, p.numChannels)) return std::nullopt;
        if (!readUint64LE(ptr, rem, p.durationUs)) return std::nullopt;

        uint32_t floatCount = 0;
        if (!readUint32LE(ptr, rem, floatCount)) return std::nullopt;

        // Verificación defensiva contra allocs masivos o malformados
        if (floatCount > (kIpcMaxBlockSamples * kIpcMaxAudioChannels)) return std::nullopt;

        const size_t expectedBytes = static_cast<size_t>(floatCount) * sizeof(float);
        if (rem < expectedBytes) return std::nullopt;

        if (floatCount > 0)
        {
            p.audioData.resize(floatCount);
            std::memcpy(p.audioData.data(), ptr, expectedBytes);
            ptr += expectedBytes;
            rem -= expectedBytes;
        }

        return p;
    }
};

} // namespace abdaudiolab::ipc


