#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GENERAL_ASMMACROS_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GENERAL_ASMMACROS_HPP

#if defined(_WIN32)
#define APS5_ASM_SYMBOL(name) name
#define APS5_ASM_FUNCTION(name) ".globl " name "\n.def " name "; .scl 2; .type 32; .endef\n" name ":\n"
#define APS5_ASM_FUNCTION_END(name) ""
#elif defined(__APPLE__)
#define APS5_ASM_SYMBOL(name) "_" name
#define APS5_ASM_FUNCTION(name) ".globl _" name "\n_" name ":\n"
#define APS5_ASM_FUNCTION_END(name) ""
#else
#define APS5_ASM_SYMBOL(name) name
#define APS5_ASM_FUNCTION(name) ".globl " name "\n.type " name ", @function\n" name ":\n"
#define APS5_ASM_FUNCTION_END(name) ".size " name ", .-" name "\n"
#endif

#endif
