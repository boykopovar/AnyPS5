#include "prx/libSceIcu/include/DataMemory.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

void Check(bool ok, const char* condition) {
    if (!ok) {
        std::fprintf(stderr, "ICU data memory: %s\n", condition);
        std::abort();
    }
}

template<class F>
void ExpectUnreadable(F call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return;
    }
    Check(false, "expected unreadable input to throw");
}

}

int main() {
    Check(!udata_getRawMemory_nid_postfix(nullptr), "null raw memory");
    Check(!udata_getMemory_nid_postfix(nullptr), "null payload");
    Check(udata_getLength_nid_postfix(nullptr) == -1, "null length");
    alignas(8) std::array<std::uint8_t, 56> handle{};
    auto* data = reinterpret_cast<Icu::DataMemory*>(handle.data());
    Check(!udata_getRawMemory_nid_postfix(data), "missing header");
    Check(!udata_getMemory_nid_postfix(data), "missing payload");
    Check(udata_getLength_nid_postfix(data) == -1, "missing header length");

    std::array<std::uint8_t, 65536> bytes{};
    const void* header = bytes.data();
    std::memcpy(handle.data() + 8, &header, sizeof(header));
    for (const std::uint8_t flag : {0, 1, 2}) {
        for (const unsigned size : {0u, 1u, 32u, 255u, 256u, 0x1234u, 65535u}) {
            bytes[0] = static_cast<std::uint8_t>(flag ? size >> 8 : size);
            bytes[1] = static_cast<std::uint8_t>(flag ? size : size >> 8);
            bytes[8] = flag;
            for (const std::int32_t length : {std::numeric_limits<std::int32_t>::min(), -2, -1, 0, static_cast<int>(size), 65536, std::numeric_limits<std::int32_t>::max()}) {
                std::memcpy(handle.data() + 48, &length, sizeof(length));
                errno = EDOM;
                Check(udata_getRawMemory_nid_postfix(data) == header, "raw address");
                Check(udata_getMemory_nid_postfix(data) == bytes.data() + size, "payload address");
                Check(udata_getLength_nid_postfix(data) == (length < 0 ? -1 : length - static_cast<int>(size)), "payload length");
                Check(errno == EDOM, "errno changed");
            }
        }
    }

    header = reinterpret_cast<const void*>(1);
    std::memcpy(handle.data() + 8, &header, sizeof(header));
    std::int32_t length = -1;
    std::memcpy(handle.data() + 48, &length, sizeof(length));
    Check(udata_getRawMemory_nid_postfix(data) == header, "raw must not dereference header");
    Check(udata_getLength_nid_postfix(data) == -1, "unknown length must not dereference header");
    ExpectUnreadable([&] { udata_getMemory_nid_postfix(data); });
    length = 20;
    std::memcpy(handle.data() + 48, &length, sizeof(length));
    ExpectUnreadable([&] { udata_getLength_nid_postfix(data); });
    const auto* invalid = reinterpret_cast<const Icu::DataMemory*>(std::numeric_limits<std::uintptr_t>::max());
    ExpectUnreadable([&] { udata_getRawMemory_nid_postfix(invalid); });
    std::puts("ICU data memory accessors passed");
}
