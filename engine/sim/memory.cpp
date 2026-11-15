#include "sim/memory.hpp"

#include "core/log.hpp"

namespace ac3::sim {
void Memory::fault(u32 a) const {
    if (faults_++ == 0) {
        first_fault_ = a;
        log::warn("sim: access outside RAM and scratchpad at {:08X}", a);
    }
}
}
