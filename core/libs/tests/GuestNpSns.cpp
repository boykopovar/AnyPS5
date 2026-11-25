#include "prx/libSceNpSns/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kErrInvalidArg = static_cast<int>(0x80550003);
constexpr int kErrInvalidSize = static_cast<int>(0x80550011);
constexpr int kErrAborted = static_cast<int>(0x80550012);
constexpr int kErrRequestMax = static_cast<int>(0x80550013);
constexpr int kErrIdNotFound = static_cast<int>(0x80550014);
constexpr int kErrFbInvalidArg = static_cast<int>(0x80552602);
constexpr int kErrYtInvalidArg = static_cast<int>(0x80552d02);
constexpr int kErrYtRequestMax = static_cast<int>(0x80552d04);
constexpr int kErrYtAborted = static_cast<int>(0x80552d06);

void TestInternalLifecycle() {
    Require(sceNpSnsIntAbortRequest(0) == kErrInvalidArg);
    Require(sceNpSnsIntAbortRequest(0x60009999) == kErrIdNotFound);
    Require(sceNpSnsIntDeleteRequest(0) == kErrIdNotFound);
    Require(sceNpSnsIntDeleteRequest(0x60009999) == kErrIdNotFound);

    std::vector<int> ids;
    for (int i = 0; i < 32; ++i) {
        const int id = sceNpSnsIntCreateRequest();
        Require(id >= 0x60000001);
        ids.push_back(id);
    }
    Require(sceNpSnsIntCreateRequest() == kErrRequestMax);

    Require(sceNpSnsIntAbortRequest(ids[0]) == 0);
    std::array<std::uint8_t, 64> outBuf{};
    Require(sceNpSnsIntLinkedStatus(ids[0], 1, 0, outBuf.data()) == kErrAborted);
    Require(sceNpSnsIntFbGetSystemAccessToken(ids[0], 1, outBuf.data()) == kErrAborted);
    Require(sceNpSnsIntTwGetSystemAccessToken(ids[0], 1, outBuf.data()) == kErrAborted);
    Require(sceNpSnsIntYtRefreshMasterToken(ids[0], 1, outBuf.data()) == kErrAborted);
    Require(sceNpSnsIntUnlink(ids[0], 1, 0) == kErrAborted);

    for (int id : ids) {
        Require(sceNpSnsIntDeleteRequest(id) == 0);
        Require(sceNpSnsIntDeleteRequest(id) == kErrIdNotFound);
    }
}

void TestInternalValidation() {
    std::array<std::uint8_t, 64> buf{};
    int flag = 1;
    Require(sceNpSnsIntFbGetTitleInfo(nullptr, buf.data(), buf.data()) == kErrInvalidArg);
    Require(sceNpSnsIntFbGetTitleInfo(buf.data(), nullptr, buf.data()) == kErrInvalidArg);
    Require(sceNpSnsIntFbGetTitleInfo(buf.data(), buf.data(), nullptr) == kErrInvalidArg);
    Require(sceNpSnsIntLinkedStatus(1, 1, 0, nullptr) == kErrInvalidArg);
    Require(sceNpSnsIntLinkedStatus(0, 1, 0, buf.data()) == kErrIdNotFound);
    Require(sceNpSnsIntFbGetSystemAccessToken(1, 1, nullptr) == kErrInvalidArg);
    Require(sceNpSnsIntTwGetSystemAccessToken(1, 1, nullptr) == kErrInvalidArg);
    Require(sceNpSnsIntYtRefreshMasterToken(1, 1, nullptr) == kErrInvalidArg);
    Require(sceNpSnsIntUnlink(0, 1, 0) == kErrIdNotFound);

    Require(sceNpSnsIntYtGetAccessToken(0, 1, nullptr, nullptr, &flag, buf.data(), buf.data()) == kErrInvalidArg);
    Require(flag == 0);
    Require(sceNpSnsIntYtGetAccessToken(0, 1, "scope", nullptr, reinterpret_cast<int*>(0x10), buf.data(), buf.data()) == kErrInvalidArg);
    Require(sceNpSnsIntYtGetAccessToken(0x60009999, 1, "scope", nullptr, &flag, buf.data(), buf.data()) == kErrIdNotFound);

    flag = 1;
    Require(sceNpSnsIntFbGetGameAccessToken(0, 1, nullptr, buf.data(), buf.data(), nullptr, buf.data(), buf.data()) == kErrInvalidArg);
    Require(sceNpSnsIntFbGetGameAccessToken(0, 1, nullptr, nullptr, buf.data(), &flag, buf.data(), buf.data()) == kErrInvalidArg);
    Require(flag == 0);
    Require(sceNpSnsIntFbGetGameAccessToken(0, 1, nullptr, buf.data(), buf.data(), reinterpret_cast<int*>(0x10), buf.data(), buf.data()) == kErrInvalidArg);
    Require(sceNpSnsIntFbGetGameAccessTokenAllowed(0, 1, nullptr, nullptr, buf.data(), buf.data()) == kErrInvalidArg);
    Require(sceNpSnsIntFbGetGameAccessTokenAllowed(0, 1, nullptr, buf.data(), buf.data(), nullptr) == kErrInvalidArg);
}

void TestFacebookAndTwitch() {
    Require(sceNpSnsFacebookAbortRequest(0) == kErrInvalidArg);
    Require(sceNpSnsFacebookAbortRequest(0x60009999) == kErrFbInvalidArg);

    std::vector<int> twitchIds;
    for (int i = 0; i < 16; ++i) {
        const int id = sceNpSnsTwitchCreateRequest();
        Require(id >= 0x60000001);
        twitchIds.push_back(id);
    }
    Require(sceNpSnsTwitchCreateRequest() == kErrRequestMax);

    std::array<std::uint8_t, 0x30> param{};
    std::array<std::uint8_t, 0x80> token{};
    Require(sceNpSnsTwitchGetAccessToken(twitchIds[0], nullptr, token.data()) == kErrInvalidArg);
    Require(sceNpSnsTwitchGetAccessToken(twitchIds[0], param.data(), nullptr) == kErrInvalidArg);
    Require(sceNpSnsTwitchGetAccessToken(twitchIds[0], reinterpret_cast<const void*>(0x10), token.data()) == kErrInvalidArg);
    Require(sceNpSnsTwitchGetAccessToken(twitchIds[0], param.data(), token.data()) == kErrInvalidSize);

    const std::uint64_t validSize = 0x30;
    std::memcpy(param.data(), &validSize, sizeof(validSize));
    param[0x0c] = 1;
    Require(sceNpSnsTwitchGetAccessToken(twitchIds[0], param.data(), token.data()) == kErrInvalidArg);
    param[0x0c] = 0;

    Require(sceNpSnsTwitchGetAccessToken(0, param.data(), token.data()) == kErrIdNotFound);
    Require(sceNpSnsTwitchAbortRequest(twitchIds[0]) == 0);
    Require(sceNpSnsTwitchGetAccessToken(twitchIds[0], param.data(), token.data()) == kErrAborted);
    Require(sceNpSnsTwitchAbortRequest(0) == kErrInvalidArg);
    Require(sceNpSnsTwitchAbortRequest(0x60009999) == kErrIdNotFound);

    for (int id : twitchIds) {
        Require(sceNpSnsTwitchDeleteRequest(id) == 0);
    }
    Require(sceNpSnsTwitchDeleteRequest(twitchIds[0]) == kErrIdNotFound);
}

void TestYouTube() {
    std::vector<int> ytIds;
    for (int i = 0; i < 16; ++i) {
        const int id = sceNpSnsYouTubeCreateRequest();
        Require(id >= 0x60000001);
        ytIds.push_back(id);
    }
    Require(sceNpSnsYouTubeCreateRequest() == kErrYtRequestMax);

    std::vector<std::uint8_t> param(0xc30, 0);
    std::array<std::uint8_t, 0x80> token{};
    Require(sceNpSnsYouTubeGetAccessToken(ytIds[0], nullptr, token.data()) == kErrYtInvalidArg);
    Require(sceNpSnsYouTubeGetAccessToken(ytIds[0], param.data(), nullptr) == kErrYtInvalidArg);
    Require(sceNpSnsYouTubeGetAccessToken(ytIds[0], reinterpret_cast<const void*>(0x10), token.data()) == kErrYtInvalidArg);
    Require(sceNpSnsYouTubeGetAccessToken(ytIds[0], param.data(), token.data()) == kErrInvalidSize);

    const std::uint64_t validSize = 0xc30;
    std::memcpy(param.data(), &validSize, sizeof(validSize));
    Require(sceNpSnsYouTubeGetAccessToken(ytIds[0], param.data(), token.data()) == kErrYtInvalidArg);
    param[0x0c] = 'a';
    param[0x0c0c] = 1;
    Require(sceNpSnsYouTubeGetAccessToken(ytIds[0], param.data(), token.data()) == kErrYtInvalidArg);
    param[0x0c0c] = 0;

    Require(sceNpSnsYouTubeGetAccessToken(0, param.data(), token.data()) == kErrYtInvalidArg);
    Require(sceNpSnsYouTubeAbortRequest(ytIds[0]) == 0);
    Require(sceNpSnsYouTubeGetAccessToken(ytIds[0], param.data(), token.data()) == kErrYtAborted);
    Require(sceNpSnsYouTubeAbortRequest(0) == kErrInvalidArg);
    Require(sceNpSnsYouTubeAbortRequest(0x60009999) == kErrYtInvalidArg);

    for (int id : ytIds) {
        Require(sceNpSnsYouTubeDeleteRequest(id) == 0);
    }
    Require(sceNpSnsYouTubeDeleteRequest(ytIds[0]) == kErrYtInvalidArg);
}

void TestCrossServiceOwnershipAndConcurrency() {
    const int intId = sceNpSnsIntCreateRequest();
    const int ytId = sceNpSnsYouTubeCreateRequest();
    Require(intId >= 0x60000001 && ytId >= 0x60000001);
    Require(sceNpSnsTwitchDeleteRequest(intId) == kErrIdNotFound);
    Require(sceNpSnsTwitchDeleteRequest(ytId) == kErrIdNotFound);
    Require(sceNpSnsYouTubeDeleteRequest(intId) == kErrYtInvalidArg);
    Require(sceNpSnsYouTubeDeleteRequest(ytId) == 0);
    Require(sceNpSnsIntDeleteRequest(intId) == 0);

    std::vector<int> twitchIds;
    for (int i = 0; i < 16; ++i) {
        const int id = sceNpSnsTwitchCreateRequest();
        Require(id >= 0x60000001);
        twitchIds.push_back(id);
    }
    const int extraIntId = sceNpSnsIntCreateRequest();
    Require(extraIntId >= 0x60000001);
    Require(sceNpSnsTwitchCreateRequest() == kErrRequestMax);
    Require(sceNpSnsTwitchDeleteRequest(extraIntId) == kErrIdNotFound);
    Require(sceNpSnsTwitchCreateRequest() == kErrRequestMax);
    Require(sceNpSnsIntDeleteRequest(extraIntId) == 0);
    for (int id : twitchIds) {
        Require(sceNpSnsTwitchDeleteRequest(id) == 0);
    }

    std::mutex resultsMutex;
    std::vector<int> created;
    std::vector<std::thread> workers;
    for (int t = 0; t < 4; ++t) {
        workers.emplace_back([&]() {
            for (int i = 0; i < 16; ++i) {
                const int id = sceNpSnsTwitchCreateRequest();
                if (id > 0) {
                    std::lock_guard<std::mutex> lock(resultsMutex);
                    created.push_back(id);
                } else {
                    Require(id == kErrRequestMax);
                }
            }
        });
    }
    for (auto& w : workers) w.join();
    Require(created.size() == 16);
    for (int id : created) {
        Require(sceNpSnsTwitchDeleteRequest(id) == 0);
    }
}

}

int main() {
    TestInternalLifecycle();
    TestInternalValidation();
    TestFacebookAndTwitch();
    TestYouTube();
    TestCrossServiceOwnershipAndConcurrency();
}