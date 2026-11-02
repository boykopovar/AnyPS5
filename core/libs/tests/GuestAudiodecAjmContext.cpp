#include "prx/libSceAudiodec.native/include/Api.hpp"
#include "SceTypes.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

constexpr int InvalidArgument = static_cast<int>(0x807F0002u);
constexpr std::uint32_t Untouched = 0x5a5a5a5au;

using GetAjmContextFn = int (APS5_VABI*)(const void*, std::uint32_t*, std::uint32_t*);

GetAjmContextFn Resolve() {
#ifdef _WIN32
    void* symbol = reinterpret_cast<void*>(GetProcAddress(LoadLibraryA("libSceAudiodec.native.prx"), "sceAudiodecGetAjmContext"));
#else
    void* symbol = dlsym(RTLD_DEFAULT, "sceAudiodecGetAjmContext");
#endif
    if (!symbol) {
        std::fprintf(stderr, "sceAudiodecGetAjmContext is not exported by libSceAudiodec.native\n");
        std::abort();
    }
    return reinterpret_cast<GetAjmContextFn>(symbol);
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "Audiodec GetAjmContext: %s\n", message);
    std::abort();
}

}

int main() {
    const auto getAjmContext = Resolve();
    alignas(8) unsigned char context[0x40];
    std::memset(context, 0xee, sizeof(context));
    const std::uint32_t ids[2] = {0x12345678u, 0x9abcdef0u};
    std::memcpy(context, ids, sizeof(ids));

    std::uint32_t ajmi = Untouched;
    std::uint32_t ajm = Untouched;
    Require(getAjmContext(nullptr, &ajmi, &ajm) == InvalidArgument && ajmi == Untouched && ajm == Untouched, "a null context was taken");
    Require(getAjmContext(context, &ajmi, &ajm) == 0 && ajmi == ids[0] && ajm == ids[1], "the two ids were not read from offsets 0 and 4");

    ajmi = Untouched;
    ajm = Untouched;
    Require(getAjmContext(context, nullptr, &ajm) == 0 && ajm == ids[1], "a null Ajmi output was not skipped");
    Require(getAjmContext(context, &ajmi, nullptr) == 0 && ajmi == ids[0], "a null Ajm output was not skipped");
    Require(getAjmContext(context, nullptr, nullptr) == 0, "two null outputs were not skipped");

    unsigned char before[sizeof(context)];
    std::memcpy(before, context, sizeof(context));
    Require(getAjmContext(context, &ajmi, &ajm) == 0 && std::memcmp(before, context, sizeof(context)) == 0, "the context was written");

    std::puts("Audiodec GetAjmContext tests passed");
    return 0;
}
