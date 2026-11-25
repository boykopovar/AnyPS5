#include "prx/libSceVnaWebsocket/libSceVnaWebsocket.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

template <typename TAction>
bool ThrowsMessage(TAction action, const char* expected) {
    try {
        action();
    } catch (const std::invalid_argument& error) {
        return std::string(error.what()).find(expected) != std::string::npos;
    }
    return false;
}

std::array<unsigned char, 64> Counting() {
    std::array<unsigned char, 64> bytes{};
    for (std::size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<unsigned char>(i);
    return bytes;
}

template <typename TInit>
void CheckConfigInit(TInit initFn, const char* nid) {
    std::array<unsigned char, 0x40> buffer{};
    buffer.fill(0xA5);
    errno = 71;
    initFn(buffer.data());
    Require(errno == 71);
    constexpr std::array<unsigned char, 28> expectedTail = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,
        0xb8, 0x0b, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0xb8, 0x0b, 0x00, 0x00,
    };
    for (std::size_t i = 0; i < 9; ++i) Require(buffer[i] == 0);
    for (std::size_t i = 9; i < 0x10; ++i) Require(buffer[i] == 0xA5);
    Require(std::memcmp(buffer.data() + 0x10, expectedTail.data(), expectedTail.size()) == 0);
    for (std::size_t i = 0x2c; i < buffer.size(); ++i) Require(buffer[i] == 0xA5);

    Require(ThrowsMessage([&] { initFn(nullptr); }, (std::string(nid) + ": null object").c_str()));
    errno = 71;
    Require(ThrowsMessage([&] { initFn(reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x10))); }, (std::string(nid) + ": unmapped object").c_str()));
    Require(errno == 71);
}

template <typename TClear>
void CheckCallbackClear(TClear clearFn, const char* nid) {
    std::array<unsigned char, 0x1d0> buffer{};
    buffer.fill(0xA5);
    errno = 71;
    clearFn(buffer.data());
    Require(errno == 71);
    for (std::size_t i = 0; i < buffer.size(); ++i) {
        const bool isClearedSlot = i >= 0x38 && i < 0x1c0 && ((i - 0x38) % 0x40) < 8;
        Require(buffer[i] == (isClearedSlot ? 0x00 : 0xA5));
    }

    Require(ThrowsMessage([&] { clearFn(nullptr); }, (std::string(nid) + ": null object").c_str()));
    errno = 71;
    Require(ThrowsMessage([&] { clearFn(reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x10))); }, (std::string(nid) + ": unmapped object").c_str()));
    Require(errno == 71);
}

} // namespace

int main() {
    libSceVnaWebsocketUnknown_Bw0_minus_YBkPNm4_nid_no_patch();
    int object = 0;
    for (void* value : {static_cast<void*>(nullptr), static_cast<void*>(&object), reinterpret_cast<void*>(~std::uintptr_t{0})}) {
        Require(libSceVnaWebsocketUnknown_hSnmIgcBCMk_nid_no_patch(value) == value);
        Require(libSceVnaWebsocketUnknown_t534VXlYZI8_nid_no_patch(value) == value);
    }

    {
        const auto source = Counting();
        std::array<unsigned char, 64> destination{};
        destination.fill(0xEE);
        Require(libSceVnaWebsocketUnknown_J3fL2giuM_plus_0_nid_no_patch(destination.data(), source.data()) == destination.data());
        for (std::size_t i = 0; i < destination.size(); ++i) Require(destination[i] == (i < 24 ? i : 0xEE));
    }
    {
        auto bytes = Counting();
        Require(libSceVnaWebsocketUnknown_J3fL2giuM_plus_0_nid_no_patch(bytes.data(), bytes.data() + 8) == bytes.data());
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            const std::size_t expected = i < 8 ? i + 8 : i < 16 ? i + 16 : i < 24 ? i + 8 : i;
            Require(bytes[i] == expected);
        }
    }

    Require(ThrowsMessage([&] { libSceVnaWebsocketUnknown_J3fL2giuM_plus_0_nid_no_patch(nullptr, &object); }, "J3fL2giuM+0: null object"));
    Require(ThrowsMessage([&] { libSceVnaWebsocketUnknown_J3fL2giuM_plus_0_nid_no_patch(&object, nullptr); }, "J3fL2giuM+0: null object"));
    const auto* invalidSource = reinterpret_cast<const void*>(0x10);
    auto* invalidDestination = reinterpret_cast<void*>(0x10);
    {
        const auto source = Counting();
        std::array<unsigned char, 64> destination{};
        errno = 71;
        Require(ThrowsMessage([&] { libSceVnaWebsocketUnknown_J3fL2giuM_plus_0_nid_no_patch(destination.data(), invalidSource); }, "J3fL2giuM+0: unmapped source"));
        Require(errno == 71);
        Require(ThrowsMessage([&] { libSceVnaWebsocketUnknown_J3fL2giuM_plus_0_nid_no_patch(invalidDestination, source.data()); }, "J3fL2giuM+0: unmapped destination"));
        Require(errno == 71);
    }

    {
        const auto source = Counting();
        std::array<unsigned char, 64> destination{};
        destination.fill(0xEE);
        Require(libSceVnaWebsocketUnknown_fxgYzL5aGyw_nid_no_patch(destination.data(), source.data()) == destination.data());
        for (std::size_t i = 0; i < destination.size(); ++i) Require(destination[i] == (i < 44 ? i : 0xEE));
    }
    {
        auto bytes = Counting();
        Require(libSceVnaWebsocketUnknown_fxgYzL5aGyw_nid_no_patch(bytes.data(), bytes.data() + 4) == bytes.data());
        for (std::size_t i = 0; i < bytes.size(); ++i) Require(bytes[i] == (i < 44 ? i + 4 : i));
    }

    Require(ThrowsMessage([&] { libSceVnaWebsocketUnknown_fxgYzL5aGyw_nid_no_patch(nullptr, &object); }, "fxgYzL5aGyw: null object"));
    Require(ThrowsMessage([&] { libSceVnaWebsocketUnknown_fxgYzL5aGyw_nid_no_patch(&object, nullptr); }, "fxgYzL5aGyw: null object"));
    {
        const auto source = Counting();
        std::array<unsigned char, 64> destination{};
        errno = 71;
        Require(ThrowsMessage([&] { libSceVnaWebsocketUnknown_fxgYzL5aGyw_nid_no_patch(destination.data(), invalidSource); }, "fxgYzL5aGyw: unmapped source"));
        Require(errno == 71);
        Require(ThrowsMessage([&] { libSceVnaWebsocketUnknown_fxgYzL5aGyw_nid_no_patch(invalidDestination, source.data()); }, "fxgYzL5aGyw: unmapped destination"));
        Require(errno == 71);
    }

    CheckConfigInit(libSceVnaWebsocketUnknown_MI6xIf_minus_bHXs_nid_no_patch, "MI6xIf-bHXs");
    CheckConfigInit(libSceVnaWebsocketUnknown_K_minus_logS9LtFw_nid_no_patch, "K-logS9LtFw");
    CheckCallbackClear(libSceVnaWebsocketUnknown_qUkQKyPORE8_nid_no_patch, "qUkQKyPORE8");
    CheckCallbackClear(libSceVnaWebsocketUnknown_OUogBgTPp6E_nid_no_patch, "OUogBgTPp6E");
    return 0;
}
