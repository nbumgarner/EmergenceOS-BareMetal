#include "memory.hpp"

namespace EmergenceOS {

    PhysicalMemory* g_pmm = nullptr;

    PhysicalMemory::PhysicalMemory(uintptr_t start, uintptr_t total) {
        current_address = start;
        max_address = start + total;
    }

    void* PhysicalMemory::allocate(size_t size) {
        if (current_address % 16384 != 0) 
            current_address += 16384 - (current_address % 16384);
        
        if (current_address + size >= max_address) return (void*)0;
        
        void* ram = (void*)current_address;
        current_address += size;
        return ram;
    }
}

extern "C" size_t kstrlen(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

extern "C" void* kmemcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

extern "C" void* kmemset(void* s, int c, size_t n) {
    uint8_t* p = (uint8_t*)s;
    for (size_t i = 0; i < n; i++) p[i] = (uint8_t)c;
    return s;
}

extern "C" void* memset(void* s, int c, size_t n) { return kmemset(s, c, n); }

void* operator new(size_t size) { return EmergenceOS::g_pmm->allocate(size); }
void* operator new[](size_t size) { return EmergenceOS::g_pmm->allocate(size); }
void operator delete(void*) noexcept {}
void operator delete(void*, size_t) noexcept {}
void operator delete[](void*) noexcept {}
void operator delete[](void*, size_t) noexcept {}

extern "C" {
    void* __dso_handle = (void*)0;
    int __cxa_atexit(void (*)(void*), void*, void*) { return 0; }
}
