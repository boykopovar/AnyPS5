#include "prx/libSceAvPlayerStreaming/libSceAvPlayerStreaming.hpp"
#include "tests/GuardedBuffer.hpp"

#include <array>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

void RequireUnchanged(const std::array<unsigned char, 0x300>& block) {
    for (const unsigned char byte : block) Require(byte == 0x5A);
}

void CheckReleaseBufferSecondWriteFailure() {
    GuardedBuffer guarded;
    std::uint8_t* const buffer = guarded.At<std::uint8_t>(sizeof(std::uint64_t));
    errno = 71;
    Require(sceTsReleaseBuffer(nullptr, buffer) == -1);
    Require(errno == 71);
    for (std::size_t index = 0; index < sizeof(std::uint64_t); ++index) Require(buffer[index] == 0);
}

void NullArgumentAnswers() {
    std::array<unsigned char, 0x300> file{};
    std::array<unsigned char, 0x300> first{};
    std::array<unsigned char, 0x300> second{};
    file.fill(0x5A);
    first.fill(0x5A);
    second.fill(0x5A);
    for (int call = 0; call < 2; ++call) {
        Require(sceTsCloseFile(nullptr) == -1);
        Require(sceTsDisableRepresentation(nullptr) == -1);
        Require(sceTsEnableRepresentation(nullptr) == -1);
        Require(sceTsEnableStream(nullptr) == -1);
        Require(sceTsGetRepresentationCount(nullptr) == -1);
        Require(sceTsGetUrlEncryptionType(nullptr) == -1);
        Require(sceTsJumpPTS(nullptr) == -1);
        Require(sceTsSetAvailableBW(nullptr) == -1);
        Require(sceTsSetStartingBW(nullptr) == -1);
        Require(sceTsStopFileStreaming(nullptr) == static_cast<int>(0xF0000200u));

        Require(sceTsGetAvailableBW(nullptr, first.data()) == -1);
        Require(sceTsGetAvailableBW(file.data(), nullptr) == -1);
        Require(sceTsGetLiveStatus(nullptr, first.data()) == -1);
        Require(sceTsGetLiveStatus(file.data(), nullptr) == -1);
        Require(sceTsGetNextAuInfo(nullptr, first.data()) == -1);
        Require(sceTsGetNextAuInfo(file.data(), nullptr) == -1);
        Require(sceTsGetNextUnitData(nullptr, first.data()) == -1);
        Require(sceTsGetNextUnitData(file.data(), nullptr) == -1);

        Require(sceTsGetStreamInfo(nullptr, 0, first.data()) == -1);
        Require(sceTsGetStreamInfo(file.data(), 0, nullptr) == -1);
        Require(sceTsGetStreamInfo(file.data(), UINT32_MAX, nullptr) == -1);

        Require(sceTsGetSeekWindow(nullptr, first.data(), second.data()) == -1);
        Require(sceTsGetSeekWindow(file.data(), nullptr, second.data()) == -1);
        Require(sceTsGetSeekWindow(file.data(), first.data(), nullptr) == -1);
        Require(sceTsStartFileStreaming(nullptr, first.data(), second.data()) == -1);
        Require(sceTsStartFileStreaming(file.data(), nullptr, second.data()) == -1);
        Require(sceTsStartFileStreaming(file.data(), first.data(), nullptr) == -1);
    }
    RequireUnchanged(file);
    RequireUnchanged(first);
    RequireUnchanged(second);
}

}

int main() {
    NullArgumentAnswers();
    for (int call = 0; call < 2; ++call) Require(MvpHttpGetLastError_nid_postfix(nullptr) == -1);
    errno = 71;
    Require(MvpHttpGetLastError_nid_postfix(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(0x10))) == -1);
    Require(errno == 71);

    for (const std::int32_t error : {0, 1, -1, static_cast<std::int32_t>(0x80020016), INT32_MAX}) {
        std::array<unsigned char, 0x40> context{};
        context.fill(0x5A);
        std::memcpy(context.data() + 0x30, &error, sizeof(error));
        const auto before = context;
        for (int call = 0; call < 2; ++call) Require(MvpHttpGetLastError_nid_postfix(context.data()) == error);
        Require(context == before);
    }

    Require(sceAvPlayerStreamingRegisterVprinf(nullptr) == 0);
    Require(sceAvPlayerStreamingRegisterVprinf(reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1234))) == 0);

    Require(sceTsOpenFile() == 0);

    Require(sceTsReleaseBuffer(nullptr, nullptr) == 0);
    struct TsBuffer {
        std::uint64_t f0{0x123456789ULL};
        std::uint32_t f8{0xabcdef};
    };
    TsBuffer buf{};
    Require(sceTsReleaseBuffer(nullptr, &buf) == 0);
    Require(buf.f0 == 0 && buf.f8 == 0);

    errno = 71;
    Require(sceTsReleaseBuffer(nullptr, reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x10))) == -1);
    Require(errno == 71);
    CheckReleaseBufferSecondWriteFailure();

    std::array<unsigned char, 0x10> dummy{};
    Require(sceTsGetRepresentationInfo(nullptr, 0, 0, dummy.data()) == -1);
    Require(sceTsGetRepresentationInfo(dummy.data(), 0, 0, nullptr) == -1);
    Require(sceTsGetRepresentationInfo(nullptr, 0, 0, nullptr) == -1);

    Require(sceTsRepresentationIsEnabled(nullptr, 0, 0, dummy.data()) == -1);
    Require(sceTsRepresentationIsEnabled(dummy.data(), 0, 0, nullptr) == -1);
    Require(sceTsRepresentationIsEnabled(nullptr, 0, 0, nullptr) == -1);

    return 0;
}
