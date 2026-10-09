#ifdef __APPLE__

#include "prx/libc/include/specifics/linux/ElfTypes.hpp"
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#include <mach-o/loader.h>
#include <cstring>

namespace {

constexpr std::uint64_t MetaVersion = 2;
constexpr std::size_t LoadBiasOffset = 0x08;
constexpr std::size_t HeaderCountOffset = 0x58;
constexpr std::size_t HeadersOffset = 0x60;

}

extern "C" int dl_iterate_phdr(int (*callback)(dl_phdr_info*, std::size_t, void*), void* data) {
    for (std::uint32_t index = 0; index < _dyld_image_count(); ++index) {
        const auto* header = reinterpret_cast<const mach_header_64*>(_dyld_get_image_header(index));
        if (header == nullptr || header->magic != MH_MAGIC_64 || (header->flags & MH_DYLIB_IN_CACHE) != 0) continue;
        unsigned long size = 0;
        const auto* meta = getsectiondata(header, "__APS5DATA", "__meta", &size);
        if (meta == nullptr || size < HeadersOffset) continue;
        std::uint64_t version;
        std::uint64_t loadBias;
        std::uint64_t count;
        std::memcpy(&version, meta, sizeof(version));
        std::memcpy(&loadBias, meta + LoadBiasOffset, sizeof(loadBias));
        std::memcpy(&count, meta + HeaderCountOffset, sizeof(count));
        if (version != MetaVersion || count > 0xffff || count > (size - HeadersOffset) / sizeof(Elf64_Phdr)) continue;
        dl_phdr_info info {};
        info.dlpi_addr = static_cast<std::uintptr_t>(loadBias);
        info.dlpi_name = header->filetype == MH_EXECUTE ? "" : _dyld_get_image_name(index);
        info.dlpi_phdr = reinterpret_cast<const Elf64_Phdr*>(meta + HeadersOffset);
        info.dlpi_phnum = static_cast<std::uint16_t>(count);
        if (const int result = callback(&info, sizeof(info), data); result != 0) return result;
    }
    return 0;
}

#endif
