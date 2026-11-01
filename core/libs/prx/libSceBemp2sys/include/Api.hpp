#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI bemp2sys_logprintf_getlevel_nid_postfix(void);
int APS5_VABI bemp2sys_logprintf_setfunc_nid_postfix(void);
int APS5_VABI bemp2sys_logprintf_setlevel_nid_postfix(void);
int APS5_VABI bemp2sys_memmanager_set_nid_postfix(void);
int APS5_VABI bemp2sys_packetizer_create_nid_postfix(void);
int APS5_VABI bemp2sys_packetizer_destroy_nid_postfix(void);
int APS5_VABI bemp2sys_packetizer_init_nid_postfix(void);
int APS5_VABI bemp2sys_packetizer_release_nid_postfix(void);
int APS5_VABI bemp2sys_pesparser_create_nid_postfix(void);
int APS5_VABI bemp2sys_pesparser_destroy_nid_postfix(void);
int APS5_VABI bemp2sys_pesparser_init_nid_postfix(void);
int APS5_VABI bemp2sys_pesparser_parse_nid_postfix(void);
int APS5_VABI bemp2sys_pesparser_release_nid_postfix(void);
int APS5_VABI bemp2sys_pesparser_sendmessage_nid_postfix(void);
int APS5_VABI bemp2sys_psdecoder_create_nid_postfix(void);
int APS5_VABI bemp2sys_psdecoder_destroy_nid_postfix(void);
int APS5_VABI bemp2sys_psdecoder_init_nid_postfix(void);
int APS5_VABI bemp2sys_psdecoder_readpacket_nid_postfix(void);
int APS5_VABI bemp2sys_psdecoder_release_nid_postfix(void);
int APS5_VABI bemp2sys_psdecoder_sendmessage_nid_postfix(void);
int APS5_VABI bemp2sys_tsdecoder_create_nid_postfix(void);
int APS5_VABI bemp2sys_tsdecoder_destroy_nid_postfix(void);
int APS5_VABI bemp2sys_tsdecoder_init_nid_postfix(void);
int APS5_VABI bemp2sys_tsdecoder_readpacket_nid_postfix(void);
int APS5_VABI bemp2sys_tsdecoder_release_nid_postfix(void);
int APS5_VABI bemp2sys_tsdecoder_sendmessage_nid_postfix(void);
int APS5_VABI bemp2sys_tsfilter_create_nid_postfix(void);
int APS5_VABI bemp2sys_tsfilter_destroy_nid_postfix(void);
int APS5_VABI bemp2sys_tsfilter_init_nid_postfix(void);
int APS5_VABI bemp2sys_tsfilter_release_nid_postfix(void);
int APS5_VABI bemp2sys_tsfilter_sendmessage_nid_postfix(void);

}
