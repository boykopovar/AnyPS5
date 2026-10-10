#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/specifics/linux/ElfTypes.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstdint>
#include <initializer_list>
#include <sstream>
#include <string>

extern "C" {
int APS5_VABI __elf_phdr_match_addr_nid_postfix(dl_phdr_info*, void*);
}

namespace {

using Testing::Case;
using Testing::RequireEqual;

struct Probe {
    std::uintptr_t address;
    int expected;
};

class Image {
public:
    Image()
        : headers{{
              {PT_LOAD, PF_W, 0, 0x1000, 0, 0x1000, 0x1000, 0x1000},
              {PT_GNU_EH_FRAME, PF_X, 0, 0x2000, 0, 0x1000, 0x1000, 4},
              {PT_LOAD, PF_X, 0, 0x3000, 0, 0x1000, 0x1000, 0x1000},
              {PT_LOAD, PF_X | PF_W, 0, 0x8000, 0, 0x100, 0x100, 0x1000},
          }},
          info{0x400000, "guest", headers.data(), static_cast<std::uint16_t>(headers.size())} {}

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    void ClearHeaders() { info.dlpi_phnum = 0; }

    void RequireMatches(std::initializer_list<Probe> probes) {
        for (const auto& probe : probes) {
            RequireEqual(__elf_phdr_match_addr_nid_postfix(&info, reinterpret_cast<void*>(probe.address)), probe.expected,
                         "address " + Hex(probe.address));
        }
    }

private:
    static std::string Hex(std::uintptr_t value) {
        std::ostringstream stream;
        stream << "0x" << std::hex << value;
        return stream.str();
    }

    std::array<Elf64_Phdr, 4> headers;
    dl_phdr_info info;
};

const Case executableSegment{"ElfPhdrMatchAddr_AddressInExecutableLoad_Matches", [] {
    Image image;
    image.RequireMatches({{0x403800, 1}});
}};

const Case nonExecutableSegments{"ElfPhdrMatchAddr_AddressInWritableLoadOrEhFrame_DoesNotMatch", [] {
    Image image;
    image.RequireMatches({{0x401800, 0}, {0x402800, 0}});
}};

const Case segmentBounds{"ElfPhdrMatchAddr_ExecutableLoadBoundaries_MatchOnlyInside", [] {
    Image image;
    image.RequireMatches({{0x403000, 1}, {0x402fff, 0}, {0x404000 - 9, 1}, {0x404000 - 8, 0}, {0x404000, 0}});
}};

const Case outsideSegments{"ElfPhdrMatchAddr_AddressOutsideLoadsOrUnrelocated_DoesNotMatch", [] {
    Image image;
    image.RequireMatches({{0x400800, 0}, {0x3800, 0}});
}};

const Case writableExecutable{"ElfPhdrMatchAddr_AddressInWritableExecutableLoad_Matches", [] {
    Image image;
    image.RequireMatches({{0x408010, 1}});
}};

const Case noHeaders{"ElfPhdrMatchAddr_NoProgramHeaders_DoesNotMatch", [] {
    Image image;
    image.ClearHeaders();
    image.RequireMatches({{0x403800, 0}});
}};

} // namespace
