# shader

## change [`s_memtime`]

- [`s_memtime`](../../../core/shader/recompiler/SpirvBackend/src/SpirvModuleEmitter.cpp) counts the 100 MHz Vulkan device clock instead of shader cycles where the subgroup clock is the 20-bit `SHADER_CYCLES` register, which wraps every 2^20 cycles, so its deltas are 22.35 times smaller than on a PS5, where it counts the ~2.23 GHz core clock (measured 2026-10-19 by a gfx1013 kernel run inside a retail title). [`NarrowSubgroupClock`](../../../core/libs/prx/libSceAgcDriver/Execution/src/SubgroupClock.cpp) selects it on RADV by the GFX10.3 to GFX11.7 family name and on AMD's Windows driver by device: 0x1114 (Radeon 860M, #1615), 0x744c (RX 7900 XT, #1632), 0x747e (RX 7800 XT, #1632) and 0x7480 (RX 7600 XT, wrapped in 34 of 50 `agc_driver_shader_clock` runs, 2026-11-20). A Mesa family or Windows device not listed keeps the wrapping clock.
