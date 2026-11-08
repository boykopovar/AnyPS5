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

int main() {
    ParsesValidEndpoint();
    return 0;
}
