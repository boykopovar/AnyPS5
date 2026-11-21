#include "prx/libSceGameCustomDataDialog/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kStatusNone = 0;
constexpr int kStatusInitialized = 1;
constexpr int kStatusFinished = 3;
constexpr int kErrNotInitialized = static_cast<int>(0x80B80003);
constexpr int kErrAlreadyInitialized = static_cast<int>(0x80B80004);
constexpr int kErrNotFinished = static_cast<int>(0x80B80005);
constexpr int kErrParam = static_cast<int>(0x80B8000A);
constexpr int kErrNotRunning = static_cast<int>(0x80B8000B);
constexpr int kErrArgNull = static_cast<int>(0x80B8000D);
constexpr int kResultUserCanceled = 1;
constexpr int kModeSend = 1;
constexpr int kModeRecv = 2;
constexpr int kModeSendUrl = 3;
constexpr int kAddressUserSelectDisable = 1;
constexpr int kAddressUserSelectEnable = 2;
constexpr std::size_t kAccountSentListBytes = 520;
constexpr std::size_t kOnlineSentListBytes = 324;

struct OnlineId {
    char data[16];
    char term;
    char dummy[3];
};

struct SendInfo {
    const char* userMessage;
    const char* dataName;
    const void* dataNameMultiLang;
    std::uint32_t dataNameMultiLangNum;
    std::uint32_t padding1C;
    const char* dataDetail;
    const void* dataDetailMultiLang;
    std::uint32_t dataDetailMultiLangNum;
    std::uint32_t padding34;
    const void* thumbnail;
    std::uint32_t thumbnailSize;
    std::uint32_t padding44;
    const void* payload;
    std::uint32_t dataSize;
    std::uint32_t padding54;
    std::int32_t expireMinutes;
    std::uint32_t padding5C;
    std::uint32_t platformCount;
    std::uint32_t padding64;
    const char* const* platformName;
    std::int32_t addressType;
    std::uint32_t padding74;
    const void* ids;
    std::uint32_t idsCount;
    std::uint32_t padding84;
};

static_assert(sizeof(SendInfo) == 0x88);

union DataParam {
    SendInfo send;
    std::uint8_t recvReserved[160];
};

struct Param {
    std::uint64_t baseSize;
    std::uint8_t baseReserved[36];
    std::uint32_t baseMagic;
    std::uint32_t size;
    std::int32_t mode;
    std::int32_t userId;
    std::uint32_t padding3C;
    void* callbackArg;
    const DataParam* dataParam;
    std::uint8_t reserved[64];
};

static_assert(sizeof(Param) == 0x90);

struct Result {
    void* callbackArg;
    std::int32_t errorCode;
    std::int32_t result;
    void* sent;
    std::uint8_t reserved[32];
};

static_assert(sizeof(Result) == 0x38);

using OpenFn = int (APS5_VABI *)(const void*);
using GetResultFn = int (APS5_VABI *)(void*);

const char* const kPlatforms[] = {"first", "second"};
const char* const kNullPlatform[] = {"first", nullptr};
const char* const kEmptyPlatform[] = {"first", ""};
const std::uint64_t kAccountIds[] = {1, 2};
const std::uint64_t kZeroAccountId[] = {1, 0};
const OnlineId kOnlineIds[] = {{"alpha", 0, {}}, {"beta", 0, {}}};
const OnlineId kEmptyOnlineId[] = {{"alpha", 0, {}}, {"", 0, {}}};
const std::uint8_t kThumbnail[4] = {0x89, 'P', 'N', 'G'};
const std::uint8_t kData[8] = {1, 2, 3, 4, 5, 6, 7, 8};

Param MakeParam(int mode, const DataParam* data, void* callbackArg) {
    Param param{};
    param.baseSize = 48;
    param.size = sizeof(Param);
    param.mode = mode;
    param.userId = USER_SERVICE_INITIAL_USER_ID;
    param.callbackArg = callbackArg;
    param.dataParam = data;
    return param;
}

DataParam MakeSend(const void* ids) {
    DataParam data{};
    data.send.userMessage = "message";
    data.send.dataName = "name";
    data.send.dataDetail = "detail";
    data.send.thumbnail = kThumbnail;
    data.send.thumbnailSize = sizeof(kThumbnail);
    data.send.payload = kData;
    data.send.dataSize = sizeof(kData);
    data.send.platformCount = 2;
    data.send.platformName = kPlatforms;
    data.send.addressType = kAddressUserSelectDisable;
    data.send.ids = ids;
    data.send.idsCount = 2;
    return data;
}

int OpenSend(OpenFn open, const DataParam& data, int mode = kModeSend) {
    const Param param = MakeParam(mode, &data, nullptr);
    return open(&param);
}

void RequireSendChecks(OpenFn open, const void* ids, const void* rejectedIds) {
    const std::string longMessage(2049, 'm');
    const std::string asciiMessage(513, 'm');
    std::string wideMessage;
    for (int i = 0; i < 512; ++i) wideMessage += "\xC3\xA9";
    const std::string wideTooLong = wideMessage + "x";
    const std::string longName(257, 'n');
    const std::string maxName(256, 'n');
    const std::string longDetail(2049, 'd');
    const std::string longUrl(1025, 'u');
    const std::string maxUrl(1024, 'u');

    DataParam data = MakeSend(ids);
    Require(OpenSend(open, data) == 0);
    Require(sceGameCustomDataDialogGetStatus() == kStatusFinished);

    data = MakeSend(rejectedIds);
    Require(OpenSend(open, data) == kErrParam);

    data = MakeSend(ids);
    data.send.userMessage = nullptr;
    Require(OpenSend(open, data) == kErrArgNull);
    data.send.userMessage = "";
    Require(OpenSend(open, data) == 0);
    data.send.userMessage = longMessage.c_str();
    Require(OpenSend(open, data) == kErrParam);
    data.send.userMessage = asciiMessage.c_str();
    Require(OpenSend(open, data) == kErrParam);
    data.send.userMessage = wideMessage.c_str();
    Require(OpenSend(open, data) == 0);
    data.send.userMessage = wideTooLong.c_str();
    Require(OpenSend(open, data) == kErrParam);

    data = MakeSend(ids);
    data.send.dataName = nullptr;
    Require(OpenSend(open, data) == kErrArgNull);
    data.send.dataName = "";
    Require(OpenSend(open, data) == kErrParam);
    data.send.dataName = longName.c_str();
    Require(OpenSend(open, data) == kErrParam);
    data.send.dataName = maxName.c_str();
    Require(OpenSend(open, data) == 0);

    data = MakeSend(ids);
    data.send.dataDetail = nullptr;
    Require(OpenSend(open, data) == kErrArgNull);
    data.send.dataDetail = "";
    Require(OpenSend(open, data) == kErrParam);
    data.send.dataDetail = longDetail.c_str();
    Require(OpenSend(open, data) == kErrParam);

    data = MakeSend(ids);
    data.send.thumbnail = nullptr;
    Require(OpenSend(open, data) == kErrArgNull);
    data.send.thumbnail = kThumbnail;
    data.send.thumbnailSize = 0;
    Require(OpenSend(open, data) == kErrParam);
    data.send.thumbnailSize = 160 * 1024 + 1;
    Require(OpenSend(open, data) == kErrParam);
    data.send.thumbnailSize = 160 * 1024;
    Require(OpenSend(open, data) == 0);

    data = MakeSend(ids);
    data.send.payload = nullptr;
    Require(OpenSend(open, data) == kErrArgNull);
    data.send.payload = kData;
    data.send.dataSize = 0;
    Require(OpenSend(open, data) == kErrParam);
    data.send.dataSize = 1024 * 1024 + 1;
    Require(OpenSend(open, data) == kErrParam);
    data.send.dataSize = 1024 * 1024;
    Require(OpenSend(open, data) == 0);

    data = MakeSend(ids);
    data.send.payload = nullptr;
    data.send.dataSize = 0;
    Require(OpenSend(open, data, kModeSendUrl) == kErrArgNull);
    data.send.payload = "";
    Require(OpenSend(open, data, kModeSendUrl) == kErrParam);
    data.send.payload = longUrl.c_str();
    Require(OpenSend(open, data, kModeSendUrl) == kErrParam);
    data.send.payload = maxUrl.c_str();
    Require(OpenSend(open, data, kModeSendUrl) == 0);

    data = MakeSend(ids);
    data.send.addressType = 0;
    Require(OpenSend(open, data) == kErrParam);
    data.send.addressType = 3;
    Require(OpenSend(open, data) == kErrParam);

    data = MakeSend(ids);
    data.send.platformCount = 0;
    Require(OpenSend(open, data) == kErrParam);
    data.send.platformCount = 2;
    data.send.platformName = kNullPlatform;
    Require(OpenSend(open, data) == kErrArgNull);
    data.send.platformName = kEmptyPlatform;
    Require(OpenSend(open, data) == kErrParam);
    data.send.platformCount = 1;
    Require(OpenSend(open, data) == 0);

    data = MakeSend(ids);
    data.send.idsCount = 0;
    Require(OpenSend(open, data) == kErrParam);
    data.send.idsCount = 17;
    Require(OpenSend(open, data) == kErrParam);
    data.send.idsCount = 1;
    data.send.ids = nullptr;
    Require(OpenSend(open, data) == kErrArgNull);
    data.send.ids = rejectedIds;
    Require(OpenSend(open, data) == 0);

    data = MakeSend(nullptr);
    data.send.addressType = kAddressUserSelectEnable;
    data.send.idsCount = 0;
    Require(OpenSend(open, data) == kErrParam);
    data.send.ids = reinterpret_cast<const void*>(std::uintptr_t{17});
    Require(OpenSend(open, data) == kErrParam);
    data.send.ids = reinterpret_cast<const void*>(std::uintptr_t{16});
    Require(OpenSend(open, data) == 0);

    data = MakeSend(ids);
    data.send.dataNameMultiLangNum = 31;
    Require(OpenSend(open, data) == kErrParam);
    data.send.dataNameMultiLangNum = 30;
    data.send.dataDetailMultiLangNum = 31;
    Require(OpenSend(open, data) == kErrParam);
    data.send.dataDetailMultiLangNum = 30;
    Require(OpenSend(open, data) == 0);
}

void RequireVariant(OpenFn open, GetResultFn getResult, const void* ids, const void* rejectedIds, std::size_t sentListBytes) {
    Result result{};
    Require(open(nullptr) == kErrNotInitialized);
    Require(getResult(nullptr) == kErrArgNull);
    Require(getResult(&result) == kErrNotInitialized);

    Require(sceGameCustomDataDialogInitialize() == 0);
    Require(sceGameCustomDataDialogInitialize() == kErrAlreadyInitialized);
    Require(sceGameCustomDataDialogClose() == kErrNotRunning);
    Require(getResult(&result) == kErrNotFinished);
    result.reserved[31] = 1;
    Require(getResult(&result) == kErrParam);
    result.reserved[31] = 0;

    DataParam recv{};
    int callbackTarget = 0;
    Param param = MakeParam(kModeRecv, &recv, &callbackTarget);
    Require(open(nullptr) == kErrArgNull);
    param.size = sizeof(Param) - 1;
    Require(open(&param) == kErrParam);
    param.size = sizeof(Param);
    param.reserved[63] = 1;
    Require(open(&param) == kErrParam);
    param.reserved[63] = 0;
    param.dataParam = nullptr;
    Require(open(&param) == kErrArgNull);
    param.dataParam = &recv;
    param.userId = USER_SERVICE_USER_ID_INVALID;
    Require(open(&param) == kErrParam);
    param.userId = USER_SERVICE_INITIAL_USER_ID + 1;
    Require(open(&param) == kErrParam);
    param.userId = USER_SERVICE_INITIAL_USER_ID;
    param.mode = 0;
    Require(open(&param) == kErrParam);
    param.mode = 4;
    Require(open(&param) == kErrParam);
    param.mode = kModeRecv;
    recv.recvReserved[159] = 1;
    Require(open(&param) == kErrParam);
    recv.recvReserved[159] = 0;
    Require(sceGameCustomDataDialogGetStatus() == kStatusInitialized);
    Require(open(&param) == 0);
    Require(sceGameCustomDataDialogGetStatus() == kStatusFinished);
    Require(sceGameCustomDataDialogUpdateStatus() == kStatusFinished);
    Require(sceGameCustomDataDialogClose() == kErrNotRunning);

    std::vector<std::uint8_t> sent(sentListBytes + 1, 0xA5);
    std::memset(&result, 0xA5, offsetof(Result, sent));
    result.sent = sent.data();
    Require(getResult(&result) == kResultUserCanceled);
    Require(result.callbackArg == &callbackTarget);
    Require(result.errorCode == 0);
    Require(result.result == kResultUserCanceled);
    Require(result.sent == sent.data());
    for (std::size_t i = 0; i < sentListBytes; ++i) Require(sent[i] == 0);
    Require(sent[sentListBytes] == 0xA5);
    result.sent = nullptr;
    Require(getResult(&result) == kResultUserCanceled);
    errno = 71;
    Require(getResult(reinterpret_cast<void*>(0x10)) == kErrArgNull);
    Require(open(reinterpret_cast<const void*>(0x10)) == kErrArgNull);
    Require(errno == 71);

    RequireSendChecks(open, ids, rejectedIds);

    Require(sceGameCustomDataDialogTerminate() == 0);
    Require(sceGameCustomDataDialogTerminate() == kErrNotInitialized);
    Require(sceGameCustomDataDialogClose() == kErrNotInitialized);
    Require(open(&param) == kErrNotInitialized);
    Require(getResult(&result) == kErrNotInitialized);
    Require(sceGameCustomDataDialogGetStatus() == kStatusNone);
    Require(sceGameCustomDataDialogUpdateStatus() == kStatusNone);
}

}

int main() {
    Require(sceGameCustomDataDialogTerminate() == kErrNotInitialized);
    Require(sceGameCustomDataDialogClose() == kErrNotInitialized);
    Require(sceGameCustomDataDialogGetStatus() == kStatusNone);
    Require(sceGameCustomDataDialogUpdateStatus() == kStatusNone);

    Require(sceUserServiceInitialize(nullptr) == 0);
    RequireVariant(sceGameCustomDataDialogOpenA, sceGameCustomDataDialogGetResultA, kAccountIds, kOnlineIds, kAccountSentListBytes);
    RequireVariant(sceGameCustomDataDialogOpen, sceGameCustomDataDialogGetResult, kOnlineIds, kEmptyOnlineId, kOnlineSentListBytes);
}
