#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI sceNpWebApi2CreateRequest(int user_context_id, const char* api_group, const char* path, const char* method, const void* content_parameter, std::int64_t* request_id);
int APS5_VABI sceNpWebApi2SetRequestTimeout(std::int64_t request_id, std::uint32_t timeout);
int APS5_VABI sceNpWebApi2DeleteRequest(std::int64_t request_id);
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    constexpr int invalidArgument = static_cast<int>(0x80553402);

    std::int64_t requestId = 0;
    Require(sceNpWebApi2CreateRequest(0, nullptr, "/", "GET", nullptr, &requestId) == 0);
    Require(requestId > 0);

    Require(sceNpWebApi2SetRequestTimeout(requestId, 1000) == 0);
    Require(sceNpWebApi2SetRequestTimeout(0, 1000) == invalidArgument);
    Require(sceNpWebApi2SetRequestTimeout(-1, 1000) == invalidArgument);

    Require(sceNpWebApi2DeleteRequest(requestId) == 0);
}
