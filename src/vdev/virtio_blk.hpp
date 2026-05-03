#ifndef EOS_VIRTIO_BLK_HPP
#define EOS_VIRTIO_BLK_HPP

#include "substrate_service.hpp"
#include "serial.hpp"

namespace EmergenceOS {

    // Simplified VirtIO Block Device Emulator
    class VirtIOBlock {
    private:
        SubstrateService& substrate_;
        UnifiedConsole* console_;

        // Virtqueue structures (simplified)
        struct virtq_desc {
            uint64_t addr;
            uint32_t len;
            uint16_t flags;
            uint16_t next;
        };

        struct virtio_blk_req {
            uint32_t type;
            uint32_t reserved;
            uint64_t sector;
        };

    public:
        VirtIOBlock(SubstrateService& substrate, UnifiedConsole* console = nullptr) 
            : substrate_(substrate), console_(console) {}

        void initialize() {
            if (console_) {
                console_->print("[VirtIO] Block Device Emulator Online.\n");
                console_->print("[VirtIO] Mapping Substrate to Virtual PCI ID 0x1042.\n");
            }
        }

        // Called when the Guest OS performs an I/O write to the Virtqueue Notify register
        void handle_guest_notify(uint16_t queue_index, uint64_t desc_table_paddr, uint16_t desc_idx) {
            if (queue_index != 0) return; // We only support queue 0 (request queue)
            
            // In a real implementation, we would translate desc_table_paddr from Guest Physical 
            // to Host Physical. For now, we simulate the request extraction.
            
            virtq_desc* desc = (virtq_desc*)desc_table_paddr;
            if (!desc) return;

            virtio_blk_req* req = (virtio_blk_req*)desc[desc_idx].addr;
            uint8_t* buffer = (uint8_t*)desc[desc[desc_idx].next].addr;
            uint32_t sector_count = desc[desc[desc_idx].next].len / 512;

            if (req->type == 0) { // VIRTIO_BLK_T_IN (Read)
                substrate_.serve_read(req->sector, sector_count, buffer);
            } else if (req->type == 1) { // VIRTIO_BLK_T_OUT (Write)
                substrate_.serve_write(req->sector, sector_count, buffer);
            }

            // Raise virtual interrupt to Guest to signal completion
            inject_guest_interrupt();
        }

    private:
        void inject_guest_interrupt() {
            // Emulate IRQ injection into the VMCS (Intel) or VMCB (AMD)
        }
    };
}

#endif
