#include "prx/libSceAvPlayerStreaming/libSceAvPlayerStreaming.hpp"

#include <array>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

} // namespace

int main() {
    for (int call = 0; call < 2; ++call) Require(MvpHttpGetLastError_nid_postfix(nullptr) == -1);

    for (const std::int32_t error : {0, 1, -1, static_cast<std::int32_t>(0x80020016), INT32_MAX}) {
        std::array<unsigned char, 0x40> context{};
        context.fill(0x5A);
        std::memcpy(context.data() + 0x30, &error, sizeof(error));
        const auto before = context;
        for (int call = 0; call < 2; ++call) Require(MvpHttpGetLastError_nid_postfix(context.data()) == error);
        Require(context == before);
    }
    return 0;
}
