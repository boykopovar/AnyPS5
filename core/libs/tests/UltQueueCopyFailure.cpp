#include "prx/libSceUlt/include/Api.hpp"
#include "prx/libSceUlt/SceUltTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <source_location>
#include <thread>

namespace {

thread_local bool transferThread = false;
bool push = false;
bool throwOnFailure = false;
std::atomic<bool> failNextCopy{false};

bool QueueCopyIn(void* destination, const void* source, std::size_t bytes);
bool QueueCopyOut(void* destination, const void* source, std::size_t bytes);

}

#define GuestCopyIn_nid_no_patch QueueCopyIn
#define GuestCopyOut_nid_no_patch QueueCopyOut
#include "../prx/libSceUlt/Export.cpp"
#undef GuestCopyIn_nid_no_patch
#undef GuestCopyOut_nid_no_patch

namespace {

bool QueueCopyIn(void* destination, const void* source, std::size_t bytes) {
    if (transferThread && push && failNextCopy.exchange(false)) {
        if (throwOnFailure) {
            errno = 5;
            throw std::runtime_error("guest copy exception");
        }
        source = reinterpret_cast<const void*>(0x10);
    }
    return GuestCopyIn_nid_no_patch(destination, source, bytes);
}

bool QueueCopyOut(void* destination, const void* source, std::size_t bytes) {
    if (transferThread && !push && failNextCopy.exchange(false)) {
        if (throwOnFailure) {
            errno = 5;
            throw std::runtime_error("guest copy exception");
        }
        destination = reinterpret_cast<void*>(0x10);
    }
    return GuestCopyOut_nid_no_patch(destination, source, bytes);
}

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "ULT queue copy failure check failed at line %u\n", location.line());
    std::abort();
}

struct alignas(8) Object { std::array<std::byte, 512> bytes{}; };

bool Waiting(void* queue) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    do {
        UltQueueInfo info{};
        Require(sceUltGetQueueInfo(queue, &info, nullptr, 0) == ULT_OK);
        if ((push ? info.waitingPushers : info.waitingPoppers) == 2) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
}

bool Completed(std::array<std::future<int>, 2>& results) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    do {
        if (results[0].wait_for(std::chrono::seconds(0)) == std::future_status::ready &&
            results[1].wait_for(std::chrono::seconds(0)) == std::future_status::ready) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
}

void Run() {
    Object pool;
    Object queue;
    constexpr std::uint64_t value = 0x123456789ABCDEF0;
    constexpr std::uint64_t untouched = 0xFEEDFACEDEADBEEF;
    std::array<std::uint64_t, 2> output{untouched, untouched};
    Require(sceUltQueueDataResourcePoolCreate(&pool, "pool", 1, sizeof(value), 1, nullptr, nullptr, nullptr, 0) == ULT_OK);
    Require(sceUltQueueCreate(&queue, "queue", sizeof(value), nullptr, &pool, nullptr, 0) == ULT_OK);
    if (push) Require(sceUltQueuePush(&queue, &value) == ULT_OK);
    const auto operation = [&](std::size_t index) {
        transferThread = true;
        errno = 71;
        int result;
        try {
            result = push ? sceUltQueuePush(&queue, &value) : sceUltQueuePop(&queue, &output[index]);
        } catch (const std::runtime_error& error) {
            Require(throwOnFailure && std::strcmp(error.what(), "guest copy exception") == 0);
            result = ULT_ERROR_NULL;
        }
        Require(errno == 71);
        return result;
    };
    std::array<std::future<int>, 2> results{
        std::async(std::launch::async, operation, 0),
        std::async(std::launch::async, operation, 1)
    };
    if (!Waiting(&queue)) {
        Require(sceUltFinalize() == ULT_OK);
        for (auto& result : results) result.get();
        Require(false);
    }
    failNextCopy.store(true);
    if (push) {
        std::uint64_t removed = 0;
        Require(sceUltQueuePop(&queue, &removed) == ULT_OK && removed == value);
    } else {
        Require(sceUltQueuePush(&queue, &value) == ULT_OK);
    }
    const bool completed = Completed(results);
    if (!completed) Require(sceUltFinalize() == ULT_OK);
    const int first = results[0].get();
    const int second = results[1].get();
    Require(completed);
    Require((first == ULT_OK && second == ULT_ERROR_NULL) || (first == ULT_ERROR_NULL && second == ULT_OK));
    Require(!failNextCopy.load());
    UltQueueInfo info{};
    Require(sceUltGetQueueInfo(&queue, &info, nullptr, 0) == ULT_OK);
    Require(info.waitingPushers == 0 && info.waitingPoppers == 0);
    if (push) {
        Require(info.queuedItems == 1);
        std::uint64_t saved = 0;
        Require(sceUltQueuePop(&queue, &saved) == ULT_OK && saved == value);
    } else {
        Require(info.queuedItems == 0);
        Require(output[first == ULT_OK ? 0 : 1] == value);
        Require(output[first == ULT_ERROR_NULL ? 0 : 1] == untouched);
    }
    Require(sceUltQueueDestroy(&queue) == ULT_OK);
    Require(sceUltQueueDataResourcePoolDestroy(&pool) == ULT_OK);
}

}

int main(int argc, char** argv) {
    Require(argc == 2);
    Require(std::strcmp(argv[1], "push") == 0 || std::strcmp(argv[1], "pop") == 0 ||
            std::strcmp(argv[1], "push-exception") == 0 || std::strcmp(argv[1], "pop-exception") == 0);
    push = std::strncmp(argv[1], "push", 4) == 0;
    throwOnFailure = std::strstr(argv[1], "exception") != nullptr;
    Run();
}
