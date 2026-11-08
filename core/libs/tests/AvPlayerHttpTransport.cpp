#include "prx/libSceAvPlayer/include/HttpStream.hpp"
#include "prx/libSceHttp/libSceHttp.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>

extern "C" {
#include <libavformat/avio.h>
}

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "AvPlayer HTTP transport: %s\n", message);
    std::abort();
}

int main(int argc, char** argv) {
    Require(argc == 2, "missing server URL");
    std::filesystem::create_directories("app0");
    {
        std::ofstream file("app0/payload.bin", std::ios::binary);
        for (int byte = 0; byte < 100000; ++byte) file.put(static_cast<char>(byte % 251));
    }
    {
        AvPlayer::HttpStream invalid;
        auto url = std::string(argv[1]) + "/range/payload.bin";
        url.push_back('\0');
        url += "suffix";
        Require(!invalid.Open(url), "embedded NUL URL was truncated into a valid source");
    }
    AvPlayer::HttpStream stream;
    Require(stream.Open(std::string(argv[1]) + "/stall/payload.bin"), "opening range source failed");
    Require(avio_size(stream.Context()) == 100000, "range source length is incorrect");
    Require(avio_seek(stream.Context(), 90000, SEEK_SET) == 90000, "seeking range source failed");
    std::array<std::uint8_t, 64> bytes{};
    auto read = std::async(std::launch::async, [&] { return avio_read(stream.Context(), bytes.data(), bytes.size()); });
    {
        std::unique_ptr<Aps5HttpClient, decltype(&Aps5HttpClient_Destroy_nid_postfix)> observer(Aps5HttpClient_Create_nid_postfix(), Aps5HttpClient_Destroy_nid_postfix);
        Require(observer != nullptr && Aps5HttpClient_Open_nid_postfix(observer.get(), (std::string(argv[1]) + "/sync/read").c_str()) == 1, "server did not observe the stalled read");
    }
    Require(read.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout, "stalled source did not block");
    stream.Stop();
    Require(read.wait_for(std::chrono::seconds(1)) == std::future_status::ready && read.get() < 0, "stop did not cancel a stalled read");
    bool cancelled = false;
    try { stream.Rethrow(); } catch (const std::exception&) { cancelled = true; }
    Require(cancelled, "cancelled callback failure was not retained");
    stream.Start();
    stream.Rethrow();
    Require(avio_seek(stream.Context(), 0, SEEK_SET) == 0, "restart seek failed");
    Require(avio_read(stream.Context(), bytes.data(), bytes.size()) == bytes.size(), "restart read failed");
    for (std::size_t byte = 0; byte < bytes.size(); ++byte) Require(bytes[byte] == byte, "restart returned incorrect bytes");
    AvPlayer::HttpStream changed;
    Require(changed.Open(std::string(argv[1]) + "/changed/payload.bin"), "opening validator source failed");
    Require(avio_seek(changed.Context(), 90000, SEEK_SET) == 90000, "validator seek failed");
    Require(avio_read(changed.Context(), bytes.data(), bytes.size()) < 0, "changed representation was mixed into cache");
    bool threw = false;
    try { changed.Rethrow(); } catch (...) { threw = true; }
    Require(threw, "callback failure was not retained");
    AvPlayer::HttpStream modified;
    Require(modified.Open(std::string(argv[1]) + "/modified/payload.bin"), "opening Last-Modified source failed");
    Require(avio_seek(modified.Context(), 90000, SEEK_SET) == 90000, "Last-Modified seek failed");
    Require(avio_read(modified.Context(), bytes.data(), bytes.size()) == bytes.size(), "Last-Modified If-Range read failed");
    for (std::size_t byte = 0; byte < bytes.size(); ++byte) Require(bytes[byte] == (90000 + byte) % 251, "Last-Modified range returned incorrect bytes");
    Require(modified.Open(std::string(argv[1]) + "/range/payload.bin"), "reopening the AVIO adapter failed");
    Require(avio_read(modified.Context(), bytes.data(), bytes.size()) == bytes.size(), "reopened adapter read failed");
    for (std::size_t byte = 0; byte < bytes.size(); ++byte) Require(bytes[byte] == byte, "reopened adapter retained its old position");
}
