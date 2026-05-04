#include "memory.hpp"
#include "serial.hpp"
#include "pci.hpp"
#include "substrate_io.hpp"
#include "topology.hpp"
#include "state_engine.hpp"
#include "idt.hpp"
#include "vmx_impl.hpp"
#include "vga.hpp"
#include "consensus_keygen.hpp"
#include "timer.hpp"
#include "keyboard.hpp"
#include "dynamic_allocator.hpp"
#include "ahci.hpp"
#include "substrate_service.hpp"
#include "virtio_blk.hpp"
#include "logic_transducer.hpp"
#include "argon2_hardener.hpp"
#include "audit_log.hpp"
#include "neumann_control_panel.hpp"
#include "sha256.hpp"

static uint8_t pmm_storage[sizeof(EmergenceOS::PhysicalMemory)] __attribute__((aligned(16)));
static uint8_t manifold_storage[sizeof(Emergence::SubstrateManifold)] __attribute__((aligned(16)));
static uint8_t service_storage[sizeof(EmergenceOS::SubstrateService)] __attribute__((aligned(16)));
static uint8_t virtio_storage[sizeof(EmergenceOS::VirtIOBlock)] __attribute__((aligned(16)));

namespace EmergenceOS {
    Emergence::SubstrateManifold* g_manifold_instance = nullptr;
    Graphics* g_vga = nullptr;
    DynamicResourceAllocator* g_res = nullptr;
    AHCIDriver* g_disk = nullptr;
    SubstrateService* g_service = nullptr;
    VirtIOBlock* g_virtio = nullptr;
    Emergence::LogicTransducer* g_transducer = nullptr;
    VMXController* g_vmx = nullptr;
    Emergence::AuditLogger* g_audit = nullptr;
    NeumannControlPanel* g_control = nullptr;
    Emergence::Seed g_master_seed;
    
    bool g_in_shell = true;
    bool g_in_control_panel = false;
    bool g_hardware_locked = false;

    inline uint64_t rdtsc() {
        uint32_t lo, hi;
        __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
        return ((uint64_t)hi << 32) | lo;
    }

    void kstrcmp(const char* s1, const char* s2, bool& match) {
        match = true;
        int i = 0;
        while(s1[i] != '\0' || s2[i] != '\0') {
            if (s1[i] != s2[i]) { match = false; return; }
            i++;
        }
    }

    bool kstarts_with(const char* str, const char* prefix) {
        int i = 0;
        while(prefix[i] != '\0') {
            if (str[i] != prefix[i]) return false;
            i++;
        }
        return true;
    }

    uint32_t hypercube_frame = 0;
    int phase_lock_divisor = 1;

    void sovereign_shell(Keyboard& kb) {
        char cmd[128];
        int cmd_idx = 0;
        kmemset(cmd, 0, 128);

        while(g_in_shell) {
            if (g_in_control_panel) {
                g_control->draw_panel(g_res->get_active_cores(), g_res->get_active_nodes(), nullptr);
                g_vga->draw_spinning_cube(hypercube_frame, 0, 0, (hypercube_frame % (60 * phase_lock_divisor) == 0), 0, 0);
                
                g_vga->print_at("[PHOENIX]> ", 320, 500, 0x0000FF00);
                g_vga->print_at(cmd, 410, 500, 0x00FFFFFF);
                if ((hypercube_frame / 16) % 2) g_vga->print_at("_", 410 + (cmd_idx * 8), 500, 0x0000FF00);
            }

            g_vga->swap_buffers();
            hypercube_frame++;
            
            char c = kb.read_char_nonblock();
            if (c == 0) continue;

            if (c == '+') phase_lock_divisor++;
            if (c == '-' && phase_lock_divisor > 1) phase_lock_divisor--;

            if (c == '\n' || c == '\r') {
                cmd[cmd_idx] = '\0';
                if (kstarts_with(cmd, "back")) {
                    g_in_control_panel = false;
                } else if (kstarts_with(cmd, "fold")) {
                    g_res->fold_manifold(1);
                } else if (kstarts_with(cmd, "verify")) {
                    static const uint8_t EXPECTED[32] = {0x33,0x9A,0x2A,0xFC,0x43,0x35,0x91,0x23,0x1F,0x0A,0x99,0x87,0x6A,0x1C,0xDE,0x43,0x21,0x7F,0xA3,0x99,0xBC,0xD1,0x23,0x4F,0x6E,0x1A,0x2B,0x3C,0x4D,0x5E,0x6F,0x7A};
                    g_control->draw_panel(g_res->get_active_cores(), g_res->get_active_nodes(), EXPECTED);
                    g_vga->swap_buffers();
                    for(volatile int delay=0; delay<100000000; delay++);
                }
                kmemset(cmd, 0, 128);
                cmd_idx = 0;
                continue;
            }

            if (c == '\b' && cmd_idx > 0) {
                cmd[--cmd_idx] = '\0';
            } else if (cmd_idx < 127 && c >= 32) {
                cmd[cmd_idx++] = c;
            }
        }
    }
}

extern "C" void kmain(uint32_t magic, uint32_t info_addr) {
    EmergenceOS::g_pmm = new (pmm_storage) EmergenceOS::PhysicalMemory(0x2000000, 0x40000000);
    EmergenceOS::init_idt();
    EmergenceOS::Timer::initialize(100);
    __asm__ __volatile__ ("sti");

    EmergenceOS::PCIController pci;
    EmergenceOS::UnifiedConsole serial;
    uintptr_t ahci_base = pci.find_ahci_base();
    if (ahci_base) {
        static uint8_t ahci_storage[sizeof(EmergenceOS::AHCIDriver)] __attribute__((aligned(16)));
        EmergenceOS::g_disk = new (ahci_storage) EmergenceOS::AHCIDriver(ahci_base);
        EmergenceOS::g_disk->initialize(&serial);
    }

    char hwid[16]; Consensus::generate_device_id(hwid);
    Emergence::Seed master = Emergence::Argon2Sovereign::derive("phoenix-v1", hwid, nullptr);
    EmergenceOS::g_master_seed = master;

    EmergenceOS::g_manifold_instance = new (manifold_storage) Emergence::SubstrateManifold();
    EmergenceOS::g_manifold_instance->manual_init(32, EmergenceOS::g_disk);
    EmergenceOS::g_manifold_instance->initialize(master);

    static uint8_t trans_storage[sizeof(Emergence::LogicTransducer)] __attribute__((aligned(16)));
    EmergenceOS::g_transducer = new (trans_storage) Emergence::LogicTransducer(*EmergenceOS::g_manifold_instance);

    static uint8_t audit_storage[sizeof(Emergence::AuditLogger)] __attribute__((aligned(16)));
    EmergenceOS::g_audit = new (audit_storage) Emergence::AuditLogger(*EmergenceOS::g_manifold_instance);
    EmergenceOS::g_audit->log(Emergence::AuditLogger::EVENT_BOOT);

    uint64_t lfb = 0; uint32_t w=0, h=0, p=0;
    bool vga_active = false;
    if (magic == 0x36d76289) {
        uint32_t total_size = *(uint32_t*)(uintptr_t)info_addr;
        uint8_t* tag = (uint8_t*)(uintptr_t)(info_addr + 8);
        while (tag < (uint8_t*)(uintptr_t)(info_addr + total_size)) {
            uint32_t type = *(uint32_t*)tag;
            uint32_t size = *(uint32_t*)(tag + 4);
            if (type == 0) break;
            if (type == 8) { 
                lfb = *(uint64_t*)(tag + 8);
                p = *(uint32_t*)(tag + 16);
                w = *(uint32_t*)(tag + 20);
                h = *(uint32_t*)(tag + 24);
                vga_active = true;
            }
            tag += ((size + 7) & ~7);
        }
    }

    static uint8_t vga_storage[sizeof(EmergenceOS::Graphics)] __attribute__((aligned(16)));
    EmergenceOS::g_vga = new (vga_storage) EmergenceOS::Graphics();
    if (vga_active) EmergenceOS::g_vga->initialize(lfb, w, h, p);

    static uint8_t res_storage[sizeof(EmergenceOS::DynamicResourceAllocator)] __attribute__((aligned(16)));
    EmergenceOS::g_res = new (res_storage) EmergenceOS::DynamicResourceAllocator(0x40000000);

    static uint8_t vmx_storage[sizeof(EmergenceOS::VMXController)] __attribute__((aligned(16)));
    EmergenceOS::g_vmx = new (vmx_storage) EmergenceOS::VMXController();
    bool vmx_ready = EmergenceOS::g_vmx->enable();
    
    static uint8_t control_storage[sizeof(EmergenceOS::NeumannControlPanel)] __attribute__((aligned(16)));
    EmergenceOS::g_control = new (control_storage) EmergenceOS::NeumannControlPanel(EmergenceOS::g_vga, EmergenceOS::g_res, master);
    
    EmergenceOS::Keyboard kb;
    int selection = 0;
    while(true) {
        if (EmergenceOS::g_vga) {
            EmergenceOS::g_vga->clear(0x00080808);
            EmergenceOS::g_vga->print_at("=== CONSENSUS ENTERPRISE HYPERVISOR ===", 300, 200, 0x0000FFFF);
            if (vmx_ready) EmergenceOS::g_vga->print_at("VMX_ACCELERATION: [ENABLED]", 300, 150, 0x0000FF00);
            if (selection == 0) EmergenceOS::g_vga->print_at("> 1. Launch Sovereign Shell", 250, 240, 0x0000FF00);
            else EmergenceOS::g_vga->print_at("  1. Launch Sovereign Shell", 250, 240, 0x00FFFFFF);
            if (selection == 1) EmergenceOS::g_vga->print_at("> 2. Neumann Control Panel", 250, 260, 0x0000FF00);
            else EmergenceOS::g_vga->print_at("  2. Neumann Control Panel", 250, 260, 0x00FFFFFF);
            EmergenceOS::g_vga->swap_buffers();
        }
        char c = kb.read_char();
        if (c == 'w') selection = 0;
        if (c == 's') selection = 1;
        if (c == '\n' || c == '\r') break;
    }

    if (selection == 0) {
        EmergenceOS::sovereign_shell(kb);
    } else {
        EmergenceOS::g_in_control_panel = true;
        EmergenceOS::sovereign_shell(kb);
    }

    while(true) {
        kb.update_state();
        if (kb.consume_hotkey()) EmergenceOS::sovereign_shell(kb);
    }
}
