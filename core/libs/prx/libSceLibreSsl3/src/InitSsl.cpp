#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/General.hpp"

#include <atomic>
#include <cstdint>

namespace {

constexpr std::uint64_t NoLoadConfig = 0x1;
constexpr std::uint64_t LoadConfig = 0x2;

std::atomic<bool> configured{false};

}

extern "C" int APS5_VABI OPENSSL_init_ssl_nid_postfix(std::uint64_t options, const void*) {
    if (options & NoLoadConfig) configured.store(true);
    if ((options & LoadConfig) && !configured.load())
        NotImplemented_nid_no_patch("OPENSSL_init_ssl with OPENSSL_INIT_LOAD_CONFIG before the configuration was settled");
    return 1;
}
