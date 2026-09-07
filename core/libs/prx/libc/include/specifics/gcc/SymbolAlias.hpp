#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GCC_SYMBOLALIAS_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GCC_SYMBOLALIAS_HPP

#define GCC_LOCAL_ALIAS(alias, target) \
    asm(".local " #alias "\n.set " #alias "," #target "\n")

#define GCC_GLOBAL_ALIAS(alias, target) \
    asm(".globl " #alias "\n.set " #alias "," #target "\n")

#define GCC_HIDDEN_FN

#endif
