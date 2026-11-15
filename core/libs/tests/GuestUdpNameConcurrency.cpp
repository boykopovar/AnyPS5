#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <future>

static std::atomic<int> pausedDescriptor{-1};
static std::promise<void> connectPaused;
static std::promise<void> continueConnect;
static std::shared_future<void> resumeConnect = continueConnect.get_future().share();

extern "C" int connect(int descriptor, const sockaddr* address, socklen_t size) {
    const int result = static_cast<int>(::syscall(SYS_connect, descriptor, address, size));
    int expected = descriptor;
    if (result == 0 && pausedDescriptor.compare_exchange_strong(expected, -1)) {
        connectPaused.set_value();
        resumeConnect.wait();
    }
    return result;
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    const int peer = socket_nid_postfix(2, 2, 0);
    const int client = socket_nid_postfix(2, 2, 0);
    Require(peer >= 0 && client >= 0);
    std::array<unsigned char, 16> destination{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(peer, destination.data(), destination.size()) == 0);
    std::uint32_t size = destination.size();
    Require(getsockname_nid_postfix(peer, destination.data(), &size) == 0);
    Require(connect_nid_postfix(client, destination.data(), destination.size()) == 0);
    std::array<unsigned char, 16> local{};
    size = local.size();
    Require(getsockname_nid_postfix(client, local.data(), &size) == 0);
    const auto native = GuestSockets::NativeDescriptor(client);
    Require(native >= 0);
    pausedDescriptor = static_cast<int>(native);
    const std::array<unsigned char, 16> unspecified{16, 0};
    auto disconnect = std::async(std::launch::async, [&] {
        return connect_nid_postfix(client, unspecified.data(), unspecified.size());
    });
    connectPaused.get_future().wait();
    auto reconnect = std::async(std::launch::async, [&] {
        return connect_nid_postfix(client, destination.data(), destination.size());
    });
    reconnect.wait_for(std::chrono::milliseconds(250));
    continueConnect.set_value();
    Require(disconnect.get() == -1 && reconnect.get() == 0);
    std::array<unsigned char, 16> name{};
    size = name.size();
    Require(getpeername_nid_postfix(client, name.data(), &size) == 0 && name == destination);
    size = name.size();
    Require(getsockname_nid_postfix(client, name.data(), &size) == 0 && name == local);
    Require(close_nid_postfix(client) == 0 && close_nid_postfix(peer) == 0);
}
