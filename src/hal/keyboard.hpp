#ifndef EOS_KEYBOARD_HPP
#define EOS_KEYBOARD_HPP

#include <stdint.h>

namespace EmergenceOS {
    class Keyboard {
    private:
        const uint16_t DATA_PORT = 0x60;
        const uint16_t STATUS_PORT = 0x64;
        
        uint8_t modifiers = 0; // bits: 0=ctrl, 1=alt, 2=shift
        bool hotkey_triggered = false;

        inline uint8_t inb(uint16_t port) {
            uint8_t ret;
            asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
            return ret;
        }

        bool is_data_available() {
            return inb(STATUS_PORT) & 0x01;
        }

        const char scancode_map[128] = {
            0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
            '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
            0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
            '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*',
            0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        };

    public:
        // Non-ASCII Scancodes
        static constexpr uint8_t KEY_TAB = 0x0F;
        static constexpr uint8_t KEY_UP = 0x48;
        static constexpr uint8_t KEY_DOWN = 0x50;
        static constexpr uint8_t KEY_LEFT = 0x4B;
        static constexpr uint8_t KEY_RIGHT = 0x4D;
        static constexpr uint8_t KEY_F1 = 0x3B;
        static constexpr uint8_t KEY_F2 = 0x3C;

        Keyboard() {}

        uint8_t read_raw_scancode() {
            if (!is_data_available()) return 0;
            return inb(DATA_PORT);
        }

        char scancode_to_char(uint8_t scancode) {
            if (scancode & 0x80) return 0;
            return scancode_map[scancode];
        }

        void update_state() {
            if (!is_data_available()) return;
            uint8_t scancode = inb(DATA_PORT);
            
            // Handle Modifiers
            if (scancode == 0x1D) modifiers |= 0x01;      // Ctrl press
            else if (scancode == 0x9D) modifiers &= ~0x01; // Ctrl release
            else if (scancode == 0x38) modifiers |= 0x02;      // Alt press
            else if (scancode == 0xB8) modifiers &= ~0x02; // Alt release
            else if (scancode == 0x2A || scancode == 0x36) modifiers |= 0x04; // Shift press
            else if (scancode == 0xAA || scancode == 0xB6) modifiers &= ~0x04; // Shift release
            
            // Check Hotkey: Ctrl+Alt+Shift + E (0x12)
            if (scancode == 0x12 && modifiers == 0x07) {
                hotkey_triggered = true;
            }
        }

        bool consume_hotkey() {
            bool ret = hotkey_triggered;
            hotkey_triggered = false;
            return ret;
        }

        char read_char() {
            while (true) {
                update_state();
                if (is_data_available()) {
                    uint8_t scancode = inb(DATA_PORT);
                    if (scancode & 0x80) continue;
                    char c = scancode_map[scancode];
                    if (c > 0) return c;
                }
            }
        }
        
        char read_char_nonblock() {
            update_state();
            if (is_data_available()) {
                uint8_t scancode = inb(DATA_PORT);
                if (scancode & 0x80) return 0;
                return scancode_map[scancode];
            }
            return 0;
        }

        void read_password(char* buf, int max, Graphics* vga = nullptr) {
            int i = 0;
            while(i < max - 1) {
                char c = read_char();
                if (c == '\n' || c == '\r') break;
                if (c == '\b' && i > 0) {
                    i--;
                    if (vga) { vga->put_char('\b', 0); vga->swap_buffers(); }
                    continue;
                }
                buf[i++] = c;
                if (vga) { vga->put_char('*', 0x00FFFF00); vga->swap_buffers(); }
            }
            buf[i] = '\0';
        }

        void read_line(char* buf, int max, Graphics* vga = nullptr) {
            int i = 0;
            while(i < max - 1) {
                char c = read_char();
                if (c == '\n') break;
                if (c == '\b' && i > 0) {
                    i--;
                    if (vga) { vga->put_char('\b', 0); vga->swap_buffers(); }
                    continue;
                }
                buf[i++] = c;
                if (vga) { vga->put_char(c, 0x00FFFFFF); vga->swap_buffers(); }
            }
            buf[i] = '\0';
        }
    };
}

#endif
