#ifndef CORE_LIBS_PRX_LIBKERNEL_APR_APRCOMMANDBUFFER_HPP
#define CORE_LIBS_PRX_LIBKERNEL_APR_APRCOMMANDBUFFER_HPP

#include <cstddef>
#include <cstdint>

// Shared between libSceAmpr (which records commands) and libkernel (which executes them).
// The guest treats both the command buffer object and its memory as opaque, so the encoding is ours.
namespace Apr {

enum class BufferType : std::uint32_t {
    Generic = 0,
    Apr = 1,
};

struct CommandBufferObject {
    std::uint8_t* base;
    std::uint32_t size;
    std::uint32_t offset;
    std::uint32_t numCommands;
    BufferType type;
};
static_assert(sizeof(CommandBufferObject) == 0x18, "guest reserves 0x18 bytes for sce::Ampr::CommandBuffer");

enum class Opcode : std::uint32_t {
    Nop = 0,
    ReadFile = 1,
    WriteAddress = 2,
};

struct CommandHeader {
    Opcode opcode;
    std::uint32_t bytes;
};

struct ReadFileCommand {
    CommandHeader header;
    std::uint32_t fileId;
    std::uint32_t reserved;
    std::uint64_t destination;
    std::uint64_t size;
    std::uint64_t offset;
};

struct WriteAddressCommand {
    CommandHeader header;
    std::uint64_t address;
    std::uint64_t value;
    std::uint32_t flags;
    std::uint32_t reserved;
};

}

#endif
