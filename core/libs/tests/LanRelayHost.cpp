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

static void GivesEachOpenRelayItsOwnInstance() {
    LanRelay::Relay first;
    LanRelay::Relay second;
    Require(first.Instance() == 0, "a closed relay has no instance id");
    first.Open(LanRelay::Endpoint{"127.0.0.1", 0});
    second.Open(LanRelay::Endpoint{"127.0.0.1", 0});
    Require(first.Instance() != second.Instance(), "two open relays have different instance ids");
    first.Close();
    Require(first.Instance() == 0, "Close clears the instance id");
    second.Close();
}

static void RecognisesOwnFrames() {
    LanRelay::Frame frame;
    frame.Instance = 7;
    Require(LanRelay::IsOwnFrame(frame, 7), "a frame with this instance is own");
    Require(!LanRelay::IsOwnFrame(frame, 8), "a frame with another instance is not own");
}

static void DeliversFrameOnceBetweenRelays() {
    LanRelay::Relay a;
    LanRelay::Relay b;
    a.Open(LanRelay::Endpoint{"127.0.0.1", 0});
    b.Open(LanRelay::Endpoint{"127.0.0.1", 0});
    const LanRelay::Endpoint toA{"127.0.0.1", a.Port()};
    const LanRelay::Endpoint toB{"127.0.0.1", b.Port()};
    Require(a.SendFrame(toB, 5000, 47000, {9, 8, 7}), "a frame is sent to a peer");
    const auto received = b.Receive(2000);
    Require(received.has_value(), "the peer receives the frame");
    Require(received->Payload == std::vector<std::uint8_t>({9, 8, 7}) && received->SourcePort == 5000 && received->DestinationPort == 47000
            && received->Instance == a.Instance(), "the frame keeps its ports, payload and sender instance");
    Require(!b.Receive(200).has_value(), "the frame is delivered once");
    Require(b.SendDatagram(toA, LanRelay::EncodeFrame(*received)), "the peer sends the frame back");
    Require(!a.Receive(500).has_value(), "a frame that carries this instance is dropped when it returns");
    a.Close();
    b.Close();
}

static void SkipsMalformedDatagrams() {
    LanRelay::Relay a;
    LanRelay::Relay b;
    a.Open(LanRelay::Endpoint{"127.0.0.1", 0});
    b.Open(LanRelay::Endpoint{"127.0.0.1", 0});
    const LanRelay::Endpoint toB{"127.0.0.1", b.Port()};
    LanRelay::Frame frame;
    frame.DestinationPort = 47000;
    frame.Payload = {4};
    auto otherVersion = LanRelay::EncodeFrame(frame);
    otherVersion[0] = LanRelay::FrameVersion + 1;
    auto truncated = LanRelay::EncodeFrame(frame);
    truncated.pop_back();
    Require(a.SendDatagram(toB, otherVersion), "a datagram with another version is sent");
    Require(a.SendDatagram(toB, truncated), "a truncated frame is sent");
    Require(a.SendDatagram(toB, {1, 2}), "a datagram shorter than the header is sent");
    Require(a.SendFrame(toB, 5000, 47000, {4}), "a valid frame is sent after them");
    const auto received = b.Receive(2000);
    Require(received.has_value() && received->Payload == std::vector<std::uint8_t>({4}),
        "the malformed datagrams are skipped and the valid frame is returned");
    Require(!b.Receive(200).has_value(), "no other frame is delivered");
    a.Close();
    b.Close();
}

static void ClosedRelayNeitherSendsNorReceives() {
    LanRelay::Relay relay;
    const LanRelay::Endpoint peer{"127.0.0.1", 47000};
    Require(!relay.SendFrame(peer, 5000, 47000, {1}), "a closed relay sends no frame");
    Require(!relay.SendDatagram(peer, {1}), "a closed relay sends no datagram");
    Require(!relay.Receive(10).has_value(), "a closed relay receives nothing");
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

static void RecognisesMulticastGroups() {
    Require(LanRelay::IsMulticast(0xE0000001u), "224.0.0.1 is a multicast group");
    Require(LanRelay::IsMulticast(0xEFFFFFFFu), "239.255.255.255 is a multicast group");
    Require(!LanRelay::IsMulticast(0xDFFFFFFFu), "223.255.255.255 is not a multicast group");
    Require(!LanRelay::IsMulticast(0xF0000000u), "240.0.0.0 is not a multicast group");
    Require(!LanRelay::IsMulticast(0xFFFFFFFFu), "255.255.255.255 is a broadcast, not a group");
}

static void MatchesLimitedAndInterfaceBroadcasts() {
    const std::vector<LanRelay::Ipv4Interface> lan{{0xC0A80114u, 0xFFFFFF00u}};
    Require(LanRelay::IsBroadcast(0xFFFFFFFFu, {}), "255.255.255.255 is a broadcast without any interface");
    Require(LanRelay::IsBroadcast(0xC0A801FFu, lan), "192.168.1.255 is the broadcast of 192.168.1.20/24");
    Require(!LanRelay::IsBroadcast(0xC0A80101u, lan), "192.168.1.1 is not a broadcast");
    Require(!LanRelay::IsBroadcast(0xC0A80200u, lan), "192.168.2.0 is outside the interface subnet");
    Require(!LanRelay::IsBroadcast(0xC0A801FFu, {}), "a subnet broadcast needs its interface");
    Require(!LanRelay::IsBroadcast(0x0A000005u, {{0x0A000005u, 0xFFFFFFFFu}}), "a host route address is not a broadcast");
}

static void SendsFrameToEveryPeer() {
    LanRelay::Relay sender;
    LanRelay::Relay first;
    LanRelay::Relay second;
    sender.Open(LanRelay::Endpoint{"127.0.0.1", 0});
    first.Open(LanRelay::Endpoint{"127.0.0.1", 0});
    second.Open(LanRelay::Endpoint{"127.0.0.1", 0});
    Require(sender.SendToPeers(5000, 47000, {1}) && !first.Receive(100).has_value(), "with no peers nothing is sent");
    sender.SetPeers({LanRelay::Endpoint{"127.0.0.1", first.Port()}, LanRelay::Endpoint{"127.0.0.1", second.Port()}});
    Require(sender.Peers().size() == 2, "both peers are kept in order");
    Require(sender.SendToPeers(5000, 47000, {7, 8}), "a frame is sent to each peer");
    const auto atFirst = first.Receive(2000);
    const auto atSecond = second.Receive(2000);
    Require(atFirst.has_value() && atFirst->Payload == std::vector<std::uint8_t>({7, 8}) && atFirst->SourcePort == 5000
            && atFirst->DestinationPort == 47000 && atFirst->Instance == sender.Instance(), "the first peer receives the frame");
    Require(atSecond.has_value() && atSecond->Payload == std::vector<std::uint8_t>({7, 8}) && atSecond->Instance == sender.Instance(),
        "the second peer receives the frame");
    Require(!first.Receive(100).has_value(), "each peer receives the frame once");
    sender.Close();
    first.Close();
    second.Close();
}

static void StartsAndStopsDelivery() {
    LanRelay::Relay relay;
    Require(!relay.StartDelivery(), "a closed relay cannot start delivery");
    relay.Open(LanRelay::Endpoint{"127.0.0.1", 0});
    Require(relay.StartDelivery(), "an open relay starts delivery");
    Require(!relay.StartDelivery(), "delivery starts once");
    relay.Close();
    Require(!relay.IsOpen(), "Close stops delivery and closes the relay");
}

static void RoundTripsFrames() {
    LanRelay::Frame frame;
    frame.Instance = 0x01020304;
    frame.SourcePort = 5000;
    frame.DestinationPort = 47000;
    frame.Payload = {0xAA, 0xBB, 0xCC};
    const auto datagram = LanRelay::EncodeFrame(frame);
    Require(datagram.size() == LanRelay::FrameHeaderSize + 3, "the header adds FrameHeaderSize bytes to the payload");
    Require(datagram[0] == LanRelay::FrameVersion, "the first byte is the frame version");
    Require(datagram[1] == 0x01 && datagram[4] == 0x04, "the instance is written big-endian");
    Require(datagram[9] == 0 && datagram[10] == 3, "the payload length is written big-endian");
    const auto decoded = LanRelay::DecodeFrame(datagram);
    Require(decoded.has_value(), "an encoded frame decodes");
    Require(decoded->Instance == frame.Instance && decoded->SourcePort == 5000 && decoded->DestinationPort == 47000 && decoded->Payload == frame.Payload,
        "the decoded frame matches the encoded one");
    LanRelay::Frame empty;
    empty.DestinationPort = 1;
    const auto emptyDecoded = LanRelay::DecodeFrame(LanRelay::EncodeFrame(empty));
    Require(emptyDecoded.has_value() && emptyDecoded->Payload.empty(), "an empty payload round-trips");
    LanRelay::Frame largest = frame;
    largest.Payload.assign(LanRelay::MaxFramePayload, 0x5A);
    Require(LanRelay::EncodeFrame(largest).size() == LanRelay::MaxDatagramSize, "the largest payload fills a datagram");
}

static void RefusesMalformedFrames() {
    LanRelay::Frame frame;
    frame.DestinationPort = 47000;
    frame.Payload = {1, 2};
    const auto valid = LanRelay::EncodeFrame(frame);
    auto otherVersion = valid;
    otherVersion[0] = LanRelay::FrameVersion + 1;
    Require(!LanRelay::DecodeFrame(otherVersion), "a frame with another version is dropped");
    Require(!LanRelay::DecodeFrame({}), "an empty datagram is dropped");
    Require(!LanRelay::DecodeFrame(std::vector<std::uint8_t>(valid.begin(), valid.begin() + LanRelay::FrameHeaderSize - 1)),
        "a datagram shorter than the header is dropped");
    Require(!LanRelay::DecodeFrame(std::vector<std::uint8_t>(valid.begin(), valid.end() - 1)), "a truncated payload is dropped");
    auto trailing = valid;
    trailing.push_back(0);
    Require(!LanRelay::DecodeFrame(trailing), "trailing bytes past the declared payload are dropped");
    auto portZero = valid;
    portZero[7] = 0;
    portZero[8] = 0;
    Require(!LanRelay::DecodeFrame(portZero), "a frame to port 0 is dropped");
}

template <typename TError, typename Function>
static void ThrowsError(const char* message, Function function) {
    try {
        function();
    } catch (const TError&) {
        return;
    }
    std::fprintf(stderr, "%s\n", message);
    std::exit(1);
}

static void RefusesFramesThatCannotBeSent() {
    LanRelay::Frame oversized;
    oversized.DestinationPort = 47000;
    oversized.Payload.assign(LanRelay::MaxFramePayload + 1, 0);
    ThrowsError<std::length_error>("a payload that does not fit in a datagram is refused", [&] { LanRelay::EncodeFrame(oversized); });
    LanRelay::Frame toPortZero;
    toPortZero.Payload = {1};
    ThrowsError<std::invalid_argument>("a frame to port 0 is refused", [&] { LanRelay::EncodeFrame(toPortZero); });
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
    GivesEachOpenRelayItsOwnInstance();
    RecognisesOwnFrames();
    SendsFrameToEveryPeer();
    StartsAndStopsDelivery();
    DeliversFrameOnceBetweenRelays();
    SkipsMalformedDatagrams();
    ClosedRelayNeitherSendsNorReceives();
    LeavesNoDescriptorsBehind();
    StaysOffByDefault();
    InvalidSettingsLogOneLineAndStayOff();
    StartsWhenConfigured();
    RecognisesMulticastGroups();
    MatchesLimitedAndInterfaceBroadcasts();
    RoundTripsFrames();
    RefusesMalformedFrames();
    RefusesFramesThatCannotBeSent();
    return 0;
}
