#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstring>

namespace {

constexpr char TerminalPath[] = "/dev/tty";
constexpr std::size_t MaximumTerminalPath = 1024;
constexpr int GuestEfault = 14;

}

extern "C" char* APS5_VABI ctermid_nid_postfix(char* s) {
    static char buffer[MaximumTerminalPath];
    char* destination = s == nullptr ? buffer : s;
    if (s == nullptr) {
        std::memcpy(destination, TerminalPath, sizeof(TerminalPath));
        return destination;
    }
    if (!GuestCopyOut_nid_no_patch(destination, TerminalPath, sizeof(TerminalPath))) {
        *__error_nid_postfix() = GuestEfault;
        return nullptr;
    }
    return destination;
}
