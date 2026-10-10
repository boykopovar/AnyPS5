#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <link.h>
#include <stdexcept>

extern "C" {
int APS5_VABI sceKernelGetModuleInfoForUnwind(uint64_t addr, int flags, ModuleInfoForUnwind* info);
void* APS5_VABI dlopen_nid_postfix(const char* path, int flags);
void* APS5_VABI dlsym_nid_postfix(void* handle, const char* name);
int APS5_VABI dlclose_nid_postfix(void* handle);
}

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
