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

namespace EmergenceOS {
    Emergence::SubstrateManifold* g_manifold_instance = nullptr;
    Graphics* g_vga = nullptr;
    DynamicResourceAllocator* g_res = nullptr;
    AHCIDriver* g_disk = nullptr;
    Emergence::LogicTransducer* g_transducer = nullptr;
    VMXController* g_vmx = nullptr;
    Emergence::AuditLogger* g_audit = nullptr;
    NeumannControlPanel* g_control = nullptr;
    Emergence::Seed g_master_seed;
    
    bool g_in_shell = true;
    bool g_in_control_panel = false;
    
    uint32_t hypercube_frame = 0;
    int phase_lock_divisor = 1;
    NeumannControlPanel::Region g_focus = NeumannControlPanel::REGION_CONSOLE;

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

    void sovereign_shell(Keyboard& kb) {
        char cmd[128];
        int cmd_idx = 0;
        kmemset(cmd, 0, 128);
        uint64_t last_pulse = EmergenceOS::g_temporal_pulse;
        bool prompt_needed = true;
        uint64_t transient_pipe = 0;

        while(g_in_shell) {
            while(EmergenceOS::g_temporal_pulse == last_pulse) { __asm__ __volatile__ ("pause"); }
            last_pulse = EmergenceOS::g_temporal_pulse;

            if (g_in_control_panel) {
                g_control->draw_panel(g_res->get_active_cores(), g_res->get_active_nodes(), g_focus, hypercube_frame, nullptr);
                g_vga->draw_spinning_cube(hypercube_frame, 0, 0, (hypercube_frame % (60 * phase_lock_divisor) == 0), 0, 0);
                
                uint32_t prompt_color = (g_focus == NeumannControlPanel::REGION_CONSOLE) ? 0x0000FF00 : 0x00003333;
                g_vga->print_at("[PHOENIX]> ", 320, 520, prompt_color);
                g_vga->print_at(cmd, 410, 520, 0x00FFFFFF);
                if (g_focus == NeumannControlPanel::REGION_CONSOLE && (hypercube_frame / 16) % 2) 
                    g_vga->print_at("_", 410 + (cmd_idx * 8), 520, 0x0000FF00);
            } else {
                if (prompt_needed) {
                    g_vga->clear(0x00080808);
                    g_vga->print_at("=== CONSENSUS SOVEREIGN WORKSPACE ===", 10, 10, 0x0000FFFF);
                    g_vga->print_at("\n[Sovereign]> ", 10, 30, 0x0000FF00);
                    prompt_needed = false;
                }
                g_vga->print_at(cmd, 120, 30, 0x00FFFFFF);
            }

            g_vga->swap_buffers();
            hypercube_frame++;
            
            uint8_t sc = kb.read_raw_scancode();
            if (sc == 0) continue;
            
            if (sc == Keyboard::KEY_TAB) {
                g_focus = (NeumannControlPanel::Region)((g_focus + 1) % 3);
                continue;
            }

            char c = kb.scancode_to_char(sc);
            if (c == 0) continue;

            if (c == '\n' || c == '\r') {
                cmd[cmd_idx] = '\0';
                
                bool match;
                kstrcmp(cmd, "help", match);
                if (match) {
                    g_vga->print_at("\n COMMANDS: ls, reg, rm, burn, run, seal, stats, control, back", 10, g_vga->get_cursor_y(), 0x0000FFFF);
                    g_vga->print_at("\n SCRIPTING: Use '|' to pipe terminal lo to next resolve seed.", 10, g_vga->get_cursor_y(), 0x00AAAAAA);
                }
                else if (kstarts_with(cmd, "reg ")) {
                    // Syntax: reg [mnemonic] [address]
                    char name[32]; char addr_str[32];
                    // Very simple parser for proof of concept
                    g_transducer->register_hook("Hello World", 0x00ADDFE33);
                    g_vga->print_at("\n [HOOK] MNEMONIC 'Hello World' BOUND TO 0x00ADDFE33", 10, g_vga->get_cursor_y(), 0x00FFFF00);
                }
                else if (kstarts_with(cmd, "tree")) {
                    g_vga->print_at("\n [SILICON TREE - HARDWARE APERTURES]", 10, g_vga->get_cursor_y(), 0x0000FFFF);
                    PCIController pci;
                    // Literal BAR Scan
                    for (uint8_t bus = 0; bus < 4; bus++) {
                        for (uint8_t slot = 0; slot < 32; slot++) {
                            uintptr_t bar = pci.find_ahci_base(); // Simplified: find first aperture
                            if (bar) {
                                char b_buf[32]; g_vga->int_to_str(bar, b_buf);
                                g_vga->print_at("\n   AHCI_CONTROLLER: ", 10, g_vga->get_cursor_y(), 0x00FFFFFF);
                                g_vga->print_at(b_buf, 200, g_vga->get_cursor_y(), 0x0000FF00);
                                break;
                            }
                        }
                    }
                    g_vga->print_at("\n   LFB_APERTURE: 0xFD000000", 10, g_vga->get_cursor_y(), 0x00FFFFFF);
                    g_vga->print_at("\n   VMX_RESERVED: 0x00001000", 10, g_vga->get_cursor_y(), 0x00FFFFFF);
                }
                else if (kstarts_with(cmd, "rm ")) {
                    g_vga->print_at("\n PURGING TOPOLOGICAL BLOCK...", 10, g_vga->get_cursor_y(), 0x00FF0000);
                }
                else if (kstarts_with(cmd, "burn ")) {
                    g_vga->print_at("\n MATERIALIZING SPATIAL ATOM...", 10, g_vga->get_cursor_y(), 0x0000FF00);
                    g_transducer->burn_atom(0x1337, 0xBEEF);
                }
                else if (kstarts_with(cmd, "run ")) {
                    g_vga->print_at("\n EXECUTING SPATIAL CHAIN...", 10, g_vga->get_cursor_y(), 0x00FFFF00);
                    uint64_t script[] = {0x1337};
                    auto res = g_transducer->resolve_chain(script, 1);
                    transient_pipe = res.last_resolve;
                }
                else if (kstarts_with(cmd, "back")) { g_in_shell = false; return; }
                else if (kstarts_with(cmd, "fold")) { g_res->fold_manifold(1); }
                
                kmemset(cmd, 0, 128); cmd_idx = 0; prompt_needed = true;
                continue;
            }

            if (c == '\b' && cmd_idx > 0) {
                cmd[--cmd_idx] = '\0';
            } else if (cmd_idx < 127 && c >= 32 && (g_in_control_panel ? g_focus == NeumannControlPanel::REGION_CONSOLE : true)) {
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

    // 4. SOVEREIGN IGNITION (AUTHENTICATION)
    EmergenceOS::Keyboard kb;
    char pass[64];
    if (EmergenceOS::g_vga) {
        EmergenceOS::g_vga->clear(0x00080808);
        EmergenceOS::g_vga->draw_border(200, 200, 600, 200, 0x00222222, 1);
        EmergenceOS::g_vga->print_at("CONSENSUS SOVEREIGN ACCESS", 350, 220, 0x0000FFFF);
        EmergenceOS::g_vga->print_at("ENTER ACCESS KEY: ", 250, 280, 0x00FFFFFF);
        EmergenceOS::g_vga->swap_buffers();
        kb.read_password(pass, 64, EmergenceOS::g_vga);
    }

    char hwid[16]; Consensus::generate_device_id(hwid);
    Emergence::Seed master = Emergence::Argon2Sovereign::derive(pass, hwid, nullptr);
    EmergenceOS::g_master_seed = master;

    // 5. Initialize Manifold (Literal Resolve)
    if (EmergenceOS::g_vga) {
        EmergenceOS::g_vga->print_at("\nIGNITING MANIFOLD...", 250, 320, 0x00FFFF00);
        EmergenceOS::g_vga->swap_buffers();
    }
    
    EmergenceOS::g_manifold_instance = new (manifold_storage) Emergence::SubstrateManifold();
    EmergenceOS::g_manifold_instance->manual_init(32, EmergenceOS::g_disk);
    EmergenceOS::g_manifold_instance->initialize(master);

    static uint8_t control_storage[sizeof(EmergenceOS::NeumannControlPanel)] __attribute__((aligned(16)));
    EmergenceOS::g_control = new (control_storage) EmergenceOS::NeumannControlPanel(EmergenceOS::g_vga, EmergenceOS::g_res, master, EmergenceOS::g_manifold_instance);

    int selection = 0;
    while(true) {
        EmergenceOS::g_in_shell = true;
        EmergenceOS::g_in_control_panel = false;

        while(true) {
            if (EmergenceOS::g_vga) {
                EmergenceOS::g_vga->clear(0x00080808);
                EmergenceOS::g_vga->print_at("=== CONSENSUS ENTERPRISE HYPERVISOR ===", 300, 200, 0x0000FFFF);
                
                #ifdef SOVEREIGN_BUILD
                    EmergenceOS::g_vga->print_at("PHOENIX vX [SOVEREIGN] | CAPACITY: UNLIMITED", 250, 130, 0x00FF00FF);
                #elif defined(EVALUATION_BUILD)
                    EmergenceOS::g_vga->print_at("PHOENIX v0.5 [OPEN] | CAPACITY: 1 TB", 250, 130, 0x00FF0000);
                #else
                    EmergenceOS::g_vga->print_at("PHOENIX v1.0 [RELEASE] | CAPACITY: 10 TB", 250, 130, 0x00FFFF00);
                #endif

                if (vmx_ready) EmergenceOS::g_vga->print_at("VMX_ACCELERATION: [ENABLED]", 300, 150, 0x0000FF00);
                
                if (selection == 0) EmergenceOS::g_vga->print_at("> 1. Launch Sovereign Workspace", 250, 240, 0x0000FF00);
                else EmergenceOS::g_vga->print_at("  1. Launch Sovereign Workspace", 250, 240, 0x00FFFFFF);

                if (selection == 1) EmergenceOS::g_vga->print_at("> 2. Neumann Control Panel", 250, 260, 0x0000FF00);
                else EmergenceOS::g_vga->print_at("  2. Neumann Control Panel", 250, 260, 0x00FFFFFF);
                
                EmergenceOS::g_vga->swap_buffers();
            }
            
            uint8_t sc = kb.read_raw_scancode();
            if (sc == EmergenceOS::Keyboard::KEY_UP) selection = 0;
            if (sc == EmergenceOS::Keyboard::KEY_DOWN) selection = 1;
            
            if (sc == 0x1C) break; // Enter key
        }

        if (selection == 0) {
            EmergenceOS::sovereign_shell(kb);
        } else {
            EmergenceOS::g_in_control_panel = true;
            EmergenceOS::sovereign_shell(kb);
        }
    }
}
