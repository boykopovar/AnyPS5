#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Http2 {

using LookupHook = int (*)(const char* host);

inline std::atomic<LookupHook>& LookupHookSlot() {
    static std::atomic<LookupHook> hook{nullptr};
    return hook;
}

struct Settings {
    std::string userAgent;
    std::vector<std::pair<std::string, std::string>> headers;
    std::uint64_t contentLength = 0;
    bool noContentLength = false;
    bool auth = true;
    int redirect = 1;
    std::uint32_t verifyFlags = 0x3d;
    std::uint32_t timeout = 120000000;
    std::uint32_t connectTimeout = 30000000;
    std::int32_t resolveRetry = 0;
    std::uint32_t sendTimeout = 120000000;
    std::uint32_t recvTimeout = 120000000;
    std::uint32_t resolveTimeout = 30000000;
    void* authInfoCallback = nullptr;
    void* authInfoArgument = nullptr;
    void* preSendCallback = nullptr;
    void* preSendArgument = nullptr;
};

struct Capacity {
    explicit Capacity(int maximum) : maximum(maximum) {}
    const int maximum;
    std::atomic<int> active{0};
};

class Request {
public:
    Request(int sslContext, std::shared_ptr<Capacity> capacity, Settings settings, std::string method, std::string url);
    ~Request();
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;
    int Send(const void* data, std::size_t size);
    int Read(void* data, std::size_t size);
    int Status(int* status);
    int ContentLength(int* kind, std::uint64_t* length);
    void Cancel();
    void Stop();
private:
    struct State;
    std::unique_ptr<State> state;
};

int AddHeader(Settings& settings, const char* name, const char* value, std::uint32_t mode);
std::shared_ptr<Capacity> CreateCapacity_nid_no_patch(int maximum);
std::shared_ptr<Request> CreateRequest_nid_no_patch(int sslContext, std::shared_ptr<Capacity> capacity, Settings settings, std::string method, std::string url);

}

extern "C" void Http2SetLookupHook_nid_no_patch(Http2::LookupHook hook);
