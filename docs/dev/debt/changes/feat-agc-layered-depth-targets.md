# libSceAgcDriver

## change [Layered color targets]

- [Layered color targets](../../../core/libs/prx/libSceAgcDriver/Graphics/src/State.cpp) (libSceAgcDriver) - 3D color targets with 64 KiB render-target or 4 KiB standard tiling render several slices. A CB_COLOR_VIEW slice range uses a 2D array's resident image, or expands a one-slice surface to the view's last slice + 1 slices. Layered draws require matching color layer counts; layered depth draws use a single-sample depth array image. Draws without a resident array image and mismatched counts throw. Vertex layer export applies only to layered draws; out-of-range layers are undefined in Vulkan and the console SLICE_MAX clamp is not modeled. Uniform DCC keys are interpreted across 3D slices; mixed keys preserve stored texels. The PS5 DCC layout for 3D is unverified (LEGO Horizon Adventures, PPSA14632, uses DCC).

## add Functional

- [Layered depth targets](../../../core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp) (libSceAgcDriver) - Layered draws use a DB_DEPTH_VIEW slice range matching the color layer count, or the view's slices without color, in a single-sample array image. Each layer retains depth, stencil and HTILE state; depth and stencil slice pitches span the mip chain, and HTILE slices span the sum of their mip metadata. Clears and plane copies are tracked per layer. Mismatched color/depth counts, multisampled layered depth and views without a depth plane throw. A later draw into one slice binds that layer. Out-of-range layer behavior is undefined in Vulkan; the DB SLICE_MAX clamp is not modeled.
