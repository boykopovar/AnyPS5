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
constexpr int InvalidType = static_cast<int>(0x807F0001u);
constexpr int NotRegistered = static_cast<int>(0x8093000Au);
constexpr std::uint32_t Untouched = 0x5a5a5a5au;

using GetAjmContextFn = int (APS5_VABI*)(const void*, std::uint32_t*, std::uint32_t*);
using UnregisterCodecFn = int (APS5_VABI*)(const void*, std::uint32_t);

template <typename Function>
Function Resolve(const char* name) {
#ifdef _WIN32
    void* symbol = reinterpret_cast<void*>(GetProcAddress(LoadLibraryA("libSceAudiodec.native.prx"), name));
#else
    void* symbol = dlsym(RTLD_DEFAULT, name);
#endif
    if (!symbol) {
        std::fprintf(stderr, "%s is not exported by libSceAudiodec.native\n", name);
        std::abort();
    }
    return reinterpret_cast<Function>(symbol);
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "Audiodec GetAjmContext: %s\n", message);
    std::abort();
}

}

int main() {
    const auto getAjmContext = Resolve<GetAjmContextFn>("sceAudiodecGetAjmContext");
    const auto unregisterCodec = Resolve<UnregisterCodecFn>("sceAudiodecUnregisterCodec");
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

    Require(unregisterCodec(nullptr, 0) == InvalidArgument, "a null context was not rejected");
    Require(unregisterCodec(nullptr, 3) == InvalidArgument, "a null context with a codec type was not rejected");
    Require(unregisterCodec(context, 0) == InvalidType, "codec type 0 was not rejected");
    Require(unregisterCodec(context, 5) == InvalidType, "codec type 5 was not rejected");
    Require(unregisterCodec(context, 13) == InvalidType, "codec type 13 was not rejected");
    Require(std::memcmp(before, context, sizeof(context)) == 0, "the rejected-type calls wrote the context");

    alignas(8) unsigned char idle[sizeof(context)];
    std::memset(idle, 0, sizeof(idle));
    unsigned char idleBefore[sizeof(idle)];
    std::memcpy(idleBefore, idle, sizeof(idle));
    Require(unregisterCodec(idle, 11) == NotRegistered, "codec type 11 with a clear flag was not rejected");
    Require(unregisterCodec(idle, 12) == NotRegistered, "codec type 12 with a clear flag was not rejected");
    Require(std::memcmp(idleBefore, idle, sizeof(idle)) == 0, "the clear-flag calls wrote the context");

    std::puts("Audiodec GetAjmContext tests passed");
    return 0;
}
