#ifndef EOS_NEUMANN_CONTROL_PANEL_HPP
#define EOS_NEUMANN_CONTROL_PANEL_HPP

#include "serial.hpp"
#include "vga.hpp"
#include "dynamic_allocator.hpp"
#include "sha256.hpp"

namespace EmergenceOS {

    class NeumannControlPanel {
    private:
        Graphics* vga_;
        DynamicResourceAllocator* res_;
        Emergence::Seed master_seed_;

    public:
        NeumannControlPanel(Graphics* vga, DynamicResourceAllocator* res, Emergence::Seed seed)
            : vga_(vga), res_(res), master_seed_(seed) {}

        void draw_panel(uint32_t active_cores, uint32_t active_vms, const uint8_t* expected_hash = nullptr) {
            vga_->clear(0x00111111);
            vga_->print_at("=== NEUMANN CONTROL PANEL ===", 300, 50, 0x0000FFFF);

            // 1. Core Assignment
            vga_->print_at("[ CORE SCHEDULING ]", 50, 100, 0x00FFFF00);
            char buf[16];
            vga_->int_to_str(active_cores, buf);
            vga_->print_at("Active Neumann Bypass Cores: ", 50, 120, 0x00FFFFFF);
            vga_->print_at(buf, 280, 120, 0x0000FF00);

            // 2. Manifold Layouts
            vga_->print_at("[ MANIFOLD DATA CENTER ]", 50, 160, 0x00FFFF00);
            vga_->int_to_str(active_vms, buf);
            vga_->print_at("Active Sovereign Nodes: ", 50, 180, 0x00FFFFFF);
            vga_->print_at(buf, 280, 180, 0x0000FF00);
            vga_->print_at("Available Layouts: 1. Default  2. High-Compute  3. Storage-Heavy", 50, 200, 0x00AAAAAA);

            // 3. SHA256 Seed Verification
            vga_->print_at("[ CRYPTOGRAPHIC VERIFICATION ]", 50, 240, 0x00FFFF00);
            if (expected_hash) {
                bool verified = Emergence::SHA256::verify_seed(master_seed_, expected_hash);
                vga_->print_at("Master Seed Integrity: ", 50, 260, 0x00FFFFFF);
                if (verified) {
                    vga_->print_at("VERIFIED (SHA-256 MATCH)", 280, 260, 0x0000FF00);
                } else {
                    vga_->print_at("FAILED (TAMPER DETECTED)", 280, 260, 0x00FF0000);
                }
            } else {
                vga_->print_at("Master Seed Integrity: PENDING VERIFICATION", 50, 260, 0x00AAAAAA);
            }

            vga_->print_at("Commands: 'cores <num>', 'fold <layout>', 'verify', 'back'", 50, 320, 0x0000FFFF);
            vga_->swap_buffers();
        }
    };
}

#endif
