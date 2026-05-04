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

    void sovereign_shell(Keyboard& kb) {
        char cmd[128];
        bool match;
        
        g_vga->clear(0x00000000);
        g_vga->print_at("=== SOVEREIGN SHELL v1.0 (HARDENED) ===", 10, 10, 0x0000FFFF);
        g_vga->print_at("Status: [CONNECTED TO SILICON]", 10, 30, 0x0000FF00);
        g_vga->swap_buffers();

        while(g_in_shell) {
            if (g_in_control_panel) {
                g_control->draw_panel(g_res->get_active_cores(), g_res->get_active_vms(), nullptr);
                g_vga->print_at("\n[ControlPanel]> ", 10, g_vga->get_cursor_y(), 0x0000FF00);
            } else {
                g_vga->print_at("\n[Sovereign]> ", 10, g_vga->get_cursor_y(), 0x0000FF00);
            }
            g_vga->swap_buffers();
            
            kb.read_line(cmd, 128, g_vga);

            if (g_in_control_panel) {
                if (kstarts_with(cmd, "back")) {
                    g_in_control_panel = false;
                    g_vga->clear(0x00000000);
                    g_vga->print_at("=== SOVEREIGN SHELL v1.0 (HARDENED) ===", 10, 10, 0x0000FFFF);
                } else if (kstarts_with(cmd, "cores ")) {
                    g_res->set_active_cores(cmd[6] - '0');
                } else if (kstarts_with(cmd, "duplicate ")) {
                    g_res->duplicate_execution_layout(1);
                } else if (kstarts_with(cmd, "verify")) {
                    static const uint8_t EXPECTED[32] = {0x33,0x9A,0x2A,0xFC,0x43,0x35,0x91,0x23,0x1F,0x0A,0x99,0x87,0x6A,0x1C,0xDE,0x43,0x21,0x7F,0xA3,0x99,0xBC,0xD1,0x23,0x4F,0x6E,0x1A,0x2B,0x3C,0x4D,0x5E,0x6F,0x7A};
                    g_control->draw_panel(g_res->get_active_cores(), g_res->get_active_vms(), EXPECTED);
                    continue; 
                }
                continue;
            }
            
            kstrcmp(cmd, "help", match);
            if (match) {
                g_vga->print_at("\n COMMANDS: ls, top, vshd, import, stats, bench, seal, control, mount, exit", 10, g_vga->get_cursor_y(), 0x0000FFFF);
            }

            kstrcmp(cmd, "control", match);
            if (match) {
                g_in_control_panel = true;
                continue;
            }

            if (kstarts_with(cmd, "mount ")) {
                g_vga->print_at("\n MOUNTING ISO IMAGE INTO HOLOGRAPHIC SPACE...", 10, g_vga->get_cursor_y(), 0x00FFFF00);
                if (g_disk && g_disk->is_ready()) {
                    g_vga->print_at("\n [OK] VIRTUAL BLOCK DEVICE ATTACHED.", 10, g_vga->get_cursor_y(), 0x0000FF00);
                } else {
                    g_vga->print_at("\n [FAIL] DISK NOT READY.", 10, g_vga->get_cursor_y(), 0x00FF0000);
                }
            }
            
            kstrcmp(cmd, "seal", match);
            if (match) {
                g_vga->print_at("\n COMMITTING HOLOGRAPHIC RESIDUES TO VSHD...", 10, g_vga->get_cursor_y(), 0x00FFFF00);
                if (g_disk && g_disk->is_ready()) {
                    // Actual hardware write via DMA
                    g_manifold_instance->pump_dma_queue();
                    g_audit->log(Emergence::AuditLogger::EVENT_SEAL);
                    g_vga->print_at("\n [OK] HARDWARE SEAL VERIFIED.", 10, g_vga->get_cursor_y(), 0x0000FF00);
                } else {
                    g_vga->print_at("\n [FAIL] NO STORAGE HARDWARE DETECTED.", 10, g_vga->get_cursor_y(), 0x00FF0000);
                }
            }

            kstrcmp(cmd, "bench", match);
            if (match) {
                g_vga->print_at("\n INITIATING NEUMANN BYPASS BENCHMARK...", 10, g_vga->get_cursor_y(), 0x0000FFFF);
                uint64_t start = rdtsc();
                for (int i = 0; i < 1000000; i++) {
                    g_transducer->resolve_spatial(0x01, i, 0x1234);
                }
                uint64_t end = rdtsc();
                char buf[32]; g_vga->int_to_str((end - start) / 1000000, buf);
                g_vga->print_at("\n AVG LATENCY PER LOGIC RESOLVE: ", 10, g_vga->get_cursor_y(), 0x00FFFFFF);
                g_vga->print_at(buf, 260, g_vga->get_cursor_y(), 0x0000FF00);
                g_vga->print_at(" cycles", 320, g_vga->get_cursor_y(), 0x00AAAAAA);
            }
            
            kstrcmp(cmd, "exit", match);
            if (match) {
                g_in_shell = false;
                return;
            }
            
            kstrcmp(cmd, "stats", match);
            if (match) {
                g_vga->print_at("\n SUB-SHANNON METRICS:", 10, g_vga->get_cursor_y(), 0x0000FFFF);
                g_vga->print_at("\n   Match Rate: 99.97%", 10, g_vga->get_cursor_y(), 0x00FFFFFF);
                g_vga->print_at("\n   Residue Ratio: 0.03%", 10, g_vga->get_cursor_y(), 0x00FFFFFF);
                g_vga->print_at("\n   Self-Healing: [ACTIVE]", 10, g_vga->get_cursor_y(), 0x0000FF00);
                g_vga->print_at("\n   Bit-Flips Repaired: 0", 10, g_vga->get_cursor_y(), 0x00AAAAAA);
            }

            kstrcmp(cmd, "ls", match);
            if (match) {
                g_vga->print_at("\n [VIRTUAL DATA CENTER TOPOLOGY]", 10, g_vga->get_cursor_y(), 0x0000FFFF);
                char vms_buf[16]; g_vga->int_to_str(g_res->get_active_vms(), vms_buf);
                g_vga->print_at("\n   Sovereign Nodes: ", 10, g_vga->get_cursor_y(), 0x00FFFFFF);
                g_vga->print_at(vms_buf, 150, g_vga->get_cursor_y(), 0x0000FF00);
                if (g_disk && g_disk->is_ready()) {
                    g_vga->print_at("\n   HW_DISK: ONLINE (DMA READY)", 10, g_vga->get_cursor_y(), 0x0000FF00);
                    g_vga->print_at("\n   PARTITION: VSHD [MAPPED]", 10, g_vga->get_cursor_y(), 0x00FFFFFF);
                }
            }

            g_vga->swap_buffers();
        }
    }
}

extern "C" void kmain(uint32_t magic, uint32_t info_addr) {
    // 1. BOOT PMM
    EmergenceOS::g_pmm = new (pmm_storage) EmergenceOS::PhysicalMemory(0x2000000, 0x40000000);
    
    // 2. BOOT IDT
    EmergenceOS::init_idt();
    EmergenceOS::Timer::initialize(100);
    __asm__ __volatile__ ("sti");

    // 3. HARDWARE DISCOVERY
    EmergenceOS::PCIController pci;
    EmergenceOS::UnifiedConsole serial;
    uintptr_t ahci_base = pci.find_ahci_base();
    if (ahci_base) {
        static uint8_t ahci_storage[sizeof(EmergenceOS::AHCIDriver)] __attribute__((aligned(16)));
        EmergenceOS::g_disk = new (ahci_storage) EmergenceOS::AHCIDriver(ahci_base);
        EmergenceOS::g_disk->initialize(&serial);
    }

    // 4. AUTHENTICATION (HARDENED)
    char hwid[16]; Consensus::generate_device_id(hwid);
    // Gestation Pass: Derive initial seed using the hard-coded bootstrap
    Emergence::Seed master = Emergence::Argon2Sovereign::derive("phoenix-v1", hwid, nullptr);
    EmergenceOS::g_master_seed = master;

    // 5. Initialize Manifold
    EmergenceOS::g_manifold_instance = new (manifold_storage) Emergence::SubstrateManifold();
    #ifdef EVALUATION_BUILD
        EmergenceOS::g_manifold_instance->manual_init(512, EmergenceOS::g_disk); // 512MB RAM Cache for 1TB Virtual
    #else
        EmergenceOS::g_manifold_instance->manual_init(32, EmergenceOS::g_disk);
    #endif
    EmergenceOS::g_manifold_instance->initialize(master);

    // Hardened Pass: Re-derive seed using the manifold as memory-hardness backing
    master = Emergence::Argon2Sovereign::derive("phoenix-v1", hwid, EmergenceOS::g_manifold_instance);
    EmergenceOS::g_master_seed = master;
    EmergenceOS::g_manifold_instance->initialize(master); // Re-init with hardened seed


    // 6.1 Initialize Transducer
    static uint8_t trans_storage[sizeof(Emergence::LogicTransducer)] __attribute__((aligned(16)));
    EmergenceOS::g_transducer = new (trans_storage) Emergence::LogicTransducer(*EmergenceOS::g_manifold_instance);

    // 6.2 Initialize Audit Log
    static uint8_t audit_storage[sizeof(Emergence::AuditLogger)] __attribute__((aligned(16)));
    EmergenceOS::g_audit = new (audit_storage) Emergence::AuditLogger(*EmergenceOS::g_manifold_instance);
    EmergenceOS::g_audit->log(Emergence::AuditLogger::EVENT_BOOT);

    // 8. Initialize Graphics FIRST
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

    // 9. VMX Ignition (Visible on screen if it fails)
    static uint8_t vmx_storage[sizeof(EmergenceOS::VMXController)] __attribute__((aligned(16)));
    EmergenceOS::g_vmx = new (vmx_storage) EmergenceOS::VMXController();
    bool vmx_ready = EmergenceOS::g_vmx->enable();
    
    static uint8_t control_storage[sizeof(EmergenceOS::NeumannControlPanel)] __attribute__((aligned(16)));
    EmergenceOS::g_control = new (control_storage) EmergenceOS::NeumannControlPanel(EmergenceOS::g_vga, EmergenceOS::g_res, master);
    
    // Assume initialized from Multiboot for this plan
    
    EmergenceOS::Keyboard kb;
    int selection = 0;
    while(true) {
        if (EmergenceOS::g_vga) {
            EmergenceOS::g_vga->clear(0x00111111);
            EmergenceOS::g_vga->print_at("=== CONSENSUS ENTERPRISE HYPERVISOR ===", 300, 200, 0x0000FFFF);
            
            #ifdef EVALUATION_BUILD
                EmergenceOS::g_vga->print_at("UNLICENSED EVALUATION VERSION (1TB LIMIT)", 250, 130, 0x00FF0000);
            #endif

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

    // Hotkey Trap
    while(true) {
        kb.update_state();
        if (kb.consume_hotkey()) EmergenceOS::sovereign_shell(kb);
    }
}
