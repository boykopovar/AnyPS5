#include "prx/libSceAvcap2mvr/libSceAvcap2mvr.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

constexpr int invalidArgument = static_cast<int>(0x81AC0001u);

std::array<std::uint8_t, 0x80> Stream(std::uint8_t state, std::int64_t word) {
    std::array<std::uint8_t, 0x80> stream{};
    stream[0] = state;
    std::memcpy(stream.data() + 0x70, &word, sizeof(word));
    return stream;
}

}

int main() {
    const std::array<std::uint8_t, 16> other{};
    for (const std::uint8_t state : std::array<std::uint8_t, 3>{0, 1, 2}) {
        for (const std::int64_t word : std::array<std::int64_t, 2>{-1, 0x10}) {
            const auto stream = Stream(state, word);
            Require(libSceAvcap2mvrUnknown_AVuH0FujF30_nid_no_patch(stream.data(), other.data()) == invalidArgument);
            Require(libSceAvcap2mvrUnknown_QSLdUawhEhg_nid_no_patch(stream.data(), other.data()) == invalidArgument);
            Require(libSceAvcap2mvrUnknown_JgAC1LkbWns_nid_no_patch(stream.data()) == invalidArgument);
            Require(libSceAvcap2mvrUnknown_R5tFB0tMbgY_nid_no_patch(stream.data(), other.data()) == invalidArgument);
            Require(libSceAvcap2mvrUnknown_s7aQGkLitBU_nid_no_patch(stream.data(), other.data()) == invalidArgument);
            Require(libSceAvcap2mvrUnknown_tq_minus_PG8QaNDM_nid_no_patch(stream.data(), other.data()) == invalidArgument);
            if (state != 1 || word == -1) {
                Require(libSceAvcap2mvrUnknown_Kt2IOGEfaFc_nid_no_patch(stream.data(), other.data()) == invalidArgument);
                Require(libSceAvcap2mvrUnknown_ZMR_plus_Q0NYcmU_nid_no_patch(stream.data()) == invalidArgument);
            }
        }
    }
    const auto open = Stream(1, 0x10);
    Require(libSceAvcap2mvrUnknown_AVuH0FujF30_nid_no_patch(nullptr, other.data()) == invalidArgument);
    Require(libSceAvcap2mvrUnknown_AVuH0FujF30_nid_no_patch(open.data(), nullptr) == invalidArgument);
    Require(libSceAvcap2mvrUnknown_QSLdUawhEhg_nid_no_patch(nullptr, nullptr) == invalidArgument);
    Require(libSceAvcap2mvrUnknown_JgAC1LkbWns_nid_no_patch(nullptr) == invalidArgument);
    Require(libSceAvcap2mvrUnknown_R5tFB0tMbgY_nid_no_patch(open.data(), nullptr) == invalidArgument);
    Require(libSceAvcap2mvrUnknown_s7aQGkLitBU_nid_no_patch(nullptr, other.data()) == invalidArgument);
    Require(libSceAvcap2mvrUnknown_tq_minus_PG8QaNDM_nid_no_patch(open.data(), nullptr) == invalidArgument);
    Require(libSceAvcap2mvrUnknown_Kt2IOGEfaFc_nid_no_patch(nullptr, other.data()) == invalidArgument);
    Require(libSceAvcap2mvrUnknown_Kt2IOGEfaFc_nid_no_patch(open.data(), nullptr) == invalidArgument);
    Require(libSceAvcap2mvrUnknown_ZMR_plus_Q0NYcmU_nid_no_patch(nullptr) == invalidArgument);
    return 0;
}
