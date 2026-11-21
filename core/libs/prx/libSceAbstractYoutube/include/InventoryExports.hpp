#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI _ZN15AbstractStorage13YoutubeFolder6RemoveEb(void*, bool);
int APS5_VABI _ZN15AbstractStorage13YoutubeFolderD0Ev();
int APS5_VABI _ZN15AbstractStorage13YoutubeFolderD1Ev();
int APS5_VABI _ZN15AbstractStorage13YoutubeFolderD2Ev();
int APS5_VABI _ZN15AbstractStorage14YoutubeContent11SetMetadataEPN23sceMetadataReaderWriter8MetadataE();
int APS5_VABI _ZN15AbstractStorage14YoutubeContent13ParseMetadataEPN23sceMetadataReaderWriter8MetadataES3_();
int APS5_VABI _ZN15AbstractStorage14YoutubeContent4OpenEit();
int APS5_VABI _ZN15AbstractStorage14YoutubeContent4ReadEPvlPl(void*, void*, std::int64_t, std::int64_t*);
int APS5_VABI _ZN15AbstractStorage14YoutubeContent5CloseEv();
int APS5_VABI _ZN15AbstractStorage14YoutubeContent5FlushEv(void*);
int APS5_VABI _ZN15AbstractStorage14YoutubeContent5WriteEPKvlPl();
int APS5_VABI _ZN15AbstractStorage14YoutubeContent6RemoveEv(void*);
int APS5_VABI _ZN15AbstractStorage14YoutubeContent8SeekByteEliPl();
int APS5_VABI _ZN15AbstractStorage14YoutubeContent8SeekTimeElPl(void*, std::int64_t, std::int64_t*);
int APS5_VABI _ZN15AbstractStorage14YoutubeContentD0Ev();
int APS5_VABI _ZN15AbstractStorage14YoutubeContentD1Ev();
int APS5_VABI _ZN15AbstractStorage14YoutubeContentD2Ev();
int APS5_VABI _ZN15AbstractStorage14YoutubeService13SearchStorageEv();
int APS5_VABI _ZN15AbstractStorage14YoutubeService4StopEv();
int APS5_VABI _ZN15AbstractStorage14YoutubeService5StartEv();
int APS5_VABI _ZN15AbstractStorage14YoutubeServiceC1Ev();
int APS5_VABI _ZN15AbstractStorage14YoutubeServiceC2Ev();
int APS5_VABI _ZN15AbstractStorage14YoutubeServiceD0Ev();
int APS5_VABI _ZN15AbstractStorage14YoutubeServiceD1Ev();
int APS5_VABI _ZN15AbstractStorage14YoutubeServiceD2Ev();
int APS5_VABI _ZN15AbstractStorage14YoutubeStorage14GetStorageSizeEPlS1_S1_(void*, std::int64_t*, std::int64_t*, std::int64_t*);
int APS5_VABI _ZN15AbstractStorage14YoutubeStorageD0Ev();
int APS5_VABI _ZN15AbstractStorage14YoutubeStorageD1Ev();
int APS5_VABI _ZN15AbstractStorage14YoutubeStorageD2Ev();
int APS5_VABI _ZNK15AbstractStorage13YoutubeFolder10GetServiceEv(const void*);
std::uint64_t APS5_VABI _ZNK15AbstractStorage13YoutubeFolder13GetCapabilityEv(const void*);
int APS5_VABI _ZNK15AbstractStorage13YoutubeFolder7GetStatEPNS_4StatE(const void*, void*);
int APS5_VABI _ZNK15AbstractStorage14YoutubeContent10GetServiceEv(const void*);
std::uint64_t APS5_VABI _ZNK15AbstractStorage14YoutubeContent13GetCapabilityEv(const void*);
int APS5_VABI _ZNK15AbstractStorage14YoutubeContent7GetStatEPNS_4StatE(const void*, void*);
int APS5_VABI _ZNK15AbstractStorage14YoutubeService14GetServiceTypeEv(const void*);
int APS5_VABI _ZNK15AbstractStorage14YoutubeStorage10GetServiceEv(const void*);
std::uint64_t APS5_VABI _ZNK15AbstractStorage14YoutubeStorage13GetCapabilityEv(const void*);
int APS5_VABI _ZNK15AbstractStorage14YoutubeStorage7GetStatEPNS_4StatE(const void*, void*);
int APS5_VABI _ZN15AbstractStorage10YoutubeAPI11SendContentEPKvl();
int APS5_VABI _ZN15AbstractStorage10YoutubeAPI14SetAccessTokenERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE();
int APS5_VABI _ZN15AbstractStorage10YoutubeAPI17GetUploadLocationERKSbIcSt11char_traitsIcENS_8StlAllocIcEEEmS7_RS5_();
int APS5_VABI _ZN15AbstractStorage10YoutubeAPI5StartERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE();
int APS5_VABI _ZN15AbstractStorage10YoutubeAPI6ResumeERSbIcSt11char_traitsIcENS_8StlAllocIcEEERlm();
int APS5_VABI _ZN15AbstractStorage10YoutubeAPI7SuspendEv();
int APS5_VABI _ZN15AbstractStorage10YoutubeAPI8initHttpEv();
int APS5_VABI _ZN15AbstractStorage10YoutubeAPI8termHttpEv();
int APS5_VABI _ZN15AbstractStorage10YoutubeAPIC1Ev();
int APS5_VABI _ZN15AbstractStorage10YoutubeAPIC2Ev();
int APS5_VABI _ZN15AbstractStorage10YoutubeAPID1Ev();
int APS5_VABI _ZN15AbstractStorage10YoutubeAPID2Ev();
int APS5_VABI _ZN15AbstractStorage13YoutubeFolder10NewContentERKSbIcSt11char_traitsIcENS_8StlAllocIcEEEPSt10shared_ptrINS_7ContentEE();
int APS5_VABI _ZN15AbstractStorage13YoutubeFolder12RemoveFolderERKSbIcSt11char_traitsIcENS_8StlAllocIcEEEb();
int APS5_VABI _ZN15AbstractStorage13YoutubeFolder13RemoveContentERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE();
int APS5_VABI _ZN15AbstractStorage13YoutubeFolder4OpenENS_8ItemTypeE();
int APS5_VABI _ZN15AbstractStorage13YoutubeFolder5CloseEv();

}
