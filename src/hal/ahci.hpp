#ifndef EOS_AHCI_HPP
#define EOS_AHCI_HPP

#include "substrate_io.hpp"
#include "memory.hpp"
#include "serial.hpp"

namespace EmergenceOS {

    /**
     * AHCI DMA Structures (HBA Specifications)
     */
    struct HBAPRDTEntry {
        uint32_t dba;       // Data base address
        uint32_t dbau;      // Data base address upper 32 bits
        uint32_t rsv0;      // Reserved
        uint32_t dbc:31;    // Byte count (max 4MB)
        uint32_t i:1;       // Interrupt on completion
    };

    struct HBACommandTable {
        uint8_t  cfis[64];  // Command FIS
        uint8_t  acmd[16];  // ATAPI command
        uint8_t  rsv[48];   // Reserved
        HBAPRDTEntry prdt_entry[1]; // PRDT entries (we use 1 for simplicity)
    };

    struct HBACommandHeader {
        uint8_t  cfl:5;     // Command FIS length in dwords
        uint8_t  a:1;       // ATAPI
        uint8_t  w:1;       // Write
        uint8_t  p:1;       // Prefetchable
        uint8_t  r:1;       // Reset
        uint8_t  b:1;       // BIST
        uint8_t  c:1;       // Clear busy upon R_OK
        uint8_t  rsv0:1;    // Reserved
        uint8_t  pmp:4;     // Port multiplier port
        uint16_t prdtl;     // PRDT length in entries
        volatile uint32_t prdbc; // PRD byte count transferred
        uint32_t ctba;      // Command table descriptor base address
        uint32_t ctbau;     // Command table descriptor base address upper 32 bits
        uint32_t rsv1[4];   // Reserved
    };

    struct AHCIHBA {
        uint32_t cap;       // Host capabilities
        uint32_t ghc;       // Global host control
        uint32_t is;        // Interrupt status
        uint32_t pi;        // Ports implemented
        uint32_t vs;        // Version
        uint32_t ccc_ctl;   // Command completion coalescing control
        uint32_t ccc_pts;   // Command completion coalescing ports
        uint32_t em_loc;    // Enclosure management location
        uint32_t em_ctl;    // Enclosure management control
        uint32_t cap2;      // Host capabilities extended
        uint32_t bohc;      // BIOS/OS handoff control and status
    };

    struct HBAPort {
        uint32_t clb;       // Command list base address
        uint32_t clbu;      // Command list base address upper
        uint32_t fb;        // FIS base address
        uint32_t fbu;       // FIS base address upper
        uint32_t is;        // Interrupt status
        uint32_t ie;        // Interrupt enable
        uint32_t cmd;       // Command and status
        uint32_t rsv0;      // Reserved
        uint32_t tfd;       // Task file data
        uint32_t sig;       // Signature
        uint32_t ssts;      // SATA status
        uint32_t sctl;      // SATA control
        uint32_t serr;      // SATA error
        uint32_t sact;      // SATA active
        uint32_t ci;        // Command issue
        uint32_t sntf;      // SATA notification
        uint32_t fbs;       // FIS-based switching control
        uint32_t rsv1[11];  // Reserved
        uint32_t vendor[4]; // Vendor specific
    };

    class AHCIDriver : public SubstrateIO {
    private:
        uintptr_t base_addr_;
        volatile AHCIHBA* hba_;
        int active_port_;
        
        HBACommandHeader* cmd_list_;
        HBACommandTable* cmd_table_;
        uint8_t* fis_base_;

        volatile HBAPort* get_port(int n) {
            return (volatile HBAPort*)(base_addr_ + 0x100 + (n * 0x80));
        }

    public:
        AHCIDriver(uintptr_t base) : base_addr_(base), active_port_(-1) {
            hba_ = (volatile AHCIHBA*)base;
        }

        void initialize(void* console_ptr) override {
            UnifiedConsole* console = (UnifiedConsole*)console_ptr;
            if (!base_addr_) return;

            // Global Host Control: Reset and Enable AHCI
            hba_->ghc |= (1 << 31); // AE bit
            
            for (int i = 0; i < 32; i++) {
                if (hba_->pi & (1 << i)) {
                    volatile HBAPort* p = get_port(i);
                    if ((p->ssts & 0x0F) == 3) { 
                        active_port_ = i;
                        setup_port(p);
                        console->print("[AHCI] Hardware Verified on Port ");
                        console->print_hex(i);
                        console->print("\n");
                        break;
                    }
                }
            }
        }

        void setup_port(volatile HBAPort* port) {
            // Stop port
            port->cmd &= ~(1 << 0);  // ST
            port->cmd &= ~(1 << 4);  // FRE
            while (port->cmd & (1 << 15) || port->cmd & (1 << 14)); // Wait for CR, FR to clear

            // Allocate and set base addresses
            cmd_list_ = (HBACommandHeader*)EmergenceOS::g_pmm->allocate(1024);
            kmemset(cmd_list_, 0, 1024);
            port->clb = (uint32_t)(uintptr_t)cmd_list_;
            port->clbu = 0;

            fis_base_ = (uint8_t*)EmergenceOS::g_pmm->allocate(256);
            kmemset(fis_base_, 0, 256);
            port->fb = (uint32_t)(uintptr_t)fis_base_;
            port->fbu = 0;

            cmd_table_ = (HBACommandTable*)EmergenceOS::g_pmm->allocate(sizeof(HBACommandTable));
            kmemset(cmd_table_, 0, sizeof(HBACommandTable));

            // Start port
            port->cmd |= (1 << 4);  // FRE
            port->cmd |= (1 << 0);  // ST
        }

        bool issue_command(uint64_t lba, uint32_t count, uint8_t* buffer, bool write) {
            if (active_port_ == -1) return false;
            volatile HBAPort* port = get_port(active_port_);
            
            cmd_list_[0].cfl = 5; // FIS length
            cmd_list_[0].w = write ? 1 : 0;
            cmd_list_[0].prdtl = 1;
            cmd_list_[0].ctba = (uint32_t)(uintptr_t)cmd_table_;
            cmd_list_[0].ctbau = 0;

            kmemset(cmd_table_, 0, sizeof(HBACommandTable));
            cmd_table_->prdt_entry[0].dba = (uint32_t)(uintptr_t)buffer;
            cmd_table_->prdt_entry[0].dbau = 0;
            cmd_table_->prdt_entry[0].dbc = (count << 9) - 1; // 512 bytes per sector
            cmd_table_->prdt_entry[0].i = 1;

            uint8_t* fis = cmd_table_->cfis;
            fis[0] = 0x27; // Register H2D
            fis[1] = 0x80; // Command
            fis[2] = write ? 0x35 : 0x25; // WRITE DMA EXT or READ DMA EXT
            
            fis[4] = lba & 0xFF;
            fis[5] = (lba >> 8) & 0xFF;
            fis[6] = (lba >> 16) & 0xFF;
            fis[7] = 0x40; // LBA mode
            
            fis[8] = (lba >> 24) & 0xFF;
            fis[9] = (lba >> 32) & 0xFF;
            fis[10] = (lba >> 40) & 0xFF;
            
            fis[12] = count & 0xFF;
            fis[13] = (count >> 8) & 0xFF;

            // Wait for port ready
            int spin = 0;
            while ((port->tfd & (0x80 | 0x08)) && spin < 1000000) spin++;
            if (spin == 1000000) return false;

            port->ci = 1; // Issue command

            // Wait for completion
            while (true) {
                if (!(port->ci & 1)) break;
                if (port->is & (1 << 30)) return false; // Task file error
            }
            
            return true;
        }

        void read(uint64_t lba, uint32_t count, uint8_t* buffer) override {
            issue_command(lba, count, buffer, false);
        }

        void write(uint64_t lba, uint32_t count, const uint8_t* buffer) override {
            issue_command(lba, count, (uint8_t*)buffer, true);
        }
        
        bool is_ready() { return active_port_ != -1; }
    };
}

#endif
