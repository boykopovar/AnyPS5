#pragma once

#include "SceTypes.hpp"

extern "C" {

void APS5_VABI sceLibcInternalIstreamCompleteDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalIstreamDeletingDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalIstreamCompleteDestructorThunk_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalIstreamDeletingDestructorThunk_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalOstreamCompleteDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalOstreamDeletingDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalOstreamCompleteDestructorThunk_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalOstreamDeletingDestructorThunk_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalWideIstreamCompleteDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalWideIstreamDeletingDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalWideIstreamCompleteDestructorThunk_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalWideIstreamDeletingDestructorThunk_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalWideOstreamCompleteDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalWideOstreamDeletingDestructor_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalWideOstreamCompleteDestructorThunk_nid_no_patch(void* self);
void APS5_VABI sceLibcInternalWideOstreamDeletingDestructorThunk_nid_no_patch(void* self);

}
