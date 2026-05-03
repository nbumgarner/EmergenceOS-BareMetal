#ifndef EOS_PCI_HPP
#define EOS_PCI_HPP

#include "serial.hpp"

namespace EmergenceOS {
    class PCIController {
    private:
        // Assembly instructions for 32-bit Port I/O
        inline void outl(unsigned short port, unsigned int data) {
            asm volatile("outl %0, %1" : : "a"(data), "Nd"(port));
        }
        inline unsigned int inl(unsigned short port) {
            unsigned int ret;
            asm volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
            return ret;
        }

        struct Device {
            unsigned char bus, slot, func;
            unsigned short vendor, device;
            unsigned char base_class, sub_class, prog_if;
            uintptr_t bar5;
        };

        Device get_device(unsigned char bus, unsigned char slot, unsigned char func) {
            Device dev = {bus, slot, func, 0, 0, 0, 0, 0, 0};
            unsigned int d0 = pci_config_read(bus, slot, func, 0);
            dev.vendor = d0 & 0xFFFF;
            dev.device = (d0 >> 16) & 0xFFFF;
            
            if (dev.vendor == 0xFFFF) return dev;

            unsigned int d8 = pci_config_read(bus, slot, func, 8);
            dev.base_class = (d8 >> 24) & 0xFF;
            dev.sub_class = (d8 >> 16) & 0xFF;
            dev.prog_if = (d8 >> 8) & 0xFF;

            unsigned int d24 = pci_config_read(bus, slot, func, 0x24);
            dev.bar5 = d24 & ~0xF; // Base address (memory mapped)
            
            return dev;
        }

        unsigned int pci_config_read(unsigned char bus, unsigned char slot, unsigned char func, unsigned char offset) {
            unsigned int address = (unsigned int)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | 0x80000000);
            outl(0xCF8, address);
            return inl(0xCFC);
        }

    public:
        void scan_bus(UnifiedConsole& serial) {
            serial.print("\n[PCI] Initiating Hardware Scan...\n");
            
            for (unsigned char bus = 0; bus < 8; bus++) {
                for (unsigned char slot = 0; slot < 32; slot++) {
                    Device dev = get_device(bus, slot, 0);
                    if (dev.vendor == 0xFFFF) continue;

                    serial.print("[PCI] ");
                    serial.print_hex(dev.vendor); serial.print(":");
                    serial.print_hex(dev.device);
                    serial.print(" | Class "); serial.print_hex(dev.base_class);
                    serial.print("."); serial.print_hex(dev.sub_class);

                    if (dev.base_class == 0x01 && dev.sub_class == 0x06) {
                        serial.print(" <-- [AHCI STORAGE]");
                    }
                    if (dev.base_class == 0x02) {
                        serial.print(" <-- [NETWORK]");
                    }
                    serial.print("\n");
                }
            }
        }
        
        uintptr_t find_ahci_base() {
            for (unsigned char bus = 0; bus < 8; bus++) {
                for (unsigned char slot = 0; slot < 32; slot++) {
                    Device dev = get_device(bus, slot, 0);
                    if (dev.base_class == 0x01 && dev.sub_class == 0x06) return dev.bar5;
                }
            }
            return 0;
        }
    };
}
#endif
