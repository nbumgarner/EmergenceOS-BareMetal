#ifndef EOS_TVM_HPP
#define EOS_TVM_HPP

#include "topology.hpp"
#include "memory.hpp"
#include "serial.hpp"
#include "substrate_service.hpp"

#if defined(__x86_64__)
#include "vmx.hpp"
#endif

namespace EmergenceOS {

    struct GuestContext {
        uint64_t gprs[32];
        uint64_t rip;
        uint64_t rsp;
        uint32_t exit_reason;
        uint32_t exit_qual;
    } __attribute__((packed));

    typedef void (*TopologicalExitHandler)(GuestContext*, Emergence::Topology&);

    class TopologicalVM {
    private:
        Emergence::Topology& substrate_;
        SubstrateService* preemption_service_;
        GuestContext context_;
        uint64_t vmexits_;
        bool is_active_;

        static constexpr uint64_t HYPERVISOR_DISPATCH_ROOT = 0x8000000000ULL;

        inline void init_hardware() {
            #if defined(__x86_64__)
                VMX::enable_vmx();
            #elif defined(__aarch64__)
                uint64_t hcr = (1ULL << 31) | (1ULL << 0);
                __asm__ __volatile__("msr hcr_el2, %0" : : "r"(hcr));
            #endif
        }

    public:
        TopologicalVM(Emergence::Topology& topo, SubstrateService* srv = nullptr)
            : substrate_(topo), preemption_service_(srv), vmexits_(0), is_active_(false) {
            kmemset(&context_, 0, sizeof(GuestContext));
            init_hardware();
        }

        void load_guest(const uint8_t* image, size_t size, uint64_t entry_point) {
            substrate_.materialize_range(0x1000, image, size);
            context_.rip = entry_point;
            is_active_ = true;
        }

        void run() {
            while (is_active_) {
                #if defined(__x86_64__)
                    extern "C" void vmx_enter_guest(GuestContext* ctx);
                    vmx_enter_guest(&context_);

                    // Check for HLT
                    if (context_.exit_reason == 12 && preemption_service_) {
                        preemption_service_->utilize_idle_cycles(128);
                        continue;
                    }
                #elif defined(__aarch64__)
                    extern "C" void el2_enter_guest(GuestContext* ctx);
                    el2_enter_guest(&context_);

                    // Check for WFI (ESR_EL2 encoding)
                    if ((context_.exit_reason >> 26) == 0x01 && preemption_service_) {
                        preemption_service_->utilize_idle_cycles(128);
                        continue;
                    }
                #endif

                vmexits_++;
                uint64_t dispatch_vaddr = HYPERVISOR_DISPATCH_ROOT | ((uint64_t)context_.exit_reason << 10);
                
                Emergence::Value128 handler_state = substrate_.holographic_fetch(dispatch_vaddr);
                
                if (handler_state.route() == Emergence::HALT_ROUTE || handler_state.route() == Emergence::ERROR_ROUTE) {
                    is_active_ = false;
                    break;
                }

                TopologicalExitHandler handler_func = (TopologicalExitHandler)(handler_state.lo);
                if (handler_func) {
                    handler_func(&context_, substrate_);
                } else {
                    is_active_ = false;
                }
            }
        }
    };
}

#endif
