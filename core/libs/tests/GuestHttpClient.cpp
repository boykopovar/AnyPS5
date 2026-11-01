#include "prx/libSceHttp/libSceHttp.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

using Client = std::unique_ptr<Aps5HttpClient, decltype(&Aps5HttpClient_Destroy_nid_postfix)>;

Client Create() {
    Client client(Aps5HttpClient_Create_nid_postfix(), Aps5HttpClient_Destroy_nid_postfix);
    Check(client != nullptr, "client allocation failed");
    return client;
}

template<class TAction>
void Invalid(TAction action) {
    bool failed = false;
    try { action(); } catch (const std::invalid_argument&) { failed = true; }
    Check(failed, "invalid client argument accepted");
}

std::uint8_t Byte(std::size_t offset, bool alternate = false) {
    return static_cast<std::uint8_t>((alternate ? offset * 13 + 71 : offset) % 251);
}

void Write(const char* name, std::size_t size, bool alternate = false) {
    std::ofstream file(std::filesystem::path("app0") / name, std::ios::binary);
    Check(static_cast<bool>(file), "payload creation failed");
    for (std::size_t offset = 0; offset < size; ++offset) file.put(static_cast<char>(Byte(offset, alternate)));
    Check(static_cast<bool>(file), "payload write failed");
}

void Read(Aps5HttpClient* client, std::size_t offset, std::size_t size, bool alternate = false) {
    constexpr std::array<std::size_t, 5> capacities{1, 7, 113, 1023, 4096};
    std::array<std::uint8_t, 4098> bytes;
    std::size_t iteration = 0;
    while (size) {
        bytes.fill(0xa5);
        const auto capacity = std::min(size, capacities[iteration++ % capacities.size()]);
        const auto count = Aps5HttpClient_Read_nid_postfix(client, bytes.data() + 1, static_cast<std::int32_t>(capacity));
        Check(count > 0 && static_cast<std::size_t>(count) <= capacity, "read returned an invalid byte count");
        Check(bytes.front() == 0xa5 && std::all_of(bytes.begin() + count + 1, bytes.end(), [](auto byte) { return byte == 0xa5; }), "read changed bytes outside its result");
        for (std::int32_t index = 0; index < count; ++index) Check(bytes[index + 1] == Byte(offset + index, alternate), "read returned incorrect payload bytes");
        offset += count;
        size -= count;
    }
}

void End(Aps5HttpClient* client) {
    std::array<std::uint8_t, 3> bytes{0xa5, 0xa5, 0xa5};
    Check(Aps5HttpClient_Read_nid_postfix(client, bytes.data() + 1, 1) == 0, "EOF returned data");
    Check(bytes == std::array<std::uint8_t, 3>{0xa5, 0xa5, 0xa5}, "EOF changed the destination");
}

void WaitForRequest(const std::string& base, const char* kind) {
    auto observer = Create();
    Check(Aps5HttpClient_Open_nid_postfix(observer.get(), (base + "/sync/" + kind).c_str()) == 1, "server did not observe the stalled request");
    std::array<std::uint8_t, 5> ready{};
    Check(Aps5HttpClient_GetSize_nid_postfix(observer.get()) == 5, "server synchronization response length differs");
    Check(Aps5HttpClient_Read_nid_postfix(observer.get(), ready.data(), ready.size()) == 5 && ready == std::array<std::uint8_t, 5>{'r', 'e', 'a', 'd', 'y'}, "server synchronization response differs");
}

void CheckInputValidation(const std::string& base) {
    Aps5HttpClient_Destroy_nid_postfix(nullptr);
    Invalid([&] { Aps5HttpClient_Open_nid_postfix(nullptr, base.c_str()); });
    Invalid([] { Aps5HttpClient_Read_nid_postfix(nullptr, nullptr, 0); });
    Invalid([] { Aps5HttpClient_Seek_nid_postfix(nullptr, 0, 0); });
    Invalid([] { Aps5HttpClient_GetSize_nid_postfix(nullptr); });
    Invalid([] { Aps5HttpClient_Start_nid_postfix(nullptr); });
    Invalid([] { Aps5HttpClient_Stop_nid_postfix(nullptr); });
    auto client = Create();
    Check(Aps5HttpClient_GetSize_nid_postfix(client.get()) == -1 && Aps5HttpClient_Seek_nid_postfix(client.get(), 0, 0) == -1, "closed client exposes a body position or size");
    Check(Aps5HttpClient_Read_nid_postfix(client.get(), nullptr, 0) == 0, "closed zero-capacity read failed");
    std::uint8_t byte = 0xa5;
    bool failed = false;
    try { Aps5HttpClient_Read_nid_postfix(client.get(), &byte, 1); } catch (const std::exception&) { failed = true; }
    Check(failed && byte == 0xa5, "closed read did not reject without writing");
    Check(Aps5HttpClient_Open_nid_postfix(client.get(), (base + "/range/payload.bin").c_str()) == 1, "validation source open failed");
    Read(client.get(), 0, 13);
    Invalid([&] { Aps5HttpClient_Open_nid_postfix(client.get(), nullptr); });
    Invalid([&] { Aps5HttpClient_Read_nid_postfix(client.get(), &byte, -1); });
    Invalid([&] { Aps5HttpClient_Read_nid_postfix(client.get(), nullptr, 1); });
    Check(byte == 0xa5, "invalid read changed the destination");
    Read(client.get(), 13, 17);
    Aps5HttpClient_Stop_nid_postfix(client.get());
    Check(Aps5HttpClient_Read_nid_postfix(client.get(), nullptr, 0) == 0, "stopped zero-capacity read failed");
    Aps5HttpClient_Start_nid_postfix(client.get());
    Read(client.get(), 30, 19);
}

void CheckBodies(const std::string& base) {
    for (const char* route : {"/range/", "/full/", "/full-chunk/", "/full-close/", "/client-redirect/"}) {
        auto client = Create();
        Check(Aps5HttpClient_GetSize_nid_postfix(client.get()) == -1, "unopened client has a body size");
        Check(Aps5HttpClient_Open_nid_postfix(client.get(), (base + route + "payload.bin?token=direct#fragment").c_str()) == 1, "body open failed");
        Check(Aps5HttpClient_GetSize_nid_postfix(client.get()) == 100000, "open did not consume and size the response");
        Check(Aps5HttpClient_Read_nid_postfix(client.get(), nullptr, 0) == 0, "zero-capacity read failed");
        Read(client.get(), 0, 100000);
        End(client.get());
        Check(Aps5HttpClient_Seek_nid_postfix(client.get(), -17, 2) == 99983, "end-relative seek failed");
        Read(client.get(), 99983, 17);
        End(client.get());
    }
}

void CheckCacheAndReopen(const std::string& base) {
    auto client = Create();
    Check(Aps5HttpClient_Open_nid_postfix(client.get(), (base + "/cache/payload.bin").c_str()) == 1, "cache source open failed");
    Read(client.get(), 0, 64);
    Check(Aps5HttpClient_Seek_nid_postfix(client.get(), 0, 0) == 0, "cached seek failed");
    Read(client.get(), 0, 64);
    Check(Aps5HttpClient_Seek_nid_postfix(client.get(), -1, 0) == -1, "negative seek accepted");
    Check(Aps5HttpClient_Seek_nid_postfix(client.get(), 0, 3) == -1, "invalid seek origin accepted");
    Check(Aps5HttpClient_Seek_nid_postfix(client.get(), std::numeric_limits<std::int64_t>::max(), 1) == -1, "overflowing seek accepted");
    Read(client.get(), 64, 16);
    Check(Aps5HttpClient_Open_nid_postfix(client.get(), (base + "/range/alternate.bin").c_str()) == 1, "reopening a client failed");
    Check(Aps5HttpClient_GetSize_nid_postfix(client.get()) == 777, "reopen retained the old body size");
    Read(client.get(), 0, 777, true);
    End(client.get());
    {
        auto other = Create();
        Check(Aps5HttpClient_Open_nid_postfix(other.get(), (base + "/range/payload.bin").c_str()) == 1, "second independent client failed");
        Read(other.get(), 0, 33);
        Check(Aps5HttpClient_Seek_nid_postfix(client.get(), 20, 0) == 20, "independent client position changed");
        Read(client.get(), 20, 21, true);
        Read(other.get(), 33, 19);
    }
    Read(client.get(), 41, 23, true);
}

void CheckFailures(const std::string& base) {
    auto client = Create();
    for (const char* route : {"/bad/", "/short/", "/chunk-bad/"}) Check(Aps5HttpClient_Open_nid_postfix(client.get(), (base + route + "payload.bin").c_str()) == 0, "malformed HTTP response accepted");
    Check(Aps5HttpClient_Open_nid_postfix(client.get(), (base + "/changed/payload.bin").c_str()) == 1, "validator source open failed");
    Check(Aps5HttpClient_Seek_nid_postfix(client.get(), 90000, 0) == 90000, "validator source seek failed");
    bool failed = false;
    std::array<std::uint8_t, 3> bytes{0xa5, 0xa5, 0xa5};
    try { Aps5HttpClient_Read_nid_postfix(client.get(), bytes.data() + 1, 1); } catch (const std::exception&) { failed = true; }
    Check(failed && bytes == std::array<std::uint8_t, 3>{0xa5, 0xa5, 0xa5}, "changed representation did not fail without writing");
    Check(Aps5HttpClient_Open_nid_postfix(client.get(), (base + "/modified/payload.bin").c_str()) == 1, "reopen after failed read failed");
    Check(Aps5HttpClient_Seek_nid_postfix(client.get(), 90000, 0) == 90000, "Last-Modified seek failed");
    Read(client.get(), 90000, 127);
}

void CheckCancellation(const std::string& base) {
    auto client = Create();
    Check(Aps5HttpClient_Open_nid_postfix(client.get(), (base + "/stall/payload.bin").c_str()) == 1, "stalled source open failed");
    Check(Aps5HttpClient_Seek_nid_postfix(client.get(), 90000, 0) == 90000, "stalled source seek failed");
    auto read = std::async(std::launch::async, [&] {
        std::uint8_t byte = 0xa5;
        try { Aps5HttpClient_Read_nid_postfix(client.get(), &byte, 1); } catch (const std::exception&) { return byte == 0xa5; }
        return false;
    });
    WaitForRequest(base, "read");
    Check(read.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout, "stalled read already finished");
    Aps5HttpClient_Stop_nid_postfix(client.get());
    Check(read.wait_for(std::chrono::seconds(1)) == std::future_status::ready && read.get(), "stop did not cancel the read without writing");
    Aps5HttpClient_Start_nid_postfix(client.get());
    Check(Aps5HttpClient_Seek_nid_postfix(client.get(), 0, 0) == 0, "restart seek failed");
    Read(client.get(), 0, 64);
    auto opening = Create();
    auto open = std::async(std::launch::async, [&] { return Aps5HttpClient_Open_nid_postfix(opening.get(), (base + "/stall-open/payload.bin").c_str()); });
    WaitForRequest(base, "open");
    Check(open.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout, "stalled open already finished");
    Aps5HttpClient_Stop_nid_postfix(opening.get());
    Check(open.wait_for(std::chrono::seconds(1)) == std::future_status::ready && open.get() == 0, "stop did not cancel open");
    Aps5HttpClient_Start_nid_postfix(opening.get());
    Check(Aps5HttpClient_Open_nid_postfix(opening.get(), (base + "/range/alternate.bin").c_str()) == 1, "restart after interrupted open failed");
    Read(opening.get(), 0, 777, true);
}

}

int main(int argc, char** argv) {
    try {
        Check(argc == 2, "missing server URL");
        std::filesystem::create_directories("app0");
        Write("payload.bin", 100000);
        Write("alternate.bin", 777, true);
        const std::string base = argv[1];
        CheckInputValidation(base);
        CheckBodies(base);
        CheckCacheAndReopen(base);
        CheckFailures(base);
        CheckCancellation(base);
        std::puts("Reusable HTTP client body, framing, range, cache, reopen, isolation and cancellation tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Reusable HTTP client: %s\n", error.what());
        return 1;
    }
}
