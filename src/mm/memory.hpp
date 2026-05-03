#ifndef EOS_MEMORY_HPP
#define EOS_MEMORY_HPP

typedef unsigned long size_t;
typedef unsigned long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
typedef unsigned long uintptr_t;

extern "C" void* kmemcpy(void* dest, const void* src, size_t n);
extern "C" void* kmemset(void* s, int c, size_t n);

void* operator new(size_t size);
void* operator new[](size_t size);
inline void* operator new(size_t, void* p) throw() { return p; }
inline void* operator new[](size_t, void* p) throw() { return p; }
void operator delete(void*) noexcept;
void operator delete(void*, size_t) noexcept;

namespace EmergenceOS {

    struct HardwareBarrier {
        static inline void full_sync() {
            #if defined(__x86_64__)
                __asm__ __volatile__("mfence" ::: "memory");
            #elif defined(__aarch64__)
                __asm__ __volatile__("dsb sy" ::: "memory");
            #endif
        }

        static inline void store_sync() {
            #if defined(__x86_64__)
                __asm__ __volatile__("sfence" ::: "memory");
            #elif defined(__aarch64__)
                __asm__ __volatile__("dmb ishst" ::: "memory");
            #endif
        }

        static inline void load_sync() {
            #if defined(__x86_64__)
                __asm__ __volatile__("lfence" ::: "memory");
            #elif defined(__aarch64__)
                __asm__ __volatile__("dmb ishld" ::: "memory");
            #endif
        }
    };

    class PhysicalMemory {
    private:
        uintptr_t current_address;
        uintptr_t max_address;

    public:
        PhysicalMemory(uintptr_t start, uintptr_t total);
        void* allocate(size_t size);
    };

    extern PhysicalMemory* g_pmm;
}

#endif
