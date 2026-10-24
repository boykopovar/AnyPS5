#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdlib>
#include <cstring>

static constexpr int POSIX_EINVAL = 22;
static constexpr std::size_t INITIAL_DEFAULT = 1u << 20;

static void Require(bool value) { if (!value) std::abort(); }

static std::size_t NewAttrStackSize() {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == 0);
    std::size_t size = 0;
    Require(scePthreadAttrGetstacksize(&attr, &size) == 0);
    Require(scePthreadAttrDestroy(&attr) == 0);
    return size;
}

static void* APS5_VABI ReportStackSize(void* out) {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == 0);
    Require(scePthreadAttrGet(scePthreadSelf(), &attr) == 0);
    Require(scePthreadAttrGetstacksize(&attr, static_cast<std::size_t*>(out)) == 0);
    Require(scePthreadAttrDestroy(&attr) == 0);
    return nullptr;
}

static std::size_t DefaultThreadStackSize() {
    std::size_t size = 0;
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, ReportStackSize, &size, "stack") == 0);
    Require(scePthreadJoin(thread, nullptr) == 0);
    return size;
}

static void CheckDefaultStackSize() {
    Require(NewAttrStackSize() == INITIAL_DEFAULT);
    Require(pthread_set_defaultstacksize_np_nid_postfix(0) == POSIX_EINVAL);
    Require(pthread_set_defaultstacksize_np_nid_postfix(16384) == POSIX_EINVAL);
    Require(scePthreadSetDefaultstacksize(16384) == SCE_KERNEL_ERROR_EINVAL);
    Require(NewAttrStackSize() == INITIAL_DEFAULT);

    Require(pthread_set_defaultstacksize_np_nid_postfix(16385) == 0);
    Require(NewAttrStackSize() == 16385);

    Require(scePthreadSetDefaultstacksize(0x40000) == 0);
    Require(NewAttrStackSize() == 0x40000);
    Require(DefaultThreadStackSize() == 0x40000);

    Require(scePthreadSetDefaultstacksize(INITIAL_DEFAULT) == 0);
    Require(DefaultThreadStackSize() == INITIAL_DEFAULT);
}

static void CheckSetName() {
    char name[32] = {};
    scePthreadSetName(scePthreadSelf(), "renamed main");

    Require(scePthreadGetname(scePthreadSelf(), name) == 0);
    Require(std::strcmp(name, "renamed main") == 0);
    scePthreadSetName(scePthreadSelf(), nullptr);

    Require(scePthreadGetname(scePthreadSelf(), name) == 0);
    Require(name[0] == '\0');
}

int main() {
    CheckDefaultStackSize();
    CheckSetName();
    return 0;
}
