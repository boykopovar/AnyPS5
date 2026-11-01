#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "WtfStringAbi.hpp"

#include <cstdint>
#include <stdexcept>

namespace {

using NKWebKit::WtfAbi::ProcessPrivileges;
unsigned g_processPrivileges = 0;

}  // namespace

extern "C" {
ProcessPrivileges APS5_VABI _ZN3WTF13allPrivilegesEv() {
    return {7};
}

void APS5_VABI _ZN3WTF20setProcessPrivilegesENS_9OptionSetINS_16ProcessPrivilegeEEE(ProcessPrivileges privileges) {
    g_processPrivileges = privileges.storage;
}

bool APS5_VABI _ZN3WTF19hasProcessPrivilegeENS_16ProcessPrivilegeE(unsigned privilege) {
    return !!(g_processPrivileges & privilege);
}

void APS5_VABI _ZN3WTF19addProcessPrivilegeENS_16ProcessPrivilegeE(unsigned privilege) {
    g_processPrivileges |= privilege;
}

void APS5_VABI _ZN3WTF22removeProcessPrivilegeENS_16ProcessPrivilegeE(unsigned privilege) {
    g_processPrivileges &= ~privilege;
}

unsigned APS5_VABI _ZN3WTF31sixCharacterHashStringToIntegerEPKc(const char* data) {
    if (!data) throw std::invalid_argument("null WTF six-character hash");
    unsigned hash = 0;
    for (unsigned i = 0; i < 6; ++i) {
        unsigned c = static_cast<unsigned char>(data[i]);
        unsigned value;
        if (c >= 'A' && c <= 'Z') value = c - 'A';
        else if (c >= 'a' && c <= 'z') value = c - 'a' + 26;
        else if (c >= '0' && c <= '9') value = c - '0' + 52;
        else throw std::invalid_argument("invalid WTF six-character hash");
        hash = hash * 62 + value;
    }
    if (data[6]) throw std::invalid_argument("WTF hash must contain six characters");
    return hash;
}

bool APS5_VABI _ZN7WebCore21isBackForwardLoadTypeENS_13FrameLoadTypeE(std::uint8_t type) {
    if (type > 9) throw std::invalid_argument("invalid WebCore frame load type");
    return type >= 1 && type <= 3;
}

bool APS5_VABI _ZN7WebCore8isReloadENS_13FrameLoadTypeE(std::uint8_t type) {
    if (type > 9) throw std::invalid_argument("invalid WebCore frame load type");
    return type == 4 || type == 8 || type == 9;
}

}
