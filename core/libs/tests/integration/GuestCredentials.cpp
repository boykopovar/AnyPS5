#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI seteuid_nid_postfix(std::uint32_t);
int APS5_VABI setegid_nid_postfix(std::uint32_t);
int* APS5_VABI __error_nid_postfix();
}

namespace {

using Testing::Case;
using Testing::RequireEqual;
using Testing::RequireThrows;

const Case rootIds{"SetEffectiveIds_Root_SucceedsAndKeepsErrno", [] {
    *__error_nid_postfix() = 13;
    RequireEqual(seteuid_nid_postfix(0), 0, "seteuid(0)");
    RequireEqual(setegid_nid_postfix(0), 0, "setegid(0)");
    RequireEqual(*__error_nid_postfix(), 13, "errno untouched on success");
}};

const Case nonRootUid{"SetEffectiveUid_NonRootId_Throws", [] {
    for (const std::uint32_t id : {1000u, 0xFFFFFFFFu}) {
        RequireThrows<std::runtime_error>([id] { seteuid_nid_postfix(id); }, "seteuid(" + std::to_string(id) + ")");
    }
}};

const Case nonRootGid{"SetEffectiveGid_NonRootId_Throws", [] {
    for (const std::uint32_t id : {1000u, 0xFFFFFFFFu}) {
        RequireThrows<std::runtime_error>([id] { setegid_nid_postfix(id); }, "setegid(" + std::to_string(id) + ")");
    }
}};

const Case rootAfterRejection{"SetEffectiveIds_RootAfterRejectedChange_StillSucceeds", [] {
    RequireThrows<std::runtime_error>([] { seteuid_nid_postfix(1000); }, "seteuid(1000)");
    RequireThrows<std::runtime_error>([] { setegid_nid_postfix(1000); }, "setegid(1000)");
    RequireEqual(seteuid_nid_postfix(0), 0, "seteuid(0) after rejection");
    RequireEqual(setegid_nid_postfix(0), 0, "setegid(0) after rejection");
}};

} // namespace
