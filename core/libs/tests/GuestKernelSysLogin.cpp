#include "SceTypes.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>

extern "C" {
void LoginNameForTest_nid_no_patch(const char* name);
}

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::exit(1);
}

int Error() {
    return *__error_nid_postfix();
}

void Getlogin() {
    char expected[33]{};
    const int error = getlogin_r_nid_postfix(expected, static_cast<int>(sizeof(expected)));
    char* name = getlogin_nid_postfix();
    if (error != 0) {
        Require(name == nullptr && Error() == error);
        return;
    }
    if (expected[0] == '\0') {
        Require(name == nullptr);
        return;
    }
    Require(name != nullptr && std::strcmp(name, expected) == 0);
    Require(getlogin_nid_postfix() == name);
}

void GetloginPaths() {
    LoginNameForTest_nid_no_patch("player");
    char* name = getlogin_nid_postfix();
    Require(name != nullptr && std::strcmp(name, "player") == 0);
    char buffer[7];
    std::memset(buffer, 'x', sizeof(buffer));
    Require(getlogin_r_nid_postfix(buffer, 6) == 34 && buffer[0] == 'x');
    Require(getlogin_r_nid_postfix(buffer, 7) == 0 && std::strcmp(buffer, "player") == 0);

    const std::string longName(40, 'n');
    LoginNameForTest_nid_no_patch(longName.c_str());
    *__error_nid_postfix() = 0;
    Require(getlogin_nid_postfix() == nullptr && Error() == 34);

    LoginNameForTest_nid_no_patch("");
    bool threw = false;
    try {
        getlogin_nid_postfix();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    LoginNameForTest_nid_no_patch(nullptr);
}

#ifdef _WIN32
void LoopbackRefusal() {
    constexpr int FamilyInet = 2;
    constexpr int FamilyInet6 = 28;
    constexpr int TypeStream = 1;
    constexpr int ErrorRefused = 61;
    constexpr int ErrorAffamily = 47;
    constexpr int ErrorAddrNotAvail = 49;
    constexpr int ErrorNetUnreach = 51;

    const int listener = socket_nid_postfix(FamilyInet, TypeStream, 0);
    Require(listener >= 0);
    std::array<std::uint8_t, 16> bindAddr{16, FamilyInet, 0, 0, 127, 0, 0, 2};
    Require(bind_nid_postfix(listener, bindAddr.data(), bindAddr.size()) == 0);
    std::uint32_t len = bindAddr.size();
    Require(getsockname_nid_postfix(listener, bindAddr.data(), &len) == 0 && len == 16);
    Require(listen_nid_postfix(listener, 1) == 0);

    const int client = socket_nid_postfix(FamilyInet, TypeStream, 0);
    Require(client >= 0);
    std::array<std::uint8_t, 16> targetAddr{16, FamilyInet, bindAddr[2], bindAddr[3], 127, 0, 0, 1};
    const auto start = std::chrono::steady_clock::now();
    const int result = connect_nid_postfix(client, targetAddr.data(), targetAddr.size());
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    Require(result == -1 && Error() == ErrorRefused);
    Require(elapsedMs < 1000);
    Require(close_nid_postfix(client) == 0);

    const int matchedClient = socket_nid_postfix(FamilyInet, TypeStream, 0);
    Require(matchedClient >= 0);
    Require(connect_nid_postfix(matchedClient, bindAddr.data(), bindAddr.size()) == 0);
    Require(close_nid_postfix(matchedClient) == 0);
    Require(close_nid_postfix(listener) == 0);

    const int listener4 = socket_nid_postfix(FamilyInet, TypeStream, 0);
    Require(listener4 >= 0);
    std::array<std::uint8_t, 16> bind4{16, FamilyInet, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener4, bind4.data(), bind4.size()) == 0);
    len = bind4.size();
    Require(getsockname_nid_postfix(listener4, bind4.data(), &len) == 0 && len == 16);
    Require(listen_nid_postfix(listener4, 1) == 0);

    const int client6 = socket_nid_postfix(FamilyInet6, TypeStream, 0);
    if (client6 >= 0) {
        std::array<std::uint8_t, 28> target6{};
        target6[0] = 28;
        target6[1] = FamilyInet6;
        target6[2] = bind4[2];
        target6[3] = bind4[3];
        target6[23] = 1;
        const auto start6 = std::chrono::steady_clock::now();
        const int res6 = connect_nid_postfix(client6, target6.data(), target6.size());
        const auto elapsed6Ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start6).count();
        Require(res6 == -1);
        const int err6 = Error();
        if (err6 != ErrorAffamily && err6 != ErrorAddrNotAvail && err6 != ErrorNetUnreach) {
            Require(err6 == ErrorRefused);
            Require(elapsed6Ms < 1000);
        }
        Require(close_nid_postfix(client6) == 0);
    } else {
        Require(Error() == ErrorAffamily);
    }
    Require(close_nid_postfix(listener4) == 0);

    const int listenerMapped = socket_nid_postfix(FamilyInet, TypeStream, 0);
    Require(listenerMapped >= 0);
    std::array<std::uint8_t, 16> bindMapped{16, FamilyInet, 0, 0, 127, 0, 0, 2};
    Require(bind_nid_postfix(listenerMapped, bindMapped.data(), bindMapped.size()) == 0);
    len = bindMapped.size();
    Require(getsockname_nid_postfix(listenerMapped, bindMapped.data(), &len) == 0 && len == 16);
    Require(listen_nid_postfix(listenerMapped, 1) == 0);

    const int clientMapped = socket_nid_postfix(FamilyInet6, TypeStream, 0);
    if (clientMapped >= 0) {
        const int v6Only = 0;
        if (setsockopt_nid_postfix(clientMapped, 41, 27, &v6Only, sizeof(v6Only)) == 0) {
            std::array<std::uint8_t, 28> targetMapped{};
            targetMapped[0] = 28;
            targetMapped[1] = FamilyInet6;
            targetMapped[2] = bindMapped[2];
            targetMapped[3] = bindMapped[3];
            targetMapped[18] = 0xff;
            targetMapped[19] = 0xff;
            targetMapped[20] = 127;
            targetMapped[23] = 1;
            const auto startMapped = std::chrono::steady_clock::now();
            const int resMapped = connect_nid_postfix(clientMapped, targetMapped.data(), targetMapped.size());
            const auto elapsedMappedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startMapped).count();
            Require(resMapped == -1);
            const int errMapped = Error();
            if (errMapped != ErrorAffamily && errMapped != ErrorAddrNotAvail && errMapped != ErrorNetUnreach) {
                Require(errMapped == ErrorRefused);
                Require(elapsedMappedMs < 1000);
            }
        }
        Require(close_nid_postfix(clientMapped) == 0);
    } else {
        Require(Error() == ErrorAffamily);
    }
    Require(close_nid_postfix(listenerMapped) == 0);
}
#endif

}

int main() {
    Getlogin();
    GetloginPaths();
#ifdef _WIN32
    LoopbackRefusal();
#endif
    std::puts("getlogin passed");
    return 0;
}
