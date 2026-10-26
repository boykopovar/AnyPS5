#include "prx/libSceSysmodule/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

namespace {

constexpr int kInvalidValue = static_cast<int>(0x805A1000);

void Require(bool value) { if (!value) std::abort(); }

void CheckInternalIdRejected(std::uint32_t id) {
    Require(sceSysmoduleIsLoadedInternal(id) == kInvalidValue);
    Require(sceSysmoduleLoadModuleInternal(id) == kInvalidValue);
    Require(sceSysmoduleIsLoadedInternal(id) == kInvalidValue);
    Require(sceSysmoduleUnloadModuleInternal(id) == kInvalidValue);
}

}

int main() {
    CheckInternalIdRejected(0x80ffff00);
    CheckInternalIdRejected(0x8000ffff);
    Require(sceSysmoduleIsLoadedInternal(0x00ffffff) == kInvalidValue);
}
