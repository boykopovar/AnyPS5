#include <dlfcn.h>
#include <cstdio>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::puts("no library given");
        return 1;
    }
    for (int index = 1; index < argc; ++index) {
        void* library = dlopen(argv[index], RTLD_NOW | RTLD_LOCAL);
        if (library == nullptr) {
            std::printf("cannot load %s: %s\n", argv[index], dlerror());
            return 1;
        }
        if (dlclose(library) != 0) {
            std::printf("cannot unload %s: %s\n", argv[index], dlerror());
            return 1;
        }
    }
    std::puts("every suppressed library was loaded and unloaded without a sanitizer abort");
    return 0;
}
