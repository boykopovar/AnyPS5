#include "prx/libc/include/Api.hpp"

#include <atomic>
#include <cstdint>

namespace {

template <typename Fn>
void UpdateCount(std::int32_t& count, Fn&& compute_next) {
  std::atomic_ref<std::int32_t> atomic_count(count);
  std::int32_t current = atomic_count.load(std::memory_order_relaxed);
  while (!atomic_count.compare_exchange_weak(
      current, compute_next(current), std::memory_order_acq_rel, std::memory_order_relaxed)) {
  }
}

void Increment(std::int32_t& count) {
  UpdateCount(count, [](std::int32_t current) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(current < 0 ? 0 : current) + 1u);
  });
}

void Decrement(std::int32_t& count) {
  UpdateCount(count, [](std::int32_t current) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(current) - 1u);
  });
}

}

extern "C" {

static_assert(alignof(std::int32_t) >= std::atomic_ref<std::int32_t>::required_alignment);

std::int32_t _ZNSt8__sce_v26_Winit9_Init_cntE_nid_postfix = -1;
std::int32_t _ZNSt8__sce_v28ios_base4Init9_Init_cntE_nid_postfix = -1;

void APS5_VABI _ZNSt8__sce_v26_WinitC1Ev_nid_postfix(void*) { Increment(_ZNSt8__sce_v26_Winit9_Init_cntE_nid_postfix); }
void APS5_VABI _ZNSt8__sce_v26_WinitC2Ev_nid_postfix(void*) { Increment(_ZNSt8__sce_v26_Winit9_Init_cntE_nid_postfix); }
void APS5_VABI _ZNSt8__sce_v26_WinitD1Ev_nid_postfix(void*) { Decrement(_ZNSt8__sce_v26_Winit9_Init_cntE_nid_postfix); }
void APS5_VABI _ZNSt8__sce_v26_WinitD2Ev_nid_postfix(void*) { Decrement(_ZNSt8__sce_v26_Winit9_Init_cntE_nid_postfix); }

void APS5_VABI _ZNSt8__sce_v28ios_base4Init10_Init_ctorEPS1__nid_postfix(void*) { Increment(_ZNSt8__sce_v28ios_base4Init9_Init_cntE_nid_postfix); }
void APS5_VABI _ZNSt8__sce_v28ios_base4Init10_Init_dtorEPS1__nid_postfix(void*) { Decrement(_ZNSt8__sce_v28ios_base4Init9_Init_cntE_nid_postfix); }
std::int32_t* APS5_VABI _ZNSt8__sce_v28ios_base4Init14_Init_cnt_funcEv_nid_postfix() { return &_ZNSt8__sce_v28ios_base4Init9_Init_cntE_nid_postfix; }

}
