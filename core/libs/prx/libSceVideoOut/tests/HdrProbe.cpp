#include "prx/libSceVideoOut/include/HostHdrProbe.hpp"
#include "prx/libSceAgcDriver/Execution/include/HostHdr.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#include "SDL_syswm.h"
#include <windows.h>
#include <dxgi1_6.h>
#endif

#undef main

namespace {

#ifdef _WIN32
bool monitorReportsHdr(SDL_Window* window) {
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window, &info) != SDL_TRUE || info.subsystem != SDL_SYSWM_WINDOWS) return false;
    const HMONITOR monitor = MonitorFromWindow(info.info.win.window, MONITOR_DEFAULTTONEAREST);
    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))) return false;
    bool hdr = false;
    IDXGIAdapter1* adapter = nullptr;
    for (UINT adapterIndex = 0; !hdr && SUCCEEDED(factory->EnumAdapters1(adapterIndex, &adapter)); ++adapterIndex) {
        IDXGIOutput* output = nullptr;
        for (UINT outputIndex = 0; !hdr && SUCCEEDED(adapter->EnumOutputs(outputIndex, &output)); ++outputIndex) {
            DXGI_OUTPUT_DESC description{};
            IDXGIOutput6* output6 = nullptr;
            DXGI_OUTPUT_DESC1 description1{};
            if (SUCCEEDED(output->GetDesc(&description)) && description.Monitor == monitor &&
                SUCCEEDED(output->QueryInterface(__uuidof(IDXGIOutput6), reinterpret_cast<void**>(&output6))) && SUCCEEDED(output6->GetDesc1(&description1)))
                hdr = description1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
            if (output6 != nullptr) output6->Release();
            output->Release();
        }
        adapter->Release();
    }
    factory->Release();
    return hdr;
}

bool testWindowQueryFailure(SDL_Window*& window) {
    HostHdrProbe probe;
    probe.Poll(window);
    AgcDriverSetHostDisplayHdr_nid_postfix(AgcDriver::UndescribedHdrDisplay);
    SDL_DestroyWindow(window);
    window = nullptr;
    SDL_VideoQuit();
    if (SDL_VideoInit("dummy") != 0) {
        std::fprintf(stderr, "FAIL: could not initialize the dummy video driver: %s\n", SDL_GetError());
        return false;
    }
    window = SDL_CreateWindow("HDR probe failure test", 0, 0, 64, 64, SDL_WINDOW_HIDDEN);
    if (window == nullptr) {
        std::fprintf(stderr, "FAIL: could not create a dummy window: %s\n", SDL_GetError());
        return false;
    }
    probe.Poll(window);
    bool passed = !AgcDriver::CurrentHostHdr().display.hdr;
    AgcDriverSetHostDisplayHdr_nid_postfix(AgcDriver::UndescribedHdrDisplay);
    probe.Poll(window);
    passed = passed && !AgcDriver::CurrentHostHdr().display.hdr;
    SDL_DestroyWindow(window);
    window = nullptr;
    SDL_VideoQuit();
    if (SDL_VideoInit("windows") != 0) {
        std::fprintf(stderr, "FAIL: could not restore the Windows video driver: %s\n", SDL_GetError());
        return false;
    }
    window = SDL_CreateWindow("HDR probe recovery test", 0, 0, 64, 64, SDL_WINDOW_HIDDEN);
    if (window == nullptr) {
        std::fprintf(stderr, "FAIL: could not create a Windows window: %s\n", SDL_GetError());
        return false;
    }
    AgcDriverSetHostDisplayHdr_nid_postfix(AgcDriver::UndescribedHdrDisplay);
    probe.Poll(window);
    passed = passed && AgcDriver::CurrentHostHdr().display.hdr == monitorReportsHdr(window);
    if (!passed) std::fprintf(stderr, "FAIL: HDR probe did not clear or recover its Windows monitor report\n");
    return passed;
}
#endif

}

int main() {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        std::printf("skipped, no SDL video driver: %s\n", SDL_GetError());
        return 77;
    }
    SDL_Window* window = SDL_CreateWindow("HDR probe test", 0, 0, 64, 64, SDL_WINDOW_HIDDEN);
    if (window == nullptr) {
        std::printf("skipped, no SDL window: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return 77;
    }
    const std::string driver = SDL_GetCurrentVideoDriver() != nullptr ? SDL_GetCurrentVideoDriver() : "unknown";
#ifdef _WIN32
    if (monitorReportsHdr(window)) {
        std::printf("skipped, the window's monitor reports HDR to Windows (driver %s)\n", driver.c_str());
        SDL_DestroyWindow(window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return 77;
    }
#endif
    AgcDriverSetSurfaceHdr10_nid_postfix(true);
    int result = 0;
    {
        HostHdrProbe probe;
        probe.Poll(window);
        probe.Poll(window);
        const auto host = AgcDriver::CurrentHostHdr();
        if (host.display.hdr || host.available) {
            std::fprintf(stderr, "FAIL: a window without a Windows HDR monitor report reads as HDR although only the surface offers HDR10 (driver %s)\n", driver.c_str());
            result = 1;
        }
    }
#ifdef _WIN32
    if (driver == "windows" && !testWindowQueryFailure(window)) result = 1;
#endif
    if (window != nullptr) SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    if (result == 0) std::printf("HDR probe tests passed (driver %s)\n", driver.c_str());
    return result;
}
