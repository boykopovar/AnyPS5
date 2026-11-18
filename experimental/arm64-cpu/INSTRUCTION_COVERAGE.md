# Phase 2 — Instruction coverage and correctness ledger

Status: **in progress**. This is a source-level inventory, **not** a measured percentage of x86-64 ISA support. The interpreter implements a deliberately limited integer subset.

## Implemented instruction groups (32/64-bit unless noted)

| Family | Opcodes / forms | Notes |
| --- | --- | --- |
| Register immediates | B8..BF, 66 B8..BF, REX.W B8..BF | 16-bit register writes preserve upper bits; 32-bit writes clear upper half |
| Data movement | 88/8A byte MOV; 89/8B MOV; 66 89/8B word MOV; C6 /0 byte immediate; C7 /0 word/dword/qword immediate | ModR/M register and bounded memory; REX high-byte semantics |
| Addressing | 8D LEA; ModR/M with SIB, displacement, RIP-relative | 64-bit addressing subset; no segment or address-size overrides |
| Extensions | 63 MOVSXD; 0F B6/B7 MOVZX; 0F BE/BF MOVSX; 98 CBW/CWDE/CDQE; 99 CWD/CDQ/CQO | Operand-size support varies by form |
| Integer ALU | ADD, SUB, ADC, SBB, CMP, TEST, AND, OR, XOR register and selected immediate forms; F7 NOT/NEG/TEST; FF INC/DEC | No 16-bit ALU beyond MOV; byte group F6 not implemented |
| Multiplication | 0F AF two-operand IMUL; 69/6B three-operand IMUL; F7 /4 MUL, /5 IMUL, /6 DIV, /7 IDIV | 32/64-bit forms; no byte F6 multiply or divide |
| Shifts | C1/D1/D3 ROL/ROR/SHL/SHR/SAR | No SHLD/SHRD, D0/D2 byte counts, or 16-bit forms |
| Conditions | short and near Jcc, 0F 90..9F SETcc, 0F 40..4F CMOVcc | ZF/CF/SF/OF/PF tracked; AF not tracked |
| Control flow | E8 CALL, C3 RET, E9/EB JMP, FF /2 CALL and /4 JMP, PUSH/POP, C9 LEAVE | No syscalls, exceptions or far branches |
| Alignment | 90 NOP, 0F 1F /0 NOP | Some prefix combinations unsupported |

## Known correctness and completeness limitations

1. **Prefix handling:** legacy 66 and REX ordering improved, but the full x86-64 prefix state machine (segment overrides, address-size 67, LOCK/REP) is not implemented.
2. **Flags:** parity flag was added in the current batch; auxiliary carry flag and undefined-flag behaviours are not modelled. Differential coverage remains incomplete.
3. **Memory:** one bounded flat byte vector currently acts as guest memory/stack; no independent code/data mappings or permissions.
4. **Instruction coverage:** no SIMD/FPU, atomics, system instructions, exception delivery, or comprehensive 8/16-bit arithmetic.
5. **Decoder:** instruction length and unsupported combinations need further fuzzing. No production-grade instruction decoder.
6. **Compiler fixtures:** existing hand-copied byte arrays are not a substitute for reproducible compilation and extraction.
7. **Platform:** ARM64/iOS is compile-tested; native device runtime and performance have not been established.
8. **PS5:** a partial x86-64 interpreter is not a PS5 emulator; guest OS, GPU, executable formats, and other subsystems are out of scope.

## Validation gates

- Existing unit tests and program corpus green on CI.
- Differential tests green on supported x86-64 GCC/Clang runners.
- New flag/prefix tests green.
- No known regressions before further feature expansion.

## Next high-impact batches

- Implement the 16-bit ALU forms and the byte F6 group with the same edge-case and differential coverage.
- Expand native differential testing and use sanitizers/fuzzing to find decoder and memory defects.
- Consolidate duplicated width-specific operand access after tests protect current semantics.
- Add compiler-generated signed-division fixtures once the loader/ABI fixtures support signed arguments.
