#include "prx/libc/include/general/ThreadName.hpp"
#include <string>

#ifdef _WIN32
int main() {
    constexpr const char* name = "AnyPS5 queue worker 0x2a";
    const HRESULT set = ThreadName::SetCurrent(name);
    if (set == S_FALSE) return 0;
    if (FAILED(set)) return 1;
    using GetThreadDescriptionFunction = HRESULT(WINAPI*)(HANDLE, PWSTR*);
    const auto getThreadDescription = ThreadName::Resolve<GetThreadDescriptionFunction>("GetThreadDescription");
    if (getThreadDescription == nullptr) return 0;
    PWSTR description = nullptr;
    if (FAILED(getThreadDescription(GetCurrentThread(), &description))) return 2;
    const bool matches = description != nullptr && std::wstring(description) == L"AnyPS5 queue worker 0x2a";
    if (description != nullptr) LocalFree(description);
    return matches ? 0 : 3;
}
#else
#include <atomic>
#include <fstream>
#include <thread>

namespace {

std::string CurrentName() {
    char name[32] = {};
    if (pthread_getname_np(pthread_self(), name, sizeof name) != 0) return "<error>";
    return name;
}

std::string TaskName(int tid) {
    std::ifstream file("/proc/self/task/" + std::to_string(tid) + "/comm");
    std::string name;
    std::getline(file, name);
    return name;
}

}

int main() {
    if (ThreadName::HostName("draw committer") != "draw committer") return 1;
    if (ThreadName::HostName("queue worker 0x20") != "queueworker0x20") return 2;
    if (ThreadName::HostName("AnyPS5 queue worker 0x2a") != "AnyPS5queuework") return 3;
    if (ThreadName::HostName("abcdefghijklmn\xc3\xa9") != "abcdefghijklmn") return 4;
    if (ThreadName::HostName("abcdefghijklm\xc3\xa9") != "abcdefghijklm\xc3\xa9") return 5;
    if (ThreadName::HostName("") != "") return 6;
    if (ThreadName::SetCurrent(nullptr) == 0) return 7;
    if (ThreadName::SetCurrent("queue worker 0x2a") != 0 || CurrentName() != "queueworker0x2a") return 8;
    std::atomic<int> tid{0};
    std::atomic<bool> done{false};
    std::thread other([&] {
        tid.store(static_cast<int>(gettid()), std::memory_order_release);
        while (!done.load(std::memory_order_acquire)) std::this_thread::yield();
    });
    while (tid.load(std::memory_order_acquire) == 0) std::this_thread::yield();
    const int renamed = ThreadName::SetTask(tid.load(), "GPU progress watcher");
    const auto observed = TaskName(tid.load());
    done.store(true, std::memory_order_release);
    other.join();
    if (renamed != 0 || observed != "GPUprogresswatc") return 9;
    if (ThreadName::SetTask(0, "name") == 0) return 10;
    return 0;
}
#endif
