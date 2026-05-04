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
            lfb_ = (uint32_t*)addr;
            width_ = w;
            height_ = h;
            pitch_ = p;
            size_t sz = height_ * pitch_;
            back_buffer_ = (uint32_t*)EmergenceOS::g_pmm->allocate(sz);
            clear(0x00080808); // Deep Obsidian
            swap_buffers();
        }

        void clear(uint32_t color) {
            uint64_t c64 = ((uint64_t)color << 32) | color;
            uint64_t* buf = (uint64_t*)back_buffer_;
            size_t count = (height_ * pitch_) / 8;
            for (size_t i = 0; i < count; i++) buf[i] = c64;
            cursor_x = 0; cursor_y = 0;
        }

        void draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
            for (uint32_t i = y; i < y + h; i++) {
                for (uint32_t j = x; j < x + w; j++) {
                    put_pixel(j, i, color);
                }
            }
        }

        void draw_border(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color, uint32_t thickness) {
            draw_rect(x, y, w, thickness, color); // Top
            draw_rect(x, y + h - thickness, w, thickness, color); // Bottom
            draw_rect(x, y, thickness, h, color); // Left
            draw_rect(x + w - thickness, y, thickness, h, color); // Right
        }

        void put_pixel(uint32_t x, uint32_t y, uint32_t color) {
            if (x >= width_ || y >= height_) return;
            back_buffer_[y * (pitch_ / 4) + x] = color;
        }

        void print_at(const char* str, uint32_t x, uint32_t y, uint32_t color) {
            uint32_t cx = x;
            for (int i = 0; str[i] != '\0'; i++) {
                if (str[i] == '\n') {
                    y += 12;
                    cx = x;
                    continue;
                }
                const uint8_t* glyph = font8x8_basic[str[i] - 32];
                for (int gy = 0; gy < 8; gy++) {
                    for (int gx = 0; gx < 8; gx++) {
                        if (glyph[gy] & (0x80 >> gx)) put_pixel(cx + gx, y + gy, color);
                    }
                }
                cx += 8;
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

        uint32_t get_cursor_y() const { return cursor_y; }
        uint32_t get_width() const { return width_; }
        uint32_t get_height() const { return height_; }

        void swap_buffers() {
            uint64_t* src = (uint64_t*)back_buffer_;
            uint64_t* dst = (uint64_t*)lfb_;
            size_t count = (height_ * pitch_) / 8;
            for (size_t i = 0; i < count; i++) dst[i] = src[i];
        }

        void put_char(char c, uint32_t color) {
            if (c == '\n') { cursor_x = 0; cursor_y += 12; return; }
            if (c == '\b') { 
                if (cursor_x >= 8) cursor_x -= 8;
                draw_rect(cursor_x, cursor_y, 8, 8, 0x00080808);
                return;
            }
            const uint8_t* glyph = font8x8_basic[c - 32];
            for (int gy = 0; gy < 8; gy++) {
                for (int gx = 0; gx < 8; gx++) {
                    if (glyph[gy] & (0x80 >> gx)) put_pixel(cursor_x + gx, cursor_y + gy, color);
                }
            }
            cursor_x += 8;
            if (cursor_x >= width_) { cursor_x = 0; cursor_y += 12; }
        }
    };
}

#endif
