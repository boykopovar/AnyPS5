#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"

#include <cstdint>
#include <cstdlib>
#include <string>

namespace {

constexpr int SceOk = 0;

constexpr std::uint32_t EvfAttrThFifo = 0x01;
constexpr std::uint32_t EvfAttrThPrio = 0x02;
constexpr std::uint32_t EvfAttrSingle = 0x10;
constexpr std::uint32_t EvfAttrMulti = 0x20;
constexpr std::uint32_t EvfAttrShared = 0x100;

constexpr std::uint32_t EvfWaitModeAnd = 0x01;
constexpr std::uint32_t EvfWaitModeOr = 0x02;
constexpr std::uint32_t EvfWaitModeClearAll = 0x10;
constexpr std::uint32_t EvfWaitModeClearPat = 0x20;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void AuditEventFlagCreateParameterBounds() {
    Require(sceKernelCreateEventFlag(nullptr, "test_null_handle", EvfAttrMulti, 0, nullptr) == SCE_KERNEL_ERROR_EINVAL);

    KernelEventFlag ef = nullptr;
    Require(sceKernelCreateEventFlag(&ef, nullptr, EvfAttrMulti, 0, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(ef == nullptr);

    Require(sceKernelCreateEventFlag(&ef, "test_single_multi", EvfAttrSingle | EvfAttrMulti, 0, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelCreateEventFlag(&ef, "test_fifo_prio", EvfAttrThFifo | EvfAttrThPrio, 0, nullptr) == SCE_KERNEL_ERROR_EINVAL);

    const std::uint32_t invalidAttrs[] = {
        0x04u,
        0x08u,
        0x40u,
        0x80u,
        0x200u,
        0x400u,
        0x800u,
        0x1000u,
        0x80000000u,
        EvfAttrMulti | 0x04u,
        EvfAttrSingle | 0x40u,
    };
    for (const auto attr : invalidAttrs) {
        Require(sceKernelCreateEventFlag(&ef, "test_invalid_attr", attr, 0, nullptr) == SCE_KERNEL_ERROR_EINVAL);
        Require(ef == nullptr);
    }

    const std::string name32(32, 'a');
    Require(sceKernelCreateEventFlag(&ef, name32.c_str(), EvfAttrMulti, 0, nullptr) == SCE_KERNEL_ERROR_ENAMETOOLONG);

    const std::string name33(33, 'b');
    Require(sceKernelCreateEventFlag(&ef, name33.c_str(), EvfAttrMulti, 0, nullptr) == SCE_KERNEL_ERROR_ENAMETOOLONG);

    const std::string name31(31, 'c');
    Require(sceKernelCreateEventFlag(&ef, name31.c_str(), EvfAttrMulti, 0, nullptr) == SceOk);
    Require(ef != nullptr);
    Require(sceKernelDeleteEventFlag(ef) == SceOk);
    ef = nullptr;

    Require(sceKernelCreateEventFlag(&ef, "", EvfAttrMulti, 0, nullptr) == SceOk);
    Require(ef != nullptr);
    Require(sceKernelDeleteEventFlag(ef) == SceOk);
    ef = nullptr;

    const std::uint32_t validAttrs[] = {
        0u,
        EvfAttrThFifo,
        EvfAttrThPrio,
        EvfAttrSingle,
        EvfAttrMulti,
        EvfAttrThFifo | EvfAttrSingle,
        EvfAttrThFifo | EvfAttrMulti,
        EvfAttrThPrio | EvfAttrSingle,
        EvfAttrThPrio | EvfAttrMulti,
        EvfAttrShared | EvfAttrSingle,
        EvfAttrShared | EvfAttrMulti,
    };
    for (const auto attr : validAttrs) {
        Require(sceKernelCreateEventFlag(&ef, "valid_attr_test", attr, 0, nullptr) == SceOk);
        Require(ef != nullptr);
        Require(sceKernelDeleteEventFlag(ef) == SceOk);
        ef = nullptr;
    }

    const std::uint64_t initialPatterns[] = {
        0ULL,
        1ULL,
        0x5555555555555555ULL,
        0xAAAAAAAAAAAAAAAAULL,
        0x8000000000000000ULL,
        0xFFFFFFFFFFFFFFFFULL,
    };
    for (const auto pattern : initialPatterns) {
        Require(sceKernelCreateEventFlag(&ef, "init_pat_test", EvfAttrMulti, pattern, nullptr) == SceOk);
        Require(ef != nullptr);
        std::uint64_t current = 0;
        if (pattern != 0) {
            Require(sceKernelPollEventFlag(ef, pattern, EvfWaitModeAnd, &current) == SceOk);
            Require(current == pattern);
        } else {
            Require(sceKernelPollEventFlag(ef, 1ULL, EvfWaitModeAnd, &current) == SCE_KERNEL_ERROR_EBUSY);
            Require(current == 0);
        }
        Require(sceKernelDeleteEventFlag(ef) == SceOk);
        ef = nullptr;
    }
}

void AuditEventFlagSharedCreationAndCollisionBounds() {
    KernelEventFlag ef1 = nullptr;
    KernelEventFlag ef2 = nullptr;
    Require(sceKernelCreateEventFlag(&ef1, "unshared_collision", EvfAttrMulti, 0x1, nullptr) == SceOk);
    Require(sceKernelCreateEventFlag(&ef2, "unshared_collision", EvfAttrMulti, 0x2, nullptr) == SceOk);
    Require(ef1 != nullptr && ef2 != nullptr);
    Require(ef1 != ef2);
    Require(sceKernelDeleteEventFlag(ef1) == SceOk);
    Require(sceKernelDeleteEventFlag(ef2) == SceOk);

    KernelEventFlag shared1 = nullptr;
    KernelEventFlag shared2 = nullptr;
    Require(sceKernelCreateEventFlag(&shared1, "shared_collision", EvfAttrShared | EvfAttrMulti, 0, nullptr) == SceOk);
    Require(sceKernelCreateEventFlag(&shared2, "shared_collision", EvfAttrShared | EvfAttrMulti, 0, nullptr) == SCE_KERNEL_ERROR_EPERM);
    Require(shared2 == nullptr);
    Require(sceKernelDeleteEventFlag(shared1) == SceOk);

    Require(sceKernelCreateEventFlag(&shared2, "shared_collision", EvfAttrShared | EvfAttrMulti, 0, nullptr) == SceOk);
    Require(shared2 != nullptr);
    Require(sceKernelDeleteEventFlag(shared2) == SceOk);
}

void AuditEventFlagOpenCloseBounds() {
    KernelEventFlag opened = nullptr;
    Require(sceKernelOpenEventFlag(&opened, nullptr) == SCE_KERNEL_ERROR_EFAULT);

    const std::string name32(32, 'y');
    Require(sceKernelOpenEventFlag(&opened, name32.c_str()) == SCE_KERNEL_ERROR_ENAMETOOLONG);

    Require(sceKernelOpenEventFlag(&opened, "nonexistent_shared") == SCE_KERNEL_ERROR_ESRCH);

    KernelEventFlag unshared = nullptr;
    Require(sceKernelCreateEventFlag(&unshared, "not_shared_for_open", EvfAttrMulti, 0, nullptr) == SceOk);
    Require(sceKernelOpenEventFlag(&opened, "not_shared_for_open") == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelDeleteEventFlag(unshared) == SceOk);

    KernelEventFlag shared = nullptr;
    Require(sceKernelCreateEventFlag(&shared, "shared_open_target", EvfAttrShared | EvfAttrMulti, 0x1234, nullptr) == SceOk);
    Require(sceKernelOpenEventFlag(nullptr, "shared_open_target") == SCE_KERNEL_ERROR_EFAULT);

    Require(sceKernelOpenEventFlag(&opened, "shared_open_target") == SceOk);
    Require(opened != nullptr);
    Require(opened != shared);

    Require(sceKernelCloseEventFlag(nullptr) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelCloseEventFlag(reinterpret_cast<KernelEventFlag>(0x5555)) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelCloseEventFlag(shared) == SCE_KERNEL_ERROR_EPERM);

    Require(sceKernelDeleteEventFlag(opened) == SCE_KERNEL_ERROR_EPERM);

    Require(sceKernelCloseEventFlag(opened) == SceOk);
    Require(sceKernelCloseEventFlag(opened) == SCE_KERNEL_ERROR_ESRCH);

    Require(sceKernelOpenEventFlag(&opened, "shared_open_target") == SceOk);
    std::uint64_t pattern = 0;
    Require(sceKernelPollEventFlag(opened, 0x1234, EvfWaitModeAnd, &pattern) == SceOk);
    Require(pattern == 0x1234);

    Require(sceKernelCloseEventFlag(opened) == SceOk);
    Require(sceKernelDeleteEventFlag(shared) == SceOk);
}

void AuditEventFlagDeleteBounds() {
    Require(sceKernelDeleteEventFlag(nullptr) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelDeleteEventFlag(reinterpret_cast<KernelEventFlag>(0x9999)) == SCE_KERNEL_ERROR_ESRCH);

    KernelEventFlag ef = nullptr;
    Require(sceKernelCreateEventFlag(&ef, "del_lifecycle", EvfAttrMulti, 0x55, nullptr) == SceOk);
    Require(ef != nullptr);

    Require(sceKernelDeleteEventFlag(ef) == SceOk);
    Require(sceKernelDeleteEventFlag(ef) == SCE_KERNEL_ERROR_ESRCH);

    Require(sceKernelSetEventFlag(ef, 0x1) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelClearEventFlag(ef, 0x1) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelCancelEventFlag(ef, 0, nullptr) == SCE_KERNEL_ERROR_ESRCH);

    std::uint64_t result = 0;
    Require(sceKernelPollEventFlag(ef, 0x1, EvfWaitModeAnd, &result) == SCE_KERNEL_ERROR_ESRCH);

    KernelUseconds timeout = 0;
    Require(sceKernelWaitEventFlag(ef, 0x1, EvfWaitModeAnd, &result, &timeout) == SCE_KERNEL_ERROR_ESRCH);
}

void AuditEventFlagSetAndClearBounds() {
    Require(sceKernelSetEventFlag(nullptr, 0x1) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelClearEventFlag(nullptr, 0x1) == SCE_KERNEL_ERROR_ESRCH);

    KernelEventFlag ef = nullptr;
    Require(sceKernelCreateEventFlag(&ef, "set_clear_test", EvfAttrMulti, 0x0, nullptr) == SceOk);

    Require(sceKernelSetEventFlag(ef, 0x1) == SceOk);
    std::uint64_t pattern = 0;
    Require(sceKernelPollEventFlag(ef, 0x1, EvfWaitModeAnd, &pattern) == SceOk);
    Require(pattern == 0x1);

    Require(sceKernelSetEventFlag(ef, 0x2) == SceOk);
    Require(sceKernelPollEventFlag(ef, 0x3, EvfWaitModeAnd, &pattern) == SceOk);
    Require(pattern == 0x3);

    Require(sceKernelSetEventFlag(ef, 0x8000000000000000ULL) == SceOk);
    Require(sceKernelPollEventFlag(ef, 0x8000000000000000ULL, EvfWaitModeAnd, &pattern) == SceOk);

    Require(sceKernelClearEventFlag(ef, 0x2) == SceOk);
    Require(sceKernelPollEventFlag(ef, 0x2, EvfWaitModeAnd, &pattern) == SceOk);
    Require(pattern == 0x2);
    Require(sceKernelPollEventFlag(ef, 0x1, EvfWaitModeAnd, &pattern) == SCE_KERNEL_ERROR_EBUSY);
    Require(pattern == 0x2);

    Require(sceKernelClearEventFlag(ef, 0x0) == SceOk);
    Require(sceKernelPollEventFlag(ef, 0x2, EvfWaitModeAnd, &pattern) == SCE_KERNEL_ERROR_EBUSY);
    Require(pattern == 0x0);

    Require(sceKernelDeleteEventFlag(ef) == SceOk);
}

void AuditEventFlagCancelBounds() {
    Require(sceKernelCancelEventFlag(nullptr, 0, nullptr) == SCE_KERNEL_ERROR_ESRCH);

    KernelEventFlag ef = nullptr;
    Require(sceKernelCreateEventFlag(&ef, "cancel_test", EvfAttrMulti, 0x11, nullptr) == SceOk);

    int waitingThreads = -1;
    Require(sceKernelCancelEventFlag(ef, 0x77, &waitingThreads) == SceOk);
    Require(waitingThreads == 0);

    std::uint64_t pattern = 0;
    Require(sceKernelPollEventFlag(ef, 0x77, EvfWaitModeAnd, &pattern) == SceOk);
    Require(pattern == 0x77);

    Require(sceKernelCancelEventFlag(ef, 0x0, nullptr) == SceOk);
    Require(sceKernelPollEventFlag(ef, 0x1, EvfWaitModeAnd, &pattern) == SCE_KERNEL_ERROR_EBUSY);
    Require(pattern == 0x0);

    Require(sceKernelDeleteEventFlag(ef) == SceOk);
}

void AuditEventFlagPollBounds() {
    Require(sceKernelPollEventFlag(nullptr, 1, EvfWaitModeAnd, nullptr) == SCE_KERNEL_ERROR_ESRCH);

    KernelEventFlag ef = nullptr;
    Require(sceKernelCreateEventFlag(&ef, "poll_test", EvfAttrMulti, 0x0F, nullptr) == SceOk);

    Require(sceKernelPollEventFlag(ef, 0, EvfWaitModeAnd, nullptr) == SCE_KERNEL_ERROR_EINVAL);

    const std::uint32_t invalidModes[] = {
        0u,
        EvfWaitModeAnd | EvfWaitModeOr,
        EvfWaitModeClearAll,
        EvfWaitModeClearPat,
        EvfWaitModeAnd | EvfWaitModeClearAll | EvfWaitModeClearPat,
        EvfWaitModeOr | EvfWaitModeClearAll | EvfWaitModeClearPat,
        0x04u,
        0x08u,
        0x40u,
    };
    for (const auto mode : invalidModes) {
        Require(sceKernelPollEventFlag(ef, 1, mode, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    }

    std::uint64_t result = 0;
    Require(sceKernelPollEventFlag(ef, 0x10, EvfWaitModeAnd, &result) == SCE_KERNEL_ERROR_EBUSY);
    Require(result == 0x0F);
    Require(sceKernelPollEventFlag(ef, 0x10, EvfWaitModeAnd, nullptr) == SCE_KERNEL_ERROR_EBUSY);

    Require(sceKernelPollEventFlag(ef, 0x03, EvfWaitModeAnd, &result) == SceOk);
    Require(result == 0x0F);
    Require(sceKernelPollEventFlag(ef, 0x03, EvfWaitModeAnd, nullptr) == SceOk);

    Require(sceKernelPollEventFlag(ef, 0x11, EvfWaitModeOr, &result) == SceOk);
    Require(result == 0x0F);

    Require(sceKernelPollEventFlag(ef, 0x03, EvfWaitModeAnd | EvfWaitModeClearPat, &result) == SceOk);
    Require(result == 0x0F);

    Require(sceKernelPollEventFlag(ef, 0x0C, EvfWaitModeAnd, &result) == SceOk);
    Require(result == 0x0C);
    Require(sceKernelPollEventFlag(ef, 0x01, EvfWaitModeAnd, &result) == SCE_KERNEL_ERROR_EBUSY);
    Require(result == 0x0C);

    Require(sceKernelPollEventFlag(ef, 0x04, EvfWaitModeAnd | EvfWaitModeClearAll, &result) == SceOk);
    Require(result == 0x0C);
    Require(sceKernelPollEventFlag(ef, 0x01, EvfWaitModeAnd, &result) == SCE_KERNEL_ERROR_EBUSY);
    Require(result == 0x00);

    Require(sceKernelDeleteEventFlag(ef) == SceOk);
}

void AuditEventFlagWaitBounds() {
    Require(sceKernelWaitEventFlag(nullptr, 1, EvfWaitModeAnd, nullptr, nullptr) == SCE_KERNEL_ERROR_ESRCH);

    KernelEventFlag ef = nullptr;
    Require(sceKernelCreateEventFlag(&ef, "wait_test", EvfAttrMulti, 0x0F, nullptr) == SceOk);

    Require(sceKernelWaitEventFlag(ef, 0, EvfWaitModeAnd, nullptr, nullptr) == SCE_KERNEL_ERROR_EINVAL);

    const std::uint32_t invalidModes[] = {
        0u,
        EvfWaitModeAnd | EvfWaitModeOr,
        EvfWaitModeClearAll,
        EvfWaitModeClearPat,
        EvfWaitModeAnd | EvfWaitModeClearAll | EvfWaitModeClearPat,
        EvfWaitModeOr | EvfWaitModeClearAll | EvfWaitModeClearPat,
    };
    for (const auto mode : invalidModes) {
        Require(sceKernelWaitEventFlag(ef, 1, mode, nullptr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    }

    std::uint64_t result = 0;
    Require(sceKernelWaitEventFlag(ef, 0x03, EvfWaitModeAnd, &result, nullptr) == SceOk);
    Require(result == 0x0F);

    KernelUseconds timeout = 0;
    result = 0;
    Require(sceKernelWaitEventFlag(ef, 0x20, EvfWaitModeAnd, &result, &timeout) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(result == 0x0F);

    timeout = 0;
    Require(sceKernelWaitEventFlag(ef, 0x20, EvfWaitModeAnd, nullptr, &timeout) == SCE_KERNEL_ERROR_ETIMEDOUT);

    Require(sceKernelWaitEventFlag(ef, 0x03, EvfWaitModeAnd | EvfWaitModeClearPat, &result, nullptr) == SceOk);
    Require(result == 0x0F);

    timeout = 0;
    Require(sceKernelWaitEventFlag(ef, 0x01, EvfWaitModeAnd, &result, &timeout) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(result == 0x0C);

    Require(sceKernelWaitEventFlag(ef, 0x04, EvfWaitModeAnd | EvfWaitModeClearAll, &result, nullptr) == SceOk);
    Require(result == 0x0C);

    timeout = 0;
    Require(sceKernelWaitEventFlag(ef, 0x08, EvfWaitModeAnd, &result, &timeout) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(result == 0x00);

    Require(sceKernelDeleteEventFlag(ef) == SceOk);
}

} // namespace

int main() {
    AuditEventFlagCreateParameterBounds();
    AuditEventFlagSharedCreationAndCollisionBounds();
    AuditEventFlagOpenCloseBounds();
    AuditEventFlagDeleteBounds();
    AuditEventFlagSetAndClearBounds();
    AuditEventFlagCancelBounds();
    AuditEventFlagPollBounds();
    AuditEventFlagWaitBounds();
    return 0;
}
