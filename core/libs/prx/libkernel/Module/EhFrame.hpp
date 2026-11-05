#ifndef CORE_LIBS_PRX_LIBKERNEL_MODULE_EHFRAME_HPP
#define CORE_LIBS_PRX_LIBKERNEL_MODULE_EHFRAME_HPP

#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace EhFrame {

struct Tables {
    std::uintptr_t header = 0;
    std::uint64_t headerSize = 0;
    std::uintptr_t frames = 0;
    std::uint64_t framesSize = 0;
};

[[noreturn]] inline void Fail(const char* caller, const char* reason) {
    throw std::runtime_error(std::string(caller) + ": " + reason);
}

inline std::uint64_t EncodedSize(std::uint8_t encoding, const char* caller) {
    switch (encoding & 0x0fu) {
    case 0x00: case 0x04: case 0x0c: return 8;
    case 0x02: case 0x0a: return 2;
    case 0x03: case 0x0b: return 4;
    default: Fail(caller, "unsupported eh_frame_hdr value format");
    }
}

template<typename TContains>
std::uintptr_t EncodedValue(const TContains& contains, std::uintptr_t field, std::uint8_t encoding, const char* caller) {
    if ((encoding & 0x80u) != 0) Fail(caller, "unsupported indirect eh_frame_hdr encoding");
    const auto application = encoding & 0x70u;
    if (application != 0x00 && application != 0x10) Fail(caller, "eh_frame_hdr encoding other than absptr or pcrel");
    const auto size = EncodedSize(encoding, caller);
    if (!contains(field, size)) Fail(caller, "eh_frame_hdr outside the image");
    const auto* bytes = reinterpret_cast<const void*>(field);
    std::int64_t value = 0;
    switch (encoding & 0x0fu) {
    case 0x02: { std::uint16_t data; std::memcpy(&data, bytes, sizeof(data)); value = data; break; }
    case 0x0a: { std::int16_t data; std::memcpy(&data, bytes, sizeof(data)); value = data; break; }
    case 0x03: { std::uint32_t data; std::memcpy(&data, bytes, sizeof(data)); value = data; break; }
    case 0x0b: { std::int32_t data; std::memcpy(&data, bytes, sizeof(data)); value = data; break; }
    default: std::memcpy(&value, bytes, sizeof(value)); break;
    }
    return application == 0x10 ? field + static_cast<std::uintptr_t>(value) : static_cast<std::uintptr_t>(value);
}

template<typename TContains>
std::uint64_t HeaderSize(const TContains& contains, std::uintptr_t header, const char* caller) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(header);
    std::uint64_t size = 4 + EncodedSize(bytes[1], caller);
    if (bytes[2] == 0xff) return size;
    const auto count = static_cast<std::uint64_t>(EncodedValue(contains, header + size, bytes[2] & 0x0fu, caller));
    size += EncodedSize(bytes[2], caller);
    if (bytes[3] == 0xff) return size;
    const auto entry = 2 * EncodedSize(bytes[3], caller);
    if (count > (std::numeric_limits<std::uint64_t>::max() - size) / entry || !contains(header, size + count * entry))
        Fail(caller, "eh_frame_hdr table outside the image");
    return size + count * entry;
}

template<typename TContains>
std::uint64_t FramesSize(const TContains& contains, std::uintptr_t frames, const char* caller) {
    std::uintptr_t position = frames;
    for (;;) {
        if (!contains(position, 4)) Fail(caller, "eh_frame has no terminator inside the image");
        std::uint32_t length;
        std::memcpy(&length, reinterpret_cast<const void*>(position), sizeof(length));
        if (length == 0) return position - frames;
        if (length == 0xffffffffu) Fail(caller, "unsupported eh_frame record with a 64-bit length");
        if (!contains(position + 4, length)) Fail(caller, "eh_frame record outside the image");
        position += 4 + length;
    }
}

template<typename TContains>
Tables ReadTables(const TContains& contains, std::uintptr_t header, const char* caller) {
    if (!contains(header, 4)) Fail(caller, "eh_frame_hdr outside the image");
    if (reinterpret_cast<const std::uint8_t*>(header)[0] != 1) Fail(caller, "unsupported eh_frame_hdr version");
    Tables tables;
    tables.header = header;
    tables.frames = EncodedValue(contains, header + 4, reinterpret_cast<const std::uint8_t*>(header)[1], caller);
    tables.headerSize = HeaderSize(contains, header, caller);
    tables.framesSize = FramesSize(contains, tables.frames, caller);
    return tables;
}

#ifdef _WIN32
inline bool ReadEhmeta(std::uintptr_t base, const IMAGE_NT_HEADERS64& nt, const char* caller, Tables& tables) {
    const std::uint64_t imageSize = nt.OptionalHeader.SizeOfImage;
    const auto contains = [&](std::uintptr_t begin, std::uint64_t size) { return begin >= base && begin - base <= imageSize && size <= imageSize - (begin - base); };
    const auto* sections = IMAGE_FIRST_SECTION(&nt);
    for (unsigned i = 0; i < nt.FileHeader.NumberOfSections; ++i) {
        if (std::strncmp(reinterpret_cast<const char*>(sections[i].Name), ".ehmeta", IMAGE_SIZEOF_SHORT_NAME) != 0) continue;
        const auto start = base + sections[i].VirtualAddress;
        if (!contains(start, 4)) Fail(caller, ".ehmeta outside the image");
        std::uint32_t headerRva;
        std::memcpy(&headerRva, reinterpret_cast<const void*>(start), sizeof(headerRva));
        tables = ReadTables(contains, base + headerRva, caller);
        return true;
    }
    return false;
}
#endif

}  // namespace EhFrame

#endif
