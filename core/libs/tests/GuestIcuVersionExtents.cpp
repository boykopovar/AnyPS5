#include "prx/libSceIcu/libSceIcu.hpp"

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

struct Version { const char* input; std::array<std::uint8_t,4> bytes; const char* text; };
int main() {
    const Version versions[]{
        {"",{0,0,0,0},"0.0"},{"0",{0,0,0,0},"0.0"},{"1",{1,0,0,0},"1.0"},
        {"1.2.3.4",{1,2,3,4},"1.2.3.4"},{"255.0.255.1",{255,0,255,1},"255.0.255.1"},
        {"105.101.100.110",{105,101,100,110},"15.11.10.110"},
        {"  +1.2.3.4",{1,2,3,4},"1.2.3.4"},{"1.2.3.4.567890123456789",{1,2,3,4},"1.2.3.4"}};
    std::size_t cases = 0;
    for (const auto& version : versions) {
        const auto length = std::strlen(version.input);
        Guarded source(length+1), wide((length+1)*2), destination(4), text(20);
        auto* input = reinterpret_cast<char*>(source.End(length+1));
        std::memcpy(input,version.input,length+1);
        auto* units = reinterpret_cast<char16_t*>(wide.End((length+1)*2));
        for (std::size_t i = 0; i <= length; ++i) std::construct_at(units+i,static_cast<char16_t>(version.input[i]));
        source.Protect(true); wide.Protect(true);
        for (bool utf16 : {false,true}) {
            destination.Reset(); text.Reset();
            auto* bytes = destination.End(4);
            if (utf16) u_versionFromUString_nid_postfix(bytes,units);
            else u_versionFromString_nid_postfix(bytes,input);
            Require(std::equal(version.bytes.begin(),version.bytes.end(),bytes));
            auto* output = reinterpret_cast<char*>(text.End(20));
            u_versionToString_nid_postfix(bytes,output);
            Require(std::string(output) == version.text);
            Require(std::memcmp(input,version.input,length+1) == 0);
            source.Prefix(input); wide.Prefix(units); destination.Prefix(bytes); text.Prefix(output);
            ++cases;
        }
    }
    std::printf("%zu guarded ICU version conversions passed\n", cases);
}
