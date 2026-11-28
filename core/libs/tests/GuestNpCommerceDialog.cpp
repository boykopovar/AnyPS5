#include "prx/libSceNpCommerce/include/Api.hpp"
#include "ProcessParametersFixture.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include "prx/libSceIme/include/ModalDialog.hpp"
#include "prx/libSceIme/include/PsnDialogNotice.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <stdexcept>
#include <thread>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool value) { if (!value) std::abort(); }

alignas(16) std::array<std::byte, 64> process{};

constexpr int statusNone = 0;
constexpr int statusInitialized = 1;
constexpr int statusFinished = 3;
constexpr int errorAlreadyInitialized = static_cast<int>(0x80B80004u);
constexpr std::int32_t resultUserCanceled = 1;

void RequireCanceledResult(std::uint64_t userData, bool clearsReserved) {
    NpCommerceDialogResult result;
    std::memset(&result, 0x33, sizeof(result));
    Require(sceNpCommerceDialogGetResult(reinterpret_cast<NpCommerceDialogResult*>(0x10)) == static_cast<int>(0x80B8000Au));
    Require(sceNpCommerceDialogGetResult(&result) == resultUserCanceled);
    Require(result.result == resultUserCanceled);
    Require(result.authorized == 0);
    Require(result.userData == userData);
    for (const auto value : result.padding) Require(value == 0x33);
    for (const auto value : result.reserved) Require(value == (clearsReserved ? 0 : 0x33));
}


bool PsnNoticeShown() {
    ModalDialogView view;
    if (!ModalDialogReadView_nid_postfix(view)) return false;
    return view.message == PsnDialogNoticeRequest(PsnDialogNoticeKind::PsnFeature).message;
}

void RequireNotices() {
    std::uint32_t sdkVersion = 0x03500000;
    std::memcpy(process.data() + 16, &sdkVersion, sizeof(sdkVersion));
    NpCommerceDialogParam param{};
    param.base.size = sizeof(param.base);
    param.base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&param)) - 1060003575u;
    param.size = sizeof(param);
    param.userId = USER_SERVICE_INITIAL_USER_ID;
    param.mode = 5;
    param.argument = 1;
    NpCommerceDialogParam2 param2{};
    param2.base.size = sizeof(param2.base);
    param2.base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&param2)) - 1060003575u;
    param2.size = sizeof(param2);
    param2.userId = USER_SERVICE_INITIAL_USER_ID;
    param2.mode = 5;
    param2.argument = 1;

    ModalDialogSetPresenter_nid_postfix(true);
    sceNpCommerceDialogTerminate();
    Require(sceNpCommerceDialogInitialize() == 0);

    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Silent);
    ModalDialogReset_nid_postfix(ModalDialogAnyOwner);
    Require(sceNpCommerceDialogOpen(&param) == 0);
    Require(!PsnNoticeShown() && !ModalDialogCapturesInput_nid_postfix());

    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Show);
    ModalDialogReset_nid_postfix(ModalDialogAnyOwner);
    Require(sceNpCommerceDialogOpen(nullptr) == static_cast<int>(0x80B8000Du));
    errno = 71;
    Require(sceNpCommerceDialogOpen(reinterpret_cast<const NpCommerceDialogParam*>(0x10)) == static_cast<int>(0x80B8000Du));
    Require(errno == 71);
    Require(!PsnNoticeShown());
    Require(sceNpCommerceDialogOpen(&param) == 0);
    Require(PsnNoticeShown());

    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Show);
    ModalDialogReset_nid_postfix(ModalDialogAnyOwner);
    Require(sceNpCommerceDialogOpen2(&param2) == 0);
    Require(PsnNoticeShown());

    ModalDialogReset_nid_postfix(ModalDialogAnyOwner);
    ModalDialogSetPresenter_nid_postfix(false);
    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Silent);
    Require(sceNpCommerceDialogTerminate() == 0);
}

class SyncBarrier {
public:
    explicit SyncBarrier(int count) : total(count) {}

    void ArriveAndWait() {
        const std::uint64_t current = generation.load(std::memory_order_acquire);
        if (arrived.fetch_add(1, std::memory_order_acq_rel) + 1 == total) {
            arrived.store(0, std::memory_order_relaxed);
            generation.fetch_add(1, std::memory_order_release);
        } else {
            unsigned spins = 0;
            while (generation.load(std::memory_order_acquire) == current) {
                if (++spins >= 256u) {
                    spins = 0;
                    std::this_thread::yield();
                }
            }
        }
    }

private:
    const int total;
    std::atomic<int> arrived{0};
    std::atomic<std::uint64_t> generation{0};
};

void RequireConcurrentLifecycle() {
    std::uint32_t sdkVersion = 0x03500000;
    std::memcpy(process.data() + 16, &sdkVersion, sizeof(sdkVersion));
    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Silent);
    Require(sceNpCommerceDialogGetStatus() == statusNone);

    const int threadCount = std::max(4, std::min(8, static_cast<int>(std::thread::hardware_concurrency())));
    constexpr int rounds = 2048;
    SyncBarrier barrier(threadCount + 1);
    std::vector<int> initResults(threadCount, -1);
    std::vector<int> termResults(threadCount, -1);
    std::vector<std::thread> lifecycleThreads;
    lifecycleThreads.reserve(threadCount);
    for (int i = 0; i < threadCount; ++i) {
        lifecycleThreads.emplace_back([&, i]() {
            for (int round = 0; round < rounds; ++round) {
                barrier.ArriveAndWait();
                initResults[i] = ((i + round) % 2 == 0) ? sceNpCommerceDialogInitialize() : sceNpCommerceDialogInitializeInternal();
                barrier.ArriveAndWait();
                barrier.ArriveAndWait();
                termResults[i] = sceNpCommerceDialogTerminate();
                barrier.ArriveAndWait();
            }
        });
    }

    for (int round = 0; round < rounds; ++round) {
        std::fill(initResults.begin(), initResults.end(), -1);
        barrier.ArriveAndWait();
        barrier.ArriveAndWait();

        int initSuccessCount = 0;
        int alreadyInitCount = 0;
        for (int rc : initResults) {
            if (rc == 0) {
                ++initSuccessCount;
            } else {
                Require(rc == COMMON_DIALOG_ERROR_ALREADY_INITIALIZED);
                ++alreadyInitCount;
            }
        }
        Require(initSuccessCount == 1);
        Require(alreadyInitCount == threadCount - 1);
        Require(sceNpCommerceDialogGetStatus() == statusInitialized);

        std::fill(termResults.begin(), termResults.end(), -1);
        barrier.ArriveAndWait();
        barrier.ArriveAndWait();

        int termSuccessCount = 0;
        int notInitCount = 0;
        for (int rc : termResults) {
            if (rc == 0) {
                ++termSuccessCount;
            } else {
                Require(rc == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
                ++notInitCount;
            }
        }
        Require(termSuccessCount == 1);
        Require(notInitCount == threadCount - 1);
        Require(sceNpCommerceDialogGetStatus() == statusNone);
    }
    for (auto& thread : lifecycleThreads) thread.join();

    Require(sceNpCommerceDialogInitialize() == 0);
    {
        NpCommerceDialogParam2 throwParam{};
        throwParam.base.size = sizeof(throwParam.base);
        throwParam.base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&throwParam)) - 1060003575u;
        throwParam.size = sizeof(throwParam);
        throwParam.userId = USER_SERVICE_INITIAL_USER_ID;
        throwParam.mode = 2;
        throwParam.contentMode = 1;
        bool threw = false;
        try {
            sceNpCommerceDialogOpen2(&throwParam);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw);
        Require(sceNpCommerceDialogGetStatus() == statusInitialized);
    }

    constexpr std::array<std::uint64_t, 4> sentinels{
        0x1122334455667788ull,
        0x2233445566778899ull,
        0x99aabbccddeeff00ull,
        0x8899aabbccddeeffull,
    };
    {
        NpCommerceDialogParam seedParam{};
        seedParam.base.size = sizeof(seedParam.base);
        seedParam.base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&seedParam)) - 1060003575u;
        seedParam.size = sizeof(seedParam);
        seedParam.userId = USER_SERVICE_INITIAL_USER_ID;
        seedParam.mode = 5;
        seedParam.argument = 1;
        seedParam.userData = sentinels[0];
        Require(sceNpCommerceDialogOpen(&seedParam) == 0);
    }

    constexpr int iterations = 200;
    SyncBarrier startGate(9);
    std::atomic<int> resultChecks{0};
    std::vector<std::thread> workers;
    workers.reserve(8);

    for (int i = 0; i < 2; ++i) {
        workers.emplace_back([&, i]() {
            NpCommerceDialogParam param{};
            param.base.size = sizeof(param.base);
            param.base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&param)) - 1060003575u;
            param.size = sizeof(param);
            param.userId = USER_SERVICE_INITIAL_USER_ID;
            param.mode = 5;
            param.argument = 1;
            param.userData = sentinels[i];
            startGate.ArriveAndWait();
            for (int iter = 0; iter < iterations; ++iter) {
                Require(sceNpCommerceDialogOpen(&param) == 0);
            }
        });
    }

    for (int i = 0; i < 2; ++i) {
        workers.emplace_back([&, i]() {
            NpCommerceDialogParam2 param2{};
            param2.base.size = sizeof(param2.base);
            param2.base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&param2)) - 1060003575u;
            param2.size = sizeof(param2);
            param2.userId = USER_SERVICE_INITIAL_USER_ID;
            param2.mode = 5;
            param2.argument = 1;
            param2.userData = sentinels[i + 2];
            startGate.ArriveAndWait();
            for (int iter = 0; iter < iterations; ++iter) {
                Require(sceNpCommerceDialogOpen2(&param2) == 0);
            }
        });
    }

    for (int i = 0; i < 2; ++i) {
        workers.emplace_back([&]() {
            startGate.ArriveAndWait();
            for (int iter = 0; iter < iterations; ++iter) {
                const int status = sceNpCommerceDialogGetStatus();
                Require(status == statusInitialized || status == statusFinished);
                const int updated = sceNpCommerceDialogUpdateStatus();
                Require(updated == statusInitialized || updated == statusFinished);
            }
        });
    }

    for (int i = 0; i < 2; ++i) {
        workers.emplace_back([&]() {
            startGate.ArriveAndWait();
            for (int iter = 0; iter < iterations; ++iter) {
                NpCommerceDialogResult result;
                std::memset(&result, 0x33, sizeof(result));
                const int rc = sceNpCommerceDialogGetResult(&result);
                if (rc == static_cast<int>(0x80B80005u)) continue;
                Require(rc == resultUserCanceled);
                Require(result.result == resultUserCanceled);
                Require(result.authorized == 0);
                bool matched = false;
                for (std::uint64_t sentinel : sentinels) {
                    if (result.userData == sentinel) {
                        matched = true;
                        break;
                    }
                }
                Require(matched);
                for (const auto value : result.padding) Require(value == 0x33);
                for (const auto value : result.reserved) Require(value == 0);
                resultChecks.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    startGate.ArriveAndWait();
    for (auto& worker : workers) worker.join();

    Require(sceNpCommerceDialogGetStatus() == statusFinished);
    NpCommerceDialogResult finalResult;
    std::memset(&finalResult, 0x33, sizeof(finalResult));
    Require(sceNpCommerceDialogGetResult(&finalResult) == resultUserCanceled);
    bool finalMatched = false;
    for (std::uint64_t sentinel : sentinels) {
        if (finalResult.userData == sentinel) {
            finalMatched = true;
            break;
        }
    }
    Require(finalMatched);
    for (const auto value : finalResult.padding) Require(value == 0x33);
    for (const auto value : finalResult.reserved) Require(value == 0);
    Require(resultChecks.load(std::memory_order_relaxed) == 2 * iterations);
    Require(sceNpCommerceDialogTerminate() == 0);
    Require(sceNpCommerceDialogGetStatus() == statusNone);
}

}

int main() {
    Require(ProcessParametersFixture::Install(process.data(), process.size()));
    std::uint32_t sdkVersion = 0x03500000;
    std::memcpy(process.data() + 16, &sdkVersion, sizeof(sdkVersion));
    Require(sceUserServiceInitialize(nullptr) == 0);
    NpCommerceDialogParam2 param2{};
    param2.base.size = sizeof(param2.base);
    param2.base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&param2)) - 1060003575u;
    param2.size = sizeof(param2);
    param2.userId = USER_SERVICE_INITIAL_USER_ID;
    param2.userData = 0x123456789abcdef0;
    Require(sceNpCommerceDialogGetStatus() == statusNone);
    Require(sceNpCommerceDialogOpen(nullptr) == static_cast<int>(0x80B8000Du));
    Require(sceNpCommerceDialogOpen2(nullptr) == static_cast<int>(0x80B8000Du));
    errno = 71;
    Require(sceNpCommerceDialogOpen(reinterpret_cast<const NpCommerceDialogParam*>(0x10)) == static_cast<int>(0x80B8000Du));
    Require(errno == 71);
    errno = 71;
    Require(sceNpCommerceDialogOpen2(reinterpret_cast<const NpCommerceDialogParam2*>(0x10)) == static_cast<int>(0x80B8000Du));
    Require(errno == 71);
    Require(sceNpCommerceDialogOpen2(&param2) == static_cast<int>(0x80B80003u));
    param2.base.magic = 0;
    Require(sceNpCommerceDialogOpen2(&param2) == static_cast<int>(0x80B8000Au));
    param2.base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&param2)) - 1060003575u;
    param2.base.reserved[35] = 1;
    Require(sceNpCommerceDialogOpen2(&param2) == static_cast<int>(0x80B8000Au));
    param2.base.reserved[35] = 0;
    Require(sceNpCommerceDialogTerminate() == static_cast<int>(0x80B80003u));
    Require(sceNpCommerceDialogClose() == static_cast<int>(0x80B80003u));
    Require(sceNpCommerceDialogGetResult(nullptr) == static_cast<int>(0x80B8000Du));
    NpCommerceDialogResult result{};
    Require(sceNpCommerceDialogGetResult(&result) == static_cast<int>(0x80B80003u));

    Require(sceNpCommerceDialogInitializeInternal() == 0);
    Require(sceNpCommerceDialogGetStatus() == statusInitialized);
    Require(sceNpCommerceDialogInitializeInternal() == errorAlreadyInitialized);
    Require(sceNpCommerceDialogInitialize() == errorAlreadyInitialized);
    Require(sceNpCommerceDialogGetStatus() == statusInitialized);
    Require(sceNpCommerceDialogTerminate() == 0);
    Require(sceNpCommerceDialogGetStatus() == statusNone);
    Require(sceNpCommerceDialogInitialize() == 0);
    Require(sceNpCommerceDialogInitializeInternal() == errorAlreadyInitialized);
    Require(sceNpCommerceDialogGetStatus() == statusInitialized);
    Require(sceNpCommerceDialogTerminate() == 0);
    Require(sceNpCommerceDialogGetStatus() == statusNone);
    Require(sceNpCommerceDialogInitialize() == 0);
    Require(sceNpCommerceDialogClose() == static_cast<int>(0x80B8000Bu));
    Require(sceNpCommerceDialogGetResult(&result) == static_cast<int>(0x80B80005u));
    Require(sceNpCommerceDialogGetResult(nullptr) == static_cast<int>(0x80B8000Du));
    Require(sceNpCommerceDialogGetStatus() == statusInitialized);
    errno = 71;
    Require(sceNpCommerceDialogOpen(reinterpret_cast<const NpCommerceDialogParam*>(0x10)) == static_cast<int>(0x80B8000Du));
    Require(errno == 71);
    errno = 71;
    Require(sceNpCommerceDialogOpen2(reinterpret_cast<const NpCommerceDialogParam2*>(0x10)) == static_cast<int>(0x80B8000Du));
    Require(errno == 71);

    Require(sceNpCommerceDialogOpen2(&param2) == 0);
    Require(sceNpCommerceDialogUpdateStatus() == statusFinished);
    Require(sceNpCommerceDialogGetStatus() == statusFinished);
    RequireCanceledResult(param2.userData, true);

    Require(sceNpCommerceDialogClose() == static_cast<int>(0x80B8000Bu));
    Require(sceNpCommerceDialogGetStatus() == statusFinished);
    Require(sceNpCommerceDialogTerminate() == 0);
    Require(sceNpCommerceDialogGetStatus() == statusNone);
    errno = 71;
    Require(sceNpCommerceDialogOpen(reinterpret_cast<const NpCommerceDialogParam*>(0x10)) == static_cast<int>(0x80B8000Du));
    Require(errno == 71);
    errno = 71;
    Require(sceNpCommerceDialogOpen2(reinterpret_cast<const NpCommerceDialogParam2*>(0x10)) == static_cast<int>(0x80B8000Du));
    Require(errno == 71);

    NpCommerceDialogParam param{};
    param.base.size = sizeof(param.base);
    param.base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&param)) - 1060003575u;
    param.size = sizeof(param);
    param.userId = USER_SERVICE_INITIAL_USER_ID;
    Require(sceNpCommerceDialogInitialize() == 0);
    param.base.size = 0;
    Require(sceNpCommerceDialogOpen(&param) == static_cast<int>(0x80B8000Au));
    param.base.size = sizeof(param.base);
    param.size = 0;
    Require(sceNpCommerceDialogOpen(&param) == static_cast<int>(0x80B8000Au));
    param.size = sizeof(param);
    param.option = 8;
    Require(sceNpCommerceDialogOpen(&param) == static_cast<int>(0x80B8000Au));
    param.option = 0;
    param.reserved[31] = 1;
    Require(sceNpCommerceDialogOpen(&param) == static_cast<int>(0x80B8000Au));
    param.reserved[31] = 0;
    param.userId = -1;
    Require(sceNpCommerceDialogOpen(&param) == static_cast<int>(0x80B8000Au));
    param.userId = USER_SERVICE_INITIAL_USER_ID;
    const char* products[11]{};
    for (std::uint32_t mode = 0; mode <= 6; ++mode) {
        param.mode = mode;
        for (std::uint32_t count = 0; count <= 11; ++count) {
            param.productCount = count;
            for (bool hasProducts : {false, true}) {
                param.products = hasProducts ? products : nullptr;
                bool valid = false;
                if (mode == 0 || mode == 2) valid = count <= 1 && hasProducts == (count != 0);
                if (mode == 1) valid = count == 1 && hasProducts;
                if (mode == 3) valid = count >= 1 && count <= 10 && hasProducts;
                if (mode == 4) valid = count <= 10 && hasProducts == (count != 0);
                Require(sceNpCommerceDialogOpen(&param) == (valid ? 0 : static_cast<int>(0x80B8000Au)));
                param2.mode = mode;
                param2.productCount = count;
                param2.products = param.products;
                Require(sceNpCommerceDialogOpen2(&param2) == (valid ? 0 : static_cast<int>(0x80B8000Au)));
            }
        }
    }
    param2.mode = 1;
    param2.contentMode = 1;
    param2.productCount = 1;
    param2.products = products;
    param2.option = 99;
    Require(sceNpCommerceDialogOpen2(&param2) == 0);
    param2.option = 100;
    Require(sceNpCommerceDialogOpen2(&param2) == static_cast<int>(0x80B8000Au));
    param2.option = 0;
    param2.mode = 2;
    bool threw = false;
    try {
        sceNpCommerceDialogOpen2(&param2);

    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    param2.contentMode = 0;
    param2.mode = 0;
    param2.productCount = 0;
    param2.products = nullptr;
    param2.reserved[31] = 1;
    Require(sceNpCommerceDialogOpen2(&param2) == static_cast<int>(0x80B8000Au));
    param2.reserved[31] = 0;
    param2.size = 0;
    Require(sceNpCommerceDialogOpen2(&param2) == static_cast<int>(0x80B8000Au));
    param2.size = sizeof(param2);
    param2.userId = -1;
    Require(sceNpCommerceDialogOpen2(&param2) == static_cast<int>(0x80B8000Au));
    param2.userId = USER_SERVICE_INITIAL_USER_ID;
    param2.userData = 0x123456789abcdef0;
    for (std::uint32_t mode : {1000u, 1001u}) {
        param.mode = mode;
        param.productCount = 0;
        param.argument = 1;
        Require(sceNpCommerceDialogOpen(&param) == static_cast<int>(0x80B8000Au));
    }
    param.mode = 5;
    param.productCount = 0;
    param.products = nullptr;
    for (std::uint64_t argument = 0; argument <= 4; ++argument) {
        param.argument = argument;
        Require(sceNpCommerceDialogOpen(&param) == (argument == 1 ? 0 : static_cast<int>(0x80B8000Au)));
    }
    sdkVersion = 0x01700000;
    std::memcpy(process.data() + 16, &sdkVersion, sizeof(sdkVersion));
    param.argument = 3;
    param.userData = 0xfedcba9876543210;
    Require(sceNpCommerceDialogOpen(&param) == 0);
    RequireCanceledResult(param.userData, true);
    sdkVersion = 0x01600000;
    std::memcpy(process.data() + 16, &sdkVersion, sizeof(sdkVersion));
    param.size = 0;
    param.option = 100;
    param.userId = -1;
    param.reserved[31] = 1;
    param.argument = 7;
    Require(sceNpCommerceDialogOpen(&param) == 0);
    RequireCanceledResult(param.userData, false);
    Require(sceNpCommerceDialogOpen2(&param2) == 0);
    Require(sceNpCommerceDialogUpdateStatus() == statusFinished);
    RequireCanceledResult(param2.userData, false);
    Require(sceNpCommerceDialogTerminate() == 0);
    Require(sceNpCommerceDialogTerminate() == static_cast<int>(0x80B80003u));
    Require(sceNpCommerceDialogClose() == static_cast<int>(0x80B80003u));
    Require(sceNpCommerceDialogOpen(nullptr) == static_cast<int>(0x80B8000Du));
    Require(sceNpCommerceDialogOpen2(nullptr) == static_cast<int>(0x80B8000Du));
    Require(sceNpCommerceDialogOpen2(&param2) == static_cast<int>(0x80B80003u));
    Require(sceNpCommerceDialogGetResult(nullptr) == static_cast<int>(0x80B8000Du));
    Require(sceNpCommerceDialogGetResult(&result) == static_cast<int>(0x80B80003u));
    Require(sceNpCommerceDialogGetStatus() == statusNone);
    Require(sceNpCommerceDialogUpdateStatus() == statusNone);
    RequireNotices();
    RequireConcurrentLifecycle();
    Require(sceNpCommerceHidePsStoreIcon() == 0);
    Require(sceNpCommerceShowPsStoreIcon(0) == 0);
    Require(sceNpCommerceShowPsStoreIcon(1) == 0);
    Require(sceNpCommerceSetPsStoreIconLayout(0) == 0);
    Require(sceNpCommerceSetPsStoreIconLayout(1) == 0);
    Require(sceUserServiceTerminate() == 0);
    return 0;
}
