# libkernel

## change SET_FSBASE throws because guest and host FS/TLS are shared

- [sysarch](../../../core/libs/prx/libkernel/System/src/Process.cpp) (libkernel) - SET_FSBASE throws because guest and host FS/TLS are shared. Windows and macOS GET_FSBASE/I386_GET_FSBASE return EFAULT for null arguments and throw for non-null arguments. GS GET/SET follows FreeBSD 11 amd64 [sysarch](https://github.com/freebsd/freebsd-src/blob/release/11.0.0/sys/amd64/amd64/sys_machdep.c); bases at or above VM_MAXUSER_ADDRESS (0x800000000000) fail with EINVAL and invalid parameter memory with EFAULT. A non-null argument the host cannot read (`AMD64_SET_GSBASE`) or write (the `GET` operations) also returns `EFAULT`; this is a host-safety fallback, and the console's answer for a non-null invalid pointer was not measured. Linux x86-64 uses arch_prctl; Windows and macOS x86-64 store a separate guest base. Other hosts throw for segment operations.

## change [guest GS base]

- The initial Windows and macOS [guest GS base](../../../core/libs/prx/libkernel/System/src/Process.cpp) (libkernel) is zero; the console's initial base is unmeasured. Native host-created Windows and macOS threads start with zero and their inheritance is not implemented. In a PS5 firmware 12.70 payload, one thread's GS changes left the main thread unchanged and pthread_create/scePthreadCreate children inherited their creator's base. Both guest thread APIs inherit on Windows and macOS; Linux [copy_thread](https://github.com/torvalds/linux/blob/v6.6/arch/x86/kernel/process.c#L182-L186) copies the creator's GS base. I386_GET_FSBASE (7) and I386_GET_GSBASE (9) wrote four low-base bytes on the console and failed with EFAULT for null; Linux implements both and Windows and macOS implement 9. Windows guest GS memory instructions still trap; only the thread state API is implemented.

# relinker

## change In macOS output, the conversion fails when a code entry lies after the first byte of a guest GS instruction

- In macOS output, the conversion fails when a code entry lies after the first byte of a guest GS instruction that the [macOS image writer](../../../core/relinker/elfpatcher/src/macos/MacOsImage.cpp) would replace with `UD2` or a jump to a stub: `Branch enters a guest GS instruction` for a relative branch target, `GS trap would overwrite an executable entry point` for any other entry the [code analysis](../../../core/relinker/relinker/src/analysis/CodeInstructionCollector.cpp) follows (a function symbol, a relocated pointer, a RIP-relative `lea` target, an initializer array entry) or for a pointer target it takes as data. Both messages give the position of the entered byte, and no option skips either check.

## change In macOS output, a guest instruction that addresses memory through GS

- In macOS output, a guest GS memory instruction that the [macOS image writer](../../../core/relinker/elfpatcher/src/macos/MacOsImage.cpp) cannot rewrite into a stub becomes `UD2` and NOP padding of its own length, and faults when reached: one shorter than 5 bytes; RIP-relative or `67` addressing; a `moffs` address outside signed 32 bits; VEX, EVEX or XOP; an opcode other than scalar `mov`, ALU, `test`, `xchg`, shifts, `not`, `neg`, `mul`, `imul`, `div`, `idiv`, `inc`, `dec`, `cmovcc`, `setcc`, `movzx`, `movsx`, `cmpxchg`, `xadd` and the SSE or MMX moves `0F 10/11/28/29/6E/6F/7E/7F/D6`; one whose register operand is the stack pointer; or one whose register operand, base and index take `rbx`, `rsi` and `rdi`. A fault in a rewritten access is reported in the stub, with the stack 136 bytes lower, not at the guest instruction. Whether the six in Prospero Win's (PPSA99995) Wine `ntdll` are rewritten is unmeasured.
