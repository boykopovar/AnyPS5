#include "SceTypes.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <source_location>

extern "C" {
int APS5_VABI sceAjmInitialize(int64_t reserved, uint32_t* context);
int APS5_VABI sceAjmFinalize(uint32_t context);
int APS5_VABI sceAjmModuleRegister(uint32_t context, uint32_t codec, int64_t reserved);
int APS5_VABI sceAjmInstanceCreate(uint32_t context, uint32_t codec, uint64_t flags, uint32_t* instance);
int APS5_VABI sceAjmInstanceDestroy(uint32_t context, uint32_t instance);
int APS5_VABI sceAjmDecAt9ParseConfigData(const void* config_data, AjmDecAt9ConfigDataInfo* config_info);
const char* APS5_VABI sceAjmStrError(int error);
}

namespace {

constexpr std::uint32_t CodecAt9 = 1;
constexpr int InvalidInstance = static_cast<int>(0x80930003);
constexpr int InvalidParameter = static_cast<int>(0x80930005);

int failures = 0;

void Require(const bool condition, const char* what, const std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "AjmWrapper: %s at %s:%u\n", what, location.file_name(), static_cast<unsigned>(location.line()));
    ++failures;
}

}

int main() {
    std::uint32_t context = 0xFFFFFFFFu;
    Require(sceAjmInitialize(0, &context) == 0, "sceAjmInitialize did not reach the native module");
    Require(sceAjmInitialize(0, nullptr) == InvalidParameter, "a null context was accepted");
    Require(sceAjmModuleRegister(context, CodecAt9, 0) == 0, "the ATRAC9 module did not register");

    std::uint32_t instance = 0xFFFFFFFFu;
    Require(sceAjmInstanceCreate(context, CodecAt9, 0, &instance) == 0, "an ATRAC9 instance was not created");
    Require(sceAjmInstanceCreate(context, CodecAt9, 0, nullptr) == InvalidParameter, "a null instance pointer was accepted");
    Require(sceAjmInstanceDestroy(context, instance) == 0, "the instance was not destroyed");
    Require(sceAjmInstanceDestroy(context, instance) == InvalidInstance, "destroying twice was accepted");

    AjmDecAt9ConfigDataInfo info{};
    const unsigned char invalid[4] = {0, 0, 0, 0};
    Require(sceAjmDecAt9ParseConfigData(invalid, &info) == InvalidParameter, "an invalid ATRAC9 configuration was accepted");
    Require(sceAjmDecAt9ParseConfigData(nullptr, &info) == InvalidParameter, "a null configuration was accepted");

    bool strErrorIsUnimplemented = false;
    try {
        (void)sceAjmStrError(0);
    } catch (const std::exception& exception) {
        strErrorIsUnimplemented = std::strcmp(exception.what(), "sceAjmStrError not implemented") == 0;
    }
    Require(strErrorIsUnimplemented, "sceAjmStrError did not report its unimplemented status");

    Require(sceAjmFinalize(context) == 0, "sceAjmFinalize did not reach the native module");
    if (failures != 0) return 1;
    std::printf("Ajm wrapper tests passed\n");
    return 0;
}
