#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace abdaudiolab::synth
{

/**
 * @brief Implementación autónoma y determinista de SHA-256 conforme a FIPS 180-4.
 * Sin dependencias externas de OpenSSL o módulos JUCE deprecados.
 */
class Sha256
{
public:
    Sha256() { reset(); }

    void reset() noexcept
    {
        state_[0] = 0x6a09e667;
        state_[1] = 0xbb67ae85;
        state_[2] = 0x3c6ef372;
        state_[3] = 0xa54ff53a;
        state_[4] = 0x510e527f;
        state_[5] = 0x9b05688c;
        state_[6] = 0x1f83d9ab;
        state_[7] = 0x5be0cd19;
        count_ = 0;
        bufferLen_ = 0;
    }

    void update(const void* data, size_t len) noexcept
    {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        count_ += len;

        while (len > 0)
        {
            size_t copyLen = std::min(len, size_t(64 - bufferLen_));
            for (size_t i = 0; i < copyLen; ++i)
                buffer_[bufferLen_ + i] = p[i];

            bufferLen_ += copyLen;
            p += copyLen;
            len -= copyLen;

            if (bufferLen_ == 64)
            {
                transform(buffer_);
                bufferLen_ = 0;
            }
        }
    }

    void update(std::string_view sv) noexcept
    {
        update(sv.data(), sv.size());
    }

    [[nodiscard]] std::string finalHex() noexcept
    {
        uint8_t bits[8];
        uint64_t bitCount = count_ * 8;
        for (int i = 0; i < 8; ++i)
            bits[i] = static_cast<uint8_t>((bitCount >> ((7 - i) * 8)) & 0xff);

        uint8_t pad = 0x80;
        update(&pad, 1);

        uint8_t zero = 0x00;
        while (bufferLen_ != 56)
        {
            update(&zero, 1);
        }

        update(bits, 8);

        char hexBuf[65];
        for (int i = 0; i < 8; ++i)
        {
            std::snprintf(hexBuf + i * 8, 9, "%08x", state_[i]);
        }
        hexBuf[64] = '\0';
        return std::string(hexBuf);
    }

    [[nodiscard]] static std::string computeHex(const void* data, size_t len)
    {
        Sha256 ctx;
        ctx.update(data, len);
        return ctx.finalHex();
    }

    [[nodiscard]] static std::string computeHex(std::string_view sv)
    {
        Sha256 ctx;
        ctx.update(sv);
        return ctx.finalHex();
    }

private:
    static inline uint32_t rotr(uint32_t x, uint32_t n) noexcept { return (x >> n) | (x << (32 - n)); }
    static inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) noexcept { return (x & y) ^ (~x & z); }
    static inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) noexcept { return (x & y) ^ (x & z) ^ (y & z); }
    static inline uint32_t sig0(uint32_t x) noexcept { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
    static inline uint32_t sig1(uint32_t x) noexcept { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
    static inline uint32_t gam0(uint32_t x) noexcept { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
    static inline uint32_t gam1(uint32_t x) noexcept { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

    void transform(const uint8_t block[64]) noexcept
    {
        static const uint32_t K[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
        };

        uint32_t w[64];
        for (int i = 0; i < 16; ++i)
        {
            w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
                   (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
                   (static_cast<uint32_t>(block[i * 4 + 3]));
        }
        for (int i = 16; i < 64; ++i)
        {
            w[i] = gam1(w[i - 2]) + w[i - 7] + gam0(w[i - 15]) + w[i - 16];
        }

        uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
        uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];

        for (int i = 0; i < 64; ++i)
        {
            uint32_t t1 = h + sig1(e) + ch(e, f, g) + K[i] + w[i];
            uint32_t t2 = sig0(a) + maj(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

    uint32_t state_[8];
    uint64_t count_ { 0 };
    uint8_t buffer_[64];
    size_t bufferLen_ { 0 };
};

} // namespace abdaudiolab::synth
