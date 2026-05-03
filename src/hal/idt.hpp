#ifndef EOS_IDT_HPP
#define EOS_IDT_HPP

#include "memory.hpp"
#include "serial.hpp"
#include "topology.hpp"

namespace EmergenceOS {

struct IDTEntry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct IDTPtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

extern IDTEntry idt[256];
extern IDTPtr idt_ptr;

extern Emergence::SubstrateManifold* g_manifold_instance;
extern volatile uint64_t g_temporal_pulse;
extern volatile uint8_t  g_fuzz_active;
extern volatile uint8_t  g_fault_detected;

extern "C" void exception_handler_ud();
extern "C" void exception_handler_df();
extern "C" void exception_handler_gp();
extern "C" void exception_handler_nm();
extern "C" void timer_interrupt_handler();

extern "C" void handle_timer_pulse();
void init_pic();
void set_idt_gate(int n, uint64_t handler);
void init_idt();

// BARE-METAL LOGGING
inline void raw_putc(char c) {
    uint16_t p_stat = 0x3FD; uint16_t p_data = 0x3F8; uint8_t status;
    for(int i=0; i<10000; i++) {
        __asm__ __volatile__ ("inb %1, %0" : "=a"(status) : "Nd"(p_stat));
        if (status & 0x20) break;
    }
    __asm__ __volatile__ ("outb %0, %1" : : "a"((uint8_t)c), "Nd"(p_data));
}

inline void raw_print(const char* s) { while (*s) raw_putc(*s++); }
inline void raw_print_hex(uint64_t val) {
    const char* hex = "0123456789ABCDEF";
    for (int i = 15; i >= 0; i--) raw_putc(hex[(val >> (i * 4)) & 0xF]);
}

} // namespace EmergenceOS
#endif
