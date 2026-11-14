#include "prx/libSceHidControl/include/Api.hpp"
#include "prx/libSceHidControl/include/InventoryExports.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorInvalidArgument = static_cast<int>(0x803b3001);
constexpr int ErrorNotInitialized = static_cast<int>(0x803b3002);
constexpr std::uint64_t MaximumUpdateChunk = 0x7fd5;
constexpr std::uint64_t UpdateImageSize = 0x40;
constexpr std::uint32_t QueryCommand = 0xc0184852;
constexpr std::uint32_t StateCommand = 0xc0184853;
constexpr std::uint32_t SettingCommand = 0x80104850;
constexpr std::uint32_t FocusCommand = 0x80184808;
constexpr std::uint32_t DeviceNameCommand = 0xc0104880;
constexpr std::uint32_t PswCommandCommand = 0xc010487d;
constexpr std::uint32_t EqPresetNameCommand = 0xc0184883;
constexpr std::uint32_t EqBandValuesCommand = 0xc0184884;
constexpr std::uint32_t UpdateCheckCommand = 0x8028487e;
constexpr std::uint32_t ChannelCommand = 0x800c485c;
constexpr std::array<std::uint32_t, 5> UpdateReadiness{0, 3, 1, 3, 2};
constexpr std::uint32_t UnknownUpdateReadiness = 3;

struct Single {
    std::uint32_t value;
};

struct Pair {
    std::uint32_t first;
    std::uint32_t second;
};

struct Triple {
    std::uint32_t first;
    std::uint32_t second;
    std::uint32_t third;
};

struct Query {
    std::uint8_t kind;
    std::uint8_t reserved0[3];
    std::uint32_t device;
    std::uint32_t value;
    std::uint32_t reserved1;
    void* output;
};

struct Focus {
    std::uint32_t first;
    std::uint32_t reserved0;
    void* data;
    std::uint8_t application;
    std::uint8_t reserved1[7];
};

struct Transfer {
    std::uint32_t device;
    std::uint32_t mode;
    void* data;
};

struct Setting {
    std::uint8_t kind;
    std::uint8_t reserved0[3];
    std::uint32_t value;
    std::uint32_t device;
    std::uint32_t reserved1;
};

struct Indexed {
    std::uint32_t device;
    std::uint32_t mode;
    std::uint32_t index;
    std::uint32_t reserved;
    void* data;
};

struct DeviceIndex {
    std::uint8_t first;
    std::uint8_t second;
    std::uint8_t reserved[6];
    void* output;
};

struct PointerPair {
    void* first;
    void* second;
    std::uint32_t value;
    std::uint32_t reserved;
};

struct Pointer {
    void* data;
};

struct UpdateCheck {
    std::uint32_t device;
    std::uint32_t reserved;
    const void* image;
    std::uint64_t imageSize;
    std::uint16_t* first;
    std::uint16_t* second;
};

struct UpdateData {
    std::uint32_t device;
    std::uint32_t reserved;
    const void* data;
    std::uint64_t size;
};

struct DataResult {
    std::uint32_t device;
    std::uint32_t reserved;
    const void* data;
    void* result;
};

struct Reply {
    std::uint32_t device;
    std::uint32_t reserved0;
    void* reply;
    std::uint32_t mode;
    std::uint32_t reserved1;
};

struct AuthRequest {
    std::uint32_t reserved0;
    std::uint16_t first;
    std::uint16_t second;
    std::uint32_t kind;
    std::uint32_t reserved1;
    void* reply;
    std::uint32_t mode;
    std::uint32_t reserved2;
};

std::atomic<int> g_descriptor{-1};
std::atomic<bool> g_initialized{false};

bool Initialized() {
    return g_initialized.load(std::memory_order_acquire);
}

template <typename T>
bool StoreGuest(T* destination, const T& value) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, &value, sizeof(T));
    errno = savedErrno;
    return copied;
}

template <typename T>
bool LoadGuest(T& value, const T* source) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&value, source, sizeof(T));
    errno = savedErrno;
    return copied;
}

template <std::uint32_t TCommand, typename TRequest>
int Control(TRequest& request) {
    static_assert(sizeof(TRequest) == ((TCommand >> 16) & 0x1fff));
    return ioctl_nid_postfix(g_descriptor.load(std::memory_order_acquire), TCommand, &request);
}

template <std::uint32_t TCommand>
int SendSingle(std::uint32_t value) {
    if (!Initialized()) return ErrorNotInitialized;
    Single request{value};
    return Control<TCommand>(request);
}

template <std::uint32_t TCommand>
int SendPair(std::uint32_t first, std::uint32_t second) {
    if (!Initialized()) return ErrorNotInitialized;
    Pair request{first, second};
    return Control<TCommand>(request);
}

template <std::uint32_t TCommand>
int SendTriple(std::uint32_t first, std::uint32_t second, std::uint32_t third) {
    if (!Initialized()) return ErrorNotInitialized;
    Triple request{first, second, third};
    return Control<TCommand>(request);
}

template <std::uint32_t TCommand, std::uint8_t TKind, bool TChecked>
int SendQuery(std::uint32_t device, std::uint32_t value, void* output) {
    if (!Initialized()) return ErrorNotInitialized;
    if (TChecked && output == nullptr) return ErrorInvalidArgument;
    Query request{};
    request.kind = TKind;
    request.device = device;
    request.value = value;
    request.output = output;
    return Control<TCommand>(request);
}

template <std::uint32_t TCommand, bool TChecked>
int SendTransfer(std::uint32_t device, std::uint32_t mode, const void* data) {
    if (!Initialized()) return ErrorNotInitialized;
    if (TChecked && data == nullptr) return ErrorInvalidArgument;
    Transfer request{device, mode, const_cast<void*>(data)};
    return Control<TCommand>(request);
}

int SendSetting(std::uint8_t kind, std::uint32_t value, std::uint32_t device) {
    Setting request{};
    request.kind = kind;
    request.value = value;
    request.device = device;
    return Control<SettingCommand>(request);
}

int SendFocus(std::uint32_t first, const void* data, bool application) {
    if (!Initialized()) return ErrorNotInitialized;
    if (data == nullptr) return ErrorInvalidArgument;
    Focus request{};
    request.first = first;
    request.data = const_cast<void*>(data);
    request.application = application ? 1 : 0;
    return Control<FocusCommand>(request);
}

template <std::uint32_t TCommand>
int SendIndexed(std::uint32_t device, std::uint32_t mode, std::uint32_t index, const void* data) {
    Indexed request{};
    request.device = device;
    request.mode = mode;
    request.index = index;
    request.data = const_cast<void*>(data);
    return Control<TCommand>(request);
}

bool IsUserPreset(std::uint32_t preset) {
    return preset >= 3 && preset <= 5;
}

template <std::uint32_t TCommand>
int SendUpdateData(std::uint32_t device, const void* chunk) {
    if (!Initialized()) return ErrorNotInitialized;
    if (chunk == nullptr) return ErrorInvalidArgument;
    std::uint64_t size = 0;
    std::memcpy(&size, chunk, sizeof(size));
    if (size > MaximumUpdateChunk) return ErrorInvalidArgument;
    UpdateData request{};
    request.device = device;
    request.data = static_cast<const std::uint8_t*>(chunk) + sizeof(size);
    request.size = size;
    return Control<TCommand>(request);
}

}

extern "C" {

int APS5_VABI sceHidControlInit() {
    if (Initialized()) return 0;
    const int descriptor = open_nid_postfix("/dev/hid", 0, 0);
    g_descriptor.store(descriptor, std::memory_order_release);
    if (descriptor < 0) return descriptor;
    g_initialized.store(true, std::memory_order_release);
    return 0;
}

int APS5_VABI sceHidControlConnectPort(std::uint32_t first, std::uint32_t second) {
    return SendPair<0x80084805>(first, second);
}

int APS5_VABI sceHidControlDisconnectPort(std::uint32_t first, std::uint32_t second) {
    return SendPair<0x80084806>(first, second);
}

int APS5_VABI sceHidControlConnectHandle(std::uint32_t first, std::uint32_t second, std::uint32_t third) {
    return SendTriple<0x800c480a>(first, second, third);
}

int APS5_VABI sceHidControlDisconnectHandle(std::uint32_t first, std::uint32_t second) {
    return SendTriple<0x800c480b>(first, second, 0xffffffff);
}

int APS5_VABI sceHidControlDisconnectDevice(std::uint32_t device) {
    return SendSingle<0x80044807>(device);
}

int APS5_VABI sceHidControlGetDeviceInfo(std::uint32_t device, void* output) {
    return SendQuery<QueryCommand, 0x00, true>(device, 0, output);
}

int APS5_VABI sceHidControlSetProcessFocus(std::uint32_t first, const void* data) {
    return SendFocus(first, data, false);
}

int APS5_VABI sceHidControlSetApplicationFocus(std::uint32_t first, const void* data) {
    return SendFocus(first, data, true);
}

int APS5_VABI sceHidControlGetBatteryState(std::uint32_t device, void* output) {
    return SendQuery<StateCommand, 0x00, true>(device, 0, output);
}

int APS5_VABI sceHidControlGetDeviceName(std::uint32_t device, void* output) {
    return SendTransfer<0x8010484b, true>(device, 0, output);
}

int APS5_VABI libSceHidControlUnknown_HZJRMD3BeKk_nid_no_patch(std::uint32_t device, void* output) {
    return SendQuery<StateCommand, 0x05, true>(device, 0, output);
}
APS5_EXPORT("HZJRMD3BeKk", libSceHidControlUnknown_HZJRMD3BeKk_nid_no_patch);

int APS5_VABI sceHidControlGetJediState(std::uint32_t device, void* output) {
    return SendQuery<StateCommand, 0x03, true>(device, 0, output);
}

int APS5_VABI sceHidControlSetJediVolumeGain(std::uint32_t device, const void* gain) {
    return SendTransfer<0x8010483d, true>(device, 0, gain);
}

int APS5_VABI sceHidControlSetJediAudioOutPath(std::uint32_t device, std::uint32_t path) {
    return SendPair<0x8008483e>(device, path);
}

int APS5_VABI sceHidControlGetJediExtensionUnitInfo(std::uint32_t device, void* output) {
    return SendTransfer<0x80104842, false>(device, 1, output);
}

int APS5_VABI sceHidControlGetSparkState(std::uint32_t device, void* output) {
    return SendQuery<StateCommand, 0x02, true>(device, 0, output);
}

int APS5_VABI sceHidControlSetHandleState(std::uint32_t first, std::uint32_t second, std::uint32_t third) {
    return SendTriple<0x800c480d>(first, second, third);
}

int APS5_VABI sceHidControlSetUserColor(std::uint32_t device, std::uint32_t color) {
    if (!Initialized()) return ErrorNotInitialized;
    if (color == 0) return ErrorInvalidArgument;
    Pair request{device, color};
    return Control<0x80084824>(request);
}

int APS5_VABI libSceHidControlUnknown_tcDE01gUoN0_nid_no_patch(std::uint32_t first, std::uint32_t second, std::uint8_t value) {
    return SendTriple<ChannelCommand>(first, second, value);
}
APS5_EXPORT("tcDE01gUoN0", libSceHidControlUnknown_tcDE01gUoN0_nid_no_patch);

int APS5_VABI libSceHidControlUnknown_iVF5VrBHJzk_nid_no_patch(std::uint32_t device, std::uint8_t value) {
    return SendTriple<ChannelCommand>(device, 3, value);
}
APS5_EXPORT("iVF5VrBHJzk", libSceHidControlUnknown_iVF5VrBHJzk_nid_no_patch);

int APS5_VABI sceHidControlSetVibrationTriggerEffectWeakForce(std::uint32_t device, std::uint8_t value) {
    if (!Initialized()) return ErrorNotInitialized;
    return SendSetting(0x11, value, device);
}

int APS5_VABI sceHidControlSetAllVibrationMute(std::uint8_t mute) {
    if (!Initialized()) return ErrorNotInitialized;
    return SendSetting(0x12, mute, 0);
}

int APS5_VABI libSceHidControlUnknown_C05jP9w4PCc_nid_no_patch(std::uint32_t first, std::uint32_t second) {
    return SendPair<0x8008485d>(first, second);
}
APS5_EXPORT("C05jP9w4PCc", libSceHidControlUnknown_C05jP9w4PCc_nid_no_patch);

int APS5_VABI libSceHidControlUnknown_yJJq5Arw8EI_nid_no_patch(std::uint32_t device, void* output) {
    return SendQuery<StateCommand, 0x04, false>(device, 0, output);
}
APS5_EXPORT("yJJq5Arw8EI", libSceHidControlUnknown_yJJq5Arw8EI_nid_no_patch);

int APS5_VABI sceHidControlEnableMicBeamForming(std::uint32_t device, std::uint8_t enable) {
    if (!Initialized()) return ErrorNotInitialized;
    return SendSetting(0x1a, enable, device);
}

int APS5_VABI libSceHidControlUnknown_w2MEBm2Tvm8_nid_no_patch(std::uint32_t first, std::uint32_t second) {
    return SendPair<0x8008485e>(first, second);
}
APS5_EXPORT("w2MEBm2Tvm8", libSceHidControlUnknown_w2MEBm2Tvm8_nid_no_patch);

int APS5_VABI sceHidControlGetPswBatteryState(std::uint32_t device, void* output) {
    return SendQuery<StateCommand, 0x08, true>(device, 0, output);
}

int APS5_VABI sceHidControlGetPswState(std::uint32_t device, void* output) {
    return SendQuery<StateCommand, 0x09, true>(device, 0, output);
}

int APS5_VABI sceHidControlSetPswDeviceName(std::uint32_t device, const void* name) {
    return SendTransfer<DeviceNameCommand, true>(device, 1, name);
}

int APS5_VABI sceHidControlGetPswDeviceName(std::uint32_t device, void* output) {
    return SendTransfer<DeviceNameCommand, true>(device, 0, output);
}

int APS5_VABI libSceHidControlUnknown_WikCB91EU0w_nid_no_patch(std::uint32_t device, void* output) {
    return SendTransfer<0xc0104881, true>(device, 0, output);
}
APS5_EXPORT("WikCB91EU0w", libSceHidControlUnknown_WikCB91EU0w_nid_no_patch);

int APS5_VABI sceHidControlGetPswNearbyDeviceInfo(std::uint32_t device, std::uint32_t value, void* output) {
    return SendQuery<QueryCommand, 0x0c, true>(device, value, output);
}

int APS5_VABI sceHidControlGetPswDeviceNameForNearby(std::uint32_t device, std::uint32_t value, void* output) {
    return SendTransfer<0xc0104882, true>(device, value, output);
}

int APS5_VABI sceHidControlSetPswConsoleMute(std::uint32_t device, std::uint8_t mute) {
    if (!Initialized()) return ErrorNotInitialized;
    return SendSetting(0x2d, mute, device);
}

int APS5_VABI sceHidControlSetPswSpeakerVolume(std::uint32_t device, std::uint8_t volume) {
    if (!Initialized()) return ErrorNotInitialized;
    if (volume > 15) return ErrorInvalidArgument;
    return SendSetting(0x2e, volume, device);
}

int APS5_VABI sceHidControlSetPswSidetoneLevel(std::uint32_t device, std::uint8_t level) {
    if (!Initialized()) return ErrorNotInitialized;
    if (level > 15) return ErrorInvalidArgument;
    return SendSetting(0x2f, level, device);
}

int APS5_VABI sceHidControlGetPswSidetoneLevel(std::uint32_t device, void* output) {
    return SendQuery<StateCommand, 0x0b, true>(device, 0, output);
}

int APS5_VABI sceHidControlSetPswEqPresetSelection(std::uint32_t device, std::uint32_t preset) {
    if (!Initialized()) return ErrorNotInitialized;
    if (preset > 5) return ErrorInvalidArgument;
    return SendSetting(0x30, preset, device);
}

int APS5_VABI sceHidControlGetPswEqPresetSelection(std::uint32_t device, void* output) {
    return SendQuery<StateCommand, 0x0d, true>(device, 0, output);
}

int APS5_VABI sceHidControlSetPswEqPresetName(std::uint32_t device, std::uint32_t preset, const void* name) {
    if (!Initialized()) return ErrorNotInitialized;
    if (!IsUserPreset(preset) || name == nullptr) return ErrorInvalidArgument;
    return SendIndexed<EqPresetNameCommand>(device, 1, preset, name);
}

int APS5_VABI sceHidControlGetPswEqPresetName(std::uint32_t device, std::uint32_t preset, void* output) {
    if (!Initialized()) return ErrorNotInitialized;
    if (preset > 5 || output == nullptr) return ErrorInvalidArgument;
    return SendIndexed<EqPresetNameCommand>(device, 0, preset, output);
}

int APS5_VABI sceHidControlSetPswEqBandValues(std::uint32_t device, std::uint32_t preset, const void* values) {
    if (!Initialized()) return ErrorNotInitialized;
    if (!IsUserPreset(preset) || values == nullptr) return ErrorInvalidArgument;
    return SendIndexed<EqBandValuesCommand>(device, 1, preset, values);
}

int APS5_VABI sceHidControlGetPswEqBandValues(std::uint32_t device, std::uint32_t preset, void* output) {
    if (!Initialized()) return ErrorNotInitialized;
    if (preset > 5 || output == nullptr) return ErrorInvalidArgument;
    return SendIndexed<EqBandValuesCommand>(device, 0, preset, output);
}

int APS5_VABI sceHidControlGetPswAllEqBandValues(std::uint32_t device, void* output) {
    return SendTransfer<0xc0104885, true>(device, 0, output);
}

int APS5_VABI sceHidControlSetPswResetSettings(std::uint32_t device) {
    if (!Initialized()) return ErrorNotInitialized;
    return SendSetting(0x2b, 0, device);
}

int APS5_VABI sceHidControlSetPswNetworkLocale(std::uint32_t device, std::uint32_t locale) {
    if (!Initialized()) return ErrorNotInitialized;
    if (locale < 1 || locale > 31) return ErrorInvalidArgument;
    return SendSetting(0x28, locale, device);
}

int APS5_VABI sceHidControlGetPswNetworkLocale(std::uint32_t device, void* output) {
    return SendQuery<QueryCommand, 0x09, true>(device, 0, output);
}

int APS5_VABI sceHidControlConnectPswDeviceForNearby(std::uint32_t device, std::uint32_t value) {
    if (!Initialized()) return ErrorNotInitialized;
    return SendSetting(0x2c, value, device);
}

int APS5_VABI libSceHidControlUnknown_i5ALHbvjRi8_nid_no_patch(std::uint32_t device, void* output) {
    return SendTransfer<0xc0104886, true>(device, 0, output);
}
APS5_EXPORT("i5ALHbvjRi8", libSceHidControlUnknown_i5ALHbvjRi8_nid_no_patch);

int APS5_VABI sceHidControlSetPswCommand(std::uint32_t device, const void* command) {
    return SendTransfer<PswCommandCommand, true>(device, 1, command);
}

int APS5_VABI sceHidControlGetPswCommand(std::uint32_t device, void* output) {
    return SendTransfer<PswCommandCommand, true>(device, 0, output);
}

int APS5_VABI sceHidControlGetPswDeviceIdFromIdxAdd(std::uint8_t first, std::uint8_t second, void* output) {
    if (!Initialized()) return ErrorNotInitialized;
    if (output == nullptr) return ErrorInvalidArgument;
    DeviceIndex request{};
    request.first = first;
    request.second = second;
    request.output = output;
    return Control<0xc0104889>(request);
}

int APS5_VABI libSceHidControlUnknown_OFyJjudLKlA_nid_no_patch(void* first, void* second, std::uint32_t value) {
    if (!Initialized()) return ErrorNotInitialized;
    if (first == nullptr || second == nullptr) return ErrorInvalidArgument;
    PointerPair request{first, second, value, 0};
    return Control<0xc018488a>(request);
}
APS5_EXPORT("OFyJjudLKlA", libSceHidControlUnknown_OFyJjudLKlA_nid_no_patch);

int APS5_VABI libSceHidControlUnknown_8fIm7UR4VDc_nid_no_patch(std::uint32_t device, std::uint32_t value, void* data) {
    return SendTransfer<0xc010488b, true>(device, value, data);
}
APS5_EXPORT("8fIm7UR4VDc", libSceHidControlUnknown_8fIm7UR4VDc_nid_no_patch);

int APS5_VABI sceHidControlGetAuthInformation(std::uint16_t first, std::uint16_t second, std::uint32_t kind, void* output) {
    if (!Initialized()) return ErrorNotInitialized;
    if (kind < 1 || kind > 8 || first == 0 || output == nullptr) return ErrorInvalidArgument;
    constexpr std::size_t replyOffset = 0x610;
    constexpr std::size_t resultSize = 0x24;
    std::memset(output, 0, resultSize);
    std::array<std::uint8_t, 0x6b4> reply{};
    AuthRequest request{};
    request.first = first;
    request.second = second;
    request.kind = kind;
    request.reply = reply.data();
    request.mode = 1;
    const int result = Control<0x8020483f>(request);
    if (result == 0) std::memcpy(output, reply.data() + replyOffset, resultSize);
    return result;
}

int APS5_VABI sceHidControlGetDeviceId(std::uint32_t device, void* output) {
    return SendTransfer<0x8010484c, true>(device, 0, output);
}

int APS5_VABI sceHidControlGetVersionInfo(std::uint32_t device, void* output) {
    return SendQuery<QueryCommand, 0x02, true>(device, 0, output);
}

int APS5_VABI sceHidControlGetPswDeviceInfo(std::uint32_t device, void* output) {
    return SendQuery<QueryCommand, 0x07, true>(device, 0, output);
}

int APS5_VABI libSceHidControlUnknown_AjBZjowWaFI_nid_no_patch(std::uint32_t device, void* output) {
    return SendQuery<QueryCommand, 0x08, true>(device, 0, output);
}
APS5_EXPORT("AjBZjowWaFI", libSceHidControlUnknown_AjBZjowWaFI_nid_no_patch);

int APS5_VABI sceHidControlSetRevokeList(std::uint32_t, std::int32_t, std::int64_t) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHidControlSetRevokeList2(std::uint32_t first, std::uint32_t count, const void* list) {
    if (!Initialized()) return ErrorNotInitialized;
    if (count == 0) return 0;
    if (list == nullptr) return ErrorInvalidArgument;
    Transfer request{first, count, const_cast<void*>(list)};
    return Control<0x8010484a>(request);
}

int APS5_VABI sceHidControlIsForceUpdateNeeded(std::uint32_t device, std::uint32_t value, std::uint8_t* needed) {
    if (!Initialized()) return ErrorNotInitialized;
    if (value == 0 || needed == nullptr) return ErrorInvalidArgument;
    *needed = 0;
    std::uint8_t reply = 0;
    Transfer request{device, value, &reply};
    const int result = Control<0x8010486b>(request);
    if (result == 0 && reply == 1) *needed = 1;
    return result;
}

int APS5_VABI sceHidControlSetRemoteMergeProcess(std::uint32_t first, std::uint32_t second) {
    return SendPair<0x8008487a>(first, second);
}

int APS5_VABI sceHidControlSetCollaboInputSettingOverrideProcess(std::uint32_t process) {
    if (!Initialized()) return ErrorNotInitialized;
    return SendSetting(0x23, process, 0);
}

int APS5_VABI sceHidControlSetForceAdvancedVibrationMode(std::uint8_t enable) {
    if (!Initialized()) return ErrorNotInitialized;
    return SendSetting(0x29, enable, 0);
}

int APS5_VABI sceHidControlPswUpdateIsUpdateNeeded(std::uint32_t device, const void* image, std::uint16_t* result) {
    if (!Initialized()) return ErrorNotInitialized;
    if (result == nullptr) return ErrorInvalidArgument;
    std::uint16_t initialResult = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(result, &initialResult, sizeof(initialResult));
    errno = savedErrno;
    if (!copied) return ErrorInvalidArgument;
    if (image == nullptr) return ErrorInvalidArgument;
    std::uint16_t reply = 0;
    UpdateCheck request{device, 0, image, UpdateImageSize, &reply, nullptr};
    const int status = Control<UpdateCheckCommand>(request);
    if (status == 0) {
        const int savedErrnoReply = errno;
        GuestCopyOut_nid_no_patch(result, &reply, sizeof(reply));
        errno = savedErrnoReply;
    }
    return status;
}

int APS5_VABI sceHidControlPswUpdateIsUpdateNeededForMultiDevice(std::uint32_t device, const std::uint16_t* version,
                                                                 std::uint16_t* first, std::uint16_t* second) {
    if (!Initialized()) return ErrorNotInitialized;
    if (first == nullptr) return ErrorInvalidArgument;
    if (!StoreGuest(first, std::uint16_t{0})) return ErrorInvalidArgument;
    if (version == nullptr) return ErrorInvalidArgument;
    if (second != nullptr && !StoreGuest(second, std::uint16_t{0})) return ErrorInvalidArgument;
    constexpr std::size_t versionOffset = 0xa;
    std::array<std::uint8_t, UpdateImageSize> image{};
    std::uint16_t requested = 0;
    if (!LoadGuest(requested, version)) return ErrorInvalidArgument;
    std::memcpy(image.data() + versionOffset, &requested, sizeof(requested));
    std::uint16_t firstReply = 0;
    std::uint16_t secondReply = 0;
    UpdateCheck request{device, 0, image.data(), UpdateImageSize, &firstReply, &secondReply};
    const int status = Control<UpdateCheckCommand>(request);
    if (status != 0) return status;
    if (!StoreGuest(first, firstReply)) return ErrorInvalidArgument;
    if (second != nullptr && !StoreGuest(second, secondReply)) return ErrorInvalidArgument;
    return status;
}

int APS5_VABI libSceHidControlUnknown_00qes5NswlQ_nid_no_patch(std::uint32_t device, const void* data, std::uint8_t* result) {
    if (!Initialized()) return ErrorNotInitialized;
    if (result == nullptr || data == nullptr) return ErrorInvalidArgument;
    *result = 0;
    constexpr std::size_t copied = 0x10;
    std::array<std::uint8_t, 0x18> buffer{};
    std::memcpy(buffer.data(), data, copied);
    std::uint8_t reply = 0;
    DataResult request{device, 0, buffer.data(), &reply};
    const int status = Control<0x8018487f>(request);
    if (status == 0) *result = reply != 0 ? 1 : 0;
    return status;
}
APS5_EXPORT("00qes5NswlQ", libSceHidControlUnknown_00qes5NswlQ_nid_no_patch);

int APS5_VABI sceHidControlPswUpdateInit() {
    if (!Initialized()) return ErrorNotInitialized;
    return SendSetting(0x13, 1, 0);
}

int APS5_VABI sceHidControlPswUpdateIsReady(std::uint32_t device, std::uint32_t* state) {
    if (!Initialized()) return ErrorNotInitialized;
    if (state == nullptr) return ErrorInvalidArgument;
    std::uint32_t reply = 0;
    Transfer request{device, 0, &reply};
    const int result = Control<0x80104865>(request);
    const std::uint32_t value = reply < UpdateReadiness.size() ? UpdateReadiness[reply] : UnknownUpdateReadiness;
    if (!StoreGuest(state, value)) return ErrorInvalidArgument;
    return result;
}

int APS5_VABI sceHidControlPswUpdateStart(std::uint32_t device, const void* image) {
    if (!Initialized()) return ErrorNotInitialized;
    if (image == nullptr) return ErrorInvalidArgument;
    UpdateData request{device, 0, image, UpdateImageSize};
    return Control<0x80184866>(request);
}

int APS5_VABI sceHidControlPswUpdateGetStatus(std::uint32_t device, std::uint32_t* status) {
    if (!Initialized()) return ErrorNotInitialized;
    if (status == nullptr) return ErrorInvalidArgument;
    std::uint32_t reply = 0;
    Transfer request{device, 0, &reply};
    const int result = Control<0x80104867>(request);
    if (!StoreGuest(status, reply)) return ErrorInvalidArgument;
    return result;
}

int APS5_VABI sceHidControlPswUpdateSendData(std::uint32_t device, const void* chunk) {
    return SendUpdateData<0x80184868>(device, chunk);
}

int APS5_VABI sceHidControlPswUpdateSendAppendixData(std::uint32_t device, const void* chunk) {
    return SendUpdateData<0x8018486c>(device, chunk);
}

int APS5_VABI libSceHidControlUnknown_TKax3WNHtEg_nid_no_patch(std::uint32_t device) {
    if (!Initialized()) return ErrorNotInitialized;
    return SendSetting(0x14, 0, device);
}
APS5_EXPORT("TKax3WNHtEg", libSceHidControlUnknown_TKax3WNHtEg_nid_no_patch);

int APS5_VABI sceHidControlPswUpdateGetReconnectStatus(std::uint32_t* status) {
    if (!Initialized()) return ErrorNotInitialized;
    if (status == nullptr) return ErrorInvalidArgument;
    std::uint32_t reply = 0;
    Reply request{};
    request.reply = &reply;
    request.mode = 1;
    const int result = Control<0x80184869>(request);
    const std::uint32_t value = reply == 0 ? 0 : reply == 1 ? 1 : 2;
    if (!StoreGuest(status, value)) return ErrorInvalidArgument;
    return result;
}

int APS5_VABI sceHidControlPswUpdateTerm() {
    if (!Initialized()) return ErrorNotInitialized;
    return SendSetting(0x15, 1, 0);
}

int APS5_VABI sceHidControlPswUpdateGetDeviceFirmwareUpdateTelemetryData(std::uint32_t device, void* output) {
    if (!Initialized()) return ErrorNotInitialized;
    if (output == nullptr) return ErrorInvalidArgument;
    constexpr std::size_t outputSize = 0x40;
    std::array<std::uint8_t, outputSize> reply{};
    Reply request{};
    request.device = device;
    request.reply = reply.data();
    request.mode = 1;
    const int result = Control<0x8018486d>(request);
    if (result < 0) return result;
    auto* bytes = static_cast<std::uint8_t*>(output);
    std::memset(bytes + sizeof(std::uint32_t), 0, outputSize - sizeof(std::uint32_t));
    std::memcpy(bytes, reply.data(), sizeof(std::uint32_t));
    return result;
}

int APS5_VABI libSceHidControlUnknown_DUGI7AXFtV0_nid_no_patch(void* data) {
    if (!Initialized()) return ErrorNotInitialized;
    if (data == nullptr) return ErrorInvalidArgument;
    Pointer request{data};
    return Control<0xc0084888>(request);
}
APS5_EXPORT("DUGI7AXFtV0", libSceHidControlUnknown_DUGI7AXFtV0_nid_no_patch);

int APS5_VABI sceHidControlGetControllerSerialNumberInformation(std::uint32_t device, void* output) {
    if (!Initialized()) return ErrorNotInitialized;
    if (output == nullptr) return ErrorInvalidArgument;
    constexpr std::size_t outputSize = 0x20;
    constexpr std::size_t copied = 0x11;
    std::array<std::uint8_t, outputSize> reply{};
    Query request{};
    request.kind = 0x0b;
    request.device = device;
    request.output = reply.data();
    const int result = Control<QueryCommand>(request);
    if (result < 0) return result;
    auto* bytes = static_cast<std::uint8_t*>(output);
    std::memset(bytes + copied, 0, outputSize - copied);
    std::memcpy(bytes, reply.data(), copied);
    return result;
}

int APS5_VABI sceHidControlSetPswControllerCommunicationMethod(std::uint32_t method) {
    if (!Initialized()) return ErrorNotInitialized;
    return method < 2 ? 0 : ErrorInvalidArgument;
}

}
