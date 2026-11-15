#include "prx/libSceAudiodec.native/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "SceTypes.hpp"

#include <cerrno>
#include <cstddef>
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

void ClobberErrno(std::uintptr_t, std::size_t) { errno = 99; }

constexpr int InvalidArgument = static_cast<int>(0x807F0002u);
constexpr std::uint32_t Untouched = 0x5a5a5a5au;

using GetAjmContextFn = int (APS5_VABI*)(const void*, std::uint32_t*, std::uint32_t*);

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
    std::fprintf(stderr, "Audiodec GetAjmContext pointers: %s\n", message);
    std::abort();
}

}

int main() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);
    const auto getAjmContext = Resolve<GetAjmContextFn>("sceAudiodecGetAjmContext");
    alignas(8) unsigned char context[0x40];
    std::memset(context, 0xee, sizeof(context));
    const std::uint32_t ids[2] = {0x12345678u, 0x9abcdef0u};
    std::memcpy(context, ids, sizeof(ids));

    unsigned char before[sizeof(context)];
    std::memcpy(before, context, sizeof(before));

    std::uint32_t ajmi = Untouched;
    std::uint32_t ajm = Untouched;
    errno = 71;
    Require(getAjmContext(context, &ajmi, &ajm) == 0 && ajmi == ids[0] && ajm == ids[1], "the two ids were not read from offsets 0 and 4");
    Require(errno == 71, "errno was modified on valid context read/write");
    Require(std::memcmp(before, context, sizeof(context)) == 0, "a valid call wrote the context");

    ajmi = Untouched;
    ajm = Untouched;
    errno = 71;
    Require(getAjmContext(context, nullptr, nullptr) == 0 && ajmi == Untouched && ajm == Untouched, "two null outputs were not skipped");
    Require(errno == 71, "errno was modified with null outputs");

    ajmi = Untouched;
    ajm = Untouched;
    errno = 71;
    Require(getAjmContext(reinterpret_cast<const void*>(0x10), &ajmi, &ajm) == InvalidArgument && ajmi == Untouched && ajm == Untouched, "a dangling context was not rejected");
    Require(errno == 71, "errno was modified on dangling context");

    ajmi = Untouched;
    ajm = Untouched;
    errno = 71;
    Require(getAjmContext(context, reinterpret_cast<std::uint32_t*>(0x10), &ajm) == InvalidArgument && ajmi == Untouched && ajm == Untouched, "a dangling Ajmi output was not rejected");
    Require(errno == 71, "errno was modified on dangling Ajmi output");

    ajmi = Untouched;
    ajm = Untouched;
    errno = 71;
    Require(getAjmContext(context, &ajmi, reinterpret_cast<std::uint32_t*>(0x10)) == InvalidArgument && ajmi == ids[0] && ajm == Untouched, "a dangling Ajm output was not rejected");
    Require(errno == 71, "errno was modified on dangling Ajm output");

    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    std::puts("Audiodec GetAjmContext pointer tests passed");
    return 0;
}
