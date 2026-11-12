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

void CheckRangeUnits(const std::string& base) {
    auto invalid = Create();
    for (const char* route : {"/unit/items/", "/unit/BYTES=/", "/range-number/"}) Check(Aps5HttpClient_Open_nid_postfix(invalid.get(), (base + route + "payload.bin").c_str()) == 0, "invalid Content-Range accepted");
    bool opened = true;
    for (const char* unit : {"bytes", "Bytes", "BYTES", "bYtEs"}) {
        auto client = Create();
        if (Aps5HttpClient_Open_nid_postfix(client.get(), (base + "/unit/" + unit + "/payload.bin").c_str()) != 1) {
            std::fprintf(stderr, "Content-Range unit open failed: %s\n", unit);
            opened = false;
            continue;
        }
        Check(Aps5HttpClient_GetSize_nid_postfix(client.get()) == 100000, "range unit body size differs");
        Read(client.get(), 0, 32);
        Check(Aps5HttpClient_Seek_nid_postfix(client.get(), 90000, 0) == 90000, "range unit seek failed");
        Read(client.get(), 90000, 127);
        Check(Aps5HttpClient_GetSize_nid_postfix(client.get()) == 100000, "range unit read changed the body size");
        std::printf("Content-Range unit open, size and range read passed: %s\n", unit);
    }
    Check(opened, "case-insensitive Content-Range unit rejected");
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

void CheckEmpty(const std::string& base) {
    auto client = Create();
    Check(Aps5HttpClient_Open_nid_postfix(client.get(), (base + "/range/empty.bin").c_str()) == 1, "empty source open failed");
    Check(Aps5HttpClient_GetSize_nid_postfix(client.get()) == 0, "empty source size differs");
    End(client.get());
    bool opened = true;
    for (const char* name : {"lower", "title", "upper", "mixed", "padded"}) {
        if (Aps5HttpClient_Open_nid_postfix(client.get(), (base + "/empty-range/" + name + "/empty.bin").c_str()) != 1) {
            std::fprintf(stderr, "Empty Content-Range open failed: %s\n", name);
            opened = false;
            continue;
        }
        Check(Aps5HttpClient_GetSize_nid_postfix(client.get()) == 0, "empty range source size differs");
        Check(Aps5HttpClient_Seek_nid_postfix(client.get(), 0, 2) == 0, "empty range end-relative seek failed");
        End(client.get());
        std::printf("Empty Content-Range open, size, seek and EOF passed: %s\n", name);
    }
    for (const char* name : {"other-unit", "bad-unit", "missing-length", "nonzero", "negative", "signed", "suffix",
                             "spacing", "unknown", "overflow", "satisfied", "missing-header"}) {
        Check(Aps5HttpClient_Open_nid_postfix(client.get(), (base + "/empty-range/" + name + "/empty.bin").c_str()) == 0, "invalid empty Content-Range accepted");
        Check(Aps5HttpClient_GetSize_nid_postfix(client.get()) == -1, "refused empty range retained the previous source");
    }
    Check(Aps5HttpClient_Open_nid_postfix(client.get(), (base + "/range/alternate.bin").c_str()) == 1, "reopen after empty source failed");
    Read(client.get(), 0, 777, true);
    Check(opened, "valid empty Content-Range rejected");
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

void CheckRedirects(const std::string& base) {
    bool opened = true;
    for (const char* reference : {"absolute", "network", "relative", "root", "query", "empty-query", "encoded", "absolute-slashes", "network-slashes", "absolute-empty", "network-empty"}) {
        auto client = Create();
        if (Aps5HttpClient_Open_nid_postfix(client.get(), (base + "/client-reference/" + reference + "?token=direct").c_str()) != 1) {
            std::fprintf(stderr, "Redirect reference failed: %s\n", reference);
            opened = false;
            continue;
        }
        Check(Aps5HttpClient_GetSize_nid_postfix(client.get()) == 100000, "redirect body size differs");
        Read(client.get(), 0, 32);
        Check(Aps5HttpClient_Seek_nid_postfix(client.get(), 90000, 0) == 90000, "redirect body seek failed");
        Read(client.get(), 90000, 32);
    }
    Check(opened, "redirect reference open failed");
    auto client = Create();
    Check(Aps5HttpClient_Open_nid_postfix(client.get(), (base + "/client-unsupported/scheme").c_str()) == 0, "unsupported redirect scheme accepted");
    for (const char* reference : {"https", "userinfo"}) {
        bool failed = false;
        try { Aps5HttpClient_Open_nid_postfix(client.get(), (base + "/client-unsupported/" + reference).c_str()); }
        catch (const std::runtime_error&) { failed = true; }
        Check(failed, "unsupported redirect did not throw");
    }
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

void CheckRepresentations(const std::string& base) {
    bool passed = true;
    int legacyPassed = 0;
    int legacySnapshots = 0;
    const auto run = [&](const char* name, auto action) {
        try {
            action(base + "/identity/" + name + "/payload.bin");
        } catch (const std::exception& error) {
            std::fprintf(stderr, "HTTP representation %s: %s\n", name, error.what());
            passed = false;
        }
    };
    for (const char* name : {"etag-206-omit", "date-206-omit", "date-boundary", "etag-200-match", "date-200-match", "both-200-match",
                             "date-leap-second", "date-minimum-year", "etag-initial-trailer", "etag-206-trailer",
                             "etag-206-trailer-framing", "etag-200-trailer", "etag-200-mixed-trailer"}) {
        run(name, [&](const std::string& url) {
            auto client = Create();
            Check(Aps5HttpClient_Open_nid_postfix(client.get(), url.c_str()) == 1, "validated source open failed");
            Read(client.get(), 0, 31);
            Check(Aps5HttpClient_GetSize_nid_postfix(client.get()) == 100000, "validated source size differs");
            Check(Aps5HttpClient_Seek_nid_postfix(client.get(), 90000, 0) == 90000, "validated source seek failed");
            Read(client.get(), 90000, 67);
            Check(Aps5HttpClient_Seek_nid_postfix(client.get(), 0, 0) == 0, "validated source rewind failed");
            Read(client.get(), 0, 31);
        });
    }
    for (const char* name : {"date-rfc850", "date-asctime", "date-legacy-rfc850-date", "date-legacy-asctime-date",
                             "date-legacy-rfc850-both", "date-legacy-asctime-both", "date-legacy-asctime-two-digit",
                             "date-legacy-century", "date-legacy-200-match", "date-legacy-206-omit"}) {
        run(name, [&](const std::string& url) {
            auto client = Create();
            Check(Aps5HttpClient_Open_nid_postfix(client.get(), url.c_str()) == 1, "legacy date source open failed");
            Check(Aps5HttpClient_GetSize_nid_postfix(client.get()) == 100000, "legacy date source exposed a fallback snapshot");
            Read(client.get(), 0, 31);
            Check(Aps5HttpClient_Seek_nid_postfix(client.get(), 90000, 0) == 90000, "legacy date source seek failed");
            Read(client.get(), 90000, 67);
            Check(Aps5HttpClient_Seek_nid_postfix(client.get(), 0, 0) == 0, "legacy date source rewind failed");
            Read(client.get(), 0, 31);
            ++legacyPassed;
        });
    }
    for (const char* name : {"etag-200-absent", "etag-200-changed", "etag-200-weak", "etag-200-invalid", "date-200-absent", "date-200-changed", "both-200-absent", "etag-206-changed", "date-206-changed", "late-size", "late-range", "late-short", "late-chunk", "late-redirect-path", "late-redirect-query", "late-redirect-host",
                             "etag-200-trailer-changed", "etag-200-trailer-weak", "etag-200-trailer-conflict",
                             "etag-206-trailer-conflict", "etag-206-trailer-changed", "etag-200-trailer-duplicate", "etag-200-trailer-malformed",
                             "etag-200-trailer-name", "date-200-trailer-only", "date-legacy-206-changed"}) {
        run(name, [&](const std::string& url) {
            auto client = Create();
            Check(Aps5HttpClient_Open_nid_postfix(client.get(), url.c_str()) == 1, "changing source open failed");
            Read(client.get(), 0, 31);
            Check(Aps5HttpClient_Seek_nid_postfix(client.get(), 90000, 0) == 90000, "changing source seek failed");
            std::array<std::uint8_t, 10> bytes;
            bytes.fill(0xa5);
            bool failed = false;
            try { Aps5HttpClient_Read_nid_postfix(client.get(), bytes.data() + 1, 8); }
            catch (const std::exception&) { failed = true; }
            Check(failed, "an invalid or changed response was accepted");
            Check(std::all_of(bytes.begin(), bytes.end(), [](auto byte) { return byte == 0xa5; }), "failed read changed the destination");
            Check(Aps5HttpClient_GetSize_nid_postfix(client.get()) == 100000, "failed read changed the source size");
            Check(Aps5HttpClient_Seek_nid_postfix(client.get(), 0, 1) == 90000, "failed read changed the source position");
            Check(Aps5HttpClient_Seek_nid_postfix(client.get(), 0, 0) == 0, "failed read lost the cached position");
            Read(client.get(), 0, 31);
            Check(Aps5HttpClient_Seek_nid_postfix(client.get(), 90000, 0) == 90000, "retry seek failed");
            Read(client.get(), 90000, 67);
            Check(Aps5HttpClient_GetSize_nid_postfix(client.get()) == 100000, "retry changed the source size");
            Check(Aps5HttpClient_Open_nid_postfix(client.get(), (base + "/range/alternate.bin").c_str()) == 1, "reopen after changing source failed");
            Read(client.get(), 0, 777, true);
        });
    }
    for (const char* name : {"absent", "absent-chunk", "absent-close", "weak", "weak-date", "invalid-tag", "tag-space", "tag-quote", "tag-unclosed", "recent-date", "invalid-date", "date-without-date", "complete-range", "initial-full", "initial-chunk", "initial-close",
                             "date-short-year", "date-short-day", "date-short-time", "date-full-names", "date-wrong-case",
                             "date-wrong-weekday", "date-early-year", "date-invalid-day", "date-short-response-time",
                             "date-wrong-response-weekday", "date-extra-response-space",
                             "date-legacy-rfc850-weekday", "date-legacy-asctime-weekday", "date-legacy-rfc850-month", "date-legacy-asctime-month",
                             "date-legacy-rfc850-recent", "date-legacy-asctime-recent", "date-legacy-rfc850-short-year", "date-legacy-asctime-short-year",
                             "date-legacy-rfc850-time", "date-legacy-asctime-day",
                             "date-trailer-only", "weak-trailer-date"}) {
        run(name, [&](const std::string& url) {
            auto client = Create();
            Check(Aps5HttpClient_Open_nid_postfix(client.get(), url.c_str()) == 1, "snapshot source open failed");
            Check(Aps5HttpClient_GetSize_nid_postfix(client.get()) == 777, "open exposed the unvalidated partial response");
            Read(client.get(), 0, 777, true);
            End(client.get());
            Check(Aps5HttpClient_Seek_nid_postfix(client.get(), -77, 2) == 700, "snapshot end seek failed");
            Read(client.get(), 700, 77, true);
            Check(Aps5HttpClient_Seek_nid_postfix(client.get(), 0, 0) == 0, "snapshot rewind failed");
            Read(client.get(), 0, 777, true);
            End(client.get());
            if (std::string(name).starts_with("date-legacy-")) ++legacySnapshots;
        });
    }
    for (const char* name : {"fallback-206", "fallback-short", "fallback-chunk", "fallback-refused"}) {
        run(name, [&](const std::string& url) {
            auto client = Create();
            Check(Aps5HttpClient_Open_nid_postfix(client.get(), (base + "/range/payload.bin").c_str()) == 1, "fallback control open failed");
            Read(client.get(), 0, 31);
            Check(Aps5HttpClient_Open_nid_postfix(client.get(), url.c_str()) == 0, "invalid full fallback was accepted");
            Check(Aps5HttpClient_GetSize_nid_postfix(client.get()) == -1, "failed fallback left a source open");
            Check(Aps5HttpClient_Seek_nid_postfix(client.get(), 0, 0) == -1, "failed fallback retained a position");
            std::uint8_t byte = 0xa5;
            bool failed = false;
            try { Aps5HttpClient_Read_nid_postfix(client.get(), &byte, 1); }
            catch (const std::exception&) { failed = true; }
            Check(failed && byte == 0xa5, "failed fallback retained readable bytes");
            Check(Aps5HttpClient_Open_nid_postfix(client.get(), (base + "/range/alternate.bin").c_str()) == 1, "reopen after failed fallback failed");
            Read(client.get(), 0, 777, true);
            End(client.get());
        });
    }
    std::printf("HTTP legacy date cases: %d/10; conservative snapshots: %d/10\n", legacyPassed, legacySnapshots);
    Check(passed && legacyPassed == 10 && legacySnapshots == 10, "HTTP representation integrity cases failed");
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
        Check(argc == 2 || (argc == 3 && std::string(argv[2]) == "--transformed"), "invalid server arguments");
        std::filesystem::create_directories("app0");
        Write("payload.bin", 100000);
        Write("alternate.bin", 777, true);
        Write("empty.bin", 0);
        const std::string base = argv[1];
        if (argc == 3) {
            CheckRepresentations(base + "/transformed");
            std::puts("HTTP 203 representation tests passed");
            return 0;
        }
        CheckInputValidation(base);
        CheckBodies(base);
        CheckEmpty(base);
        CheckRedirects(base);
        CheckCacheAndReopen(base);
        CheckFailures(base);
        CheckRepresentations(base);
        CheckCancellation(base);
        CheckRangeUnits(base);
        std::puts("Reusable HTTP client body, framing, range, cache, reopen, isolation and cancellation tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Reusable HTTP client: %s\n", error.what());
        return 1;
    }
}
