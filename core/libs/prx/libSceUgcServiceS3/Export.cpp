#include "prx/libSceUgcServiceS3/include/Api.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <unordered_map>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int kErrOutOfMemory = static_cast<int>(0x81b70001u);
constexpr int kErrNoClient = static_cast<int>(0x81b70002u);
constexpr int kErrAlreadyInitialized = static_cast<int>(0x81b70003u);
constexpr int kErrInvalidArg = static_cast<int>(0x81b70004u);
constexpr int kErrUnsupportedFileType = static_cast<int>(0x81b71001u);
constexpr int kErrNotFoundRequest = static_cast<int>(0x81b71005u);
constexpr int kErrUploadedInfoEmpty = static_cast<int>(0x81b71006u);

constexpr std::uint32_t kMaxPartIndex = 0x2710u;
constexpr std::size_t kMaxProbeStringBytes = 4096u;

struct RequestState {
    float progress = 0.0f;
    std::uint64_t progressSize = 0;
    bool cancelled = false;
    void* callback = nullptr;
    void* userData = nullptr;
    std::string uploadedInfo;
};

struct ClientState {
    std::mutex mutex;
    int nextRequestId = 0;
    std::unordered_map<int, RequestState> requests;
};

std::mutex g_clientLifecycleMutex;
ClientState* g_client = nullptr;

bool ProbeGuestCString(const char* str, std::string* out) {
    if (str == nullptr) return false;
    if (out != nullptr) out->clear();
    for (std::size_t i = 0; i < kMaxProbeStringBytes; ++i) {
        char ch = '\0';
        if (!GuestCopyIn_nid_no_patch(&ch, str + i, 1)) return false;
        if (ch == '\0') return true;
        if (out != nullptr) out->push_back(ch);
    }
    return false;
}

bool ValidatePartInfo(const void* partInfo) {
    if (partInfo == nullptr) return false;
    std::uint32_t partIndex = 0;
    const auto* bytePtr = static_cast<const std::uint8_t*>(partInfo);
    if (!GuestCopyIn_nid_no_patch(&partIndex, bytePtr + 8, sizeof(partIndex))) return false;
    return partIndex <= kMaxPartIndex;
}

ClientState* GetClient() {
    std::lock_guard<std::mutex> lock(g_clientLifecycleMutex);
    return g_client;
}

bool IsValidFileType(int fileType, bool allowVideo) {
    if (fileType >= 1 && fileType <= 4) return true;
    return allowVideo && fileType == 5;
}

}

extern "C" {

APS5_EXPORT("stkA1oba6eY", sceUgcServiceS3Unknown00);
int APS5_VABI sceUgcServiceS3Unknown00(void) {
    std::lock_guard<std::mutex> lock(g_clientLifecycleMutex);
    if (g_client != nullptr) return kErrAlreadyInitialized;
    auto* client = new (std::nothrow) ClientState();
    if (client == nullptr) return kErrOutOfMemory;
    g_client = client;
    return 0;
}

APS5_EXPORT("VlNz4Z+qWu8", sceUgcServiceS3Unknown01);
int APS5_VABI sceUgcServiceS3Unknown01(void) {
    ClientState* client = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_clientLifecycleMutex);
        if (g_client == nullptr) return 0;
        client = g_client;
        g_client = nullptr;
    }
    delete client;
    return 0;
}

int APS5_VABI sceUgcServiceS3CreateRequest(const char* accessKey) {
    ClientState* client = GetClient();
    if (client == nullptr) return kErrNoClient;
    if (accessKey != nullptr) {
        std::string probedKey;
        if (!ProbeGuestCString(accessKey, &probedKey)) return kErrInvalidArg;
    }
    std::lock_guard<std::mutex> lock(client->mutex);
    if (client->nextRequestId >= (std::numeric_limits<int>::max)()) {
        client->nextRequestId = 1;
    } else {
        ++client->nextRequestId;
    }
    int reqId = client->nextRequestId;
    while (reqId <= 0 || client->requests.find(reqId) != client->requests.end()) {
        if (client->nextRequestId >= (std::numeric_limits<int>::max)()) {
            client->nextRequestId = 1;
        } else {
            ++client->nextRequestId;
        }
        reqId = client->nextRequestId;
    }
    client->requests.emplace(reqId, RequestState{});
    return reqId;
}

APS5_EXPORT("6MeapakWJb8", sceUgcServiceS3Unknown02);
int APS5_VABI sceUgcServiceS3Unknown02(int requestId) {
    ClientState* client = GetClient();
    if (client == nullptr) return kErrNoClient;
    std::lock_guard<std::mutex> lock(client->mutex);
    const auto it = client->requests.find(requestId);
    if (it == client->requests.end()) return kErrNotFoundRequest;
    client->requests.erase(it);
    return 0;
}

int APS5_VABI sceUgcServiceS3CancelRequest(int requestId) {
    ClientState* client = GetClient();
    if (client == nullptr) return kErrNoClient;
    std::lock_guard<std::mutex> lock(client->mutex);
    const auto it = client->requests.find(requestId);
    if (it == client->requests.end()) return kErrNotFoundRequest;
    it->second.cancelled = true;
    return 0;
}

APS5_EXPORT("bxfGN4Ghqmg", sceUgcServiceS3Unknown03);
int APS5_VABI sceUgcServiceS3Unknown03(int requestId, void* callback) {
    ClientState* client = GetClient();
    if (client == nullptr) return kErrNoClient;
    std::lock_guard<std::mutex> lock(client->mutex);
    const auto it = client->requests.find(requestId);
    if (it == client->requests.end()) return kErrNotFoundRequest;
    it->second.callback = callback;
    return 0;
}

APS5_EXPORT("40fNo08E7T4", sceUgcServiceS3Unknown04);
int APS5_VABI sceUgcServiceS3Unknown04(int requestId, void* userData) {
    ClientState* client = GetClient();
    if (client == nullptr) return kErrNoClient;
    std::lock_guard<std::mutex> lock(client->mutex);
    const auto it = client->requests.find(requestId);
    if (it == client->requests.end()) return kErrNotFoundRequest;
    it->second.userData = userData;
    return 0;
}

APS5_EXPORT("diqgMYgC9oE", sceUgcServiceS3Unknown05);
int APS5_VABI sceUgcServiceS3Unknown05(int requestId, float* progress) {
    ClientState* client = GetClient();
    if (client == nullptr) return kErrNoClient;
    if (progress == nullptr) return kErrInvalidArg;
    float probeValue = 0.0f;
    if (!GuestCopyIn_nid_no_patch(&probeValue, progress, sizeof(probeValue))) return kErrInvalidArg;
    float value = 0.0f;
    {
        std::lock_guard<std::mutex> lock(client->mutex);
        const auto it = client->requests.find(requestId);
        if (it == client->requests.end()) return kErrNotFoundRequest;
        value = it->second.progress;
    }
    if (!GuestCopyOut_nid_no_patch(progress, &value, sizeof(value))) return kErrInvalidArg;
    return 0;
}

APS5_EXPORT("9NCT1TQWTQE", sceUgcServiceS3Unknown06);
int APS5_VABI sceUgcServiceS3Unknown06(int requestId, std::uint64_t* progressSize) {
    ClientState* client = GetClient();
    if (client == nullptr) return kErrNoClient;
    if (progressSize == nullptr) return kErrInvalidArg;
    std::uint64_t probeValue = 0;
    if (!GuestCopyIn_nid_no_patch(&probeValue, progressSize, sizeof(probeValue))) return kErrInvalidArg;
    std::uint64_t value = 0;
    {
        std::lock_guard<std::mutex> lock(client->mutex);
        const auto it = client->requests.find(requestId);
        if (it == client->requests.end()) return kErrNotFoundRequest;
        value = it->second.progressSize;
    }
    if (!GuestCopyOut_nid_no_patch(progressSize, &value, sizeof(value))) return kErrInvalidArg;
    return 0;
}

int APS5_VABI sceUgcServiceS3UploadFileWithGmsResponse(int requestId, int fileType, const char* filePath, const char* gmsResponse) {
    ClientState* client = GetClient();
    if (client == nullptr) return kErrNoClient;
    if (!ProbeGuestCString(filePath, nullptr)) return kErrInvalidArg;
    {
        std::lock_guard<std::mutex> lock(client->mutex);
        const auto it = client->requests.find(requestId);
        if (it == client->requests.end()) return kErrNotFoundRequest;
        it->second.progress = 0.0f;
        it->second.progressSize = 0;
    }
    if (!IsValidFileType(fileType, false)) return kErrUnsupportedFileType;
    if (!ProbeGuestCString(gmsResponse, nullptr)) return kErrInvalidArg;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("QOhoeuKdo7E", sceUgcServiceS3Unknown07);
int APS5_VABI sceUgcServiceS3Unknown07(int requestId, int fileType, const char* filePath, const char* gmsResponse, const void* partInfo, void* outInfo) {
    (void)outInfo;
    ClientState* client = GetClient();
    if (client == nullptr) return kErrNoClient;
    if (!ProbeGuestCString(filePath, nullptr)) return kErrInvalidArg;
    if (!ValidatePartInfo(partInfo)) return kErrInvalidArg;
    {
        std::lock_guard<std::mutex> lock(client->mutex);
        const auto it = client->requests.find(requestId);
        if (it == client->requests.end()) return kErrNotFoundRequest;
        it->second.progress = 0.0f;
        it->second.progressSize = 0;
    }
    if (!IsValidFileType(fileType, false)) return kErrUnsupportedFileType;
    if (!ProbeGuestCString(gmsResponse, nullptr)) return kErrInvalidArg;
    NotImplemented_nid_no_patch("QOhoeuKdo7E");
    return 0;
}

APS5_EXPORT("9967FcQ+zos", sceUgcServiceS3Unknown08);
int APS5_VABI sceUgcServiceS3Unknown08(int requestId, int fileType, const char* filePath, const char* gmsResponse, const void* partInfo, void* outInfo) {
    (void)outInfo;
    ClientState* client = GetClient();
    if (client == nullptr) return kErrNoClient;
    if (!ProbeGuestCString(filePath, nullptr)) return kErrInvalidArg;
    if (!ValidatePartInfo(partInfo)) return kErrInvalidArg;
    {
        std::lock_guard<std::mutex> lock(client->mutex);
        const auto it = client->requests.find(requestId);
        if (it == client->requests.end()) return kErrNotFoundRequest;
        it->second.progress = 0.0f;
        it->second.progressSize = 0;
    }
    if (!IsValidFileType(fileType, false)) return kErrUnsupportedFileType;
    if (!ProbeGuestCString(gmsResponse, nullptr)) return kErrInvalidArg;
    NotImplemented_nid_no_patch("9967FcQ+zos");
    return 0;
}

int APS5_VABI sceUgcServiceS3DownloadFileWithGmsResponse(int requestId, int fileType, const char* filePath, std::uint64_t* outSize, const char* gmsResponse) {
    (void)outSize;
    ClientState* client = GetClient();
    if (client == nullptr) return kErrNoClient;
    if (!ProbeGuestCString(filePath, nullptr)) return kErrInvalidArg;
    {
        std::lock_guard<std::mutex> lock(client->mutex);
        const auto it = client->requests.find(requestId);
        if (it == client->requests.end()) return kErrNotFoundRequest;
        it->second.progress = 0.0f;
        it->second.progressSize = 0;
    }
    if (!IsValidFileType(fileType, true)) return kErrUnsupportedFileType;
    if (!ProbeGuestCString(gmsResponse, nullptr)) return kErrInvalidArg;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("0lMDwFrLjkM", sceUgcServiceS3Unknown09);
int APS5_VABI sceUgcServiceS3Unknown09(int requestId, char* info, std::size_t infoSize) {
    ClientState* client = GetClient();
    if (client == nullptr) return kErrNoClient;
    if (info == nullptr || infoSize == 0) return kErrInvalidArg;
    std::string uploaded;
    {
        std::lock_guard<std::mutex> lock(client->mutex);
        const auto it = client->requests.find(requestId);
        if (it == client->requests.end()) return kErrNotFoundRequest;
        if (it->second.uploadedInfo.empty()) return kErrUploadedInfoEmpty;
        uploaded = it->second.uploadedInfo;
    }
    const std::size_t copyLen = (std::min)(uploaded.size() + 1, infoSize);
    if (!GuestCopyOut_nid_no_patch(info, uploaded.c_str(), copyLen)) return kErrInvalidArg;
    return 0;
}

}
