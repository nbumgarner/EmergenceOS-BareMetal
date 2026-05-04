#ifndef EOS_VMX_IMPL_HPP
#define EOS_VMX_IMPL_HPP

#include <stdint.h>
#include "memory.hpp"

namespace EmergenceOS {

    /**
     * VMX Field Encodings (Partial)
     */
    enum VMXField {
        GUEST_ES_SELECTOR = 0x00000800,
        GUEST_CS_SELECTOR = 0x00000802,
        GUEST_SS_SELECTOR = 0x00000804,
        GUEST_DS_SELECTOR = 0x00000806,
        GUEST_CR0 = 0x00006800,
        GUEST_CR3 = 0x00006802,
        GUEST_CR4 = 0x00006804,
        GUEST_RIP = 0x0000681E,
        GUEST_RSP = 0x0000681C,
        HOST_CR0 = 0x00006C00,
        HOST_CR3 = 0x00006C02,
        HOST_CR4 = 0x00006C04,
        HOST_RIP = 0x00006C16,
        HOST_RSP = 0x00006C14,
        VMCS_LINK_POINTER = 0x00002800,
        EXIT_REASON = 0x00004402,
    };

    inline void vmwrite(uint64_t field, uint64_t value) {
        __asm__ __volatile__ ("vmwrite %0, %1" : : "r"(value), "r"(field));
    }

    inline uint64_t vmread(uint64_t field) {
        uint64_t val;
        __asm__ __volatile__ ("vmread %1, %0" : "=r"(val) : "r"(field));
        return val;
    }

    class VMXController {
    private:
        uint8_t* vmxon_region;
        uint8_t* vmcs_region;

    public:
        VMXController() {
            vmxon_region = (uint8_t*)g_pmm->allocate(4096);
            vmcs_region = (uint8_t*)g_pmm->allocate(4096);
            kmemset(vmxon_region, 0, 4096);
            kmemset(vmcs_region, 0, 4096);
        }

        bool enable() {
            // 0. Enable VMX in IA32_FEATURE_CONTROL (MSR 0x3A)
            uint32_t lo, hi;
            __asm__ __volatile__ ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0x3A));
            if (!(lo & 1)) {
                // Lock bit (bit 0) not set, enable VMX outside SMX (bit 2) and lock
                lo |= 0x5; 
                __asm__ __volatile__ ("wrmsr" : : "a"(lo), "d"(hi), "c"(0x3A));
            } else if (!(lo & 0x4)) {
                // Locked but VMX disabled by BIOS/Firmware
                return false;
            }

            // 1. Check VMX support (CPUID.1:ECX.bit 5)
            uint32_t ecx;
            __asm__ __volatile__ ("cpuid" : "=c"(ecx) : "a"(1));
            if (!(ecx & (1 << 5))) return false;

            // 2. Set CR4.VMXE (bit 13) and apply fixed bits
            uint64_t cr4, cr0;
            __asm__ __volatile__ ("mov %%cr4, %0" : "=r"(cr4));
            cr4 |= (1 << 13);
            
            // Apply VMX fixed bits for CR4
            uint32_t f0, f1;
            __asm__ __volatile__ ("rdmsr" : "=a"(f0), "=d"(f1) : "c"(0x488)); // IA32_VMX_CR4_FIXED0
            cr4 |= f0;
            __asm__ __volatile__ ("rdmsr" : "=a"(f0), "=d"(f1) : "c"(0x489)); // IA32_VMX_CR4_FIXED1
            cr4 &= f0; // In rdmsr, 'a' is the low 32 bits (the mask)
            __asm__ __volatile__ ("mov %0, %%cr4" : : "r"(cr4));

            // Apply VMX fixed bits for CR0
            __asm__ __volatile__ ("mov %%cr0, %0" : "=r"(cr0));
            __asm__ __volatile__ ("rdmsr" : "=a"(f0), "=d"(f1) : "c"(0x486)); // IA32_VMX_CR0_FIXED0
            cr0 |= f0;
            __asm__ __volatile__ ("rdmsr" : "=a"(f0), "=d"(f1) : "c"(0x487)); // IA32_VMX_CR0_FIXED1
            cr0 &= f0;
            __asm__ __volatile__ ("mov %0, %%cr0" : : "r"(cr0));

            // 3. VMXON
            uint64_t revision_id_full;
            uint32_t r_lo, r_hi;
            __asm__ __volatile__ ("rdmsr" : "=a"(r_lo), "=d"(r_hi) : "c"(0x480));
            
            uint32_t* vmxon_ptr = (uint32_t*)vmxon_region;
            *vmxon_ptr = r_lo; // VMX revision identifier must be in first 31 bits

            uintptr_t vmxon_phys = (uintptr_t)vmxon_region;
            uint8_t error;
            __asm__ __volatile__ (
                "vmxon %[ptr];"
                "setna %[err]"
                : [err] "=g"(error)
                : [ptr] "m"(vmxon_phys)
                : "cc", "memory"
            );
            if (error) return false;

            // 4. VMPTRLD
            uint32_t* vmcs_ptr = (uint32_t*)vmcs_region;
            *vmcs_ptr = r_lo;
            uintptr_t vmcs_phys = (uintptr_t)vmcs_region;
            __asm__ __volatile__ (
                "vmptrld %[ptr];"
                "setna %[err]"
                : [err] "=g"(error)
                : [ptr] "m"(vmcs_phys)
                : "cc", "memory"
            );
            
            return !error;
        }

        void setup_guest(uintptr_t entry_point, uintptr_t stack_ptr) {
            vmwrite(VMCS_LINK_POINTER, 0xFFFFFFFFFFFFFFFFULL);
            vmwrite(GUEST_RIP, entry_point);
            vmwrite(GUEST_RSP, stack_ptr);
            
            // Set basic Host state to current kernel
            uint64_t cr0, cr3, cr4;
            __asm__ __volatile__ ("mov %%cr0, %0" : "=r"(cr0));
            __asm__ __volatile__ ("mov %%cr3, %0" : "=r"(cr3));
            __asm__ __volatile__ ("mov %%cr4, %0" : "=r"(cr4));
            vmwrite(HOST_CR0, cr0);
            vmwrite(HOST_CR3, cr3);
            vmwrite(HOST_CR4, cr4);
        }

        void launch() {
            uint8_t error;
            __asm__ __volatile__ ("vmlaunch; setna %0" : "=g"(error) : : "cc");
            // If vmlaunch returns, something failed
        }
    };
}

#endif
