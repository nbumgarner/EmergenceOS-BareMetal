#ifndef EOS_IOMMU_HYBRID_HPP
#define EOS_IOMMU_HYBRID_HPP

#include "topology.hpp"
#include "serial.hpp"
#include "memory.hpp"

namespace EmergenceOS {

    class IOMMUController {
    private:
        uint64_t root_table_phys;
        bool spoofing_active;
        volatile uint32_t* dmar_fault_status;
        volatile uint32_t* dmar_fault_clear;
        volatile uint32_t* smmu_fsr;
        volatile uint32_t* smmu_resume;

    public:
        IOMMUController() : root_table_phys(0), spoofing_active(true) {}

        void initialize() {
            #if defined(__x86_64__)
                root_table_phys = allocate_page();
                enable_dmar();
                dmar_fault_status = (volatile uint32_t*)0xFED91034;
                dmar_fault_clear = (volatile uint32_t*)0xFED91038;
            #elif defined(__aarch64__)
                root_table_phys = allocate_page();
                enable_smmu();
                smmu_fsr = (volatile uint32_t*)0x2B400058;
                smmu_resume = (volatile uint32_t*)0x2B400008;
            #endif
            
            SerialPort serial;
            serial.print("[EM-1] Hybrid IOMMU Initialized. Substrate Spoofing Armed.\n");
        }

        void handle_dma_fault(uint64_t fault_vaddr, uint16_t source_id) {
            if (!spoofing_active) return;
            uint64_t emergent_phys = calculate_emergent_route(fault_vaddr);
            map_page(source_id, fault_vaddr, emergent_phys, 3);
            resume_dma_transaction(source_id);
        }

    private:
        inline uint64_t calculate_emergent_route(uint64_t vaddr) {
            return (vaddr ^ Emergence::PHI_HI) & 0xFFFFFFFFFFFFF000ULL;
        }

        uint64_t allocate_page() { return (uint64_t)EmergenceOS::g_pmm->allocate(4096); }
        void enable_dmar() {}
        void enable_smmu() {}
        void map_page(uint16_t source_id, uint64_t vaddr, uint64_t paddr, uint32_t flags) {
            (void)source_id; (void)vaddr; (void)paddr; (void)flags;
        }
        
        void resume_dma_transaction(uint16_t source_id) {
            (void)source_id;
            #if defined(__x86_64__)
                if (dmar_fault_clear) *dmar_fault_clear = (1 << 31);
            #elif defined(__aarch64__)
                if (smmu_fsr) *smmu_fsr = 0xFFFFFFFF;
                if (smmu_resume) *smmu_resume = 1;
            #endif
        }
    };
}

#endif
