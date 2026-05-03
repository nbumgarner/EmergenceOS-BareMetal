#ifndef EMERGENCE_STATE_ENGINE_HPP
#define EMERGENCE_STATE_ENGINE_HPP

#include "topology.hpp"

namespace Emergence {

    class StateEngine {
    public:
        explicit StateEngine(Topology& topo) : topo_(topo), current_state_(0) {
            __atomic_store_n(&chaos_accumulator_, 0, __ATOMIC_RELAXED);
        }

        void reset(uint16_t initial_state = 0) {
            current_state_ = initial_state;
            __atomic_store_n(&chaos_accumulator_, 0, __ATOMIC_RELEASE);
        }

        struct ExecutionResult {
            uint16_t prev_state;
            uint16_t input;
            uint16_t next_state;
            Value128 value;
            bool valid;
        };

        ExecutionResult step(uint16_t input) {
            uint64_t vaddr = ((uint64_t)current_state_ << 10) | (input & LENS_MASK);
            ExecutionResult res;
            res.prev_state = current_state_;
            res.input = input;

            uint32_t current_chaos = __atomic_fetch_add(&chaos_accumulator_, 1, __ATOMIC_RELAXED);

            if (current_chaos < ENTROPY_THRESHOLD) {
                res.value.lo = (vaddr * PHI_LO) ^ input;
                res.value.pack_route(Emergence::FREE_ROUTE); // Use FREE_ROUTE for speculative transient
                res.valid = true;
                res.next_state = (current_state_ ^ input) & LENS_MASK;
                current_state_ = res.next_state;
                return res;
            }

            Value128 v = topo_.holographic_fetch(vaddr);
            res.value = v;
            uint16_t nxt = v.route();

            if (nxt != FREE_ROUTE && nxt != ERROR_ROUTE) {
                res.valid = true;
                res.next_state = nxt;
                current_state_ = nxt;
                __atomic_store_n(&chaos_accumulator_, 0, __ATOMIC_RELEASE);
            } else {
                res.valid = false;
                res.next_state = (current_state_ ^ input) & LENS_MASK;
                current_state_ = res.next_state;
                __atomic_store_n(&chaos_accumulator_, 0, __ATOMIC_RELEASE);
            }

            return res;
        }

    private:
        Topology& topo_;
        uint16_t current_state_;
        volatile uint32_t chaos_accumulator_;

        static constexpr uint32_t ENTROPY_THRESHOLD = 64;
    };
}

#endif
