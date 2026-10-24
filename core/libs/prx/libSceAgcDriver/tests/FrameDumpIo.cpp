#include "prx/libSceAgcDriver/Execution/include/FrameDumpIo.hpp"
#include <array>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(1);
    }
}

void Close(int descriptor) {
#ifdef _WIN32
    _close(descriptor);
#else
    close(descriptor);
#endif
}

int Read(int descriptor, void* bytes, unsigned count) {
#ifdef _WIN32
    return _read(descriptor, bytes, count);
#else
    return static_cast<int>(read(descriptor, bytes, count));
#endif
}

std::FILE* Pipe(int& reader) {
    int descriptors[2];
#ifdef _WIN32
    Check(_pipe(descriptors, 4096, _O_BINARY) == 0, "cannot create test pipe");
    auto* file = _fdopen(descriptors[1], "wb");
#else
    Check(pipe(descriptors) == 0, "cannot create test pipe");
    auto* file = fdopen(descriptors[1], "wb");
#endif
    Check(file != nullptr, "cannot open test pipe writer");
    reader = descriptors[0];
    return file;
}

void SuccessfulBytes() {
    auto* file = std::tmpfile();
    Check(file != nullptr, "cannot create BMP test file");
    const std::array<std::byte, 16> pixels{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    Check(!AgcDriver::WriteBmpStream_nid_no_patch(file, 2, 2, pixels), "valid BMP write failed");
    Check(std::fflush(file) == 0, "valid BMP flush failed");
    std::rewind(file);
    std::array<std::byte, 70> actual{};
    Check(std::fread(actual.data(), 1, actual.size(), file) == actual.size(), "BMP has the wrong byte count");
    const std::array<unsigned char, 54> header{
        0x42, 0x4d, 70, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0,
        40, 0, 0, 0, 2, 0, 0, 0, 0xfe, 0xff, 0xff, 0xff,
        1, 0, 32, 0, 0, 0, 0, 0, 16, 0, 0, 0,
        0x13, 0x0b, 0, 0, 0x13, 0x0b, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    Check(std::memcmp(actual.data(), header.data(), header.size()) == 0, "BMP header changed");
    Check(std::memcmp(actual.data() + header.size(), pixels.data(), pixels.size()) == 0, "BMP pixel payload changed");
    Check(std::fclose(file) == 0, "BMP test close failed");
}

void HeaderFailure() {
    int reader;
    auto* file = Pipe(reader);
    Check(std::setvbuf(file, nullptr, _IONBF, 0) == 0, "cannot disable pipe buffering");
    Close(reader);
    const std::array<std::byte, 16> pixels{};
    const auto failure = AgcDriver::WriteBmpAndClose_nid_no_patch(file, 2, 2, pixels);
    Check(failure && std::strcmp(failure.operation, "header") == 0, "closed pipe header write was accepted");
}

void PixelFailure() {
    int reader;
    auto* file = Pipe(reader);
    Check(std::setvbuf(file, nullptr, _IONBF, 0) == 0, "cannot disable pipe buffering");
    unsigned received = 0;
    std::thread consumer([&] {
        std::array<std::byte, 54> header{};
        while (received < header.size()) {
            const auto count = Read(reader, header.data() + received, static_cast<unsigned>(header.size() - received));
            if (count <= 0) break;
            received += static_cast<unsigned>(count);
        }
        Close(reader);
    });
    const std::vector<std::byte> pixels(512 * 512 * 4);
    const auto failure = AgcDriver::WriteBmpAndClose_nid_no_patch(file, 512, 512, pixels);
    consumer.join();
    Check(received == 54, "pixel failure did not follow a complete BMP header");
    Check(failure && std::strcmp(failure.operation, "pixels") == 0, "closed pipe pixel write was accepted");
}

void CloseFailure() {
    int reader;
    auto* file = Pipe(reader);
    std::array<char, 4096> buffer{};
    Check(std::setvbuf(file, buffer.data(), _IOFBF, buffer.size()) == 0, "cannot buffer test pipe");
    Close(reader);
    const std::array<std::byte, 16> pixels{};
    const auto failure = AgcDriver::WriteBmpAndClose_nid_no_patch(file, 2, 2, pixels);
    Check(failure && std::strcmp(failure.operation, "close") == 0, "failed buffered close was accepted");
}

void DisableAfterFailure() {
    const auto directory = std::filesystem::temp_directory_path() / ("anyps5-bmp-io-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Check(std::filesystem::create_directory(directory), "cannot create output-failure fixture");
    AgcDriver::FrameDumpOutput output;
    const std::array<std::byte, 16> pixels{};
    Check(output.Enabled_nid_no_patch(), "BMP output was disabled before any failure");
    const auto blocked = directory.string();
    Check(!output.Write_nid_no_patch(blocked.c_str(), 2, 2, pixels), "BMP output accepted a directory as a file");
    Check(!output.Enabled_nid_no_patch(), "failed BMP output remained enabled");
    const auto skipped = directory / "must-not-exist.bmp";
    const auto path = skipped.string();
    for (unsigned i = 0; i < 64; ++i) Check(!output.Write_nid_no_patch(path.c_str(), 2, 2, pixels), "disabled BMP output wrote another file");
    Check(!std::filesystem::exists(skipped), "disabled BMP output created a file");
    Check(std::filesystem::remove(directory), "cannot remove empty fixture directory");
}

}

int main() {
#ifndef _WIN32
    const auto previous = std::signal(SIGPIPE, SIG_IGN);
#endif
    SuccessfulBytes();
    HeaderFailure();
    PixelFailure();
    CloseFailure();
    DisableAfterFailure();
#ifndef _WIN32
    std::signal(SIGPIPE, previous);
#endif
    std::puts("frame dump I/O tests passed: bytes/header/pixels/close/disable");
    return 0;
}
