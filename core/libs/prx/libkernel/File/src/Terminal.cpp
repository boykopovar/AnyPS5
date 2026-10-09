#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include <cstdint>
#include <stdexcept>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

extern "C" int* APS5_VABI __error_nid_postfix();
#ifdef _WIN32
extern "C" _invalid_parameter_handler _set_thread_local_invalid_parameter_handler(_invalid_parameter_handler);
#endif

namespace {
constexpr int BadDescriptor = 9;
constexpr int BadAddress = 14;
constexpr int InvalidArgument = 22;
constexpr int NotTerminal = 25;
constexpr int OperationNotSupported = 45;
constexpr int AddressNotAvailable = 49;
constexpr int SetNow = 0;
constexpr int SetDrain = 1;
constexpr int SetFlush = 2;
constexpr int SetSoft = 0x10;
constexpr int InternetFamily = 2;

int Fail(int error) {
    *__error_nid_postfix() = error;
    return -1;
}

#ifdef _WIN32
void IgnoreInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, std::uintptr_t) {}
bool HostOpen(int descriptor) {
    const auto previous = _set_thread_local_invalid_parameter_handler(IgnoreInvalidParameter);
    const auto handle = ::_get_osfhandle(descriptor);
    _set_thread_local_invalid_parameter_handler(previous);
    return reinterpret_cast<HANDLE>(handle) != INVALID_HANDLE_VALUE;
}
#else
bool HostOpen(int descriptor) { return ::fcntl(descriptor, F_GETFD) != -1; }
#endif

int TerminalError(int descriptor) {
    if (descriptor >= GuestSockets::FirstDescriptor) {
        const int family = GuestSockets::Family(descriptor);
        if (family < 0) return BadDescriptor;
        return family == InternetFamily ? AddressNotAvailable : OperationNotSupported;
    }
    if (descriptor < 0 || !HostOpen(descriptor)) return BadDescriptor;
    return NotTerminal;
}
}

extern "C" {
int APS5_VABI tcgetattr_nid_postfix(int descriptor, void* attributes) {
    static_cast<void>(attributes);
    return Fail(TerminalError(descriptor));
}

int APS5_VABI tcsetattr_nid_postfix(int descriptor, int action, const void* attributes) {
    if ((action & SetSoft) != 0 && attributes == nullptr) throw std::invalid_argument("tcsetattr: attributes is null");
    const int mode = action & ~SetSoft;
    if (mode != SetNow && mode != SetDrain && mode != SetFlush) return Fail(InvalidArgument);
    if (attributes == nullptr) return Fail(BadAddress);
    return Fail(TerminalError(descriptor));
}
}
