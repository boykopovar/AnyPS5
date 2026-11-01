#include "prx/libScePlayReady/libScePlayReady.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {

constexpr int InvalidArgument = static_cast<int>(0x80070057);

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

bool Filled(const unsigned char* bytes, std::size_t begin, std::size_t end, unsigned char value) {
    for (std::size_t i = begin; i < end; ++i)
        if (bytes[i] != value) return false;
    return true;
}

void CheckBufferInit() {
    for (unsigned char fill : {0xA5, 0x5A}) {
        unsigned char buffer[48];
        std::memset(buffer, fill, sizeof(buffer));
        sceBufferInit(buffer);
        Require(Filled(buffer, 0, 24, 0) && Filled(buffer, 24, sizeof(buffer), fill), "sceBufferInit clears exactly 24 bytes");
        std::memset(buffer, fill, sizeof(buffer));
        Require(scePlayReadyBufferInit(buffer) == 0, "scePlayReadyBufferInit succeeds");
        Require(Filled(buffer, 0, 24, 0) && Filled(buffer, 24, sizeof(buffer), fill), "scePlayReadyBufferInit clears exactly 24 bytes");
    }
    sceBufferInit(nullptr);
    Require(scePlayReadyBufferInit(nullptr) == InvalidArgument, "scePlayReadyBufferInit rejects a null buffer");
}

void CheckInitParams() {
    using Initialize = void (APS5_VABI*)(void*);
    for (Initialize initialize : {scePlayReadyApiInitializeInitParams, scePlayReadyInitializeInitParams}) {
        for (unsigned char fill : {0xA5, 0x5A}) {
            unsigned char params[0x150];
            std::memset(params, fill, sizeof(params));
            initialize(params);
            uint32_t magic = 0;
            uint32_t version = 0;
            std::memcpy(&magic, params, sizeof(magic));
            std::memcpy(&version, params + 4, sizeof(version));
            Require(magic == 0x50524459 && version == 1, "init params start with 0x50524459 and 1");
            Require(Filled(params, 8, 0x138, 0), "init params clear bytes 8 to 0x138");
            Require(Filled(params, 0x138, sizeof(params), fill), "init params leave bytes from 0x138 untouched");
        }
        initialize(nullptr);
    }
}

void CheckVersion() {
    const uint64_t expected[2][4] = {{2, 5, 0, 1788}, {1, 3, 3, 2021111201}};
    for (uint32_t index = 0; index < 2; ++index) {
        uint64_t version[6];
        std::memset(version, 0xA5, sizeof(version));
        Require(scePlayReadyApiVersion(index, version) == 0, "scePlayReadyApiVersion succeeds for index 0 and 1");
        Require(std::memcmp(version, expected[index], sizeof(expected[index])) == 0, "scePlayReadyApiVersion writes the version");
        Require(Filled(reinterpret_cast<unsigned char*>(version), 32, sizeof(version), 0xA5), "scePlayReadyApiVersion writes 32 bytes");
        Require(scePlayReadyApiVersion(index, nullptr) == InvalidArgument, "scePlayReadyApiVersion rejects a null output");
    }
    for (uint32_t index : {2u, 3u, 0x80000000u, 0xFFFFFFFFu}) {
        uint64_t version[4];
        std::memset(version, 0xA5, sizeof(version));
        Require(scePlayReadyApiVersion(index, version) == InvalidArgument, "scePlayReadyApiVersion rejects an index above 1");
        Require(Filled(reinterpret_cast<unsigned char*>(version), 0, sizeof(version), 0xA5), "a rejected index writes nothing");
    }
}

void CheckVersionString() {
    const char* first = scePlayReadyApiVersionString(0);
    Require(std::strcmp(first, "2.5.0.1788") == 0, "version string 0");
    const char* second = scePlayReadyApiVersionString(1);
    Require(second == first, "version strings share one buffer");
    Require(std::strcmp(second, "1.3.3.2021111201") == 0, "version string 1");
    Require(std::strcmp(scePlayReadyApiVersionString(0), "2.5.0.1788") == 0, "version string 0 again");
    Require(std::memcmp(first + 11, "11201", 6) == 0, "a shorter version string leaves the earlier tail after its terminator");
    for (uint32_t index : {2u, 0x80000000u, 0xFFFFFFFFu}) {
        const char* text = scePlayReadyApiVersionString(index);
        Require(text == first, "an unknown index returns the same buffer");
        Require(Filled(reinterpret_cast<const unsigned char*>(text), 0, 32, 0), "an unknown index clears the 32-byte buffer");
    }
}

} // namespace

int main() {
    CheckBufferInit();
    CheckInitParams();
    CheckVersion();
    CheckVersionString();
    return 0;
}
