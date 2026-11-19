#include "prx/libSceNpUniversalDataSystem/include/Api.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

#include "NpUniversalDataSystem.hpp"
#include "SceTypes.hpp"
#include "prx/libSceNpTrophy2/include/TrophyStore.hpp"
#include "prx/libc/include/General.hpp"

namespace {

Uds::Owned<NpUniversalDataSystemEvent>& events() {
    static Uds::Owned<NpUniversalDataSystemEvent> registry;
    return registry;
}

void ignore(const NpUniversalDataSystemEvent& event, int context, const std::string& reason) {
    std::fprintf(stderr, "[trophy] ignored %s posted to context %d: %s\n", Uds::Describe(event).c_str(), context, reason.c_str());
}

void post(const NpUniversalDataSystemEvent& event, int context) {
    static const bool trace = std::getenv("APS5_TRACE_UDS") != nullptr;
    if (trace) std::fprintf(stderr, "[uds] context %d posted %s\n", context, Uds::Describe(event).c_str());
    const auto trophy = Uds::RecognizeTrophyEvent(event);
    switch (trophy.kind) {
    case Uds::TrophyEventKind::None:
    case Uds::TrophyEventKind::Progress:
        if (const auto owner = Uds::FindContext(context)) NpTrophy2ApplyEvent_nid_no_patch(owner->userId, owner->serviceLabel, Uds::StatEvent(event));
        return;
    case Uds::TrophyEventKind::Malformed:
        ignore(event, context, trophy.problem);
        return;
    case Uds::TrophyEventKind::Unlock:
        if (const auto owner = Uds::FindContext(context)) NpTrophy2RecordUnlock_nid_no_patch(owner->userId, owner->serviceLabel, trophy.trophyId);
        else ignore(event, context, "the context does not exist");
        return;
    }
}

}

extern "C" {

int APS5_VABI sceNpUniversalDataSystemCreateEvent(
    const char* event_name,
    const NpUniversalDataSystemEventPropertyObject* prop,
    NpUniversalDataSystemEvent** new_event,
    NpUniversalDataSystemEventPropertyObject** prop_ptr)
{
    std::string name;
    if (event_name == nullptr || CopyGuestString_nid_no_patch(event_name, 0x1000, name) != 0 || name.empty()) {
        return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_INVALID_EVENT_NAME;
    }
    if (new_event == nullptr) return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_INVALID_ARGUMENT;
    auto event = std::make_unique<NpUniversalDataSystemEvent>();
    event->name = std::move(name);
    if (prop != nullptr) event->properties = prop->Clone();
    auto* created = events().Adopt(std::move(event));
    int savedErrno = errno;
    bool copied = GuestCopyOut_nid_no_patch(new_event, &created, sizeof(created));
    errno = savedErrno;
    if (!copied) {
        events().Release(created);
        return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_INVALID_ARGUMENT;
    }
    if (prop_ptr != nullptr) {
        auto* properties = &created->properties;
        savedErrno = errno;
        copied = GuestCopyOut_nid_no_patch(prop_ptr, &properties, sizeof(properties));
        errno = savedErrno;
        if (!copied) {
            events().Release(created);
            return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_INVALID_ARGUMENT;
        }
    }
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemDestroyEvent(NpUniversalDataSystemEvent* event) {
    events().Release(event);
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemPostEvent(int context, int handle, const void* event, uint64_t options) {
    if (event == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    const auto* posted = static_cast<const NpUniversalDataSystemEvent*>(event);
    if (!events().Contains(posted)) {
        throw std::invalid_argument(std::string(__func__) + ": the event was not created by sceNpUniversalDataSystemCreateEvent or was destroyed");
    }
    post(*posted, context);
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventEstimateSize(const NpUniversalDataSystemEvent* event, size_t* size) {
    if (event == nullptr || !events().Contains(event)) return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_NULL_EVENT;
    if (size == nullptr) return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_INVALID_ARGUMENT;
    const size_t value = Uds::MessagePackSize(*event);
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(size, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_INVALID_ARGUMENT;
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventToString(
    const NpUniversalDataSystemEvent* event,
    char* buf,
    size_t buf_size,
    size_t* string_size)
{
    if (event == nullptr || !events().Contains(event)) return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_NULL_EVENT;
    if (buf == nullptr) {
        if (string_size == nullptr) return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_INVALID_ARGUMENT;
        buf_size = 0;
    }
    const std::string text = Uds::DebugString(*event);
    if (buf_size != 0) {
        const size_t copied = std::min(text.size(), buf_size - 1);
        std::string truncated(text, 0, copied);
        truncated.push_back('\0');
        const int savedErrno = errno;
        const bool written = GuestCopyOut_nid_no_patch(buf, truncated.data(), truncated.size());
        errno = savedErrno;
        if (!written) return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_INVALID_ARGUMENT;
    }
    if (string_size != nullptr) {
        const size_t value = text.size();
        const int savedErrno = errno;
        const bool written = GuestCopyOut_nid_no_patch(string_size, &value, sizeof(value));
        errno = savedErrno;
        if (!written) return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_INVALID_ARGUMENT;
    }
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

}
