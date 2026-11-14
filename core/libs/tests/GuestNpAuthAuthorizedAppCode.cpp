#include "prx/libSceNpAuth/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cerrno>
#include <cstdlib>
#include <stdexcept>

static void Require(bool condition) {
    if (!condition) std::abort();
}

int main() {
    constexpr auto invalidArgument = static_cast<int>(0x80550301u);
    constexpr auto invalidSize = static_cast<int>(0x80550302u);
    constexpr auto requestNotFound = static_cast<int>(0x80550306u);
    std::array<char, 136> client{};
    std::array<char, 128> code{};
    code.fill('x');
    const auto original = code;
    bool authorizationRequired = true;
    NpAuthAuthorizedAppCodeParameter parameter{40, 1, reinterpret_cast<const NpAuthClientId*>(client.data()), "", 0};
    Require(sceNpAuthGetAuthorizedAppCode(0, nullptr, code.data(), nullptr, &authorizationRequired) == invalidArgument);
    errno = 71;
    Require(sceNpAuthGetAuthorizedAppCode(0, reinterpret_cast<const NpAuthAuthorizedAppCodeParameter*>(0x10), code.data(), nullptr, &authorizationRequired) == invalidArgument);
    Require(errno == 71);
    parameter.size = 39;
    Require(sceNpAuthGetAuthorizedAppCode(0, &parameter, code.data(), nullptr, &authorizationRequired) == invalidSize);
    parameter.size = 40;
    Require(sceNpAuthGetAuthorizedAppCode(0, &parameter, nullptr, nullptr, &authorizationRequired) == invalidArgument);
    Require(sceNpAuthGetAuthorizedAppCode(0, &parameter, code.data(), nullptr, nullptr) == invalidArgument);
    parameter.option = 2;
    Require(sceNpAuthGetAuthorizedAppCode(0, &parameter, code.data(), nullptr, &authorizationRequired) == invalidArgument);
    parameter.option = 0;
    parameter.userId = -1;
    Require(sceNpAuthGetAuthorizedAppCode(0, &parameter, code.data(), nullptr, &authorizationRequired) == invalidArgument);
    parameter.userId = 1;
    Require(sceNpAuthGetAuthorizedAppCode(0, &parameter, code.data(), nullptr, &authorizationRequired) == requestNotFound);
    const auto requestId = sceNpAuthCreateRequest();
    Require(requestId > 0);
    bool threw = false;
    try {
        sceNpAuthGetAuthorizedAppCode(requestId, &parameter, code.data(), nullptr, &authorizationRequired);

    } catch (const std::logic_error&) {
        threw = true;
    }
    Require(threw && code == original && authorizationRequired);
    parameter.clientId = nullptr;
    Require(sceNpAuthGetAuthorizedAppCode(0, &parameter, code.data(), nullptr, &authorizationRequired) == invalidArgument);
    parameter.clientId = reinterpret_cast<const NpAuthClientId*>(client.data());
    parameter.scope = nullptr;
    Require(sceNpAuthGetAuthorizedAppCode(0, &parameter, code.data(), nullptr, &authorizationRequired) == invalidArgument);
    parameter.scope = "";
    Require(sceNpAuthDeleteRequest(requestId) == 0);
    Require(sceNpAuthGetAuthorizedAppCode(requestId, &parameter, code.data(), nullptr, &authorizationRequired) == requestNotFound);
    Require(sceNpAuthDeleteRequest(requestId) == 0);
    Require(sceNpAuthDeleteRequest(-1) == 0);

#ifdef _WIN32
    Require(_putenv("APS5_NET_DISCONNECTED=1") == 0);
#else
    Require(setenv("APS5_NET_DISCONNECTED", "1", 1) == 0);
#endif
    constexpr auto signedOut = static_cast<int>(0x80550006u);
    const auto offlineRequest = sceNpAuthCreateRequest();
    Require(offlineRequest > 0);
    int issuerId = -1;
    Require(sceNpAuthGetAuthorizedAppCode(offlineRequest, nullptr, code.data(), &issuerId, &authorizationRequired) == invalidArgument);
    Require(sceNpAuthGetAuthorizedAppCode(-1, &parameter, code.data(), &issuerId, &authorizationRequired) == requestNotFound);
    Require(sceNpAuthGetAuthorizedAppCode(offlineRequest, &parameter, code.data(), &issuerId, &authorizationRequired) == signedOut);
    Require(code == original && issuerId == -1 && authorizationRequired);
    Require(sceNpAuthDeleteRequest(offlineRequest) == 0);
}
