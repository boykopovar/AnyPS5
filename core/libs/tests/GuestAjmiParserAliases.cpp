#include "prx/libSceAjmi/libSceAjmi.hpp"
#include "prx/libSceAjm.native/libSceAjm.native.hpp"

#include <array>
#include <bit>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <source_location>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "AJMI parser alias check failed at line %u\n", location.line());
        std::abort();
    }
}

class Module {
public:
    explicit Module(const std::filesystem::path& path) {
#ifdef _WIN32
        handle = LoadLibraryW(path.c_str());
        if (!handle) std::fprintf(stderr, "LoadLibraryW failed: %lu\n", GetLastError());
#else
        handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle) std::fprintf(stderr, "%s\n", dlerror());
#endif
        Require(handle != nullptr);
    }

    ~Module() {
#ifdef _WIN32
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
    }

    template <typename TFunction>
    TFunction Import(const char* name) const {
#ifdef _WIN32
        auto symbol = GetProcAddress(handle, name);
#else
        void* symbol = dlsym(handle, name);
#endif
        if (!symbol) std::fprintf(stderr, "Missing symbol: %s\n", name);
        Require(symbol != nullptr);
        return std::bit_cast<TFunction>(symbol);
    }

private:
#ifdef _WIN32
    HMODULE handle = nullptr;
#else
    void* handle = nullptr;
#endif
};

constexpr int InvalidParameter = static_cast<int>(0x80930005u);

template <typename TValue>
struct Guarded {
    std::uint64_t before = 0x1122334455667788ull;
    TValue value{};
    std::uint64_t after = 0x8877665544332211ull;

    void Check() const {
        Require(before == 0x1122334455667788ull && after == 0x8877665544332211ull);
    }
};

void CheckAt9(decltype(&sceAjmiDecAt9ParseConfigData) call, decltype(&sceAjmDecAt9ParseConfigData) reference) {
    std::array<std::uint8_t, 4> config{0xFE, 0x72, 0x1F, 0xF0};
    Guarded<AjmDecAt9ConfigDataInfo> output;
    Require(call(config.data(), &output.value) == 0);
    Require(output.value.channels == 2 && output.value.sample_rate == 48000);
    Require(output.value.frame_samples_per_channel == 256 && output.value.superframe_samples_per_channel == 1024);
    Require(output.value.superframe_size == 1024);
    output.Check();
    for (unsigned fields = 0; fields < 256; ++fields) {
        for (unsigned geometry = 0; geometry < 256; geometry += 17) {
            config = {0xFE, static_cast<std::uint8_t>(fields), static_cast<std::uint8_t>(geometry), 0xF0};
            const auto original = config;
            AjmDecAt9ConfigDataInfo expected;
            std::memset(&expected, 0xA5, sizeof(expected));
            output.value = expected;
            const int status = reference(config.data(), &expected);
            errno = 71;
            Require(call(config.data(), &output.value) == status);
            Require(errno == 71 && std::memcmp(&output.value, &expected, sizeof(expected)) == 0);
            Require(config == original);
            output.Check();
        }
    }
    config = {0xFD, 0x72, 0x1F, 0xF0};
    std::memset(&output.value, 0xA5, sizeof(output.value));
    const auto unchanged = output.value;
    Require(call(config.data(), &output.value) == InvalidParameter);
    Require(call(nullptr, &output.value) == InvalidParameter);
    Require(call(config.data(), nullptr) == InvalidParameter);
    errno = 71;
    Require(call(reinterpret_cast<const void*>(0x10), &output.value) == InvalidParameter && errno == 71);
    config[0] = 0xFE;
    Require(call(config.data(), reinterpret_cast<AjmDecAt9ConfigDataInfo*>(0x10)) == InvalidParameter && errno == 71);
    Require(std::memcmp(&output.value, &unchanged, sizeof(unchanged)) == 0);
    output.Check();
}

void CheckMp3(decltype(&sceAjmiDecMp3ParseFrame) call, decltype(&sceAjmDecMp3ParseFrame) reference) {
    std::array<std::uint8_t, 512> stream{};
    stream[0] = 0xFF;
    stream[1] = 0xFB;
    stream[2] = 0x90;
    Guarded<AjmDecMp3ParseFrame> output;
    Require(call(stream.data(), 4, 0, &output.value) == 0);
    Require(output.value.frame_size == 417 && output.value.num_channels == 2);
    Require(output.value.bitrate == 128000 && output.value.sample_rate == 44100);
    Require(output.value.samples_per_channel == 1152 && output.value.ofl_type == 0);
    for (const std::uint8_t format : {0xE0u, 0xE2u, 0xE3u, 0xF2u, 0xF3u, 0xFAu, 0xFBu, 0xFCu, 0xFEu}) {
        for (unsigned rate = 0; rate < 256; ++rate) {
            for (const int parseOfl : {0, 1, 2, -1}) {
                stream[1] = format;
                stream[2] = static_cast<std::uint8_t>(rate);
                stream[3] = rate & 1 ? 0xC0 : 0;
                const auto original = stream;
                AjmDecMp3ParseFrame expected;
                std::memset(&expected, 0xA5, sizeof(expected));
                output.value = expected;
                const int status = reference(stream.data(), stream.size(), parseOfl, &expected);
                Require(call(stream.data(), stream.size(), parseOfl, &output.value) == status);
                Require(std::memcmp(&output.value, &expected, sizeof(expected)) == 0 && stream == original);
                output.Check();
            }
        }
    }
    stream.fill(0);
    stream[0] = 0xFF;
    stream[1] = 0xFB;
    stream[2] = 0x90;
    std::memcpy(stream.data() + 36, "Xing", 4);
    stream[43] = 10;
    stream[47] = 4;
    stream[51] = 8;
    std::memcpy(stream.data() + 52, "LAME3.100", 8);
    for (const std::uint32_t length : {4u, 35u, 36u, 43u, 51u, 52u, 60u, 72u, 73u, 80u, 417u, 512u}) {
        AjmDecMp3ParseFrame expected{};
        const int status = reference(stream.data(), length, 1, &expected);
        Require(call(stream.data(), length, 1, &output.value) == status);
        Require(std::memcmp(&output.value, &expected, sizeof(expected)) == 0);
        output.Check();
    }
    const auto unchanged = output.value;
    Require(call(nullptr, 4, 0, &output.value) == InvalidParameter);
    Require(call(stream.data(), 4, 0, nullptr) == InvalidParameter);
    for (const std::uint32_t length : {0u, 1u, 2u, 3u}) Require(call(stream.data(), length, 0, &output.value) == InvalidParameter);
    Require(std::memcmp(&output.value, &unchanged, sizeof(unchanged)) == 0);
    output.Check();
}

void CheckErrors(decltype(&sceAjmiStrError) call, decltype(&sceAjmStrError) reference) {
    Require(std::strcmp(call(0), "ok") == 0);
    Require(std::strcmp(call(InvalidParameter), "invalid parameter") == 0);
    Require(std::strcmp(call(-1), "(unknown)") == 0);
    for (std::uint32_t code = 0x80930000u; code < 0x80930100u; ++code) {
        Require(std::strcmp(call(static_cast<int>(code)), reference(static_cast<int>(code))) == 0);
    }
    for (const int code : {std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), -1, 0, 1}) {
        const char* text = call(code);
        Require(text && std::strcmp(text, reference(code)) == 0 && call(code) == text);
    }
}

} // namespace

int main(int argc, char** argv) {
    Require(argc == 5);
    const std::filesystem::path aliasPath = std::u8string(reinterpret_cast<const char8_t*>(argv[1]));
    const std::filesystem::path nativePath = std::u8string(reinterpret_cast<const char8_t*>(argv[2]));
    const bool patched = std::string_view(argv[4]) == "nid";
    Require(patched || std::string_view(argv[4]) == "unpatched");
    Module native(nativePath);
    Module alias(aliasPath);
    const std::string_view operation = argv[3];
    if (operation == "at9") {
        CheckAt9(alias.Import<decltype(&sceAjmiDecAt9ParseConfigData)>(patched ? "MTdd8ApwO50" : "sceAjmiDecAt9ParseConfigData"),
            native.Import<decltype(&sceAjmDecAt9ParseConfigData)>(patched ? "1t3ixYNXyuc" : "sceAjmDecAt9ParseConfigData"));
    } else if (operation == "mp3") {
        CheckMp3(alias.Import<decltype(&sceAjmiDecMp3ParseFrame)>(patched ? "cWZnCK7na5Y" : "sceAjmiDecMp3ParseFrame"),
            native.Import<decltype(&sceAjmDecMp3ParseFrame)>(patched ? "eDFeTyi+G3Y" : "sceAjmDecMp3ParseFrame"));
    } else {
        Require(operation == "errors");
        CheckErrors(alias.Import<decltype(&sceAjmiStrError)>(patched ? "EYIibEpcHuw" : "sceAjmiStrError"),
            native.Import<decltype(&sceAjmStrError)>(patched ? "AxhcqVv5AYU" : "sceAjmStrError"));
    }
}
