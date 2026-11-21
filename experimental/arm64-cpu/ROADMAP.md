# Experimental guest CPU: phase-based delivery roadmap

> Scope: x86-64 guest CPU research on ARM64/iOS. This is **not** a functional PS5 emulator.
> Replaces the former one-opcode/one-milestone cadence after M29.

## Delivery protocol

- Work in **phase-sized batches**, not individual opcodes. A batch may contain multiple commits.
- For each batch: implement, run local/static checks when available, push, inspect GitHub Actions results and job logs, fix failures, repeat until green. Never equate a successful build with functional correctness.
- Do not ask the maintainer to report 'green' after each commit. Ask only when human-only validation, credentials, device testing, scope decisions, or an external permission boundary is required.
- Keep all experimental code isolated under `experimental/arm64-cpu`; no implicit changes to the upstream project's main emulator.
- Maintain a reproducible test corpus and a short compatibility report for each phase. A regression blocks promotion.
- Preserve portable C++20 and Linux/Windows/macOS host builds, with iOS ARM64 compile validation.
- Avoid unverifiable compiler-byte provenance. Generate machine code from documented compiler/toolchain versions and compare with native execution where supported.
- Do not claim unattended background monitoring: CI checks and follow-up fixes occur when an active assistant/tool session is running.

## Phase 1 — Interpreter foundation [complete through M29]

Registers, bounded guest memory, stack, branches, calls, core arithmetic and bitwise instructions, selected x86-64 addressing modes, flags, conditionals, initial native differential tests, and a program-level regression corpus.

## Phase 2 — Correctness and instruction coverage [active]

**Deliverables**
1. Inventory implemented opcodes, operand widths, prefixes, addressing modes, flags and known gaps.
2. Consolidate duplicated byte/word/dword/qword operand helpers and prefix parsing. Validate unsupported combinations instead of silently executing wrong semantics.
3. Implement missing high-frequency compiler instructions as cohesive families, selected by actual compiled-corpus failures.
4. Add deterministic randomized and native differential tests for flags, boundary values, RIP-relative addresses, and malformed code.
5. Fix discovered correctness issues before expanding surface area.

**Exit criteria:** all existing tests green on supported CI hosts; documented instruction coverage and failure list; reproducible differential suite.

## Phase 3 — Compiled-program execution [baseline complete: 2,047/2,047 confirmed green]\n\nThe baseline covers real compiler-generated C functions, guarded arrays, sorting,\nrecursive callbacks, and separately compiled function calls. This does not\nrepresent general x86-64 software compatibility.\n\n## Phase 3 — Compiled-program execution

**Deliverables**
1. Reproducibly compile a C fixture suite to x86-64 with pinned flags/toolchains; extract function bytes with relocation checks.
2. Run loops, recursion, array access, pointer arithmetic, sorting, and integer algorithms against reference outputs.
3. Add test runner that reports first unsupported opcode/offset and failure category.
4. Establish baseline compatibility percentage **only for the published fixture corpus**.

**Exit criteria:** all selected baseline programs pass; unsupported programs are enumerated, not hidden.

## Phase 4 — Runtime and guest memory model [active]\n\n**First batch:** opt-in guest data/stack regions with guard gap, bounded AG64\nversion-1 image loader (entry zero only), initialized data, explicit register\nsetup, malformed image tests, and cross-platform CTest coverage. No ELF,\nrelocation processing, syscall emulation or general ABI yet.\n\n## Phase 4 — Runtime and guest memory model

**Deliverables:** separated code/data/stack mappings; safe memory permissions and bounds; relocations; guest ABI/calling convention; loader for a deliberately scoped object/executable format; deterministic error reporting.

**Exit criteria:** end-to-end execution of small multi-function binaries with bounded guest memory and tests.

## Phase 5 — ARM64 acceleration feasibility

**Deliverables:** interpreter profiling on ARM64; design review of ahead-of-time translation versus JIT; iOS code-signing/JIT entitlement feasibility assessment; optional cached decoded blocks or translation prototype where allowed.

**Exit criteria:** benchmarked improvement over the portable interpreter on representative workloads, or a documented reason to defer.

## Phase 6 — iOS application integration

**Deliverables:** ARM64/iOS app harness, test corpus runner, diagnostics, device performance and memory measurements, release/debug build workflows, device validation.

**Exit criteria:** reproducible signed test build and on-device test report. Device tests require maintainer access when not automatable.

## Phase 7 — PS5-specific research gate

Evaluate CPU ISA assumptions, operating system interfaces, executable formats, GPU and shader translation, firmware dependencies, graphics APIs, synchronization, legal constraints and performance. Only then decide whether a separate PS5 compatibility implementation is viable.

**Exit criteria:** evidence-backed go/no-go assessment; no implication that a CPU interpreter alone can boot PS5 games.

## Priority rules

1. Correctness regressions and CI failures.
2. Compiled-corpus blockers affecting many programs.
3. Portability, diagnostic quality, and automated validation.
4. Runtime integration and measured performance.
5. Low-frequency instructions only when demonstrated useful.

## Phase reporting

At meaningful checkpoints provide: phase and batch identifier; merged/pushed commits; CI matrix status; new passing/total corpus cases; regressions; top blockers; next planned batch. Avoid invented overall project completion percentages.
