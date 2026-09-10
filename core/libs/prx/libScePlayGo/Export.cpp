#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI scePlayGoClose(int handle) {
 (void)handle;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePlayGoGetChunkId(int handle, uint16_t* out_chunk_id_list, uint32_t number_of_entries, uint32_t* out_entries) {
 (void)handle;
 (void)out_chunk_id_list;
 (void)number_of_entries;
 (void)out_entries;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePlayGoGetEta(int handle, const uint16_t* chunk_ids, uint32_t number_of_entries, int64_t* out_eta) {
 (void)handle;
 (void)chunk_ids;
 (void)number_of_entries;
 (void)out_eta;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePlayGoGetInstallChunkId(int handle, uint16_t* out_chunk_id_list, uint32_t number_of_entries, uint32_t* out_entries) {
 (void)handle;
 (void)out_chunk_id_list;
 (void)number_of_entries;
 (void)out_entries;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePlayGoGetInstallSpeed(int handle, int32_t* out_speed) {
 (void)handle;
 (void)out_speed;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePlayGoGetLanguageMask(int handle, uint64_t* out_language_mask) {
 (void)handle;
 (void)out_language_mask;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePlayGoGetLocus(int handle, const uint16_t* chunk_ids, uint32_t number_of_entries, int8_t* out_loci) {
 (void)handle;
 (void)chunk_ids;
 (void)number_of_entries;
 (void)out_loci;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePlayGoGetOptionalChunk(int handle, int32_t type, PlayGoOptionalChunk* option) {
 (void)handle;
 (void)type;
 (void)option;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePlayGoGetProgress(int handle, const uint16_t* chunk_ids, uint32_t number_of_entries, PlayGoProgress* out_progress) {
 (void)handle;
 (void)chunk_ids;
 (void)number_of_entries;
 (void)out_progress;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePlayGoGetSupportedOptionalChunk(int handle, int32_t type, PlayGoOptionalChunk* option) {
 (void)handle;
 (void)type;
 (void)option;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePlayGoGetToDoList(int handle, PlayGoToDo* out_todo_list, uint32_t number_of_entries, uint32_t* out_entries) {
 (void)handle;
 (void)out_todo_list;
 (void)number_of_entries;
 (void)out_entries;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePlayGoInitialize(const PlayGoInitParams* init) {
 (void)init;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePlayGoOpen(int* out_handle, const void* param) {
 (void)out_handle;
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePlayGoPrefetch(int handle, const uint16_t* chunk_ids, uint32_t number_of_entries, int8_t minimum_locus) {
 (void)handle;
 (void)chunk_ids;
 (void)number_of_entries;
 (void)minimum_locus;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePlayGoPrefetchOptionalChunk(int handle, int32_t type, const PlayGoOptionalChunk* option) {
 (void)handle;
 (void)type;
 (void)option;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePlayGoSetInstallSpeed(int handle, int32_t speed) {
 (void)handle;
 (void)speed;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePlayGoSetToDoList(int handle, const PlayGoToDo* todo_list, uint32_t number_of_entries) {
 (void)handle;
 (void)todo_list;
 (void)number_of_entries;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePlayGoTerminate(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
