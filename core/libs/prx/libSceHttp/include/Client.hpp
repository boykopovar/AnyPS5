#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct Aps5HttpClient {};

extern "C" {

Aps5HttpClient* APS5_VABI Aps5HttpClient_Create_nid_postfix();
void APS5_VABI Aps5HttpClient_Destroy_nid_postfix(Aps5HttpClient* client);
std::int32_t APS5_VABI Aps5HttpClient_Open_nid_postfix(Aps5HttpClient* client, const char* url);
std::int32_t APS5_VABI Aps5HttpClient_Read_nid_postfix(Aps5HttpClient* client, std::uint8_t* destination, std::int32_t capacity);
std::int64_t APS5_VABI Aps5HttpClient_Seek_nid_postfix(Aps5HttpClient* client, std::int64_t offset, std::int32_t origin);
std::int64_t APS5_VABI Aps5HttpClient_GetSize_nid_postfix(const Aps5HttpClient* client);
void APS5_VABI Aps5HttpClient_Start_nid_postfix(Aps5HttpClient* client);
void APS5_VABI Aps5HttpClient_Stop_nid_postfix(Aps5HttpClient* client);

}
