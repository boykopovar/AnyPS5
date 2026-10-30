#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU converter callbacks: %s\n", message);
    std::abort();
}

template<class TCall>
bool Rejects(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

template<class TCall>
void CheckError(TCall call, std::int32_t initial, bool cleared) {
    std::array<std::int32_t, 3> storage{0x13579bdf, initial, 0x2468ace0};
    errno = 73;
    call(&storage[1]);
    Require(storage[1] == (cleared ? 0 : initial), "error policy result");
    Require(storage.front() == 0x13579bdf && storage.back() == 0x2468ace0, "error write stays within one int32");
    Require(errno == 73, "callback preserves errno");
}

struct PointCase {
    std::int32_t point;
    bool ignorable;
};

constexpr PointCase Points[]{
    {INT32_MIN, false}, {-1, false}, {0, false}, {0x41, false},
    {0xac, false}, {0xad, true}, {0xae, false},
    {0x34e, false}, {0x34f, true}, {0x350, false},
    {0x61b, false}, {0x61c, true}, {0x61d, false},
    {0x115e, false}, {0x115f, true}, {0x1160, true}, {0x1161, false},
    {0x17b3, false}, {0x17b4, true}, {0x17b5, true}, {0x17b6, false},
    {0x180a, false}, {0x180b, true}, {0x180e, true}, {0x180f, true}, {0x1810, false},
    {0x200a, false}, {0x200b, true}, {0x200f, true}, {0x2010, false},
    {0x2029, false}, {0x202a, true}, {0x202e, true}, {0x202f, false},
    {0x205f, false}, {0x2060, true}, {0x206f, true}, {0x2070, false},
    {0x3163, false}, {0x3164, true}, {0x3165, false},
    {0xd800, false}, {0xdfff, false},
    {0xfdff, false}, {0xfe00, true}, {0xfe0f, true}, {0xfe10, false},
    {0xfefe, false}, {0xfeff, true}, {0xff00, false},
    {0xff9f, false}, {0xffa0, true}, {0xffa1, false},
    {0xffef, false}, {0xfff0, true}, {0xfff8, true}, {0xfff9, false},
    {0x1bc9f, false}, {0x1bca0, true}, {0x1bca3, true}, {0x1bca4, false},
    {0x1d172, false}, {0x1d173, true}, {0x1d17a, true}, {0x1d17b, false},
    {0xdffff, false}, {0xe0000, true}, {0xe0fff, true}, {0xe1000, false},
    {0x10ffff, false}, {0x110000, false}, {INT32_MAX, false}
};

void PolicyMatrix() {
    const std::array<char, 4> contextBytes{'i', 'x', 0, 'I'};
    const std::array<const void*, 5> contexts{nullptr, &contextBytes[0], &contextBytes[1], &contextBytes[2], &contextBytes[3]};
    constexpr bool skipPolicy[5][8]{
        {true, true, true, false, false, false, false, false},
        {true, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false}
    };
    std::array<std::uint8_t, 64> arguments;
    arguments.fill(0x5a);
    const std::array<char16_t, 3> units{0xd800, 0, 0xbeef};
    const std::array<char, 3> bytes{static_cast<char>(0xff), 0, 'Z'};
    for (std::int32_t reason = 0; reason <= 7; ++reason) {
        for (std::size_t context = 0; context < contexts.size(); ++context) {
            for (const auto& point : Points) {
                const bool stopClears = reason == 0 && point.ignorable;
                const bool skipClears = stopClears || skipPolicy[context][reason];
                const auto stop = [&](std::int32_t* error) {
                    UCNV_FROM_U_CALLBACK_STOP_nid_postfix(contexts[context], reinterpret_cast<Icu::ConverterFromUnicodeArgs*>(arguments.data()), units.data(), INT32_MIN, point.point, reason, error);
                };
                const auto skip = [&](std::int32_t* error) {
                    UCNV_FROM_U_CALLBACK_SKIP_nid_postfix(contexts[context], reinterpret_cast<Icu::ConverterFromUnicodeArgs*>(arguments.data()), units.data(), INT32_MAX, point.point, reason, error);
                };
                for (std::int32_t initial : {0, 10, 12, -128, INT32_MIN, INT32_MAX}) {
                    CheckError(stop, initial, stopClears);
                    CheckError(skip, initial, skipClears);
                }
                Require(Rejects([&] { stop(nullptr); }) == stopClears, "FROM STOP only requires an error pointer when writing");
                Require(Rejects([&] { skip(nullptr); }) == skipClears, "FROM SKIP only requires an error pointer when writing");
            }
            const auto stop = [&](std::int32_t* error) {
                UCNV_TO_U_CALLBACK_STOP_nid_postfix(contexts[context], reinterpret_cast<Icu::ConverterToUnicodeArgs*>(arguments.data()), bytes.data(), INT32_MAX, reason, error);
            };
            const auto skip = [&](std::int32_t* error) {
                UCNV_TO_U_CALLBACK_SKIP_nid_postfix(contexts[context], reinterpret_cast<Icu::ConverterToUnicodeArgs*>(arguments.data()), bytes.data(), INT32_MIN, reason, error);
            };
            for (std::int32_t initial : {0, 10, 12, -128, INT32_MIN, INT32_MAX}) {
                CheckError(stop, initial, false);
                CheckError(skip, initial, skipPolicy[context][reason]);
            }
            Require(!Rejects([&] { stop(nullptr); }), "TO STOP never requires an error pointer");
            Require(Rejects([&] { skip(nullptr); }) == skipPolicy[context][reason], "TO SKIP only requires an error pointer when writing");
        }
    }
    for (auto byte : arguments) Require(byte == 0x5a, "converter argument storage is untouched");
    Require(units == std::array<char16_t, 3>{0xd800, 0, 0xbeef}, "UTF-16 input is untouched");
    Require(bytes == std::array<char, 3>{static_cast<char>(0xff), 0, 'Z'}, "byte input is untouched");
    Require(contextBytes == std::array<char, 4>{'i', 'x', 0, 'I'}, "context is untouched");
}

void IgnoredPointers() {
    const auto* unused = reinterpret_cast<const void*>(std::uintptr_t{1});
    auto* from = reinterpret_cast<Icu::ConverterFromUnicodeArgs*>(std::uintptr_t{1});
    auto* to = reinterpret_cast<Icu::ConverterToUnicodeArgs*>(std::uintptr_t{1});
    auto* error = reinterpret_cast<std::int32_t*>(std::uintptr_t{1});
    const auto* units = reinterpret_cast<const char16_t*>(std::uintptr_t{1});
    const auto* bytes = reinterpret_cast<const char*>(std::uintptr_t{1});
    for (std::int32_t reason = 3; reason <= 7; ++reason) {
        UCNV_FROM_U_CALLBACK_STOP_nid_postfix(unused, from, units, -1, 0xad, reason, error);
        UCNV_FROM_U_CALLBACK_SKIP_nid_postfix(unused, from, units, -1, 0xad, reason, error);
        UCNV_TO_U_CALLBACK_STOP_nid_postfix(unused, to, bytes, -1, reason, error);
        UCNV_TO_U_CALLBACK_SKIP_nid_postfix(unused, to, bytes, -1, reason, error);
    }
    for (std::int32_t reason = 0; reason <= 2; ++reason)
        UCNV_TO_U_CALLBACK_STOP_nid_postfix(unused, to, bytes, -1, reason, error);
    UCNV_FROM_U_CALLBACK_STOP_nid_postfix(unused, from, units, -1, 0x41, 0, error);
    std::int32_t status = 10;
    UCNV_FROM_U_CALLBACK_STOP_nid_postfix(unused, from, units, -1, 0xad, 0, &status);
    Require(status == 0, "FROM STOP default-ignorable handling does not read context or converter state");
    status = 10;
    UCNV_FROM_U_CALLBACK_SKIP_nid_postfix(unused, from, units, -1, 0xad, 0, &status);
    Require(status == 0, "FROM SKIP default-ignorable handling precedes context access");
    status = 12;
    UCNV_FROM_U_CALLBACK_SKIP_nid_postfix(nullptr, nullptr, nullptr, 0, 0x41, 1, &status);
    Require(status == 0, "FROM SKIP accepts unused null conversion pointers");
    status = 12;
    UCNV_TO_U_CALLBACK_SKIP_nid_postfix(nullptr, nullptr, nullptr, 0, 1, &status);
    Require(status == 0, "TO SKIP accepts unused null conversion pointers");
}

void InvalidReasons() {
    const auto* unused = reinterpret_cast<const void*>(std::uintptr_t{1});
    std::int32_t status = 10;
    for (std::int32_t reason : {INT32_MIN, -1, 8, INT32_MAX}) {
        Require(Rejects([&] { UCNV_FROM_U_CALLBACK_STOP_nid_postfix(unused, nullptr, nullptr, 0, 0xad, reason, &status); }), "FROM STOP rejects values outside the enum range");
        Require(Rejects([&] { UCNV_FROM_U_CALLBACK_SKIP_nid_postfix(unused, nullptr, nullptr, 0, 0xad, reason, &status); }), "FROM SKIP rejects values outside the enum range");
        Require(Rejects([&] { UCNV_TO_U_CALLBACK_STOP_nid_postfix(unused, nullptr, nullptr, 0, reason, &status); }), "TO STOP rejects values outside the enum range");
        Require(Rejects([&] { UCNV_TO_U_CALLBACK_SKIP_nid_postfix(unused, nullptr, nullptr, 0, reason, &status); }), "TO SKIP rejects values outside the enum range");
        Require(status == 10, "invalid reasons preserve the error");
    }
}

}

int main() {
    PolicyMatrix();
    IgnoredPointers();
    InvalidReasons();
}
