#pragma once

#ifdef _WIN32

#include "prx/libkernel/libkernel.hpp"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <vector>

namespace FreeTypeTest {

struct KernelApi {
    decltype(&sceKernelAllocateDirectMemory) allocate;
    decltype(&sceKernelMapDirectMemory) map;
    decltype(&sceKernelMprotect) protect;
    decltype(&sceKernelMunmap) unmap;
    decltype(&sceKernelReleaseDirectMemory) release;
};

class GuestOutput {
public:
    static constexpr std::size_t Page = 0x4000;
    static constexpr std::size_t Bytes = Page * 2;

    explicit GuestOutput(const KernelApi& api) : api(api) {
        Require(api.allocate(0, 0x7fffffffffll, Bytes, 0, 0, &physical) == 0);
        Require(api.map(&mapping, Bytes, 3, 0, physical, 0) == 0);
        Reset();
    }

    ~GuestOutput() {
        Require(api.unmap(reinterpret_cast<std::uint64_t>(mapping), Bytes) == 0);
        Require(api.release(physical, Bytes) == 0);
    }

    template <typename TValue>
    TValue* Record(std::size_t offset) const {
        return reinterpret_cast<TValue*>(static_cast<unsigned char*>(mapping) + offset);
    }

    void Reset() const {
        Require(api.protect(mapping, Bytes, 3) == 0);
        std::memset(mapping, 0xA5, Bytes);
    }

    void Protect(unsigned page, bool writable) const {
        Require(api.protect(Record<void>(page * Page), Page, writable ? 3 : 1) == 0);
        MEMORY_BASIC_INFORMATION information{};
        Require(VirtualQuery(Record<void>(page * Page), &information, sizeof(information)) == sizeof(information));
        Require(information.Protect == (writable ? PAGE_READWRITE : PAGE_READONLY));
    }

    void PermitNativeWrite(unsigned page) const {
        DWORD previous = 0;
        Require(VirtualProtect(Record<void>(page * Page), Page, PAGE_READWRITE, &previous) != 0);
    }

    std::vector<unsigned char> Snapshot() const {
        const auto* bytes = static_cast<const unsigned char*>(mapping);
        return {bytes, bytes + Bytes};
    }

    void Unchanged(const std::vector<unsigned char>& before) const {
        Require(before.size() == Bytes && std::memcmp(mapping, before.data(), Bytes) == 0);
    }

private:
    static void Require(bool value, std::source_location location = std::source_location::current()) {
        if (value) return;
        std::fprintf(stderr, "FreeType guest output fixture failed at line %u\n", location.line());
        std::abort();
    }

    const KernelApi api;
    std::int64_t physical = 0;
    void* mapping = nullptr;
};

}

#endif
