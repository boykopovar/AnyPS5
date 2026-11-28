#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceTtsCoreG3Bc/libSceTtsCoreG3Bc.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "GuestTtsCoreG3BcUnknownReturns.cpp:%d: requirement failed\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Fn>
bool ThrowsInvalidArgument(Fn&& fn) {
    try {
        fn();
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
}

void* gDestroyedObject = nullptr;
std::size_t gDestroyCalls = 0;

void APS5_VABI MockDeletingDtor(void* object) {
    ++gDestroyCalls;
    gDestroyedObject = object;
}

template <typename T>
void WriteField(void* base, std::size_t offset, T value) {
    std::memcpy(static_cast<std::byte*>(base) + offset, &value, sizeof(value));
}

template <typename T>
T ReadField(const void* base, std::size_t offset) {
    T value{};
    std::memcpy(&value, static_cast<const std::byte*>(base) + offset, sizeof(value));
    return value;
}

}

int main() {
    libSceTtsCoreG3BcUnknown_lQLzsMt_minus_6NU_nid_no_patch();
    libSceTtsCoreG3BcUnknown_V_plus_nR_plus_xHS2ag_nid_no_patch();
    Require(libSceTtsCoreG3BcUnknown_k1_minus_orvzQrHw_nid_no_patch() == 8);
    Require(libSceTtsCoreG3BcUnknown__plus_b5_minus_z8Ff4cw_nid_no_patch() == 100);

    libSceTtsCoreG3BcUnknown_RByzVVv6e0A_nid_no_patch(nullptr);
    Require(gDestroyCalls == 0);
    std::array<void*, 2> vtable{nullptr, reinterpret_cast<void*>(&MockDeletingDtor)};
    struct MockObject {
        void* vptr;
    } mockObj{vtable.data()};
    libSceTtsCoreG3BcUnknown_RByzVVv6e0A_nid_no_patch(&mockObj);
    Require(gDestroyCalls == 1 && gDestroyedObject == &mockObj);
    MockObject nullVtableObj{nullptr};
    Require(ThrowsInvalidArgument([&] {
        libSceTtsCoreG3BcUnknown_RByzVVv6e0A_nid_no_patch(&nullVtableObj);
    }));
    std::array<void*, 2> nullDtorVtable{nullptr, nullptr};
    MockObject nullDtorObj{nullDtorVtable.data()};
    Require(ThrowsInvalidArgument([&] {
        libSceTtsCoreG3BcUnknown_RByzVVv6e0A_nid_no_patch(&nullDtorObj);
    }));
    Require(gDestroyCalls == 1);

    std::array<std::byte, 0xe80> engine{};
    engine.fill(std::byte{0xa5});

    Require(ThrowsInvalidArgument([] {
        libSceTtsCoreG3BcUnknown_iW0DoaHZE5Y_nid_no_patch(nullptr, 1);
    }));
    libSceTtsCoreG3BcUnknown_iW0DoaHZE5Y_nid_no_patch(engine.data(), 1);
    Require(static_cast<std::uint8_t>(engine[0xe79]) == 1);
    Require(static_cast<std::uint8_t>(engine[0xe78]) == 0xa5);
    Require(static_cast<std::uint8_t>(engine[0xe7a]) == 0xa5);

    const std::array<std::int32_t, 4> params{12, -3, 150, 7};
    Require(ThrowsInvalidArgument([&] {
        libSceTtsCoreG3BcUnknown_Sq5GSDo_minus_Rhk_nid_no_patch(nullptr, params.data());
    }));
    Require(ThrowsInvalidArgument([&] {
        libSceTtsCoreG3BcUnknown_Sq5GSDo_minus_Rhk_nid_no_patch(engine.data(), nullptr);
    }));
    libSceTtsCoreG3BcUnknown_Sq5GSDo_minus_Rhk_nid_no_patch(engine.data(), params.data());
    for (std::size_t i = 0; i < params.size(); ++i) {
        Require(ReadField<std::int32_t>(engine.data(), 0xe58 + i * sizeof(std::int32_t)) == params[i]);
    }
    Require(static_cast<std::uint8_t>(engine[0xe57]) == 0xa5);
    Require(static_cast<std::uint8_t>(engine[0xe68]) == 0xa5);

    Require(ThrowsInvalidArgument([] {
        (void)libSceTtsCoreG3BcUnknown_pacG6ffHAvU_nid_no_patch(nullptr);
    }));
    WriteField<std::int32_t>(engine.data(), 0xe6c, 10);
    Require(libSceTtsCoreG3BcUnknown_pacG6ffHAvU_nid_no_patch(engine.data()) == 27);
    WriteField<std::int32_t>(engine.data(), 0xe6c, 9);
    Require(libSceTtsCoreG3BcUnknown_pacG6ffHAvU_nid_no_patch(engine.data()) == 30);
    WriteField<std::int32_t>(engine.data(), 0xe6c, 0);
    Require(libSceTtsCoreG3BcUnknown_pacG6ffHAvU_nid_no_patch(engine.data()) == 77);
    WriteField<std::int32_t>(engine.data(), 0xe6c, -9);
    Require(libSceTtsCoreG3BcUnknown_pacG6ffHAvU_nid_no_patch(engine.data()) == 199);
    WriteField<std::int32_t>(engine.data(), 0xe6c, -100);
    Require(libSceTtsCoreG3BcUnknown_pacG6ffHAvU_nid_no_patch(engine.data()) == 2656971);
    WriteField<std::int32_t>(engine.data(), 0xe6c, -101);
    Require(ThrowsInvalidArgument([&] {
        (void)libSceTtsCoreG3BcUnknown_pacG6ffHAvU_nid_no_patch(engine.data());
    }));
    WriteField<std::int32_t>(engine.data(), 0xe6c, 11);
    Require(ThrowsInvalidArgument([&] {
        (void)libSceTtsCoreG3BcUnknown_pacG6ffHAvU_nid_no_patch(engine.data());
    }));

    Require(libSceTtsCoreG3BcUnknown_bapD6H6e7Yk_nid_no_patch(nullptr) == 0);
    WriteField<const void*>(engine.data(), 0x5f8, nullptr);
    Require(libSceTtsCoreG3BcUnknown_bapD6H6e7Yk_nid_no_patch(engine.data()) == 0x5622);

    std::array<std::byte, 0x60> ctx{};
    std::array<std::byte, 0x48> slot{};
    std::array<std::byte, 0x800> voiceData{};
    WriteField<const void*>(engine.data(), 0x5f8, ctx.data());
    WriteField<std::int32_t>(ctx.data(), 0x48, 0);
    WriteField<std::int32_t>(ctx.data(), 0x38, 0);
    Require(libSceTtsCoreG3BcUnknown_bapD6H6e7Yk_nid_no_patch(engine.data()) == 0x5622);

    WriteField<std::int32_t>(ctx.data(), 0x48, 1);
    WriteField<std::int32_t>(ctx.data(), 0x38, -1);
    Require(libSceTtsCoreG3BcUnknown_bapD6H6e7Yk_nid_no_patch(engine.data()) == 0x5622);

    WriteField<std::int32_t>(ctx.data(), 0x38, 0);
    WriteField<const void*>(ctx.data(), 0x40, nullptr);
    Require(libSceTtsCoreG3BcUnknown_bapD6H6e7Yk_nid_no_patch(engine.data()) == 0x5622);

    WriteField<const void*>(ctx.data(), 0x40, slot.data());
    WriteField<const void*>(slot.data(), 0x38, nullptr);
    Require(libSceTtsCoreG3BcUnknown_bapD6H6e7Yk_nid_no_patch(engine.data()) == 0x5622);

    WriteField<const void*>(slot.data(), 0x38, voiceData.data());
    WriteField<std::uint32_t>(voiceData.data(), 0x7e4, 48000u);
    Require(libSceTtsCoreG3BcUnknown_bapD6H6e7Yk_nid_no_patch(engine.data()) == 48000);

    std::array<std::byte, 0x800> inner{};
    std::array<std::byte, 0x90> outInfo{};

    Require(ThrowsInvalidArgument([&] {
        (void)libSceTtsCoreG3BcUnknown_fF3Q3LdPUtk_nid_no_patch(nullptr, outInfo.data());
    }));
    Require(ThrowsInvalidArgument([&] {
        (void)libSceTtsCoreG3BcUnknown_fF3Q3LdPUtk_nid_no_patch(engine.data(), nullptr);
    }));
    WriteField<const void*>(engine.data(), 0x20, nullptr);
    Require(ThrowsInvalidArgument([&] {
        (void)libSceTtsCoreG3BcUnknown_fF3Q3LdPUtk_nid_no_patch(engine.data(), outInfo.data());
    }));

    WriteField<const void*>(engine.data(), 0x20, inner.data());
    WriteField<std::uint32_t>(inner.data(), 0x7e4, 0u);
    Require(ThrowsInvalidArgument([&] {
        (void)libSceTtsCoreG3BcUnknown_fF3Q3LdPUtk_nid_no_patch(engine.data(), outInfo.data());
    }));

    outInfo.fill(std::byte{0xa5});
    WriteField<std::uint32_t>(inner.data(), 0x7e4, 16000u);
    Require(libSceTtsCoreG3BcUnknown_fF3Q3LdPUtk_nid_no_patch(engine.data(), outInfo.data()) == 0);
    for (std::size_t i = 0; i < 0x80; ++i) Require(static_cast<std::uint8_t>(outInfo[i]) == 0);
    Require(ReadField<std::uint32_t>(outInfo.data(), 0x80) == 62499u);
    Require(ReadField<std::uint32_t>(outInfo.data(), 0x84) == 64u);
    Require(static_cast<std::uint8_t>(outInfo[0x88]) == 0xa5);

    outInfo.fill(std::byte{0xa5});
    WriteField<std::uint32_t>(inner.data(), 0x7e4, 22050u);
    Require(libSceTtsCoreG3BcUnknown_fF3Q3LdPUtk_nid_no_patch(engine.data(), outInfo.data()) == 0);
    Require(ReadField<std::uint32_t>(outInfo.data(), 0x80) == (1000000000u / 22050u));
    Require(ReadField<std::uint32_t>(outInfo.data(), 0x84) == 64u);

    return 0;
}
