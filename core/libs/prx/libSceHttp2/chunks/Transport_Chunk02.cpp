#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#endif

#include "Transport.hpp"
#include "Cookies.hpp"
#include "prx/libSceHttp/include/Http.hpp"

#include "zlib.h"
#include "prx/libSceNet/include/Net.hpp"
#include "prx/libSceSsl/include/Ssl.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel/Socket/include/SocketPoll.hpp"
#include <nghttp2/nghttp2.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <limits>
#include <mutex>
#include <string_view>
#include <thread>

extern "C" int SslRead_nid_no_patch(int, void*, std::size_t, int);
extern "C" int SslTrustSystemRoots_nid_no_patch(int);

namespace Http2 {
namespace {
int Request::TrailingHeaders(char** header, std::size_t* size) {
    auto lock = state->Lock();
    if (state->networkResult) return state->networkResult;
    Require(header && size && state->ready && state->ended, "sceHttp2GetAllTrailingHeaders: null output or response unavailable");
    Require(!state->trailerHeaders.empty(), "sceHttp2GetAllTrailingHeaders: response has no trailers");
    *header = state->trailerHeaders.data();
    *size = state->trailerHeaders.size();
    return 0;
}

void Request::Cancel() { state->cancelled.store(true); }

void Request::Stop() {
    state->stopped.store(true);
    Cancel();
    std::lock_guard lock(state->mutex);
    state->Close();
}

std::shared_ptr<Capacity> CreateCapacity_nid_no_patch(int maximum) {
    return std::make_shared<Capacity>(maximum);
}

std::shared_ptr<Request> CreateRequest_nid_no_patch(int id, int version, int sslContext, std::shared_ptr<Capacity> capacity, Settings settings, std::string method, std::string url) {
    return std::make_shared<Request>(id, version, sslContext, std::move(capacity), std::move(settings), std::move(method), std::move(url));
}

}
}
