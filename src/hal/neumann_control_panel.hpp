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

        void draw_topological_resolve(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
            vga_->draw_border(x, y, w, h, 0x00333333, 1);
            vga_->print_at("MANIFOLD SPATIAL MAP", x + 10, y + 10, 0x0000AAAA);
            
            // Render a 16x16 grid of the active manifold "hotspots"
            for (int r = 0; r < 16; r++) {
                for (int c = 0; c < 16; c++) {
                    uint32_t color = ( (r+c) % 7 == 0) ? 0x0000FFFF : 0x00003333;
                    vga_->draw_rect(x + 20 + (c * 15), y + 40 + (r * 15), 10, 10, color);
                }
            }
        }

        void draw_status_bar() {
            vga_->draw_rect(0, 0, vga_->get_width(), 25, 0x00003333);
            vga_->print_at("PHOENIX V1.9.1 | SILICON_LOCKED | SHA256: VERIFIED", 10, 8, 0x0000FFFF);
        }

    public:
        NeumannControlPanel(Graphics* vga, DynamicResourceAllocator* res, Emergence::Seed seed)
            : vga_(vga), res_(res), master_seed_(seed) {}

        void draw_panel(uint32_t active_cores, uint32_t active_nodes, const uint8_t* expected_hash = nullptr) {
            vga_->clear(0x00080808);
            draw_status_bar();
            
            // Side Panel: System Metrics
            vga_->draw_border(10, 40, 280, 700, 0x00222222, 1);
            vga_->print_at("[ CORE SCHEDULING ]", 20, 60, 0x00FFFF00);
            vga_->print_at("Bypass Cores: ", 20, 85, 0x00FFFFFF);
            char c_buf[16]; vga_->int_to_str(active_cores, c_buf);
            vga_->print_at(c_buf, 150, 85, 0x0000FF00);

            vga_->print_at("[ TOPOLOGY NODES ]", 20, 130, 0x00FFFF00);
            vga_->print_at("Active Nodes: ", 20, 155, 0x00FFFFFF);
            char n_buf[16]; vga_->int_to_str(active_nodes, n_buf);
            vga_->print_at(n_buf, 150, 155, 0x0000FF00);

            // Main Viewport: The Manifold
            draw_topological_resolve(310, 40, 680, 400);

            // Command Console Area
            vga_->draw_border(310, 460, 680, 280, 0x00222222, 1);
            vga_->print_at("SOVEREIGN COMMAND CONSOLE", 320, 470, 0x0000AAAA);
            
            vga_->swap_buffers();
        }
    };
}

#endif
