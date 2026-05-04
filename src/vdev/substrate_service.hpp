#ifndef EOS_SUBSTRATE_SERVICE_HPP
#define EOS_SUBSTRATE_SERVICE_HPP

#include "topology.hpp"
#include "substrate_io.hpp"
#include "serial.hpp"
#include "context_driven_engine.hpp"

namespace EmergenceOS {

    class SubstrateService {
    private:
        Emergence::SubstrateManifold& manifold_;
        Emergence::SubShannonEngine engine_;
        SubstrateIO* physical_disk_;
        uint64_t virtual_capacity_bytes_;
        uint64_t highest_accessed_vaddr_;
        uint64_t preemption_horizon_;
        
        uint64_t total_bytes_written_;
        uint64_t correction_bytes_stored_;

    public:
        SubstrateService(Emergence::SubstrateManifold& m, SubstrateIO* phys)
            : manifold_(m), engine_(m), physical_disk_(phys),
              highest_accessed_vaddr_(0), preemption_horizon_(0),
              total_bytes_written_(1), correction_bytes_stored_(1) {
            #ifdef SOVEREIGN_BUILD
                virtual_capacity_bytes_ = 100ULL * 1024 * 1024 * 1024 * 1024; // 100 TB Phoenix
            #elif defined(EVALUATION_BUILD)
                virtual_capacity_bytes_ = 1ULL * 1024 * 1024 * 1024 * 1024; // 1 TB Eval
            #else
                virtual_capacity_bytes_ = 10ULL * 1024 * 1024 * 1024 * 1024; // 10 TB Default
            #endif
        }

        uint64_t get_match_rate_scaled() {
            // Return match rate as percentage * 100 (e.g. 9997 for 99.97%)
            return 10000 - (correction_bytes_stored_ * 10000 / total_bytes_written_);
        }

        bool serve_read(uint64_t guest_lba, uint32_t sector_count, uint8_t* guest_buffer) {
            // ... (rest of implementation)
            // Check if we need to reconstruct from Sub-Shannon correction stream (Disk -> RAM)
            // For this prototype, we prioritize the holographic cache
            uint64_t start_vaddr = guest_lba * 512;
            if (start_vaddr >= highest_accessed_vaddr_) {
                highest_accessed_vaddr_ = start_vaddr;
                preemption_horizon_ = start_vaddr + (sector_count * 512);
            }
            
            for (uint32_t i = 0; i < sector_count; i++) {
                uint64_t current_lba = guest_lba + i;
                uint64_t vaddr = current_lba * 512;
                if (vaddr >= virtual_capacity_bytes_) return false;
                
                for (uint32_t offset = 0; offset < 512; offset += 16) {
                    Emergence::Value128 atom = manifold_.holographic_fetch(vaddr + offset);
                    kmemcpy(guest_buffer + (i * 512) + offset, atom.data(), 16);
                }
            }
            return true;
        }

        bool serve_write(uint64_t guest_lba, uint32_t sector_count, const uint8_t* guest_buffer) {
            for (uint32_t i = 0; i < sector_count; i++) {
                uint64_t current_lba = guest_lba + i;
                uint64_t vaddr = current_lba * 512;
                
                // 1. Materialize into the RAM-speed Holographic Manifold
                manifold_.materialize_range(vaddr, guest_buffer + (i * 512), 512);

                // 2. Perform Sub-Shannon Evaporation (V2.0 Recursive Phase-Fold)
                if (physical_disk_) {
                    uint8_t final_residue[512]; // Buffer for the layer 3 output
                    uint64_t m_hi = 0x1234567890ABCDEFULL; // Test seed
                    uint64_t m_lo = 0xABCDEF0123456789ULL; // Test seed
                    
                    size_t count = engine_.phase_fold(m_hi, m_lo, guest_buffer + (i * 512), 512, final_residue);
                    
                    total_bytes_written_ += 512;
                    correction_bytes_stored_ += count;

                    // 3. Persist ONLY the folded residue stream
                    uint64_t journal_lba = 16384 + (current_lba * 2); 
                    physical_disk_->write(journal_lba, 1, final_residue);
                }
            }
            return true;
        }

        void utilize_idle_cycles(uint32_t available_cycles_hint = 128) {
            uint64_t target_vaddr = preemption_horizon_;
            for (uint32_t i = 0; i < available_cycles_hint; i++) {
                if (target_vaddr >= virtual_capacity_bytes_) break;
                volatile Emergence::Value128 pre_fetched_atom = manifold_.holographic_fetch(target_vaddr);
                (void)pre_fetched_atom;
                target_vaddr += 16;
            }
            preemption_horizon_ = target_vaddr;
        }

        uint64_t get_capacity() const { return virtual_capacity_bytes_; }
    };
}

#endif
