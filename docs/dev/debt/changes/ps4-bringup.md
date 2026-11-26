# libc

## add Unknown function info

- [_ZSt22_Random_device_entropyv](../../../core/libs/prx/libc/src/Random.cpp) (libc) - returns 32.0, the entropy of one 32-bit `random_device` result as this library's `operator()` produces from host entropy; follows the documented `random_device::entropy` behaviour (MicrosoftDocs `random-device-class`, cppreference `random_device::entropy`) and not a copied implementation. Not measured on a console. Closes when: measured on a console.

# libSceGnmDriver

## add Unknown function info

- [Validate queries](../../../core/libs/prx/libSceGnmDriver/Export.cpp) (libSceGnmDriver, PS4) - the nine validation entry points answer as the Neo provider does (validation not enabled; version and on-submit checks 0), which the console's regular module must also export: Heretic + Hexen (CUSA51664) imports all nine from `libSceGnmDriver` while AnyPS5 only provided them in `libSceGnmDriverForNeoMode`. Not measured on a console. Complements #27342 (shared compute waits and GPU queries). Closes when: measured on a console.

# libSceNgs2.native

## change [NGS2 rack block limit]

- [NGS2 rack block limit](../../../core/libs/prx/libSceNgs2.native/src/Rack.cpp) (libSceNgs2.native) - the PS4 library applies no limit to sampler racks and keeps only the check that a custom sampler rack with 0 blocks returns `0x804A8053`. A 168-byte PS4 sampler option is accepted: its first 128 bytes map onto the common fields (size, 16-byte name, flags, grain, voices, delay, matrices, ports) and its last 40 bytes onto the first nine sampler fields, as Heretic + Hexen (CUSA51664) passes them; the 10th trailing word and the base tail past the ports are ignored. Other rack types still require the PS5 sizes. PS4 11.00 takes sampler racks with `max_waveform_blocks` 1 to 16 and returns `0x804A0106` otherwise; the order of that range check against the voice and channel count checks was not run; here it comes first. Closes when: the ignored bytes and the other racks' PS4 layouts are measured.
