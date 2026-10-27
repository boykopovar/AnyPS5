#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/specifics/linux/ElfTypes.hpp"
#include "tests/GuestUnwindModuleInfoFixture.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

struct GuestImageMeta {
    std::uint64_t version = 2;
    std::uint64_t loadBias = 0;
    std::uint8_t unused[0x48] = {};
    std::uint64_t count = 0;
    Elf64_Phdr headers[2] = {};
};
static_assert(offsetof(GuestImageMeta, count) == 0x58 && offsetof(GuestImageMeta, headers) == 0x60);

[[gnu::section("__APS5DATA,__meta"), gnu::used]] static GuestImageMeta meta;
static UnwindFixture fixture;

static void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

static std::uint64_t Address(const void* pointer) {
    return reinterpret_cast<std::uint64_t>(pointer);
}

static void Describe(bool withHeader, std::uint64_t loadSize) {
    meta.count = withHeader ? 2 : 1;
    meta.headers[0] = {};
    meta.headers[0].p_type = PT_LOAD;
    meta.headers[0].p_vaddr = Address(&fixture);
    meta.headers[0].p_memsz = loadSize;
    meta.headers[1] = {};
    meta.headers[1].p_type = PT_GNU_EH_FRAME;
    meta.headers[1].p_vaddr = Address(&fixture.header);
    meta.headers[1].p_memsz = sizeof(fixture.header);
}

static ModuleInfoForUnwind Query() {
    ModuleInfoForUnwind info{};
    Require(sceKernelGetModuleInfoForUnwind(Address(&fixture.frames), 0, &info) == 0, "unwind query failed");
    Require(info.st_size == sizeof(ModuleInfoForUnwind), "unwind info has the wrong size");
    Require(info.seg0_addr == Address(&fixture) && info.seg0_size == sizeof(fixture), "unwind segment is not the fixture's load segment");
    return info;
}

static void RequireTables(std::uint64_t size) {
    const auto info = Query();
    Require(info.eh_frame_hdr_addr == Address(&fixture.header), "wrong EH header address");
    Require(info.eh_frame_addr == Address(&fixture.frames), "wrong EH frame address");
    Require(info.eh_frame_size == size, "EH frame size must include the four-byte terminator");
}

static bool QueryThrows() {
    ModuleInfoForUnwind info{};
    try {
        sceKernelGetModuleInfoForUnwind(Address(&fixture.frames), 0, &info);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int main() {
    try {
        fixture.header.frames = static_cast<std::int32_t>(Address(&fixture.frames) - (Address(&fixture.header) + 4));
        Describe(true, sizeof(fixture));
        RequireTables(4 + 12 + 4);
        fixture.frames.terminator = 12;
        fixture.frames.spare[0] = 1;
        RequireTables(4 + 12 + 4 + 12 + 4);
        fixture.frames.terminator = 0;
        Describe(false, sizeof(fixture));
        const auto bare = Query();
        Require(bare.eh_frame_hdr_addr == 0 && bare.eh_frame_addr == 0 && bare.eh_frame_size == 0, "an image without PT_GNU_EH_FRAME reported tables");
        fixture.frames.terminator = 12;
        Describe(true, offsetof(UnwindFixture, frames) + offsetof(UnwindFixtureFrames, spareTerminator));
        Require(QueryThrows(), "a table whose terminator lies outside the image was accepted");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
