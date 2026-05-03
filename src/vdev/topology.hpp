#ifndef EMERGENCE_TOPOLOGY_HPP
#define EMERGENCE_TOPOLOGY_HPP

#include "memory.hpp"
#include "substrate_io.hpp"

namespace Emergence {

    constexpr int LENS_COUNT = 1024;
    constexpr uint16_t LENS_MASK = 0x3FF;
    
    constexpr uint16_t HALT_ROUTE  = 0x3FF;
    constexpr uint16_t ERROR_ROUTE = 0x3FE;
    constexpr uint16_t FREE_ROUTE  = 0x3FD;
    constexpr uint16_t SPECULATIVE_ROUTE = 0x3FC;

    constexpr uint64_t PHI_HI = 0x9E3779B97F4A7C15ULL;
    constexpr uint64_t PHI_LO = 0x517CC1B727220A95ULL;
    constexpr size_t ENTRIES_PER_BLOCK = 1024;
    constexpr size_t BLOCK_SIZE_BYTES = ENTRIES_PER_BLOCK * 16;
    
    constexpr size_t PAYLOAD_BYTES_PER_HOP = 14; 
    constexpr size_t BYTES_PER_BLOCK = ENTRIES_PER_BLOCK * PAYLOAD_BYTES_PER_HOP;
    constexpr size_t MAX_L2_PER_SLOT = ENTRIES_PER_BLOCK;
    constexpr size_t BYTES_PER_SLOT = MAX_L2_PER_SLOT * BYTES_PER_BLOCK;

    struct Seed { uint64_t hi, lo; };

    struct Value128 {
        uint64_t hi, lo;
        Value128() : hi(0), lo(0) {}
        Value128(uint64_t h, uint64_t l) : hi(h), lo(l) {}
        
        uint16_t route() const { return (uint16_t)(hi >> 54) & LENS_MASK; }
        
        void pack_route(uint16_t r) {
            hi &= 0x000FFFFFFFFFFFFFULL;
            hi |= ((uint64_t)(r & LENS_MASK) << 54);
        }
        
        void serialize(uint8_t* b) const { kmemcpy(b, &lo, 8); kmemcpy(b+8, &hi, 8); }
        void read_from(const uint8_t* b) { kmemcpy(&lo, b, 8); kmemcpy(&hi, b+8, 8); }
        uint8_t* data() { return (uint8_t*)this; }

        size_t get_payload(uint8_t* out, size_t max_bytes) const {
            size_t copied = 0;
            for (size_t i = 0; i < 8 && copied < max_bytes; i++, copied++) 
                out[copied] = (uint8_t)(lo >> (i * 8));
            for (size_t i = 0; i < 6 && copied < max_bytes; i++, copied++) 
                out[copied] = (uint8_t)(hi >> (i * 8));
            return copied;
        }

        void set_payload(const uint8_t* data, size_t len) {
            lo = 0; uint64_t top = hi & 0xFFF0000000000000ULL; hi = 0;
            size_t pos = 0;
            for (size_t i = 0; i < 8 && pos < len; i++, pos++) lo |= ((uint64_t)data[pos] << (i * 8));
            for (size_t i = 0; i < 5 && pos < len; i++, pos++) hi |= ((uint64_t)data[pos] << (i * 8));
            hi |= top;
        }
    };

    struct Block {
        Value128 entries[ENTRIES_PER_BLOCK];
        Block() { for (int i=0; i<(int)ENTRIES_PER_BLOCK; i++) entries[i].pack_route(FREE_ROUTE); }
    };

    struct LensKey { uint64_t hi, lo; };

    struct LensGenerator {
        static void generate(Seed master, LensKey keys[LENS_COUNT]) {
            for (int i = 0; i < LENS_COUNT; i++) {
                uint64_t mix_hi = master.hi ^ (uint64_t)i;
                uint64_t mix_lo = master.lo ^ ((uint64_t)i << 32);
                for (int r = 0; r < 64; r++) {
                    mix_hi = ((mix_hi << 7) | (mix_hi >> 57)) ^ (mix_lo * PHI_HI);
                    mix_lo = ((mix_lo << 11) | (mix_lo >> 53)) ^ (mix_hi * PHI_LO);
                    mix_hi += (uint64_t)i;
                }
                keys[i].hi = mix_hi; 
                keys[i].lo = mix_lo;
            }
        }
    };

    class Topology {
    public:
        virtual ~Topology() = default;
        virtual void initialize(Seed s) = 0;
        virtual Value128 holographic_fetch(uint64_t vaddr) = 0;
        virtual size_t get_used_blocks() const = 0;
        virtual void materialize_range(uint64_t vstart, const uint8_t* data, size_t len) = 0;
        
        virtual bool traverse_8_hops(const uint16_t path[8], Value128& result) const = 0;
        virtual void materialize_8_hops(const uint16_t path[8], const Value128& terminal) = 0;
        virtual Seed get_seed() const = 0;
        virtual uint64_t get_mtv() const = 0;
        virtual size_t write_slot(uint16_t l1_slot, const uint8_t* data, size_t len) = 0;
        virtual size_t read_slot(uint16_t l1_slot, uint8_t* out, size_t max_len) const = 0;
        virtual void clear_slot(uint16_t l1_slot) = 0;
        virtual size_t total_l2_blocks() const = 0;
        
        virtual bool save_image(const char*) = 0;
        virtual bool load_image(const char*, Seed) = 0;
        virtual size_t get_l2_count(uint16_t) const = 0;
    };

    class SubstrateManifold : public Topology {
    private:
        enum class BlockState : uint8_t { FREE, PENDING_DMA, RESIDENT, DIRTY };
        
        struct DMARequest {
            uint64_t vblock_idx;
            uint32_t phys_idx;
            bool is_write;
        };

        Seed seed_;
        LensKey lens_keys_[LENS_COUNT];
        Block* cache_pool_;
        BlockState* cache_state_;
        uint64_t* cached_vblock_;
        size_t cache_size_;
        size_t used_cache_;
        EmergenceOS::SubstrateIO* disk_;
        uint32_t vblock_counter_ = 1;

        // h010g1rAM Divergence Cache
        mutable uint64_t last_vneighborhood_ = 0xFFFFFFFFFFFFFFFFULL;
        mutable uint32_t last_terminal_phys_ = 0xFFFFFFFF;

        volatile DMARequest dma_ring_[256];
        volatile uint32_t dma_head_ = 0;
        volatile uint32_t dma_tail_ = 0;

        struct SubstrateHeader {
            uint64_t magic;
            uint32_t vblock_count;
            uint32_t reserved;
        };

        inline void enqueue_dma(uint64_t vblock, uint32_t phys_idx, bool is_write) {
            if (!disk_) return;
            uint32_t next = (dma_head_ + 1) % 256;
            if (next != dma_tail_) {
                dma_ring_[dma_head_].vblock_idx = vblock;
                dma_ring_[dma_head_].phys_idx = phys_idx;
                dma_ring_[dma_head_].is_write = is_write;
                dma_head_ = next;
            }
        }

        inline void fill_probabilistic_state(uint32_t phys_idx, uint64_t vblock) {
            for(int j = 0; j < (int)ENTRIES_PER_BLOCK; j++) {
                cache_pool_[phys_idx].entries[j].pack_route(FREE_ROUTE);
                cache_pool_[phys_idx].entries[j].lo = seed_.lo ^ (vblock * PHI_LO) ^ j;
            }
        }

        uint32_t get_physical_block(uint64_t virtual_block_idx, bool create) {
            for (uint32_t i = 0; i < used_cache_; i++) {
                if (cached_vblock_[i] == virtual_block_idx) return i;
            }
            uint32_t idx = used_cache_ < cache_size_ ? used_cache_++ : 0;
            cached_vblock_[idx] = virtual_block_idx;
            if (disk_ && !create) {
                cache_state_[idx] = BlockState::PENDING_DMA;
                fill_probabilistic_state(idx, virtual_block_idx);
                enqueue_dma(virtual_block_idx, idx, false);
            } else {
                cache_state_[idx] = BlockState::DIRTY;
                for(int j=0; j<(int)ENTRIES_PER_BLOCK; j++)
                    cache_pool_[idx].entries[j].pack_route(FREE_ROUTE);
            }
            return idx;
        }

    public:
        SubstrateManifold() : cache_pool_(nullptr), cache_state_(nullptr), 
            cached_vblock_(nullptr), cache_size_(0), used_cache_(0), disk_(nullptr) {}

        void manual_init(size_t aperture_mb, EmergenceOS::SubstrateIO* disk) {
            cache_size_ = (aperture_mb * 1024 * 1024) / BLOCK_SIZE_BYTES;
            if (cache_size_ > 1024) cache_size_ = 1024;
            
            cache_pool_ = (Block*)EmergenceOS::g_pmm->allocate(cache_size_ * sizeof(Block));
            cache_state_ = (BlockState*)EmergenceOS::g_pmm->allocate(cache_size_ * sizeof(BlockState));
            cached_vblock_ = (uint64_t*)EmergenceOS::g_pmm->allocate(cache_size_ * sizeof(uint64_t));

            for(size_t i=0; i < cache_size_; i++) cache_state_[i] = BlockState::FREE;
            
            disk_ = disk;
            if (disk_) {
                SubstrateHeader head;
                disk_->read(2048, 1, (uint8_t*)&head);
                if (head.magic == 0x505645524549474EULL) {
                    vblock_counter_ = head.vblock_count;
                } else {
                    vblock_counter_ = 1;
                    head.magic = 0x505645524549474EULL;
                    head.vblock_count = 1;
                    disk_->write(2048, 1, (uint8_t*)&head);
                }
            } else {
                vblock_counter_ = 1;
            }
        }

        void initialize(Seed s) override {
            seed_ = s;
            LensGenerator::generate(seed_, lens_keys_);
            used_cache_ = 0;
            get_physical_block(0, true);
        }

        Value128 holographic_fetch(uint64_t vaddr) override {
            uint64_t current_neighborhood = vaddr >> 10;
            if (current_neighborhood == last_vneighborhood_ && last_terminal_phys_ != 0xFFFFFFFF) {
                return cache_pool_[last_terminal_phys_].entries[vaddr & LENS_MASK];
            }

            uint16_t current_route = (uint16_t)(vaddr & LENS_MASK);
            uint64_t current_vblock = 0;

            for (int i = 0; i < 7; i++) {
                uint32_t phys_idx = get_physical_block(current_vblock, false);
                __builtin_prefetch(&cache_pool_[phys_idx].entries[current_route], 0, 3);

                Value128& v = cache_pool_[phys_idx].entries[current_route];
                uint16_t nxt = v.route();
                
                if (nxt == HALT_ROUTE || nxt == FREE_ROUTE || nxt == 0) return Value128(0,0);
                
                uint16_t static_next = (vaddr >> ((i + 1) * 8)) & LENS_MASK;
                uint16_t resonance = (v.lo >> 54) & LENS_MASK;
                current_route = static_next ^ resonance;
                current_vblock = nxt;
            }

            uint32_t final_phys = get_physical_block(current_vblock, false);
            last_vneighborhood_ = current_neighborhood;
            last_terminal_phys_ = final_phys;
            return cache_pool_[final_phys].entries[current_route];
        }

        void materialize_range(uint64_t vstart, const uint8_t* data, size_t len) override {
            size_t offset = 0;
            while (offset < len) {
                uint64_t vaddr = vstart + offset;
                uint16_t current_route = (uint16_t)(vaddr & LENS_MASK);
                uint64_t current_vblock = 0;

                for (int i = 0; i < 7; i++) {
                    uint32_t phys_idx = get_physical_block(current_vblock, true);
                    Value128& v = cache_pool_[phys_idx].entries[current_route];
                    uint16_t nxt = v.route();

                    if (nxt == FREE_ROUTE || nxt == 0) {
                        nxt = (uint16_t)(vblock_counter_++ & 0x3FF);
                        v.pack_route(nxt);
                        if (disk_) enqueue_dma(current_vblock, phys_idx, true);
                    }
                    
                    uint16_t static_next = (vaddr >> ((i + 1) * 8)) & LENS_MASK;
                    uint16_t resonance = (v.lo >> 54) & LENS_MASK;
                    current_route = static_next ^ resonance;
                    current_vblock = nxt;
                }

                uint32_t final_phys = get_physical_block(current_vblock, true);
                for (uint32_t entry = current_route; entry < ENTRIES_PER_BLOCK && offset < len; entry++) {
                    Value128 term;
                    kmemcpy(&term, data + offset, 16);
                    term.pack_route(HALT_ROUTE);
                    cache_pool_[final_phys].entries[entry] = term;
                    offset += 16;
                }
                if (disk_) enqueue_dma(current_vblock, final_phys, true);
            }
        }

        bool traverse_8_hops(const uint16_t path[8], Value128& result) const override {
            uint64_t current_vblock = 0;
            for (int i = 0; i < 7; i++) {
                // Const-safe block access
                uint32_t phys_idx = 0xFFFFFFFF;
                for (uint32_t k = 0; k < used_cache_; k++) { if (cached_vblock_[k] == current_vblock) { phys_idx = k; break; } }
                if (phys_idx == 0xFFFFFFFF) return false;

                uint16_t nxt = cache_pool_[phys_idx].entries[path[i] & LENS_MASK].route();
                if (nxt == FREE_ROUTE || nxt == 0) return false;
                current_vblock = nxt;
            }
            // Final hop
            uint32_t final_idx = 0xFFFFFFFF;
            for (uint32_t k = 0; k < used_cache_; k++) { if (cached_vblock_[k] == current_vblock) { final_idx = k; break; } }
            if (final_idx == 0xFFFFFFFF) return false;
            result = cache_pool_[final_idx].entries[path[7] & LENS_MASK];
            return true;
        }

        void materialize_8_hops(const uint16_t path[8], const Value128& terminal) override {
            uint64_t current_vblock = 0;
            for (int i = 0; i < 7; i++) {
                uint32_t phys_idx = get_physical_block(current_vblock, true);
                Value128& v = cache_pool_[phys_idx].entries[path[i] & LENS_MASK];
                uint16_t nxt = v.route();
                if (nxt == FREE_ROUTE || nxt == 0) {
                    nxt = (uint16_t)(vblock_counter_++ & 0x3FF);
                    v.pack_route(nxt);
                }
                current_vblock = nxt;
            }
            uint32_t final_phys = get_physical_block(current_vblock, true);
            cache_pool_[final_phys].entries[path[7] & LENS_MASK] = terminal;
        }

        size_t get_used_blocks() const override { return vblock_counter_; }
        Seed get_seed() const override { return seed_; }
        uint64_t get_mtv() const override { return 0; }
        
        size_t write_slot(uint16_t l1_slot, const uint8_t* data, size_t len) override {
            materialize_range((uint64_t)l1_slot << 32, data, len);
            return len;
        }
        
        size_t read_slot(uint16_t l1_slot, uint8_t* out, size_t max_len) const override {
            uint64_t vbase = (uint64_t)l1_slot << 32;
            for (size_t i = 0; i < max_len; i += 16) {
                // Note: Const-cast workaround for prototype interface
                Value128 v = const_cast<SubstrateManifold*>(this)->holographic_fetch(vbase + i);
                kmemcpy(out + i, &v, 16);
            }
            return max_len;
        }

        void clear_slot(uint16_t l1_slot) override { (void)l1_slot; }
        size_t total_l2_blocks() const override { return vblock_counter_; }
        bool save_image(const char*) override { return true; }
        bool load_image(const char*, Seed) override { return true; }
        size_t get_l2_count(uint16_t) const override { return vblock_counter_; }

        void pump_dma_queue() {
            while (dma_tail_ != dma_head_) {
                DMARequest req;
                req.vblock_idx = dma_ring_[dma_tail_].vblock_idx;
                req.phys_idx = dma_ring_[dma_tail_].phys_idx;
                req.is_write = dma_ring_[dma_tail_].is_write;
                
                if (req.is_write) {
                    disk_->write(2112 + (req.vblock_idx * 32), 32, (uint8_t*)&cache_pool_[req.phys_idx]);
                    cache_state_[req.phys_idx] = BlockState::RESIDENT;
                } else {
                    disk_->read(2112 + (req.vblock_idx * 32), 32, (uint8_t*)&cache_pool_[req.phys_idx]);
                    cache_state_[req.phys_idx] = BlockState::RESIDENT;
                }
                dma_tail_ = (dma_tail_ + 1) % 256;
            }
        }

        void inject_restored_atom(uint64_t virtual_block_idx, uint16_t atom_idx, Value128 atom) {
            uint32_t phys_idx = get_physical_block(virtual_block_idx, true);
            cache_pool_[phys_idx].entries[atom_idx] = atom;
            cache_state_[phys_idx] = BlockState::RESIDENT;
        }
    };
}

#endif
