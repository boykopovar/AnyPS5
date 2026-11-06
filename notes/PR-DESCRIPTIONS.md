# Pull request texts — giannireale/AnyPS5 → boykopovar/AnyPS5

Eleven pull requests, each from a `pr/*` branch rebased on upstream `main` (`75a8668`).
Open them in this order: three of them are stacked and their base branch must be merged first.

| # | Branch | Base | Commits | Debt item closed |
|---|---|---|---|---|
| 1 | `pr/insertq-register-form` | upstream main | 1 | INSERTQ register form |
| 2 | `pr/amd-system-instructions` | **after #1** | 2 | MONITORX/MWAITX/CLZERO/MCOMMIT |
| 3 | `pr/sha-ni` | **after #2** | 6 | SHA-NI substitution |
| 4 | `pr/guest-intel-stubs` | upstream main | 1 | stubs rejected for guest modules |
| 5 | `pr/pe-icon` | upstream main | 1 | no icon in the generated executable |
| 6 | `pr/jpeg-sampling` | upstream main | 1 | libSceJpegEnc 4:2:2 and grayscale |
| 7 | `pr/mimg-coverage` | upstream main | 1 | — (MIMG coverage) |
| 8 | `pr/mimg-atomics` | **after #7** | 3 | — (MIMG coverage, progress report) |
| 9 | `pr/barycentric-target` | upstream main | 1 | barycentric coordinates silently skipped |
| 10 | `pr/linux-stub-execution` | **after #4** | 2 | Linux stub placement only covered synthetically |
| 11 | `pr/rip-relative-absorption` | **the whole stack** | 16 | — (usability: relink no longer fails on a common site) |

Every branch was built and its tests were run on top of upstream `main`, not only stacked.

---

## 1. `pr/insertq-register-form`

**Title:** `Lower the INSERTQ register form for --to-intel`

The register form of INSERTQ (`F2 0F 79 /r`) had no Intel lowering, so `--to-intel` failed the
relink on any title that uses it.

The new stub reads the length and index fields from bytes 8 and 9 of the source register, builds
the field mask with a variable `PSRLQ`/`PSLLQ` pair and merges it into the destination with
`PANDN`/`POR`. Two details worth reviewing:

* `PSLLQ`/`PSRLQ` with a register count treat a count above 63 as zero, so moving the control
  bytes down needs `PSRLDQ` (a byte shift) rather than a quadword shift.
* The final `MOVSD` keeps the upper quadword of the destination intact. The immediate form
  already clobbers it through its `PSHUFB` mask, so this is stricter than the existing path.

Constants that repeat are now shared inside one stub body, which shrinks the EXTRQ register form
from 224 to 160 bytes.

**How it was verified:** the test maps the generated stub with `mmap` and runs it on hardware over
distinct and equal operand pairs and over length/index pairs including length 0, comparing against
a reference model, and checks that the scratch registers and the upper quadword survive.

---

## 2. `pr/amd-system-instructions`

**Title:** `Lower MONITORX, MWAITX, MCOMMIT and CLZERO for --to-intel`

Four of the five remaining AMD-only system instructions have a correct Intel equivalent:

| Instruction | Lowering | Why it is correct |
|---|---|---|
| `MONITORX` | three byte `NOP`, in place | it only arms an address monitor |
| `MWAITX` | `PAUSE`, in place | MWAIT may wake for any reason, so returning at once is allowed |
| `MCOMMIT` | `MFENCE` + `CLC`, in place | `CLC` reports the success the caller tests on CF |
| `CLZERO` | stub | the address needs masking to a cache line |

`MWAITX` deserves a note: this is not an approximation. Spurious wake-ups are architecturally
permitted, so the pair degrades to a spin, which is a legal implementation rather than a lossy one.
That is why no technical debt marker is added for it.

`RDPRU` stays unsupported: its AMD performance counters have no Intel equivalent and returning zero
would be a silent lie.

**How it was verified:** the CLZERO stub is executed on a 192 byte buffer with an unaligned
address, checking that exactly the 64 bytes of the line are zeroed and that RAX and the flags
survive.

---

## 3. `pr/sha-ni`

**Title:** `Lower the SHA-NI instructions for --to-intel`

All seven SHA-NI instructions, in both the register and the memory operand form. Previously
`IsShaNi` existed but no caller used it, so these instructions were left in the image and would
raise SIGILL on an Intel CPU without SHA-NI.

| Instruction | Strategy |
|---|---|
| `SHA1NEXTE` | `PSLLD`/`PSRLD`/`POR` rotate, masked to the high lane, `PADDD` |
| `SHA1MSG1` | byte shifts to align the lanes, `PXOR` |
| `SHA1MSG2` | rotate on every lane, second pass for the word that depends on the first result |
| `SHA256MSG1` | sigma0 as a rotate pair and a logical shift |
| `SHA256MSG2` | no lane mask is needed because sigma of a zeroed lane is zero |
| `SHA1RNDS4` | four sequential rounds on general purpose registers in a frame below the red zone |
| `SHA256RNDS2` | two rounds, with the implicit XMM0 word pair kept in the same frame |

The stub body builder moves out of the SSE4a lowering so every AMD-only lowering can emit spills,
RIP-relative constants, dword shifts and now integer instructions. The memory operand form loads
the operand with `MOVDQU` and then reuses the register form unchanged; a stack relative operand is
re-encoded with the frame size added to its displacement. A RIP-relative operand is rejected,
because a stub sits at a different address and the displacement cannot be recomputed at this level.

**How it was verified:** the development host has SHA-NI, so every stub was compared against the
hardware instruction itself over random inputs, which is stronger than a digest test vector: it
isolates a single instruction rather than checking only the final hash. The committed test keeps
both oracles, a reference model and the native instruction behind a runtime `cpuid` check, with the
instruction encoded as raw bytes so the test still builds without `-msha`. Six addressing modes are
covered for the memory form, including an indexed SIB and a stack relative operand.

---

## 4. `pr/guest-intel-stubs`

**Title:** `Write --to-intel stubs into guest modules`

Guest modules refused every lowering that needs a stub, so `--to-intel` failed on any `sce_module`
that uses the register form of EXTRQ or INSERTQ, CLZERO, or a short SSE4a site.

The trampoline appender moves out of the Linux ELF patcher into a shared writer, the guest image
carries its stub sites, the Linux guest writer places them at the start of its executable extra
block, and the Windows guest writer reuses the existing `.amdstub` builder, so the PE side gains no
new code.

**How it was verified:** a new integration test builds an `sce_module` holding a guest ELF with an
EXTRQ register form, runs the relinker for both targets, and inspects the produced `.guest.prx`:
for Linux it resolves the jump displacement and checks the target lands inside an executable
segment, for Windows it checks the `.amdstub` section exists.

---

## 5. `pr/pe-icon`

**Title:** `Embed an icon into the generated PE with --icon`

`--icon <path.ico>` writes the icon into the generated executable using only the standard library,
as the debt item requires.

Every image in the ICO becomes an `RT_ICON` resource and a synthesised `GRPICONDIR` becomes
`RT_GROUP_ICON` id 1, which is the structure Explorer reads to pick a size. The three level
resource tree goes into a new `.rsrc` section wired to data directory 2, with absolute RVAs in the
data entries. Image payloads are copied verbatim, so both DIB and PNG icons work.

**How it was verified:** an integration test runs the real ELF to PE pipeline and then walks the
resource tree of the produced binary, comparing every payload with the source image and every
group entry, plus the negative cases (a cursor file is rejected, `--icon` without `--windows` is
rejected, no `.rsrc` appears when the option is absent). The output was also parsed with `pefile`
as an independent check.

---

## 6. `pr/jpeg-sampling`

**Title:** `Encode 4:2:2 and grayscale as the caller asks`

The stb writer only produces three component images with 4:2:0 or 4:4:4 chroma, so libSceJpegEnc
silently answered a 4:2:2 request with 4:2:0 and a grayscale request with neutral chroma in three
components: valid files, but not the ones the caller asked for.

This adds a baseline JPEG encoder using only the standard library (Annex K quantisation tables
scaled by quality, the standard Huffman tables, a separable DCT, a bitstream with byte stuffing,
and an MCU loop with variable sampling factors), wires the requested sampling through
`libSceJpegEnc`, and drops the quality clamp that worked around the old writer. Decoding still goes
through stb.

**How it was verified:** the existing `decoder_jpeg` test now checks the sampling factors in SOF0,
that grayscale really has one component, and that the encode/decode round trip degrades
monotonically with subsampling. The output was also decoded with Pillow, an implementation
unrelated to stb, which reads all four variants.

---

## 7. `pr/mimg-coverage`

**Title:** `Decode nine more MIMG opcodes and lower the signed image atomics`

Adds `IMAGE_GATHER4`, `IMAGE_GATHER4_O`, `IMAGE_ATOMIC_SUB`, `IMAGE_ATOMIC_SMIN`,
`IMAGE_ATOMIC_SMAX` and the shadow sample encodings with an offset (`IMAGE_SAMPLE_C_D_O`,
`C_L_O`, `C_B_O`, `C_LZ_O`), which are the common PCF shadow taps.

The signed atomics reach SPIR-V through `OpAtomicISub`, `OpAtomicSMin` and `OpAtomicSMax`. The
gathers reuse the existing flag driven emitter, which now rejects an implicit-LOD gather outside a
pixel shader instead of emitting an invalid `OpImageGather`.

Deliberately left out, because they would need a silent approximation: `GATHER4_L` and `_B`, which
need `SPV_AMD_texture_gather_bias_lod`; `ATOMIC_INC` and `DEC`, whose wrap semantics have no direct
SPIR-V equivalent; and every `_CL` variant, which `validateFlags` still rejects.

**How it was verified:** a new `rdna_image_decoder` test target, standalone so it does not need
glslang, checks opcodes, address flags, component and dword counts, and the negative cases. A
mutation check confirms the assertions bite.

---

## 8. `pr/mimg-atomics`

**Title:** `Lower IMAGE_ATOMIC_CMPSWAP and correct the shader progress report`

Two independent commits on top of #7.

`IMAGE_ATOMIC_CMPSWAP` maps exactly onto `OpAtomicCompareExchange`. The ISA encodes its value and
comparator in two consecutive data registers as DMASK `0x3`, so the decoder now applies that rule
while every other 32-bit image atomic keeps requiring a single bit mask and the 64-bit form stays
rejected.

The second commit corrects a measurement, not coverage, and is separate on purpose. `progress.py`
derived shader coverage from the `RdnaOpcode` enum alone, but the image decoder folds every
`IMAGE_SAMPLE` variant into `RdnaOpcode::ImageSample` plus an address flag mask, so encodings that
have been decoded for a long time were counted as missing. The report now also reads the decoder
table and credits the named entries whose flags the decoder accepts, keeping the lod clamp, coarse
derivative and adjust variants in the todo list because `validateFlags` still rejects them. MIMG
goes from 29 to 47 of 130 without any decoder change.

---

## 9. `pr/barycentric-target`

**Title:** `Refuse barycentric coordinates the target does not enable`

`fragmentShaderBarycentricEnabled` reached the translator, which generates the barycentric inputs,
but never reached the SPIR-V backend: `SpirvTargetOptions` had no such field. The backend therefore
emitted `CapabilityFragmentBarycentricKHR` and its extension without checking the device, unlike
every other optional capability in the backend. On a device without the extension the driver
rejects the module at runtime instead of the recompilation failing with a clear message.

The flag now travels into `SpirvTargetOptions` and the barycentric branch throws when the target
lacks the enable, the capability or the extension, in the same shape as `SpirvBdaRead.cpp`.

**How it was verified, and its limit:** the recompiler compiles and the request serialisation
contract test now covers the flag, but the new diagnostic itself was not executed: that needs a
pixel `IrProgram` with a barycentric input and a binding allocation, or a GPU. Unlike the other
pull requests here, this one is reviewed logic rather than measured behaviour.

---

## 10. `pr/linux-stub-execution`

**Title:** `Run a relinked Linux executable through an AMD-only stub`

The Linux stub placement was only checked by inspecting bytes, so nothing proved that a relinked
image actually runs through a stub.

This test builds a small position independent executable whose exit code is produced by the
register form of EXTRQ, relinks it with `--to-intel`, and then **runs it**. The jump into the stub,
the stub body, the instruction absorbed into the stub because the site was shorter than a jump, and
the return jump are all exercised on the host, end to end. Outside Linux on x86-64 the test falls
back to resolving the patched jump and checking that its target lands inside an executable segment.

**How it was verified:** the relinked executable exits with 42, the value EXTRQ extracts. A
mutation check confirms the test bites: with a deliberately wrong shift in the EXTRQ lowering the
relinked executable exits with 0 and the test fails.

*Note: this one is stacked on #4 only because both touch the same block of the relinker
CMakeLists; there is no code dependency between them.*

---

## 11. `pr/rip-relative-absorption`

**Title:** `Relocate RIP-relative operands absorbed into a stub`

*This branch is the whole stack, because the change touches components introduced by almost every
other pull request here. Open it last, or take it as a single catch-all.*

An AMD-only instruction shorter than a jump needs the instructions that follow it to move into the
stub, and the converter refused to move anything with a RIP-relative operand. In compiled SSE code
such an operand is everywhere, usually a constant pool load, so this rejected a large class of real
call sites and made `--to-intel` fail on code it could otherwise handle.

The trampoline site now carries the absolute target of every RIP-relative operand it absorbed, and
both the ELF and the PE trampoline writers recompute the displacement from the address the stub
ends up at. Branches still stop the absorption, because their target would need the same treatment
in the opposite direction.

**How it was verified:** the Linux execution test gained a second scenario where the instruction
following the four byte site is `paddd xmm0, [rip+addend]`. The relinked executable runs and exits
with the extracted field plus the addend it loaded from inside the stub. With the fixups disabled
the same executable segfaults.
