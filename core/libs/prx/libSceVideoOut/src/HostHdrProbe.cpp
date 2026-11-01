#include "prx/libSceVideoOut/include/HostHdrProbe.hpp"
#include "prx/libSceAgcDriver/Execution/include/HostHdr.hpp"
#include <stdexcept>

#ifdef _WIN32
#include "SDL_syswm.h"
#include <windows.h>
#include <dxgi1_6.h>
#endif

namespace {

#ifdef _WIN32
template<typename TInterface>
void releaseCom(TInterface*& object) noexcept {
    if (object != nullptr) object->Release();
    object = nullptr;
}

AgcDriver::HostDisplayHdr describeMonitor(IDXGIFactory1* factory, HMONITOR monitor) {
    for (UINT adapterIndex = 0;; ++adapterIndex) {
        IDXGIAdapter1* adapter = nullptr;
        if (FAILED(factory->EnumAdapters1(adapterIndex, &adapter))) return {};
        for (UINT outputIndex = 0;; ++outputIndex) {
            IDXGIOutput* output = nullptr;
            if (FAILED(adapter->EnumOutputs(outputIndex, &output))) break;
            DXGI_OUTPUT_DESC description{};
            const bool same = SUCCEEDED(output->GetDesc(&description)) && description.Monitor == monitor;
            IDXGIOutput6* output6 = nullptr;
            DXGI_OUTPUT_DESC1 description1{};
            const bool described = same && SUCCEEDED(output->QueryInterface(__uuidof(IDXGIOutput6), reinterpret_cast<void**>(&output6))) && SUCCEEDED(output6->GetDesc1(&description1));
            releaseCom(output6);
            releaseCom(output);
            if (!same) continue;
            releaseCom(adapter);
            if (!described) return {};
            return AgcDriver::DisplayHdrFromOutput(description1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020, description1.MaxLuminance, description1.MinLuminance, description1.MaxFullFrameLuminance);
        }
        releaseCom(adapter);
    }
}
#endif

}

HostHdrProbe::~HostHdrProbe() {
    Release();
}

void HostHdrProbe::Release() noexcept {
#ifdef _WIN32
    auto* dxgiFactory = static_cast<IDXGIFactory1*>(factory);
    releaseCom(dxgiFactory);
#endif
    factory = nullptr;
    monitor = nullptr;
}

void HostHdrProbe::Poll(SDL_Window* window) {
    if (window == nullptr) throw std::runtime_error("VideoOut: the HDR probe needs the window");
#ifdef _WIN32
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window, &info) != SDL_TRUE || info.subsystem != SDL_SYSWM_WINDOWS) {
        if (!published) AgcDriverSetHostDisplayHdr_nid_postfix({});
        published = true;
        return;
    }
    const HMONITOR current = MonitorFromWindow(info.info.win.window, MONITOR_DEFAULTTONEAREST);
    auto* dxgiFactory = static_cast<IDXGIFactory1*>(factory);
    if (published && current == monitor && (dxgiFactory == nullptr || dxgiFactory->IsCurrent())) return;
    releaseCom(dxgiFactory);
    factory = nullptr;
    monitor = current;
    published = true;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory)))) {
        AgcDriverSetHostDisplayHdr_nid_postfix({});
        return;
    }
    factory = dxgiFactory;
    AgcDriverSetHostDisplayHdr_nid_postfix(describeMonitor(dxgiFactory, current));
    return;
#else
    if (published) return;
    AgcDriverSetHostDisplayHdr_nid_postfix({});
#endif
    published = true;
}
