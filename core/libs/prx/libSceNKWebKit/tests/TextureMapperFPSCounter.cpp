#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/Api.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore::TextureMapperFPSCounter check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

using NKWebKit::TextureMapperAbi::TextureMapperFPSCounter;

static TextureMapperFPSCounter Filled() {
    return {true, 99.0, 98.0, 7, 8};
}

static double Parse(const char* text, bool& ok) {
    const NKWebKit::WtfAbi::String string = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
    ok = false;
    return _ZNK3WTF6String8toDoubleEPb(&string, &ok);
}

static void RequireHidden(const TextureMapperFPSCounter& counter, double interval) {
    Require(!counter.isShowingFPS);
    Require(counter.fpsInterval == interval || (std::isnan(counter.fpsInterval) && std::isnan(interval)));
    Require(counter.fpsTimestamp == 0);
    Require(counter.lastFPS == 0 && counter.frameCount == 0);
}

static void RequireShown(const TextureMapperFPSCounter& counter, double interval, double before) {
    const double after = _ZN3WTF13MonotonicTime3nowEv().m_value;
    Require(counter.isShowingFPS);
    Require(counter.fpsInterval == interval);
    Require(counter.fpsTimestamp >= before && counter.fpsTimestamp <= after);
    Require(counter.lastFPS == 0 && counter.frameCount == 0);
}

static void Construct(bool complete, TextureMapperFPSCounter& counter) {
    if (complete) _ZN7WebCore23TextureMapperFPSCounterC1Ev(&counter);
    else _ZN7WebCore23TextureMapperFPSCounterC2Ev(&counter);
}

int main() {
    for (bool complete : {true, false}) {
        Require(unsetenv_nid_postfix("WEBKIT_SHOW_FPS") == 0);
        TextureMapperFPSCounter counter = Filled();
        Construct(complete, counter);
        RequireHidden(counter, 0);

        for (const char* text : {"0.5", "2", "-1", "1e-3"}) {
            Require(setenv_nid_postfix("WEBKIT_SHOW_FPS", text, 1) == 0);
            bool ok = false;
            const double expected = Parse(text, ok);
            Require(ok);
            const double before = _ZN3WTF13MonotonicTime3nowEv().m_value;
            counter = Filled();
            Construct(complete, counter);
            RequireShown(counter, expected, before);
        }

        for (const char* text : {"0", "-0", "", "abc", "1.5x"}) {
            Require(setenv_nid_postfix("WEBKIT_SHOW_FPS", text, 1) == 0);
            bool ok = false;
            const double expected = Parse(text, ok);
            counter = Filled();
            Construct(complete, counter);
            if (ok) Require(expected == 0);
            RequireHidden(counter, expected);
        }
    }
    Require(unsetenv_nid_postfix("WEBKIT_SHOW_FPS") == 0);

    bool threw = false;
    try {
        _ZN7WebCore23TextureMapperFPSCounterC1Ev(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    std::puts("libSceNKWebKit WebCore::TextureMapperFPSCounter passed");
    return 0;
}
