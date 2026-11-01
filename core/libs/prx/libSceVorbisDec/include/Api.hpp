#pragma once

#include "prx/libSceVorbisDec/include/GuestVorbisFile.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {

int APS5_VABI sceVorbisDecOpenCallbacks(void* datasource, void* vf, const char* initial, std::int64_t ibytes, VorbisDec::GuestCallbacks callbacks);
int APS5_VABI sceVorbisDecClear(void* vf);
std::int64_t APS5_VABI sceVorbisDecBitrate(void* vf, int link);
std::int64_t APS5_VABI sceVorbisDecBitrateInstant(void* vf);
VorbisDec::GuestComment* APS5_VABI sceVorbisDecComment(void* vf, int link);
int APS5_VABI sceVorbisDecCrossLap(void* first, void* second);
int APS5_VABI sceVorbisDecGetDecodePriority(void);
int APS5_VABI sceVorbisDecGetLastDecodeError(void);
int APS5_VABI sceVorbisDecHalfrate(void* vf, int flag);
int APS5_VABI sceVorbisDecHalfrateP(void* vf);
VorbisDec::GuestInfo* APS5_VABI sceVorbisDecInfo(void* vf, int link);
int APS5_VABI sceVorbisDecPcmSeek(void* vf, std::int64_t position);
int APS5_VABI sceVorbisDecPcmSeekLap(void* vf, std::int64_t position);
int APS5_VABI sceVorbisDecPcmSeekPage(void* vf, std::int64_t position);
int APS5_VABI sceVorbisDecPcmSeekPageLap(void* vf, std::int64_t position);
std::int64_t APS5_VABI sceVorbisDecPcmTell(void* vf);
std::int64_t APS5_VABI sceVorbisDecPcmTotal(void* vf, int link);
int APS5_VABI sceVorbisDecRawSeek(void* vf, std::int64_t position);
int APS5_VABI sceVorbisDecRawSeekLap(void* vf, std::int64_t position);
std::int64_t APS5_VABI sceVorbisDecRawTell(void* vf);
std::int64_t APS5_VABI sceVorbisDecRawTotal(void* vf, int link);
std::int64_t APS5_VABI sceVorbisDecRead(void* vf, char* buffer, int length, int bigEndian, int word, int isSigned, int* bitstream);
std::int64_t APS5_VABI sceVorbisDecReadFilter(void* vf, char* buffer, int length, int bigEndian, int word, int isSigned, int* bitstream, VorbisDec::GuestFilter filter, void* filterParam);
std::int64_t APS5_VABI sceVorbisDecReadFloat(void* vf, float*** pcmChannels, int samples, int* bitstream);
std::int64_t APS5_VABI sceVorbisDecSeekable(void* vf);
std::int64_t APS5_VABI sceVorbisDecSerialNumber(void* vf, int link);
int APS5_VABI sceVorbisDecSetDecodePriority(void);
std::int64_t APS5_VABI sceVorbisDecStreams(void* vf);
int APS5_VABI sceVorbisDecTimeSeek(void* vf, double position);
int APS5_VABI sceVorbisDecTimeSeekLap(void* vf, double position);
int APS5_VABI sceVorbisDecTimeSeekPage(void* vf, double position);
int APS5_VABI sceVorbisDecTimeSeekPageLap(void* vf, double position);
double APS5_VABI sceVorbisDecTimeTell(void* vf);
double APS5_VABI sceVorbisDecTimeTotal(void* vf, int link);

}
