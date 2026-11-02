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
    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    if (result == 0) std::printf("HDR probe tests passed (driver %s)\n", driver.c_str());
    return result;
}
