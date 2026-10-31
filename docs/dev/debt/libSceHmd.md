# libSceHmd technical debt

### Silent stubs

- [libSceHmd](../../../core/libs/prx/libSceHmd/Export.cpp) implements only the disconnected-headset path: initialization succeeds, device queries report `NotDetected`, and opening a device returns `DeviceDisconnected`. Headset support, tracking, rendering and additional HMD exports are not implemented; SDK-level ABI compatibility and in-game behaviour remain unverified.

### Unknown function info

- [libSceHmd internal and capture exports](../../../core/libs/prx/libSceHmd/Export.cpp) - input/output directions and argument registers are recovered for shared VR2D data, device binding, optical and serial queries, screen status and capture operations. VR2D records are 128 bytes, optical output 16, serial output 16, PU position output 20, crash-dump output 68, audio/video status 32, and reprojection debug outputs 216 or 1136. Payload field meanings and capture callback contracts remain incomplete; these structures use forward declarations.

- [libSceHmd queries without a headset](../../../core/libs/prx/libSceHmd/Export.cpp) (libSceHmd) - the signatures, the distortion work memory (`0x20000`, aligned to `0x400`) and reprojection buffer sizes (garlic `0x100000`, onion `0x810`, both aligned to `0x100`), and the results of `sceHmdGetFieldOfView`, `sceHmdGet2DEyeOffset`, `sceHmdGetInertialSensorData` (`HANDLE_INVALID` once initialized; field of view checks its output for null first) and `sceHmdGetAssyError` (null output, then `DEVICE_DISCONNECTED` once initialized) follow [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/a808e76/src/core/libraries/hmd/hmd.cpp) (PS4) with no headset, not a PS5 measurement. Before initialization `sceHmdGetFieldOfView` returns `NOT_INITIALIZED` for a non-null output, where shadPS4 checks the handle first and returns `HANDLE_INVALID`. The third argument of `sceHmdGetInertialSensorData` is unnamed there and taken as a count
- [libSceHmd VR2D queries](../../../core/libs/prx/libSceHmd/Export.cpp) (libSceHmd) - `sceHmdInternalIsGameVr2d`, `sceHmdInternalIsMiniAppVr2d`, `sceHmdInternalIsCommonDlgVr2d` and `sceHmdInternalIsCommonDlgMiniAppVr2d` return false: nothing runs in VR2D without a headset, and the console answers 0 to the same four queries in libSceHmd2 (`Hmd2BeforeInitializeZero` in [libSceHmd2](libSceHmd2.md)). The libSceHmd answers themselves are not measured.
- [sceHmdDistortionGetCompoundEyeCorrectionCommand](../../../core/libs/prx/libSceHmd/Export.cpp) returns a constant error without consuming arguments in the supplied module; unused argument types remain unresolved.
- [sceHmdInternalGetDefaultLedData](../../../core/libs/prx/libSceHmd/Export.cpp) takes an unused handle, a 32-bit mask output and a nine-byte LED output. Output sizes match the paired setter; mask bits and LED payload semantics remain unresolved. The protected body is unchanged.

[Device, field-of-view, assembly-error, distortion, sensor and DFU query signatures](../../../core/libs/prx/libSceHmd/Export.cpp) are recovered from native register use. Field of view and assembly errors each output four floats; device information occupies 64 bytes and distortion output spans 76 bytes. The remaining device/sensor/report field semantics and DFU status bit meanings are unresolved. Assembly confirms integer error returns where decompilation incorrectly inferred void. The internal device-information, distortion-parameter, status-report and DFU-status queries throw.

[Reprojection initialization and display-buffer signatures](../../../core/libs/prx/libSceHmd/Export.cpp) are recovered: initialization takes a parameter pointer, 32-bit mode and null reserved pointer; display-buffer setup takes a VideoOut handle, two buffer indices and reserved pointer, while adding a buffer takes an index and reserved pointer. Parameter fields remain incomplete. Protected bodies are unchanged.

[Work-memory queries, capture start, separate-mode open and crash-report cancellation](../../../core/libs/prx/libSceHmd/Export.cpp) consume no arguments in the supplied native implementation. The mirroring aspect debug setter takes five 16-bit values followed by a byte mode; the individual scalar meanings remain unresolved. These argument lists are confirmed; capture start, separate-mode open, crash-report cancellation and the mirroring aspect debug setter throw.

[Reprojection start signatures](../../../core/libs/prx/libSceHmd/Export.cpp) are recovered, including the five arguments of `StartMultilayer2` hidden by an incomplete decompilation. Standard and wide-near starts take frame parameters, pose, 64-bit flip argument and reserved pointer; overlay variants add an overlay descriptor. The 2D variant takes parameters, flip argument and reserved pointer. Multilayer takes a layer array/count, submission parameters, pose, flip argument and reserved pointer. Frame, pose, overlay and submission field semantics remain incomplete. Protected bodies are unchanged.

[Distortion and 2D VR command queries](../../../core/libs/prx/libSceHmd/Export.cpp) take a 32-bit HMD handle, input descriptor and command-buffer output. The output contains a command pointer at offset zero and a 32-bit dword count at offset eight. Descriptor fields and GPU resource types remain incomplete; the standard, wide-near and 2D descriptors are distinct. Without a headset `sceHmdOpen` never issues a handle, so the stored handle stays 0. As firmware 13.42's libSceHmd does when its exports are run under an emulator ([#9577](https://github.com/boykopovar/AnyPS5/pull/9577#issuecomment-6336871370)), the distortion queries return `HANDLE_INVALID` for any other handle before checking initialization, and `NOT_INITIALIZED` for handle 0 before it when the title's compiled SDK version is below 4.50. The 2D VR query first returns `PARAMETER_NULL` for a null descriptor or output or a null pointer at descriptor offset 0, 8 or 0x20, and `PARAMETER_INVALID` for a non-zero field at 0x34 (32-bit) or 0x38 (64-bit). None writes the output. From SDK 4.50 the firmware first checks state the library sets up when it starts, which is not measured ([#9577](https://github.com/boykopovar/AnyPS5/pull/9577#issuecomment-6343495321)), and what handle 0 gets after initialization is not measured either, so both throw.

The ELF export table aliases the [Distortion correction/2D command and work-memory queries](../../../core/libs/prx/libSceHmd/Export.cpp) to their corresponding HMD queries, confirming identical signatures. Distortion initialization consumes only a work-memory pointer. No shader code or calibration constants are copied; each aliased command query answers as its twin, and distortion initialization keeps its protected body.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceHmd/Export.cpp) - 19 entries observed in a 3.20-labelled image and a later 13.60-labelled image are import-resolution declarations only. Neither image label authenticates a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

[Version-pinned selected ELF export records](https://gist.github.com/kyon-eth/3fbb1a4a6f95a936b4cd58e9c3d8c50c/5e3af709224ed8235af11337309af0725e5685cb) list every NID below with its image label, decoded export module and library, raw qualified symbol, ELF function type, value and size for each inspected version-labelled image. The extract SHA-256 is `02ab27f12bcd0cc9fa03fa33f55fc201110f0d54a86bbbaa093d55d0a8cf262d`; its full-module sorted-function-NID digests and raw module-parameter words permit independent comparison. This is an analyst-produced metadata record, not independent authentication of the original images or proof of prototypes or behavior.

| Name | NID |
| --- | --- |
| `Unknown` | `-Bk71lPyry4` |
| `Unknown` | `-y4OUwFf4jE` |
| `Unknown` | `IC0NGmh-zS8` |
| `Unknown` | `NY2-gYo9ihI` |
| `Unknown` | `XMutp2-o9A4` |
| `Unknown` | `Y9QDFn3AjPA` |
| `Unknown` | `aTg7K0466r8` |
| `Unknown` | `eRVgwy9PbWg` |
| `Unknown` | `mVIneDkja6c` |
| `Unknown` | `miduc55U7q8` |
| `Unknown` | `nlAZlOKJy+c` |
| `Unknown` | `ox9NqLO9LhI` |
| `Unknown` | `oxoDINgOrZk` |
| `Unknown` | `qS18I6w2SZM` |
| `Unknown` | `rczCXLh2-b4` |
| `Unknown` | `sWZSZB-mnw4` |
| `Unknown` | `smQw6nT8PcA` |
| `Unknown` | `thTykLZ-tZs` |
| `Unknown` | `uab6BzXsfkk` |
