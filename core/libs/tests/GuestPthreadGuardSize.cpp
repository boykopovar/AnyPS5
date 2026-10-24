#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI scePthreadAttrInit(PthreadAttr* attr);
int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr);
int APS5_VABI scePthreadAttrSetguardsize(PthreadAttr* attr, std::size_t guardSize);
int APS5_VABI scePthreadAttrGetguardsize(const PthreadAttr* attr, std::size_t* guardSize);
int APS5_VABI scePthreadAttrGet(Pthread thread, PthreadAttr* attr);
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
Pthread APS5_VABI scePthreadSelf();
int APS5_VABI pthread_attr_get_np_nid_postfix(Pthread thread, PthreadAttr* attr);
int APS5_VABI pthread_attr_getguardsize_nid_postfix(const PthreadAttr* attr, std::size_t* guardSize);
}

static constexpr int SCE_OK = 0;
static constexpr std::size_t DefaultGuardSize = 0x1000;

static void Require(bool value) { if (!value) std::abort(); }

struct Probe {
    std::size_t viaSce = 0;
    std::size_t viaNp = 0;
};

static void* ProbeEntry(void* opaque) {
    auto* probe = static_cast<Probe*>(opaque);
    const Pthread self = scePthreadSelf();
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(scePthreadAttrGet(self, &attr) == SCE_OK);
    Require(scePthreadAttrGetguardsize(&attr, &probe->viaSce) == SCE_OK);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    PthreadAttr np = nullptr;
    Require(scePthreadAttrInit(&np) == SCE_OK);
    Require(pthread_attr_get_np_nid_postfix(self, &np) == SCE_OK);
    Require(pthread_attr_getguardsize_nid_postfix(&np, &probe->viaNp) == SCE_OK);
    Require(scePthreadAttrDestroy(&np) == SCE_OK);
    return nullptr;
}

static void RequireProbe(const Pthread thread, std::size_t expected) {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(scePthreadAttrSetguardsize(&attr, 0xdeadbeef) == SCE_OK);
    Require(scePthreadAttrGet(thread, &attr) == SCE_OK);
    std::size_t viaSce = 0;
    Require(scePthreadAttrGetguardsize(&attr, &viaSce) == SCE_OK && viaSce == expected);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    PthreadAttr np = nullptr;
    Require(scePthreadAttrInit(&np) == SCE_OK);
    Require(scePthreadAttrSetguardsize(&np, 0xdeadbeef) == SCE_OK);
    Require(pthread_attr_get_np_nid_postfix(thread, &np) == SCE_OK);
    std::size_t viaNp = 0;
    Require(pthread_attr_getguardsize_nid_postfix(&np, &viaNp) == SCE_OK && viaNp == expected);
    Require(scePthreadAttrDestroy(&np) == SCE_OK);
}

int main() {
    const std::size_t sizes[] = {0, 1, 100, 0x1000, 0x10000};
    for (const std::size_t size : sizes) {
        PthreadAttr attr = nullptr;
        Require(scePthreadAttrInit(&attr) == SCE_OK);
        Require(scePthreadAttrSetguardsize(&attr, size) == SCE_OK);
        Probe probe{};
        Pthread thread = nullptr;
        Require(scePthreadCreate(&thread, &attr, ProbeEntry, &probe, "guard-probe") == SCE_OK);
        void* retval = nullptr;
        Require(scePthreadJoin(thread, &retval) == SCE_OK);
        Require(probe.viaSce == size && probe.viaNp == size);
        RequireProbe(thread, size);
        Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    }
    {
        PthreadAttr attr = nullptr;
        Require(scePthreadAttrInit(&attr) == SCE_OK);
        Probe probe{};
        Pthread thread = nullptr;
        Require(scePthreadCreate(&thread, &attr, ProbeEntry, &probe, "guard-default") == SCE_OK);
        void* retval = nullptr;
        Require(scePthreadJoin(thread, &retval) == SCE_OK);
        Require(probe.viaSce == DefaultGuardSize && probe.viaNp == DefaultGuardSize);
        RequireProbe(thread, DefaultGuardSize);
        Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    }
    {
        PthreadAttr np = nullptr;
        Require(scePthreadAttrInit(&np) == SCE_OK);
        Require(scePthreadAttrSetguardsize(&np, 0xdeadbeef) == SCE_OK);
        Require(pthread_attr_get_np_nid_postfix(scePthreadSelf(), &np) == SCE_OK);
        std::size_t guard = 0;
        Require(pthread_attr_getguardsize_nid_postfix(&np, &guard) == SCE_OK && guard == DefaultGuardSize);
        Require(scePthreadAttrDestroy(&np) == SCE_OK);
    }
    return 0;
}
