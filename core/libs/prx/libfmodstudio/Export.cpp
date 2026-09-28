#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

enum FMOD_STUDIO_STOP_MODE {
    FMOD_STUDIO_STOP_ALLOWFADEOUT = 0,
    FMOD_STUDIO_STOP_IMMEDIATE = 1,
    FMOD_STUDIO_STOP_MAX = 2
};

enum FMOD_STUDIO_PLAYBACK_STATE {
    FMOD_STUDIO_PLAYBACK_PLAYING = 0,
    FMOD_STUDIO_PLAYBACK_SUSTAINING = 1,
    FMOD_STUDIO_PLAYBACK_STOPPED = 2,
    FMOD_STUDIO_PLAYBACK_STARTING = 3,
    FMOD_STUDIO_PLAYBACK_STOPPING = 4,
    FMOD_STUDIO_PLAYBACK_MAX = 5
};

enum FMOD_STUDIO_LOADING_STATE {
    FMOD_STUDIO_LOADING_STATE_UNLOADING = 0,
    FMOD_STUDIO_LOADING_STATE_UNLOADED = 1,
    FMOD_STUDIO_LOADING_STATE_LOADING = 2,
    FMOD_STUDIO_LOADING_STATE_LOADED = 3,
    FMOD_STUDIO_LOADING_STATE_ERROR = 4,
    FMOD_STUDIO_LOADING_STATE_MAX = 5
};

enum FMOD_STUDIO_EVENT_PROPERTY {
    FMOD_STUDIO_EVENT_PROPERTY_CHANNELPRIORITY = 0,
    FMOD_STUDIO_EVENT_PROPERTY_SCHEDULE_DELAY = 1,
    FMOD_STUDIO_EVENT_PROPERTY_SCHEDULE_LOOKAHEAD = 2,
    FMOD_STUDIO_EVENT_PROPERTY_MINIMUM_DISTANCE = 3,
    FMOD_STUDIO_EVENT_PROPERTY_MAXIMUM_DISTANCE = 4,
    FMOD_STUDIO_EVENT_PROPERTY_MAX = 5
};

enum FMOD_STUDIO_SYSTEM_CALLBACK_TYPE {
    FMOD_STUDIO_SYSTEM_CALLBACK_PREUPDATE = 1,
    FMOD_STUDIO_SYSTEM_CALLBACK_POSTUPDATE = 2,
    FMOD_STUDIO_SYSTEM_CALLBACK_BANK_UNLOAD = 4,
    FMOD_STUDIO_SYSTEM_CALLBACK_ALL = 0xFFFFFFFF
};

enum FMOD_STUDIO_EVENT_CALLBACK_TYPE {
    FMOD_STUDIO_EVENT_CALLBACK_CREATED = 1,
    FMOD_STUDIO_EVENT_CALLBACK_DESTROYED = 2,
    FMOD_STUDIO_EVENT_CALLBACK_STARTING = 4,
    FMOD_STUDIO_EVENT_CALLBACK_STARTED = 8,
    FMOD_STUDIO_EVENT_CALLBACK_RESTARTED = 16,
    FMOD_STUDIO_EVENT_CALLBACK_STOPPED = 32,
    FMOD_STUDIO_EVENT_CALLBACK_START_FAILED = 64,
    FMOD_STUDIO_EVENT_CALLBACK_ALL = 0xFFFFFFFF
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

struct FMOD_CHANNELGROUP {
    int unused;
};

struct FMOD_DSP_DESCRIPTION {
    unsigned pluginsdkversion;
    char name[32];
    unsigned version;
};

struct FMOD_STUDIO_PARAMETER_ID {
    unsigned data1;
    unsigned data2;
};

struct FMOD_STUDIO_PARAMETER_DESCRIPTION {
    char name[256];
    float minimum;
    float maximum;
    float defaultvalue;
};

struct FMOD_STUDIO_BANK_INFO {
    int size;
    void* userdata;
    int userdatalength;
};

struct FMOD_STUDIO_ADVANCEDSETTINGS {
    int cbsize;
    unsigned commandqueuesize;
    unsigned handleinitialsize;
};

struct FMOD_STUDIO_CPU_USAGE {
    float update;
    float nonstreaming;
    float streaming;
};

struct FMOD_STUDIO_BUFFER_USAGE {
    int studiocommandqueue;
    int studiohandle;
    int values[6];
};

struct FMOD_STUDIO_MEMORY_USAGE {
    int exclusive;
    int inclusive;
    int sampledata;
};

struct FMOD_STUDIO_SOUND_INFO {
    const char* name_or_data;
    unsigned mode;
    int subsoundindex;
};

struct FMOD_STUDIO_SYSTEM {
    int unused;
};

struct FMOD_STUDIO_EVENTINSTANCE {
    int unused;
};

typedef int (*FMOD_STUDIO_SYSTEM_CALLBACK)(FMOD_STUDIO_SYSTEM* system, FMOD_STUDIO_SYSTEM_CALLBACK_TYPE type, void* commanddata, void* userdata);
typedef int (*FMOD_STUDIO_EVENT_CALLBACK)(FMOD_STUDIO_EVENT_CALLBACK_TYPE type, FMOD_STUDIO_EVENTINSTANCE* event, void* parameters);

namespace {

std::mutex gStudioMutex;

constexpr int FMOD_OK = 0;

}

namespace FMOD {
namespace Studio {

class System;
class Bank;
class EventDescription;
class EventInstance;
class Bus;
class VCA;
class CommandReplay;

class ID : public FMOD_GUID {
};

class Bank {
public:
    std::string path;
    bool sampleDataLoaded = false;
    void* userData = nullptr;
    bool APS5_VABI isValid() {
        return true;
    }
    int APS5_VABI getID(ID* id) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (id) std::memset(id, 0, sizeof(*id));
        return FMOD_OK;
    }
    int APS5_VABI getPath(char* path, int size, int* retrieved) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (path && size > 0) {
            std::strncpy(path, this->path.c_str(), static_cast<std::size_t>(size - 1));
            path[size - 1] = '\0';
        }
        if (retrieved) *retrieved = static_cast<int>(this->path.size() + 1);
        return FMOD_OK;
    }
    int APS5_VABI unload() {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        delete this;
        return FMOD_OK;
    }
    int APS5_VABI loadSampleData() {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        sampleDataLoaded = true;
        return FMOD_OK;
    }
    int APS5_VABI unloadSampleData() {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        sampleDataLoaded = false;
        return FMOD_OK;
    }
    int APS5_VABI getLoadingState(FMOD_STUDIO_LOADING_STATE* state) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (state) *state = FMOD_STUDIO_LOADING_STATE_LOADED;
        return FMOD_OK;
    }
    int APS5_VABI getSampleLoadingState(FMOD_STUDIO_LOADING_STATE* state) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (state) *state = sampleDataLoaded ? FMOD_STUDIO_LOADING_STATE_LOADED : FMOD_STUDIO_LOADING_STATE_UNLOADED;
        return FMOD_OK;
    }
    int APS5_VABI getStringCount(int* count) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (count) *count = 0;
        return FMOD_OK;
    }
    int APS5_VABI getEventCount(int* count) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (count) *count = 0;
        return FMOD_OK;
    }
    int APS5_VABI getEventList(EventDescription** array, int capacity, int* count) {
        (void)array; (void)capacity;
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (count) *count = 0;
        return FMOD_OK;
    }
    int APS5_VABI getBusCount(int* count) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (count) *count = 0;
        return FMOD_OK;
    }
    int APS5_VABI getBusList(Bus** array, int capacity, int* count) {
        (void)array; (void)capacity;
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (count) *count = 0;
        return FMOD_OK;
    }
    int APS5_VABI getVCACount(int* count) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (count) *count = 0;
        return FMOD_OK;
    }
    int APS5_VABI getVCAList(VCA** array, int capacity, int* count) {
        (void)array; (void)capacity;
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (count) *count = 0;
        return FMOD_OK;
    }
    int APS5_VABI setUserData(void* userdata) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        userData = userdata;
        return FMOD_OK;
    }
    int APS5_VABI getUserData(void** userdata) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (userdata) *userdata = userData;
        return FMOD_OK;
    }
};

class EventDescription {
public:
    std::string path;
    bool sampleDataLoaded = false;
    void* userData = nullptr;
    int APS5_VABI isValid() {
        return true;
    }
    int APS5_VABI getID(ID* id) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (id) std::memset(id, 0, sizeof(*id));
        return FMOD_OK;
    }
    int APS5_VABI getPath(char* path, int size, int* retrieved) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (path && size > 0) {
            std::strncpy(path, this->path.c_str(), static_cast<std::size_t>(size - 1));
            path[size - 1] = '\0';
        }
        if (retrieved) *retrieved = static_cast<int>(this->path.size() + 1);
        return FMOD_OK;
    }
    int APS5_VABI getInstanceCount(int* count) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (count) *count = 0;
        return FMOD_OK;
    }
    int APS5_VABI getInstanceList(EventInstance** array, int capacity, int* count) {
        (void)array; (void)capacity;
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (count) *count = 0;
        return FMOD_OK;
    }
    int APS5_VABI createInstance(EventInstance** instance);
    int APS5_VABI loadSampleData() {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        sampleDataLoaded = true;
        return FMOD_OK;
    }
    int APS5_VABI unloadSampleData() {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        sampleDataLoaded = false;
        return FMOD_OK;
    }
    int APS5_VABI getSampleLoadingState(FMOD_STUDIO_LOADING_STATE* state) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (state) *state = sampleDataLoaded ? FMOD_STUDIO_LOADING_STATE_LOADED : FMOD_STUDIO_LOADING_STATE_UNLOADED;
        return FMOD_OK;
    }
    int APS5_VABI getLength(int* length) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (length) *length = 0;
        return FMOD_OK;
    }
    int APS5_VABI isSnapshot(bool* snapshot) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (snapshot) *snapshot = false;
        return FMOD_OK;
    }
    int APS5_VABI isOneshot(bool* oneshot) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (oneshot) *oneshot = false;
        return FMOD_OK;
    }
    int APS5_VABI isStream(bool* isstream) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (isstream) *isstream = false;
        return FMOD_OK;
    }
    int APS5_VABI is3D(bool* is3d) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (is3d) *is3d = false;
        return FMOD_OK;
    }
    int APS5_VABI hasSustainPoint(bool* sustainpoint) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (sustainpoint) *sustainpoint = false;
        return FMOD_OK;
    }
    int APS5_VABI getMinimumDistance(float* distance) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (distance) *distance = 0.0f;
        return FMOD_OK;
    }
    int APS5_VABI getMaximumDistance(float* distance) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (distance) *distance = 10000.0f;
        return FMOD_OK;
    }
    int APS5_VABI setUserData(void* userdata) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        userData = userdata;
        return FMOD_OK;
    }
    int APS5_VABI getUserData(void** userdata) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (userdata) *userdata = userData;
        return FMOD_OK;
    }
};

class EventInstance {
public:
    EventDescription* description = nullptr;
    bool playing = false;
    bool paused = false;
    float volume = 1.0f;
    float pitch = 1.0f;
    void* userData = nullptr;
    std::unordered_map<std::string, float> paramsByName;
    int APS5_VABI isValid() {
        return true;
    }
    int APS5_VABI getDescription(EventDescription** description) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (description) *description = this->description;
        return FMOD_OK;
    }
    int APS5_VABI getVolume(float* volume, float* finalvolume) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (volume) *volume = this->volume;
        if (finalvolume) *finalvolume = this->volume;
        return FMOD_OK;
    }
    int APS5_VABI setVolume(float volume) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        this->volume = volume;
        return FMOD_OK;
    }
    int APS5_VABI getPitch(float* pitch, float* finalpitch) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (pitch) *pitch = this->pitch;
        if (finalpitch) *finalpitch = this->pitch;
        return FMOD_OK;
    }
    int APS5_VABI setPitch(float pitch) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        this->pitch = pitch;
        return FMOD_OK;
    }
    int APS5_VABI getPaused(bool* paused) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (paused) *paused = this->paused;
        return FMOD_OK;
    }
    int APS5_VABI setPaused(bool paused) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        this->paused = paused;
        return FMOD_OK;
    }
    int APS5_VABI start() {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        playing = true;
        return FMOD_OK;
    }
    int APS5_VABI stop(FMOD_STUDIO_STOP_MODE mode) {
        (void)mode;
        std::lock_guard<std::mutex> lock(gStudioMutex);
        playing = false;
        return FMOD_OK;
    }
    int APS5_VABI release() {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        delete this;
        return FMOD_OK;
    }
    int APS5_VABI getPlaybackState(FMOD_STUDIO_PLAYBACK_STATE* state) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (state) *state = playing ? FMOD_STUDIO_PLAYBACK_PLAYING : FMOD_STUDIO_PLAYBACK_STOPPED;
        return FMOD_OK;
    }
    int APS5_VABI setTimelinePosition(int position) {
        (void)position;
        return FMOD_OK;
    }
    int APS5_VABI getTimelinePosition(int* position) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (position) *position = 0;
        return FMOD_OK;
    }
    int APS5_VABI setParameterByName(const char* name, float value, bool ignoreseekspeed) {
        (void)ignoreseekspeed;
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (name) paramsByName[name] = value;
        return FMOD_OK;
    }
    int APS5_VABI getParameterByName(const char* name, float* value, float* finalvalue) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        float stored = 0.0f;
        if (name) {
            auto it = paramsByName.find(name);
            if (it != paramsByName.end()) stored = it->second;
        }
        if (value) *value = stored;
        if (finalvalue) *finalvalue = stored;
        return FMOD_OK;
    }
    int APS5_VABI setParameterByID(FMOD_STUDIO_PARAMETER_ID id, float value, bool ignoreseekspeed) {
        (void)id; (void)ignoreseekspeed;
        return FMOD_OK;
    }
    int APS5_VABI keyOff() { return FMOD_OK; }
    int APS5_VABI setReverbLevel(int index, float level) {
        (void)index; (void)level;
        return FMOD_OK;
    }
    int APS5_VABI set3DAttributes(const FMOD_3D_ATTRIBUTES* attributes) {
        (void)attributes;
        return FMOD_OK;
    }
    int APS5_VABI setListenerMask(unsigned mask) {
        (void)mask;
        return FMOD_OK;
    }
    int APS5_VABI getChannelGroup(FMOD_CHANNELGROUP** group) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (group) *group = nullptr;
        return FMOD_OK;
    }
    int APS5_VABI setCallback(FMOD_STUDIO_EVENT_CALLBACK callback) {
        (void)callback;
        return FMOD_OK;
    }
    int APS5_VABI setUserData(void* userdata) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        userData = userdata;
        return FMOD_OK;
    }
    int APS5_VABI getUserData(void** userdata) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (userdata) *userdata = userData;
        return FMOD_OK;
    }
    int APS5_VABI hasSustainPoint(bool* sustainpoint) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (sustainpoint) *sustainpoint = false;
        return FMOD_OK;
    }
    int APS5_VABI triggerCue() { return FMOD_OK; }
};

int APS5_VABI EventDescription::createInstance(EventInstance** instance) {
    std::lock_guard<std::mutex> lock(gStudioMutex);
    if (!instance) return FMOD_OK;
    *instance = new EventInstance();
    (*instance)->description = this;
    return FMOD_OK;
}

class Bus {
public:
    std::string path;
    float volume = 1.0f;
    bool paused = false;
    bool mute = false;
    void* userData = nullptr;
    int APS5_VABI isValid() {
        return true;
    }
    int APS5_VABI getVolume(float* volume, float* finalvolume) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (volume) *volume = this->volume;
        if (finalvolume) *finalvolume = this->volume;
        return FMOD_OK;
    }
    int APS5_VABI setVolume(float volume) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        this->volume = volume;
        return FMOD_OK;
    }
    int APS5_VABI getPaused(bool* paused) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (paused) *paused = this->paused;
        return FMOD_OK;
    }
    int APS5_VABI setPaused(bool paused) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        this->paused = paused;
        return FMOD_OK;
    }
    int APS5_VABI getMute(bool* mute) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (mute) *mute = this->mute;
        return FMOD_OK;
    }
    int APS5_VABI setMute(bool mute) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        this->mute = mute;
        return FMOD_OK;
    }
    int APS5_VABI stopAllEvents(FMOD_STUDIO_STOP_MODE mode) {
        (void)mode;
        return FMOD_OK;
    }
    int APS5_VABI getChannelGroup(FMOD_CHANNELGROUP** group) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (group) *group = nullptr;
        return FMOD_OK;
    }
    int APS5_VABI lockChannelGroup() { return FMOD_OK; }
    int APS5_VABI unlockChannelGroup() { return FMOD_OK; }
    int APS5_VABI setUserData(void* userdata) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        userData = userdata;
        return FMOD_OK;
    }
    int APS5_VABI getUserData(void** userdata) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (userdata) *userdata = userData;
        return FMOD_OK;
    }
};

class VCA {
public:
    std::string path;
    float volume = 1.0f;
    void* userData = nullptr;
    int APS5_VABI isValid() {
        return true;
    }
    int APS5_VABI getVolume(float* volume, float* finalvolume) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (volume) *volume = this->volume;
        if (finalvolume) *finalvolume = this->volume;
        return FMOD_OK;
    }
    int APS5_VABI setVolume(float volume) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        this->volume = volume;
        return FMOD_OK;
    }
    int APS5_VABI setUserData(void* userdata) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        userData = userdata;
        return FMOD_OK;
    }
    int APS5_VABI getUserData(void** userdata) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (userdata) *userdata = userData;
        return FMOD_OK;
    }
};

class CommandReplay {
public:
    void* userData = nullptr;
    int APS5_VABI isValid() {
        return true;
    }
    int APS5_VABI getLength(float* length) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (length) *length = 0.0f;
        return FMOD_OK;
    }
    int APS5_VABI start() { return FMOD_OK; }
    int APS5_VABI stop() { return FMOD_OK; }
    int APS5_VABI seekToTime(float time) {
        (void)time;
        return FMOD_OK;
    }
    int APS5_VABI release() {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        delete this;
        return FMOD_OK;
    }
    int APS5_VABI setUserData(void* userdata) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        userData = userdata;
        return FMOD_OK;
    }
    int APS5_VABI getUserData(void** userdata) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (userdata) *userdata = userData;
        return FMOD_OK;
    }
};

class System {
public:
    bool initialized = false;
    int maxChannels = 0;
    int numListeners = 1;
    void* userData = nullptr;
    std::unordered_map<std::string, float> paramsByName;
    std::vector<Bank*> banks;
    static int APS5_VABI create(System** system, unsigned headerversion) {
        (void)headerversion;
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (!system) return FMOD_OK;
        *system = new System();
        return FMOD_OK;
    }
    int APS5_VABI isValid() {
        return true;
    }
    int APS5_VABI release() {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        for (Bank* bank : banks) delete bank;
        banks.clear();
        delete this;
        return FMOD_OK;
    }
    int APS5_VABI initialize(int maxchannels, unsigned studioflags, unsigned flags, void* extradriverdata) {
        (void)studioflags; (void)flags; (void)extradriverdata;
        std::lock_guard<std::mutex> lock(gStudioMutex);
        maxChannels = maxchannels;
        initialized = true;
        return FMOD_OK;
    }
    int APS5_VABI update() { return FMOD_OK; }
    int APS5_VABI getEvent(const char* pathOrID, EventDescription** event) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (!event) return FMOD_OK;
        *event = new EventDescription();
        if (pathOrID) (*event)->path = pathOrID;
        return FMOD_OK;
    }
    int APS5_VABI getBankList(Bank** array, int capacity, int* count) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (count) *count = static_cast<int>(banks.size());
        if (array && capacity > 0) {
            int filled = capacity < static_cast<int>(banks.size()) ? capacity : static_cast<int>(banks.size());
            for (int i = 0; i < filled; i++) array[i] = banks[static_cast<std::size_t>(i)];
        }
        return FMOD_OK;
    }
    int APS5_VABI getBankCount(int* count) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (count) *count = static_cast<int>(banks.size());
        return FMOD_OK;
    }
    int APS5_VABI loadBankFile(const char* filename, unsigned flags, Bank** bank) {
        (void)flags;
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (!bank) return FMOD_OK;
        *bank = new Bank();
        if (filename) (*bank)->path = filename;
        banks.push_back(*bank);
        return FMOD_OK;
    }
    int APS5_VABI loadBankMemory(const char* buffer, int length, unsigned mode, unsigned flags, Bank** bank) {
        (void)buffer; (void)length; (void)mode; (void)flags;
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (!bank) return FMOD_OK;
        *bank = new Bank();
        banks.push_back(*bank);
        return FMOD_OK;
    }
    int APS5_VABI unloadAll() {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        for (Bank* bank : banks) delete bank;
        banks.clear();
        return FMOD_OK;
    }
    int APS5_VABI flushCommands() { return FMOD_OK; }
    int APS5_VABI flushSampleLoading() { return FMOD_OK; }
    int APS5_VABI setNumListeners(int numlisteners) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        numListeners = numlisteners;
        return FMOD_OK;
    }
    int APS5_VABI getNumListeners(int* numlisteners) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (numlisteners) *numlisteners = numListeners;
        return FMOD_OK;
    }
    int APS5_VABI setListenerAttributes(int index, const FMOD_3D_ATTRIBUTES* attributes, const FMOD_VECTOR* attenuationposition) {
        (void)index; (void)attributes; (void)attenuationposition;
        return FMOD_OK;
    }
    int APS5_VABI getBus(const char* pathOrID, Bus** bus) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (!bus) return FMOD_OK;
        *bus = new Bus();
        if (pathOrID) (*bus)->path = pathOrID;
        return FMOD_OK;
    }
    int APS5_VABI getVCA(const char* pathOrID, VCA** vca) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (!vca) return FMOD_OK;
        *vca = new VCA();
        if (pathOrID) (*vca)->path = pathOrID;
        return FMOD_OK;
    }
    int APS5_VABI getParameterByName(const char* name, float* value, float* finalvalue) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        float stored = 0.0f;
        if (name) {
            auto it = paramsByName.find(name);
            if (it != paramsByName.end()) stored = it->second;
        }
        if (value) *value = stored;
        if (finalvalue) *finalvalue = stored;
        return FMOD_OK;
    }
    int APS5_VABI setParameterByName(const char* name, float value, bool ignoreseekspeed) {
        (void)ignoreseekspeed;
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (name) paramsByName[name] = value;
        return FMOD_OK;
    }
    int APS5_VABI setCallback(FMOD_STUDIO_SYSTEM_CALLBACK callback, FMOD_STUDIO_SYSTEM_CALLBACK_TYPE callbackmask) {
        (void)callback; (void)callbackmask;
        return FMOD_OK;
    }
    int APS5_VABI registerPlugin(const FMOD_DSP_DESCRIPTION* description) {
        (void)description;
        return FMOD_OK;
    }
    int APS5_VABI setUserData(void* userdata) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        userData = userdata;
        return FMOD_OK;
    }
    int APS5_VABI getUserData(void** userdata) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (userdata) *userdata = userData;
        return FMOD_OK;
    }
    int APS5_VABI getCPUUsage(FMOD_STUDIO_CPU_USAGE* usage) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (usage) std::memset(usage, 0, sizeof(*usage));
        return FMOD_OK;
    }
    int APS5_VABI getBufferUsage(FMOD_STUDIO_BUFFER_USAGE* usage) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (usage) std::memset(usage, 0, sizeof(*usage));
        return FMOD_OK;
    }
    int APS5_VABI resetBufferUsage() { return FMOD_OK; }
    int APS5_VABI getMemoryUsage(FMOD_STUDIO_MEMORY_USAGE* memoryusage) {
        std::lock_guard<std::mutex> lock(gStudioMutex);
        if (memoryusage) std::memset(memoryusage, 0, sizeof(*memoryusage));
        return FMOD_OK;
    }
};

}
}

extern "C" {

namespace {

// Round2: 20/25 studio stubs identified via real FMOD headers (const-correct
// Itanium manglings, verified with g++ + c++filt + NidCompute SHA1+suffix).
// Wrappers below are ABI-compatible (this+params, SysV) with the real const
// methods; most delegate to the existing non-const logic (const is
// compile-time only, same calling convention).

// +V2QcIcvWpw = EventDescription::createInstance(EventInstance**) const
__attribute__((used)) int APS5_VABI fmodstudio_real_createInstance(const void* self, void** out) {
    std::lock_guard<std::mutex> lock(gStudioMutex);
    if (!out) return 0;
    auto* desc = const_cast<FMOD::Studio::EventDescription*>(static_cast<const FMOD::Studio::EventDescription*>(self));
    auto* inst = new FMOD::Studio::EventInstance();
    inst->description = desc;
    *out = inst;
    return 0;
}
// 2QOSvz5hvAg = EventInstance::getPlaybackState(PLAYBACK_STATE*) const
__attribute__((used)) int APS5_VABI fmodstudio_real_getPlaybackState(const void* self, int* state) {
    std::lock_guard<std::mutex> lock(gStudioMutex);
    const auto* inst = static_cast<const FMOD::Studio::EventInstance*>(self);
    if (state) *state = (inst && inst->playing) ? 0 : 2; // PLAYING=0, STOPPED=2
    return 0;
}
// 3loRRCoqXfU = EventInstance::isValid() const
__attribute__((used)) int APS5_VABI fmodstudio_real_ei_isValid(const void* self) {
    (void)self;
    return 1;
}
// 6E6MUTGjxhI = System::getEvent(const char*, EventDescription**) const
__attribute__((used)) int APS5_VABI fmodstudio_real_getEvent(const void* self, const char* path, void** out) {
    (void)self;
    std::lock_guard<std::mutex> lock(gStudioMutex);
    if (!out) return 0;
    auto* ed = new FMOD::Studio::EventDescription();
    if (path) ed->path = path;
    *out = ed;
    return 0;
}
// 6nWYF1yMOl4 = Bank::getLoadingState(LOADING_STATE*) const
__attribute__((used)) int APS5_VABI fmodstudio_real_bankLoading(const void* self, int* state) {
    (void)self;
    std::lock_guard<std::mutex> lock(gStudioMutex);
    if (state) *state = 3; // LOADED
    return 0;
}
// 7hd4bRJuLMg = System::getCoreSystem(CoreSystem**) const (placeholder 256B zeroed)
__attribute__((used)) int APS5_VABI fmodstudio_real_getCoreSystem(const void* self, void** out) {
    (void)self;
    std::lock_guard<std::mutex> lock(gStudioMutex);
    if (!out) return 0;
    void* placeholder = std::calloc(1, 256);
    *out = placeholder;
    return 0;
}
// -7VafNNns2A = EventInstance::getUserData(void**) const
__attribute__((used)) int APS5_VABI fmodstudio_real_ei_getUserData(const void* self, void** out) {
    std::lock_guard<std::mutex> lock(gStudioMutex);
    const auto* inst = static_cast<const FMOD::Studio::EventInstance*>(self);
    if (out) *out = inst ? inst->userData : nullptr;
    return 0;
}
// AdR01fo-WaM = System::getParameterByName(const char*, float*, float*) const
__attribute__((used)) int APS5_VABI fmodstudio_real_sysGetParam(const void* self, const char* name, float* v, float* fv) {
    std::lock_guard<std::mutex> lock(gStudioMutex);
    const auto* sys = static_cast<const FMOD::Studio::System*>(self);
    float stored = 0.0f;
    if (sys && name) {
        auto it = sys->paramsByName.find(name);
        if (it != sys->paramsByName.end()) stored = it->second;
    }
    if (v) *v = stored;
    if (fv) *fv = stored;
    return 0;
}
// alleMuKEWr8 = EventDescription::isOneshot(bool*) const
__attribute__((used)) int APS5_VABI fmodstudio_real_isOneshot(const void* self, bool* out) {
    (void)self;
    std::lock_guard<std::mutex> lock(gStudioMutex);
    if (out) *out = false;
    return 0;
}
// Bu8uFP7ME2k = System::getVCA(const char*, VCA**) const
__attribute__((used)) int APS5_VABI fmodstudio_real_getVCA(const void* self, const char* path, void** out) {
    (void)self;
    std::lock_guard<std::mutex> lock(gStudioMutex);
    if (!out) return 0;
    auto* vca = new FMOD::Studio::VCA();
    if (path) vca->path = path;
    *out = vca;
    return 0;
}
// DsFsPD0MWNc = System::getSoundInfo(const char*, SOUND_INFO*) const
__attribute__((used)) int APS5_VABI fmodstudio_real_getSoundInfo(const void* self, const char* key, void* info) {
    (void)self; (void)key;
    std::lock_guard<std::mutex> lock(gStudioMutex);
    if (info) std::memset(info, 0, sizeof(FMOD_STUDIO_SOUND_INFO));
    return 0;
}
// --f9RJwAh1A = EventDescription::getSampleLoadingState(LOADING_STATE*) const
__attribute__((used)) int APS5_VABI fmodstudio_real_edSampleState(const void* self, int* state) {
    std::lock_guard<std::mutex> lock(gStudioMutex);
    const auto* ed = static_cast<const FMOD::Studio::EventDescription*>(self);
    if (state) *state = (ed && ed->sampleDataLoaded) ? 3 : 1; // LOADED=3, UNLOADED=1
    return 0;
}
// fbAVXdyBuso = Bank::isValid() const
__attribute__((used)) int APS5_VABI fmodstudio_real_bankIsValid(const void* self) {
    (void)self;
    return 1;
}
// LG53EJZJDnA = EventInstance::setCallback(callback, mask) (non-const, 2 params)
__attribute__((used)) int APS5_VABI fmodstudio_real_eiSetCallback(void* self, void* cb, unsigned mask) {
    (void)self; (void)cb; (void)mask;
    return 0;
}
// nhBPjhZ+VWs = EventDescription::is3D(bool*) const
__attribute__((used)) int APS5_VABI fmodstudio_real_is3D(const void* self, bool* out) {
    (void)self;
    std::lock_guard<std::mutex> lock(gStudioMutex);
    if (out) *out = false;
    return 0;
}
// omDBr+dcDVc = EventDescription::getInstanceCount(int*) const
__attribute__((used)) int APS5_VABI fmodstudio_real_getInstanceCount(const void* self, int* out) {
    (void)self;
    std::lock_guard<std::mutex> lock(gStudioMutex);
    if (out) *out = 0;
    return 0;
}
// uWXqw4LeM5Q = EventDescription::isValid() const
__attribute__((used)) int APS5_VABI fmodstudio_real_edIsValid(const void* self) {
    (void)self;
    return 1;
}
// VdQjbIsdhXQ = EventDescription::getLength(int*) const
__attribute__((used)) int APS5_VABI fmodstudio_real_edGetLength(const void* self, int* out) {
    (void)self;
    std::lock_guard<std::mutex> lock(gStudioMutex);
    if (out) *out = 0;
    return 0;
}
// WP51b8simn8 = EventInstance::getTimelinePosition(int*) const
__attribute__((used)) int APS5_VABI fmodstudio_real_getTimeline(const void* self, int* out) {
    (void)self;
    std::lock_guard<std::mutex> lock(gStudioMutex);
    if (out) *out = 0;
    return 0;
}
// Zi1n4MvZ3-0 = System::getBufferUsage(BUFFER_USAGE*) const
__attribute__((used)) int APS5_VABI fmodstudio_real_getBufferUsage(const void* self, void* out) {
    (void)self;
    std::lock_guard<std::mutex> lock(gStudioMutex);
    if (out) std::memset(out, 0, sizeof(FMOD_STUDIO_BUFFER_USAGE));
    return 0;
}

__attribute__((used)) int fmodstudio_nid_stub_10() { NotImplemented_nid_no_patch("Cm2cmtCv8cA"); return 0; }
__attribute__((used)) int fmodstudio_nid_stub_12() { NotImplemented_nid_no_patch("EfM9xXvBmxk"); return 0; }
__attribute__((used)) int fmodstudio_nid_stub_15() { NotImplemented_nid_no_patch("Js90KQXVS4s"); return 0; }
__attribute__((used)) int fmodstudio_nid_stub_18() { NotImplemented_nid_no_patch("oCYWES02VPc"); return 0; }
__attribute__((used)) int fmodstudio_nid_stub_22() { NotImplemented_nid_no_patch("vVwA2cZA5e4"); return 0; }

}

APS5_EXPORT("+V2QcIcvWpw", fmodstudio_real_createInstance);
APS5_EXPORT("2QOSvz5hvAg", fmodstudio_real_getPlaybackState);
APS5_EXPORT("3loRRCoqXfU", fmodstudio_real_ei_isValid);
APS5_EXPORT("6E6MUTGjxhI", fmodstudio_real_getEvent);
APS5_EXPORT("6nWYF1yMOl4", fmodstudio_real_bankLoading);
APS5_EXPORT("7hd4bRJuLMg", fmodstudio_real_getCoreSystem);
APS5_EXPORT("-7VafNNns2A", fmodstudio_real_ei_getUserData);
APS5_EXPORT("AdR01fo-WaM", fmodstudio_real_sysGetParam);
APS5_EXPORT("alleMuKEWr8", fmodstudio_real_isOneshot);
APS5_EXPORT("Bu8uFP7ME2k", fmodstudio_real_getVCA);
APS5_EXPORT("Cm2cmtCv8cA", fmodstudio_nid_stub_10);
APS5_EXPORT("DsFsPD0MWNc", fmodstudio_real_getSoundInfo);
APS5_EXPORT("EfM9xXvBmxk", fmodstudio_nid_stub_12);
APS5_EXPORT("--f9RJwAh1A", fmodstudio_real_edSampleState);
APS5_EXPORT("fbAVXdyBuso", fmodstudio_real_bankIsValid);
APS5_EXPORT("Js90KQXVS4s", fmodstudio_nid_stub_15);
APS5_EXPORT("LG53EJZJDnA", fmodstudio_real_eiSetCallback);
APS5_EXPORT("nhBPjhZ+VWs", fmodstudio_real_is3D);
APS5_EXPORT("oCYWES02VPc", fmodstudio_nid_stub_18);
APS5_EXPORT("omDBr+dcDVc", fmodstudio_real_getInstanceCount);
APS5_EXPORT("uWXqw4LeM5Q", fmodstudio_real_edIsValid);
APS5_EXPORT("VdQjbIsdhXQ", fmodstudio_real_edGetLength);
APS5_EXPORT("vVwA2cZA5e4", fmodstudio_nid_stub_22);
APS5_EXPORT("WP51b8simn8", fmodstudio_real_getTimeline);
APS5_EXPORT("Zi1n4MvZ3-0", fmodstudio_real_getBufferUsage);

}
