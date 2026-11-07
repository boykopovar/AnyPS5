# libSceGLSlimVSH technical debt

### Unknown function info

- The 22 Sony-specific exports in [Export.cpp](../../../core/libs/prx/libSceGLSlimVSH/Export.cpp) have names recovered by `tools/nid_names.py` from its pinned [NID database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv). Each name hashes to the original explicit NID. Their prototypes and behavior remain unverified; all retain no-parameter throwing placeholders. No recovered name is a standard EGL or GLES2 function.

| Name | NID |
| --- | --- |
| `glOrbisMapTextureResourceSCE` | `iIsF4NzYgD4` |
| `glOrbisTexImageCanvas2DSCE` | `hA8TWZkwPRU` |
| `glOrbisTexImageResourceSCE` | `LuP1plTAR7g` |
| `glOrbisUnmapTextureResourceSCE` | `J5IG3lL56FI` |
| `scePigletAllocateSystemMemoryEx` | `oXkQEVitkCs` |
| `scePigletAllocateVideoMemoryEx` | `sDotUt1uEhA` |
| `scePigletGetConfigurationVSH` | `3gtNzvkq-XY` |
| `scePigletReleaseSystemMemoryEx` | `T-+xooU3VvQ` |
| `scePigletReleaseVideoMemoryEx` | `AcruVcmKz78` |
| `sceSlimglClientStartIPC` | `QvXbiLqpn6c` |
| `sceSlimglCompositorAllocateIndex` | `mgKTS0PIvd8` |
| `sceSlimglCompositorCreateIndirectRenderTarget` | `RP5cXUjm48U` |
| `sceSlimglCompositorDeleteIndirectRenderTarget` | `srJHx3p48v0` |
| `sceSlimglCompositorFlush` | `6ZQXcSKKjyo` |
| `sceSlimglCompositorFlushWithRepeat` | `wdRBDMIxI7I` |
| `sceSlimglCompositorGetCanvasHandle` | `CzmA-OALrH8` |
| `sceSlimglCompositorGetError` | `aRqvgeH+oe0` |
| `sceSlimglCompositorReleaseIndex` | `gI+XEd5mWPU` |
| `sceSlimglCompositorSetEndOfFrameCommand` | `1TFdFqQBGqY` |
| `sceSlimglCompositorSetIndirectCanvasCommand` | `7Mp6qE235Qc` |
| `sceSlimglCompositorSetIndirectCompositionCommand` | `BrNgn7z6A40` |
| `sceSlimglCompositorSetIndirectRenderTargetConfigCommand` | `iyTnezpw1jE` |

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceGLSlimVSH/Export.cpp) - 7 callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceGLSlimVSH.c) and the [selected export metadata](https://gist.github.com/kyon-eth/e1b845e2be0047e065a7dbb2c2e92322/7a180a59bf97bf6ea42020d4eedf9a8d631b0fd3) from two version-labelled images (SHA-256 `5d1eb4913d2914be044c05a48c4287a3a73e526605f21ec37ce9acfb5b2170ad`). The labels are not authenticated. Prototypes and behavior are unverified; these declarations throw through `NotImplemented_nid_no_patch` and implement no runtime functionality.

| Name | NID |
| --- | --- |
| `sceSlimglCompositorSetInvisibleCanvasCommand` | `DN8v2xNOyjg` |
| `sceSlimglCompositorSetMemoryCommand` | `aswH9c+RN0E` |
| `sceSlimglCompositorSetPostEventCommand` | `WvuRaahcTfA` |
| `sceSlimglCompositorWaitEndOfRendering` | `lcaik1AxwEY` |
| `sceSlimglCompositorWaitPostEvent` | `HwBlC8TLKHQ` |
| `sceSlimglIPCEnd` | `Fa0atOcWPaI` |
| `sceSlimglIPCSetup` | `F4pQ5tsDo74` |

- [`scePigletSetConfigurationVSH`](../../../core/libs/prx/libSceGLSlimVSH/Export.cpp) (`M9RtXpjSYtE`) is listed in the same 3.20 catalogue and is declared as a no-parameter throwing placeholder; its prototype and behavior are unverified.

### Additional unidentified exports

- Seven [callable exports](../../../core/libs/prx/libSceGLSlimVSH/Export.cpp) are declared as throwing placeholders. Their names, signatures and behavior are unverified. The same module and library identities occur in two version-labelled images; those labels are not authenticated.

| Name | NID |
| --- | --- |
| `Unknown` | `9iW854bwvmc` |
| `Unknown` | `DVNKmWX1jOU` |
| `Unknown` | `JjyNMbSAgLk` |
| `Unknown` | `LLDLZ-mQK+Q` |
| `Unknown` | `Yf0LATiXPsg` |
| `Unknown` | `ZLvm5korFRA` |
| `Unknown` | `qSuVNKHENqs` |

### Functional

- The [host implementation](../../../core/libs/prx/libSceGLSlimVSH/src/Egl.cpp) is not a conformant GLES2 renderer. The single pbuffer configuration advertises `EGL_NON_CONFORMANT_CONFIG` and no `EGL_CONFORMANT` bits. Native window/pixmap integration, OpenVG client-buffer import, GLSL ES compilation, program linking/reflection and rasterization are absent. The 35 standard exports below still call `NotImplemented_nid_no_patch`; no shader or program can acquire a successful compile/link status. Uniform locations and storage require linked shader reflection and remain unavailable.

  - `eglCreatePbufferFromClientBuffer`
  - `glCompileShader`
  - `glDrawElements`
  - `glGetShaderPrecisionFormat`
  - `glGetUniformfv`
  - `glGetUniformiv`
  - `glUniform1fv`
  - `glUniform1iv`
  - `glUniform2f`
  - `glUniform2fv`
  - `glUniform2i`
  - `glUniform2iv`
  - `glUniform3f`
  - `glUniform3fv`
  - `glUniform3i`
  - `glUniform3iv`
  - `glUniform4f`
  - `glUniform4fv`
  - `glUniform4i`
  - `glUniform4iv`
  - `glUniformMatrix2fv`
  - `glUniformMatrix3fv`
  - `glValidateProgram`
  - `glDrawArrays`
  - `glLinkProgram`
  - `glShaderBinary`
  - `glGetString`
  - `glGetAttribLocation`
  - `glGetUniformLocation`
  - `glGetActiveAttrib`
  - `glGetActiveUniform`
  - `glUniform1f`
  - `glUniform1i`
  - `glUniformMatrix4fv`
  - `eglCreateWindowSurface`

- [State queries](../../../core/libs/prx/libSceGLSlimVSH/src/GlState.cpp): `glGetBooleanv`, `glGetFloatv` and `glGetIntegerv` throw for `GL_SHADER_COMPILER`; compiler availability cannot be advertised until a compiler is integrated. Other implemented state queries use host limits, not measured console limits. Calls requiring a GL context throw when none is current; GLES2 does not specify that case.
- [Console-specific ABI](../../../core/libs/prx/libSceGLSlimVSH/include/Api.hpp) behavior and implementation-dependent limits are unmeasured. The host contracts come from [EGL 1.4](https://registry.khronos.org/EGL/specs/eglspec.1.4.pdf) sections 3.1-3.8 and [OpenGL ES 2.0.25](https://registry.khronos.org/OpenGL/specs/es/2.0/es_full_spec_2.0.pdf) sections 2.7-2.10, 3.7, 4 and 6. The backend has one default display, ES2 contexts, a single RGBA8/depth16/stencil8 pbuffer configuration, 2048-pixel image limits and eight vertex attributes and texture units. It supports no compressed texture formats, shader binary formats, EGL texture-binding configurations or extensions. Native-display handles other than the default do not resolve.
- [Mipmap filtering](../../../core/libs/prx/libSceGLSlimVSH/src/Images.cpp) uses a CPU box filter; implementation-dependent rounding and dithering have not been compared with console output. The state/storage implementation does not establish shader execution precision or rendering capabilities; `glGetString` and `glGetShaderPrecisionFormat` remain throwing exports.
