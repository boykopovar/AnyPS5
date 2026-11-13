#pragma once

#include "prx/libSceHttp2/include/Http2.hpp"

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

using ConnectHook = int (*)(int socket, const void* address, std::uint32_t size);
using PollHook = int (*)(std::uintptr_t native, bool write, bool both);
using WriteHook = std::int64_t (*)(int socket, const void* data, std::size_t size, int flags);

inline std::atomic<ConnectHook>& ConnectHookSlot() {
    static std::atomic<ConnectHook> hook{nullptr};
    return hook;
}

inline std::atomic<PollHook>& PollHookSlot() {
    static std::atomic<PollHook> hook{nullptr};
    return hook;
}

inline std::atomic<WriteHook>& WriteHookSlot() {
    static std::atomic<WriteHook> hook{nullptr};
    return hook;
}

struct Settings {
    Http2RedirectCallback redirectCallback = nullptr;
    void* redirectArgument = nullptr;
    HttpsCallback sslCallback = nullptr;
    void* sslArgument = nullptr;
    std::string userAgent;
    std::vector<std::pair<std::string, std::string>> headers;
    std::uint64_t contentLength = 0;
    bool noContentLength = false;
    bool inflateGzip = false;
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
    int cookieBox = 0;
};

struct Capacity {
    explicit Capacity(int maximum) : maximum(maximum) {}
    const int maximum;
    std::atomic<int> active{0};
};

class Request {
public:
    Request(int id, int version, int sslContext, std::shared_ptr<Capacity> capacity, Settings settings, std::string method, std::string url, bool ignoresTotalTimeout = false);
    ~Request();
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;
    int Send(const void* data, std::size_t size);
    int Read(void* data, std::size_t size);
    int Status(int* status);
    int ContentLength(int* kind, std::uint64_t* length);
    int Headers(char** header, std::size_t* size);
    int TrailingHeaders(char** header, std::size_t* size);
    void Cancel();
    void Stop();
private:
    struct State;
    std::unique_ptr<State> state;
};

int AddHeader(Settings& settings, const char* name, const char* value, std::uint32_t mode);
std::shared_ptr<Capacity> CreateCapacity_nid_no_patch(int maximum);
std::shared_ptr<Request> CreateRequest_nid_no_patch(int id, int version, int sslContext, std::shared_ptr<Capacity> capacity, Settings settings, std::string method, std::string url, bool ignoresTotalTimeout = false);

}

extern "C" void Http2SetLookupHook_nid_no_patch(Http2::LookupHook hook);

extern "C" void Http2SetConnectHook_nid_no_patch(Http2::ConnectHook hook);
extern "C" void Http2SetPollHook_nid_no_patch(Http2::PollHook hook);
extern "C" void Http2SetWriteHook_nid_no_patch(Http2::WriteHook hook);
