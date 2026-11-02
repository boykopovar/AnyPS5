#include "prx/libSceVideoCoreServerInterface/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

constexpr int InvalidArgument = static_cast<int>(0x80DC0001);
constexpr int BeWrapperResourceNotFound = static_cast<int>(0x80DC000E);
constexpr int ResourceNotFound = static_cast<int>(0x80DC0015);

int callbackCalls = 0;

void APS5_VABI Callback(std::uint32_t, std::uint64_t) {
    ++callbackCalls;
}

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "VideoCoreServerInterface: %s\n", message);
        std::abort();
    }
}

void* Resolve(const char* name) {
#ifdef _WIN32
    const HMODULE module = GetModuleHandleA("libSceVideoCoreServerInterface.prx");
    Require(module != nullptr, "library module");
    return reinterpret_cast<void*>(GetProcAddress(module, name));
#else
    return dlsym(RTLD_DEFAULT, name);
#endif
}

void CheckExports() {
    Require(Resolve("sceVideoCoreAcquireDecoderResource") != nullptr, "sceVideoCoreAcquireDecoderResource export");
    Require(Resolve("sceVideoCoreAcquireDecoderResourceForBeWrapper") != nullptr, "sceVideoCoreAcquireDecoderResourceForBeWrapper export");
    Require(Resolve("sceVideoCoreReleaseDecoderResource") != nullptr, "sceVideoCoreReleaseDecoderResource export");
    Require(Resolve("sceVideoCoreReleaseDecoderResourceForBeWrapper") != nullptr, "sceVideoCoreReleaseDecoderResourceForBeWrapper export");
}

void CheckAcquireArguments() {
    const VideoCoreAcquireDecoderResourceParam param{0, 1920, 1080, 0};
    std::uint32_t id = 77;
    Require(sceVideoCoreAcquireDecoderResource(nullptr, Callback, &id) == InvalidArgument, "acquire without a param");
    Require(sceVideoCoreAcquireDecoderResource(&param, nullptr, &id) == InvalidArgument, "acquire without a callback");
    Require(sceVideoCoreAcquireDecoderResource(&param, Callback, nullptr) == InvalidArgument, "acquire without an id");
    Require(sceVideoCoreAcquireDecoderResourceForBeWrapper(nullptr, Callback, &id) == InvalidArgument, "BeWrapper acquire without a param");
    Require(sceVideoCoreAcquireDecoderResourceForBeWrapper(&param, nullptr, &id) == InvalidArgument, "BeWrapper acquire without a callback");
    Require(sceVideoCoreAcquireDecoderResourceForBeWrapper(&param, Callback, nullptr) == InvalidArgument, "BeWrapper acquire without an id");
    Require(id == 77, "a refused acquire leaves the id alone");
}

void CheckLifetimes() {
    const VideoCoreAcquireDecoderResourceParam standardParam{0, 1920, 1080, 0};
    const VideoCoreAcquireDecoderResourceParam wrapperParam{1, 3840, 2160, 2};
    std::uint32_t first = 0, second = 0, wrapper = 0;
    Require(sceVideoCoreAcquireDecoderResource(&standardParam, Callback, &first) == 0, "first acquire");
    Require(first == 1, "ids start at 1");
    Require(sceVideoCoreAcquireDecoderResource(&standardParam, Callback, &second) == 0, "second acquire");
    Require(second == 2, "ids count up");
    Require(sceVideoCoreAcquireDecoderResourceForBeWrapper(&wrapperParam, Callback, &wrapper) == 0, "BeWrapper acquire");
    Require(wrapper == 3, "BeWrapper ids share the counter");

    Require(sceVideoCoreReleaseDecoderResource(0) == ResourceNotFound, "release of id 0");
    Require(sceVideoCoreReleaseDecoderResourceForBeWrapper(0) == BeWrapperResourceNotFound, "BeWrapper release of id 0");
    Require(sceVideoCoreReleaseDecoderResource(999) == ResourceNotFound, "release of an unknown id");
    Require(sceVideoCoreReleaseDecoderResourceForBeWrapper(999) == BeWrapperResourceNotFound, "BeWrapper release of an unknown id");
    Require(sceVideoCoreReleaseDecoderResource(wrapper) == ResourceNotFound, "release of a BeWrapper resource");
    Require(sceVideoCoreReleaseDecoderResourceForBeWrapper(first) == BeWrapperResourceNotFound, "BeWrapper release of a standard resource");

    Require(sceVideoCoreReleaseDecoderResource(first) == 0, "release");
    Require(sceVideoCoreReleaseDecoderResource(first) == ResourceNotFound, "second release");
    Require(sceVideoCoreReleaseDecoderResourceForBeWrapper(wrapper) == 0, "BeWrapper release");
    Require(sceVideoCoreReleaseDecoderResourceForBeWrapper(wrapper) == BeWrapperResourceNotFound, "second BeWrapper release");
    Require(sceVideoCoreReleaseDecoderResource(second) == 0, "release of the other standard resource");

    std::uint32_t next = 0;
    Require(sceVideoCoreAcquireDecoderResource(&standardParam, Callback, &next) == 0, "acquire after releases");
    Require(next == 4, "released ids are not reused");
    Require(sceVideoCoreReleaseDecoderResource(next) == 0, "release after reacquire");
    Require(callbackCalls == 0, "neither acquire nor release calls the callback");
}

}

int main() {
    CheckExports();
    CheckAcquireArguments();
    CheckLifetimes();
    return 0;
}
