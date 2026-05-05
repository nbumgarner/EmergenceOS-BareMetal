#ifndef EMERGENCE_TOPOLOGY_HPP
#define EMERGENCE_TOPOLOGY_HPP

#include "memory.hpp"
#include "substrate_io.hpp"

namespace Emergence {

    constexpr int LENS_COUNT = 1024;
    constexpr uint16_t LENS_MASK = 0x3FF;
    constexpr uint16_t HALT_ROUTE = 0x3FF;
    constexpr uint16_t ERROR_ROUTE = 0x3FE;
    constexpr uint16_t FREE_ROUTE = 0x3FD;
    
    constexpr uint64_t PHI_HI = 0x9E3779B97F4A7C15ULL;
    constexpr uint64_t PHI_LO = 0x517CC1B727220A95ULL;

    struct Value128 {
        uint64_t hi, lo;
        uint8_t* data() { return (uint8_t*)this; }
        uint16_t route() const { return (uint16_t)(hi >> 54) & LENS_MASK; }
        void pack_route(uint16_t r) {
            hi &= 0x000FFFFFFFFFFFFFULL;
            hi |= ((uint64_t)(r & LENS_MASK) << 54);
        }
        void get_payload(uint8_t* out, int len) { kmemcpy(out, this, len); }
        void set_payload(const uint8_t* in, int len) { kmemcpy(this, in, len); }
    };

    struct Seed { uint64_t hi, lo; };

    class Topology {
    public:
        virtual ~Topology() = default;
        virtual void initialize(Seed s) = 0;
        virtual Value128 holographic_fetch(uint64_t vaddr) = 0;
        virtual void materialize_range(uint64_t vaddr, const uint8_t* data, size_t len) = 0;
        virtual size_t get_used_blocks() const = 0;
        virtual size_t total_l2_blocks() const = 0;
        virtual bool save_image(const char* path) = 0;
        virtual bool load_image(const char* path, Seed seed) = 0;
        virtual size_t get_l2_count(uint16_t slot) const = 0;
        virtual size_t read_slot(uint16_t slot, uint8_t* out, size_t max_len) const = 0;
        virtual size_t write_slot(uint16_t slot, const uint8_t* data, size_t len) = 0;
        virtual void clear_slot(uint16_t slot) = 0;
    };

    class SubstrateManifold : public Topology {
    private:
        struct Block {
            Value128 entries[1024];
        };

        enum class BlockState { EMPTY, RESIDENT, DIRTY };

        static constexpr int CACHE_POOL_SIZE = 1024;
        Block* cache_pool_;
        BlockState cache_state_[CACHE_POOL_SIZE];
        uint64_t vblock_map_[CACHE_POOL_SIZE];
        
        uint32_t vblock_counter_ = 0;

        uint32_t get_physical_block(uint64_t vblock_idx, bool create) {
            for (int i = 0; i < CACHE_POOL_SIZE; i++) {
                if (cache_state_[i] != BlockState::EMPTY && vblock_map_[i] == vblock_idx) return i;
            }
            if (!create) return 0xFFFFFFFF;
            
            uint32_t victim = vblock_counter_ % CACHE_POOL_SIZE;
            vblock_map_[victim] = vblock_idx;
            cache_state_[victim] = BlockState::RESIDENT;
            vblock_counter_++;
            return victim;
        }

    public:
        SubstrateManifold() {
            cache_pool_ = (Block*)EmergenceOS::g_pmm->allocate(sizeof(Block) * CACHE_POOL_SIZE);
            for (int i = 0; i < CACHE_POOL_SIZE; i++) cache_state_[i] = BlockState::EMPTY;
        }

        void manual_init(uint32_t ram_mb, void* io) { (void)ram_mb; (void)io; }

        void initialize(Seed s) override { (void)s; }

        Value128 holographic_fetch(uint64_t vaddr) override {
            uint64_t vblock_idx = vaddr / (1024 * 16);
            uint32_t phys_idx = get_physical_block(vblock_idx, false);
            if (phys_idx == 0xFFFFFFFF) return {0, 0};
            return cache_pool_[phys_idx].entries[(vaddr / 16) % 1024];
        }

        void materialize_range(uint64_t vaddr, const uint8_t* data, size_t len) override {
            uint64_t vblock_idx = vaddr / (1024 * 16);
            uint32_t phys_idx = get_physical_block(vblock_idx, true);
            kmemcpy(&cache_pool_[phys_idx].entries[(vaddr / 16) % 1024], data, len);
        }

        size_t get_used_blocks() const override { return vblock_counter_; }
        size_t total_l2_blocks() const override { return vblock_counter_; }
        bool save_image(const char*) override { return true; }
        bool load_image(const char*, Seed) override { return true; }
        size_t get_l2_count(uint16_t) const override { return vblock_counter_; }

        size_t read_slot(uint16_t slot, uint8_t* out, size_t max_len) const override {
            uint32_t phys_idx = (slot % CACHE_POOL_SIZE);
            if (cache_state_[phys_idx] == BlockState::EMPTY) return 0;
            size_t to_read = (max_len < 16) ? max_len : 16;
            kmemcpy(out, &cache_pool_[phys_idx].entries[0], to_read);
            return to_read;
        }

        size_t write_slot(uint16_t slot, const uint8_t* data, size_t len) override {
            uint32_t phys_idx = (slot % CACHE_POOL_SIZE);
            cache_state_[phys_idx] = BlockState::RESIDENT;
            size_t to_write = (len < 16) ? len : 16;
            kmemcpy(&cache_pool_[phys_idx].entries[0], data, to_write);
            return to_write;
        }

        void clear_slot(uint16_t slot) override {
            uint32_t phys_idx = (slot % CACHE_POOL_SIZE);
            cache_state_[phys_idx] = BlockState::EMPTY;
        }

        bool is_block_resident(uint32_t block_idx) const {
            if (block_idx >= CACHE_POOL_SIZE) return false;
            return cache_state_[block_idx] == BlockState::RESIDENT;
        }

        void pump_dma_queue() {}
    };
}

#endif
