#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PM4OPCODES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PM4OPCODES_HPP

#include <cstdint>
#include <string_view>

namespace AgcDriver::Pm4 {

struct Opcode {
    std::uint32_t value;
    std::string_view name;
};

inline constexpr Opcode Opcodes[] = {
    {0x10, "NOP"},
    {0x11, "SET_BASE"},
    {0x12, "CLEAR_STATE"},
    {0x13, "INDEX_BUFFER_SIZE"},
    {0x15, "DISPATCH_DIRECT"},
    {0x16, "DISPATCH_INDIRECT"},
    {0x20, "SET_PREDICATION"},
    {0x22, "COND_EXEC"},
    {0x24, "DRAW_INDIRECT"},
    {0x25, "DRAW_INDEX_INDIRECT"},
    {0x26, "INDEX_BASE"},
    {0x27, "DRAW_INDEX_2"},
    {0x28, "CONTEXT_CONTROL"},
    {0x2A, "INDEX_TYPE"},
    {0x2C, "DRAW_INDIRECT_MULTI"},
    {0x2D, "DRAW_INDEX_AUTO"},
    {0x2F, "NUM_INSTANCES"},
    {0x33, "INDIRECT_BUFFER_CNST"},
    {0x35, "DRAW_INDEX_OFFSET_2"},
    {0x37, "WRITE_DATA"},
    {0x39, "MEM_SEMAPHORE"},
    {0x38, "DRAW_INDEX_INDIRECT_MULTI"},
    {0x3A, "DISPATCH_DRAW_PREAMBLE"},
    {0x3C, "WAIT_REG_MEM"},
    {0x3F, "INDIRECT_BUFFER"},
    {0x40, "COPY_DATA"},
    {0x41, "CP_DMA"},
    {0x42, "PFP_SYNC_ME"},
    {0x43, "SURFACE_SYNC"},
    {0x45, "COND_WRITE"},
    {0x46, "EVENT_WRITE"},
    {0x47, "EVENT_WRITE_EOP"},
    {0x48, "EVENT_WRITE_EOS"},
    {0x49, "RELEASE_MEM"},
    {0x50, "DMA_DATA"},
    {0x58, "ACQUIRE_MEM"},
    {0x59, "REWIND"},
    {0x63, "SET_SH_REG_INDIRECT"},
    {0x64, "SET_UCONFIG_REG_INDIRECT"},
    {0x68, "SET_CONFIG_REG"},
    {0x69, "SET_CONTEXT_REG"},
    {0x76, "SET_SH_REG"},
    {0x78, "SET_QUEUE_REG"},
    {0x79, "SET_UCONFIG_REG"},
    {0x7A, "SET_UCONFIG_REG_INDEX"},
    {0x81, "WRITE_CONST_RAM"},
    {0x83, "DUMP_CONST_RAM"},
    {0x84, "INCREMENT_CE_COUNTER"},
    {0x85, "INCREMENT_DE_COUNTER"},
    {0x86, "WAIT_ON_CE_COUNTER"},
    {0x88, "WAIT_ON_DE_COUNTER_DIFF"},
    {0x8D, "DISPATCH_DRAW"},
    {0x8E, "GET_LOD_STATS"},
    {0x93, "WAIT_REG_MEM_64"},
    {0x9F, "SET_CONTEXT_REG_INDIRECT"},
};

}

#endif
