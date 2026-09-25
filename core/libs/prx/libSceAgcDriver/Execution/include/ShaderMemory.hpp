#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SHADERMEMORY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SHADERMEMORY_HPP

#include "Recompiler.hpp"
#include <array>
#include <bitset>
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <vector>

namespace AgcDriver {

// Records the guest words a shader's resource analysis reads, as the memory regions its recompile
// request carries. Guest memory is fetched a 4 KiB page at a time; the regions report exactly the
// dwords read, so cache keys built from them do not change with unrelated bytes nearby.
class ShaderMemory {
public:
    explicit ShaderMemory(std::span<const ShaderRecompiler::MemoryRegion> initial);
    // Returns what the capture resolved (plan, snapshot, specialization) for
    // ShaderRecompiler::Recompile(request, capture), which then skips its own materialization. One
    // result per call: the draw path captures several stages on one ShaderMemory.
    std::shared_ptr<const ShaderRecompiler::ResourceCapture> Capture(const ShaderRecompiler::RecompileRequest& request);
    [[nodiscard]] std::vector<ShaderRecompiler::MemoryRegion> Regions() const;

private:
    static constexpr std::size_t PageBytes = 4096;
    static constexpr std::size_t PageWords = PageBytes / sizeof(std::uint32_t);

    struct Page {
        std::array<std::uint32_t, PageWords> words{};
        std::bitset<PageWords> valid;
        std::bitset<PageWords> read;
    };

    static bool read(void* context, std::uint64_t address, std::uint32_t* value);
    Page& page(std::uint64_t base);

    // Regions given at construction (the registered shader's code and header), referenced as given:
    // the caller keeps them alive for as long as the capture is used.
    std::map<std::uint64_t, std::span<const std::byte>> initial;
    std::map<std::uint64_t, Page> pages;
};

}

#endif
