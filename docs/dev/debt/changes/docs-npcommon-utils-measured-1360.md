# libSceNpCommon

## change sceNpSetPlatformType, sceNpIntGetPlatformType, sceNpIntSetPlatformType, sceNpUtilCanonicalizeNpIdForPs4 and sceNpUtilCanonicalizeNpIdForPsp2

- [sceNpSetPlatformType, sceNpIntGetPlatformType, sceNpIntSetPlatformType, sceNpUtilCanonicalizeNpIdForPs4 and sceNpUtilCanonicalizeNpIdForPsp2](../../../core/libs/prx/libSceNpCommon/Export.cpp) (libSceNpCommon) - read from `libSceNpCommon.sprx` of firmware 13.60 disassembly and confirmed in a PS5 Pro payload process: the tag is written by `sceNpSetPlatformType` at npid offset 24 (`opt[4..7]`) and read back from there, types 0 to 3 are no tag, `ps3`, `psp2` and `ps4`, type 4 is refused publicly with `0x80550004` but accepted by `sceNpIntSetPlatformType` as `comp`, which `sceNpGetPlatformType` reports as `0x80550004` while `sceNpIntGetPlatformType` returns 4. The canonicalize calls clear the tag first when their second argument is non-zero (a `comp` tag becomes `ps4`), return `0x80550605` (no name in any source) for an npid whose `reserved[0]` is not 1 without writing, and only then fill an empty tag.

## change [sceNpUtilParseJid and sceNpUtilJidToNpId]

- [sceNpUtilParseJid and sceNpUtilJidToNpId](../../../core/libs/prx/libSceNpCommon/Strings.cpp) (libSceNpCommon) - read from `libSceNpCommon.sprx` of firmware 13.60 disassembly and confirmed in a PS5 Pro payload process, with two quirks a grammar would not have: the parser skips exactly one character after the domain (its trimmed trailing dots, a space or `/`) before the resource (`...net../res` yields `./res`), and keeps everything after that without checking it; text with no `@` parses as empty user plus whole-text domain, a too-small buffer gives `0x80550602`, and `JidToNpId` fills online ID 0, label pair 20, resource 24 and marker 1 at 28, as this module does. A domain is taken as `[A-Za-z0-9._-]+` and a user as `[A-Za-z0-9+._-]+` before `@` (C-locale alnum). `sceNpUtilJidToNpId` accepts only `XX.YY.<label>.playstation.net` domains, users of 1 to 16 bytes and resources of up to 4. The meaning of `0x80550604`, returned for every rejected JID, is not named in any source

## change [sceNpGetSdkVersion]

- [sceNpGetSdkVersion](../../../core/libs/prx/libSceNpCommon/Export.cpp) (libSceNpCommon) - clears the 8-byte buffer and then writes the libkernel system software version into it as `major.minor` without checking the pointer, as firmware 13.42's `libSceNpCommon.sprx` does ([read from its disassembly and an emulator run](https://github.com/boykopovar/AnyPS5/pull/13684#issuecomment-6537744884) and measured in a PS5 Pro payload process); it is not fixed: the console answered `13.59` there, where its own `sceKernelGetSystemSwVersion` reports string `13.590.001` and value `0x13590001` on that firmware-13.60 machine. It is not the SDK version a title was built with; AnyPS5 answers whatever its own libkernel reports.

## change [sceNpGetNavSdkVersion]

- [sceNpGetNavSdkVersion](../../../core/libs/prx/libSceNpCommon/Export.cpp) (libSceNpCommon) - writes the fixed string `00.82` padded with zeros into an 8-byte buffer without checking it and returns nothing, as `libSceNpCommon.sprx` of firmware 13.60 does, read from disassembly and confirmed in a PS5 Pro payload process (firmware 13.60), which wrote exactly `00.82` plus zeros; other firmware versions may carry another string.
