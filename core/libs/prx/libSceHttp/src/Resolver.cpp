#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#endif

#include "prx/libSceHttp/src/Resolver.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <utility>

namespace {

struct ResolveJob {
    std::string host;
    std::string service;
    addrinfo hints{};
    std::uint32_t delayUsec = 0;
    std::mutex mutex;
    std::condition_variable ready;
    bool abandoned = false;
    bool complete = false;
    int result = EAI_AGAIN;
    addrinfo* addresses = nullptr;
};

std::atomic<std::uint32_t> g_testDelayUsec{0};

class HostResolver {
public:
    bool Start() noexcept {
        std::size_t started = 0;
        try {
            for (std::size_t index = 0; index < 4; ++index) {
                std::thread([this] { Run(); }).detach();
                ++started;
            }
            return true;
        } catch (...) {
            return started != 0;
        }
    }

    int Resolve(const char* host, const char* service, const addrinfo* hints, const std::uint32_t timeoutUsec,
        addrinfo** addresses) {
        if (!host || !service || !hints || !addresses) return EAI_FAIL;
        *addresses = nullptr;
        auto job = std::make_shared<ResolveJob>();
        job->host = host;
        job->service = service;
        job->hints = *hints;
        job->delayUsec = g_testDelayUsec.load();
        {
            std::lock_guard lock(queueMutex_);
            queue_.push_back(job);
        }
        queued_.notify_one();
        std::unique_lock lock(job->mutex);
        const auto done = [&job] { return job->complete; };
        if (timeoutUsec == 0) {
            job->ready.wait(lock, done);
        } else if (!job->ready.wait_for(lock, std::chrono::microseconds(timeoutUsec), done)) {
            job->abandoned = true;
            return EAI_AGAIN;
        }
        const int result = job->result;
        if (result == 0) {
            *addresses = job->addresses;
            job->addresses = nullptr;
        } else if (job->addresses) {
            freeaddrinfo(job->addresses);
            job->addresses = nullptr;
        }
        return result;
    }

private:
    void Run() {
        for (;;) {
            std::shared_ptr<ResolveJob> job;
            {
                std::unique_lock lock(queueMutex_);
                queued_.wait(lock, [this] { return !queue_.empty(); });
                job = std::move(queue_.front());
                queue_.pop_front();
            }
            {
                std::lock_guard lock(job->mutex);
                if (job->abandoned) continue;
            }
            if (job->delayUsec) std::this_thread::sleep_for(std::chrono::microseconds(job->delayUsec));
            {
                std::lock_guard lock(job->mutex);
                if (job->abandoned) continue;
            }
            addrinfo* addresses = nullptr;
            const int result = getaddrinfo(job->host.c_str(), job->service.c_str(), &job->hints, &addresses);
            {
                std::lock_guard lock(job->mutex);
                if (job->abandoned) {
                    if (addresses) freeaddrinfo(addresses);
                    continue;
                }
                job->result = result;
                job->addresses = addresses;
                job->complete = true;
            }
            job->ready.notify_one();
        }
    }

    std::mutex queueMutex_;
    std::condition_variable queued_;
    std::deque<std::shared_ptr<ResolveJob>> queue_;
};

HostResolver* GetHostResolver() noexcept {
    static HostResolver* resolver = []() noexcept {
        auto* value = new (std::nothrow) HostResolver;
        if (!value) return static_cast<HostResolver*>(nullptr);
        if (!value->Start()) {
            delete value;
            return static_cast<HostResolver*>(nullptr);
        }
        return value;
    }();
    return resolver;
}

}

int ResolveHttpHost(const char* host, const char* service, const addrinfo* hints, const std::uint32_t timeoutUsec,
    addrinfo** addresses) {
    auto* resolver = GetHostResolver();
    if (!resolver) return EAI_AGAIN;
    try {
        return resolver->Resolve(host, service, hints, timeoutUsec, addresses);
    } catch (...) {
        return EAI_AGAIN;
    }
}

void GuestHttpTest::SetResolverDelayForTests(const std::uint32_t delayUsec) noexcept {
    g_testDelayUsec.store(delayUsec);
}
