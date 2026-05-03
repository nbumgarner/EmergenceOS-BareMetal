#ifndef EOS_PREDICTIVE_TOPOLOGY_HPP
#define EOS_PREDICTIVE_TOPOLOGY_HPP

#include "memory.hpp"
#include "topology.hpp"
#include "serial.hpp"

namespace EmergenceOS {

    inline void trigger_ipi_rollback(uint8_t target_core) {
        #if defined(__x86_64__)
            volatile uint32_t* lapic_icr_low = (volatile uint32_t*)0xFEE00300;
            volatile uint32_t* lapic_icr_high = (volatile uint32_t*)0xFEE00310;
            *lapic_icr_high = (target_core << 24);
            *lapic_icr_low = 0x4000 | 0x80;
        #elif defined(__aarch64__)
            static volatile uint32_t* g_gicd_base = (volatile uint32_t*)0x08000000; // Default for QEMU virt
            if (g_gicd_base) {
                *(g_gicd_base + (0xF00 / 4)) = (target_core << 16) | 0x00;
            }
        #endif
    }

    struct SpeculativeRecord {
        uint64_t vaddr;
        Emergence::Value128 predicted;
    };

    class PredictiveEngine {
    private:
        Emergence::Topology& substrate_;
        volatile SpeculativeRecord spec_ring_[256];
        volatile uint32_t head_;
        volatile uint32_t tail_;

    public:
        PredictiveEngine(Emergence::Topology& topo)
            : substrate_(topo), head_(0), tail_(0) {}

        Emergence::Value128 speculative_fetch(uint64_t vaddr) {
            Emergence::Value128 guess;
            guess.lo = (vaddr * Emergence::PHI_LO) ^ Emergence::PHI_HI;
            guess.pack_route(Emergence::SPECULATIVE_ROUTE);

            uint32_t current_head = head_;
            uint32_t next = (current_head + 1) % 256;

            spec_ring_[current_head].vaddr = vaddr;
            spec_ring_[current_head].predicted = guess;

            HardwareBarrier::store_sync();
            head_ = next;

            return guess;
        }

        void verify_consensus() {
            while (tail_ != head_) {
                HardwareBarrier::load_sync();
                uint32_t current_tail = tail_;
                uint64_t target_vaddr = spec_ring_[current_tail].vaddr;
                Emergence::Value128 predicted = spec_ring_[current_tail].predicted;

                HardwareBarrier::full_sync();
                Emergence::Value128 actual = substrate_.holographic_fetch(target_vaddr);

                if (actual.lo != predicted.lo || actual.route() != predicted.route()) {
                    trigger_ipi_rollback(0);
                    head_ = current_tail;
                    break;
                }
                tail_ = (current_tail + 1) % 256;
            }
        }
    };
}

#endif
