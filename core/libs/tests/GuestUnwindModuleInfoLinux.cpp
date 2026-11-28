#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <link.h>
#include <stdexcept>
#include <string>

static void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

struct TableSearch {
    std::uintptr_t address;
    std::uintptr_t header = 0;
    std::uintptr_t frames = 0;
    std::uint64_t size = 0;
    bool found = false;
};

static std::uint64_t ReadableBytes(const dl_phdr_info& image, std::uintptr_t address) {
    for (std::uint16_t i = 0; i < image.dlpi_phnum; ++i) {
        const auto& segment = image.dlpi_phdr[i];
        const std::uintptr_t start = image.dlpi_addr + segment.p_vaddr;
        if (segment.p_type == PT_LOAD && (segment.p_flags & PF_R) != 0 && address >= start && address - start < segment.p_memsz)
            return segment.p_memsz - (address - start);
    }
    return 0;
}

static int FindTables(dl_phdr_info* image, std::size_t, void* data) {
    auto& search = *static_cast<TableSearch*>(data);
    bool contains = false;
    std::uintptr_t headerAddress = 0;
    for (std::uint16_t i = 0; i < image->dlpi_phnum; ++i) {
        const auto& segment = image->dlpi_phdr[i];
        const std::uintptr_t start = image->dlpi_addr + segment.p_vaddr;
        if (segment.p_type == PT_LOAD && search.address >= start && search.address - start < segment.p_memsz) contains = true;
        if (segment.p_type == PT_GNU_EH_FRAME) headerAddress = start;
    }
    if (!contains) return 0;
    search.found = true;
    search.header = headerAddress;
    if (headerAddress == 0) return 1;
    Require(ReadableBytes(*image, headerAddress) >= 8, "fixture EH header is outside readable memory");
    const auto* header = reinterpret_cast<const std::uint8_t*>(headerAddress);
    Require(header[0] == 1 && header[1] == 0x1b, "unexpected fixture EH header encoding");
    std::int32_t offset = 0;
    std::memcpy(&offset, header + 4, sizeof(offset));
    search.frames = headerAddress + 4 + static_cast<std::intptr_t>(offset);
    const auto* frames = reinterpret_cast<const std::uint8_t*>(search.frames);
    const auto available = ReadableBytes(*image, search.frames);
    std::uint64_t consumed = 0;
    for (;;) {
        Require(available - consumed >= 4, "fixture EH table has no mapped terminator");
        std::uint32_t length = 0;
        std::memcpy(&length, frames + consumed, sizeof(length));
        consumed += 4;
        if (length == 0) break;
        std::uint64_t payloadSize = length;
        if (length == 0xffffffffu) {
            Require(available - consumed >= 8, "fixture extended EH length is outside readable memory");
            std::memcpy(&payloadSize, frames + consumed, sizeof(payloadSize));
            consumed += 8;
        }
        Require(payloadSize <= available - consumed, "fixture EH record is outside readable memory");
        consumed += payloadSize;
    }
    Require(consumed > 4, "fixture EH table is empty");
    search.size = consumed;
    return 1;
}

static TableSearch Tables(const void* address) {
    TableSearch search{reinterpret_cast<std::uintptr_t>(address)};
    dl_iterate_phdr(FindTables, &search);
    Require(search.found, "fixture was not found in loaded images");
    return search;
}

static ModuleInfoForUnwind Query(const void* address) {
    ModuleInfoForUnwind info{};
    info.st_size = sizeof(ModuleInfoForUnwind);
    Require(sceKernelGetModuleInfoForUnwind(reinterpret_cast<std::uint64_t>(address), 1, &info) == 0, "unwind query failed");
    Require(info.st_size == sizeof(ModuleInfoForUnwind), "unwind info has the wrong size");
    Require(reinterpret_cast<std::uint64_t>(address) - info.seg0_addr < info.seg0_size, "unwind segment does not contain the queried address");
    return info;
}

static void RequireTables(const void* address) {
    const auto tables = Tables(address);
    Require(tables.header != 0, "fixture has no EH header");
    const auto info = Query(address);
    Require(info.eh_frame_hdr_addr == tables.header, "wrong EH header address");
    Require(info.eh_frame_addr == tables.frames, "wrong EH frame address");
    Require(info.eh_frame_size == tables.size, "EH frame size must include the four-byte terminator");
}

static void RequireNoTables(const void* address) {
    const auto info = Query(address);
    Require(info.eh_frame_hdr_addr == 0 && info.eh_frame_addr == 0 && info.eh_frame_size == 0, "unexpected unwind tables");
}

static bool AllBytes(const ModuleInfoForUnwind& info, std::uint8_t value, std::size_t from) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&info);
    for (std::size_t index = from; index < sizeof(info); ++index) {
        if (bytes[index] != value) return false;
    }
    return true;
}

static ModuleInfoForUnwind Filled(std::uint64_t size) {
    ModuleInfoForUnwind info;
    std::memset(&info, 0xab, sizeof(info));
    info.st_size = size;
    return info;
}

static std::string FailureText(std::uint64_t address, int flags, ModuleInfoForUnwind* info) {
    try {
        sceKernelGetModuleInfoForUnwind(address, flags, info);
    } catch (const std::exception& error) {
        return error.what();
    }
    return {};
}

static void CheckContract(const void* address) {
    const auto found = reinterpret_cast<std::uint64_t>(address);
    const auto expected = Query(address);
    for (const int flags : {0, 3, 4, 8, -1}) {
        auto info = Filled(sizeof(ModuleInfoForUnwind));
        Require(sceKernelGetModuleInfoForUnwind(found, flags, &info) == static_cast<int>(0x80020016), "flags other than 1 or 2 must return EINVAL");
        Require(AllBytes(info, 0, 0), "flags other than 1 or 2 must clear the structure");
    }
    Require(FailureText(found, 0, nullptr) == "sceKernelGetModuleInfoForUnwind: a null output with flags other than 1 or 2, which the console clears without checking", "a null output with bad flags must throw");
    Require(sceKernelGetModuleInfoForUnwind(found, 1, nullptr) == static_cast<int>(0x8002000e), "a null output must return EFAULT");
    for (const std::uint64_t size : {std::uint64_t{0}, std::uint64_t{0x12f}}) {
        auto info = Filled(size);
        Require(sceKernelGetModuleInfoForUnwind(found, 1, &info) == static_cast<int>(0x80020016), "a short structure must return EINVAL");
        Require(info.st_size == size && AllBytes(info, 0xab, sizeof(info.st_size)), "a short structure must be left alone");
    }
    auto wide = Filled(0x200);
    Require(sceKernelGetModuleInfoForUnwind(found, 1, &wide) == 0, "a larger structure must be answered");
    Require(wide.st_size == 0x200, "the caller's structure size must be kept");
    wide.st_size = expected.st_size;
    Require(std::memcmp(&wide, &expected, sizeof(wide)) == 0, "a larger structure must get the same answer");
    auto second = Filled(sizeof(ModuleInfoForUnwind));
    Require(FailureText(found, 2, &second) == "sceKernelGetModuleInfoForUnwind: flags 2, a search by the module's second address range not implemented", "flags 2 must throw");
    auto absent = Filled(sizeof(ModuleInfoForUnwind));
    Require(sceKernelGetModuleInfoForUnwind(0x10, 1, &absent) == static_cast<int>(0x80020003), "an unmapped address must return ESRCH");
    Require(AllBytes(absent, 0, 0), "ESRCH must clear the structure");
}

static void CheckModule(const char* path, bool hasHeader) {
    void* module = dlopen_nid_postfix(path, 2);
    Require(module != nullptr, "failed to load guest fixture");
    const void* add = dlsym_nid_postfix(module, "GuestModuleAdd_nid_postfix");
    Require(add != nullptr, "guest fixture export was not found");
    if (hasHeader) RequireTables(add);
    else {
        Require(Tables(add).header == 0, "no-header fixture unexpectedly has an EH header");
        RequireNoTables(add);
    }
    Require(dlclose_nid_postfix(module) == 0, "failed to close guest fixture");
}

int main(int argc, char** argv) {
    try {
        Require(argc == 3, "expected guest fixtures with and without an EH header");
        RequireTables(reinterpret_cast<const void*>(&Query));
        CheckContract(reinterpret_cast<const void*>(&Query));
        CheckModule(argv[1], true);
        CheckModule(argv[2], false);
        const auto* host = reinterpret_cast<const void*>(&sceKernelGetModuleInfoForUnwind);
        Require(Tables(host).header != 0, "host fixture has no EH tables to suppress");
        RequireNoTables(host);
        RequireNoTables(reinterpret_cast<const void*>(&dl_iterate_phdr));
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
