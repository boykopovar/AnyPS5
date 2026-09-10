#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GENERAL_EXPORTMACROS_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GENERAL_EXPORTMACROS_HPP

#if defined(__GNUC__) || defined(__clang__)
    #define APS_EXPORT(exportName, funcName) \
    __asm__(".globl " exportName "\n" exportName " = " #funcName "_nid_no_patch_cut")
#elif defined(_MSC_VER)
    #define APS_EXPORT(exportName, funcName) \
    __pragma(comment(linker, "/export:" exportName "=" #funcName "_nid_no_patch_cut"))
#else
    #define APS_EXPORT(exportName, funcName)
#endif

#endif
