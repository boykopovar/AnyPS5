#include "prx/libSceVnaWebsocket/libSceVnaWebsocket.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
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

} // namespace

int main() {
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
    return 0;
}
