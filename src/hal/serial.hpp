#ifndef EOS_SERIAL_HPP
#define EOS_SERIAL_HPP

#include <stdint.h>

namespace EmergenceOS {

    inline volatile uint8_t* g_mmio_uart_base = (volatile uint8_t*)0x09000000;

    class SerialPort {
    public:
        void write_char(char c) {
            if (!g_mmio_uart_base) return;
            while ((*(g_mmio_uart_base + 5) & 0x20) == 0);
            *g_mmio_uart_base = c;
        }

        void print(const char* s) {
            while (*s) write_char(*s++);
        }

        void print_hex(uint64_t val) {
            const char* hex = "0123456789ABCDEF";
            for (int i = 15; i >= 0; i--) write_char(hex[(val >> (i * 4)) & 0xF]);
        }
    };

    class UnifiedConsole : public SerialPort {
    public:
        void set_vga(void* vga) { (void)vga; }
        void read_line(char* buffer, int max_len, bool hidden = false) {
            buffer[0] = '\0';
            (void)max_len; (void)hidden;
        }
    };
}

#endif
