#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/Api.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

void Require(bool condition, int line = __builtin_LINE()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc WavReader check failed at line %d\n", line);
    std::abort();
}

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
    std::vector<unsigned char> bytes(HeaderSize, 0xEE);
    bytes.insert(bytes.end(), static_cast<const unsigned char*>(payload), static_cast<const unsigned char*>(payload) + size);
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

}

int main() {
    TestPcm();
    TestFloat();
    return 0;
}
