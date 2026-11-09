/* Generated automatically by the program `genopinit'
   from the machine description file `md'.  */

#ifndef GCC_INSN_OPINIT_H
#define GCC_INSN_OPINIT_H 1
enum optab_tag {
  unknown_optab,
  sext_optab,
  trunc_optab,
  zext_optab,
  sfix_optab,
  ufix_optab,
  sfloat_optab,
  ufloat_optab,
  lrint_optab,
  lround_optab,
  lfloor_optab,
  lceil_optab,
  fract_optab,
  fractuns_optab,
  satfract_optab,
  satfractuns_optab,
  ustrunc_optab,
  sstrunc_optab,
  sfixtrunc_optab,
  ufixtrunc_optab,
  smul_widen_optab,
  umul_widen_optab,
  usmul_widen_optab,
  smadd_widen_optab,
  umadd_widen_optab,
  ssmadd_widen_optab,
  usmadd_widen_optab,
  smsub_widen_optab,
  umsub_widen_optab,
  ssmsub_widen_optab,
  usmsub_widen_optab,
  crc_optab,
  crc_rev_optab,
  vec_load_lanes_optab,
  vec_store_lanes_optab,
  vec_mask_load_lanes_optab,
  vec_mask_store_lanes_optab,
  vec_mask_len_load_lanes_optab,
  vec_mask_len_store_lanes_optab,
  vcond_mask_optab,
  vec_cmp_optab,
  vec_cmpu_optab,
  vec_cmpeq_optab,
  maskload_optab,
  maskstore_optab,
  mask_len_load_optab,
  mask_len_store_optab,
  gather_load_optab,
  mask_gather_load_optab,
  mask_len_gather_load_optab,
  scatter_store_optab,
  mask_scatter_store_optab,
  mask_len_scatter_store_optab,
  vec_extract_optab,
  vec_init_optab,
  sdot_prod_optab,
  udot_prod_optab,
  usdot_prod_optab,
  while_ult_optab,
  add_optab,
  addv_optab,
  ssadd_optab,
  usadd_optab,
  sub_optab,
  subv_optab,
  sssub_optab,
  ussub_optab,
  smul_optab,
  smulv_optab,
  ssmul_optab,
  usmul_optab,
  sdiv_optab,
  sdivv_optab,
  ssdiv_optab,
  udiv_optab,
  usdiv_optab,
  sdivmod_optab,
  udivmod_optab,
  smod_optab,
  umod_optab,
  ftrunc_optab,
  and_optab,
  ior_optab,
  xor_optab,
  ashl_optab,
  ssashl_optab,
  usashl_optab,
  ashr_optab,
  lshr_optab,
  rotl_optab,
  rotr_optab,
  vashl_optab,
  vashr_optab,
  vlshr_optab,
  vrotl_optab,
  vrotr_optab,
  smin_optab,
  smax_optab,
  umin_optab,
  umax_optab,
  neg_optab,
  negv_optab,
  ssneg_optab,
  usneg_optab,
  abs_optab,
  absv_optab,
  one_cmpl_optab,
  bswap_optab,
  ffs_optab,
  clz_optab,
  ctz_optab,
  clrsb_optab,
  popcount_optab,
  parity_optab,
  cmp_optab,
  ucmp_optab,
  eq_optab,
  ne_optab,
  gt_optab,
  ge_optab,
  lt_optab,
  le_optab,
  unord_optab,
  powi_optab,
  sqrt_optab,
  sync_old_add_optab,
  sync_old_sub_optab,
  sync_old_ior_optab,
  sync_old_and_optab,
  sync_old_xor_optab,
  sync_old_nand_optab,
  sync_new_add_optab,
  sync_new_sub_optab,
  sync_new_ior_optab,
  sync_new_and_optab,
  sync_new_xor_optab,
  sync_new_nand_optab,
  sync_compare_and_swap_optab,
  sync_lock_test_and_set_optab,
  mov_optab,
  movstrict_optab,
  movmisalign_optab,
  storent_optab,
  insv_optab,
  extv_optab,
  extzv_optab,
  insvmisalign_optab,
  extvmisalign_optab,
  extzvmisalign_optab,
  push_optab,
  reload_in_optab,
  reload_out_optab,
  cbranch_optab,
  tbranch_eq_optab,
  tbranch_ne_optab,
  addcc_optab,
  negcc_optab,
  notcc_optab,
  movcc_optab,
  cond_add_optab,
  cond_sub_optab,
  cond_smul_optab,
  cond_sdiv_optab,
  cond_smod_optab,
  cond_udiv_optab,
  cond_umod_optab,
  cond_and_optab,
  cond_ior_optab,
  cond_xor_optab,
  cond_ashl_optab,
  cond_ashr_optab,
  cond_lshr_optab,
  cond_smin_optab,
  cond_smax_optab,
  cond_umin_optab,
  cond_umax_optab,
  cond_copysign_optab,
  cond_fmin_optab,
  cond_fmax_optab,
  cond_fma_optab,
  cond_fms_optab,
  cond_fnma_optab,
  cond_fnms_optab,
  cond_neg_optab,
  cond_one_cmpl_optab,
  cond_len_add_optab,
  cond_len_sub_optab,
  cond_len_smul_optab,
  cond_len_sdiv_optab,
  cond_len_smod_optab,
  cond_len_udiv_optab,
  cond_len_umod_optab,
  cond_len_and_optab,
  cond_len_ior_optab,
  cond_len_xor_optab,
  cond_len_ashl_optab,
  cond_len_ashr_optab,
  cond_len_lshr_optab,
  cond_len_smin_optab,
  cond_len_smax_optab,
  cond_len_umin_optab,
  cond_len_umax_optab,
  cond_len_copysign_optab,
  cond_len_fmin_optab,
  cond_len_fmax_optab,
  cond_len_fma_optab,
  cond_len_fms_optab,
  cond_len_fnma_optab,
  cond_len_fnms_optab,
  cond_len_neg_optab,
  cond_len_one_cmpl_optab,
  vcond_mask_len_optab,
  cmov_optab,
  cstore_optab,
  ctrap_optab,
  addv4_optab,
  subv4_optab,
  mulv4_optab,
  uaddv4_optab,
  usubv4_optab,
  umulv4_optab,
  negv3_optab,
  uaddc5_optab,
  usubc5_optab,
  addptr3_optab,
  spaceship_optab,
  smul_highpart_optab,
  umul_highpart_optab,
  cmpmem_optab,
  cmpstr_optab,
  cmpstrn_optab,
  cpymem_optab,
  movmem_optab,
  setmem_optab,
  strlen_optab,
  rawmemchr_optab,
  fma_optab,
  fms_optab,
  fnma_optab,
  fnms_optab,
  rint_optab,
  round_optab,
  roundeven_optab,
  floor_optab,
  ceil_optab,
  btrunc_optab,
  nearbyint_optab,
  acos_optab,
  acosh_optab,
  asin_optab,
  asinh_optab,
  atan2_optab,
  atan_optab,
  atanh_optab,
  copysign_optab,
  xorsign_optab,
  cadd90_optab,
  cadd270_optab,
  cmul_optab,
  cmul_conj_optab,
  cmla_optab,
  cmla_conj_optab,
  cmls_optab,
  cmls_conj_optab,
  cos_optab,
  cosh_optab,
  exp10_optab,
  exp2_optab,
  exp_optab,
  expm1_optab,
  fmod_optab,
  hypot_optab,
  ilogb_optab,
  isinf_optab,
  isfinite_optab,
  isnormal_optab,
  issignaling_optab,
  ldexp_optab,
  log10_optab,
  log1p_optab,
  log2_optab,
  log_optab,
  logb_optab,
  pow_optab,
  remainder_optab,
  rsqrt_optab,
  scalb_optab,
  signbit_optab,
  significand_optab,
  sin_optab,
  sincos_optab,
  sinh_optab,
  tan_optab,
  tanh_optab,
  fegetround_optab,
  feclearexcept_optab,
  feraiseexcept_optab,
  fmax_optab,
  fmin_optab,
  reduc_fmax_scal_optab,
  reduc_fmin_scal_optab,
  reduc_smax_scal_optab,
  reduc_smin_scal_optab,
  reduc_plus_scal_optab,
  reduc_umax_scal_optab,
  reduc_umin_scal_optab,
  reduc_and_scal_optab,
  reduc_ior_scal_optab,
  reduc_xor_scal_optab,
  fold_left_plus_optab,
  mask_fold_left_plus_optab,
  mask_len_fold_left_plus_optab,
  extract_last_optab,
  fold_extract_last_optab,
  len_fold_extract_last_optab,
  uabd_optab,
  sabd_optab,
  savg_floor_optab,
  uavg_floor_optab,
  savg_ceil_optab,
  uavg_ceil_optab,
  ssum_widen_optab,
  usum_widen_optab,
  usad_optab,
  ssad_optab,
  smulhs_optab,
  smulhrs_optab,
  umulhs_optab,
  umulhrs_optab,
  sdiv_pow2_optab,
  vec_pack_sfix_trunc_optab,
  vec_pack_ssat_optab,
  vec_pack_trunc_optab,
  vec_pack_ufix_trunc_optab,
  vec_pack_sbool_trunc_optab,
  vec_pack_usat_optab,
  vec_packs_float_optab,
  vec_packu_float_optab,
  vec_perm_optab,
  vec_realign_load_optab,
  vec_set_optab,
  vec_shl_optab,
  vec_shr_optab,
  vec_unpack_sfix_trunc_hi_optab,
  vec_unpack_sfix_trunc_lo_optab,
  vec_unpack_ufix_trunc_hi_optab,
  vec_unpack_ufix_trunc_lo_optab,
  vec_unpacks_float_hi_optab,
  vec_unpacks_float_lo_optab,
  vec_unpacks_hi_optab,
  vec_unpacks_lo_optab,
  vec_unpacks_sbool_hi_optab,
  vec_unpacks_sbool_lo_optab,
  vec_unpacku_float_hi_optab,
  vec_unpacku_float_lo_optab,
  vec_unpacku_hi_optab,
  vec_unpacku_lo_optab,
  vec_widen_smult_even_optab,
  vec_widen_smult_hi_optab,
  vec_widen_smult_lo_optab,
  vec_widen_smult_odd_optab,
  vec_widen_ssub_optab,
  vec_widen_ssub_hi_optab,
  vec_widen_ssub_lo_optab,
  vec_widen_ssub_odd_optab,
  vec_widen_ssub_even_optab,
  vec_widen_sadd_optab,
  vec_widen_sadd_hi_optab,
  vec_widen_sadd_lo_optab,
  vec_widen_sadd_odd_optab,
  vec_widen_sadd_even_optab,
  vec_widen_sabd_optab,
  vec_widen_sabd_hi_optab,
  vec_widen_sabd_lo_optab,
  vec_widen_sabd_odd_optab,
  vec_widen_sabd_even_optab,
  vec_widen_sshiftl_hi_optab,
  vec_widen_sshiftl_lo_optab,
  vec_widen_umult_even_optab,
  vec_widen_umult_hi_optab,
  vec_widen_umult_lo_optab,
  vec_widen_umult_odd_optab,
  vec_widen_ushiftl_hi_optab,
  vec_widen_ushiftl_lo_optab,
  vec_widen_usub_optab,
  vec_widen_usub_hi_optab,
  vec_widen_usub_lo_optab,
  vec_widen_usub_odd_optab,
  vec_widen_usub_even_optab,
  vec_widen_uadd_optab,
  vec_widen_uadd_hi_optab,
  vec_widen_uadd_lo_optab,
  vec_widen_uadd_odd_optab,
  vec_widen_uadd_even_optab,
  vec_widen_uabd_optab,
  vec_widen_uabd_hi_optab,
  vec_widen_uabd_lo_optab,
  vec_widen_uabd_odd_optab,
  vec_widen_uabd_even_optab,
  vec_addsub_optab,
  vec_fmaddsub_optab,
  vec_fmsubadd_optab,
  sync_add_optab,
  sync_and_optab,
  sync_ior_optab,
  sync_lock_release_optab,
  sync_nand_optab,
  sync_sub_optab,
  sync_xor_optab,
  atomic_add_fetch_optab,
  atomic_add_optab,
  atomic_and_fetch_optab,
  atomic_and_optab,
  atomic_bit_test_and_set_optab,
  atomic_bit_test_and_complement_optab,
  atomic_bit_test_and_reset_optab,
  atomic_compare_and_swap_optab,
  atomic_exchange_optab,
  atomic_fetch_add_optab,
  atomic_fetch_and_optab,
  atomic_fetch_nand_optab,
  atomic_fetch_or_optab,
  atomic_fetch_sub_optab,
  atomic_fetch_xor_optab,
  atomic_load_optab,
  atomic_nand_fetch_optab,
  atomic_nand_optab,
  atomic_or_fetch_optab,
  atomic_or_optab,
  atomic_store_optab,
  atomic_sub_fetch_optab,
  atomic_sub_optab,
  atomic_xor_fetch_optab,
  atomic_xor_optab,
  atomic_add_fetch_cmp_0_optab,
  atomic_sub_fetch_cmp_0_optab,
  atomic_and_fetch_cmp_0_optab,
  atomic_or_fetch_cmp_0_optab,
  atomic_xor_fetch_cmp_0_optab,
  get_thread_pointer_optab,
  set_thread_pointer_optab,
  check_raw_ptrs_optab,
  check_war_ptrs_optab,
  vec_duplicate_optab,
  vec_series_optab,
  vec_shl_insert_optab,
  len_load_optab,
  len_store_optab,
  mask_len_strided_load_optab,
  mask_len_strided_store_optab,
  select_vl_optab,
  andn_optab,
  iorn_optab,
  FIRST_CONV_OPTAB = sext_optab,
  LAST_CONVLIB_OPTAB = sstrunc_optab,
  LAST_CONV_OPTAB = while_ult_optab,
  FIRST_NORM_OPTAB = add_optab,
  LAST_NORMLIB_OPTAB = sync_lock_test_and_set_optab,
  LAST_NORM_OPTAB = iorn_optab
};

#define NUM_OPTABS          453
#define NUM_CONVLIB_OPTABS  17
#define NUM_NORMLIB_OPTABS  80
#define NUM_OPTAB_PATTERNS  2900
typedef enum optab_tag optab;
typedef enum optab_tag convert_optab;
typedef enum optab_tag direct_optab;

struct optab_libcall_d
{
  char libcall_suffix;
  const char *libcall_basename;
  void (*libcall_gen) (optab, const char *name,
		       char suffix, machine_mode);
};

struct convert_optab_libcall_d
{
  const char *libcall_basename;
  void (*libcall_gen) (convert_optab, const char *name,
		       machine_mode, machine_mode);
};

/* Given an enum insn_code, access the function to construct
   the body of that kind of insn.  */
#define GEN_FCN(CODE) (insn_data[CODE].genfun)

#ifdef NUM_RTX_CODE
/* Contains the optab used for each rtx code, and vice-versa.  */
extern const optab code_to_optab_[NUM_RTX_CODE];
extern const enum rtx_code optab_to_code_[NUM_OPTABS];

static inline optab
code_to_optab (enum rtx_code code)
{
  return code_to_optab_[code];
}

static inline enum rtx_code
optab_to_code (optab op)
{
  return optab_to_code_[op];
}

extern insn_code maybe_code_for_ccmp (machine_mode);
inline insn_code
code_for_ccmp (machine_mode arg0)
{
  insn_code code = maybe_code_for_ccmp (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_ccmp (machine_mode, rtx, rtx, rtx, rtx, rtx);
inline rtx
gen_ccmp (machine_mode arg0, rtx x0, rtx x1, rtx x2, rtx x3, rtx x4)
{
  rtx res = maybe_gen_ccmp (arg0, x0, x1, x2, x3, x4);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_cmp_1 (machine_mode);
inline insn_code
code_for_cmp_1 (machine_mode arg0)
{
  insn_code code = maybe_code_for_cmp_1 (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_cmp_1 (machine_mode, rtx, rtx);
inline rtx
gen_cmp_1 (machine_mode arg0, rtx x0, rtx x1)
{
  rtx res = maybe_gen_cmp_1 (arg0, x0, x1);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_pushfl2 (machine_mode);
inline insn_code
code_for_pushfl2 (machine_mode arg0)
{
  insn_code code = maybe_code_for_pushfl2 (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_pushfl2 (machine_mode, rtx, rtx);
inline rtx
gen_pushfl2 (machine_mode arg0, rtx x0, rtx x1)
{
  rtx res = maybe_gen_pushfl2 (arg0, x0, x1);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_popfl1 (machine_mode);
inline insn_code
code_for_popfl1 (machine_mode arg0)
{
  insn_code code = maybe_code_for_popfl1 (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_popfl1 (machine_mode, rtx, rtx);
inline rtx
gen_popfl1 (machine_mode arg0, rtx x0, rtx x1)
{
  rtx res = maybe_gen_popfl1 (arg0, x0, x1);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_insv_1 (machine_mode);
inline insn_code
code_for_insv_1 (machine_mode arg0)
{
  insn_code code = maybe_code_for_insv_1 (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_insv_1 (machine_mode, rtx, rtx);
inline rtx
gen_insv_1 (machine_mode arg0, rtx x0, rtx x1)
{
  rtx res = maybe_gen_insv_1 (arg0, x0, x1);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_add3_carry (machine_mode);
inline insn_code
code_for_add3_carry (machine_mode arg0)
{
  insn_code code = maybe_code_for_add3_carry (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_add3_carry (machine_mode, rtx, rtx, rtx, rtx, rtx);
inline rtx
gen_add3_carry (machine_mode arg0, rtx x0, rtx x1, rtx x2, rtx x3, rtx x4)
{
  rtx res = maybe_gen_add3_carry (arg0, x0, x1, x2, x3, x4);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_sub3_carry (machine_mode);
inline insn_code
code_for_sub3_carry (machine_mode arg0)
{
  insn_code code = maybe_code_for_sub3_carry (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_sub3_carry (machine_mode, rtx, rtx, rtx, rtx, rtx);
inline rtx
gen_sub3_carry (machine_mode arg0, rtx x0, rtx x1, rtx x2, rtx x3, rtx x4)
{
  rtx res = maybe_gen_sub3_carry (arg0, x0, x1, x2, x3, x4);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_sub3_carry_ccc (machine_mode);
inline insn_code
code_for_sub3_carry_ccc (machine_mode arg0)
{
  insn_code code = maybe_code_for_sub3_carry_ccc (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_sub3_carry_ccc (machine_mode, rtx, rtx, rtx);
inline rtx
gen_sub3_carry_ccc (machine_mode arg0, rtx x0, rtx x1, rtx x2)
{
  rtx res = maybe_gen_sub3_carry_ccc (arg0, x0, x1, x2);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_sub3_carry_ccgz (machine_mode);
inline insn_code
code_for_sub3_carry_ccgz (machine_mode arg0)
{
  insn_code code = maybe_code_for_sub3_carry_ccgz (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_sub3_carry_ccgz (machine_mode, rtx, rtx, rtx);
inline rtx
gen_sub3_carry_ccgz (machine_mode arg0, rtx x0, rtx x1, rtx x2)
{
  rtx res = maybe_gen_sub3_carry_ccgz (arg0, x0, x1, x2);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_add3_cc_overflow_1 (machine_mode);
inline insn_code
code_for_add3_cc_overflow_1 (machine_mode arg0)
{
  insn_code code = maybe_code_for_add3_cc_overflow_1 (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_add3_cc_overflow_1 (machine_mode, rtx, rtx, rtx);
inline rtx
gen_add3_cc_overflow_1 (machine_mode arg0, rtx x0, rtx x1, rtx x2)
{
  rtx res = maybe_gen_add3_cc_overflow_1 (arg0, x0, x1, x2);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_test_ccno_1 (machine_mode);
inline insn_code
code_for_test_ccno_1 (machine_mode arg0)
{
  insn_code code = maybe_code_for_test_ccno_1 (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_test_ccno_1 (machine_mode, rtx, rtx);
inline rtx
gen_test_ccno_1 (machine_mode arg0, rtx x0, rtx x1)
{
  rtx res = maybe_gen_test_ccno_1 (arg0, x0, x1);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_x86_shift_adj_1 (machine_mode);
inline insn_code
code_for_x86_shift_adj_1 (machine_mode arg0)
{
  insn_code code = maybe_code_for_x86_shift_adj_1 (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_x86_shift_adj_1 (machine_mode, rtx, rtx, rtx, rtx);
inline rtx
gen_x86_shift_adj_1 (machine_mode arg0, rtx x0, rtx x1, rtx x2, rtx x3)
{
  rtx res = maybe_gen_x86_shift_adj_1 (arg0, x0, x1, x2, x3);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_x86_shift_adj_2 (machine_mode);
inline insn_code
code_for_x86_shift_adj_2 (machine_mode arg0)
{
  insn_code code = maybe_code_for_x86_shift_adj_2 (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_x86_shift_adj_2 (machine_mode, rtx, rtx, rtx);
inline rtx
gen_x86_shift_adj_2 (machine_mode arg0, rtx x0, rtx x1, rtx x2)
{
  rtx res = maybe_gen_x86_shift_adj_2 (arg0, x0, x1, x2);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_x86_shift_adj_3 (machine_mode);
inline insn_code
code_for_x86_shift_adj_3 (machine_mode arg0)
{
  insn_code code = maybe_code_for_x86_shift_adj_3 (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_x86_shift_adj_3 (machine_mode, rtx, rtx, rtx);
inline rtx
gen_x86_shift_adj_3 (machine_mode arg0, rtx x0, rtx x1, rtx x2)
{
  rtx res = maybe_gen_x86_shift_adj_3 (arg0, x0, x1, x2);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_leave (machine_mode);
inline insn_code
code_for_leave (machine_mode arg0)
{
  insn_code code = maybe_code_for_leave (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_leave (machine_mode);
inline rtx
gen_leave (machine_mode arg0)
{
  rtx res = maybe_gen_leave (arg0);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_tbm_bextri (machine_mode);
inline insn_code
code_for_tbm_bextri (machine_mode arg0)
{
  insn_code code = maybe_code_for_tbm_bextri (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_tbm_bextri (machine_mode, rtx, rtx, rtx, rtx);
inline rtx
gen_tbm_bextri (machine_mode arg0, rtx x0, rtx x1, rtx x2, rtx x3)
{
  rtx res = maybe_gen_tbm_bextri (arg0, x0, x1, x2, x3);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_tls_global_dynamic_64 (machine_mode);
inline insn_code
code_for_tls_global_dynamic_64 (machine_mode arg0)
{
  insn_code code = maybe_code_for_tls_global_dynamic_64 (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_tls_global_dynamic_64 (machine_mode, rtx, rtx, rtx, rtx);
inline rtx
gen_tls_global_dynamic_64 (machine_mode arg0, rtx x0, rtx x1, rtx x2, rtx x3)
{
  rtx res = maybe_gen_tls_global_dynamic_64 (arg0, x0, x1, x2, x3);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_tls_local_dynamic_base_64 (machine_mode);
inline insn_code
code_for_tls_local_dynamic_base_64 (machine_mode arg0)
{
  insn_code code = maybe_code_for_tls_local_dynamic_base_64 (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_tls_local_dynamic_base_64 (machine_mode, rtx, rtx, rtx);
inline rtx
gen_tls_local_dynamic_base_64 (machine_mode arg0, rtx x0, rtx x1, rtx x2)
{
  rtx res = maybe_gen_tls_local_dynamic_base_64 (arg0, x0, x1, x2);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_tls_dynamic_gnu2_64 (machine_mode);
inline insn_code
code_for_tls_dynamic_gnu2_64 (machine_mode arg0)
{
  insn_code code = maybe_code_for_tls_dynamic_gnu2_64 (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_tls_dynamic_gnu2_64 (machine_mode, rtx, rtx);
inline rtx
gen_tls_dynamic_gnu2_64 (machine_mode arg0, rtx x0, rtx x1)
{
  rtx res = maybe_gen_tls_dynamic_gnu2_64 (arg0, x0, x1);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_pro_epilogue_adjust_stack_add (machine_mode);
inline insn_code
code_for_pro_epilogue_adjust_stack_add (machine_mode arg0)
{
  insn_code code = maybe_code_for_pro_epilogue_adjust_stack_add (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_pro_epilogue_adjust_stack_add (machine_mode, rtx, rtx, rtx);
inline rtx
gen_pro_epilogue_adjust_stack_add (machine_mode arg0, rtx x0, rtx x1, rtx x2)
{
  rtx res = maybe_gen_pro_epilogue_adjust_stack_add (arg0, x0, x1, x2);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_pro_epilogue_adjust_stack_sub (machine_mode);
inline insn_code
code_for_pro_epilogue_adjust_stack_sub (machine_mode arg0)
{
  insn_code code = maybe_code_for_pro_epilogue_adjust_stack_sub (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_pro_epilogue_adjust_stack_sub (machine_mode, rtx, rtx, rtx);
inline rtx
gen_pro_epilogue_adjust_stack_sub (machine_mode arg0, rtx x0, rtx x1, rtx x2)
{
  rtx res = maybe_gen_pro_epilogue_adjust_stack_sub (arg0, x0, x1, x2);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_allocate_stack_worker_probe (machine_mode);
inline insn_code
code_for_allocate_stack_worker_probe (machine_mode arg0)
{
  insn_code code = maybe_code_for_allocate_stack_worker_probe (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_allocate_stack_worker_probe (machine_mode, rtx, rtx);
inline rtx
gen_allocate_stack_worker_probe (machine_mode arg0, rtx x0, rtx x1)
{
  rtx res = maybe_gen_allocate_stack_worker_probe (arg0, x0, x1);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_probe_stack_1 (machine_mode);
inline insn_code
code_for_probe_stack_1 (machine_mode arg0)
{
  insn_code code = maybe_code_for_probe_stack_1 (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_probe_stack_1 (machine_mode, rtx, rtx);
inline rtx
gen_probe_stack_1 (machine_mode arg0, rtx x0, rtx x1)
{
  rtx res = maybe_gen_probe_stack_1 (arg0, x0, x1);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_adjust_stack_and_probe (machine_mode);
inline insn_code
code_for_adjust_stack_and_probe (machine_mode arg0)
{
  insn_code code = maybe_code_for_adjust_stack_and_probe (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_adjust_stack_and_probe (machine_mode, rtx, rtx, rtx);
inline rtx
gen_adjust_stack_and_probe (machine_mode arg0, rtx x0, rtx x1, rtx x2)
{
  rtx res = maybe_gen_adjust_stack_and_probe (arg0, x0, x1, x2);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_probe_stack_range (machine_mode);
inline insn_code
code_for_probe_stack_range (machine_mode arg0)
{
  insn_code code = maybe_code_for_probe_stack_range (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_probe_stack_range (machine_mode, rtx, rtx, rtx);
inline rtx
gen_probe_stack_range (machine_mode arg0, rtx x0, rtx x1, rtx x2)
{
  rtx res = maybe_gen_probe_stack_range (arg0, x0, x1, x2);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_stack_protect_set_1 (machine_mode, machine_mode);
inline insn_code
code_for_stack_protect_set_1 (machine_mode arg0, machine_mode arg1)
{
  insn_code code = maybe_code_for_stack_protect_set_1 (arg0, arg1);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_stack_protect_set_1 (machine_mode, machine_mode, rtx, rtx, rtx);
inline rtx
gen_stack_protect_set_1 (machine_mode arg0, machine_mode arg1, rtx x0, rtx x1, rtx x2)
{
  rtx res = maybe_gen_stack_protect_set_1 (arg0, arg1, x0, x1, x2);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_stack_protect_test_1 (machine_mode);
inline insn_code
code_for_stack_protect_test_1 (machine_mode arg0)
{
  insn_code code = maybe_code_for_stack_protect_test_1 (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_stack_protect_test_1 (machine_mode, rtx, rtx, rtx);
inline rtx
gen_stack_protect_test_1 (machine_mode arg0, rtx x0, rtx x1, rtx x2)
{
  rtx res = maybe_gen_stack_protect_test_1 (arg0, x0, x1, x2);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_lwp_llwpcb (machine_mode);
inline insn_code
code_for_lwp_llwpcb (machine_mode arg0)
{
  insn_code code = maybe_code_for_lwp_llwpcb (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_lwp_llwpcb (machine_mode, rtx);
inline rtx
gen_lwp_llwpcb (machine_mode arg0, rtx x0)
{
  rtx res = maybe_gen_lwp_llwpcb (arg0, x0);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_lwp_slwpcb (machine_mode);
inline insn_code
code_for_lwp_slwpcb (machine_mode arg0)
{
  insn_code code = maybe_code_for_lwp_slwpcb (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_lwp_slwpcb (machine_mode, rtx);
inline rtx
gen_lwp_slwpcb (machine_mode arg0, rtx x0)
{
  rtx res = maybe_gen_lwp_slwpcb (arg0, x0);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_lwp_lwpval (machine_mode);
inline insn_code
code_for_lwp_lwpval (machine_mode arg0)
{
  insn_code code = maybe_code_for_lwp_lwpval (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_lwp_lwpval (machine_mode, rtx, rtx, rtx);
inline rtx
gen_lwp_lwpval (machine_mode arg0, rtx x0, rtx x1, rtx x2)
{
  rtx res = maybe_gen_lwp_lwpval (arg0, x0, x1, x2);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_lwp_lwpins (machine_mode);
inline insn_code
code_for_lwp_lwpins (machine_mode arg0)
{
  insn_code code = maybe_code_for_lwp_lwpins (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_lwp_lwpins (machine_mode, rtx, rtx, rtx);
inline rtx
gen_lwp_lwpins (machine_mode arg0, rtx x0, rtx x1, rtx x2)
{
  rtx res = maybe_gen_lwp_lwpins (arg0, x0, x1, x2);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_rdrand (machine_mode);
inline insn_code
code_for_rdrand (machine_mode arg0)
{
  insn_code code = maybe_code_for_rdrand (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_rdrand (machine_mode, rtx);
inline rtx
gen_rdrand (machine_mode arg0, rtx x0)
{
  rtx res = maybe_gen_rdrand (arg0, x0);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_rdseed (machine_mode);
inline insn_code
code_for_rdseed (machine_mode arg0)
{
  insn_code code = maybe_code_for_rdseed (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_rdseed (machine_mode, rtx);
inline rtx
gen_rdseed (machine_mode arg0, rtx x0)
{
  rtx res = maybe_gen_rdseed (arg0, x0);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_rdssp (machine_mode);
inline insn_code
code_for_rdssp (machine_mode arg0)
{
  insn_code code = maybe_code_for_rdssp (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_rdssp (machine_mode, rtx, rtx);
inline rtx
gen_rdssp (machine_mode arg0, rtx x0, rtx x1)
{
  rtx res = maybe_gen_rdssp (arg0, x0, x1);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_incssp (machine_mode);
inline insn_code
code_for_incssp (machine_mode arg0)
{
  insn_code code = maybe_code_for_incssp (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_incssp (machine_mode, rtx);
inline rtx
gen_incssp (machine_mode arg0, rtx x0)
{
  rtx res = maybe_gen_incssp (arg0, x0);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_wrss (machine_mode);
inline insn_code
code_for_wrss (machine_mode arg0)
{
  insn_code code = maybe_code_for_wrss (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_wrss (machine_mode, rtx, rtx);
inline rtx
gen_wrss (machine_mode arg0, rtx x0, rtx x1)
{
  rtx res = maybe_gen_wrss (arg0, x0, x1);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_wruss (machine_mode);
inline insn_code
code_for_wruss (machine_mode arg0)
{
  insn_code code = maybe_code_for_wruss (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_wruss (machine_mode, rtx, rtx);
inline rtx
gen_wruss (machine_mode arg0, rtx x0, rtx x1)
{
  rtx res = maybe_gen_wruss (arg0, x0, x1);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_monitorx (machine_mode);
inline insn_code
code_for_monitorx (machine_mode arg0)
{
  insn_code code = maybe_code_for_monitorx (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_monitorx (machine_mode, rtx, rtx, rtx);
inline rtx
gen_monitorx (machine_mode arg0, rtx x0, rtx x1, rtx x2)
{
  rtx res = maybe_gen_monitorx (arg0, x0, x1, x2);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_clzero (machine_mode);
inline insn_code
code_for_clzero (machine_mode arg0)
{
  insn_code code = maybe_code_for_clzero (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_clzero (machine_mode, rtx);
inline rtx
gen_clzero (machine_mode arg0, rtx x0)
{
  rtx res = maybe_gen_clzero (arg0, x0);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_movdir64b (machine_mode);
inline insn_code
code_for_movdir64b (machine_mode arg0)
{
  insn_code code = maybe_code_for_movdir64b (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_movdir64b (machine_mode, rtx, rtx);
inline rtx
gen_movdir64b (machine_mode arg0, rtx x0, rtx x1)
{
  rtx res = maybe_gen_movdir64b (arg0, x0, x1);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_enqcmd (int, machine_mode);
inline insn_code
code_for_enqcmd (int arg0, machine_mode arg1)
{
  insn_code code = maybe_code_for_enqcmd (arg0, arg1);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_enqcmd (int, machine_mode, rtx, rtx);
inline rtx
gen_enqcmd (int arg0, machine_mode arg1, rtx x0, rtx x1)
{
  rtx res = maybe_gen_enqcmd (arg0, arg1, x0, x1);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_umonitor (machine_mode);
inline insn_code
code_for_umonitor (machine_mode arg0)
{
  insn_code code = maybe_code_for_umonitor (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_umonitor (machine_mode, rtx);
inline rtx
gen_umonitor (machine_mode arg0, rtx x0)
{
  rtx res = maybe_gen_umonitor (arg0, x0);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_sse4_1_insertps (machine_mode);
inline insn_code
code_for_sse4_1_insertps (machine_mode arg0)
{
  insn_code code = maybe_code_for_sse4_1_insertps (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_sse4_1_insertps (machine_mode, rtx, rtx, rtx, rtx);
inline rtx
gen_sse4_1_insertps (machine_mode arg0, rtx x0, rtx x1, rtx x2, rtx x3)
{
  rtx res = maybe_gen_sse4_1_insertps (arg0, x0, x1, x2, x3);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_vec_set_0 (machine_mode);
inline insn_code
code_for_vec_set_0 (machine_mode arg0)
{
  insn_code code = maybe_code_for_vec_set_0 (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_vec_set_0 (machine_mode, rtx, rtx, rtx);
inline rtx
gen_vec_set_0 (machine_mode arg0, rtx x0, rtx x1, rtx x2)
{
  rtx res = maybe_gen_vec_set_0 (arg0, x0, x1, x2);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_vec_extract_lo (machine_mode);
inline insn_code
code_for_vec_extract_lo (machine_mode arg0)
{
  insn_code code = maybe_code_for_vec_extract_lo (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_vec_extract_lo (machine_mode, rtx, rtx);
inline rtx
gen_vec_extract_lo (machine_mode arg0, rtx x0, rtx x1)
{
  rtx res = maybe_gen_vec_extract_lo (arg0, x0, x1);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_vec_extract_hi (machine_mode);
inline insn_code
code_for_vec_extract_hi (machine_mode arg0)
{
  insn_code code = maybe_code_for_vec_extract_hi (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_vec_extract_hi (machine_mode, rtx, rtx);
inline rtx
gen_vec_extract_hi (machine_mode arg0, rtx x0, rtx x1)
{
  rtx res = maybe_gen_vec_extract_hi (arg0, x0, x1);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_vec_interleave_high (machine_mode);
inline insn_code
code_for_vec_interleave_high (machine_mode arg0)
{
  insn_code code = maybe_code_for_vec_interleave_high (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_vec_interleave_high (machine_mode, rtx, rtx, rtx);
inline rtx
gen_vec_interleave_high (machine_mode arg0, rtx x0, rtx x1, rtx x2)
{
  rtx res = maybe_gen_vec_interleave_high (arg0, x0, x1, x2);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_vec_interleave_low (machine_mode);
inline insn_code
code_for_vec_interleave_low (machine_mode arg0)
{
  insn_code code = maybe_code_for_vec_interleave_low (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_vec_interleave_low (machine_mode, rtx, rtx, rtx);
inline rtx
gen_vec_interleave_low (machine_mode arg0, rtx x0, rtx x1, rtx x2)
{
  rtx res = maybe_gen_vec_interleave_low (arg0, x0, x1, x2);
  gcc_assert (res);
  return res;
}

extern insn_code maybe_code_for_sse3_monitor (machine_mode);
inline insn_code
code_for_sse3_monitor (machine_mode arg0)
{
  insn_code code = maybe_code_for_sse3_monitor (arg0);
  gcc_assert (code != CODE_FOR_nothing);
  return code;
}

extern rtx maybe_gen_sse3_monitor (machine_mode, rtx, rtx, rtx);
inline rtx
gen_sse3_monitor (machine_mode arg0, rtx x0, rtx x1, rtx x2)
{
  rtx res = maybe_gen_sse3_monitor (arg0, x0, x1, x2);
  gcc_assert (res);
  return res;
}
#endif

extern const struct convert_optab_libcall_d convlib_def[NUM_CONVLIB_OPTABS];
extern const struct optab_libcall_d normlib_def[NUM_NORMLIB_OPTABS];

/* Returns the active icode for the given (encoded) optab.  */
extern enum insn_code raw_optab_handler (unsigned);
extern bool swap_optab_enable (optab, machine_mode, bool);

/* Target-dependent globals.  */
struct target_optabs {
  /* Patterns that are used by optabs that are enabled for this target.  */
  bool pat_enable[NUM_OPTAB_PATTERNS];

  /* Index VOIDmode caches if the target supports vec_gather_load for any
     vector mode.  Every other index X caches specifically for mode X.
     1 means yes, -1 means no.  */
  signed char supports_vec_gather_load[NUM_MACHINE_MODES];
  signed char supports_vec_scatter_store[NUM_MACHINE_MODES];
};
extern void init_all_optabs (struct target_optabs *);
extern bool partial_vectors_supported_p (void);

extern struct target_optabs default_target_optabs;
extern struct target_optabs *this_fn_optabs;
#if SWITCHABLE_TARGET
extern struct target_optabs *this_target_optabs;
#else
#define this_target_optabs (&default_target_optabs)
#endif
#endif
