#ifndef EOS_VGA_HPP
#define EOS_VGA_HPP

#include "memory.hpp"

namespace EmergenceOS {
    class Graphics {
    private:
        uint32_t* lfb_;
        uint32_t* back_buffer_;
        uint32_t width_;
        uint32_t height_;
        uint32_t pitch_; 
        uint32_t cursor_x = 0;
        uint32_t cursor_y = 0;

        static const uint8_t font8x8_basic[95][8];

    public:
        Graphics() : lfb_(nullptr), back_buffer_(nullptr), width_(0), height_(0), pitch_(0) {}
        
        void initialize(uint64_t addr, uint32_t w, uint32_t h, uint32_t p) {
            if (w == 0 || h == 0 || p == 0) return;
            lfb_ = (uint32_t*)addr;
            width_ = w;
            height_ = h;
            pitch_ = p;
            
            size_t sz = height_ * pitch_;
            back_buffer_ = (uint32_t*)EmergenceOS::g_pmm->allocate(sz);
            clear(0x00000000); 
            swap_buffers();
        }

        void swap_buffers() {
            if (!lfb_ || !back_buffer_) return;
            uint64_t* src = (uint64_t*)back_buffer_;
            uint64_t* dst = (uint64_t*)lfb_;
            size_t count = (height_ * pitch_) / 8;
            for (size_t i = 0; i < count; i++) dst[i] = src[i];
        }

        void clear(uint32_t color) {
            if (!back_buffer_) return;
            uint64_t c64 = ((uint64_t)color << 32) | color;
            uint64_t* buf = (uint64_t*)back_buffer_;
            size_t count = (height_ * pitch_) / 8;
            for (size_t i = 0; i < count; i++) buf[i] = c64;
            cursor_x = 0; cursor_y = 0;
        }

        void put_pixel(uint32_t x, uint32_t y, uint32_t color) {
            if (!back_buffer_ || x >= width_ || y >= height_) return;
            uint32_t* pixel_addr = (uint32_t*)((uint8_t*)back_buffer_ + (y * pitch_) + (x * 4));
            *pixel_addr = color;
        }

        void put_char_at(char c, uint32_t x, uint32_t y, uint32_t color) {
            if (!back_buffer_) return;
            if (c < 32 || c > 126) c = '?';
            const uint8_t* glyph = font8x8_basic[c - 32];
            for (int i = 0; i < 8; i++) {
                for (int j = 0; j < 8; j++) {
                    if (glyph[i] & (0x80 >> j)) put_pixel(x + j, y + i, color);
                }
            }
        }

        void print_at(const char* str, uint32_t x, uint32_t y, uint32_t color = 0x00FFFFFF) {
            for (int i = 0; str[i] != '\0'; i++) put_char_at(str[i], x + (i * 8), y, color);
        }

        uint32_t get_cursor_y() const { return cursor_y; }

        void put_char(char c, uint32_t color) {
            if (!back_buffer_) return;
            if (c == '\n') { cursor_x = 0; cursor_y += 12; return; }
            if (c == '\r') { cursor_x = 0; return; }
            if (cursor_x + 8 >= width_) { cursor_x = 0; cursor_y += 12; }
            if (cursor_y + 12 >= height_) { clear(0); } 
            put_char_at(c, cursor_x, cursor_y, color);
            cursor_x += 8;
        }

        void draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
            int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
            int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
            int sx = (x0 < x1) ? 1 : -1;
            int sy = (y0 < y1) ? 1 : -1;
            int err = dx - dy;
            while (true) {
                put_pixel((uint32_t)x0, (uint32_t)y0, color);
                if (x0 == x1 && y0 == y1) break;
                int e2 = 2 * err;
                if (e2 > -dy) { err -= dy; x0 += sx; }
                if (e2 < dx) { err += dx; y0 += sy; }
            }
        }

        void int_to_str(uint64_t n, char* str) {
            int i = 0;
            if (n == 0) { str[i++] = '0'; }
            else {
                char tmp[20]; int j = 0;
                while (n > 0) { tmp[j++] = (n % 10) + '0'; n /= 10; }
                while (j > 0) str[i++] = tmp[--j];
            }
            str[i] = '\0';
        }

        void draw_pulse_bar(uint64_t pulse) {
            int bw = 400; int sx = (width_/2)-(bw/2); int sy = height_-80;
            for(int i=0; i<bw; i++) for(int j=0; j<10; j++) put_pixel(sx+i, sy+j, 0x00111111);
            int pos = (pulse * 8) % bw;
            for(int i=-15; i<15; i++) {
                int px = (pos+i+bw)%bw; int alpha = 255 - (i<0?-i:i)*15;
                if (alpha < 0) alpha = 0;
                uint32_t neon = (alpha << 16) | (alpha << 8) | alpha;
                for(int j=0; j<10; j++) put_pixel(sx+px, sy+j, 0x0000FFFF & neon);
            }
            print_at("TEMPORAL_HEARTBEAT_ACTIVE", sx+110, sy+15, 0x0000AAAA);
        }

        void draw_spatial_map(uint64_t pulse, uint32_t lens, uint64_t depth) {
            int sx = width_-220; int sy = 50; int cs = 12;
            print_at("SPATIAL LOOP MAP (ROUTES)", sx, sy-15, 0x0000FFFF);
            for(int y=0; y<16; y++) for(int x=0; x<16; x++) {
                uint64_t h = (pulse ^ (lens << 16) ^ (x << 8) ^ y ^ depth) * 0x9E3779B97F4A7C15ULL;
                int d = (h >> 40) % 255;
                uint32_t c = (d < 50) ? 0x001A1A3A : (d < 180 ? (0x00442288 + (d << 8)) : (0x0000FFFF | (d<<16) | (d<<8)));
                for(int dy=0; dy<cs-1; dy++) for(int dx=0; dx<cs-1; dx++) put_pixel(sx+x*cs+dx, sy+y*cs+dy, c);
            }
        }

        void draw_spinning_cube(int frame, uint64_t lat, uint64_t depth, bool lock, uint64_t mips, uint64_t mbs, uint32_t lens) {
            static const int s_tbl[64] = {0, 25, 50, 74, 98, 120, 142, 162, 180, 197, 212, 225, 236, 244, 250, 254, 255, 254, 250, 244, 236, 225, 212, 197, 180, 162, 142, 120, 98, 74, 50, 25, 0, -25, -50, -74, -98, -120, -142, -162, -180, -197, -212, -225, -236, -244, -250, -254, -255, -254, -250, -244, -236, -225, -212, -197, -180, -162, -142, -120, -98, -74, -50, -25};
            int sin_a = s_tbl[frame % 64]; int cos_a = s_tbl[(frame + 16) % 64];
            int cx = (int)width_ / 3; int cy = (int)height_ / 2; int size = 150;
            static const int v[8][3] = {{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
            int proj[8][2];
            for(int i=0; i<8; i++) {
                int rx = (v[i][0]*size*cos_a - v[i][2]*size*sin_a) >> 8;
                int rz = (v[i][0]*size*sin_a + v[i][2]*size*cos_a) >> 8;
                int ry = (v[i][1]*size*cos_a - rz*sin_a) >> 8;
                rz = (v[i][1]*size*sin_a + rz*cos_a) >> 8;
                int dist = 500 + rz;
                proj[i][0] = cx + (rx << 9) / dist; proj[i][1] = cy + (ry << 9) / dist;
            }
            clear(0);
            uint32_t col = lock ? 0x00FF0000 : 0x0000FF00;
            for(int i=0; i<4; i++) {
                draw_line(proj[i][0], proj[i][1], proj[(i+1)%4][0], proj[(i+1)%4][1], col);
                draw_line(proj[i+4][0], proj[i+4][1], proj[((i+1)%4)+4][0], proj[((i+1)%4)+4][1], col);
                draw_line(proj[i][0], proj[i][1], proj[i+4][0], proj[i+4][1], col);
            }
            char buf[32];
            print_at("EM-1 MONITOR [LIVE]", 20, 20, 0x00FFFF00);
            print_at("LATENCY: ", 20, 45); int_to_str(lat, buf); print_at(buf, 100, 45, 0x0000FF00);
            print_at("DEPTH:   ", 20, 60); int_to_str(depth, buf); print_at(buf, 100, 60, 0x0000FF00);
            print_at("MIPS:    ", 20, 75); int_to_str(mips, buf); print_at(buf, 100, 75, 0x0000FF00);
            print_at("LENS:    ", 20, 90); int_to_str(lens, buf); print_at(buf, 100, 90, 0x0000FF00);
            draw_spatial_map((uint64_t)frame, lens, depth);
            draw_pulse_bar((uint64_t)frame);
            swap_buffers();
        }

        size_t kstrlen(const char* s) { size_t l=0; while(s[l]) l++; return l; }
    };
}
#endif
