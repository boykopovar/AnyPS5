#include "prx/libSceCesCs/include/Api.hpp"
#include <bit>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <source_location>
#include <span>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "boundary check failed at line %u\n", location.line());
        std::abort();
    }
}

class Guarded {
    unsigned char* memory;
    std::size_t page;
    std::size_t writable;
public:
    explicit Guarded(std::size_t capacity = 1) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        page = info.dwPageSize;
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        page = static_cast<std::size_t>(size);
#endif
        writable = ((capacity + 128 + page - 1) / page) * page;
#ifdef _WIN32
        memory = static_cast<unsigned char*>(VirtualAlloc(nullptr, writable + page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
        Require(memory != nullptr);
#else
        memory = static_cast<unsigned char*>(mmap(nullptr, writable + page * 2, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED);
#endif
        Reset();
    }
    ~Guarded() {
#ifdef _WIN32
        Require(VirtualFree(memory, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(memory, writable + page * 2) == 0);
#endif
    }
    Guarded(const Guarded&) = delete;
    Guarded& operator=(const Guarded&) = delete;
    void Protect(bool readOnly) {
#ifdef _WIN32
        DWORD previous;
        Require(VirtualProtect(memory + page, writable, readOnly ? PAGE_READONLY : PAGE_READWRITE, &previous) != 0);
#else
        Require(mprotect(memory + page, writable, readOnly ? PROT_READ : PROT_READ | PROT_WRITE) == 0);
#endif
    }
    void Reset() {
        Protect(false);
        std::memset(memory + page, 0xa5, writable);
    }
    unsigned char* End(std::size_t count) {
        Require(count + 64 <= writable);
        return memory + page + writable - (std::max)(count, std::size_t{1});
    }
    void Prefix(const void* address) const {
        const auto* bytes = static_cast<const unsigned char*>(address);
        for (int index = 1; index <= 32; ++index) Require(bytes[-index] == 0xa5);
    }
};

using To16 = int (APS5_VABI*)(const std::uint8_t*,std::uint32_t,std::uint32_t*,std::uint16_t*,std::uint32_t,std::uint32_t*);
using From16 = int (APS5_VABI*)(const std::uint16_t*,std::uint32_t,std::uint32_t*,std::uint8_t*,std::uint32_t,std::uint32_t*);
struct Scalar { std::vector<unsigned char> utf8; std::vector<std::uint16_t> units; };
static const Scalar scalars[]{{{0},{0}},
{{1},{1}},
{{127},{127}},
{{194,128},{128}},
{{223,191},{2047}},
{{224,160,128},{2048}},
{{237,159,191},{55295}},
{{238,128,128},{57344}},
{{239,191,191},{65535}},
{{240,144,128,128},{55296,56320}},
{{240,159,152,128},{55357,56832}},
{{244,143,191,191},{56319,57343}}};
int main() {
    const To16 to16[]{sceCesUtf8ToUtf16,sceCesUtf8ToUtf16be,sceCesUtf8ToUtf16le};
    const From16 from16[]{sceCesUtf16ToUtf8,sceCesUtf16beToUtf8,sceCesUtf16leToUtf8};
    std::size_t cases = 0;
    for (const auto& scalar : scalars) {
        Guarded input(scalar.utf8.size());
        auto* source = input.End(scalar.utf8.size());
        std::copy(scalar.utf8.begin(), scalar.utf8.end(), source);
        input.Protect(true);
        for (unsigned variant = 0; variant < 3; ++variant) {
            Guarded output(scalar.units.size() * 2);
            auto* target = reinterpret_cast<std::uint16_t*>(output.End(scalar.units.size() * 2));
            std::uninitialized_fill_n(target, scalar.units.size(), std::uint16_t{});
            std::uint32_t consumed = 0, produced = 0;
            Require(to16[variant](source,scalar.utf8.size(),&consumed,target,scalar.units.size(),&produced) == 0);
            Require(consumed == scalar.utf8.size() && produced == scalar.units.size());
            const bool big = variant == 0 ? std::endian::native == std::endian::big : variant == 1;
            for (std::size_t i = 0; i < scalar.units.size(); ++i) {
                const auto value = scalar.units[i];
                const auto* bytes = reinterpret_cast<const unsigned char*>(target + i);
                Require(bytes[0] == (big ? value >> 8 : value & 255));
                Require(bytes[1] == (big ? value & 255 : value >> 8));
            }
            output.Protect(true);
            Guarded reverse(scalar.utf8.size());
            auto* bytes = reverse.End(scalar.utf8.size());
            consumed = produced = 0;
            Require(from16[variant](target,scalar.units.size(),&consumed,bytes,scalar.utf8.size(),&produced) == 0);
            Require(consumed == scalar.units.size() && produced == scalar.utf8.size());
            Require(std::equal(scalar.utf8.begin(), scalar.utf8.end(), bytes));
            Require(std::equal(scalar.utf8.begin(), scalar.utf8.end(), source));
            input.Prefix(source); output.Prefix(target); reverse.Prefix(bytes);
            for (std::size_t capacity = 0; capacity < scalar.units.size(); ++capacity) {
                Guarded shortOutput(4);
                auto* shortUnits = reinterpret_cast<std::uint16_t*>(shortOutput.End((std::max)(capacity,std::size_t{1}) * 2));
                std::uninitialized_fill_n(shortUnits,(std::max)(capacity,std::size_t{1}),std::uint16_t{0xa5a5});
                Require(to16[variant](source,scalar.utf8.size(),&consumed,shortUnits,capacity,&produced) != 0);
                shortOutput.Prefix(shortUnits);
                if (capacity == 0) Require(shortUnits[0] == 0xa5a5);
                ++cases;
            }
            ++cases;
        }
    }
    std::printf("%zu guarded Unicode conversion cases passed\n", cases);
}
