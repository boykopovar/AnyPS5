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

template <typename Function>
static void RejectsPeers(const char* text, const char* message, Function function) {
    try {
        function(text);
    } catch (const std::invalid_argument&) {
        return;
    }
    std::fprintf(stderr, "%s: %s\n", message, text);
    std::exit(1);
}

static void RejectsEmptyPeerEntries() {
    const auto parse = [](const char* text) { LanRelay::ParsePeers(text); };
    RejectsPeers(",", "a list of one empty entry is refused", parse);
    RejectsPeers("127.0.0.1:47001,", "a trailing comma is refused", parse);
    RejectsPeers(",127.0.0.1:47001", "a leading comma is refused", parse);
    RejectsPeers("127.0.0.1:47001,,10.0.0.2:47002", "an empty entry between peers is refused", parse);
}

static void KeepsOneCopyOfDuplicatePeers() {
    const auto peers = LanRelay::ParsePeers("127.0.0.1:47001,10.0.0.2:47002,127.0.0.1:47001");
    Require(peers.size() == 2, "a repeated peer is kept once");
    Require(peers[0].Host == "127.0.0.1" && peers[0].Port == 47001, "the first copy stays");
    Require(peers[1].Host == "10.0.0.2" && peers[1].Port == 47002, "the other peer stays in order");
    Require(LanRelay::ParsePeers("127.0.0.1:47001,127.0.0.1:47002").size() == 2, "the same host on another port is another peer");
}

static void ReadsSettings() {
    const auto off = LanRelay::ReadSettings(nullptr, nullptr);
    Require(!off.Listen && off.Peers.empty(), "both unset leaves the feature off");
    Require(!LanRelay::ReadSettings("", "").Listen, "empty variables leave the feature off");
    const auto listenOnly = LanRelay::ReadSettings("127.0.0.1:47000", nullptr);
    Require(listenOnly.Listen && listenOnly.Listen->Port == 47000 && listenOnly.Peers.empty(), "a listen address alone is valid");
    const auto both = LanRelay::ReadSettings("127.0.0.1:47000", "127.0.0.1:47001");
    Require(both.Listen && both.Peers.size() == 1 && both.Peers[0].Port == 47001, "listen and peers are read together");
}

template <typename Function>
static void RejectsNaming(const char* message, const char* name, Function function) {
    try {
        function();
    } catch (const std::invalid_argument& error) {
        if (std::string(error.what()).find(name) != std::string::npos) return;
        std::fprintf(stderr, "%s: the error does not name %s: %s\n", message, name, error.what());
        std::exit(1);
    }
    std::fprintf(stderr, "%s\n", message);
    std::exit(1);
}

static void NamesTheVariableInErrors() {
    RejectsNaming("a bad listen address is refused", "ANYPS5_LAN_LISTEN",
        [] { LanRelay::ReadSettings("127.0.0.1:0", nullptr); });
    RejectsNaming("a bad peer is refused", "ANYPS5_LAN_PEERS",
        [] { LanRelay::ReadSettings("127.0.0.1:47000", "127.0.0.1:70000"); });
    RejectsNaming("peers without a listen address are refused", "ANYPS5_LAN_LISTEN",
        [] { LanRelay::ReadSettings(nullptr, "127.0.0.1:47001"); });
}

static void OpensAndClosesOnEphemeralPort() {
    LanRelay::Relay relay;
    Require(!relay.IsOpen() && relay.Port() == 0, "a new relay is closed");
    relay.Open(LanRelay::Endpoint{"127.0.0.1", 0});
    Require(relay.IsOpen() && relay.Port() != 0, "the relay opens on an OS-chosen port");
    bool refused = false;
    try {
        relay.Open(LanRelay::Endpoint{"127.0.0.1", 0});
    } catch (const std::logic_error&) {
        refused = true;
    }
    Require(refused, "a second Open on an open relay is refused");
    relay.Close();
    Require(!relay.IsOpen() && relay.Port() == 0, "Close leaves the relay closed");
    relay.Close();
}

static void RefusesBusyPort() {
    LanRelay::Relay first;
    first.Open(LanRelay::Endpoint{"127.0.0.1", 0});
    LanRelay::Relay second;
    bool refused = false;
    try {
        second.Open(LanRelay::Endpoint{"127.0.0.1", first.Port()});
    } catch (const std::runtime_error&) {
        refused = true;
    }
    Require(refused && !second.IsOpen(), "a port in use is refused and the relay stays closed");
}

static void OpensAndClosesTwoRelaysWithoutLeaks() {
    LanRelay::Relay first;
    LanRelay::Relay second;
    first.Open(LanRelay::Endpoint{"127.0.0.1", 0});
    second.Open(LanRelay::Endpoint{"127.0.0.1", 0});
    Require(first.Port() != second.Port(), "two relays take two ephemeral ports");
    const auto firstPort = first.Port();
    const auto secondPort = second.Port();
    first.Close();
    second.Close();
    LanRelay::Relay reopenedFirst;
    LanRelay::Relay reopenedSecond;
    reopenedFirst.Open(LanRelay::Endpoint{"127.0.0.1", firstPort});
    reopenedSecond.Open(LanRelay::Endpoint{"127.0.0.1", secondPort});
    Require(reopenedFirst.Port() == firstPort && reopenedSecond.Port() == secondPort,
        "closed relays release their ports");
    reopenedFirst.Close();
    reopenedSecond.Close();
}

static std::size_t OpenDescriptorCount() {
#ifdef __linux__
    std::size_t count = 0;
    if (DIR* directory = opendir("/proc/self/fd")) {
        while (readdir(directory) != nullptr) ++count;
        closedir(directory);
    }
    return count;
#else
    return 0;
#endif
}

static void LeavesNoDescriptorsBehind() {
    const auto before = OpenDescriptorCount();
    for (int cycle = 0; cycle < 200; ++cycle) {
        LanRelay::Relay relay;
        relay.Open(LanRelay::Endpoint{"127.0.0.1", 0});
        relay.Close();
    }
    for (int cycle = 0; cycle < 200; ++cycle) {
        LanRelay::Relay relay;
        relay.Open(LanRelay::Endpoint{"127.0.0.1", 0});
    }
#ifdef __linux__
    Require(OpenDescriptorCount() == before, "no descriptor is left open after the relays are closed or destroyed");
#endif
}

static void StaysOffByDefault() {
    Require(LanRelay::StartRelay(nullptr, nullptr, nullptr) == nullptr, "both variables unset starts no relay");
    Require(LanRelay::StartRelay("", "", nullptr) == nullptr, "empty variables start no relay");
}

static void InvalidSettingsLogOneLineAndStayOff() {
    std::FILE* log = std::tmpfile();
    Require(log != nullptr, "a log file is available");
    Require(LanRelay::StartRelay("127.0.0.1:0", nullptr, log) == nullptr, "a bad listen address starts no relay");
    Require(LanRelay::StartRelay(nullptr, "127.0.0.1:47001", log) == nullptr, "peers without a listen address start no relay");
    std::rewind(log);
    int lines = 0;
    char line[512];
    while (std::fgets(line, sizeof line, log) != nullptr) {
        ++lines;
        Require(std::strstr(line, "ANYPS5_LAN_") != nullptr && std::strstr(line, "LAN relay is off") != nullptr,
            "each refusal names the variable and says the relay is off");
    }
    std::fclose(log);
    Require(lines == 2, "each refused setting logs exactly one line");
}

static void StartsWhenConfigured() {
    auto relay = LanRelay::StartRelay("127.0.0.1:47123", "", nullptr);
    Require(relay != nullptr && relay->IsOpen() && relay->Port() != 0, "a listen address starts the relay");
}

int main() {
    ParsesValidEndpoint();
    ParsesPeerList();
    RejectsEmptyPeerEntries();
    KeepsOneCopyOfDuplicatePeers();
    RejectsInvalidEndpoints();
    ReadsSettings();
    NamesTheVariableInErrors();
    OpensAndClosesOnEphemeralPort();
    RefusesBusyPort();
    OpensAndClosesTwoRelaysWithoutLeaks();
    LeavesNoDescriptorsBehind();
    StaysOffByDefault();
    InvalidSettingsLogOneLineAndStayOff();
    StartsWhenConfigured();
    return 0;
}
