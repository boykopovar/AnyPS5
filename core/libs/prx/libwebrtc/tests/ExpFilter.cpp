#include "prx/libwebrtc/libwebrtc.hpp"
#include "Testing.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Reset(WebRtcExpFilter& filter, float alpha) {
    _ZN3rtc9ExpFilter5ResetEf_nid_postfix(&filter, alpha);
}

float Apply(WebRtcExpFilter& filter, float exponent, float sample) {
    return _ZN3rtc9ExpFilter5ApplyEff_nid_postfix(&filter, exponent, sample);
}

void TestReset() {
    WebRtcExpFilter filter {0.25f, 7.0f, 3.0f};
    Reset(filter, 0.5f);
    Require(filter.alpha == 0.5f);
    Require(filter.filtered == WebRtcExpFilterValueUndefined);
    Require(filter.max == 3.0f);
}

void TestApply() {
    WebRtcExpFilter filter {0.0f, 0.0f, WebRtcExpFilterValueUndefined};
    Reset(filter, 0.5f);
    Require(Apply(filter, 1.0f, 4.0f) == 4.0f);
    Require(Apply(filter, 1.0f, 8.0f) == 6.0f);
    Require(Apply(filter, 2.0f, 10.0f) == 9.0f);
    Require(filter.filtered == 9.0f);
    float alpha = std::pow(0.5f, 0.5f);
    float expected = alpha * 9.0f + (1 - alpha) * 1.0f;
    Require(Apply(filter, 0.5f, 1.0f) == expected);
    Require(Apply(filter, 1.0f, 1000.0f) > 400.0f);
}

void TestUndefinedOutputRestarts() {
    WebRtcExpFilter filter {0.0f, 0.0f, WebRtcExpFilterValueUndefined};
    Reset(filter, 0.5f);
    Require(Apply(filter, 1.0f, 0.0f) == 0.0f);
    Require(Apply(filter, 1.0f, -2.0f) == -1.0f);
    Require(Apply(filter, 1.0f, 10.0f) == 10.0f);
}

void TestMax() {
    WebRtcExpFilter filter {0.0f, 0.0f, 5.0f};
    Reset(filter, 0.5f);
    Require(Apply(filter, 1.0f, 9.0f) == 5.0f);
    Require(Apply(filter, 1.0f, 1.0f) == 3.0f);
    Require(Apply(filter, 1.0f, 5.0f) == 4.0f);
    Require(filter.max == 5.0f);
}

void TestNullFilter() {
    ExpectErr<std::invalid_argument>([] { _ZN3rtc9ExpFilter5ResetEf_nid_postfix(nullptr, 0.5f); });
    ExpectErr<std::invalid_argument>([] { _ZN3rtc9ExpFilter5ApplyEff_nid_postfix(nullptr, 1.0f, 1.0f); });
}

struct GuardedPage {
    void* base = nullptr;
    std::size_t size = 0;

    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        base = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
        size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        base = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED)
            base = nullptr;
#endif
        Require(base != nullptr);
    }

    ~GuardedPage() {
        if (!base) return;
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, size);
#endif
    }

    template<typename T> T* As() const { return static_cast<T*>(base); }

    void SetReadOnly() const {
#ifdef _WIN32
        DWORD prev = 0;
        Require(VirtualProtect(base, size, PAGE_READONLY, &prev) != 0);
#else
        Require(mprotect(base, size, PROT_READ) == 0);
#endif
    }
};

void TestGuardedPointers() {
    {
        GuardedPage page;
        *page.As<WebRtcExpFilter>() = WebRtcExpFilter{0.5f, 4.0f, 10.0f};
        page.SetReadOnly();
        ExpectErr<std::invalid_argument>([&] { _ZN3rtc9ExpFilter5ResetEf_nid_postfix(page.As<WebRtcExpFilter>(), 0.25f); });
        ExpectErr<std::invalid_argument>([&] { _ZN3rtc9ExpFilter5ApplyEff_nid_postfix(page.As<WebRtcExpFilter>(), 1.0f, 8.0f); });
    }
}

}

int main() {
    TestReset();
    TestApply();
    TestUndefinedOutputRestarts();
    TestMax();
    TestNullFilter();
    TestGuardedPointers();
    std::puts("libwebrtc ExpFilter tests passed");
}
