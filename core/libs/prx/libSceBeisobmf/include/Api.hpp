#pragma once

#include "SceTypes.hpp"

extern "C" {

void *APS5_VABI isobmf_box_getchildbox_nid_postfix(void *box);
std::uint64_t APS5_VABI isobmf_box_getdiskoffset_nid_postfix(void *box);
std::uint32_t APS5_VABI isobmf_box_getflags32b_nid_postfix(void *box);
std::uint32_t APS5_VABI isobmf_box_getmember_length_nid_postfix(void *box, const char *name);
void *APS5_VABI isobmf_box_getmember_ptr_nid_postfix(void *box, const char *name);
std::uint32_t APS5_VABI isobmf_box_getmember_type_nid_postfix(void *box, const char *name);
void *APS5_VABI isobmf_box_getnextbox_nid_postfix(void *box);
std::uint32_t APS5_VABI isobmf_box_getstate_nid_postfix(void *box);
std::uint32_t APS5_VABI isobmf_box_gettype_nid_postfix(void *box);
int APS5_VABI isobmf_decoder_clear_nid_postfix(void);
int APS5_VABI isobmf_decoder_create_nid_postfix(void);
int APS5_VABI isobmf_decoder_decode_nid_postfix(void);
int APS5_VABI isobmf_decoder_destroy_nid_postfix(void);
int APS5_VABI isobmf_decoder_get_decoder_tree_nid_postfix(void);
int APS5_VABI isobmf_decoder_getquerydata_nid_postfix(void);
std::uint64_t APS5_VABI isobmf_decoder_next_disk_offset_nid_postfix(void *decoder);
int APS5_VABI isobmf_decoder_query_nid_postfix(void);
int APS5_VABI isobmf_memmanager_set_nid_postfix(void);

}
