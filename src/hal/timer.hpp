#ifndef EOS_TIMER_HPP
#define EOS_TIMER_HPP

#include <stdint.h>

namespace EmergenceOS {
    namespace Timer {
        /**
         * @brief Initializes the PIT (Programmable Interval Timer) to a specific frequency.
         * This creates the "Temporal Pulse" required for OS-based code synchronization.
         */
        inline void initialize(uint32_t frequency) {
            uint32_t divisor = 1193180 / frequency;

            // Command Word: Channel 0, Access Mode LSB/MSB, Operating Mode 3 (Square Wave), Binary Mode
            __asm__ __volatile__ ("outb %%al, $0x43" : : "a"((uint8_t)0x36));

            // Set divisor
            __asm__ __volatile__ ("outb %%al, $0x40" : : "a"((uint8_t)(divisor & 0xFF)));
            __asm__ __volatile__ ("outb %%al, $0x40" : : "a"((uint8_t)((divisor >> 8) & 0xFF)));
        }
    }
}

#endif // EOS_TIMER_HPP
