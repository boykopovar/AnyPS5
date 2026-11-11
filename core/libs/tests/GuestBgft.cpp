#include "prx/libSceBgft/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

static void Require(bool value) { if (!value) std::abort(); }

class RawParams {
public:
    void SetPointer(std::size_t offset, const char* text) {
        std::memcpy(bytes + offset, &text, sizeof(text));
    }
    const BgftStorageTaskParam* AsParams() const {
        return reinterpret_cast<const BgftStorageTaskParam*>(bytes);
    }
private:
    std::byte bytes[sizeof(BgftStorageTaskParam)]{};
};

static void Fill(char* text, std::size_t length) {
    std::memset(text, 'A', length);
    text[length] = 0;
}

int main() {
    constexpr int alreadyInitialized = static_cast<int>(0x80990001);
    constexpr int notInitialized = static_cast<int>(0x80990002);
    constexpr int invalidArgument = static_cast<int>(0x80990004);
    constexpr int notSupported = static_cast<int>(0x80990006);

    char idTooLong[0x32];
    char idAllowed[0x31];
    char text16TooLong[0x801];
    char text16Allowed[0x800];
    char text20TooLong[0x25a];
    char text20Allowed[0x259];
    char text28TooLong[0x801];
    char text28Allowed[0x800];
    Fill(idTooLong, 0x31);
    Fill(idAllowed, 0x30);
    Fill(text16TooLong, 0x800);
    Fill(text16Allowed, 0x7ff);
    Fill(text20TooLong, 0x259);
    Fill(text20Allowed, 0x258);
    Fill(text28TooLong, 0x800);
    Fill(text28Allowed, 0x7ff);

    RawParams valid;
    valid.SetPointer(0x8, "UP0001-CUSA00000_00-AAAAAAAAAAAAAAAA");
    std::int32_t taskId = 0x5a5a5a5a;

    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(valid.AsParams(), &taskId) == notInitialized);
    Require(sceBgftServiceIntDownloadRegisterTaskByStorage(valid.AsParams(), &taskId) == notInitialized);
    Require(sceBgftServiceIntDownloadRegisterTaskByStorage(nullptr, &taskId) == invalidArgument);
    Require(sceBgftServiceIntInit(nullptr) == invalidArgument);

    std::byte heap[1024]{};
    const BgftInitParam initialized{heap, sizeof(heap)};
    Require(sceBgftServiceIntInit(&initialized) == 0);
    Require(sceBgftServiceIntInit(&initialized) == alreadyInitialized);
    Require(sceBgftServiceIntInit(nullptr) == alreadyInitialized);
    Require(sceBgftServiceIntInit(&initialized) == alreadyInitialized);

    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(nullptr, nullptr) == invalidArgument);
    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(valid.AsParams(), nullptr) == invalidArgument);

    RawParams missingId;
    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(missingId.AsParams(), &taskId) == invalidArgument);
    RawParams idOverLong = valid;
    idOverLong.SetPointer(0x8, idTooLong);
    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(idOverLong.AsParams(), &taskId) == invalidArgument);
    RawParams text16OverLong = valid;
    text16OverLong.SetPointer(0x10, text16TooLong);
    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(text16OverLong.AsParams(), &taskId) == invalidArgument);
    RawParams text20OverLong = valid;
    text20OverLong.SetPointer(0x20, text20TooLong);
    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(text20OverLong.AsParams(), &taskId) == invalidArgument);
    RawParams text28OverLong = valid;
    text28OverLong.SetPointer(0x28, text28TooLong);
    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(text28OverLong.AsParams(), &taskId) == invalidArgument);

    RawParams boundary;
    boundary.SetPointer(0x8, idAllowed);
    boundary.SetPointer(0x10, text16Allowed);
    boundary.SetPointer(0x20, text20Allowed);
    boundary.SetPointer(0x28, text28Allowed);
    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(boundary.AsParams(), &taskId) == notSupported);
    Require(sceBgftServiceIntDownloadRegisterTaskByStorageEx(valid.AsParams(), &taskId) == notSupported);
    Require(taskId == 0x5a5a5a5a);
    Require(sceBgftServiceIntDownloadRegisterTaskByStorage(boundary.AsParams(), &taskId) == notSupported);
    Require(sceBgftServiceIntDownloadRegisterTaskByStorage(nullptr, &taskId) == invalidArgument);
    Require(taskId == 0x5a5a5a5a);

    Require(sceBgftServiceIntTerm() == 0);
    return 0;
}
