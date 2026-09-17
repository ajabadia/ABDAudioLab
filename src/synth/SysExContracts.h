#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <numeric>
#include <cstdio>
#include "Sha256.h"

namespace abdaudiolab::synth
{

/**
 * @brief Tipos formalizados de dump SysEx para síntesis DX7 / FM (Fase 20.11 T3.1).
 */
enum class Dx7SysExFormat
{
    SingleVoiceDump,   /**< Dump de 1 voz (163 bytes estándar con framing F0..F7). */
    Bank32VoiceDump,   /**< Dump de banco de 32 voces (4104 bytes estándar con framing F0..F7). */
    GenericSysEx       /**< SysEx arbitrario delimitado por F0..F7. */
};

inline const char* dx7SysExFormatToString(Dx7SysExFormat fmt) noexcept
{
    switch (fmt)
    {
        case Dx7SysExFormat::SingleVoiceDump: return "dx7_single_voice";
        case Dx7SysExFormat::Bank32VoiceDump: return "dx7_32_voice_bank";
        case Dx7SysExFormat::GenericSysEx:    return "generic_sysex";
    }
    return "unknown";
}

/**
 * @brief Códigos de error y diagnóstico de validación estructural de SysEx.
 */
enum class SysExValidationStatus
{
    Valid,
    MissingSysExFraming,    /**< Falta 0xF0 inicial o 0xF7 final. */
    InvalidManufacturerId,  /**< Manufacturer ID no coincide (ej. requiere 0x43 Yamaha). */
    LengthMismatch,         /**< Longitud no coincide con el formato esperado. */
    DataByteHighBitSet,     /**< Un byte de datos tiene el bit 7 activo (>= 128). */
    ChecksumMismatch        /**< El checksum de 7 bits no coincide con la suma complementaria. */
};

inline const char* sysExValidationStatusToString(SysExValidationStatus st) noexcept
{
    switch (st)
    {
        case SysExValidationStatus::Valid:                  return "valid";
        case SysExValidationStatus::MissingSysExFraming:    return "missing_sysex_framing";
        case SysExValidationStatus::InvalidManufacturerId:  return "invalid_manufacturer_id";
        case SysExValidationStatus::LengthMismatch:         return "length_mismatch";
        case SysExValidationStatus::DataByteHighBitSet:     return "data_byte_high_bit_set";
        case SysExValidationStatus::ChecksumMismatch:       return "checksum_mismatch";
    }
    return "unknown";
}

/**
 * @brief Resultado estructurado de validación formal de SysEx.
 */
struct SysExValidationResult
{
    bool isValid { false };
    SysExValidationStatus status { SysExValidationStatus::Valid };
    std::string message;
    uint8_t calculatedChecksum { 0 };
    uint8_t receivedChecksum { 0 };
    size_t byteCount { 0 };
};

/**
 * @brief Representación inmutable de artefacto SysEx con trazabilidad SHA-256 (Fase 20.11 T3.1).
 */
struct SysExArtifact
{
    std::string format;
    std::vector<uint8_t> bytes;
    std::string sha256;
    std::string semanticStatus;

    [[nodiscard]] bool verifyFixity() const
    {
        if (bytes.empty())
            return false;
        return sha256 == Sha256::computeHex(bytes.data(), bytes.size());
    }

    static SysExArtifact create(const std::string& fmt,
                                const std::vector<uint8_t>& data,
                                const std::string& status = "valid")
    {
        SysExArtifact art;
        art.format = fmt;
        art.bytes = data;
        art.sha256 = Sha256::computeHex(data.data(), data.size());
        art.semanticStatus = status;
        return art;
    }
};

/**
 * @brief Validador metrológico de mensajes SysEx para sintetizadores Yamaha DX7 / Dexed.
 *
 * Valida de forma estricta:
 * 1. Delimitación de framing MIDI (0xF0 inicial, 0xF7 final).
 * 2. Manufacturer ID (0x43 para Yamaha).
 * 3. Identificador de formato / substatus (0x00 para voz individual, 0x09 para banco de 32 voces).
 * 4. Rango de 7 bits (0..127) en todos los bytes del payload.
 * 5. Checksum canónico de 7 bits (complemento a 2 de la suma de bytes de datos).
 * 6. Longitud configurable por tipo de fixture para variantes.
 */
class Dx7SysExValidator
{
public:
    static constexpr uint8_t kSysExStart = 0xF0;
    static constexpr uint8_t kSysExEnd = 0xF7;
    static constexpr uint8_t kYamahaManufacturerId = 0x43;

    static constexpr size_t kSingleVoiceDumpLength = 163;  // F0 43 00 00 01 1B [155 data bytes] chk F7
    static constexpr size_t kBank32VoiceDumpLength = 4104; // F0 43 00 09 20 00 [4096 data bytes] chk F7

    /**
     * @brief Valida un mensaje SysEx DX7 de banco de 32 voces (4104 bytes estándar).
     */
    static SysExValidationResult validateBank32VoiceDump(const std::vector<uint8_t>& bytes,
                                                        size_t expectedLength = kBank32VoiceDumpLength)
    {
        return validateDx7Message(bytes, expectedLength, 0x09, 6, 4096);
    }

    /**
     * @brief Valida un mensaje SysEx DX7 de 1 voz (163 bytes estándar).
     */
    static SysExValidationResult validateSingleVoiceDump(const std::vector<uint8_t>& bytes,
                                                        size_t expectedLength = kSingleVoiceDumpLength)
    {
        return validateDx7Message(bytes, expectedLength, 0x00, 6, 155);
    }

    /**
     * @brief Valida un mensaje SysEx genérico delimitado por F0..F7 con longitud configurable.
     */
    static SysExValidationResult validateGenericSysEx(const std::vector<uint8_t>& bytes,
                                                     uint8_t expectedManufacturer = kYamahaManufacturerId)
    {
        SysExValidationResult res;
        res.byteCount = bytes.size();

        if (bytes.size() < 3)
        {
            res.isValid = false;
            res.status = SysExValidationStatus::LengthMismatch;
            res.message = "SysEx message too short: minimum length is 3 bytes";
            return res;
        }

        if (bytes.front() != kSysExStart || bytes.back() != kSysExEnd)
        {
            res.isValid = false;
            res.status = SysExValidationStatus::MissingSysExFraming;
            res.message = "SysEx framing missing: must start with 0xF0 and end with 0xF7";
            return res;
        }

        if (bytes[1] != expectedManufacturer)
        {
            res.isValid = false;
            res.status = SysExValidationStatus::InvalidManufacturerId;
            res.message = "Manufacturer ID mismatch: expected 0x" + toHex(expectedManufacturer) + ", found 0x" + toHex(bytes[1]);
            return res;
        }

        for (size_t i = 1; i < bytes.size() - 1; ++i)
        {
            if (bytes[i] >= 128)
            {
                res.isValid = false;
                res.status = SysExValidationStatus::DataByteHighBitSet;
                res.message = "Payload byte at index " + std::to_string(i) + " has MSB set (>= 128)";
                return res;
            }
        }

        res.isValid = true;
        res.status = SysExValidationStatus::Valid;
        res.message = "Generic SysEx valid";
        return res;
    }

    /**
     * @brief Calcula el checksum canónico de 7 bits de un bloque de datos DX7:
     * Checksum = (-sum(data)) & 0x7F.
     */
    static uint8_t computeDx7Checksum(const uint8_t* data, size_t len) noexcept
    {
        uint32_t sum = 0;
        for (size_t i = 0; i < len; ++i)
            sum += (data[i] & 0x7F);
        return static_cast<uint8_t>((-(static_cast<int32_t>(sum))) & 0x7F);
    }

    /**
     * @brief Valida y crea un SysExArtifact con clasificación automática de formato y fixity SHA-256.
     */
    static SysExArtifact createArtifact(const std::vector<uint8_t>& bytes)
    {
        if (bytes.size() == kSingleVoiceDumpLength)
        {
            auto res = validateSingleVoiceDump(bytes);
            return SysExArtifact::create("dx7_single_voice", bytes, res.isValid ? "valid" : "invalid");
        }
        if (bytes.size() == kBank32VoiceDumpLength)
        {
            auto res = validateBank32VoiceDump(bytes);
            return SysExArtifact::create("dx7_bank_32", bytes, res.isValid ? "valid" : "invalid");
        }
        auto res = validateGenericSysEx(bytes);
        return SysExArtifact::create("generic_sysex", bytes, res.isValid ? "valid" : "invalid");
    }

private:
    static SysExValidationResult validateDx7Message(const std::vector<uint8_t>& bytes,
                                                    size_t expectedLength,
                                                    uint8_t expectedFormatByte,
                                                    size_t dataStartIndex,
                                                    size_t dataLength)
    {
        SysExValidationResult res;
        res.byteCount = bytes.size();

        if (bytes.size() != expectedLength)
        {
            res.isValid = false;
            res.status = SysExValidationStatus::LengthMismatch;
            res.message = "Length mismatch: expected " + std::to_string(expectedLength) + " bytes, received " + std::to_string(bytes.size());
            return res;
        }

        if (bytes.front() != kSysExStart || bytes.back() != kSysExEnd)
        {
            res.isValid = false;
            res.status = SysExValidationStatus::MissingSysExFraming;
            res.message = "Missing SysEx framing: expected 0xF0 start and 0xF7 end";
            return res;
        }

        if (bytes[1] != kYamahaManufacturerId)
        {
            res.isValid = false;
            res.status = SysExValidationStatus::InvalidManufacturerId;
            res.message = "Manufacturer ID mismatch: expected 0x43 (Yamaha), found 0x" + toHex(bytes[1]);
            return res;
        }

        if (bytes[3] != expectedFormatByte)
        {
            res.isValid = false;
            res.status = SysExValidationStatus::LengthMismatch;
            res.message = "Format byte mismatch: expected 0x" + toHex(expectedFormatByte) + ", found 0x" + toHex(bytes[3]);
            return res;
        }

        // Validar que todos los bytes del payload son de 7 bits (< 128)
        for (size_t i = 1; i < bytes.size() - 1; ++i)
        {
            if (bytes[i] >= 128)
            {
                res.isValid = false;
                res.status = SysExValidationStatus::DataByteHighBitSet;
                res.message = "Payload byte at index " + std::to_string(i) + " has MSB set (>= 128)";
                return res;
            }
        }

        // Validar checksum: byte inmediatamente anterior al 0xF7
        size_t checksumIndex = bytes.size() - 2;
        uint8_t expectedChecksum = computeDx7Checksum(bytes.data() + dataStartIndex, dataLength);
        uint8_t actualChecksum = bytes[checksumIndex];

        res.calculatedChecksum = expectedChecksum;
        res.receivedChecksum = actualChecksum;

        if (actualChecksum != expectedChecksum)
        {
            res.isValid = false;
            res.status = SysExValidationStatus::ChecksumMismatch;
            res.message = "Checksum mismatch: calculated 0x" + toHex(expectedChecksum) + ", received 0x" + toHex(actualChecksum);
            return res;
        }

        res.isValid = true;
        res.status = SysExValidationStatus::Valid;
        res.message = "Valid DX7 SysEx message";
        return res;
    }

    static std::string toHex(uint8_t byte)
    {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%02X", byte);
        return std::string(buf);
    }
};

} // namespace abdaudiolab::synth
