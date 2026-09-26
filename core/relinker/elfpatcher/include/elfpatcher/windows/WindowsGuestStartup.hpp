#ifndef ELFPATCHER_WINDOWSGUESTSTARTUP_HPP
#define ELFPATCHER_WINDOWSGUESTSTARTUP_HPP

#include <domain/GuestRuntime.hpp>
#include <elfpatcher/windows/WindowsStubEmitter.hpp>

namespace Elfpatcher::Windows {

class WindowsGuestStartup {
public:
    void WriteImport(WindowsStubEmitter& code, const PeImport& import, std::uint32_t handles) const;
    void Initialize(WindowsStubEmitter& code, const std::vector<Domain::GuestRuntime>& modules, std::uint32_t handles) const;
    void Finalize(WindowsStubEmitter& code, const std::vector<Domain::GuestRuntime>& modules, std::uint32_t handles, std::uint32_t finished) const;
    std::uint32_t EmitTlsResolver(WindowsStubEmitter& code) const;
private:
    void callLifecycle(WindowsStubEmitter& code, std::uint32_t handle, std::uint32_t rva) const;
};

}

#endif
