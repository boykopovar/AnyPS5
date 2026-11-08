#include "prx/libkernel/Socket/include/LanRelay.hpp"
#include <cstdio>
#ifdef __linux__
#include <dirent.h>
#endif
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(1);
    }
}

static void ParsesValidEndpoint() {
    const auto endpoint = LanRelay::ParseEndpoint("127.0.0.1:47000");
    Require(endpoint.Host == "127.0.0.1", "the host is the text before the last colon");
    Require(endpoint.Port == 47000, "the port is the number after the colon");
    Require(LanRelay::ParseEndpoint("192.168.1.20:1").Port == 1, "the lowest port parses");
}

template <typename Function>
static void Rejects(const char* text, const char* message, Function function) {
    try {
        function(text);
    } catch (const std::invalid_argument&) {
        return;
    }
    std::fprintf(stderr, "%s: %s\n", message, text);
    std::exit(1);
}

static void RejectsInvalidEndpoints() {
    const auto parse = [](const char* text) { LanRelay::ParseEndpoint(text); };
    Rejects("127.0.0.1", "a missing port is refused", parse);
    Rejects("127.0.0.1:", "an empty port is refused", parse);
    Rejects(":47000", "an empty host is refused", parse);
    Rejects("127.0.0.1:0", "port 0 is refused", parse);
    Rejects("127.0.0.1:65536", "a port above 65535 is refused", parse);
    Rejects("127.0.0.1:99999999999", "a huge port is refused", parse);
    Rejects("127.0.0.1:-1", "a signed port is refused", parse);
    Rejects("127.0.0.1:47000x", "a port with trailing text is refused", parse);
    Rejects("127.0.0.1: 47000", "a port with a space is refused", parse);
    Rejects("localhost:47000", "a host name is refused", parse);
    Rejects("256.0.0.1:47000", "an octet above 255 is refused", parse);
    Rejects("127.0.1:47000", "three octets are refused", parse);
    Rejects("127.0.0.1.5:47000", "five octets are refused", parse);
    Rejects("127..0.1:47000", "an empty octet is refused", parse);
}

static void ParsesPeerList() {
    Require(LanRelay::ParsePeers("").empty(), "an empty list has no peers");
    const auto single = LanRelay::ParsePeers("127.0.0.1:47001");
    Require(single.size() == 1 && single[0].Port == 47001, "one peer parses");
    const auto peers = LanRelay::ParsePeers("127.0.0.1:47001,10.0.0.2:47002");
    Require(peers.size() == 2, "two peers parse");
    Require(peers[0].Host == "127.0.0.1" && peers[0].Port == 47001, "the first peer keeps its order");
    Require(peers[1].Host == "10.0.0.2" && peers[1].Port == 47002, "the second peer keeps its order");
}

int main() {
    ParsesValidEndpoint();
    ParsesPeerList();
    RejectsInvalidEndpoints();
    return 0;
}
