#include "prx/libSceNpUniversalDataSystem/include/Api.hpp"
#include <algorithm>
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
    if (event_name == nullptr || new_event == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    auto event = std::make_unique<NpUniversalDataSystemEvent>();
    event->name = event_name;
    if (prop != nullptr) event->properties = prop->Clone();
    *new_event = events().Adopt(std::move(event));
    if (prop_ptr != nullptr) {
        *prop_ptr = &(*new_event)->properties;
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
    if (event == nullptr) return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_NULL_EVENT;
    if (size == nullptr) return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_INVALID_ARGUMENT;
    *size = Uds::MessagePackSize(*event);
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventToString(
    const NpUniversalDataSystemEvent* event,
    char* buf,
    size_t buf_size,
    size_t* string_size)
{
    if (event == nullptr) return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_NULL_EVENT;
    if (buf == nullptr) {
        if (string_size == nullptr) return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_INVALID_ARGUMENT;
        buf_size = 0;
    }
    const std::string text = Uds::DebugString(*event);
    if (buf_size != 0) {
        const size_t copied = std::min(text.size(), buf_size - 1);
        std::memcpy(buf, text.data(), copied);
        buf[copied] = '\0';
    }
    if (string_size != nullptr) *string_size = text.size();
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

}
