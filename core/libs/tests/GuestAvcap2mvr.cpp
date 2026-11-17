#include "prx/libSceAvcap2mvr/libSceAvcap2mvr.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "Testing.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = 99;
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
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);
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
    const auto closed = Stream(1, -1);
    errno = 71;
    Require(libSceAvcap2mvrUnknown_ZMR_plus_Q0NYcmU_nid_no_patch(closed.data()) == invalidArgument);
    Require(errno == 71);
    errno = 71;
    ExpectErr<std::invalid_argument>([&] { libSceAvcap2mvrUnknown_ZMR_plus_Q0NYcmU_nid_no_patch(reinterpret_cast<const void*>(0x10)); });
    Require(errno == 71);
    errno = 71;
    ExpectErr<std::invalid_argument>([&] { libSceAvcap2mvrUnknown_Kt2IOGEfaFc_nid_no_patch(reinterpret_cast<const void*>(0x10), other.data()); });
    Require(errno == 71);
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    return 0;
}
