#include "prx/libc/include/LegacySignals.hpp"
#include <array>
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

extern "C" bool SigvecMissingBackend_nid_no_patch(decltype(&sigvec_nid_postfix) first,
    decltype(&sigvec_nid_postfix) second, const void* core) {
#ifdef _WIN32
    const auto libc = GetModuleHandleW(L"libc.prx");
    if (libc == nullptr || reinterpret_cast<const void*>(GetProcAddress(libc, "LegacySigvec_nid_no_patch")) != core) return false;
#else
    if (reinterpret_cast<const void*>(&LegacySigvec_nid_no_patch) != core) return false;
#endif
    for (const auto function : std::array{first, second}) {
        bool threw = false;
        try {
            function(30, nullptr, nullptr);
        } catch (const std::runtime_error& error) {
            threw = std::strcmp(error.what(), "sigvec: libkernel has not registered sigaction") == 0;
        }
        if (!threw) return false;
    }
    return true;
}
