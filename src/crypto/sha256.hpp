#ifndef EOS_SHA256_HPP
#define EOS_SHA256_HPP

#include <stdint.h>
#include "memory.hpp"

namespace Emergence {

    class SHA256 {
    private:
        uint32_t state[8];
        uint64_t count;
        uint8_t buffer[64];

        inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
        inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
        inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
        inline uint32_t ep0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
        inline uint32_t ep1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
        inline uint32_t sig0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
        inline uint32_t sig1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

        void transform(const uint8_t* data) {
            uint32_t a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];
            static const uint32_t k[64] = {
                0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
                0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
                0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
                0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
                0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
                0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
                0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
                0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
            };

            for (i = 0, j = 0; i < 16; ++i, j += 4)
                m[i] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) | (data[j + 3]);
            for ( ; i < 64; ++i)
                m[i] = sig1(m[i - 2]) + m[i - 7] + sig0(m[i - 15]) + m[i - 16];

            a = state[0]; b = state[1]; c = state[2]; d = state[3];
            e = state[4]; f = state[5]; g = state[6]; h = state[7];

            for (i = 0; i < 64; ++i) {
                t1 = h + ep1(e) + ch(e, f, g) + k[i] + m[i];
                t2 = ep0(a) + maj(a, b, c);
                h = g; g = f; f = e; e = d + t1;
                d = c; c = b; b = a; a = t1 + t2;
            }

            state[0] += a; state[1] += b; state[2] += c; state[3] += d;
            state[4] += e; state[5] += f; state[6] += g; state[7] += h;
        }

    public:
        SHA256() {
            state[0] = 0x6a09e667; state[1] = 0xbb67ae85;
            state[2] = 0x3c6ef372; state[3] = 0xa54ff53a;
            state[4] = 0x510e527f; state[5] = 0x9b05688c;
            state[6] = 0x1f83d9ab; state[7] = 0x5be0cd19;
            count = 0;
        }

        void update(const uint8_t* data, size_t len) {
            size_t i = 0;
            if (count % 64) {
                size_t left = 64 - (count % 64);
                size_t to_copy = (len < left) ? len : left;
                kmemcpy(&buffer[count % 64], data, to_copy);
                count += to_copy;
                i += to_copy;
                if (count % 64 == 0) transform(buffer);
            }
            while (i + 64 <= len) {
                transform(data + i);
                count += 64;
                i += 64;
            }
            if (i < len) {
                kmemcpy(buffer, data + i, len - i);
                count += len - i;
            }
        }

        void finalize(uint8_t hash[32]) {
            uint64_t bit_len = count * 8;
            uint8_t pad = 0x80;
            update(&pad, 1);
            pad = 0x00;
            while ((count % 64) != 56) update(&pad, 1);
            uint8_t len_bytes[8];
            for (int i = 7; i >= 0; i--) len_bytes[i] = (uint8_t)(bit_len >> ((7 - i) * 8));
            update(len_bytes, 8);
            for (int i = 0; i < 8; i++) {
                hash[i * 4] = (state[i] >> 24) & 0xFF;
                hash[i * 4 + 1] = (state[i] >> 16) & 0xFF;
                hash[i * 4 + 2] = (state[i] >> 8) & 0xFF;
                hash[i * 4 + 3] = state[i] & 0xFF;
            }
        }

        static bool verify_seed(const Seed& s, const uint8_t expected_hash[32]) {
            SHA256 sha;
            uint8_t data[16];
            kmemcpy(data, &s.hi, 8);
            kmemcpy(data + 8, &s.lo, 8);
            sha.update(data, 16);
            uint8_t hash[32];
            sha.finalize(hash);
            for (int i=0; i<32; i++) if (hash[i] != expected_hash[i]) return false;
            return true;
        }
    };
}
#endif