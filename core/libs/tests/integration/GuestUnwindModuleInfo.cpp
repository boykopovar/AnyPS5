#include "SceTypes.hpp"
#include "tests/GuestUnwindModuleInfoFixture.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <windows.h>

extern "C" int APS5_VABI sceKernelGetModuleInfoForUnwind(uint64_t addr, int flags, ModuleInfoForUnwind* info);

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

class FixtureModule {
public:
    FixtureModule() : fixture(GetUnwindFixture()) {
        Require(fixture != nullptr, "fixture module returns its unwind data");
        Reset();
    }
    ~FixtureModule() { Reset(); }
    FixtureModule(const FixtureModule&) = delete;
    FixtureModule& operator=(const FixtureModule&) = delete;

    std::uint64_t Address() const { return reinterpret_cast<std::uint64_t>(fixture); }

    void RequireRejected(const std::string& message) const {
        RequireThrows<std::runtime_error>([this] {
            ModuleInfoForUnwind info{};
            sceKernelGetModuleInfoForUnwind(Address(), 0, &info);
        }, message);
    }

    UnwindFixture* fixture;

private:
    void Reset() {
        fixture->header.version = 1;
        fixture->header.framePointerEncoding = 0x1b;
        fixture->frames.cieLength = 12;
    }
};

const Case fixtureModule{"GetModuleInfoForUnwind_GuestModule_ReportsEhFrameTablesAndImageBounds", [] {
    const FixtureModule module;
    MEMORY_BASIC_INFORMATION memory{};
    Require(VirtualQuery(reinterpret_cast<LPCVOID>(module.Address()), &memory, sizeof(memory)) != 0, "query fixture memory");
    const auto base = reinterpret_cast<std::uint64_t>(memory.AllocationBase);
    Require(base != reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr)), "fixture lives outside the test executable");

    ModuleInfoForUnwind info{};
    RequireEqual(sceKernelGetModuleInfoForUnwind(module.Address(), 0, &info), 0, "query result");
    RequireEqual(info.st_size, sizeof(ModuleInfoForUnwind), "structure size");
    RequireEqual(info.eh_frame_hdr_addr, reinterpret_cast<std::uint64_t>(&module.fixture->header), "eh_frame_hdr address");
    RequireEqual(info.eh_frame_addr, reinterpret_cast<std::uint64_t>(&module.fixture->frames), "eh_frame address");
    RequireEqual(info.eh_frame_size, std::uint64_t{4 + 12}, "eh_frame size");
    RequireEqual(info.seg0_addr, base, "segment 0 address");
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    RequireEqual(info.seg0_size, std::uint64_t{nt->OptionalHeader.SizeOfImage}, "segment 0 size");
}};

const Case badVersion{"GetModuleInfoForUnwind_UnsupportedHeaderVersion_Throws", [] {
    const FixtureModule module;
    module.fixture->header.version = 2;
    module.RequireRejected("version 2");
}};

const Case badEncoding{"GetModuleInfoForUnwind_UnsupportedFramePointerEncoding_Throws", [] {
    const FixtureModule module;
    for (const std::uint8_t encoding : {0x3b, 0x9b, 0x0f, 0xff}) {
        module.fixture->header.framePointerEncoding = encoding;
        module.RequireRejected("frame pointer encoding " + std::to_string(encoding));
    }
}};

const Case extendedLength{"GetModuleInfoForUnwind_ExtendedCieLength_Throws", [] {
    const FixtureModule module;
    module.fixture->frames.cieLength = 0xffffffffu;
    module.RequireRejected("cie length 0xffffffff");
}};

const Case restored{"GetModuleInfoForUnwind_HeaderRestoredAfterRejection_Succeeds", [] {
    const FixtureModule module;
    module.fixture->frames.cieLength = 0xffffffffu;
    module.RequireRejected("cie length 0xffffffff");
    module.fixture->frames.cieLength = 12;
    ModuleInfoForUnwind info{};
    RequireEqual(sceKernelGetModuleInfoForUnwind(module.Address(), 0, &info), 0, "query after restoring");
}};

const Case hostModule{"GetModuleInfoForUnwind_HostModule_ReportsNoUnwindTables", [] {
    ModuleInfoForUnwind host{};
    RequireEqual(sceKernelGetModuleInfoForUnwind(reinterpret_cast<std::uint64_t>(&GetModuleHandleW), 0, &host), 0, "query result");
    RequireEqual(host.eh_frame_hdr_addr, std::uint64_t{0}, "eh_frame_hdr address");
    RequireEqual(host.eh_frame_addr, std::uint64_t{0}, "eh_frame address");
    RequireEqual(host.eh_frame_size, std::uint64_t{0}, "eh_frame size");
}};

} // namespace
