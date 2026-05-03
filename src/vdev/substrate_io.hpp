#ifndef EOS_SUBSTRATE_IO_HPP
#define EOS_SUBSTRATE_IO_HPP

#include <stdint.h>

namespace EmergenceOS {

    class SubstrateIO {
    public:
        virtual ~SubstrateIO() = default;
        virtual void initialize(void* console) = 0;
        virtual void read(uint64_t lba, uint32_t count, uint8_t* buffer) = 0;
        virtual void write(uint64_t lba, uint32_t count, const uint8_t* buffer) = 0;
    };
}

#endif
