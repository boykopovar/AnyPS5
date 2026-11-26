# relinker

## add Functional

- A `LEA` target that the [code analysis](../../../core/relinker/relinker/src/analysis/CodeInstructionCollector.cpp) finds used as a code pointer, in images without `PT_SCE_DYNLIBDATA` that have unwind or symbol functions, is told from data only by a port I/O instruction: it is taken as data when the bytes there, up to the end of its executable segment, decode to `IN`, `OUT`, `INS` or `OUTS`, which a title's code cannot execute, before a return, jump, trap or call, as the strings that `RC4_options` of the CRYPTOGAMS x86-64 RC4 returns do. A table used that way whose bytes reach a return, jump, trap or call before any of them is still followed as code, so its bytes can be rewritten as a TLS or GS access or fail the conversion.
