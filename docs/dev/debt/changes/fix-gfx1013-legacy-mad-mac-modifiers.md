# shader

## add Functional

- The [VOP3 result modifiers](../../../core/shader/recompiler/RdnaDecoder/src/RdnaVectorOpDecoder.cpp) on `v_mad_legacy_f32` and `v_mac_legacy_f32_e64` follow the f32 scale and clamp rules in the [AMD RDNA 1.0 ISA, section 6.3](https://www.amd.com/content/dam/amd/en/documents/radeon-tech-docs/instruction-set-architectures/rdna-shader-instruction-set-architecture.pdf). AMD COMGR assembles both encodings for gfx1013. The output-denormal enable bit and IEEE mode gate scaling independently of the legacy arithmetic's denormal flush. The modifier results, including the interaction with that flush, have been tested through Vulkan on AMD and NVIDIA and remain unmeasured on native gfx1013. Closes when: compared with native gfx1013.
