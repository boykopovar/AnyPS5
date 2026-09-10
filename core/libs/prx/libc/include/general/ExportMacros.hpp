#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GENERAL_EXPORTMACROS_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GENERAL_EXPORTMACROS_HPP

#if defined(__GNUC__) || defined(__clang__)
    #define APS5_EXPORT(exportName, funcName) \
    __asm__(".globl \"" exportName "_nid_no_patch_cut\"\n\t" \
    ".set \"" exportName "_nid_no_patch_cut\", " #funcName)
#elif defined(_MSC_VER)
    #define APS5_EXPORT(exportName, funcName) \
    __pragma(comment(linker, "/export:" exportName "_nid_no_patch_cut=" #funcName))
#else
    #define APS5_EXPORT(exportName, funcName)
#endif

#endif
