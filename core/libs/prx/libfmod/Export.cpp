#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "SDL.h"

// FMOD_MODE bits (subset, from fmod_common.h) — used to detect memory buffers.
constexpr unsigned FMOD_LOOP_OFF = 0x00000001u;
constexpr unsigned FMOD_LOOP_NORMAL = 0x00000002u;
constexpr unsigned FMOD_LOOP_BIDI = 0x00000004u;
constexpr unsigned FMOD_2D_FLAG = 0x00000008u;
constexpr unsigned FMOD_3D_FLAG = 0x00000010u;
constexpr unsigned FMOD_CREATESTREAM_FLAG = 0x00000080u;
constexpr unsigned FMOD_OPENMEMORY_FLAG = 0x00000800u;
constexpr unsigned FMOD_OPENMEMORY_POINT_FLAG = 0x10000000u;
constexpr unsigned FMOD_OPENRAW_FLAG = 0x00001000u;
constexpr unsigned FMOD_NONBLOCKING_FLAG = 0x00010000u;

enum FMOD_OUTPUTTYPE {
    FMOD_OUTPUTTYPE_AUTODETECT = 0,
    FMOD_OUTPUTTYPE_UNKNOWN = 1,
    FMOD_OUTPUTTYPE_NOSOUND = 2,
    FMOD_OUTPUTTYPE_WAVWRITER = 3,
    FMOD_OUTPUTTYPE_NOSOUND_NRT = 4,
    FMOD_OUTPUTTYPE_WAVWRITER_NRT = 5,
    FMOD_OUTPUTTYPE_WASAPI = 6,
    FMOD_OUTPUTTYPE_ASIO = 7,
    FMOD_OUTPUTTYPE_PULSEAUDIO = 8,
    FMOD_OUTPUTTYPE_ALSA = 9,
    FMOD_OUTPUTTYPE_COREAUDIO = 10,
    FMOD_OUTPUTTYPE_AUDIOTRACK = 11,
    FMOD_OUTPUTTYPE_OPENSL = 12,
    FMOD_OUTPUTTYPE_AUDIOOUT = 13,
    FMOD_OUTPUTTYPE_AUDIO3D = 14,
    FMOD_OUTPUTTYPE_MAX = 15
};

enum FMOD_SPEAKERMODE {
    FMOD_SPEAKERMODE_DEFAULT = 0,
    FMOD_SPEAKERMODE_RAW = 1,
    FMOD_SPEAKERMODE_MONO = 2,
    FMOD_SPEAKERMODE_STEREO = 3,
    FMOD_SPEAKERMODE_QUAD = 4,
    FMOD_SPEAKERMODE_SURROUND = 5,
    FMOD_SPEAKERMODE_5POINT1 = 6,
    FMOD_SPEAKERMODE_7POINT1 = 7,
    FMOD_SPEAKERMODE_7POINT1POINT4 = 8,
    FMOD_SPEAKERMODE_MAX = 9
};

enum FMOD_SOUND_TYPE {
    FMOD_SOUND_TYPE_UNKNOWN = 0,
    FMOD_SOUND_TYPE_AIFF = 1,
    FMOD_SOUND_TYPE_ASF = 2,
    FMOD_SOUND_TYPE_DLS = 3,
    FMOD_SOUND_TYPE_FLAC = 4,
    FMOD_SOUND_TYPE_FSB = 5,
    FMOD_SOUND_TYPE_IT = 6,
    FMOD_SOUND_TYPE_MIDI = 7,
    FMOD_SOUND_TYPE_MOD = 8,
    FMOD_SOUND_TYPE_MPEG = 9,
    FMOD_SOUND_TYPE_OGGVORBIS = 10,
    FMOD_SOUND_TYPE_PLAYLIST = 11,
    FMOD_SOUND_TYPE_RAW = 12,
    FMOD_SOUND_TYPE_S3M = 13,
    FMOD_SOUND_TYPE_USER = 14,
    FMOD_SOUND_TYPE_XM = 15,
    FMOD_SOUND_TYPE_XMA = 16,
    FMOD_SOUND_TYPE_AUDIOQUEUE = 17,
    FMOD_SOUND_TYPE_AT9 = 18,
    FMOD_SOUND_TYPE_VB = 19,
    FMOD_SOUND_TYPE_MAX = 20
};

enum FMOD_SOUND_FORMAT {
    FMOD_SOUND_FORMAT_NONE = 0,
    FMOD_SOUND_FORMAT_PCM8 = 1,
    FMOD_SOUND_FORMAT_PCM16 = 2,
    FMOD_SOUND_FORMAT_PCM24 = 3,
    FMOD_SOUND_FORMAT_PCM32 = 4,
    FMOD_SOUND_FORMAT_PCMFLOAT = 5,
    FMOD_SOUND_FORMAT_BITSTREAM = 6,
    FMOD_SOUND_FORMAT_MAX = 7
};

enum FMOD_OPENSTATE {
    FMOD_OPENSTATE_READY = 0,
    FMOD_OPENSTATE_LOADING = 1,
    FMOD_OPENSTATE_ERROR = 2,
    FMOD_OPENSTATE_CONNECTING = 3,
    FMOD_OPENSTATE_BUFFERING = 4,
    FMOD_OPENSTATE_SEEKING = 5,
    FMOD_OPENSTATE_PLAYING = 6,
    FMOD_OPENSTATE_SETPOSITION = 7,
    FMOD_OPENSTATE_MAX = 8
};

enum FMOD_SOUNDGROUP_BEHAVIOR {
    FMOD_SOUNDGROUP_BEHAVIOR_FAIL = 0,
    FMOD_SOUNDGROUP_BEHAVIOR_MUTE = 1,
    FMOD_SOUNDGROUP_BEHAVIOR_STEALLOWEST = 2,
    FMOD_SOUNDGROUP_BEHAVIOR_MAX = 3
};

enum FMOD_DSP_TYPE {
    FMOD_DSP_TYPE_UNKNOWN = 0,
    FMOD_DSP_TYPE_MIXER = 1,
    FMOD_DSP_TYPE_OSCILLATOR = 2,
    FMOD_DSP_TYPE_LOWPASS = 3,
    FMOD_DSP_TYPE_ITLOWPASS = 4,
    FMOD_DSP_TYPE_HIGHPASS = 5,
    FMOD_DSP_TYPE_ECHO = 6,
    FMOD_DSP_TYPE_FADER = 7,
    FMOD_DSP_TYPE_FLANGE = 8,
    FMOD_DSP_TYPE_DISTORTION = 9,
    FMOD_DSP_TYPE_NORMALIZE = 10,
    FMOD_DSP_TYPE_LIMITER = 11,
    FMOD_DSP_TYPE_PARAMEQ = 12,
    FMOD_DSP_TYPE_PITCHSHIFT = 13,
    FMOD_DSP_TYPE_CHORUS = 14,
    FMOD_DSP_TYPE_VSTPLUGIN = 15,
    FMOD_DSP_TYPE_WINAMPPLUGIN = 16,
    FMOD_DSP_TYPE_ITECHO = 17,
    FMOD_DSP_TYPE_COMPRESSOR = 18,
    FMOD_DSP_TYPE_SFXREVERB = 19,
    FMOD_DSP_TYPE_LOWPASS_SIMPLE = 20,
    FMOD_DSP_TYPE_DELAY = 21,
    FMOD_DSP_TYPE_TREMOLO = 22,
    FMOD_DSP_TYPE_SEND = 24,
    FMOD_DSP_TYPE_RETURN = 25,
    FMOD_DSP_TYPE_HIGHPASS_SIMPLE = 26,
    FMOD_DSP_TYPE_PAN = 27,
    FMOD_DSP_TYPE_THREE_EQ = 28,
    FMOD_DSP_TYPE_FFT = 29,
    FMOD_DSP_TYPE_LOUDNESS_METER = 30,
    FMOD_DSP_TYPE_ENVELOPEFOLLOWER = 31,
    FMOD_DSP_TYPE_CONVOLUTIONREVERB = 32,
    FMOD_DSP_TYPE_TRANSCEIVER = 33,
    FMOD_DSP_TYPE_MAX = 34
};

enum FMOD_DSPCONNECTION_TYPE {
    FMOD_DSPCONNECTION_TYPE_STANDARD = 0,
    FMOD_DSPCONNECTION_TYPE_SIDECHAIN = 1,
    FMOD_DSPCONNECTION_TYPE_SEND = 2,
    FMOD_DSPCONNECTION_TYPE_SEND_SIDECHAIN = 3,
    FMOD_DSPCONNECTION_TYPE_MAX = 4
};

enum FMOD_DSP_FFT_WINDOW {
    FMOD_DSP_FFT_WINDOW_RECT = 0,
    FMOD_DSP_FFT_WINDOW_TRIANGLE = 1,
    FMOD_DSP_FFT_WINDOW_HAMMING = 2,
    FMOD_DSP_FFT_WINDOW_HANNING = 3,
    FMOD_DSP_FFT_WINDOW_BLACKMAN = 4,
    FMOD_DSP_FFT_WINDOW_BLACKMANHARRIS = 5,
    FMOD_DSP_FFT_WINDOW_MAX = 6
};

enum FMOD_PLUGINTYPE {
    FMOD_PLUGINTYPE_OUTPUT = 0,
    FMOD_PLUGINTYPE_CODEC = 1,
    FMOD_PLUGINTYPE_DSP = 2,
    FMOD_PLUGINTYPE_MAX = 3
};

enum FMOD_DRIVER_STATE {
    FMOD_DRIVER_STATE_CONNECTED = 1,
    FMOD_DRIVER_STATE_DEFAULT = 2
};

enum FMOD_PORT_TYPE {
    FMOD_PORT_TYPE_MUSIC = 0,
    FMOD_PORT_TYPE_COPY = 1,
    FMOD_PORT_TYPE_SPLIT = 2,
    FMOD_PORT_TYPE_FEEDBACK = 3,
    FMOD_PORT_TYPE_MAX = 4
};

enum FMOD_SYSTEM_CALLBACK_TYPE {
    FMOD_SYSTEM_CALLBACK_DEVICELISTCHANGED = 1,
    FMOD_SYSTEM_CALLBACK_DEVICELOST = 2,
    FMOD_SYSTEM_CALLBACK_MEMORYALLOCATIONFAILED = 4,
    FMOD_SYSTEM_CALLBACK_THREADCREATED = 8,
    FMOD_SYSTEM_CALLBACK_BADDSPCONNECTION = 16,
    FMOD_SYSTEM_CALLBACK_PREMIX = 32,
    FMOD_SYSTEM_CALLBACK_POSTMIX = 64,
    FMOD_SYSTEM_CALLBACK_ERROR = 128,
    FMOD_SYSTEM_CALLBACK_MIDMIX = 256,
    FMOD_SYSTEM_CALLBACK_THREADDESTROYED = 512,
    FMOD_SYSTEM_CALLBACK_PREUPDATE = 1024,
    FMOD_SYSTEM_CALLBACK_POSTUPDATE = 2048,
    FMOD_SYSTEM_CALLBACK_ALL = 0xFFFFFFFF
};

enum FMOD_CHANNELCONTROL_TYPE {
    FMOD_CHANNELCONTROL_CHANNEL = 0,
    FMOD_CHANNELCONTROL_CHANNELGROUP = 1,
    FMOD_CHANNELCONTROL_MAX = 2
};

enum FMOD_CHANNELCONTROL_CALLBACK_TYPE {
    FMOD_CHANNELCONTROL_CALLBACK_END = 0,
    FMOD_CHANNELCONTROL_CALLBACK_VIRTUALVOICE = 1,
    FMOD_CHANNELCONTROL_CALLBACK_SYNCPOINT = 2,
    FMOD_CHANNELCONTROL_CALLBACK_OCCLUSION = 3,
    FMOD_CHANNELCONTROL_CALLBACK_MAX = 4
};

enum FMOD_TAGTYPE {
    FMOD_TAGTYPE_UNKNOWN = 0,
    FMOD_TAGTYPE_ID3V1 = 1,
    FMOD_TAGTYPE_ID3V2 = 2,
    FMOD_TAGTYPE_VORBISCOMMENT = 3,
    FMOD_TAGTYPE_SHOUTCAST = 4,
    FMOD_TAGTYPE_ICECAST = 5,
    FMOD_TAGTYPE_ASF = 6,
    FMOD_TAGTYPE_MIDI = 7,
    FMOD_TAGTYPE_PLAYLIST = 8,
    FMOD_TAGTYPE_FMOD = 9,
    FMOD_TAGTYPE_USER = 10,
    FMOD_TAGTYPE_MAX = 11
};

enum FMOD_TAGDATATYPE {
    FMOD_TAGDATATYPE_BINARY = 0,
    FMOD_TAGDATATYPE_INT = 1,
    FMOD_TAGDATATYPE_FLOAT = 2,
    FMOD_TAGDATATYPE_STRING = 3,
    FMOD_TAGDATATYPE_STRING_UTF16 = 4,
    FMOD_TAGDATATYPE_STRING_UTF16BE = 5,
    FMOD_TAGDATATYPE_STRING_UTF8 = 6,
    FMOD_TAGDATATYPE_CDTOC = 7,
    FMOD_TAGDATATYPE_MAX = 8
};

struct FMOD_GUID {
    unsigned Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char Data4[8];
};

struct FMOD_VECTOR {
    float x;
    float y;
    float z;
};

struct FMOD_3D_ATTRIBUTES {
    FMOD_VECTOR position;
    FMOD_VECTOR velocity;
    FMOD_VECTOR forward;
    FMOD_VECTOR up;
};

struct FMOD_CREATESOUNDEXINFO {
    int cbsize;
    unsigned length;
    unsigned fileoffset;
    int numchannels;
    int defaultfrequency;
    FMOD_SOUND_FORMAT format;
    unsigned decodebuffersize;
    int initialsubsound;
    int numsubsounds;
    void* inclusionlist;
    unsigned inclusionlistnum;
    unsigned char dlsrawdata[8];
    char* encryptionkey;
};

struct FMOD_REVERB_PROPERTIES {
    int Instance;
    float Environment;
    float EnvSize;
    float EnvDiffusion;
    int Room;
    int RoomHF;
    int RoomLF;
    float DecayTime;
    float DecayHFRatio;
    float DecayLFRatio;
    int Reflections;
    float ReflectionsDelay;
    int Reverb;
    float ReverbDelay;
    float EchoTime;
    float EchoDepth;
    float ModulationTime;
    float ModulationDepth;
    float AirAbsorptionHF;
    float HFReference;
    float LFReference;
    float RoomRolloffFactor;
    float Diffusion;
    float Density;
    unsigned Flags;
};

struct FMOD_ADVANCEDSETTINGS {
    int cbSize;
    unsigned maxMPEGCodecs;
    unsigned maxADPCMCodecs;
    unsigned maxXMACodecs;
    unsigned maxVorbisCodecs;
    unsigned maxAT9Codecs;
    unsigned maxFADPCMCodecs;
    unsigned maxPCMCodecs;
    unsigned ASIONumChannels;
    char* ASIOChannelList;
    char* ASIOSpeakerList;
    float vol0virtualvol;
    unsigned defaultDecodeBufferSize;
    unsigned short profilePort;
    unsigned geometryMaxFadeTime;
    float distanceFilterCenterFreq;
    int reverb3Dinstance;
    unsigned DSPBufferPoolSize;
    unsigned stackSizeStream;
    unsigned stackSizeNonBlocking;
    unsigned stackSizeMixer;
    int resamplerMethod;
    unsigned commandQueueSize;
    unsigned handleInitialSize;
};

struct FMOD_CPU_USAGE {
    float dsp;
    float stream;
    float geometry;
    float update;
    float convolution1;
    float convolution2;
};

struct FMOD_TAG {
    FMOD_TAGTYPE type;
    FMOD_TAGDATATYPE datatype;
    char* name;
    void* data;
    unsigned datalen;
    bool updated;
};

struct FMOD_SYNC_POINT {
    void* opaque;
};

struct FMOD_DSP_DESCRIPTION {
    unsigned pluginsdkversion;
    char name[32];
    unsigned version;
    int numinputbuffers;
    int numoutputbuffers;
    void* create;
    void* release;
    void* reset;
    void* read;
    void* process;
    void* setposition;
    int numparameters;
    void* paramdesc;
    void* setparameterfloat;
    void* setparameterint;
    void* setparameterbool;
    void* setparameterdata;
    void* getparameterfloat;
    void* getparameterint;
    void* getparameterbool;
    void* getparameterdata;
    void* shouldiprocess;
    void* userdata;
    void* sys_register;
    void* sys_deregister;
    void* sys_mix;
};

struct FMOD_DSP_PARAMETER_DESC {
    int type;
    char name[16];
    char label[16];
    char* description;
};

struct FMOD_DSP_METERING_INFO {
    int numsamples;
    float peaklevel[32];
    float rmslevel[32];
    short numchannels;
};

struct FMOD_DSP_BUFFER_ARRAY {
    int numbuffers;
    int* buffernumchannels;
    int* bufferchannelmask;
    float** buffers;
    FMOD_SPEAKERMODE speakermode;
};

struct FMOD_DSP_STATE {
    void* instance;
    void* plugindata;
    int channelmask;
    FMOD_SPEAKERMODE source_speakermode;
    float* sidechaindata;
    int sidechainchannels;
    void* callbacks;
    int systemobject;
};

struct FMOD_SYSTEM {
    int unused;
};

struct FMOD_CHANNELCONTROL {
    int unused;
};

struct FMOD_CHANNELGROUP {
    int unused;
};

typedef int (*FMOD_SYSTEM_CALLBACK)(FMOD_SYSTEM* system, FMOD_SYSTEM_CALLBACK_TYPE type, void* commanddata1, void* commanddata2, void* userdata);
typedef int (*FMOD_CHANNELCONTROL_CALLBACK)(FMOD_CHANNELCONTROL* channelcontrol, FMOD_CHANNELCONTROL_TYPE controltype, FMOD_CHANNELCONTROL_CALLBACK_TYPE callbacktype, void* commanddata1, void* commanddata2);

namespace {

std::mutex gFmodMutex;

constexpr int FMOD_OK = 0;
constexpr unsigned FMOD_VERSION_CURRENT = 0x00020206;

// ---- Real-audio backend (SDL queue, mirrors libSceAudioOut) ----
// Silent fallback is kept via ANYPS5_FMOD_SILENT=1 or NOSOUND output type
// or when SDL cannot be opened. When silent, update() is a no-op and
// isPlaying() keeps the previous sticky behaviour so the old smoke test
// (playSound(nullptr) -> playing==true) still passes.
constexpr int kFmodOutRate = 48000;
constexpr int kFmodOutCh = 2;
constexpr int kFmodMixFrames = 1024;

static SDL_AudioDeviceID gFmodDevice = 0;
static SDL_AudioSpec gFmodSpec = {};
static bool gFmodSdlTried = false;
static bool gFmodSilent = false;

static bool FmodSilentRequested() {
    const char* s = std::getenv("ANYPS5_FMOD_SILENT");
    if (s && (s[0] == '1' || s[0] == 'y' || s[0] == 'Y')) return true;
    s = std::getenv("ANYPS5_FMOD_NOSOUND");
    if (s && (s[0] == '1' || s[0] == 'y' || s[0] == 'Y')) return true;
    return false;
}

static bool FmodEnsureDevice() {
    if (gFmodDevice != 0) return true;
    if (gFmodSdlTried) return gFmodDevice != 0;
    gFmodSdlTried = true;
    if (FmodSilentRequested()) {
        gFmodSilent = true;
        return false;
    }
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        gFmodSilent = true;
        return false;
    }
    SDL_AudioSpec desired{};
    desired.freq = kFmodOutRate;
    desired.format = AUDIO_F32SYS;
    desired.channels = static_cast<Uint8>(kFmodOutCh);
    desired.samples = 1024;
    desired.callback = nullptr;
    SDL_AudioSpec obtained{};
    gFmodDevice = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (gFmodDevice == 0) {
        gFmodSilent = true;
        return false;
    }
    gFmodSpec = obtained;
    SDL_PauseAudioDevice(gFmodDevice, 0);
    return true;
}

static void FmodMakeSine(std::vector<float>& out, int rate, int ch, float freqHz, float seconds) {
    const size_t frames = static_cast<size_t>(rate * seconds);
    out.resize(frames * static_cast<size_t>(ch));
    const float phaseInc = 2.0f * 3.14159265358979323846f * freqHz / static_cast<float>(rate);
    float phase = 0.0f;
    for (size_t f = 0; f < frames; f++) {
        const float s = std::sin(phase) * 0.25f;
        phase += phaseInc;
        for (int c = 0; c < ch; c++) out[f * static_cast<size_t>(ch) + static_cast<size_t>(c)] = s;
    }
}

// Minimal WAV (RIFF/WAVE) parser -> float stereo 48k. Returns false if not WAV.
static bool FmodParseWav(const uint8_t* data, size_t len, std::vector<float>& outPcm, int& outRate, int& outCh) {
    if (len < 44 || std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WAVE", 4) != 0) return false;
    uint16_t audioFmt = 0, numCh = 0, bitsPerSample = 0;
    uint32_t sampleRate = 0;
    const uint8_t* snd = nullptr;
    size_t sndLen = 0;
    size_t off = 12;
    auto rd16 = [](const uint8_t* p) -> uint16_t { return static_cast<uint16_t>(p[0] | (p[1] << 8)); };
    auto rd32 = [](const uint8_t* p) -> uint32_t { return static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24)); };
    while (off + 8 <= len) {
        const uint8_t* chunk = data + off;
        uint32_t chunkLen = rd32(chunk + 4);
        if (off + 8 + chunkLen > len) break;
        if (std::memcmp(chunk, "fmt ", 4) == 0 && chunkLen >= 16) {
            audioFmt = rd16(chunk + 8);
            numCh = rd16(chunk + 10);
            sampleRate = rd32(chunk + 12);
            bitsPerSample = rd16(chunk + 22);
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            snd = chunk + 8;
            sndLen = chunkLen;
        }
        off += 8 + ((chunkLen + 1) & ~static_cast<size_t>(1));
    }
    if (!snd || sndLen == 0 || numCh == 0 || sampleRate == 0) return false;
    if (audioFmt != 1 && audioFmt != 3) return false; // PCM or FLOAT
    int bytesPerSamp = bitsPerSample / 8;
    if (bytesPerSamp <= 0) return false;
    size_t frames = sndLen / (static_cast<size_t>(numCh) * static_cast<size_t>(bytesPerSamp));
    if (frames == 0) return false;
    if (frames > 48000u * 30u) frames = 48000u * 30u; // cap 30s
    std::vector<float> tmp;
    tmp.reserve(frames * 2);
    for (size_t f = 0; f < frames; f++) {
        float l = 0.0f, r = 0.0f;
        for (int c = 0; c < numCh && c < 8; c++) {
            const uint8_t* p = snd + (f * numCh + c) * bytesPerSamp;
            float s = 0.0f;
            if (audioFmt == 1) {
                if (bitsPerSample == 8) s = (static_cast<int>(p[0]) - 128) / 128.0f;
                else if (bitsPerSample == 16) { int16_t v; std::memcpy(&v, p, 2); s = v / 32768.0f; }
                else if (bitsPerSample == 24) { int32_t v = (p[0] | (p[1] << 8) | (p[2] << 16)); if (v & 0x800000) v |= ~0xFFFFFF; s = v / 8388608.0f; }
                else if (bitsPerSample == 32) { int32_t v; std::memcpy(&v, p, 4); s = v / 2147483648.0f; }
                else return false;
            } else {
                if (bitsPerSample == 32) { float v; std::memcpy(&v, p, 4); s = v; }
                else return false;
            }
            if (c == 0) l = s;
            else if (c == 1) r = s;
            else { l += s; r += s; }
        }
        if (numCh == 1) r = l;
        tmp.push_back(l);
        tmp.push_back(r);
    }
    // Resample to 48k if needed (linear).
    if (sampleRate != static_cast<uint32_t>(kFmodOutRate)) {
        const size_t inFrames = tmp.size() / 2;
        const size_t outFrames = static_cast<size_t>((static_cast<double>(inFrames) * kFmodOutRate) / sampleRate);
        std::vector<float> rs;
        rs.reserve(outFrames * 2);
        for (size_t i = 0; i < outFrames; i++) {
            double pos = (static_cast<double>(i) * sampleRate) / kFmodOutRate;
            size_t i0 = static_cast<size_t>(pos);
            size_t i1 = i0 + 1 < inFrames ? i0 + 1 : i0;
            float t = static_cast<float>(pos - i0);
            rs.push_back(tmp[i0 * 2] * (1 - t) + tmp[i1 * 2] * t);
            rs.push_back(tmp[i0 * 2 + 1] * (1 - t) + tmp[i1 * 2 + 1] * t);
        }
        outPcm.swap(rs);
    } else {
        outPcm.swap(tmp);
    }
    outRate = kFmodOutRate;
    outCh = 2;
    return true;
}

}

namespace FMOD {

class System;
class ChannelGroup;
class Channel;
class Sound;
class SoundGroup;
class DSP;
class DSPConnection;
class Geometry;
class Reverb3D;

class Sound {
public:
    std::string name;
    unsigned mode = 0;
    unsigned length = 0;
    int loopCount = 0;
    float minDistance = 0.0f;
    float maxDistance = 10000.0f;
    SoundGroup* soundGroup = nullptr;
    void* userData = nullptr;
    // Real-audio decoded PCM (float stereo 48k, interleaved). Empty = silent/placeholder.
    std::vector<float> pcm;
    int pcmRate = kFmodOutRate;
    int pcmCh = 2;
    int APS5_VABI release();
    int APS5_VABI getSystemObject(System** system);
    int APS5_VABI setMode(unsigned mode) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        this->mode = mode;
        return FMOD_OK;
    }
    int APS5_VABI getMode(unsigned* mode) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (mode) *mode = this->mode;
        return FMOD_OK;
    }
    int APS5_VABI set3DMinMaxDistance(float mindistance, float maxdistance) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        minDistance = mindistance;
        maxDistance = maxdistance;
        return FMOD_OK;
    }
    int APS5_VABI getNumSubSounds(int* numsubsounds) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (numsubsounds) *numsubsounds = 0;
        return FMOD_OK;
    }
    int APS5_VABI getLength(unsigned* length, unsigned lengthtype) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (!length) return FMOD_OK;
        if (!pcm.empty()) {
            const unsigned frames = static_cast<unsigned>(pcm.size() / 2);
            // FMOD_TIMEUNIT: MS=1, PCM=2, PCMBYTES=4, RAWBYTES=8
            if (lengthtype == 1) *length = (frames * 1000u) / static_cast<unsigned>(kFmodOutRate);
            else if (lengthtype == 2) *length = frames;
            else if (lengthtype == 4 || lengthtype == 8) *length = frames * 2u * sizeof(float);
            else *length = frames;
            return FMOD_OK;
        }
        *length = this->length;
        return FMOD_OK;
    }
    int APS5_VABI getFormat(FMOD_SOUND_TYPE* type, FMOD_SOUND_FORMAT* format, int* channels, int* bits) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (!pcm.empty()) {
            if (type) *type = FMOD_SOUND_TYPE_UNKNOWN;
            if (format) *format = FMOD_SOUND_FORMAT_PCMFLOAT;
            if (channels) *channels = 2;
            if (bits) *bits = 32;
            return FMOD_OK;
        }
        if (type) *type = FMOD_SOUND_TYPE_UNKNOWN;
        if (format) *format = FMOD_SOUND_FORMAT_PCM16;
        if (channels) *channels = 2;
        if (bits) *bits = 16;
        return FMOD_OK;
    }
    int APS5_VABI getName(char* name, int namelen) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (name && namelen > 0) {
            std::strncpy(name, this->name.c_str(), static_cast<std::size_t>(namelen - 1));
            name[namelen - 1] = '\0';
        }
        return FMOD_OK;
    }
    int APS5_VABI setLoopCount(int loopcount) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        loopCount = loopcount;
        return FMOD_OK;
    }
    int APS5_VABI getLoopCount(int* loopcount) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (loopcount) *loopcount = loopCount;
        return FMOD_OK;
    }
    int APS5_VABI setSoundGroup(SoundGroup* soundgroup) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        soundGroup = soundgroup;
        return FMOD_OK;
    }
    int APS5_VABI getSoundGroup(SoundGroup** soundgroup) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (soundgroup) *soundgroup = soundGroup;
        return FMOD_OK;
    }
    int APS5_VABI getSubSound(int index, Sound** subsound) {
        (void)index;
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (subsound) *subsound = nullptr;
        return FMOD_OK;
    }
    int APS5_VABI setUserData(void* userdata) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        userData = userdata;
        return FMOD_OK;
    }
    int APS5_VABI getUserData(void** userdata) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (userdata) *userdata = userData;
        return FMOD_OK;
    }
    int APS5_VABI getOpenState(FMOD_OPENSTATE* openstate, unsigned* percentbuffered, bool* starving, bool* diskbusy) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (openstate) *openstate = FMOD_OPENSTATE_READY;
        if (percentbuffered) *percentbuffered = 100;
        if (starving) *starving = false;
        if (diskbusy) *diskbusy = false;
        return FMOD_OK;
    }
    int APS5_VABI readData(void* buffer, unsigned length, unsigned* read) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (buffer && length) std::memset(buffer, 0, length);
        if (read) *read = length;
        return FMOD_OK;
    }
    int APS5_VABI seekData(unsigned pcm) {
        (void)pcm;
        return FMOD_OK;
    }
};

class SoundGroup {
public:
    int maxAudible = 0;
    float volume = 1.0f;
    void* userData = nullptr;
    int APS5_VABI release() {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        delete this;
        return FMOD_OK;
    }
    int APS5_VABI setMaxAudible(int maxaudible) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        maxAudible = maxaudible;
        return FMOD_OK;
    }
    int APS5_VABI getMaxAudible(int* maxaudible) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (maxaudible) *maxaudible = maxAudible;
        return FMOD_OK;
    }
    int APS5_VABI setVolume(float volume) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        this->volume = volume;
        return FMOD_OK;
    }
    int APS5_VABI getVolume(float* volume) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (volume) *volume = this->volume;
        return FMOD_OK;
    }
    int APS5_VABI stop() { return FMOD_OK; }
    int APS5_VABI setUserData(void* userdata) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        userData = userdata;
        return FMOD_OK;
    }
    int APS5_VABI getUserData(void** userdata) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (userdata) *userdata = userData;
        return FMOD_OK;
    }
    int APS5_VABI getNumSounds(int* numsounds) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (numsounds) *numsounds = 0;
        return FMOD_OK;
    }
};

class DSPConnection {
public:
    DSP* input = nullptr;
    DSP* output = nullptr;
    void* userData = nullptr;
    int APS5_VABI getInput(DSP** input) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (input) *input = this->input;
        return FMOD_OK;
    }
    int APS5_VABI getOutput(DSP** output) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (output) *output = this->output;
        return FMOD_OK;
    }
    int APS5_VABI getType(FMOD_DSPCONNECTION_TYPE* type) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (type) *type = FMOD_DSPCONNECTION_TYPE_STANDARD;
        return FMOD_OK;
    }
    int APS5_VABI setUserData(void* userdata) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        userData = userdata;
        return FMOD_OK;
    }
    int APS5_VABI getUserData(void** userdata) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (userdata) *userdata = userData;
        return FMOD_OK;
    }
};

class DSP {
public:
    FMOD_DSP_TYPE type = FMOD_DSP_TYPE_UNKNOWN;
    bool active = true;
    bool bypass = false;
    void* userData = nullptr;
    int APS5_VABI release() {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        delete this;
        return FMOD_OK;
    }
    int APS5_VABI setActive(bool active) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        this->active = active;
        return FMOD_OK;
    }
    int APS5_VABI getActive(bool* active) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (active) *active = this->active;
        return FMOD_OK;
    }
    int APS5_VABI setBypass(bool bypass) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        this->bypass = bypass;
        return FMOD_OK;
    }
    int APS5_VABI getBypass(bool* bypass) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (bypass) *bypass = this->bypass;
        return FMOD_OK;
    }
    int APS5_VABI reset() { return FMOD_OK; }
    int APS5_VABI setParameterFloat(int index, float value) {
        (void)index; (void)value;
        return FMOD_OK;
    }
    int APS5_VABI getParameterFloat(int index, float* value) {
        (void)index;
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (value) *value = 0.0f;
        return FMOD_OK;
    }
    int APS5_VABI getType(FMOD_DSP_TYPE* type) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (type) *type = this->type;
        return FMOD_OK;
    }
    int APS5_VABI getInfo(char* name, unsigned* version, int* channels, int* configwidth, int* configheight) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (name) name[0] = '\0';
        if (version) *version = 0;
        if (channels) *channels = 0;
        if (configwidth) *configwidth = 0;
        if (configheight) *configheight = 0;
        return FMOD_OK;
    }
    int APS5_VABI setUserData(void* userdata) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        userData = userdata;
        return FMOD_OK;
    }
    int APS5_VABI getUserData(void** userdata) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (userdata) *userdata = userData;
        return FMOD_OK;
    }
};

class ChannelControl {
public:
    float volume = 1.0f;
    float pitch = 1.0f;
    bool paused = false;
    bool mute = false;
    bool playing = false;
    unsigned mode = 0;
    bool volumeRamp = false;
    void* userData = nullptr;
    // Mixer state for real-audio backend (used when this control is a Channel
    // with decoded PCM; groups keep mixHasData==false and use sticky flag).
    size_t mixCursor = 0;
    size_t mixTotal = 0;
    bool mixHasData = false;
    bool mixLoop = false;
    int APS5_VABI setVolume(float volume) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        this->volume = volume;
        return FMOD_OK;
    }
    int APS5_VABI getVolume(float* volume) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (volume) *volume = this->volume;
        return FMOD_OK;
    }
    int APS5_VABI setPitch(float pitch) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        this->pitch = pitch;
        return FMOD_OK;
    }
    int APS5_VABI getPitch(float* pitch) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (pitch) *pitch = this->pitch;
        return FMOD_OK;
    }
    int APS5_VABI setPaused(bool paused) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        this->paused = paused;
        return FMOD_OK;
    }
    int APS5_VABI getPaused(bool* paused) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (paused) *paused = this->paused;
        return FMOD_OK;
    }
    int APS5_VABI setMute(bool mute) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        this->mute = mute;
        return FMOD_OK;
    }
    int APS5_VABI getMute(bool* mute) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (mute) *mute = this->mute;
        return FMOD_OK;
    }
    int APS5_VABI stop() {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        playing = false;
        return FMOD_OK;
    }
    int APS5_VABI isPlaying(bool* isplaying) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (!isplaying) return FMOD_OK;
        if (mixHasData) {
            *isplaying = playing && !paused && (mixLoop || mixCursor < mixTotal);
        } else {
            *isplaying = playing && !paused;
        }
        return FMOD_OK;
    }
    int APS5_VABI setMode(unsigned mode) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        this->mode = mode;
        return FMOD_OK;
    }
    int APS5_VABI getMode(unsigned* mode) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (mode) *mode = this->mode;
        return FMOD_OK;
    }
    int APS5_VABI addFadePoint(unsigned long long dspclock, float volume) {
        (void)dspclock; (void)volume;
        return FMOD_OK;
    }
    int APS5_VABI setDelay(unsigned long long dspclock_start, unsigned long long dspclock_end, bool stopchannels) {
        (void)dspclock_start; (void)dspclock_end; (void)stopchannels;
        return FMOD_OK;
    }
    int APS5_VABI getDelay(unsigned long long* dspclock_start, unsigned long long* dspclock_end, bool* stopchannels) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (dspclock_start) *dspclock_start = 0;
        if (dspclock_end) *dspclock_end = 0;
        if (stopchannels) *stopchannels = false;
        return FMOD_OK;
    }
    int APS5_VABI setVolumeRamp(bool ramp) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        volumeRamp = ramp;
        return FMOD_OK;
    }
    int APS5_VABI getVolumeRamp(bool* ramp) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (ramp) *ramp = volumeRamp;
        return FMOD_OK;
    }
    int APS5_VABI setCallback(FMOD_CHANNELCONTROL_CALLBACK callback) {
        (void)callback;
        return FMOD_OK;
    }
    int APS5_VABI isVirtual(bool* isvirtual) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (isvirtual) *isvirtual = false;
        return FMOD_OK;
    }
    int APS5_VABI getIndex(int* index) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (index) *index = 0;
        return FMOD_OK;
    }
    int APS5_VABI getSystemObject(System** system);
    int APS5_VABI setUserData(void* userdata) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        userData = userdata;
        return FMOD_OK;
    }
    int APS5_VABI getUserData(void** userdata) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (userdata) *userdata = userData;
        return FMOD_OK;
    }
    int APS5_VABI set3DAttributes(const FMOD_VECTOR* pos, const FMOD_VECTOR* vel) {
        (void)pos; (void)vel;
        return FMOD_OK;
    }
    int APS5_VABI setLowPassGain(float gain) {
        (void)gain;
        return FMOD_OK;
    }
    int APS5_VABI getLowPassGain(float* gain) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (gain) *gain = 1.0f;
        return FMOD_OK;
    }
    int APS5_VABI setPan(float pan) {
        (void)pan;
        return FMOD_OK;
    }
    int APS5_VABI getDSPClock(unsigned long long* dspclock, unsigned long long* parentclock) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (dspclock) *dspclock = 0;
        if (parentclock) *parentclock = 0;
        return FMOD_OK;
    }
    int APS5_VABI getDSP(int index, DSP** dsp) {
        (void)index;
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (dsp) *dsp = nullptr;
        return FMOD_OK;
    }
};

class Channel : public ChannelControl {
public:
    Sound* sound = nullptr;
    ChannelGroup* group = nullptr;
    float frequency = 48000.0f;
    int priority = 128;
    unsigned position = 0;
    int loopCount = 0;
    System* owner = nullptr;
    int APS5_VABI setPriority(int priority) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        this->priority = priority;
        return FMOD_OK;
    }
    int APS5_VABI getPriority(int* priority) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (priority) *priority = this->priority;
        return FMOD_OK;
    }
    int APS5_VABI setFrequency(float frequency) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        this->frequency = frequency;
        return FMOD_OK;
    }
    int APS5_VABI getFrequency(float* frequency) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (frequency) *frequency = this->frequency;
        return FMOD_OK;
    }
    int APS5_VABI setPosition(unsigned position, unsigned postype) {
        (void)postype;
        std::lock_guard<std::mutex> lock(gFmodMutex);
        this->position = position;
        // Keep mixer cursor in sync (PCM frames). postype==2 (PCM) assumed.
        mixCursor = static_cast<size_t>(position);
        if (mixHasData && mixCursor > mixTotal) mixCursor = mixTotal;
        return FMOD_OK;
    }
    int APS5_VABI getPosition(unsigned* position, unsigned postype) {
        (void)postype;
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (position) *position = static_cast<unsigned>(mixHasData ? mixCursor : this->position);
        return FMOD_OK;
    }
    int APS5_VABI setChannelGroup(ChannelGroup* channelgroup) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        group = channelgroup;
        return FMOD_OK;
    }
    int APS5_VABI getChannelGroup(ChannelGroup** channelgroup) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (channelgroup) *channelgroup = group;
        return FMOD_OK;
    }
    int APS5_VABI setLoopCount(int loopcount) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        loopCount = loopcount;
        return FMOD_OK;
    }
    int APS5_VABI getCurrentSound(Sound** sound) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (sound) *sound = this->sound;
        return FMOD_OK;
    }
};

class ChannelGroup : public ChannelControl {
public:
    std::string name;
    int APS5_VABI release() {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        delete this;
        return FMOD_OK;
    }
    int APS5_VABI addGroup(ChannelGroup* group, bool propagatedspclock, DSPConnection** connection) {
        (void)group; (void)propagatedspclock;
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (connection) *connection = nullptr;
        return FMOD_OK;
    }
    int APS5_VABI getNumGroups(int* numgroups) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (numgroups) *numgroups = 0;
        return FMOD_OK;
    }
    int APS5_VABI getGroup(int index, ChannelGroup** group) {
        (void)index;
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (group) *group = nullptr;
        return FMOD_OK;
    }
    int APS5_VABI getParentGroup(ChannelGroup** group) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (group) *group = nullptr;
        return FMOD_OK;
    }
    int APS5_VABI getName(char* name, int namelen) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (name && namelen > 0) {
            std::strncpy(name, this->name.c_str(), static_cast<std::size_t>(namelen - 1));
            name[namelen - 1] = '\0';
        }
        return FMOD_OK;
    }
    int APS5_VABI getNumChannels(int* numchannels) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (numchannels) *numchannels = 0;
        return FMOD_OK;
    }
    int APS5_VABI getChannel(int index, Channel** channel) {
        (void)index;
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (channel) *channel = nullptr;
        return FMOD_OK;
    }
};

class Geometry {
public:
    void* userData = nullptr;
    int APS5_VABI release() {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        delete this;
        return FMOD_OK;
    }
    int APS5_VABI addPolygon(float directocclusion, float reverbocclusion, bool doublesided, int numvertices, const FMOD_VECTOR* vertices, int* polygonindex) {
        (void)directocclusion; (void)reverbocclusion; (void)doublesided; (void)numvertices; (void)vertices;
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (polygonindex) *polygonindex = 0;
        return FMOD_OK;
    }
    int APS5_VABI setActive(bool active) {
        (void)active;
        return FMOD_OK;
    }
    int APS5_VABI setUserData(void* userdata) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        userData = userdata;
        return FMOD_OK;
    }
    int APS5_VABI getUserData(void** userdata) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (userdata) *userdata = userData;
        return FMOD_OK;
    }
};

class Reverb3D {
public:
    void* userData = nullptr;
    int APS5_VABI release() {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        delete this;
        return FMOD_OK;
    }
    int APS5_VABI set3DAttributes(const FMOD_VECTOR* pos, float mindistance, float maxdistance) {
        (void)pos; (void)mindistance; (void)maxdistance;
        return FMOD_OK;
    }
    int APS5_VABI setActive(bool active) {
        (void)active;
        return FMOD_OK;
    }
    int APS5_VABI setUserData(void* userdata) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        userData = userdata;
        return FMOD_OK;
    }
    int APS5_VABI getUserData(void** userdata) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (userdata) *userdata = userData;
        return FMOD_OK;
    }
};

static std::vector<Channel*> gFmodChannels;

static int APS5_VABI SoundReleaseImpl(Sound* self) {
    std::lock_guard<std::mutex> lock(gFmodMutex);
    for (Channel* ch : gFmodChannels) {
        if (ch && ch->sound == self) {
            ch->playing = false;
            ch->sound = nullptr;
            ch->mixHasData = false;
        }
    }
    delete self;
    return FMOD_OK;
}

int APS5_VABI Sound::release() { return SoundReleaseImpl(this); }

class System {
public:
    bool initialized = false;
    int maxChannels = 0;
    unsigned initFlags = 0;
    int driver = 0;
    FMOD_OUTPUTTYPE output = FMOD_OUTPUTTYPE_AUTODETECT;
    int softwareChannels = 0;
    int numListeners = 1;
    float dopplerScale = 1.0f;
    float distanceFactor = 1.0f;
    float rolloffScale = 1.0f;
    void* userData = nullptr;
    ChannelGroup* masterChannelGroup = nullptr;
    SoundGroup* masterSoundGroup = nullptr;
    static int APS5_VABI create(System** system) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (!system) return FMOD_OK;
        *system = new System();
        return FMOD_OK;
    }
    static int APS5_VABI create(System** system, unsigned headerversion) {
        (void)headerversion;
        return create(system);
    }
    int APS5_VABI release() {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        // Drop channels owned by this system (game holds raw pointers; mark
        // them stopped and free to avoid unbounded growth).
        for (auto it = gFmodChannels.begin(); it != gFmodChannels.end();) {
            if (*it && (*it)->owner == this) {
                delete *it;
                it = gFmodChannels.erase(it);
            } else {
                ++it;
            }
        }
        delete masterChannelGroup;
        delete masterSoundGroup;
        masterChannelGroup = nullptr;
        masterSoundGroup = nullptr;
        delete this;
        return FMOD_OK;
    }
    int APS5_VABI init(int maxchannels, unsigned flags, void* extradriverdata) {
        (void)extradriverdata;
        std::lock_guard<std::mutex> lock(gFmodMutex);
        maxChannels = maxchannels;
        initFlags = flags;
        initialized = true;
        if (!masterChannelGroup) masterChannelGroup = new ChannelGroup();
        if (!masterSoundGroup) masterSoundGroup = new SoundGroup();
        // Real-audio: try SDL unless NOSOUND output or env requests silent.
        // Keep silent backend as fallback (no failure returned).
        bool wantSilent = FmodSilentRequested() ||
            output == FMOD_OUTPUTTYPE_NOSOUND ||
            output == FMOD_OUTPUTTYPE_NOSOUND_NRT;
        if (!wantSilent) {
            // Unlock during SDL open (SDL may call back)? FmodEnsureDevice uses
            // no FMOD lock internally except SDL, so we can call with lock held
            // for simplicity; SDL_OpenAudioDevice does not call into FMOD.
            if (!FmodEnsureDevice()) {
                APS5_LOG_ERR("%s", "FMOD: SDL audio unavailable, silent fallback");
            } else {
                APS5_LOG_OUT("%s", "FMOD: SDL audio output active (48k stereo)");
            }
        }
        return FMOD_OK;
    }
    int APS5_VABI close() {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        initialized = false;
        return FMOD_OK;
    }
    int APS5_VABI update() {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (!initialized) return FMOD_OK;
        if (gFmodSilent || gFmodDevice == 0) return FMOD_OK; // silent fallback
        // Throttle like libSceAudioOut: avoid unbounded queue growth.
        Uint32 queued = SDL_GetQueuedAudioSize(gFmodDevice);
        const Uint32 oneBuf = static_cast<Uint32>(kFmodMixFrames * kFmodOutCh * sizeof(float));
        if (queued > oneBuf * 8u) return FMOD_OK;
        float mix[kFmodMixFrames * 2] = {};
        bool anyActive = false;
        for (Channel* ch : gFmodChannels) {
            if (!ch || ch->owner != this) continue;
            if (!ch->playing || ch->paused || ch->mute) continue;
            if (!ch->mixHasData || !ch->sound || ch->sound->pcm.empty()) continue;
            Sound* s = ch->sound;
            const size_t total = ch->mixTotal;
            if (total == 0) continue;
            size_t cur = ch->mixCursor;
            const float vol = ch->volume;
            if (vol <= 0.0f) {
                // Still advance cursor so silent channels finish.
                size_t adv = kFmodMixFrames;
                if (!ch->mixLoop) {
                    if (cur + adv >= total) { cur = total; ch->playing = false; }
                    else cur += adv;
                } else {
                    cur = (cur + adv) % (total ? total : 1);
                }
                ch->mixCursor = cur;
                ch->position = static_cast<unsigned>(cur);
                continue;
            }
            anyActive = true;
            const float* pcm = s->pcm.data();
            size_t pos = cur;
            for (int i = 0; i < kFmodMixFrames; i++) {
                if (pos >= total) {
                    if (ch->mixLoop || s->loopCount != 0 ||
                        (s->mode & FMOD_LOOP_NORMAL) != 0) {
                        pos = 0;
                    } else {
                        break;
                    }
                }
                mix[i * 2] += pcm[pos * 2] * vol;
                mix[i * 2 + 1] += pcm[pos * 2 + 1] * vol;
                pos++;
            }
            // Advance cursor by frames consumed (approx, ignoring loop wrap details above).
            size_t adv = static_cast<size_t>(kFmodMixFrames);
            if (!ch->mixLoop && s->loopCount == 0 && (s->mode & FMOD_LOOP_NORMAL) == 0) {
                if (cur + adv >= total) {
                    cur = total;
                    ch->playing = false;
                } else {
                    cur += adv;
                }
            } else {
                cur = total ? ((cur + adv) % total) : 0;
            }
            ch->mixCursor = cur;
            ch->position = static_cast<unsigned>(cur);
        }
        if (!anyActive) {
            // Still queue silence rarely to keep device alive? No — skip to
            // avoid filling queue with silence when nothing plays.
            return FMOD_OK;
        }
        for (int i = 0; i < kFmodMixFrames * 2; i++) {
            if (mix[i] > 1.0f) mix[i] = 1.0f;
            else if (mix[i] < -1.0f) mix[i] = -1.0f;
        }
        // Convert to device format if needed.
        if (gFmodSpec.format == AUDIO_F32SYS || gFmodSpec.format == 0) {
            if (SDL_QueueAudio(gFmodDevice, mix, sizeof(mix)) < 0) {
                APS5_LOG_ERR("%s", "FMOD: SDL_QueueAudio failed");
            }
        } else {
            int16_t tmp[kFmodMixFrames * 2];
            for (int i = 0; i < kFmodMixFrames * 2; i++) {
                float v = mix[i] * 32767.0f;
                if (v > 32767.0f) v = 32767.0f;
                if (v < -32768.0f) v = -32768.0f;
                tmp[i] = static_cast<int16_t>(v);
            }
            // Resample/format convert via SDL if spec differs (channels/rate).
            // For minimal path, queue directly when spec is S16 stereo 48k;
            // otherwise use SDL_ConvertAudio.
            if (gFmodSpec.channels == 2 && gFmodSpec.freq == kFmodOutRate &&
                (gFmodSpec.format == AUDIO_S16SYS)) {
                SDL_QueueAudio(gFmodDevice, tmp, sizeof(tmp));
            } else {
                SDL_AudioCVT cvt{};
                if (SDL_BuildAudioCVT(&cvt, AUDIO_F32SYS, 2, kFmodOutRate,
                                      gFmodSpec.format, gFmodSpec.channels, gFmodSpec.freq) >= 0) {
                    std::vector<uint8_t> buf(sizeof(mix) * 4u);
                    std::memcpy(buf.data(), mix, sizeof(mix));
                    cvt.buf = buf.data();
                    cvt.len = static_cast<int>(sizeof(mix));
                    if (SDL_ConvertAudio(&cvt) == 0) {
                        SDL_QueueAudio(gFmodDevice, cvt.buf, static_cast<Uint32>(cvt.len_cvt));
                    }
                }
            }
        }
        return FMOD_OK;
    }
    int APS5_VABI setOutput(FMOD_OUTPUTTYPE output) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        this->output = output;
        if (output == FMOD_OUTPUTTYPE_NOSOUND || output == FMOD_OUTPUTTYPE_NOSOUND_NRT) {
            gFmodSilent = true;
        }
        return FMOD_OK;
    }
    int APS5_VABI getOutput(FMOD_OUTPUTTYPE* output) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (output) *output = this->output;
        return FMOD_OK;
    }
    int APS5_VABI getNumDrivers(int* numdrivers) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (numdrivers) *numdrivers = 1;
        return FMOD_OK;
    }
    int APS5_VABI getDriverInfo(int id, char* name, int namelen, FMOD_GUID* guid, int* systemrate, FMOD_SPEAKERMODE* speakermode, int* speakermodechannels) {
        (void)id;
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (name && namelen > 0) {
            const char* driverName = (gFmodDevice != 0 && !gFmodSilent) ? "AnyPS5 FMOD Output" : "AnyPS5 Null Output";
            std::strncpy(name, driverName, static_cast<std::size_t>(namelen - 1));
            name[namelen - 1] = '\0';
        }
        if (guid) std::memset(guid, 0, sizeof(*guid));
        if (systemrate) *systemrate = 48000;
        if (speakermode) *speakermode = FMOD_SPEAKERMODE_STEREO;
        if (speakermodechannels) *speakermodechannels = 2;
        return FMOD_OK;
    }
    int APS5_VABI setDriver(int driver) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        this->driver = driver;
        return FMOD_OK;
    }
    int APS5_VABI getDriver(int* driver) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (driver) *driver = this->driver;
        return FMOD_OK;
    }
    int APS5_VABI setSoftwareChannels(int numsoftwarechannels) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        softwareChannels = numsoftwarechannels;
        return FMOD_OK;
    }
    int APS5_VABI getSoftwareChannels(int* numsoftwarechannels) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (numsoftwarechannels) *numsoftwarechannels = softwareChannels;
        return FMOD_OK;
    }
    int APS5_VABI setSoftwareFormat(int samplerate, FMOD_SPEAKERMODE speakermode, int numrawspeakers) {
        (void)samplerate; (void)speakermode; (void)numrawspeakers;
        return FMOD_OK;
    }
    int APS5_VABI getSoftwareFormat(int* samplerate, FMOD_SPEAKERMODE* speakermode, int* numrawspeakers) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (samplerate) *samplerate = 48000;
        if (speakermode) *speakermode = FMOD_SPEAKERMODE_STEREO;
        if (numrawspeakers) *numrawspeakers = 0;
        return FMOD_OK;
    }
    int APS5_VABI setDSPBufferSize(unsigned bufferlength, int numbuffers) {
        (void)bufferlength; (void)numbuffers;
        return FMOD_OK;
    }
    int APS5_VABI getDSPBufferSize(unsigned* bufferlength, int* numbuffers) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (bufferlength) *bufferlength = 1024;
        if (numbuffers) *numbuffers = 4;
        return FMOD_OK;
    }
    int APS5_VABI setAdvancedSettings(FMOD_ADVANCEDSETTINGS* settings) {
        (void)settings;
        return FMOD_OK;
    }
    int APS5_VABI getAdvancedSettings(FMOD_ADVANCEDSETTINGS* settings) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (settings) std::memset(settings, 0, sizeof(*settings));
        return FMOD_OK;
    }
    int APS5_VABI setCallback(FMOD_SYSTEM_CALLBACK callback, FMOD_SYSTEM_CALLBACK_TYPE callbackmask) {
        (void)callback; (void)callbackmask;
        return FMOD_OK;
    }
    int APS5_VABI setFileSystem(void* useropen, void* userclose, void* userread, void* userseek, void* userasyncread, void* userasynccancel, int blockalign) {
        (void)useropen; (void)userclose; (void)userread; (void)userseek;
        (void)userasyncread; (void)userasynccancel; (void)blockalign;
        return FMOD_OK;
    }
    int APS5_VABI setPluginPath(const char* pluginpath) {
        (void)pluginpath;
        return FMOD_OK;
    }
    int APS5_VABI loadPlugin(const char* filename, unsigned* handle, FMOD_PLUGINTYPE plugintype) {
        (void)filename; (void)plugintype;
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (handle) *handle = 0;
        return FMOD_OK;
    }
    int APS5_VABI getVersion(unsigned* version) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (version) *version = FMOD_VERSION_CURRENT;
        return FMOD_OK;
    }
    int APS5_VABI getOutputHandle(void** handle) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (handle) *handle = nullptr;
        return FMOD_OK;
    }
    int APS5_VABI getChannelsPlaying(int* channels, int* realchannels) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        int active = 0;
        for (Channel* ch : gFmodChannels) {
            if (!ch || ch->owner != this) continue;
            if (!ch->playing || ch->paused) continue;
            if (ch->mixHasData) {
                if (ch->mixLoop || ch->mixCursor < ch->mixTotal) active++;
            }
            // Null/empty sounds (smoke test) are not counted to preserve old
            // behaviour (previously always returned 0).
        }
        if (channels) *channels = active;
        if (realchannels) *realchannels = active;
        return FMOD_OK;
    }
    int APS5_VABI getCPUUsage(FMOD_CPU_USAGE* usage) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (usage) std::memset(usage, 0, sizeof(*usage));
        return FMOD_OK;
    }
    int APS5_VABI getFileUsage(long long* samplebytesread, long long* streambytesread, long long* otherbytesread) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (samplebytesread) *samplebytesread = 0;
        if (streambytesread) *streambytesread = 0;
        if (otherbytesread) *otherbytesread = 0;
        return FMOD_OK;
    }
    int APS5_VABI getSpectrum(float* spectrumarray, int numvalues, int channeloffset, FMOD_DSP_FFT_WINDOW window) {
        (void)channeloffset; (void)window;
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (spectrumarray && numvalues > 0) std::memset(spectrumarray, 0, sizeof(float) * static_cast<std::size_t>(numvalues));
        return FMOD_OK;
    }
    int APS5_VABI getWaveData(float* wavearray, int numvalues, int channeloffset) {
        (void)channeloffset;
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (wavearray && numvalues > 0) std::memset(wavearray, 0, sizeof(float) * static_cast<std::size_t>(numvalues));
        return FMOD_OK;
    }
    int APS5_VABI createSound(const char* name_or_data, unsigned mode, FMOD_CREATESOUNDEXINFO* exinfo, Sound** sound) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (!sound) return FMOD_OK;
        Sound* created = new Sound();
        created->mode = mode;
        unsigned exLen = exinfo ? exinfo->length : 0;
        if (exLen) created->length = exLen;
        const bool isMem = (mode & (FMOD_OPENMEMORY_FLAG | FMOD_OPENMEMORY_POINT_FLAG)) != 0;
        const bool isRaw = (mode & FMOD_OPENRAW_FLAG) != 0;
        if (isMem && name_or_data && exLen > 0 && exLen < (1u << 30)) {
            const uint8_t* buf = reinterpret_cast<const uint8_t*>(name_or_data);
            // Keep a copy of the name only for file-based sounds; for memory
            // buffers the pointer is binary data (not a C string).
            created->name.clear();
            std::vector<float> pcm;
            int rate = kFmodOutRate, ch = 2;
            bool decoded = false;
            if (!isRaw) {
                decoded = FmodParseWav(buf, exLen, pcm, rate, ch);
            }
            if (!decoded && isRaw) {
                // OPENRAW: interpret buffer using exinfo (default stereo 48k PCM16).
                int rawCh = exinfo ? exinfo->numchannels : 0;
                int rawRate = exinfo ? exinfo->defaultfrequency : 0;
                int rawFmt = exinfo ? static_cast<int>(exinfo->format) : 2;
                if (rawCh <= 0 || rawCh > 8) rawCh = 2;
                if (rawRate <= 0) rawRate = kFmodOutRate;
                size_t bytesPerSamp = 2;
                if (rawFmt == 1) bytesPerSamp = 1;       // PCM8
                else if (rawFmt == 2) bytesPerSamp = 2;  // PCM16
                else if (rawFmt == 5) bytesPerSamp = 4;  // FLOAT
                else bytesPerSamp = 2;
                size_t frames = exLen / (static_cast<size_t>(rawCh) * bytesPerSamp);
                if (frames > 0 && frames < 48000u * 60u) {
                    pcm.reserve(frames * 2);
                    for (size_t f = 0; f < frames; f++) {
                        float l = 0, r = 0;
                        for (int c = 0; c < rawCh; c++) {
                            const uint8_t* p = buf + (f * rawCh + c) * bytesPerSamp;
                            float s = 0;
                            if (rawFmt == 1) s = (static_cast<int>(p[0]) - 128) / 128.0f;
                            else if (rawFmt == 2) { int16_t v; std::memcpy(&v, p, 2); s = v / 32768.0f; }
                            else if (rawFmt == 5) { float v; std::memcpy(&v, p, 4); s = v; }
                            if (c == 0) l = s; else if (c == 1) r = s; else { l += s; r += s; }
                        }
                        if (rawCh == 1) r = l;
                        pcm.push_back(l); pcm.push_back(r);
                    }
                    // Resample if needed.
                    if (rawRate != kFmodOutRate && !pcm.empty()) {
                        size_t inF = pcm.size() / 2;
                        size_t outF = static_cast<size_t>((static_cast<double>(inF) * kFmodOutRate) / rawRate);
                        std::vector<float> rs; rs.reserve(outF * 2);
                        for (size_t i = 0; i < outF; i++) {
                            double pos = (static_cast<double>(i) * rawRate) / kFmodOutRate;
                            size_t i0 = static_cast<size_t>(pos);
                            size_t i1 = i0 + 1 < inF ? i0 + 1 : i0;
                            float t = static_cast<float>(pos - i0);
                            rs.push_back(pcm[i0 * 2] * (1 - t) + pcm[i1 * 2] * t);
                            rs.push_back(pcm[i0 * 2 + 1] * (1 - t) + pcm[i1 * 2 + 1] * t);
                        }
                        pcm.swap(rs);
                    }
                    decoded = !pcm.empty();
                    rate = kFmodOutRate; ch = 2;
                }
            }
            if (decoded) {
                created->pcm.swap(pcm);
                created->pcmRate = rate;
                created->pcmCh = ch;
            } else {
                // Compressed (Vorbis/MP3/FSB/AT9...) or unknown: audible
                // placeholder so the mixer path can be heard. Real decoders
                // (Vorbis/MP3/AT9) remain future work — see docs/TechnicalDebt.
                FmodMakeSine(created->pcm, kFmodOutRate, 2, 440.0f, 1.0f);
                created->pcmRate = kFmodOutRate;
                created->pcmCh = 2;
            }
        } else {
            if (name_or_data) {
                // File-based: store name only (first bytes may still be checked
                // for embedded RIFF in case game passes a path that is data).
                size_t nlen = std::strlen(name_or_data);
                if (nlen < 256) created->name = name_or_data;
            }
            // Placeholder 0.5s sine so file-based playSound is audible.
            FmodMakeSine(created->pcm, kFmodOutRate, 2, 330.0f, 0.5f);
            created->pcmRate = kFmodOutRate;
            created->pcmCh = 2;
        }
        *sound = created;
        return FMOD_OK;
    }
    int APS5_VABI createStream(const char* name_or_data, unsigned mode, FMOD_CREATESOUNDEXINFO* exinfo, Sound** sound) {
        return createSound(name_or_data, mode, exinfo, sound);
    }
    int APS5_VABI createDSP(FMOD_DSP_DESCRIPTION* description, DSP** dsp) {
        (void)description;
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (dsp) *dsp = new DSP();
        return FMOD_OK;
    }
    int APS5_VABI createDSPByType(FMOD_DSP_TYPE type, DSP** dsp) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (dsp) {
            *dsp = new DSP();
            (*dsp)->type = type;
        }
        return FMOD_OK;
    }
    int APS5_VABI playSound(Sound* sound, ChannelGroup* channelgroup, bool paused, Channel** channel) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (!channel) return FMOD_OK;
        Channel* created = new Channel();
        created->sound = sound;
        created->group = channelgroup ? channelgroup : masterChannelGroup;
        created->paused = paused;
        created->playing = true;
        created->owner = this;
        created->mixCursor = 0;
        if (sound && !sound->pcm.empty()) {
            created->mixHasData = true;
            created->mixTotal = sound->pcm.size() / 2;
            const bool loopMode = (sound->mode & FMOD_LOOP_NORMAL) != 0;
            created->mixLoop = loopMode || sound->loopCount != 0;
        } else {
            created->mixHasData = false;
            created->mixTotal = 0;
            created->mixLoop = false;
        }
        gFmodChannels.push_back(created);
        *channel = created;
        return FMOD_OK;
    }
    int APS5_VABI playDSP(DSP* dsp, ChannelGroup* channelgroup, bool paused, Channel** channel) {
        (void)dsp;
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (!channel) return FMOD_OK;
        Channel* created = new Channel();
        created->group = channelgroup ? channelgroup : masterChannelGroup;
        created->paused = paused;
        created->playing = true;
        created->owner = this;
        created->mixHasData = false;
        gFmodChannels.push_back(created);
        *channel = created;
        return FMOD_OK;
    }
    int APS5_VABI getChannel(int channelid, Channel** channel) {
        (void)channelid;
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (channel) *channel = nullptr;
        return FMOD_OK;
    }
    int APS5_VABI getMasterSoundGroup(SoundGroup** soundgroup) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (soundgroup) *soundgroup = masterSoundGroup;
        return FMOD_OK;
    }
    int APS5_VABI getMasterChannelGroup(ChannelGroup** channelgroup) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (channelgroup) *channelgroup = masterChannelGroup;
        return FMOD_OK;
    }
    int APS5_VABI setReverbProperties(int instance, FMOD_REVERB_PROPERTIES* prop) {
        (void)instance; (void)prop;
        return FMOD_OK;
    }
    int APS5_VABI getReverbProperties(int instance, FMOD_REVERB_PROPERTIES* prop) {
        (void)instance;
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (prop) std::memset(prop, 0, sizeof(*prop));
        return FMOD_OK;
    }
    int APS5_VABI createReverb3D(Reverb3D** reverb) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (reverb) *reverb = new Reverb3D();
        return FMOD_OK;
    }
    int APS5_VABI createGeometry(int maxpolygons, int maxvertices, Geometry** geometry) {
        (void)maxpolygons; (void)maxvertices;
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (geometry) *geometry = new Geometry();
        return FMOD_OK;
    }
    int APS5_VABI loadGeometry(const void* data, int datasize, Geometry** geometry) {
        (void)data; (void)datasize;
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (geometry) *geometry = new Geometry();
        return FMOD_OK;
    }
    int APS5_VABI setGeometrySettings(float maxworldsize) {
        (void)maxworldsize;
        return FMOD_OK;
    }
    int APS5_VABI getGeometrySettings(float* maxworldsize) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (maxworldsize) *maxworldsize = 0.0f;
        return FMOD_OK;
    }
    int APS5_VABI set3DNumListeners(int numlisteners) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        numListeners = numlisteners;
        return FMOD_OK;
    }
    int APS5_VABI get3DNumListeners(int* numlisteners) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (numlisteners) *numlisteners = numListeners;
        return FMOD_OK;
    }
    int APS5_VABI set3DListenerAttributes(int listener, const FMOD_VECTOR* pos, const FMOD_VECTOR* vel, const FMOD_VECTOR* forward, const FMOD_VECTOR* up) {
        (void)listener; (void)pos; (void)vel; (void)forward; (void)up;
        return FMOD_OK;
    }
    int APS5_VABI mixerSuspend() { return FMOD_OK; }
    int APS5_VABI mixerResume() { return FMOD_OK; }
    int APS5_VABI set3DSettings(float dopplerscale, float distancefactor, float rolloffscale) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        dopplerScale = dopplerscale;
        distanceFactor = distancefactor;
        rolloffScale = rolloffscale;
        return FMOD_OK;
    }
    int APS5_VABI get3DSettings(float* dopplerscale, float* distancefactor, float* rolloffscale) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (dopplerscale) *dopplerscale = dopplerScale;
        if (distancefactor) *distancefactor = distanceFactor;
        if (rolloffscale) *rolloffscale = rolloffScale;
        return FMOD_OK;
    }
    int APS5_VABI createChannelGroup(const char* name, ChannelGroup** channelgroup) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (!channelgroup) return FMOD_OK;
        *channelgroup = new ChannelGroup();
        if (name) (*channelgroup)->name = name;
        return FMOD_OK;
    }
    int APS5_VABI createSoundGroup(SoundGroup** soundgroup) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (soundgroup) *soundgroup = new SoundGroup();
        return FMOD_OK;
    }
    int APS5_VABI lockDSP() { return FMOD_OK; }
    int APS5_VABI unlockDSP() { return FMOD_OK; }
    int APS5_VABI getSoundRAM(int* currentalloced, int* maxalloced, int* total) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (currentalloced) *currentalloced = 0;
        if (maxalloced) *maxalloced = 0;
        if (total) *total = 0;
        return FMOD_OK;
    }
    int APS5_VABI setUserData(void* userdata) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        userData = userdata;
        return FMOD_OK;
    }
    int APS5_VABI getUserData(void** userdata) {
        std::lock_guard<std::mutex> lock(gFmodMutex);
        if (userdata) *userdata = userData;
        return FMOD_OK;
    }
    int APS5_VABI setStreamBufferSize(unsigned filebuffersize, unsigned filebuffersizetype) {
        (void)filebuffersize; (void)filebuffersizetype;
        return FMOD_OK;
    }
    int APS5_VABI attachChannelGroupToPort(FMOD_PORT_TYPE porttype, unsigned long long portindex, ChannelGroup* channelgroup) {
        (void)porttype; (void)portindex; (void)channelgroup;
        return FMOD_OK;
    }
    int APS5_VABI detachChannelGroupFromPort(ChannelGroup* channelgroup) {
        (void)channelgroup;
        return FMOD_OK;
    }
    int APS5_VABI setNetworkProxy(const char* proxy) {
        (void)proxy;
        return FMOD_OK;
    }
};

int APS5_VABI Sound::getSystemObject(System** system) {
    std::lock_guard<std::mutex> lock(gFmodMutex);
    if (system) *system = nullptr;
    return FMOD_OK;
}

int APS5_VABI ChannelControl::getSystemObject(System** system) {
    std::lock_guard<std::mutex> lock(gFmodMutex);
    if (system) *system = nullptr;
    return FMOD_OK;
}

}

extern "C" {

int APS5_VABI FMOD_System_Create(FMOD_SYSTEM** system) {
    std::lock_guard<std::mutex> lock(gFmodMutex);
    if (!system) return 0;
    *system = reinterpret_cast<FMOD_SYSTEM*>(new FMOD::System());
    return 0;
}

int APS5_VABI FMOD_Memory_Initialize(void* poolmem, int poollen, void* useralloc, void* userrealloc, void* userfree, unsigned memtypeflags) {
    (void)poolmem; (void)poollen; (void)useralloc; (void)userrealloc; (void)userfree; (void)memtypeflags;
    return 0;
}

int APS5_VABI FMOD_Memory_GetStats(int* currentalloced, int* maxalloced, int blocking) {
    (void)blocking;
    std::lock_guard<std::mutex> lock(gFmodMutex);
    if (currentalloced) *currentalloced = 0;
    if (maxalloced) *maxalloced = 0;
    return 0;
}

namespace {

// Round2 NID identification via real FMOD headers (const-correct manglings).
// VS1Vg5yOLH0 = FMOD::System::setFileSystem(open,close,read,seek,asyncRead,asyncCancel,blockalign)
// WAU72lnwYic = FMOD::System::setCallback(callback,mask)
// Both are ABI-compatible with void*/int (func ptrs are 8B, mask 4B); we keep
// the silent backend (return OK) — file callbacks are not needed for
// OPENMEMORY sounds used by Legends, and callbacks are stored nowhere.
__attribute__((used)) int APS5_VABI fmod_system_setfilesystem(void* self, void* a1, void* a2, void* a3, void* a4, void* a5, void* a6, int a7) {
    (void)self; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7;
    return 0;
}
__attribute__((used)) int APS5_VABI fmod_system_setcallback(void* self, void* cb, unsigned mask) {
    (void)self; (void)cb; (void)mask;
    return 0;
}

}

APS5_EXPORT("VS1Vg5yOLH0", fmod_system_setfilesystem);
APS5_EXPORT("WAU72lnwYic", fmod_system_setcallback);

}
