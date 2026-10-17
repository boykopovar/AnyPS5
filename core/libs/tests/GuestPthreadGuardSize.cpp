#include "SceTypes.hpp"
#include <cstddef>
#include <cstdlib>
#include <future>
#include <new>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI scePthreadAttrInit(PthreadAttr* attr);
int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr);
int APS5_VABI scePthreadAttrGet(Pthread thread, PthreadAttr* attr);
int APS5_VABI scePthreadAttrGetguardsize(const PthreadAttr* attr, std::size_t* guardSize);
int APS5_VABI scePthreadAttrSetguardsize(PthreadAttr* attr, std::size_t guardSize);
int APS5_VABI scePthreadAttrSetstack(PthreadAttr* attr, void* addr, std::size_t size);
Pthread APS5_VABI scePthreadSelf();
int APS5_VABI pthread_attr_get_np_nid_postfix(Pthread thread, PthreadAttr* attr);
int APS5_VABI pthread_attr_getguardsize_nid_postfix(const PthreadAttr* attr, std::size_t* guardSize);
int APS5_VABI pthread_attr_setguardsize_nid_postfix(PthreadAttr* attr, std::size_t guardSize);
}

static constexpr std::size_t DEFAULT_GUARD_SIZE = 0x1000;
static constexpr std::size_t STALE_GUARD_SIZE = 0x7000;
static constexpr std::size_t USER_STACK_SIZE = 0x40000;

static void Require(bool value) { if (!value) std::abort(); }

static void RequireReportedGuardSize(Pthread thread, std::size_t expected) {
    PthreadAttr posix = nullptr;
    Require(scePthreadAttrInit(&posix) == 0);
    Require(pthread_attr_setguardsize_nid_postfix(&posix, STALE_GUARD_SIZE) == 0);
    Require(pthread_attr_get_np_nid_postfix(thread, &posix) == 0);
    std::size_t size = 0;
    Require(pthread_attr_getguardsize_nid_postfix(&posix, &size) == 0 && size == expected);
    Require(scePthreadAttrDestroy(&posix) == 0);

    PthreadAttr sce = nullptr;
    Require(scePthreadAttrInit(&sce) == 0);
    Require(scePthreadAttrSetguardsize(&sce, STALE_GUARD_SIZE) == 0);
    Require(scePthreadAttrGet(thread, &sce) == 0);
    size = 0;
    Require(scePthreadAttrGetguardsize(&sce, &size) == 0 && size == expected);
    Require(scePthreadAttrDestroy(&sce) == 0);
}

struct Request {
    std::future<void>* released;
    std::size_t expected;
};

static void* APS5_VABI Worker(void* arg) {
    const auto* request = static_cast<Request*>(arg);
    RequireReportedGuardSize(scePthreadSelf(), request->expected);
    request->released->get();
    return nullptr;
}

static void CheckCreatedWith(const std::size_t* requested) {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == 0);
    if (requested) Require(pthread_attr_setguardsize_nid_postfix(&attr, *requested) == 0);
    const std::size_t expected = requested ? *requested : DEFAULT_GUARD_SIZE;

    std::promise<void> release;
    auto released = release.get_future();
    Request request{&released, expected};
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, requested ? &attr : nullptr, Worker, &request, nullptr) == 0);
    if (requested) Require(pthread_attr_setguardsize_nid_postfix(&attr, STALE_GUARD_SIZE) == 0);
    Require(scePthreadAttrDestroy(&attr) == 0);
    RequireReportedGuardSize(thread, expected);
    release.set_value();
    Require(scePthreadJoin(thread, nullptr) == 0);
}

static void CheckUserStackGuardSize() {
    void* stack = ::operator new(USER_STACK_SIZE, std::align_val_t{0x1000});
    Require(stack != nullptr);
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == 0);
    Require(scePthreadAttrSetstack(&attr, stack, USER_STACK_SIZE) == 0);
    Require(scePthreadAttrSetguardsize(&attr, 0x2000) == 0);

    std::promise<void> release;
    auto released = release.get_future();
    Request request{&released, 0};
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, &attr, Worker, &request, nullptr) == 0);
    Require(scePthreadAttrDestroy(&attr) == 0);
    release.set_value();
    Require(scePthreadJoin(thread, nullptr) == 0);
    ::operator delete(stack, std::align_val_t{0x1000});
}

int main() {
    RequireReportedGuardSize(scePthreadSelf(), DEFAULT_GUARD_SIZE);
    CheckCreatedWith(nullptr);
    for (const std::size_t requested : {std::size_t{0}, std::size_t{0x2000}, std::size_t{0x10000}})
        CheckCreatedWith(&requested);
    CheckUserStackGuardSize();
}
