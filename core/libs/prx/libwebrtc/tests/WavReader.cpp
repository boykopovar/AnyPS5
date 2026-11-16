#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/Api.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition)
        return;
    std::fprintf(stderr, "libwebrtc WavReader check failed at line %u\n", location.line());
    std::abort();
}

template<typename TException, typename TAction> void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const TException&) {
        return;
    }
    Require(false, location);
}

class GuardedPage {
public:
    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = VirtualAlloc(nullptr, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        Require(base != nullptr);
#else
        pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        base = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(base != MAP_FAILED);
#endif
    }

    ~GuardedPage() {
        if (!base)
            return;
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, pageSize);
#endif
    }

    void* Data() const {
        return base;
    }

    void MakeReadOnly() const {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(base, pageSize, PAGE_READONLY, &previous) != 0);
#else
        Require(mprotect(base, pageSize, PROT_READ) == 0);
#endif
    }

    void MakeInaccessible() const {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(base, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        Require(mprotect(base, pageSize, PROT_NONE) == 0);
#endif
    }

    void MakeReadWrite() const {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(base, pageSize, PAGE_READWRITE, &previous) != 0);
#else
        Require(mprotect(base, pageSize, PROT_READ | PROT_WRITE) == 0);
#endif
    }

private:
    std::size_t pageSize = 0;
    void* base = nullptr;
};

class DataFile {
public:
    DataFile(const void* bytes, std::size_t size) : name("anyps5-wav-reader-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin") {
        std::ofstream file(name, std::ios::binary);
        file.write(static_cast<const char*>(bytes), static_cast<std::streamsize>(size));
    }
    ~DataFile() {
        std::error_code error;
        std::filesystem::remove(name, error);
    }
    const std::string& Name() const { return name; }

private:
    std::string name;
};

constexpr std::size_t HeaderSize = 44;

WebRtcWavReader Reader(const DataFile& file, std::int32_t format, std::size_t samples) {
    WebRtcWavReader reader{};
    reader.format = format;
    reader.numSamplesInFile = samples;
    reader.numUnreadSamples = samples;
    reader.dataStartPos = HeaderSize;
    reader.file.file = fopen_nid_postfix(file.Name().c_str(), "rb");
    Require(reader.file.file != nullptr);
    Require(fseek_nid_postfix(reader.file.file, HeaderSize, 0) == 0);
    return reader;
}

std::vector<unsigned char> WithHeader(const void* payload, std::size_t size) {
    std::vector<unsigned char> bytes(HeaderSize + size, 0xEE);
    if (size != 0)
        std::memcpy(bytes.data() + HeaderSize, payload, size);
    return bytes;
}

void TestPcm() {
    std::vector<std::int16_t> pcm(10000);
    for (std::size_t i = 0; i < pcm.size(); ++i)
        pcm[i] = static_cast<std::int16_t>(static_cast<int>(i) * 7 - 20000);
    const auto bytes = WithHeader(pcm.data(), pcm.size() * sizeof(std::int16_t));
    DataFile file(bytes.data(), bytes.size());
    WebRtcWavReader reader = Reader(file, 1, pcm.size());

    std::vector<std::int16_t> out(pcm.size() + 5, 0x1234);
    Require(_ZN6webrtc9WavReader11ReadSamplesEmPs_nid_postfix(&reader, 4097, out.data()) == 4097);
    Require(reader.numUnreadSamples == pcm.size() - 4097);
    Require(std::memcmp(out.data(), pcm.data(), 4097 * sizeof(std::int16_t)) == 0);
    Require(_ZN6webrtc9WavReader11ReadSamplesEmPs_nid_postfix(&reader, 0, nullptr) == 0);
    Require(_ZN6webrtc9WavReader11ReadSamplesEmPs_nid_postfix(&reader, 100000, out.data() + 4097) == pcm.size() - 4097);
    Require(reader.numUnreadSamples == 0);
    Require(std::memcmp(out.data(), pcm.data(), pcm.size() * sizeof(std::int16_t)) == 0);
    Require(out[pcm.size()] == 0x1234);
    Require(_ZN6webrtc9WavReader11ReadSamplesEmPs_nid_postfix(&reader, 10, out.data()) == 0);

    _ZN6webrtc9WavReader5ResetEv_nid_postfix(&reader);
    Require(reader.numUnreadSamples == pcm.size());
    std::vector<std::int16_t> again(100);
    Require(_ZN6webrtc9WavReader11ReadSamplesEmPs_nid_postfix(&reader, 100, again.data()) == 100);
    Require(std::memcmp(again.data(), pcm.data(), 100 * sizeof(std::int16_t)) == 0);
    Require(reader.numUnreadSamples == pcm.size() - 100);

    _ZN6webrtc9WavReader5CloseEv_nid_postfix(&reader);
    Require(reader.file.file == nullptr);
}

void TestFloat() {
    const float values[] = {0.0f, 0.5f, -0.5f, 1.0f, -1.0f, 2.0f, -2.0f, 1.0f / 65536.0f, -1.0f / 65536.0f, 0.25f};
    const auto bytes = WithHeader(values, sizeof(values));
    DataFile file(bytes.data(), bytes.size());
    WebRtcWavReader reader = Reader(file, 3, 10);
    std::int16_t out[10];
    Require(_ZN6webrtc9WavReader11ReadSamplesEmPs_nid_postfix(&reader, 10, out) == 10);
    const std::int16_t expected[] = {0, 16384, -16384, 32767, -32768, 32767, -32768, 1, -1, 8192};
    Require(std::memcmp(out, expected, sizeof(expected)) == 0);
    Require(reader.numUnreadSamples == 0);
    _ZN6webrtc9WavReader5CloseEv_nid_postfix(&reader);
}

void TestGuestPointerGuards() {
    const std::int16_t pcm[4] = {10, 20, 30, 40};
    const auto bytes = WithHeader(pcm, sizeof(pcm));
    DataFile readOnlyData(bytes.data(), bytes.size());

    GuardedPage page;
    auto* guardedReader = static_cast<WebRtcWavReader*>(page.Data());
    *guardedReader = Reader(readOnlyData, 1, 4);
    page.MakeReadOnly();

    errno = 321;
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc9WavReader5ResetEv_nid_postfix(guardedReader); });
    Require(errno == 321);
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc9WavReader11ReadSamplesEmPs_nid_postfix(guardedReader, 2, nullptr); });
    std::int16_t validOut[4]{};
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc9WavReader11ReadSamplesEmPs_nid_postfix(guardedReader, 2, validOut); });
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc9WavReader5CloseEv_nid_postfix(guardedReader); });
    Require(errno == 321);

    page.MakeInaccessible();
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc9WavReader5ResetEv_nid_postfix(guardedReader); });
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc9WavReader11ReadSamplesEmPs_nid_postfix(guardedReader, 2, validOut); });
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc9WavReader5CloseEv_nid_postfix(guardedReader); });

    page.MakeReadWrite();
    Require(guardedReader->file.file != nullptr);
    Require(guardedReader->numUnreadSamples == 4);

    GuardedPage samplePage;
    samplePage.MakeReadOnly();
    auto* roSamples = static_cast<std::int16_t*>(samplePage.Data());
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc9WavReader11ReadSamplesEmPs_nid_postfix(guardedReader, 2, roSamples); });
    Require(guardedReader->numUnreadSamples == 4);

    _ZN6webrtc9WavReader5CloseEv_nid_postfix(guardedReader);
    Require(guardedReader->file.file == nullptr);

    const std::string outName = "anyps5-file-wrapper-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin";
    WebRtcFileWrapper writer{fopen_nid_postfix(outName.c_str(), "wb")};
    Require(writer.file != nullptr);

    GuardedPage dataPage;
    std::memcpy(dataPage.Data(), "abcd", 4);
    dataPage.MakeInaccessible();
    errno = 456;
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc11FileWrapper5WriteEPKvm_nid_postfix(&writer, dataPage.Data(), 4); });
    Require(errno == 456);
    Require(_ZN6webrtc11FileWrapper5WriteEPKvm_nid_postfix(&writer, "xy", 2));
    Require(_ZN6webrtc11FileWrapper5FlushEv_nid_postfix(&writer));

    auto* guardedWrapper = static_cast<WebRtcFileWrapper*>(page.Data());
    *guardedWrapper = writer;
    page.MakeReadOnly();
    WebRtcFileWrapper moved{};
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc11FileWrapperC1EOS0__nid_postfix(&moved, guardedWrapper); });
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc11FileWrapper5CloseEv_nid_postfix(guardedWrapper); });

    page.MakeReadWrite();
    _ZN6webrtc11FileWrapperC1EOS0__nid_postfix(&moved, guardedWrapper);
    Require(guardedWrapper->file == nullptr && moved.file != nullptr);
    Require(_ZN6webrtc11FileWrapper5CloseEv_nid_postfix(&moved));
    Require(moved.file == nullptr);

    std::error_code ec;
    std::filesystem::remove(outName, ec);
}

}

int main() {
    TestPcm();
    TestFloat();
    TestGuestPointerGuards();
    return 0;
}
