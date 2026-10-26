#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <process.h>
#include <windows.h>

extern "C" int __real_main(int argc, char** argv);

namespace {

template<typename TFunction>
TFunction LibcFunction(const char* name) {
    const HMODULE libc = GetModuleHandleW(L"libc.prx");
    if (libc == nullptr) return nullptr;
    return reinterpret_cast<TFunction>(reinterpret_cast<void*>(GetProcAddress(libc, name)));
}

}

extern "C" int __wrap_main(int argc, char** argv) {
    const int code = __real_main(argc, argv);
    const auto terminate = LibcFunction<void (*)(int)>("LibcTerminate_nid_no_patch");
    if (terminate == nullptr) return code;
    _cexit();
    std::cout.flush();
    std::clog.flush();
    std::cerr.flush();
    std::fflush(nullptr);
    terminate(code);
    return code;
}
