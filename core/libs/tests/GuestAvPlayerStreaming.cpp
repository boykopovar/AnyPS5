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

void RequireUnchanged(const std::array<unsigned char, 0x300>& block) {
    for (const unsigned char byte : block) Require(byte == 0x5A);
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

} // namespace

int main() {
    NullArgumentAnswers();
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
