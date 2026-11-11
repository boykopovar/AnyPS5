#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <stdexcept>
#include <thread>

static void Require(bool value) { if (!value) std::abort(); }

template<typename TException, typename TCall>
static void RequireThrows(TCall call) {
    try {
        call();
    } catch (const TException&) {
        return;
    }
    std::abort();
}

constexpr int SCE_OK = 0;
constexpr int success = 0;
constexpr int detached = 1;
constexpr std::intptr_t threadResult = 7;

static void* APS5_VABI RecordEntry(void* argument) {
    static_cast<std::atomic<int>*>(argument)->store(1);
    return reinterpret_cast<void*>(threadResult);
}

static void WaitForStart(const std::atomic<int>& started) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (started.load() == 0) {
        Require(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

static void StartAndJoin() {
    std::atomic<int> started{0};
    void* thread = nullptr;
    Require(_Thrd_start_nid_postfix(&thread, RecordEntry, &started) == success);
    Require(thread != nullptr);
    WaitForStart(started);
    int code = 0;
    Require(_Thrd_join_nid_postfix(thread, &code) == success);
    Require(code == static_cast<int>(threadResult));
}

static void StartWithName() {
    std::atomic<int> started{0};
    void* thread = nullptr;
    Require(_Thrd_start_with_name_nid_postfix(&thread, RecordEntry, &started, "named") == success);
    Require(thread != nullptr);
    char name[32]{};
    Require(scePthreadGetname(static_cast<Pthread>(thread), name) == SCE_OK);
    Require(std::string(name) == "named");
    WaitForStart(started);
    int code = 0;
    Require(_Thrd_join_nid_postfix(thread, &code) == success);
    Require(code == static_cast<int>(threadResult));

    void* anonymousThread = nullptr;
    std::atomic<int> anonymousStarted{0};
    Require(_Thrd_start_with_name_nid_postfix(&anonymousThread, RecordEntry, &anonymousStarted, "") == success);
    Require(anonymousThread != nullptr);
    char anonymous[32]{};
    Require(scePthreadGetname(static_cast<Pthread>(anonymousThread), anonymous) == SCE_OK);
    Require(std::string(anonymous) != "named");
    WaitForStart(anonymousStarted);
    Require(_Thrd_join_nid_postfix(anonymousThread, &code) == success);
}

static void StartWithAttribute() {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    std::atomic<int> started{0};
    void* thread = nullptr;
    Require(_Thrd_start_with_attr_nid_postfix(&thread, RecordEntry, &started, &attr) == success);
    Require(thread != nullptr);
    WaitForStart(started);
    int code = 0;
    Require(_Thrd_join_nid_postfix(thread, &code) == success);
    Require(code == static_cast<int>(threadResult));
    scePthreadAttrDestroy(&attr);
}

static void StartDetached() {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(scePthreadAttrSetdetachstate(&attr, detached) == SCE_OK);
    int state = 0;
    Require(scePthreadAttrGetdetachstate(&attr, &state) == SCE_OK && state == detached);
    std::atomic<int> started{0};
    void* thread = reinterpret_cast<void*>(static_cast<std::uintptr_t>(1));
    Require(_Thrd_start_with_name_attr_nid_postfix(&thread, RecordEntry, &started, "detached", &attr) == success);
    Require(thread == nullptr);
    WaitForStart(started);
    scePthreadAttrDestroy(&attr);
}

static void CreateThread() {
    std::atomic<int> started{0};
    void* thread = nullptr;
    Require(_Thrd_create_nid_postfix(&thread, RecordEntry, &started) == success);
    Require(thread != nullptr);
    int code = 0;
    Require(_Thrd_join_nid_postfix(thread, &code) == success);
    Require(code == static_cast<int>(threadResult));
}

static void CppEntryPoints() {
    int code = 0;
    std::atomic<int> namedStarted{0};
    void* named = nullptr;
    Require(_ZNSt8__sce_v221_Thrd_start_with_nameEPP7pthreadPFPvS3_ES3_PKc_nid_postfix(&named, RecordEntry, &namedStarted, "cpp") == success);
    char name[32]{};
    Require(scePthreadGetname(static_cast<Pthread>(named), name) == SCE_OK && std::string(name) == "cpp");
    WaitForStart(namedStarted);
    Require(_Thrd_join_nid_postfix(named, &code) == success);

    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    std::atomic<int> attributedStarted{0};
    void* attributed = nullptr;
    Require(_ZNSt8__sce_v221_Thrd_start_with_attrEPP7pthreadPFPvS3_ES3_PP12pthread_attr_nid_postfix(&attributed, RecordEntry, &attributedStarted, &attr) == success);
    Require(attributed != nullptr);
    WaitForStart(attributedStarted);
    Require(_Thrd_join_nid_postfix(attributed, &code) == success);
    scePthreadAttrDestroy(&attr);

    PthreadAttr detachedAttr = nullptr;
    Require(scePthreadAttrInit(&detachedAttr) == SCE_OK);
    Require(scePthreadAttrSetdetachstate(&detachedAttr, detached) == SCE_OK);
    std::atomic<int> detachedStarted{0};
    void* detachedThread = reinterpret_cast<void*>(static_cast<std::uintptr_t>(1));
    Require(_ZNSt8__sce_v226_Thrd_start_with_name_attrEPP7pthreadPFPvS3_ES3_PKcPP12pthread_attr_nid_postfix(&detachedThread, RecordEntry, &detachedStarted, "cpp-detached", &detachedAttr) == success);
    Require(detachedThread == nullptr);
    WaitForStart(detachedStarted);
    scePthreadAttrDestroy(&detachedAttr);
}

static void InvalidArguments() {
    RequireThrows<std::invalid_argument>([] { _Thrd_start_nid_postfix(nullptr, RecordEntry, nullptr); });
    RequireThrows<std::invalid_argument>([] {
        void* thread = nullptr;
        _Thrd_start_nid_postfix(&thread, nullptr, nullptr);
    });
    RequireThrows<std::invalid_argument>([] { _Thrd_create_nid_postfix(nullptr, RecordEntry, nullptr); });
    RequireThrows<std::invalid_argument>([] {
        void* thread = nullptr;
        _Thrd_create_nid_postfix(&thread, nullptr, nullptr);
    });
}

int main() {
    StartAndJoin();
    StartWithName();
    StartWithAttribute();
    StartDetached();
    CreateThread();
    CppEntryPoints();
    InvalidArguments();
}
