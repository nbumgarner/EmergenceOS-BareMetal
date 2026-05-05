#ifndef EOS_NEUMANN_CONTROL_PANEL_HPP
#define EOS_NEUMANN_CONTROL_PANEL_HPP

#include "serial.hpp"
#include "vga.hpp"
#include "dynamic_allocator.hpp"
#include "sha256.hpp"
#include "topology.hpp"

namespace EmergenceOS {

    class NeumannControlPanel {
    public:
        enum Region {
            REGION_SCHEDULING = 0,
            REGION_MANIFOLD = 1,
            REGION_CONSOLE = 2
        };

    private:
        Graphics* vga_;
        DynamicResourceAllocator* res_;
        Emergence::Seed master_seed_;
        Emergence::SubstrateManifold* manifold_;

        void draw_enigma_surpass(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t frame) {
            vga_->draw_border(x, y, w, h, 0x00333333, 1);
            vga_->print_at("SOVEREIGN STATE COMPLEXITY", x + 10, y + 10, 0x0000AAAA);
            vga_->print_at("8-HOP RESOLVE: 1.2e24 COMBINATIONS", x + 10, y + 25, 0x0000FFFF);

            uint32_t cx = x + w/2;
            uint32_t cy = y + h/2 + 20;

            // Render 8 concentric rings representing the 8 topological hops
            for (int r = 0; r < 8; r++) {
                uint32_t radius = 20 + (r * 15);
                uint32_t color = ( (frame + r) % 8 == 0) ? 0x0000FFFF : 0x00003333;
                
                // Draw 12 "Rotors" per ring
                for (int i = 0; i < 12; i++) {
                    int angle = (i * 30) + (frame * (r+1)) / 4;
                    // Fixed-point trig approximation for circle
                    static const int s_tbl[12] = {0, 50, 86, 100, 86, 50, 0, -50, -86, -100, -86, -50};
                    static const int c_tbl[12] = {100, 86, 50, 0, -50, -86, -100, -86, -50, 0, 50, 86};
                    
                    int rx = (radius * c_tbl[i % 12]) / 100;
                    int ry = (radius * s_tbl[i % 12]) / 100;
                    vga_->draw_rect(cx + rx, cy + ry, 4, 4, color);
                }
            }
        }

        void draw_status_bar() {
            vga_->draw_rect(0, 0, vga_->get_width(), 25, 0x00003333);
            #ifdef SOVEREIGN_BUILD
                vga_->print_at("PHOENIX vX [SOVEREIGN] | LITERAL RESOLVE | [TAB] REGIONS", 10, 8, 0x0000FFFF);
            #elif defined(EVALUATION_BUILD)
                vga_->print_at("PHOENIX v0.5 [OPEN] | LITERAL RESOLVE | [TAB] REGIONS", 10, 8, 0x0000FFFF);
            #else
                vga_->print_at("PHOENIX v1.0 [RELEASE] | LITERAL RESOLVE | [TAB] REGIONS", 10, 8, 0x0000FFFF);
            #endif
        }

    public:
        NeumannControlPanel(Graphics* vga, DynamicResourceAllocator* res, Emergence::Seed seed, Emergence::SubstrateManifold* manifold)
            : vga_(vga), res_(res), master_seed_(seed), manifold_(manifold) {}

        void draw_panel(uint32_t active_cores, uint32_t active_nodes, Region focused_region, uint32_t frame, const uint8_t* expected_hash = nullptr) {
            vga_->clear(0x00080808);
            draw_status_bar();
            
            // System Metrics (Region 0)
            uint32_t sched_color = (focused_region == REGION_SCHEDULING) ? 0x0000FFFF : 0x00222222;
            vga_->draw_border(10, 40, 280, 700, sched_color, 1);
            vga_->print_at("[ SILICON CONTROL ]", 20, 60, 0x00FFFF00);
            vga_->print_at("Bypass Cores: ", 20, 85, 0x00FFFFFF);
            char c_buf[16]; vga_->int_to_str(active_cores, c_buf);
            vga_->print_at(c_buf, 150, 85, 0x0000FF00);

            vga_->print_at("[ MANIFOLD STORAGE ]", 20, 130, 0x00FFFF00);
            vga_->print_at("Active Nodes: ", 20, 155, 0x00FFFFFF);
            char n_buf[16]; vga_->int_to_str(active_nodes, n_buf);
            vga_->print_at(n_buf, 150, 155, 0x0000FF00);
            
            vga_->print_at("Substrate Blocks: ", 20, 180, 0x00FFFFFF);
            char b_buf[16]; vga_->int_to_str(manifold_->get_used_blocks(), b_buf);
            vga_->print_at(b_buf, 150, 180, 0x0000FF00);

            // Main Viewport: Quad-Phase Resolve (Region 1)
            vga_->draw_quad_resolve(310, 40, 680, 400, frame, manifold_);

            // Global State Viewport: Enigma Surpass (Bottom Left)
            draw_enigma_surpass(10, 460, 280, 280, frame);

            // Command Console Area (Region 2)
            uint32_t console_color = (focused_region == REGION_CONSOLE) ? 0x0000FF00 : 0x00222222;
            vga_->draw_border(310, 460, 680, 280, console_color, 1);
            vga_->print_at("SOVEREIGN COMMAND CONSOLE", 320, 470, 0x0000AAAA);
            
            vga_->print_at("ACTIVE_RESOLVE_PHASE: ", 320, 490, 0x00FFFFFF);
            char f_buf[16]; vga_->int_to_str(frame % 2048, f_buf);
            vga_->print_at(f_buf, 500, 490, 0x00FFFF00);
        }
    };
}

#endif
