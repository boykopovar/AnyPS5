#ifndef CORE_LIBS_SCE_TYPES_HPP
#define CORE_LIBS_SCE_TYPES_HPP

#include <cstddef>
#include <cstdint>
#include <cstdio>

typedef float __m128 __attribute__((__vector_size__(16), __aligned__(16)));

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
    std::uint8_t opaque[256];
};

struct Label {
    volatile std::uint64_t value;
};

struct Shader {
    std::uint8_t opaque[128];
};

struct ShaderRegister {
    std::uint32_t offset;
    std::uint32_t value;
};

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
    std::uint8_t opaque[64];
};

struct AcmBatchInfo {
    std::uint8_t opaque[64];
};

struct AjmBatchError {
    std::uint8_t opaque[64];
};

struct AjmBatchInfo {
    std::uint8_t opaque[128];
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

struct AudioOut2ContextParam { std::uint8_t opaque[128]; };
struct AudioOut2PortParam { std::uint8_t opaque[128]; };
struct AudioOut2Attribute { std::uint8_t opaque[64]; };
struct AudioOut2PortState { std::uint8_t opaque[64]; };
struct AudioOut2SystemState { std::uint8_t opaque[64]; };
struct AudioOut2Position { float x; float y; float z; };
struct AudioOut2SpeakerInfo { std::uint8_t opaque[256]; };
struct AudioOut2SystemDebugStateParam { std::uint8_t opaque[64]; };
struct AudioOut2MasteringParamsHeader { std::uint8_t opaque[128]; };
struct AudioOut2MasteringStatesHeader { std::uint8_t opaque[128]; };

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

struct Audio3dOpenParameters { std::uint8_t opaque[256]; };

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

struct Ngs2SystemOption { std::uint8_t opaque[128]; };
struct Ngs2SystemInfo { std::uint8_t opaque[128]; };
struct Ngs2RackOption { std::uint8_t opaque[128]; };
struct Ngs2BufferAllocator { std::uint8_t opaque[64]; };
struct Ngs2VoiceParamHeader { std::uint8_t opaque[64]; };
struct Ngs2RenderBufferInfo { std::uint8_t opaque[64]; };
struct Ngs2ContextBufferInfo { std::uint8_t opaque[64]; };
struct Ngs2VoiceState { std::uint8_t opaque[128]; };
struct Ngs2WaveformFormat { std::uint8_t opaque[64]; };
struct Ngs2WaveformBlock { std::uint8_t opaque[64]; };
struct Ngs2WaveformInfo { std::uint8_t opaque[128]; };
struct Ngs2PanWork { std::uint8_t opaque[256]; };
struct Ngs2PanParam { std::uint8_t opaque[64]; };
struct Ngs2GeomListenerParam { std::uint8_t opaque[128]; };
struct Ngs2GeomListenerWork { std::uint8_t opaque[256]; };
struct Ngs2GeomSourceParam { std::uint8_t opaque[128]; };
struct Ngs2GeomAttribute { std::uint8_t opaque[128]; };

struct AvPlayerInitData { std::uint8_t opaque[256]; };
struct AvPlayerFrameInfoEx { std::uint8_t opaque[256]; };
struct AvPlayerFrameInfo { std::uint8_t opaque[256]; };
struct AvPlayerInternal { std::uint8_t opaque[1]; };

struct AudiodecAuInfo { std::uint8_t opaque[64]; };
struct AudiodecPcmItem { std::uint8_t opaque[64]; };

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

struct PadControllerInformation { std::uint8_t opaque[136]; };
struct PadVibrationParam { std::uint8_t large_motor; std::uint8_t small_motor; };
struct PadLightBarParam { std::uint8_t r; std::uint8_t g; std::uint8_t b; };
struct PadDeviceClassData { std::uint8_t opaque[64]; };
struct PadDeviceClassExtendedInformation { std::uint8_t opaque[64]; };
struct PadTriggerEffectStateInformation { std::uint8_t opaque[64]; };

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

struct FontOpenDetail { std::uint8_t opaque[64]; };
struct FontHorizontalLayout { std::uint8_t opaque[64]; };
struct FontVerticalLayout { std::uint8_t opaque[64]; };
struct FontGlyphMetrics { std::uint8_t opaque[128]; };
struct FontRenderSurface { std::uint8_t opaque[128]; };
struct FontRenderCharacter { std::uint8_t opaque[128]; };
struct FontRenderResult { std::uint8_t opaque[64]; };
struct FontCreateStringDetail { std::uint8_t opaque[64]; };
struct FontGenerateGlyphDetail { std::uint8_t opaque[64]; };
struct FontString { std::uint8_t opaque[256]; };
struct FontWriting { std::uint8_t opaque[256]; };
struct FontWritingMetrics { std::uint8_t opaque[128]; };
struct FontWritingStep { std::uint8_t opaque[64]; };
struct FontWritingLine { std::uint8_t opaque[128]; };
struct FontWritingLineStep { std::uint8_t opaque[64]; };
struct FontTextSource { std::uint8_t opaque[256]; };
struct FontTextCharacter { std::uint8_t opaque[64]; };
struct FontTextCodes { std::uint8_t opaque[64]; };

using FontTextParseFunction = int (*)(void*, std::uint32_t, void*);

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
