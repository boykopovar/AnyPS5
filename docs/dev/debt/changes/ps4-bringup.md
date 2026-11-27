# libc

## add Unknown function info

- [_ZSt22_Random_device_entropyv](../../../core/libs/prx/libc/src/Random.cpp) (libc) - returns 32.0, the entropy of one 32-bit `random_device` result as this library's `operator()` produces from host entropy; follows the documented `random_device::entropy` behaviour (MicrosoftDocs `random-device-class`, cppreference `random_device::entropy`) and not a copied implementation. Not measured on a console. Closes when: measured on a console.

# libSceGnmDriver

## add Unknown function info

- [Validate queries](../../../core/libs/prx/libSceGnmDriver/Export.cpp) (libSceGnmDriver, PS4) - the nine validation entry points answer as the Neo provider does (validation not enabled; version and on-submit checks 0), which the console's regular module must also export: Heretic + Hexen (CUSA51664) imports all nine from `libSceGnmDriver` while AnyPS5 only provided them in `libSceGnmDriverForNeoMode`. Not measured on a console. Complements #27342 (shared compute waits and GPU queries). Closes when: measured on a console.

- [Default context state](../../../core/libs/prx/libSceGnmDriver/Export.cpp) (libSceGnmDriver, PS4) - the normal provider answers `sceGnmDrawInitToDefaultContextState` for the 0x20-dword packet as the Neo provider does on Neo hardware and with a base table on base hardware, which the console's regular module must also export: Battlefield 1 (CUSA02429) imports it from `libSceGnmDriver` while AnyPS5 only provided it in `libSceGnmDriverForNeoMode`. The base table drops the Neo VGT-control words for the base `0x2aa`/`0xff` pair and grows the final span by one word to keep exact termination, as the 400-word base table does; the exact console bytes are unmeasured. Closes when: measured on a console.

# libSceAgcDriver

## add Unknown function info

- [SET_CONFIG_REG](../../../core/libs/prx/libSceAgcDriver/Execution/src/Pm4.cpp) (libSceAgcDriver) - config register writes validate like the other register packets and are recorded in a config bank without interpretation; reads of config registers still throw as outside the modeled banks. Heretic + Hexen (CUSA51664) writes 0 to config register 0x8102 twice per submission and never reads it back; what that register selects is unmeasured, as is whether config writes may appear on compute queues (they are refused there). Closes when: the written registers' effects are measured.

- [INDEX_TYPE modifiers](../../../core/libs/prx/libSceAgcDriver/Execution/src/Pm4.cpp) (libSceAgcDriver) - the packet's low two bits select the index type, as the uconfig index-type path and shadPS4's decode do; the byte-swap bits 3:2 are rejected, and other high bits are ignored. Heretic + Hexen (CUSA51664) sends 0x400 (16-bit with bit 10 set); what that bit selects is unmeasured. Closes when: the modifier bits are measured.

# libSceNpToolkit

## add Unknown function info

- [NpToolkit Future](../../../core/libs/prx/libSceNpToolkit/Export.cpp) (libSceNpToolkit) - the four `sce::Toolkit::NP::Utilities` Future methods (`Future<NpUser>::get`, `~Future`, `FutureImpl::hasError`, `FutureImpl::hasResult`) throw as unimplemented, which Battlefield 1 (CUSA02429) imports from `libSceNpToolkit.prx` with no bundled provider; the class layout, call semantics and return values are unmeasured, as are the placeholder signatures. Closes when: measured on a console.

# libJavaScriptCore

## add Unknown function info

- [JavaScriptCore gaps](../../../core/libs/prx/libJavaScriptCore/Export.cpp) (libJavaScriptCore) - four imports that Battlefield 1 (CUSA02429) names from `libJavaScriptCore.prx` have no provider in the host libraries or the title's bundled `JavaScriptCore.prx` and no aerolib name; each throws as unimplemented. The other imports from that library resolve on Linux through the bundled module under a provider mismatch. The functions' identities, signatures and behavior are unmeasured, as are the placeholder signatures. Closes when: measured on a console.

# libSceNgs2.native

## change [NGS2 rack block limit]

- [NGS2 rack block limit](../../../core/libs/prx/libSceNgs2.native/src/Rack.cpp) (libSceNgs2.native) - the PS4 library applies no limit to sampler racks and keeps only the check that a custom sampler rack with 0 blocks returns `0x804A8053`. PS4 sampler, mastering and reverb options are accepted in their 168-, 136- and 136-byte PS4 forms: the first 128 bytes map onto the common fields (size, 16-byte name, flags, grain, voices, delay, matrices, ports) and the remainder onto the rack-specific fields, as Heretic + Hexen (CUSA51664) passes them; the 10th sampler word and the base tail past the ports are ignored. Other rack types still require the PS5 sizes. PS4 11.00 takes sampler racks with `max_waveform_blocks` 1 to 16 and returns `0x804A0106` otherwise; the order of that range check against the voice and channel count checks was not run; here it comes first. Closes when: the ignored bytes and the other racks' PS4 layouts are measured.

## add Functional

- [PS4 voice event 0](../../../core/libs/prx/libSceNgs2.native/src/Voice.cpp) (libSceNgs2.native) - the PS4 library ignores a voice event id of 0, which Heretic + Hexen (CUSA51664) sends to an empty mastering voice during setup; the native library still throws. Whether the console ignores 0 in other voice states, and whether it answers an error instead, is unmeasured. Closes when: measured on a console.

- [PS4 combined voice events](../../../core/libs/prx/libSceNgs2.native/src/Voice.cpp) (libSceNgs2.native) - the PS4 library applies each known event bit in ascending numeric order, returning `0x804A8351` before any state change when the PLAY bit is present without channels, as Heretic + Hexen (CUSA51664) sends PLAY|STOP to an empty sampler voice; the native library still throws. Deferred masks publish KILL-then-PLAY pending bookkeeping to match that order. Whether the console accepts combined bits, and the bit order, are unmeasured. Closes when: measured on a console.

- [PS4 sampler voice state](../../../core/libs/prx/libSceNgs2.native/src/Voice.cpp) (libSceNgs2.native) - the PS4 library answers the 48-byte sampler state without the 4-byte error code, translating flags, heights, decoded counts, user data and waveform pointer, as Heretic + Hexen (CUSA51664) polls it; other sizes still require the PS5 56-byte form. Whether the console's 48-byte layout matches this translation is unmeasured. Closes when: measured on a console.

# libkernel

## add Unknown function info

- [MapDirectMemory alignment](../../../core/libs/prx/libkernel/DirectMemory/Export.cpp) (libkernel) - `sceKernelMapDirectMemory` normalizes the alignment to the page size for fixed placements and passes it through otherwise, as its wrapper already ignored the parameter while the validating wrappers check it: Battlefield 1 (CUSA02429) maps a fixed page-aligned 128KB-offset range with a 2MB alignment. Whether the console ignores alignment for fixed placements, and what `sceKernelMapDirectMemory2` and the named variant answer there, is unmeasured. Closes when: measured on a console.
