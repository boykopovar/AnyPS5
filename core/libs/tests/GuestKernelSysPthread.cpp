#include "SceTypes.hpp"
#include "prx/libkernel/Pthread/include/Atfork.hpp"
#include <cstdlib>

extern "C" {
int APS5_VABI scePthreadAttrInit(PthreadAttr* attr);
int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr);
int APS5_VABI scePthreadAttrGetscope(const PthreadAttr* attr, int* scope);
int APS5_VABI scePthreadAttrSetscope(PthreadAttr* attr, int scope);
int APS5_VABI scePthreadAtfork(PthreadAtforkHandler prepare, PthreadAtforkHandler parent, PthreadAtforkHandler child);
int APS5_VABI scePthreadSetconcurrency(int level);
int APS5_VABI scePthreadGetconcurrency();
}

static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);
static constexpr int SCOPE_PROCESS = 0;
static constexpr int SCOPE_SYSTEM = 2;

static void Require(bool value) { if (!value) std::abort(); }

static void APS5_VABI Prepare() {}
static void APS5_VABI Parent() {}
static void APS5_VABI Child() {}

static void TestScope() {
    PthreadAttr attr{};
    Require(scePthreadAttrInit(&attr) == 0);
    int scope = -1;
    Require(scePthreadAttrGetscope(&attr, &scope) == 0 && scope == SCOPE_SYSTEM);
    Require(scePthreadAttrSetscope(&attr, SCOPE_PROCESS) == 0);
    Require(scePthreadAttrGetscope(&attr, &scope) == 0 && scope == SCOPE_PROCESS);
    for (const int invalid : {1, 3, -1}) {
        Require(scePthreadAttrSetscope(&attr, invalid) == SCE_KERNEL_ERROR_EINVAL);
        scope = -1;
        Require(scePthreadAttrGetscope(&attr, &scope) == 0 && scope == SCOPE_PROCESS);
    }
    Require(scePthreadAttrSetscope(&attr, SCOPE_SYSTEM) == 0);
    Require(scePthreadAttrGetscope(&attr, &scope) == 0 && scope == SCOPE_SYSTEM);
    Require(scePthreadAttrGetscope(&attr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetscope(nullptr, &scope) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrSetscope(nullptr, SCOPE_PROCESS) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrDestroy(&attr) == 0);
}

static void TestConcurrency() {
    Require(scePthreadSetconcurrency(3) == 0 && scePthreadGetconcurrency() == 3);
    Require(scePthreadSetconcurrency(-1) == SCE_KERNEL_ERROR_EINVAL && scePthreadGetconcurrency() == 3);
    Require(scePthreadSetconcurrency(0) == 0 && scePthreadGetconcurrency() == 0);
}

static void TestAtfork() {
    const auto before = PthreadAtforkHandlers_nid_no_patch().size();
    Require(scePthreadAtfork(Prepare, Parent, Child) == 0);
    Require(scePthreadAtfork(nullptr, nullptr, nullptr) == 0);
    const auto handlers = PthreadAtforkHandlers_nid_no_patch();
    Require(handlers.size() == before + 2);
    Require(handlers[before].prepare == Prepare && handlers[before].parent == Parent && handlers[before].child == Child);
    Require(handlers[before + 1].prepare == nullptr && handlers[before + 1].parent == nullptr && handlers[before + 1].child == nullptr);
}

int main() {
    TestScope();
    TestConcurrency();
    TestAtfork();
    return 0;
}
