#include <elfpatcher/windows/WindowsGuestStartup.hpp>

namespace Elfpatcher::Windows {

void WindowsGuestStartup::WriteImport(WindowsStubEmitter& code, const PeImport& import, const std::uint32_t handles) const {
    if (import.TargetModule < 0) {
        code.Rip({0x48, 0x89, 0x05}, import.TargetRva);
        return;
    }
    code.Rip({0x48, 0x8b, 0x15}, CheckedRva(handles + static_cast<std::uint64_t>(import.TargetModule) * 8));
    code.Emit({0x41, 0xb8});
    code.U32(import.TargetRva);
    code.Emit({0x4a, 0x89, 0x04, 0x02});
}

void WindowsGuestStartup::callLifecycle(WindowsStubEmitter& code, const std::uint32_t handle, const std::uint32_t rva) const {
    if (rva == 0) return;
    code.Rip({0x48, 0x8b, 0x05}, handle);
    code.Emit({0xb9});
    code.U32(rva);
    code.Emit({0x48, 0x01, 0xc8, 0x31, 0xff, 0x31, 0xf6, 0x31, 0xd2, 0xff, 0xd0});
}

void WindowsGuestStartup::Initialize(WindowsStubEmitter& code, const std::vector<Domain::GuestRuntime>& modules, const std::uint32_t handles) const {
    for (std::size_t index = 0; index < modules.size(); ++index) callLifecycle(code, CheckedRva(handles + index * 8), modules[index].InitRva);
}

void WindowsGuestStartup::Finalize(WindowsStubEmitter& code, const std::vector<Domain::GuestRuntime>& modules, const std::uint32_t handles, const std::uint32_t finished) const {
    code.Emit({0xb8, 1, 0, 0, 0});
    code.Rip({0x87, 0x05}, finished);
    code.Emit({0x85, 0xc0});
    const auto skip = code.Branch({0x0f, 0x85});
    for (std::size_t index = modules.size(); index > 0; --index) callLifecycle(code, CheckedRva(handles + (index - 1) * 8), modules[index - 1].FiniRva);
    code.PatchBranch(skip, code.GetRva());
}

std::uint32_t WindowsGuestStartup::EmitTlsResolver(WindowsStubEmitter& code) const {
    const auto entry = code.GetRva();
    code.Emit({0x48, 0x8b, 0x07, 0x8b, 0x08, 0x65, 0x48, 0x8b, 0x04, 0x25, 0x58, 0, 0, 0, 0x48, 0x8b, 0x04, 0xc8, 0x48, 0x03, 0x47, 8, 0xc3});
    return entry;
}

}
