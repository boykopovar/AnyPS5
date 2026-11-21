#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_ATOMICMEMORY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_ATOMICMEMORY_HPP

#include <cstdint>
#include <span>

namespace AgcDriver::Pm4 {

struct AtomicMemoryCommand {
    std::uint64_t address;
    std::uint64_t source;
    std::uint64_t comparison;
    std::uint32_t operation;
    std::uint32_t bytes;
};

AtomicMemoryCommand DecodeAtomicMem(std::span<const std::uint32_t> packet);
void ExecuteAtomicMem(std::span<const std::uint32_t> packet);

}

#endif
