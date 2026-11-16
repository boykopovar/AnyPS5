#include "prx/libSceAjm.native/include/Api.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

bool AllZero(const void* data, std::size_t size) {
    const std::uint8_t* bytes = static_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        if (bytes[i] != 0) {
            return false;
        }
    }
    return true;
}

}

int main() {
    AjmBatchInfo info{};
    std::memset(&info, 0xAA, sizeof(info));
    AjmBatchError error{};
    std::memset(&error, 0xAA, sizeof(error));
    Require(sceAjmBatchErrorDump(&info, &error) == 0);
    Require(AllZero(&error, sizeof(error)));

    Require(sceAjmBatchErrorDump(reinterpret_cast<const AjmBatchInfo*>(0x10), nullptr) == 0);
    constexpr int kInvalidParameter = static_cast<int>(0x80930005);
    Require(sceAjmBatchErrorDump(&info, reinterpret_cast<AjmBatchError*>(0x10)) == kInvalidParameter);
    return 0;
}
