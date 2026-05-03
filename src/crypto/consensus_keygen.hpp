#ifndef CONSENSUS_KEYGEN_HPP
#define CONSENSUS_KEYGEN_HPP

#include <stdint.h>

namespace Consensus {

    inline void generate_device_id(char* output_buffer) {
        uint64_t hardware_id = 0;

        #if defined(__x86_64__)
            unsigned int eax, ebx, ecx, edx;
            __asm__ __volatile__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
            unsigned int signature = eax;
            unsigned int features = edx;

            __asm__ __volatile__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000002));
            unsigned int brand_chunk = eax ^ ebx ^ ecx ^ edx;

            hardware_id = ((uint64_t)signature << 32) | (features ^ brand_chunk);
        #elif defined(__aarch64__)
            uint64_t midr;
            uint64_t mpidr;
            __asm__ __volatile__("mrs %0, midr_el1" : "=r"(midr));
            __asm__ __volatile__("mrs %0, mpidr_el1" : "=r"(mpidr));
            hardware_id = (midr << 32) | (mpidr & 0xFFFFFFFF);
        #else
            #error "Unsupported architecture for Sovereign Consensus"
        #endif

        // Fixed-point chaotic map (Logistic Map)
        // x_next = r * x * (1 - x)
        // r = 3.99 (high chaos regime)
        // We use 32-bit fixed point with 32 bits of precision for the state
        uint64_t x = (hardware_id % 0xFFFFFFFF);
        if (x == 0) x = 0x12345678;

        const char hex_chars[] = "0123456789ABCDEF";
        for (int i = 0; i < 15; i++) {
            // x = (399 * x * (0xFFFFFFFF - x)) / (100 * 2^32)
            // Simplified: x = (4085 * (x >> 10) * ((0xFFFFFFFF - x) >> 10)) >> 12
            uint64_t term1 = x;
            uint64_t term2 = 0xFFFFFFFFULL - x;
            x = (4085ULL * (term1 >> 8) * (term2 >> 8)) >> 16;
            
            // Re-seed slightly if it settles (logistic map property)
            if (x < 0x1000) x ^= hardware_id;

            int index = (x % 16);
            output_buffer[i] = hex_chars[index];
        }
        output_buffer[15] = '\0';
    }
}

#endif
