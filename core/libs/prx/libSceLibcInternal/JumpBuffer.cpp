#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

[[noreturn]] void APS5_VABI __longjmp_nid_postfix(std::uint64_t* buffer, int value) {
    _longjmp_nid_postfix(buffer, value);
}

}
