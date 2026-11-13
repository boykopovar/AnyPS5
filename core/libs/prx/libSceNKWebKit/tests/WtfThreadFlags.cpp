#include "prx/libSceNKWebKit/include/WtfThreadFlagsExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <type_traits>

extern "C" std::uint64_t APS5_VABI NotImplementedCalls_nid_no_patch();

namespace {

using NKWebKit::WtfAbi::GCThreadType;

static_assert(sizeof(GCThreadType) == 1);
static_assert(std::is_same_v<std::underlying_type_t<GCThreadType>, std::uint8_t>);
static_assert(static_cast<std::uint8_t>(GCThreadType::None) == 0);
static_assert(static_cast<std::uint8_t>(GCThreadType::Main) == 1);
static_assert(static_cast<std::uint8_t>(GCThreadType::Helper) == 2);

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF thread flags check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

bool IsCompilationThread() {
    return _ZN3WTF19isCompilationThreadEv();
}

bool ExchangeIsCompilationThread(bool value) {
    return _ZN3WTF6Thread27exchangeIsCompilationThreadEb(value);
}

void RegisterGCThread(GCThreadType type) {
    _ZN3WTF6Thread16registerGCThreadENS_12GCThreadTypeE(type);
}

bool MayBeGCThread() {
    return _ZN3WTF6Thread13mayBeGCThreadEv();
}

}  // namespace

int main() {
    const auto before = NotImplementedCalls_nid_no_patch();

    Require(!IsCompilationThread());
    Require(!ExchangeIsCompilationThread(true));
    Require(IsCompilationThread());
    Require(ExchangeIsCompilationThread(true));
    Require(IsCompilationThread());
    Require(ExchangeIsCompilationThread(false));
    Require(!IsCompilationThread());
    Require(!ExchangeIsCompilationThread(false));
    Require(!IsCompilationThread());

    Require(!MayBeGCThread());
    RegisterGCThread(GCThreadType::Main);
    Require(MayBeGCThread());
    RegisterGCThread(GCThreadType::Helper);
    Require(MayBeGCThread());
    RegisterGCThread(GCThreadType::None);
    Require(!MayBeGCThread());
    RegisterGCThread(static_cast<GCThreadType>(3));
    Require(MayBeGCThread());
    RegisterGCThread(static_cast<GCThreadType>(4));
    Require(!MayBeGCThread());
    RegisterGCThread(static_cast<GCThreadType>(6));
    Require(MayBeGCThread());
    RegisterGCThread(GCThreadType::None);

    Require(!ExchangeIsCompilationThread(true));
    RegisterGCThread(GCThreadType::Main);

    bool otherInitialCompilation = true;
    bool otherInitialGC = true;
    bool otherAfterCompilation = false;
    bool otherAfterGC = false;
    std::thread other([&] {
        otherInitialCompilation = IsCompilationThread();
        otherInitialGC = MayBeGCThread();
        ExchangeIsCompilationThread(true);
        RegisterGCThread(GCThreadType::Helper);
        otherAfterCompilation = IsCompilationThread();
        otherAfterGC = MayBeGCThread();
        ExchangeIsCompilationThread(false);
        RegisterGCThread(GCThreadType::None);
    });
    other.join();

    Require(!otherInitialCompilation);
    Require(!otherInitialGC);
    Require(otherAfterCompilation);
    Require(otherAfterGC);
    Require(IsCompilationThread());
    Require(MayBeGCThread());

    Require(ExchangeIsCompilationThread(false));
    RegisterGCThread(GCThreadType::None);
    Require(!IsCompilationThread());
    Require(!MayBeGCThread());

    Require(NotImplementedCalls_nid_no_patch() == before);
    return 0;
}
