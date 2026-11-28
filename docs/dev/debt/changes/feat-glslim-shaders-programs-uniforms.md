# libSceGLSlimVSH

## change [host implementation]

- The [host implementation](../../../core/libs/prx/libSceGLSlimVSH/src/Egl.cpp) is not a conformant GLES2 renderer. The single pbuffer configuration advertises `EGL_NON_CONFORMANT_CONFIG` and no `EGL_CONFORMANT` bits. Native window/pixmap integration, OpenVG client-buffer import and rasterization are absent; GLSL ES 1.00 shader compilation, program linking, reflection, validation and uniform storage are backed by `glslang`. The 5 standard exports below still call `NotImplemented_nid_no_patch`:

  - `eglCreatePbufferFromClientBuffer`
  - `eglCreateWindowSurface`
  - `glDrawArrays`
  - `glDrawElements`
  - `glGetString`

## change [State queries]

- [State queries](../../../core/libs/prx/libSceGLSlimVSH/src/GlState.cpp): `glGetBooleanv`, `glGetFloatv` and `glGetIntegerv` advertise `GL_SHADER_COMPILER` as `GL_TRUE` and report `GL_CURRENT_PROGRAM` from the bound program object. Implemented state queries use host limits, not measured console limits. Calls requiring a GL context throw when none is current; GLES2 does not specify that case.

## change [Mipmap filtering]

- [Mipmap filtering](../../../core/libs/prx/libSceGLSlimVSH/src/Images.cpp) uses a CPU box filter; implementation-dependent rounding and dithering have not been compared with console output. The state/storage implementation does not establish rendering capability strings; `glGetString` remains a throwing export.

## add Functional

- [GLSL ES 1.00 compilation, linking, reflection and uniforms](../../../core/libs/prx/libSceGLSlimVSH/src/Shaders.cpp) use `glslang` against host `State.cpp` limits; diagnostics, uniform locations and automatic attribute slots are unmeasured on console. Overlapping active `glBindAttribLocation` slots always fail linking (stricter than ES 2.0.25 §2.10.4 conditional-path aliasing), built-in `gl_*` uniforms are omitted from `GL_ACTIVE_UNIFORMS`/`glGetActiveUniform`, and `glslang`'s built-in ES 1.00 extensions remain accepted at compile time even though no extensions are advertised (extension-only uniform types fail at link). `glGetShaderPrecisionFormat` reports host binary32 (`{127, 127}, 23`) and int32 (`{31, 30}, 0`) rather than measured console precision. `glShaderBinary` rejects all formats with `GL_INVALID_ENUM` and returns `GL_OUT_OF_MEMORY` (0x0505) for counts above 65536.
- [Shader, program and uniform guest pointers](../../../core/libs/prx/libSceGLSlimVSH/src/Shaders.cpp) copy through `GuestCopyIn_nid_no_patch`, `GuestCopyOut_nid_no_patch` and `CopyGuestString_nid_no_patch` while preserving `errno`. `glGetAttribLocation`, `glGetUniformLocation` and `glBindAttribLocation` cap copied names at 4096 bytes (longer mapped names return `-1`/no-op with no error unless prefixed with `gl_` in `glBindAttribLocation`; null/unmapped names set `GL_INVALID_VALUE` 0x0501 and host OOM sets `GL_OUT_OF_MEMORY` 0x0505). Unreadable or unwritable buffers in `glGetActiveAttrib`, `glGetActiveUniform`, `glGetShaderPrecisionFormat`, `glGetUniform*`, `glUniform*v`, `glUniformMatrix*fv` and `glShaderBinary` set `GL_INVALID_VALUE` (0x0501); multi-output queries stop on the first failed write, leaving earlier outputs written. `glGetShaderInfoLog`/`glGetProgramInfoLog` return compiler/linker logs via the same path. Console behavior for unmapped pointers is unmeasured.
