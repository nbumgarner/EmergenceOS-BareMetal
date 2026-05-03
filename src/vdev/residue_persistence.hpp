#ifndef EOS_RESIDUE_PERSISTENCE_HPP
#define EOS_RESIDUE_PERSISTENCE_HPP

#include "substrate_io.hpp"
#include "topology.hpp"
#include "serial.hpp"

namespace EmergenceOS {

    struct ResidueRecord {
        uint64_t vblock_idx;
        uint16_t local_atom_idx;
        Emergence::Value128 delta;
        uint32_t checksum;
    } __attribute__((packed));

    class PersistenceController {
    private:
        static constexpr uint64_t JOURNAL_START_LBA = 8192;
        static constexpr uint64_t MAX_RESIDUES = 1000000;
        uint64_t current_journal_tail_;
        SubstrateIO* disk_;
        Emergence::Seed master_seed_;

        inline uint32_t calculate_checksum(const ResidueRecord& rec) {
            return (uint32_t)(rec.vblock_idx ^ rec.local_atom_idx ^ rec.delta.hi ^ rec.delta.lo);
        }

    public:
        PersistenceController(SubstrateIO* disk, Emergence::Seed master)
            : current_journal_tail_(0), disk_(disk), master_seed_(master) {}

        void commit_residue(uint64_t vblock, Emergence::Block* actual_block) {
            if (!disk_) return;

            ResidueRecord journal_batch[Emergence::ENTRIES_PER_BLOCK];
            uint32_t batch_count = 0;

            for (uint16_t j = 0; j < Emergence::ENTRIES_PER_BLOCK; j++) {
                Emergence::Value128 expected_noise;
                expected_noise.pack_route(Emergence::SPECULATIVE_ROUTE);
                expected_noise.lo = master_seed_.lo ^ (vblock * Emergence::PHI_LO) ^ j;
                
                Emergence::Value128 actual_atom = actual_block->entries[j];
                Emergence::Value128 residue;
                residue.hi = actual_atom.hi ^ expected_noise.hi;
                residue.lo = actual_atom.lo ^ expected_noise.lo;

                if (residue.hi != 0 || residue.lo != 0) {
                    ResidueRecord rec;
                    rec.vblock_idx = vblock;
                    rec.local_atom_idx = j;
                    rec.delta = residue;
                    rec.checksum = calculate_checksum(rec);
                    journal_batch[batch_count++] = rec;
                }
            }

            if (batch_count > 0) {
                uint64_t start_lba = JOURNAL_START_LBA + (current_journal_tail_ / 16);
                uint32_t sectors_needed = (batch_count / 16) + 1;
                disk_->write(start_lba, sectors_needed, (const uint8_t*)journal_batch);
                current_journal_tail_ += batch_count;
            }
        }

        void recover_state(Emergence::SubstrateManifold& manifold) {
            if (!disk_) return;
            SerialPort serial;
            serial.print("[EM-1] Initiating Topological Resurrection from Residue Journal...\n");

            uint8_t sector_buffer[512];
            uint64_t residues_applied = 0;

            for (uint64_t lba_offset = 0; lba_offset < (MAX_RESIDUES / 16); lba_offset++) {
                disk_->read(JOURNAL_START_LBA + lba_offset, 1, sector_buffer);
                ResidueRecord* records = (ResidueRecord*)sector_buffer;

                for (int i = 0; i < 16; i++) {
                    if (records[i].vblock_idx == 0 && records[i].local_atom_idx == 0 && records[i].delta.lo == 0) {
                        current_journal_tail_ = (lba_offset * 16) + i;
                        serial.print("[EM-1] Resurrection Complete.\n");
                        return;
                    }

                    if (calculate_checksum(records[i]) != records[i].checksum) {
                        serial.print("\n[EM-1] FATAL: Journal Corruption Detected!\n");
                        while(1) { #if defined(__x86_64__) 
                                     __asm__ __volatile__("hlt"); 
                                   #elif defined(__aarch64__)
                                     __asm__ __volatile__("wfi");
                                   #endif
                                 }
                    }

                    Emergence::Value128 baseline;
                    baseline.pack_route(Emergence::SPECULATIVE_ROUTE);
                    baseline.lo = master_seed_.lo ^ (records[i].vblock_idx * Emergence::PHI_LO) ^ records[i].local_atom_idx;

                    Emergence::Value128 restored_atom;
                    restored_atom.hi = baseline.hi ^ records[i].delta.hi;
                    restored_atom.lo = baseline.lo ^ records[i].delta.lo;

                    manifold.inject_restored_atom(records[i].vblock_idx, records[i].local_atom_idx, restored_atom);
                    residues_applied++;
                }
            }
        }
    };
}

#endif
