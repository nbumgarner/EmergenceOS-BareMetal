#ifndef EMERGENCE_DYNAMIC_ALLOCATOR_HPP
#define EMERGENCE_DYNAMIC_ALLOCATOR_HPP

#include <stdint.h>
#include "serial.hpp"

namespace EmergenceOS {

    enum class AluMode {
        UNUSED,
        AVAILABLE,
        FORCE,
        INVISIBLE
    };

    struct CpuCore {
        uint8_t id;
        AluMode topological_mode;
        uint64_t ram_allocated_mb;
        bool is_isolated;
        const char* current_task;
    };

    class DynamicResourceAllocator {
    private:
        CpuCore cores[2];
        uint64_t total_ram_mb;
        uint32_t active_nodes_;

    public:
        DynamicResourceAllocator(uint64_t total_ram) : total_ram_mb(total_ram), active_nodes_(1) {
            // Default init
            cores[0] = {0, AluMode::AVAILABLE, 0, false, "System"};
            cores[1] = {1, AluMode::AVAILABLE, 0, false, "System"};
        }

        void isolate_core(uint8_t core_id, const char* task, AluMode mode, uint64_t ram_allocation, UnifiedConsole& console) {
            if (core_id > 1) return;
            cores[core_id].is_isolated = true;
            cores[core_id].topological_mode = mode;
            cores[core_id].ram_allocated_mb = ram_allocation;
            cores[core_id].current_task = task;
            console.print("[EM-1] RESOURCE: Core ");
            console.print_hex(core_id);
            console.print(" Isolated -> ");
            console.print(task);
            console.print("\n");
        }

        void optimize_holograph(Emergence::SubstrateManifold* manifold) {
            if (manifold) {
                manifold->pump_dma_queue();
            }
        }

        void set_active_cores(uint32_t num_cores) {
            if (num_cores > 2) num_cores = 2;
            for (uint32_t i=0; i<2; i++) {
                cores[i].topological_mode = (i < num_cores) ? AluMode::AVAILABLE : AluMode::UNUSED;
            }
        }

        uint32_t get_active_cores() const {
            uint32_t count = 0;
            for (uint32_t i=0; i<2; i++) if (cores[i].topological_mode != AluMode::UNUSED) count++;
            return count;
        }

        void fold_manifold(int layout_type) {
            (void)layout_type;
            active_nodes_++;
        }
        
        uint32_t get_active_nodes() const { return active_nodes_; }
    };
}

#endif
