.amdgcn_target "amdgcn-amd-amdhsa--@TARGET@"
.text
.globl probe
.p2align 8
.type probe,@function
probe:
  s_load_dwordx4 s[4:7], s[0:1], 0x0
  v_lshlrev_b32 v1, 4, v0
  v_lshlrev_b32 v2, 6, v0
  s_waitcnt lgkmcnt(0)
  global_load_dwordx4 v[4:7], v1, s[4:5]
  s_waitcnt vmcnt(0)
  v_mov_b32 v10, 0
  v_mov_b32 v11, 0
  v_mov_b32 v12, 0
  v_mov_b32 v13, 0
  v_mov_b32 v14, 0
  v_mov_b32 v15, 0
  v_mov_b32 v16, 0
  v_mov_b32 v17, 0
  v_mov_b32 v18, 0
  v_mov_b32 v19, 0
  v_mov_b32 v20, 0
  v_mov_b32 v21, 0
  v_mov_b32 v22, 0
  v_mov_b32 v23, 0
  v_mov_b32 v24, 0
  v_mov_b32 v25, 0
@BODY@
  global_store_dwordx4 v2, v[10:13], s[6:7]
  global_store_dwordx4 v2, v[14:17], s[6:7] offset:16
  global_store_dwordx4 v2, v[18:21], s[6:7] offset:32
  global_store_dwordx4 v2, v[22:25], s[6:7] offset:48
  s_endpgm
.Lend:
.size probe, .Lend-probe

.rodata
.p2align 6
.amdhsa_kernel probe
  .amdhsa_user_sgpr_kernarg_segment_ptr 1
  .amdhsa_kernarg_size 16
  .amdhsa_group_segment_fixed_size @LDS@
  .amdhsa_next_free_vgpr 128
  .amdhsa_next_free_sgpr 48
  .amdhsa_wavefront_size32 @WAVE32@
  .amdhsa_float_denorm_mode_32 @DENORM@
  .amdhsa_float_denorm_mode_16_64 @DENORM16@
  .amdhsa_ieee_mode @IEEE@
  .amdhsa_dx10_clamp @DX10_CLAMP@
  .amdhsa_float_round_mode_32 @ROUND32@
  .amdhsa_float_round_mode_16_64 @ROUND16@
  .amdhsa_fp16_overflow @FP16_OVERFLOW@
.end_amdhsa_kernel

.amdgpu_metadata
---
amdhsa.version: [ 1, 2 ]
amdhsa.kernels:
  - .name: probe
    .symbol: probe.kd
    .kernarg_segment_size: 16
    .group_segment_fixed_size: @LDS@
    .private_segment_fixed_size: 0
    .kernarg_segment_align: 8
    .wavefront_size: @WAVESIZE@
    .sgpr_count: 48
    .vgpr_count: 64
    .max_flat_workgroup_size: 1024
    .args:
      - { .size: 8, .offset: 0, .value_kind: global_buffer, .address_space: global }
      - { .size: 8, .offset: 8, .value_kind: global_buffer, .address_space: global }
...
.end_amdgpu_metadata
