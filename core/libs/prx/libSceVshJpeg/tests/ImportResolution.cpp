#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

#ifndef VSH_JPEG_MODULE
#error VSH_JPEG_MODULE must name the libSceVshJpeg module file
#endif

int main() {
    void* module = dlopen(VSH_JPEG_MODULE, RTLD_NOW | RTLD_LOCAL);
    if (!module) {
        std::fprintf(stderr, "dlopen failed: %s\n", dlerror());
        return EXIT_FAILURE;
    }
    for (const char* name : {"jpeg_CreateCompress_nid_postfix", "jpeg_std_error_nid_postfix", "jpeg_CreateDecompress_nid_postfix",
                             "jpeg_read_header_nid_postfix", "jpeg_save_markers_nid_postfix"}) {
        if (!dlsym(module, name)) {
            std::fprintf(stderr, "unresolved through libSceVshJpeg: %s\n", name);
            return EXIT_FAILURE;
        }
    }
    std::puts("Vsh jpeg import resolution passed.");
    return EXIT_SUCCESS;
}
