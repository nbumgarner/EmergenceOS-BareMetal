#include "idt.hpp"

namespace EmergenceOS {

IDTEntry idt[256];
IDTPtr idt_ptr;

volatile uint64_t g_temporal_pulse = 0;
volatile uint8_t  g_fuzz_active = 0;
volatile uint8_t  g_fault_detected = 0;

extern "C" void handle_timer_pulse() {
    g_temporal_pulse++;
}

void init_pic() {
    __asm__ __volatile__ ("outb %%al, $0x20" : : "a"((uint8_t)0x11));
    __asm__ __volatile__ ("outb %%al, $0xA0" : : "a"((uint8_t)0x11));
    __asm__ __volatile__ ("outb %%al, $0x21" : : "a"((uint8_t)0x20));
    __asm__ __volatile__ ("outb %%al, $0xA1" : : "a"((uint8_t)0x28));
    __asm__ __volatile__ ("outb %%al, $0x21" : : "a"((uint8_t)0x04));
    __asm__ __volatile__ ("outb %%al, $0xA1" : : "a"((uint8_t)0x02));
    __asm__ __volatile__ ("outb %%al, $0x21" : : "a"((uint8_t)0x01));
    __asm__ __volatile__ ("outb %%al, $0xA1" : : "a"((uint8_t)0x01));
    __asm__ __volatile__ ("outb %%al, $0x21" : : "a"((uint8_t)0xFE));
    __asm__ __volatile__ ("outb %%al, $0xA1" : : "a"((uint8_t)0xFF));
}

void set_idt_gate(int n, uint64_t handler) {
    idt[n].offset_low = handler & 0xFFFF;
    idt[n].selector = 0x08;
    idt[n].ist = 0;
    idt[n].type_attr = 0x8E;
    idt[n].offset_mid = (handler >> 16) & 0xFFFF;
    idt[n].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[n].reserved = 0;
}

void init_idt() {
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uintptr_t)&idt;
    set_idt_gate(6, (uintptr_t)exception_handler_ud);
    set_idt_gate(7, (uintptr_t)exception_handler_nm);
    set_idt_gate(8, (uintptr_t)exception_handler_df);
    set_idt_gate(13, (uintptr_t)exception_handler_gp);
    set_idt_gate(32, (uintptr_t)timer_interrupt_handler);
    init_pic();
    __asm__ __volatile__ ("lidt %0" : : "m"(idt_ptr));
}

extern "C" void dump_registers(uint64_t rip, uint64_t rax, uint64_t rbx, uint64_t rcx, uint64_t rdx, uint64_t err) {
    raw_print("\n!!! SOVEREIGN FAULT !!!\n RIP: "); raw_print_hex(rip);
    raw_print(" RAX: "); raw_print_hex(rax);
    raw_print(" RBX: "); raw_print_hex(rbx);
    while(1) { __asm__ __volatile__("hlt"); }
}

extern "C" void resolve_hic(uint64_t* rip, uint64_t* rax, uint64_t* rbx) {
    uint8_t* code = (uint8_t*)*rip;
    if (g_fuzz_active) {
        g_fault_detected = 1;
        *rip += 2; // Real physical skip
        return;
    }
    if (code[0] == 0x0F && code[1] == 0xFF) {
        if (!g_manifold_instance) { *rax = 0x0A; *rip += 2; return; }
        uint64_t intent = *rax ^ *rbx;
        Emergence::Value128 res = g_manifold_instance->holographic_fetch(intent);
        *rax = res.lo;
        *rip += 2;
    } else {
        dump_registers(*rip, *rax, *rbx, 0, 0, 0);
    }
}

extern "C" void handle_gp_fault(uint64_t rip, uint64_t err) {
    raw_print("\n[GP] Fault at "); raw_print_hex(rip);
    while(1);
}

extern "C" void handle_nm_fault(uint64_t rip) {
    raw_print("\n[NM] SSE Unavailable at "); raw_print_hex(rip);
    while(1);
}

}
