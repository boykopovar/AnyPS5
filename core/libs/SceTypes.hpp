#ifndef CORE_LIBS_SCE_TYPES_HPP
#define CORE_LIBS_SCE_TYPES_HPP

#include <cstddef>
#include <cstdint>
#include <cstdio>

#if defined(__GNUC__) || defined(__clang__)
typedef float __m128 __attribute__((__vector_size__(16), __aligned__(16)));
#elif defined(_MSC_VER)
#include <xmmintrin.h>
#else
struct alignas(16) __m128 { float v[4]; };
#endif

using Bool = std::uint8_t;

using KernelModule = std::int32_t;
using KernelCpumask = std::uint64_t;
using KernelUseconds = unsigned int;
using KernelClockid = std::int32_t;
using KernelEqueue = std::int64_t;
using KernelKey = int;

struct KernelTimespec {
    std::int64_t tv_sec;
    std::int64_t tv_nsec;
};

struct KernelTimeval {
    std::int64_t tv_sec;
    std::int64_t tv_usec;
};

struct KernelTimezone {
    std::int32_t tz_minuteswest;
    std::int32_t tz_dsttime;
};

struct KernelTimesec {
    std::int64_t t;
    std::uint32_t west_sec;
    std::uint32_t dst_sec;
};

struct KernelEvent {
    std::uintptr_t ident = 0;
    std::int16_t filter = 0;
    std::uint16_t flags = 0;
    std::uint32_t fflags = 0;
    std::intptr_t data = 0;
    void* udata = nullptr;
};

struct KernelSchedParam {
    int sched_priority;
};

struct KernelLoadModuleOpt {
    std::size_t size;
};

struct KernelUnloadModuleOpt {
    std::size_t size;
};

struct KernelAioResult {
    std::int64_t return_value;
    std::uint32_t state;
};

struct KernelAioRwRequest {
    std::int64_t offset;
    std::size_t nbyte;
    void* buf;
    KernelAioResult* result;
    std::int32_t fd;
};

struct KernelBatchMapEntry {
    void* start;
    std::uint64_t offset;
    std::uint64_t length;
    char protection;
    char type;
    std::int16_t reserved;
    std::int32_t operation;
};

struct KernelMemoryPoolBatchEntry {
    std::uint32_t op;
    std::uint32_t flags;
    union {
        struct {
            void* addr;
            std::uint64_t len;
            std::uint8_t prot;
            std::uint8_t type;
        } commit;
        struct {
            void* addr;
            std::uint64_t len;
        } decommit;
        struct {
            void* addr;
            std::uint64_t len;
            std::uint8_t prot;
        } protect;
        struct {
            void* addr;
            std::uint64_t len;
            std::uint8_t prot;
            std::uint8_t type;
        } type_protect;
        struct {
            void* dst;
            void* src;
            std::uint64_t len;
        } move;
        std::uintptr_t padding[3];
    };
};

struct KernelMemoryPoolBlockStats {
    std::int32_t available_flushed_blocks;
    std::int32_t available_cached_blocks;
    std::int32_t allocated_flushed_blocks;
    std::int32_t allocated_cached_blocks;
};

struct VirtualQueryInfo {
    std::uintptr_t start;
    std::uintptr_t end;
    std::uint64_t offset;
    std::int32_t protection;
    std::int32_t memory_type;
    std::uint32_t is_flexible : 1;
    std::uint32_t is_direct : 1;
    std::uint32_t is_stack : 1;
    std::uint32_t is_pooled : 1;
    std::uint32_t is_committed : 1;
    std::uint32_t is_gpu_prt : 1;
    std::uint32_t amm_usage : 1;
    std::uint32_t reserved : 1;
    char name[32];
    std::uint8_t gpu_mask_id;
    std::uint8_t reserved2;
};

struct KernelSemaPrivate;
struct KernelEventFlagPrivate;
struct PthreadAttrPrivate;
struct PthreadPrivate;
struct PthreadMutexPrivate;
struct PthreadMutexattrPrivate;
struct PthreadRwlockPrivate;
struct PthreadRwlockattrPrivate;
struct PthreadCondattrPrivate;
struct PthreadCondPrivate;

using KernelSema = KernelSemaPrivate*;
using KernelEventFlag = KernelEventFlagPrivate*;
using Pthread = PthreadPrivate*;
using PthreadAttr = PthreadAttrPrivate*;
using PthreadMutex = PthreadMutexPrivate*;
using PthreadMutexattr = PthreadMutexattrPrivate*;
using PthreadRwlock = PthreadRwlockPrivate*;
using PthreadRwlockattr = PthreadRwlockattrPrivate*;
using PthreadCond = PthreadCondPrivate*;
using PthreadCondattr = PthreadCondattrPrivate*;
using PthreadKey = int;
using pthread_entry_func_t = void* (*)(void*);
using pthread_key_destructor_func_t = void (*)(void*);
using thread_dtors_func_t = void (*)();
using get_thread_atexit_count_func_t = int (*)(KernelModule);
using thread_atexit_report_func_t = void (*)(KernelModule);

struct FileStat {
    std::uint32_t st_dev;
    std::uint32_t st_ino;
    std::uint16_t st_mode;
    std::uint16_t st_nlink;
    std::uint32_t st_uid;
    std::uint32_t st_gid;
    std::uint32_t st_rdev;
    KernelTimespec st_atim;
    KernelTimespec st_mtim;
    KernelTimespec st_ctim;
    std::int64_t st_size;
    std::int64_t st_blocks;
    std::uint32_t st_blksize;
    std::uint32_t st_flags;
    std::uint32_t st_gen;
    std::int32_t st_lspare;
    KernelTimespec st_birthtim;
};

struct ModuleInfo {
    std::uint64_t size;
    std::uint64_t info[32];
    KernelModule handle;
    std::uint8_t pad[156];
};

struct ModuleInfoForUnwind {
    std::uint64_t st_size;
    char name[256];
    std::uint64_t eh_frame_hdr_addr;
    std::uint64_t eh_frame_addr;
    std::uint64_t eh_frame_size;
    std::uint64_t seg0_addr;
    std::uint64_t seg0_size;
};

struct MallocReplace {
    std::uint64_t size = sizeof(MallocReplace);
    void* malloc_initialize = nullptr;
    void* malloc_finalize = nullptr;
    void* malloc = nullptr;
    void* free = nullptr;
    void* calloc = nullptr;
    void* realloc = nullptr;
    void* memalign = nullptr;
    void* reallocalign = nullptr;
    void* posix_memalign = nullptr;
    void* malloc_stats = nullptr;
    void* malloc_stats_fast = nullptr;
    void* malloc_usable_size = nullptr;
    void* aligned_alloc = nullptr;
};

struct NewReplace {
    std::uint64_t size = sizeof(NewReplace);
    void* new_p = nullptr;
    void* new_nothrow = nullptr;
    void* new_array = nullptr;
    void* new_array_nothrow = nullptr;
    void* delete_p = nullptr;
    void* delete_nothrow = nullptr;
    void* delete_array = nullptr;
    void* delete_array_nothrow = nullptr;
    void* delete_with_size = nullptr;
    void* delete_with_size_nothrow = nullptr;
    void* delete_array_with_size = nullptr;
    void* delete_array_with_size_nothrow = nullptr;
};

#pragma pack(push, 1)
struct VaList {
    std::uint32_t gp_offset;
    std::uint32_t fp_offset;
    void* overflow_arg_area;
    void* reg_save_area;
};
#pragma pack(pop)

using FiberEntry = void* (*)(std::uint64_t, void*);

struct FiberOptParam {
    std::uint32_t magic;
};

struct FiberCpuContext {
    std::uint64_t rip;
    std::uint64_t rsp;
    std::uint64_t rbp;
    std::uint64_t rbx;
    std::uint64_t r12;
    std::uint64_t r13;
    std::uint64_t r14;
    std::uint64_t r15;
};

constexpr std::uint32_t FIBER_MAX_NAME_LENGTH = 31;

struct FiberObject {
    std::uint32_t magic_start;
    std::uint32_t state;
    FiberEntry entry;
    std::uint64_t arg_on_initialize;
    void* addr_context;
    std::uint64_t size_context;
    char name[FIBER_MAX_NAME_LENGTH + 1];
    void* context;
    std::uint32_t flags;
    std::uint32_t padding;
    void* context_start;
    void* context_end;
    FiberCpuContext saved_context;
    std::uint64_t arg_on_run;
    std::uint64_t arg_on_return;
    bool context_valid;
    std::uint32_t magic_end;
};

struct FiberInfo {
    std::uint64_t size;
    FiberEntry entry;
    std::uint64_t arg_on_initialize;
    void* addr_context;
    std::uint64_t size_context;
    char name[FIBER_MAX_NAME_LENGTH + 1];
    std::uint64_t size_context_margin;
    std::uint8_t padding[48];
};

struct RtcDateTime {
    std::uint16_t year;
    std::uint16_t month;
    std::uint16_t day;
    std::uint16_t hour;
    std::uint16_t minute;
    std::uint16_t second;
    std::uint32_t microsecond;
};

struct RtcTick {
    std::uint64_t tick;
};

struct CommandBuffer {
    using Callback = bool (*)(CommandBuffer*, std::uint32_t, void*);
    std::uint32_t* bottom;
    std::uint32_t* top;
    std::uint32_t* cursor_up;
    std::uint32_t* cursor_down;
    Callback callback;
    void* user_data;
    std::uint32_t reserved_dw;
};

struct Label {
    volatile std::uint64_t value;
};

#include "SceShaders.hpp"

struct ShareCurrentRecordingStatus {
    std::uint32_t recording_2k_status;
    std::uint32_t recording_4k_status;
    std::uint8_t reserved[8];
};

union ShareCurrentStatus {
    ShareCurrentRecordingStatus recording_status;
    std::uint8_t reserved[16];
};

using AcmContextId = std::uint32_t;
using AcmBatchId = std::uint32_t;

struct AcmBatchError {
    std::uint32_t reserved[8];
};

struct AcmBatchInfo {
    void* buffer;
    std::size_t offset;
    std::size_t buffer_size;
};

struct AjmBatchError {
    std::int32_t error_code;
    const void* job_addr;
    std::uint32_t cmd_offset;
    const void* job_ra;
};

struct AjmBatchInfo {
    void* p_buffer;
    std::uint64_t offset;
    std::uint64_t size;
};

struct AjmBuffer {
    void* ptr;
    std::size_t size;
};

struct AjmDecAt9ConfigDataInfo {
    std::uint32_t channels;
    std::uint32_t sample_rate;
    std::uint32_t frame_samples_per_channel;
    std::uint32_t superframe_samples_per_channel;
    std::uint32_t superframe_size;
};

using AudioOut2ContextHandle = std::uint64_t;
using AudioOut2PortHandle = std::uint64_t;
using AudioOut2UserHandle = std::uintptr_t;
using AudioOut2SpeakerArrayHandle = void*;

struct AudioOut2ContextParam {
    std::uint32_t max_ports;
    std::uint32_t max_object_ports;
    std::uint32_t guarantee_object_ports;
    std::uint32_t queue_depth;
    std::uint32_t num_grains;
    std::uint32_t flags;
    std::uint32_t reserved[10];
};

struct AudioOut2PortParam {
    std::uint16_t port_type;
    std::uint16_t pad;
    std::uint32_t data_format;
    std::uint32_t sampling_freq;
    std::uint32_t flags;
    AudioOut2UserHandle user_handle;
    std::uint32_t reserved[10];
};

struct AudioOut2Attribute {
    std::uint32_t attribute_id;
    std::int32_t reserved;
    const void* value;
    std::size_t value_size;
};

struct AudioOut2PortState {
    std::uint16_t output;
    std::uint8_t num_channels;
    std::uint8_t pad1;
    std::int16_t volume;
    std::uint16_t reroute_counter;
    std::uint32_t flags;
    std::uint32_t pad2;
    std::uint64_t reserved[6];
};

struct AudioOut2SystemState {
    float loudness;
    std::uint32_t pad;
    std::uint64_t reserved[7];
};

struct AudioOut2Position { float x; float y; float z; };

struct AudioOut2SpeakerAngle {
    std::int16_t azimuth;
    std::int16_t elevation;
};

struct AudioOut2SpeakerInfo {
    std::uint8_t type;
    std::uint8_t pad1;
    std::int16_t pad2;
    std::uint32_t available_bits;
    std::uint32_t flags;
    std::uint32_t pad3;
    AudioOut2SpeakerAngle speaker_angle[16];
};

struct AudioOut2SystemDebugStateParam {
    std::uint32_t debug_state_id;
    std::int32_t reserved;
    void* param;
    std::size_t param_size;
};

struct AudioOut2MasteringParamsHeader {
    std::uint32_t params_id;
};

struct AudioOut2MasteringStatesHeader {
    std::uint32_t states_id;
};

struct AudioOutOutputParam {
    int handle;
    const void* ptr;
};

struct AudioOutPortState {
    std::uint16_t output;
    std::uint8_t channel;
    std::uint8_t reserved[1];
    std::int16_t volume;
    std::uint16_t rerouteCounter;
    std::uint64_t flag;
    std::uint64_t activeState;
};

struct Audio3dOpenParameters {
    std::uint64_t size_this;
    std::uint32_t granularity;
    std::uint32_t rate;
    std::uint32_t max_objects;
    std::uint32_t queue_depth;
    std::uint32_t buffer_mode;
    std::uint32_t pad;
    std::uint32_t num_beds;
};

using AudioPropagationHandle = std::uint64_t;

struct AudioPropagationStructDescriptor {
    std::uint32_t id;
    std::size_t size;
};

struct AudioPropagationSystemMemory {
    AudioPropagationStructDescriptor desc;
    void* p_cpu_mem;
    std::size_t size_cpu_mem;
    void* p_gpu_mem;
    std::size_t size_gpu_mem;
};

using Ngs2Handle = std::uintptr_t;
using Ngs2BufferAllocHandler = std::int32_t (*)(void*);
using Ngs2BufferFreeHandler = std::int32_t (*)(void*);

struct Ngs2ContextBufferInfo {
    void* host_buffer;
    std::size_t host_buffer_size;
    std::uintptr_t reserved[5];
    std::uintptr_t user_data;
};

struct Ngs2BufferAllocator {
    Ngs2BufferAllocHandler alloc_handler;
    Ngs2BufferFreeHandler free_handler;
    std::uintptr_t user_data;
};

struct Ngs2SystemOption {
    std::size_t size;
    char name[16];
    std::uint32_t flags;
    std::uint32_t max_grain_samples;
    std::uint32_t num_grain_samples;
    std::uint32_t sample_rate;
    std::uint32_t reserved[6];
};

struct Ngs2SystemInfo {
    char name[16];
    Ngs2Handle system_handle;
    Ngs2ContextBufferInfo buffer_info;
    std::uint32_t uid;
    std::uint32_t min_grain_samples;
    std::uint32_t max_grain_samples;
    std::uint32_t state_flags;
    std::uint32_t rack_count;
    float last_render_ratio;
    std::int64_t last_render_tick;
    std::int64_t render_count;
    std::uint32_t sample_rate;
    std::uint32_t num_grain_samples;
};

struct Ngs2RackOption {
    std::size_t size;
    char name[16];
    std::uint32_t flags;
    std::uint32_t max_grain_samples;
    std::uint32_t max_voices;
    std::uint32_t max_input_delay_blocks;
    std::uint32_t max_matrices;
    std::uint32_t max_ports;
    std::uint32_t reserved[20];
};

struct Ngs2VoiceParamHeader {
    std::uint16_t size;
    std::int16_t next;
    std::uint32_t id;
};

struct Ngs2RenderBufferInfo {
    void* buffer;
    std::size_t buffer_size;
    std::uint32_t waveform_type;
    std::uint32_t num_channels;
};

struct Ngs2VoiceState {
    std::uint32_t state_flags;
};

struct Ngs2WaveformFormat {
    std::uint32_t waveform_type;
    std::uint32_t num_channels;
    std::uint32_t sample_rate;
    std::uint32_t config_data;
    std::uint32_t frame_offset;
    std::uint32_t frame_margin;
};

struct Ngs2WaveformBlock {
    std::uint32_t data_offset;
    std::uint32_t data_size;
    std::uint32_t num_repeats;
    std::uint32_t num_skip_samples;
    std::uint32_t num_samples;
    std::uint32_t reserved;
    std::uintptr_t user_data;
};

struct Ngs2WaveformInfo {
    Ngs2WaveformFormat format;
    std::uint32_t data_offset;
    std::uint32_t data_size;
    std::uint32_t loop_begin_position;
    std::uint32_t loop_end_position;
    std::uint32_t num_samples;
    std::uint32_t audio_unit_size;
    std::uint32_t num_audio_unit_samples;
    std::uint32_t num_audio_unit_per_frame;
    std::uint32_t audio_frame_size;
    std::uint32_t num_audio_frame_samples;
    std::uint32_t num_delay_samples;
    std::uint32_t num_blocks;
    Ngs2WaveformBlock block[4];
};

struct Ngs2PanParam {
    std::uint32_t reserved[16];
};

struct Ngs2PanWork {
    std::uint32_t reserved[64];
};

struct Ngs2GeomListenerParam {
    std::uint32_t reserved[32];
};

struct Ngs2GeomListenerWork {
    std::uint32_t reserved[64];
};

struct Ngs2GeomSourceParam {
    std::uint32_t reserved[32];
};

struct Ngs2GeomAttribute {
    std::uint32_t reserved[32];
};

struct AvPlayerAudio {
    std::uint16_t channel_count;
    std::uint8_t reserved1[2];
    std::uint32_t sample_rate;
    std::uint32_t size;
    char language_code[4];
};

struct AvPlayerVideo {
    std::uint32_t width;
    std::uint32_t height;
    float aspect_ratio;
    char language_code[4];
};

struct AvPlayerTextPosition {
    std::uint16_t top;
    std::uint16_t left;
    std::uint16_t bottom;
    std::uint16_t right;
};

struct AvPlayerTimedText {
    char language_code[4];
    std::uint16_t text_size;
    std::uint16_t font_size;
    AvPlayerTextPosition position;
};

union AvPlayerStreamDetails {
    std::uint8_t reserved[16];
    AvPlayerAudio audio;
    AvPlayerVideo video;
    AvPlayerTimedText subs;
};

struct AvPlayerFrameInfo {
    std::uint8_t* p_data;
    std::uint8_t reserved[4];
    std::uint64_t timestamp;
    AvPlayerStreamDetails details;
};

struct AvPlayerAudioEx {
    std::uint16_t channel_count;
    std::uint8_t reserved[2];
    std::uint32_t sample_rate;
    std::uint32_t size;
    std::uint8_t language_code[4];
    std::uint8_t reserved1[64];
};

struct AvPlayerVideoEx {
    std::uint32_t width;
    std::uint32_t height;
    float aspect_ratio;
    std::uint8_t language_code[4];
    std::uint32_t framerate;
    std::uint32_t crop_left_offset;
    std::uint32_t crop_right_offset;
    std::uint32_t crop_top_offset;
    std::uint32_t crop_bottom_offset;
    std::uint32_t pitch;
    std::uint8_t luma_bit_depth;
    std::uint8_t chroma_bit_depth;
    bool video_full_range_flag;
    std::uint8_t reserved1[37];
};

struct AvPlayerTimedTextEx {
    std::uint8_t language_code[4];
    std::uint8_t reserved[12];
    std::uint8_t reserved1[64];
};

union AvPlayerStreamDetailsEx {
    AvPlayerAudioEx audio;
    AvPlayerVideoEx video;
    AvPlayerTimedTextEx subs;
    std::uint8_t reserved[80];
};

struct AvPlayerFrameInfoEx {
    void* p_data;
    std::uint8_t reserved[4];
    std::uint64_t timestamp;
    AvPlayerStreamDetailsEx details;
};

using AvPlayerAllocate = void* (*)(void*, std::uint32_t, std::uint32_t);
using AvPlayerDeallocate = void (*)(void*, void*);
using AvPlayerAllocateTexture = void* (*)(void*, std::uint32_t, std::uint32_t);
using AvPlayerDeallocateTexture = void (*)(void*, void*);

struct AvPlayerMemAllocator {
    void* object_ptr;
    AvPlayerAllocate allocate;
    AvPlayerDeallocate deallocate;
    AvPlayerAllocateTexture allocate_texture;
    AvPlayerDeallocateTexture deallocate_texture;
};

using AvPlayerOpenFile = std::int32_t (*)(void*, const char*);
using AvPlayerCloseFile = std::int32_t (*)(void*);
using AvPlayerReadOffsetFile = std::int32_t (*)(void*, std::uint8_t*, std::uint64_t, std::uint32_t);
using AvPlayerSizeFile = std::uint64_t (*)(void*);

struct AvPlayerFileReplacement {
    void* object_ptr;
    AvPlayerOpenFile open;
    AvPlayerCloseFile close;
    AvPlayerReadOffsetFile read_offset;
    AvPlayerSizeFile size;
};

using AvPlayerEventCallback = void (*)(void*, std::int32_t, std::int32_t, void*);

struct AvPlayerEventReplacement {
    void* object_ptr;
    AvPlayerEventCallback event_callback;
};

struct AvPlayerInitData {
    AvPlayerMemAllocator memory_replacement;
    AvPlayerFileReplacement file_replacement;
    AvPlayerEventReplacement event_replacement;
    std::int32_t debug_level;
    std::uint32_t base_priority;
    std::int32_t num_output_video_framebuffers;
    bool auto_start;
    std::uint8_t reserved[3];
    const char* default_language;
};

struct AvPlayerInternal {};

struct AudiodecAuInfo {
    std::uint32_t ui_size;
    void* p_au_addr;
    std::uint32_t ui_au_size;
};

struct AudiodecPcmItem {
    std::uint32_t ui_size;
    void* p_pcm_addr;
    std::uint32_t ui_pcm_size;
};

struct AudiodecCtrl {
    void* pParam;
    void* pBsiInfo;
    AudiodecAuInfo* pAuInfo;
    AudiodecPcmItem* pPcmItem;
};

struct VoiceInitParam {
    std::int32_t app_type;
    std::uint64_t on_event;
    void* user_data;
    std::uint8_t reserved[32 - sizeof(std::int32_t) - sizeof(std::uint64_t) - sizeof(void*)];
};

struct VoicePortParam {
    std::int32_t port_type;
    std::uint16_t threshold;
    std::uint16_t mute;
    float volume;
    union {
        struct { std::int32_t bitrate; } voice;
        struct { std::uint32_t buffer_size; std::int32_t data_type; std::int32_t sample_rate; } pcmaudio;
        struct { std::int32_t user_id; std::int32_t type; std::int32_t index; } device;
    };
};

struct VoicePortInfo {
    std::int32_t port_type;
    std::int32_t state;
    std::uint32_t* edge;
    std::uint32_t byte_count;
    std::uint32_t frame_size;
    std::uint16_t edge_count;
    std::uint16_t reserved;
};

struct VoiceStartParam {
    void* container;
    std::uint32_t mem_size;
    std::uint8_t reserved[32 - sizeof(void*) - sizeof(std::uint32_t)];
};

struct PadTouchPadInformation {
 float pixelDensity;
 struct { std::uint16_t x; std::uint16_t y; } resolution;
};

struct PadStickInformation {
 std::uint8_t deadZoneLeft;
 std::uint8_t deadZoneRight;
};

struct PadControllerInformation {
 PadTouchPadInformation touchPadInfo;
 PadStickInformation stickInfo;
 std::uint8_t connectionType;
 std::uint8_t connectedCount;
 bool connected;
 std::uint8_t pad[3];
 std::int32_t deviceClass;
 std::uint8_t reserve[8];
};

struct PadVibrationParam { std::uint8_t large_motor; std::uint8_t small_motor; };
struct PadLightBarParam { std::uint8_t r; std::uint8_t g; std::uint8_t b; };

struct PadDeviceClassData {
 std::int32_t deviceClass;
 bool dataValid;
 std::uint8_t pad[3];
 union {
  struct {
   float angle;
   std::uint16_t wheel;
   std::uint16_t accelerator;
   std::uint16_t brake;
   std::uint16_t clutch;
   std::uint16_t handBrake;
   std::uint8_t gear;
   std::uint8_t reserved[1];
  } steeringWheel;
  struct {
   std::uint8_t toneNumber;
   std::uint8_t whammyBar;
   std::uint8_t tilt;
   std::uint8_t fret;
   std::uint8_t fretSolo;
   std::uint8_t reserved[11];
  } guitar;
  struct {
   std::uint8_t snare;
   std::uint8_t tom1;
   std::uint8_t tom2;
   std::uint8_t floorTom;
   std::uint8_t hihatCymbal;
   std::uint8_t rideCymbal;
   std::uint8_t crashCymbal;
   std::uint8_t reserved[9];
  } drum;
  std::uint8_t data[16];
 } classData;
};

struct PadDeviceClassExtendedInformation {
 std::int32_t deviceClass;
 std::uint8_t reserved[4];
 union {
  struct {
   std::uint8_t capability;
   std::uint8_t reserved1[1];
   std::uint16_t maxPhysicalWheelAngle;
   std::uint8_t reserved2[8];
  } steeringWheel;
  struct {
   std::uint8_t capability;
   std::uint8_t quantityOfSelectorSwitch;
   std::uint8_t reserved[10];
  } guitar;
  struct {
   std::uint8_t capability;
   std::uint8_t reserved[11];
  } drum;
  std::uint8_t data[12];
 } classData;
};
struct PadTriggerEffectStateInformation {
    std::int32_t state[2];
};

struct PadData {
    std::uint32_t buttons;
    std::uint8_t left_stick_x;
    std::uint8_t left_stick_y;
    std::uint8_t right_stick_x;
    std::uint8_t right_stick_y;
    std::uint8_t analog_buttons_l2;
    std::uint8_t analog_buttons_r2;
    std::uint8_t padding[2];
    float orientation_x;
    float orientation_y;
    float orientation_z;
    float orientation_w;
    float acceleration_x;
    float acceleration_y;
    float acceleration_z;
    float angular_velocity_x;
    float angular_velocity_y;
    float angular_velocity_z;
    std::uint8_t touch_data_touch_num;
    std::uint8_t touch_data_reserve[3];
    std::uint32_t touch_data_reserve1;
    std::uint16_t touch_data_touch0_x;
    std::uint16_t touch_data_touch0_y;
    std::uint8_t touch_data_touch0_id;
    std::uint8_t touch_data_touch0_reserve[3];
    std::uint16_t touch_data_touch1_x;
    std::uint16_t touch_data_touch1_y;
    std::uint8_t touch_data_touch1_id;
    std::uint8_t touch_data_touch1_reserve[3];
    bool connected;
    std::uint64_t timestamp;
    std::uint32_t extension_unit_data_extension_unit_id;
    std::uint8_t extension_unit_data_reserve[1];
    std::uint8_t extension_unit_data_data_length;
    std::uint8_t extension_unit_data_data[10];
    std::uint8_t connected_count;
    std::uint8_t reserve[2];
    std::uint8_t device_unique_data_len;
    std::uint8_t device_unique_data[12];
};

struct MouseData {
    std::uint64_t timestamp;
    bool connected;
    std::uint8_t padding[3];
    std::uint32_t buttons;
    std::int32_t x_axis;
    std::int32_t y_axis;
    std::int32_t wheel;
    std::int32_t tilt;
    std::uint8_t reserved[8];
};

constexpr std::uint32_t KEYBOARD_MAX_KEYCODES = 6;

struct KeyboardData {
    std::uint64_t timestamp;
    bool intercepted;
    std::uint8_t reserve1[7];
    bool connected;
    std::int32_t length;
    std::uint32_t led;
    std::uint32_t modifier_key;
    std::uint16_t key_code[KEYBOARD_MAX_KEYCODES];
    std::uint8_t reserve2[32];
};

struct KeyboardCharData {
    bool processed;
    std::int32_t length;
    std::uint16_t char_code;
    std::uint8_t reserve[8];
};

struct ImeColor {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
    std::uint8_t a;
};

struct ImeKeycode {
    std::uint16_t keycode;
    char16_t character;
    std::uint32_t status;
    std::uint32_t type;
    std::int32_t user_id;
    std::uint32_t resource_id;
    std::uint64_t timestamp;
};

using ImeTextFilter = std::int32_t (*)(char16_t* out_text, std::uint32_t* out_text_length, const char16_t* source_text, std::uint32_t source_text_length);
using ImeExtKeyboardFilter = int (*)(const ImeKeycode* source_keycode, std::uint16_t* out_keycode, std::uint32_t* out_status, void* reserved);

struct ImeExtendedParam {
    std::uint32_t option;
    ImeColor color_base;
    ImeColor color_line;
    ImeColor color_text_field;
    ImeColor color_preedit;
    ImeColor color_button_default;
    ImeColor color_button_function;
    ImeColor color_button_symbol;
    ImeColor color_text;
    ImeColor color_special;
    std::uint32_t priority;
    const char* additional_dictionary_path;
    ImeExtKeyboardFilter ext_keyboard_filter;
    std::uint32_t disable_device;
    std::uint32_t ext_keyboard_mode;
    std::int8_t reserved[60];
};

using ExtendedParam = ImeExtendedParam;

struct ImeCaret {
    float x;
    float y;
    std::uint32_t height;
    std::uint32_t index;
};

using Caret = ImeCaret;

struct ImeTextGeometry {
    float x;
    float y;
    std::uint32_t width;
    std::uint32_t height;
};

using TextGeometry = ImeTextGeometry;

struct ImeRect {
    float x;
    float y;
    std::uint32_t width;
    std::uint32_t height;
};

enum class TextAreaMode : std::uint32_t { Disable = 0, Edit = 1, Preedit = 2, Select = 3 };

struct ImeTextAreaProperty {
    TextAreaMode mode;
    std::uint32_t index;
    std::int32_t length;
};

struct ImeEditText {
    char16_t* str;
    std::uint32_t caret_index;
    std::uint32_t area_num;
    ImeTextAreaProperty text_area[4];
};

struct KeyboardResourceIdArray {
    std::int32_t user_id;
    std::uint32_t resource_id[5];
};

union ImeEventParam {
    ImeRect rect;
    ImeEditText text;
    std::uint32_t caret_move;
    ImeKeycode keycode;
    KeyboardResourceIdArray resource_id_array;
    std::uint64_t reserved[8];
};

struct ImeEvent {
    std::uint32_t id;
    ImeEventParam param;
};

using EventHandler = void (*)(void* arg, const ImeEvent* event);

struct Param {
    std::int32_t user_id;
    std::uint32_t type;
    std::uint64_t supported_languages;
    std::uint32_t enter_label;
    std::uint32_t input_method;
    ImeTextFilter filter;
    std::uint32_t option;
    std::uint32_t max_text_length;
    char16_t* input_text_buffer;
    float posx;
    float posy;
    std::uint32_t horizontal_alignment;
    std::uint32_t vertical_alignment;
    void* work;
    void* arg;
    EventHandler handler;
    std::int8_t reserved[8];
};

struct KeyboardParam {
    std::uint32_t option;
    std::int8_t reserved1[4];
    void* arg;
    EventHandler handler;
    std::int8_t reserved2[8];
};

struct KeyboardInfo {
    std::int32_t user_id;
    std::uint32_t device;
    std::uint32_t type;
    std::uint32_t repeat_delay;
    std::uint32_t repeat_rate;
    std::uint32_t status;
    std::int8_t reserved[12];
};

struct ImeDialogResult {
    std::uint32_t endstatus;
    std::int8_t reserved[12];
};

using Result = ImeDialogResult;

struct PositionAndForm {
    std::uint32_t type;
    float posx;
    float posy;
    std::uint32_t horizontal_alignment;
    std::uint32_t vertical_alignment;
    std::uint32_t width;
    std::uint32_t height;
};

using FontHandle = void*;
using FontLibrary = void*;
using FontLibrarySelection = void*;
using FontRenderer = void*;
using FontRendererSelection = void*;

using FontMemoryDestroyCallback = void (*)(void*);

struct FontMemoryInterface {
    void* (*malloc_func)(std::size_t);
    void (*free_func)(void*);
};

struct FontMemory {
    std::uint16_t type;
    std::uint16_t attr;
    std::uint32_t size;
    void* address;
    void* mspace_object;
    const FontMemoryInterface* mem_interface;
    FontMemoryDestroyCallback destroy_callback;
    void* destroy_object;
    void* user_object;
    void* parent_object;
};

using FontOpenDetail = void;

struct FontHorizontalLayout {
    float base_line_y;
    float line_height;
    float effect_height;
};

struct FontVerticalLayout {
    float base_line_x;
    float line_width;
    float effect_width;
};

struct FontGlyphMetrics {
    float width;
    float height;
    struct {
        float bearing_x;
        float bearing_y;
        float advance;
    } horizontal;
    struct {
        float bearing_x;
        float bearing_y;
        float advance;
    } vertical;
};

struct FontRenderSurface {
    void* buffer;
    std::int32_t width_byte;
    std::int8_t pixel_size_byte;
    std::uint8_t system_ext0;
    std::uint8_t system_ext1;
    std::uint8_t system_ext2;
    std::int32_t width;
    std::int32_t height;
    struct {
        std::uint32_t x0;
        std::uint32_t y0;
        std::uint32_t x1;
        std::uint32_t y1;
    } scissor;
    std::uint32_t system_use[22];
};

struct FontRenderCharacter { std::uint8_t reserved[128]; };

struct FontTransImage {
    std::uint8_t* address;
    std::uint32_t width_byte;
    std::uint32_t image_width;
    std::uint32_t image_height;
};

struct FontSurfaceImage {
    std::uint8_t* address;
    std::uint32_t width_byte;
    std::uint8_t pixel_size_byte;
    std::uint8_t pixel_format;
};

struct FontGlyphImageMetrics {
    float bearing_x;
    float bearing_y;
    float advance;
    float stride;
    std::uint32_t width;
    std::uint32_t height;
};

struct FontRenderResult {
    const FontTransImage* trans_image;
    FontSurfaceImage surface_image;
    struct {
        std::uint32_t x;
        std::uint32_t y;
        std::uint32_t w;
        std::uint32_t h;
    } update_rect;
    FontGlyphImageMetrics image_metrics;
};

using FontCreateStringOrdersFunction = void (*)(void*, void*);

struct FontCreateStringDetail {
    std::uint16_t detail_id;
    std::uint8_t detail_type;
    std::uint8_t detections;
    std::uint32_t orders_option;
    FontHandle default_font;
    struct {
        FontCreateStringOrdersFunction function;
        void* object;
    } orders;
};

struct FontGenerateGlyphDetail { std::uint8_t reserved[64]; };

struct FontString { void* system_use[32]; };

struct FontWriting { void* system_use[32]; };

struct FontWritingExtent {
    float top;
    float bottom;
    float left;
    float right;
};

struct FontWritingMetrics {
    float advance_x;
    float advance_y;
    FontWritingExtent extent;
};

struct FontWritingStep {
    float x;
    float y;
    float advance_x;
    float advance_y;
    FontHandle font;
    struct {
        std::uint32_t character_count : 8;
        std::uint32_t invisible_glyph : 1;
    } profile;
    std::uint32_t glyph_code;
    struct {
        float x;
        float y;
    } positioning;
    FontGlyphMetrics glyph_metrics;
};

struct FontWritingLine { std::uint8_t reserved[128]; };

struct FontWritingLineStep {
    float x;
    float y;
    float advance_x;
    float advance_y;
    float spacing_progress;
    void* writing_orderer;
    struct {
        float x;
        float y;
    } adjusting;
    FontWritingMetrics metrics;
};

using FontTextParseFunction = int (*)(void*, std::uint32_t, void*);

struct FontTextSource {
    std::uint64_t system_use0;
    const void* start;
    const void* end;
    const void* current;
    FontTextParseFunction text_parser;
    void* text_object;
    FontHandle default_font;
    void* system_use[5];
};

struct FontTextCharacter { std::uint8_t reserved[64]; };

union FontTextCodes {
    struct {
        void* order;
        std::uint32_t code;
        std::uint32_t reserved;
    } text;
    void* system_reserved[8];
};

struct NetEtherAddr { std::uint8_t data[6]; };

union NetEpollData {
    void* ptr;
    std::uint32_t u32;
    int fd;
    std::uint64_t u64;
};

struct NetEpollEvent {
    std::uint32_t events;
    std::uint32_t reserved;
    std::uint64_t ident;
    NetEpollData data;
};

struct NetCtlNatInfo { std::uint8_t opaque[128]; };

union NetCtlInfo { std::uint8_t opaque[256]; };

using NetCtlCallback = void (*)(int, void*);

struct HttpEpoll {};
using HttpEpollHandle = HttpEpoll*;
using HttpsCallback = int (*)(int, unsigned int, void* const*, int, void*);

struct HttpNBEvent { std::uint8_t opaque[64]; };

struct SceHttpUriElement {
    int opaque = 0;
    char* scheme = nullptr;
    char* username = nullptr;
    char* password = nullptr;
    char* hostname = nullptr;
    char* path = nullptr;
    char* query = nullptr;
    char* fragment = nullptr;
    std::uint16_t port = 0;
    std::uint8_t reserved[10]{};
};

struct Http2AsyncResult {
    int event_type;
    int req_id;
    int result;
    std::uint8_t padding[4];
    void* reserved;
};

struct NpTitleId { char data[13]; char pad[3]; };
struct NpTitleSecret { std::uint8_t data[128]; };
struct NpContentRestriction { std::uint8_t opaque[128]; };
struct NpOnlineId { char data[17]; char pad[3]; };
struct NpId { NpOnlineId online_id; std::uint8_t opaque[4]; };
struct NpCreateAsyncRequestParameter { std::uint8_t opaque[64]; };
struct NpCheckPremiumParameter { std::uint8_t opaque[64]; };
struct NpCheckPremiumResult { std::uint8_t opaque[64]; };

struct NpUnifiedEntitlementLabel {
    char data[17];
    char padding[3];
};

struct NpEntitlementAccessInitParam { char reserved[32]; };
struct NpEntitlementAccessBootParam { char reserved[32]; };

struct NpEntitlementAccessAddcontEntitlementInfo {
    NpUnifiedEntitlementLabel entitlement_label;
    std::uint32_t package_type;
    std::uint32_t download_status;
};

struct NpTrophy2Progress { std::uint32_t value; };

struct NpTrophy2GameDetails {
    std::uint32_t num_groups;
    std::uint32_t num_trophies;
    std::uint32_t num_platinum;
    std::uint32_t num_gold;
    std::uint32_t num_silver;
    std::uint32_t num_bronze;
    char title[128];
};

struct NpTrophy2GameData {
    std::uint32_t unlocked_trophies;
    std::uint32_t unlocked_platinum;
    std::uint32_t unlocked_gold;
    std::uint32_t unlocked_silver;
    std::uint32_t unlocked_bronze;
    std::uint32_t progress_percentage;
};

struct NpTrophy2GroupDetails {
    std::int32_t group_id;
    std::uint32_t num_trophies;
    std::uint32_t num_platinum;
    std::uint32_t num_gold;
    std::uint32_t num_silver;
    std::uint32_t num_bronze;
    char title[128];
};

struct NpTrophy2GroupData {
    std::int32_t group_id;
    std::uint32_t unlocked_trophies;
    std::uint32_t unlocked_platinum;
    std::uint32_t unlocked_gold;
    std::uint32_t unlocked_silver;
    std::uint32_t unlocked_bronze;
    std::uint32_t progress_percentage;
    std::uint8_t reserved[4];
};

struct NpTrophy2Details {
    std::int32_t trophy_id;
    std::int32_t trophy_grade;
    std::int32_t group_id;
    bool hidden;
    bool has_reward;
    std::uint8_t reserved2[2];
    NpTrophy2Progress target;
    char name[128];
    char description[1024];
    char reward[128];
};

struct NpTrophy2Data {
    std::int32_t trophy_id;
    bool unlocked;
    std::uint8_t reserved[3];
    NpTrophy2Progress progress;
    std::uint64_t timestamp_tick;
};

struct NpUniversalDataSystemInitParam {
    std::size_t size;
    std::size_t pool_size;
};

struct NpUniversalDataSystemMemoryStat {
    std::size_t pool_size;
    std::size_t max_inuse_size;
    std::size_t current_inuse_size;
};

struct NpUniversalDataSystemEvent {};
struct NpUniversalDataSystemEventPropertyObject {};
struct NpUniversalDataSystemEventPropertyArray {};

struct NpUniversalDataSystemStorageStat {
    std::size_t in_events;
    std::size_t out_events;
    std::size_t lost_events;
    std::size_t max_inuse_size;
    std::size_t current_events;
    std::size_t current_inuse_size;
    std::size_t current_free_size;
};

constexpr std::uint32_t NP_GAME_INTENT_DATA_MAX_SIZE = 1024;
constexpr std::uint32_t NP_GAME_INTENT_TYPE_MAX_SIZE = 64;

struct NpGameIntentData {
    std::uint8_t data[NP_GAME_INTENT_DATA_MAX_SIZE];
    std::uint8_t padding[7];
};

struct NpGameIntentInfo {
    std::size_t size;
    std::int32_t user_id;
    char intent_type[NP_GAME_INTENT_TYPE_MAX_SIZE];
    std::uint8_t padding[7];
    std::uint8_t reserved[256];
    NpGameIntentData intent_data;
};

struct NpWebApi2ResponseInformationOption {
    std::int32_t http_status;
    char* error_object;
    std::size_t error_object_size;
    std::size_t response_data_size;
};

struct GameUpdateCheckParam {
    std::size_t size;
    std::uint32_t option;
    std::uint32_t reserved[9];
};

struct GameUpdateCheckResult {
    std::size_t size;
    bool found;
    bool addcont_found;
    char padding[2];
    char content_version[11];
    char padding2[1];
    std::uint32_t reserved[6];
};

struct GameUpdateAddcontVersionInfo {
    std::size_t size;
    bool found;
    char content_version[11];
    std::uint32_t reserved[6];
};

struct SaveDataMountPoint { char data[16]; };

struct SaveDataParam {
    char title[128];
    char sub_title[128];
    char detail[1024];
    std::uint32_t user_param;
    int pad;
    std::int64_t mtime;
    std::uint8_t reserved[32];
};

struct SaveDataIcon {
    void* buf;
    std::size_t buf_size;
    std::size_t data_size;
    std::uint8_t reserved[32];
};

struct SaveDataMountResult {
    SaveDataMountPoint mount_point;
    std::uint64_t required_blocks;
    std::uint32_t unused;
    std::uint32_t mount_status;
    std::uint8_t reserved[28];
    int pad;
};

struct SaveDataMountInfo {
    std::uint64_t blocks;
    std::uint64_t free_blocks;
    std::uint8_t reserved[32];
};

struct SceSaveDataTitleId { char data[10]; char pad[2]; };
struct SceSaveDataDirName { char data[33]; char pad[3]; };
struct SaveDataSearchInfo { std::uint8_t opaque[128]; };
struct SaveDataMemoryData { void* buf; std::size_t buf_size; std::size_t offset; };

struct SaveDataMount3 {
    int user_id;
    int pad;
    const SceSaveDataDirName* dir_name;
    std::uint64_t blocks;
    std::uint64_t system_blocks;
    std::uint32_t mount_mode;
    int pad2;
    std::int32_t resource;
    std::uint8_t reserved[32];
};

struct SaveDataDirNameSearchCond {
    std::int32_t user_id;
    std::int32_t pad;
    const SceSaveDataTitleId* title_id;
    const SceSaveDataDirName* dir_name;
    std::uint32_t key;
    std::uint32_t order;
    std::uint8_t reserved[32];
};

struct SaveDataDirNameSearchResult {
    std::uint32_t hit_num;
    std::int32_t pad;
    SceSaveDataDirName* dir_names;
    std::uint32_t dir_names_num;
    std::uint32_t set_num;
    SaveDataParam* params;
    SaveDataSearchInfo* infos;
    std::uint8_t reserved[12];
    std::int32_t pad2;
};

struct SaveDataMemoryGet2 {
    std::int32_t user_id;
    std::uint8_t padding[4];
    SaveDataMemoryData* data;
    SaveDataParam* param;
    SaveDataIcon* icon;
    std::uint32_t slot_id;
    std::uint8_t reserved[28];
};

struct SaveDataMemorySetup2 {
    std::uint32_t option;
    std::int32_t user_id;
    std::size_t memory_size;
    std::size_t icon_memory_size;
    const SaveDataParam* init_param;
    const SaveDataIcon* init_icon;
    std::uint32_t slot_id;
    std::uint8_t reserved[20];
};

struct SaveDataMemorySetupResult {
    std::size_t existed_memory_size;
    std::uint8_t reserved[16];
};

struct SaveDataMemorySet2 {
    std::int32_t user_id;
    std::uint8_t padding[4];
    const SaveDataMemoryData* data;
    const SaveDataParam* param;
    const SaveDataIcon* icon;
    std::uint32_t data_num;
    std::uint32_t slot_id;
    std::uint8_t reserved[24];
};

struct SaveDataTransferringMount {
    std::int32_t user_id;
    const SceSaveDataTitleId* title_id;
    const SceSaveDataDirName* dir_name;
    const void* fingerprint;
    std::uint8_t reserved[32];
};

struct SaveDataPrepareParam {
    std::int32_t resource;
    std::uint32_t prepare_mode;
    std::uint8_t reserved[32];
};

struct SaveDataCommitParam {
    std::int32_t resource;
    std::uint32_t commit_mode;
    std::uint8_t reserved[32];
};

struct SaveDataDelete {
    std::int32_t user_id;
    std::int32_t pad;
    const SceSaveDataTitleId* title_id;
    const SceSaveDataDirName* dir_name;
    std::uint32_t unused;
    std::uint8_t reserved[32];
    std::int32_t pad2;
};

struct SaveDataEvent {
    std::uint32_t type;
    std::int32_t error_code;
    std::int32_t user_id;
    std::uint8_t padding[4];
    SceSaveDataTitleId title_id;
    SceSaveDataDirName dir_name;
    std::uint8_t reserved[40];
};

struct SaveDataBackup {
    std::int32_t user_id;
    std::int32_t pad;
    const SceSaveDataTitleId* title_id;
    const SceSaveDataDirName* dir_name;
    const void* fingerprint;
    std::uint8_t reserved[32];
};

struct AppContentInitParam { char reserved[32]; };

struct AppContentBootParam {
    char reserved1[4];
    std::uint32_t attr;
    char reserved2[32];
};

struct AppContentMountPoint { char data[16]; };

struct ContentExportInitParam2 {
    void* malloc_func;
    void* free_func;
    void* user_data;
    std::size_t buffer_size;
    std::int64_t reserved0;
    std::int64_t reserved1;
};

struct ContentSearchInitParam { std::size_t memory_size; };

struct ContentDeleteInitParam {
    char reserved1[4];
    std::size_t heap_size;
    char reserved2[32];
};

struct PngDecCreateParam {
    std::uint32_t this_size;
    std::uint32_t attribute;
    std::uint32_t max_image_width;
};

struct PngDecParseParam {
    const void* png_mem_addr;
    std::uint32_t png_mem_size;
    std::uint32_t reserved0;
};

struct PngDecDecodeParam {
    const void* png_mem_addr;
    void* image_mem_addr;
    std::uint32_t png_mem_size;
    std::uint32_t image_mem_size;
    std::uint16_t pixel_format;
    std::uint16_t alpha_value;
    std::uint32_t image_pitch;
};

struct PngDecImageInfo {
    std::uint32_t image_width;
    std::uint32_t image_height;
    std::uint16_t color_space;
    std::uint16_t bit_depth;
    std::uint32_t image_flag;
};

struct PlayGoInitParams {
    const void* buf_addr;
    std::uint32_t buf_size;
    std::uint32_t reserved;
};

struct PlayGoToDo {
    std::uint16_t chunk_id;
    std::int8_t locus;
    std::int8_t reserved;
};

struct PlayGoProgress {
    std::uint64_t progress_size;
    std::uint64_t total_size;
};

union PlayGoOptionalChunk {
    std::uint64_t bitmask;
    std::uint64_t languages;
    std::uint64_t scenarios;
};

using RudpEventHandler = void (*)(int ctx_id, int event_id, int error_code, void* arg);

struct SystemServiceStatus {
    std::int32_t event_num = 0;
    bool is_system_ui_overlaid = false;
    bool is_in_background_execution = false;
    bool is_vr_play_area_overlaid = false;
    std::uint8_t reserved[127] = {};
};

struct SystemServiceEvent {
    std::int32_t event_type;
    std::uint8_t data[8192];
};

struct SystemServiceDisplaySafeAreaInfo {
    float ratio;
    std::uint8_t reserved[128];
};

struct SystemServiceHdrToneMapLuminance {
    float max_full_frame_tone_map_luminance;
    float max_tone_map_luminance;
    float min_tone_map_luminance;
};

struct SystemGestureVector2 { float x; float y; };

struct SystemGesturePrimitiveTouchEvent {
    std::int32_t event_state;
    std::uint16_t primitive_id;
    std::uint8_t is_updated;
    std::uint8_t reserved0;
    SystemGestureVector2 pressed_position;
    SystemGestureVector2 current_position;
    SystemGestureVector2 delta_vector;
    std::uint64_t delta_time;
    std::uint64_t elapsed_time;
    std::uint8_t reserve[32];
};

struct SystemGestureRectangle {
    float x;
    float y;
    float width;
    float height;
    std::uint8_t reserve[8];
};

struct SystemGestureTouchRecognizer { std::uint64_t reserve[361]; };

struct SystemGestureTouchRecognizerInformation {
    std::int32_t gesture_type;
    SystemGestureRectangle rectangle;
    std::uint64_t updated_time;
    std::uint8_t reserve[256];
};

struct SystemGestureTouchEvent { std::uint8_t reserve[168]; };

struct UserServiceLoginUserIdList { int user_id[4]; };

struct SceUserServiceEvent {
    std::uint32_t event_type;
    int user_id;
};

struct UserServiceGamePresets {
    std::size_t this_size;
    std::uint32_t difficulty;
    std::uint32_t priority;
    std::uint32_t invert_vertical_view_for_1st_person_view;
    std::uint32_t invert_horizontal_view_for_1st_person_view;
    std::uint32_t invert_vertical_view_for_3rd_person_view;
    std::uint32_t invert_horizontal_view_for_3rd_person_view;
    std::uint32_t display_sub_titles;
    std::uint32_t audio_language;
};

struct UltMutexOptParam {
    std::uint32_t reserved_header[2];
    std::uint32_t attribute;
    std::uint32_t reserved0;
};

struct UltUlthreadRuntimeOptParam { std::uint8_t bytes[128]; };

using UltUlthreadEntry = std::int32_t (*)(std::uint64_t);

struct VideoOutBufferAttribute2 {
    std::uint32_t reserved0;
    std::uint32_t tiling_mode;
    std::uint32_t aspect_ratio;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t pitch_in_pixel;
    std::uint64_t option;
    std::uint64_t pixel_format;
    std::uint64_t dcc_cb_register_clear_color;
    std::uint32_t dcc_control;
    std::uint32_t pad0;
    std::uint64_t reserved1[3];
};

struct VideoOutBuffers {
    const void* data;
    const void* metadata;
    const void* reserved[2];
};

struct VideoOutFlipStatus {
    std::uint64_t count = 0;
    std::uint64_t processTime = 0;
    std::uint64_t reserved0 = 0;
    std::int64_t flipArg = 0;
    std::uint64_t reserved1 = 0;
    std::uint64_t processTimeCounter = 0;
    std::int32_t gcQueueNum = 0;
    std::int32_t flipPendingNum = 0;
    std::int32_t currentBuffer = 0;
    std::uint32_t reserved2 = 0;
    std::uint64_t submitProcessTimeCounter = 0;
    std::uint64_t reserved3[7] = {};
};

struct VideoOutVblankStatus {
    std::uint64_t count = 0;
    std::uint64_t processTime = 0;
    std::uint64_t reserved = 0;
    std::uint64_t processTimeCounter = 0;
    std::uint8_t flags = 0;
    std::uint8_t phase = 0;
    std::uint8_t pad1[6] = {};
};

struct VideoOutOutputStatus {
    std::uint32_t resolution = 0;
    std::uint32_t dynamicRange = 0;
    std::uint64_t refreshRate = 0;
    std::uint64_t flags = 0;
    std::uint64_t reserved[3] = {};
};

struct VideoOutOutputOptions { std::uint32_t internalData[16] = {}; };


using atexit_func_t = void (*)();

struct InitEnvParams {
    int argc;
    std::uint32_t pad;
    const char* argv[3];
};

struct LibcHeapInfo {
    std::uint64_t size;
    std::uint32_t unknown1;
    std::uint32_t unknown2;
    std::uint64_t* mspace_atomic_id_mask;
    std::uint64_t* mstate_table;
};

using Info = LibcHeapInfo;

#define VA_ARGS \
    std::uint64_t rdi, std::uint64_t rsi, std::uint64_t rdx, std::uint64_t rcx, \
    std::uint64_t r8, std::uint64_t r9, std::uint64_t overflow_arg_area, \
    __m128 xmm0, __m128 xmm1, __m128 xmm2, __m128 xmm3, \
    __m128 xmm4, __m128 xmm5, __m128 xmm6, __m128 xmm7, ...

struct Packet {
    std::uint32_t* addr;
    std::uint32_t dw_num;
    std::uint8_t flags;
    std::uint8_t reserved[3];
};

#endif
