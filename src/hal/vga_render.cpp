#include "vga.hpp"
#include "topology.hpp"

namespace EmergenceOS {

    void Graphics::draw_quad_resolve(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t master_frame, const void* manifold_ptr) {
        const Emergence::SubstrateManifold* manifold = (const Emergence::SubstrateManifold*)manifold_ptr;
        uint32_t qw = w / 2;
        uint32_t qh = h / 2;
        uint32_t offsets[4] = {0, 512, 1024, 1536};
        const char* labels[4] = {"PHASE: 0", "PHASE: 512", "PHASE: 1024", "PHASE: 1536"};

        for (int i = 0; i < 4; i++) {
            uint32_t qx = x + (i % 2) * qw;
            uint32_t qy = y + (i / 2) * qh;
            draw_border(qx, qy, qw, qh, 0x00222222, 1);
            print_at(labels[i], qx + 5, qy + 5, 0x0000AAAA);

            for (int r = 0; r < 8; r++) {
                for (int c = 0; c < 8; c++) {
                    uint32_t b_idx = (master_frame + offsets[i] + (r * 8) + c) % 1024;
                    uint32_t color = manifold->is_block_resident(b_idx) ? 0x0000FF00 : 0x00002222;
                    draw_rect(qx + 10 + (c * 10), qy + 25 + (r * 10), 8, 8, color);
                }
            }
        }
    }

}
