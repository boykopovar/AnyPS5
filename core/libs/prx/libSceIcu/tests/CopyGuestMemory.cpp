#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>
#include <set>
#include <source_location>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <unistd.h>
#endif

namespace {

constexpr std::size_t Page = 0x4000;
constexpr std::size_t MappingBytes = Page * 3;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU guest copy check failed at line %u\n", location.line());
    std::abort();
}

struct Mapping {
    std::int64_t physical = 0;
    char16_t* data = nullptr;

    Mapping() {
        Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, MappingBytes, 0, 0, &physical) == 0);
        void* address = nullptr;
        Require(sceKernelMapDirectMemory(&address, MappingBytes, 3, 0, physical, 0) == 0);
        data = static_cast<char16_t*>(address);
        Require(data != nullptr);
        Require(GuestArena::GuestArenaContains_nid_postfix(data, MappingBytes));
    }

    ~Mapping() {
        Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(data), MappingBytes) == 0);
        Require(sceKernelReleaseDirectMemory(physical, MappingBytes) == 0);
    }

    char16_t* At(std::size_t offset) { return data + offset / sizeof(char16_t); }

    void Fill() {
        Require(sceKernelMprotect(data, MappingBytes, 3) == 0);
        GuestArena::HostWrite access(data, MappingBytes);
        Require(access.Open());
        std::fill_n(data, MappingBytes / sizeof(char16_t), 0xa55a);
    }
};

struct Watch {
    Mapping& mapping;
    bool enabled = false;
    bool registered = false;
    std::size_t page = 0;

    explicit Watch(Mapping& value) : mapping(value) {
#ifdef _WIN32
        enabled = GuestArena::GuestArenaWriteWatched_nid_postfix();
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        page = info.dwPageSize;
#elif defined(__APPLE__)
        enabled = GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix();
        page = static_cast<std::size_t>(getpagesize());
        if (enabled && !GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(reinterpret_cast<std::uintptr_t>(mapping.data), MappingBytes)) {
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(mapping.data, MappingBytes);
            registered = true;
        }
#endif
        if (enabled) Require(page != 0);
    }

    ~Watch() {
        if (registered) Require(GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(mapping.data, MappingBytes));
    }

    std::set<std::uintptr_t> Collect() {
        std::set<std::uintptr_t> written;
        if (!enabled) return written;
#ifdef _WIN32
        std::array<void*, MappingBytes / 4096 + 2> pages{};
        auto count = pages.size();
        Require(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(mapping.data), MappingBytes, pages.data(), &count, true));
        for (std::size_t index = 0; index < count; ++index) written.insert(reinterpret_cast<std::uintptr_t>(pages[index]));
#elif defined(__APPLE__)
        struct Target { std::set<std::uintptr_t>* written; std::size_t page; } target{&written, page};
        Require(GuestWriteWatch::GuestWriteWatchCollect_nid_postfix(reinterpret_cast<std::uintptr_t>(mapping.data), MappingBytes,
            [](void* context, std::uintptr_t begin, std::uintptr_t end) {
                auto& target = *static_cast<Target*>(context);
                for (auto address = begin; address < end; address += target.page) target.written->insert(address);
            }, &target));
#endif
        return written;
    }

    void Expect(void* destination) {
        if (!enabled) return;
        const auto address = reinterpret_cast<std::uintptr_t>(destination);
        Require(Collect() == std::set<std::uintptr_t>{address - address % page});
        Require(Collect().empty());
    }
};

struct Writer {
    bool append;
    std::int32_t count;
    std::size_t units;
    char16_t* (*call)(char16_t*, const char16_t*, std::int32_t);
};

const std::array<Writer, 5> Writers{{
    {false, 0, 4, [](char16_t* out, const char16_t* in, std::int32_t) { return u_strcpy_nid_postfix(out, in); }},
    {false, 8, 4, [](char16_t* out, const char16_t* in, std::int32_t count) { return u_strncpy_nid_postfix(out, in, count); }},
    {true, 0, 4, [](char16_t* out, const char16_t* in, std::int32_t) { return u_strcat_nid_postfix(out, in); }},
    {true, 2, 3, [](char16_t* out, const char16_t* in, std::int32_t count) { return u_strncat_nid_postfix(out, in, count); }},
    {false, 4, 4, [](char16_t* out, const char16_t* in, std::int32_t count) { return u_memcpy_nid_postfix(out, in, count); }},
}};

void WritableOutputs(Mapping& mapping, Watch& watch) {
    const std::array<char16_t, 4> source{0xd800, 0xdc00, u'Q', 0};
    for (const auto& writer : Writers) {
        mapping.Fill();
        auto* destination = mapping.At(Page + 32);
        if (writer.append) { destination[0] = u'P'; destination[1] = 0; }
        watch.Collect();
        Require(writer.call(destination, source.data(), writer.count) == destination);
        auto* output = destination + (writer.append ? 1 : 0);
        for (std::size_t index = 0; index + 1 < writer.units; ++index) Require(output[index] == source[index]);
        Require(output[writer.units - 1] == 0);
        Require(destination[-1] == 0xa55a && output[writer.units] == 0xa55a);
        if (writer.append) Require(destination[0] == u'P');
        watch.Expect(output);
    }
}

void PreciseRanges(Mapping& mapping, Watch& watch) {
    mapping.Fill();
    auto* destination = mapping.At(2 * Page - sizeof(char16_t));
    Require(sceKernelMprotect(mapping.At(2 * Page), Page, 1) == 0);
    watch.Collect();
    Require(u_strncpy_nid_postfix(destination, u"", std::numeric_limits<std::int32_t>::max()) == destination);
    Require(*destination == 0 && destination[1] == 0xa55a && destination[-1] == 0xa55a);
    watch.Expect(destination);
    for (const bool bounded : {false, true}) {
        mapping.Fill();
        destination = mapping.At(Page - sizeof(char16_t));
        destination[0] = u'P'; destination[1] = 0;
        Require(sceKernelMprotect(mapping.data, Page, 1) == 0);
        watch.Collect();
        Require((bounded ? u_strncat_nid_postfix(destination, u"XY", 1) : u_strcat_nid_postfix(destination, u"X")) == destination);
        Require(destination[0] == u'P' && destination[1] == u'X' && destination[2] == 0 && destination[3] == 0xa55a);
        watch.Expect(destination + 1);
    }
}

void ReadOnly(Mapping& mapping, Watch& watch) {
    mapping.Fill();
    auto* destination = mapping.At(Page + 32);
    destination[0] = u'P'; destination[1] = 0;
    const std::vector<char16_t> before(mapping.data, mapping.data + MappingBytes / sizeof(char16_t));
    Require(sceKernelMprotect(mapping.data, MappingBytes, 1) == 0);
    watch.Collect();
    for (const auto count : {0, -1, std::numeric_limits<std::int32_t>::min()}) {
        Require(u_strncpy_nid_postfix(destination, nullptr, count) == destination);
        Require(u_strncat_nid_postfix(destination, nullptr, count) == destination);
        Require(u_memcpy_nid_postfix(destination, nullptr, count) == destination);
    }
#ifdef _WIN32
    for (const auto& writer : Writers) {
        bool rejected = false;
        try { writer.call(destination, u"ABC", writer.count); }
        catch (const std::exception&) { rejected = true; }
        Require(rejected);
    }
#endif
    Require(std::equal(before.begin(), before.end(), mapping.data));
    Require(watch.Collect().empty());
    Require(sceKernelMprotect(mapping.data, MappingBytes, 3) == 0);
}

} // namespace

int main() {
    Mapping mapping;
    Watch watch(mapping);
    WritableOutputs(mapping, watch);
    PreciseRanges(mapping, watch);
    ReadOnly(mapping, watch);
    std::printf("guest-mapped copy and readonly no-op checks passed; write-watch %s\n", watch.enabled ? "verified" : "unavailable on this host");
}
