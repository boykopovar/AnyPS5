# Can code be taken from the other PlayStation projects?

Survey done on 2026-09-30 to answer one question: is there code in the other open PS4/PS5
projects that AnyPS5 could reuse for the libraries it still lacks?

**Short answer: no code can be taken as it is. Only knowledge transfers.** The evidence is below,
along with the map of where AnyPS5 actually stands against the most complete of them.

---

## 1. The field, filtered by license

AnyPS5 is GPL-2.0-**only**. That is the first filter, and it removes projects before any
technical question is asked.

| Project | Language | License | Reusable in AnyPS5? |
|---|---|---|---|
| shadPS4 | C++ | GPL-2.0 | compatible |
| KytyPS5 | C++ | GPL-2.0 | compatible |
| RPCSX | C++ | GPL-2.0 | compatible |
| SharpEmu | **C#** | GPL-2.0 | license fine, wrong language |
| PS5PCEM | Zig | **GPL-3.0-or-later** | **incompatible** with GPL-2.0-only |
| Prosper | C++ | **none stated** | **cannot be reused**: no license means no permission |

A compatible license still requires keeping the copyright notices and stating the provenance.
Worth asking the maintainer first: some projects refuse imports to keep the provenance of their
code clean.

## 2. The technical filter: measured coupling

Counting the includes of the most interesting files tells the whole story. "internal" means the
file includes the emulator's own runtime: kernel objects, memory manager, logging, error tables.

| Source | File | Lines | Internal includes |
|---|---|---|---|
| shadPS4 | `ngs2/ngs2.cpp` | 591 | **10 of 10** |
| shadPS4 | `voice/voice.cpp` | 202 | **4 of 4** |
| shadPS4 | `videodec/vdecsw.cpp` | 369 | **7 of 7** |
| shadPS4 | `audio3d/audio3d.cpp` | 1325 | 8 of 12 |
| KytyPS5 | `libs/ajm.cpp` | 1050 | its own `common/`, `libs/` tree |

Those files are not modules with a clean boundary: they are the inside of an emulator. AnyPS5 has
no emulator to attach them to — it relinks the executable and runs it natively, so there is no
guest memory manager and no kernel object table to bind against. Porting means rewriting the logic
against AnyPS5's model, with the other project open beside you as a reference.

The audio decoders are a special case worth knowing: in KytyPS5 they are **thin wrappers**, ATRAC9
over LibAtrac9 and AAC/MP3/Opus over FFmpeg. AnyPS5 already vendors LibAtrac9 and already wraps it
in `libSceAjm.native`, so there is nothing to gain there; the FFmpeg ones would add a dependency
the project does not have.

## 3. Where AnyPS5 actually stands against shadPS4

shadPS4 implements 52 library directories. Matching them against `core/libs/prx`:

**Largest gaps, in effort order** — these are the places worth spending time, whether the code is
written from scratch or with shadPS4 as a reference:

| Library | AnyPS5 today | Note |
|---|---|---|
| `libSceNgs2` | 0 done / 25 stubs | audio mixing engine, the biggest single gap |
| `libSceVoice` | 0 / 16 | |
| `libSceIme` | 0 / 13 | on-screen keyboard, needs UI |
| `libSceAudio3d` | 0 / 7 | |
| `libSceAudio` | 0 / 5 | |
| `libSceFont` | 100 / 93 | half done already |
| `libSceVideodec` | **absent** | shadPS4 decodes video through FFmpeg |

**Absent from AnyPS5 entirely**: `camera`, `companion`, `disc_map`, `hmd`, `invitation_dialog`,
`libpng`, `move`, `screenshot`, `ulobjmgr`, `usbd`, `video_recording`, `videodec`, `vr_tracker`,
`zlib`.

**Already complete in AnyPS5**: `libSceMouse`, `libSceRandom`, `libSceRtc`, `libSceRudp`,
`libSceSaveData`, `libSceSysmodule`, `libSceVideoOut`.

Careful with the per directory numbers: `libSceX` and `libSceX.native` are counted separately and
the implementation usually lives in the `.native` one. `libSceAjm` reads 0/13 while
`libSceAjm.native` has 33 functions and decodes ATRAC9.

## 3b. Prosper, read but not copied

Prosper (`mattias800/prosper`) was cloned and read. Its README states the position plainly:
*"No LICENSE file exists yet, so no license is granted by default. Open an issue to ask."* That is
deliberate, not an oversight, so **no line of it is in this repository** and none can be until the
author grants a license. What follows is description, not code.

It matters more than the emulators because it is the only project that shares AnyPS5's shape: a
user-space compatibility layer, guest code running natively with no CPU emulation, SELF/ELF modules
linked into one address space, system libraries reimplemented on host facilities, AGC command
streams translated to Vulkan through an RDNA2 to SPIR-V recompiler.

Three differences worth knowing:

- **When the linking happens.** Prosper loads and links the guest modules at run time, in process,
  the way Wine does. AnyPS5 relinks the executable into a native binary beforehand. Same end,
  opposite moment, and it is why their loader code has no counterpart here.
- **How the HLE is organised.** Prosper groups by subsystem — `audio`, `fs`, `graphics`, `kernel`,
  `net`, `np`, `sync`, `video` — about fifty thousand lines in thirty-seven files. AnyPS5 groups by
  PS5 module name, one directory per `libSceX`. Theirs makes a subsystem easy to reason about;
  ours makes the guest ABI surface easy to audit, which is what the NID patcher needs.
- **Pluggable host backends.** Audio and video are frontends chosen at build time: `audio_sdl3`,
  `audio_ffmpeg`, `video_vaapi`, `video_mf`. That is a way to have FFmpeg-backed decoding without
  forcing the dependency on everyone, which is the exact obstacle that stopped `libSceAudiodec`
  here.

Their working practice is also worth stealing, and stealing a practice is free: a per title script
for each game they test, a tracker issue per title, a machine-generated progress file kept in step
by CI, and a charter that records the mistakes that cost the most time. AnyPS5 has a compatibility
list with one row and no reproducible route attached to it.

## 4. What transfers, in practice

Not files. These:

- **ABI facts**: structure layouts, error code values, the order of fields a title passes in.
  Getting these wrong corrupts the game's memory, and they are the hardest thing to recover alone.
- **Semantics and quirks**: for example KytyPS5 translates the ATRAC9 channel field when the
  configuration asks for a vibration layout, because LibAtrac9 does not know those layouts.
- **Which functions a real title actually calls**, which their compatibility lists record.

A port that uses those and writes its own code against AnyPS5's model is honest work and keeps the
provenance clean. Copying a file and adapting it until it compiles is neither.
