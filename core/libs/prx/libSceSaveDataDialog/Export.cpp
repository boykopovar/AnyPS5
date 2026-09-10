#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

 int APS5_VABI sceSaveDataDialogClose(const void* close_param) {
  (void)close_param;
  NotImplemented_nid_no_patch(__func__);
  return 0;
 }

 int APS5_VABI sceSaveDataDialogGetResult(void* result) {
  (void)result;
  NotImplemented_nid_no_patch(__func__);
  return 0;
 }

 int APS5_VABI sceSaveDataDialogInitialize(void) {
  NotImplemented_nid_no_patch(__func__);
  return 0;
 }

 int APS5_VABI sceSaveDataDialogIsReadyToDisplay(void) {
  NotImplemented_nid_no_patch(__func__);
  return 0;
 }

 int APS5_VABI sceSaveDataDialogOpen(const void* param) {
  (void)param;
  NotImplemented_nid_no_patch(__func__);
  return 0;
 }

 int APS5_VABI sceSaveDataDialogProgressBarInc(int target, uint32_t delta) {
  (void)target;
  (void)delta;
  NotImplemented_nid_no_patch(__func__);
  return 0;
 }

 int APS5_VABI sceSaveDataDialogProgressBarSetValue(int target, uint32_t rate) {
  (void)target;
  (void)rate;
  NotImplemented_nid_no_patch(__func__);
  return 0;
 }

 int APS5_VABI sceSaveDataDialogTerminate(void) {
  NotImplemented_nid_no_patch(__func__);
  return 0;
 }

}
