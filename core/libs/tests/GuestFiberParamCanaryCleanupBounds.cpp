#include "prx/libSceFiber/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

constexpr std::int32_t SCE_OK = 0;
constexpr std::int32_t FiberErrorNull = static_cast<std::int32_t>(0x80590001);
constexpr std::int32_t FiberErrorInvalid = static_cast<std::int32_t>(0x80590004);
constexpr std::int32_t FiberErrorState = static_cast<std::int32_t>(0x80590006);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

struct UserContext {
    std::uint32_t magic;
    std::uint32_t iteration;
    std::uint64_t payload;
};

alignas(16) std::uint8_t g_stack[16384];
alignas(8) std::uint8_t g_fiberStorage[256];
FiberObject* const g_fiber = reinterpret_cast<FiberObject*>(g_fiberStorage);

void APS5_VABI ParamPassingEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 0xdeadbeefull);
    auto* context = reinterpret_cast<UserContext*>(argOnRun);
    Require(context != nullptr);
    Require(context->magic == 0xaabbccdd);
    Require(context->iteration == 1);
    Require(context->payload == 100);

    context->iteration = 2;
    context->payload = 200;

    std::uint64_t nextArg = 0;
    Require(sceFiberReturnToThread(reinterpret_cast<std::uint64_t>(context), &nextArg) == SCE_OK);

    context = reinterpret_cast<UserContext*>(nextArg);
    Require(context != nullptr);
    Require(context->iteration == 3);
    Require(context->payload == 300);

    context->iteration = 4;
    context->payload = 400;

    Require(sceFiberReturnToThread(reinterpret_cast<std::uint64_t>(context), nullptr) == SCE_OK);
}

void APS5_VABI CanaryCheckEntry(std::uint64_t, std::uint64_t) {
    volatile std::uint8_t localBuffer[512];
    for (std::size_t i = 0; i < sizeof(localBuffer); ++i) {
        localBuffer[i] = static_cast<std::uint8_t>(i & 0xff);
    }
    Require(sceFiberReturnToThread(999, nullptr) == SCE_OK);
}

void TestUserParameterPassingAcrossYieldAndResume() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiber, "param_pass", ParamPassingEntry, 0xdeadbeefull, g_stack, sizeof(g_stack), nullptr, 0) == SCE_OK);

    UserContext userCtx{};
    userCtx.magic = 0xaabbccdd;
    userCtx.iteration = 1;
    userCtx.payload = 100;

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiber, reinterpret_cast<std::uint64_t>(&userCtx), &returned) == SCE_OK);
    auto* resCtx = reinterpret_cast<UserContext*>(returned);
    Require(resCtx == &userCtx);
    Require(userCtx.iteration == 2);
    Require(userCtx.payload == 200);

    userCtx.iteration = 3;
    userCtx.payload = 300;
    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiber, reinterpret_cast<std::uint64_t>(&userCtx), &returned) == SCE_OK);
    resCtx = reinterpret_cast<UserContext*>(returned);
    Require(resCtx == &userCtx);
    Require(userCtx.iteration == 4);
    Require(userCtx.payload == 400);

    Require(sceFiberFinalize(g_fiber) == SCE_OK);
}

void TestStackCanaryAndMarginVerification() {
    Require(sceFiberStartContextSizeCheck(0) == SCE_OK);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiber, "canary_test", CanaryCheckEntry, 0, g_stack, sizeof(g_stack), nullptr, 0) == SCE_OK);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiber, &info) == SCE_OK);
    Require(info.size_context_margin == sizeof(g_stack));

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiber, 0, &returned) == SCE_OK);
    Require(returned == 999);

    Require(sceFiberGetInfo(g_fiber, &info) == SCE_OK);
    Require(info.size_context_margin > 0);
    Require(info.size_context_margin < sizeof(g_stack));
    Require(info.size_context_margin % 8 == 0);

    Require(sceFiberFinalize(g_fiber) == SCE_OK);
    Require(sceFiberStopContextSizeCheck() == SCE_OK);

    constexpr std::uint32_t kFlagContextSizeCheck = 0x10;
    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiber, "internal_canary", CanaryCheckEntry, 0, g_stack, sizeof(g_stack), nullptr, kFlagContextSizeCheck, 0) == SCE_OK);

    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiber, &info) == SCE_OK);
    Require(info.size_context_margin == sizeof(g_stack));

    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiber, 0, &returned) == SCE_OK);
    Require(returned == 999);

    Require(sceFiberGetInfo(g_fiber, &info) == SCE_OK);
    Require(info.size_context_margin > 0);
    Require(info.size_context_margin < sizeof(g_stack));
    Require(info.size_context_margin % 8 == 0);

    Require(sceFiberFinalize(g_fiber) == SCE_OK);
}

void TestTerminationCleanupAndBufferGuards() {
    struct alignas(16) GuardedBuffer {
        std::uint64_t guardBefore[4];
        alignas(16) std::uint8_t stack[16384];
        std::uint64_t guardAfter[4];
    } guarded;

    for (std::size_t i = 0; i < 4; ++i) {
        guarded.guardBefore[i] = 0x0123456789abcdefull + i;
        guarded.guardAfter[i] = 0xfedcba9876543210ull - i;
    }

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiber, "guarded", CanaryCheckEntry, 0, guarded.stack, sizeof(guarded.stack), nullptr, 0) == SCE_OK);

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiber, 0, &returned) == SCE_OK);
    Require(returned == 999);

    Require(sceFiberFinalize(g_fiber) == SCE_OK);
    Require(sceFiberFinalize(g_fiber) == FiberErrorInvalid);
    Require(sceFiberRun_nid_postfix(g_fiber, 0, nullptr) == FiberErrorInvalid);

    for (std::size_t i = 0; i < 4; ++i) {
        Require(guarded.guardBefore[i] == 0x0123456789abcdefull + i);
        Require(guarded.guardAfter[i] == 0xfedcba9876543210ull - i);
    }

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiber, "reinitialized", CanaryCheckEntry, 0, guarded.stack, sizeof(guarded.stack), nullptr, 0) == SCE_OK);
    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiber, 0, &returned) == SCE_OK);
    Require(returned == 999);
    Require(sceFiberFinalize(g_fiber) == SCE_OK);
}

} // namespace

int main() {
    TestUserParameterPassingAcrossYieldAndResume();
    TestStackCanaryAndMarginVerification();
    TestTerminationCleanupAndBufferGuards();
    return 0;
}
