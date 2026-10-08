#include "SceTypes.hpp"
#include <array>
#include <cstdlib>
#include <cstring>

extern "C" {
int APS5_VABI sceNpEntitlementAccessInitialize(const NpEntitlementAccessInitParam*, NpEntitlementAccessBootParam*);
int APS5_VABI sceNpEntitlementAccessRequestServiceEntitlementInfoList(int32_t, uint32_t, const NpServiceEntitlementLabel*, uint32_t, const NpEntitlementAccessRequestEntitlementInfoListParam*, int64_t*);
int APS5_VABI sceNpEntitlementAccessPollServiceEntitlementInfoList(int64_t, int32_t*, NpEntitlementAccessServiceEntitlementInfo*, uint32_t, uint32_t*, int32_t*, int32_t*);
int APS5_VABI sceNpEntitlementAccessAbortRequest(int64_t);
int APS5_VABI sceNpEntitlementAccessDeleteRequest(int64_t);
}

static constexpr int ErrorNotInitialized = static_cast<int>(0x817D0001);
static constexpr int ErrorParameter = static_cast<int>(0x817D0002);
static constexpr int ErrorSignedOut = static_cast<int>(0x817D0014);
static constexpr int ErrorRequestNotFound = static_cast<int>(0x817D0015);
static void Require(bool value) { if (!value) std::abort(); }

int main() {
    NpEntitlementAccessRequestEntitlementInfoListParam param{sizeof(param), 1, 0, 10, 0, 0, 0};
    int64_t first = -99;
    auto request = [&](const auto* value, int64_t* id) {
        return sceNpEntitlementAccessRequestServiceEntitlementInfoList(0, 0, nullptr, 0, value, id);
    };
    NpEntitlementAccessServiceEntitlementInfo info;
    std::memset(&info, 0x5a, sizeof(info));
    std::array<unsigned char, sizeof(info)> unchanged{};
    std::memcpy(unchanged.data(), &info, sizeof(info));
    int32_t result = 123, next = 234, previous = 345;
    uint32_t hit = 456;
    auto poll = [&](int64_t id) {
        return sceNpEntitlementAccessPollServiceEntitlementInfoList(id, &result, &info, 1, &hit, &next, &previous);
    };
    Require(request(&param, &first) == ErrorNotInitialized && first == -99);
    Require(poll(1) == ErrorNotInitialized && result == 123);
    Require(sceNpEntitlementAccessDeleteRequest(1) == ErrorNotInitialized);
    Require(sceNpEntitlementAccessAbortRequest(1) == ErrorNotInitialized);
    Require(sceNpEntitlementAccessInitialize(nullptr, nullptr) == 0);
    Require(request(&param, nullptr) == ErrorParameter);
    Require(request(static_cast<const NpEntitlementAccessRequestEntitlementInfoListParam*>(nullptr), &first) == ErrorParameter);
    Require(sceNpEntitlementAccessRequestServiceEntitlementInfoList(0, 0, nullptr, 1, &param, &first) == ErrorParameter);
    for (int field = 0; field < 7; ++field) {
        auto invalid = param;
        switch (field) {
        case 0: invalid.size = 0; break;
        case 1: invalid.entitlementType = 2; break;
        case 2: invalid.offset = -1; break;
        case 3: invalid.limit = 101; break;
        case 4: invalid.sort = 2; break;
        case 5: invalid.direction = 3; break;
        case 6: invalid.packageType = 4; break;
        }
        Require(request(&invalid, &first) == ErrorParameter && first == -99);
    }
    Require(poll(999) == ErrorRequestNotFound && result == 123);
    Require(request(&param, &first) == 0 && first > 0);
    int64_t second = 0;
    Require(request(&param, &second) == 0 && second != first);
    Require(sceNpEntitlementAccessPollServiceEntitlementInfoList(first, nullptr, &info, 1, &hit, &next, &previous) == ErrorParameter);
    Require(sceNpEntitlementAccessPollServiceEntitlementInfoList(first, &result, nullptr, 1, &hit, &next, &previous) == ErrorParameter);
    Require(poll(first) == 0 && result == ErrorSignedOut);
    Require(hit == 456 && next == 234 && previous == 345);
    Require(std::memcmp(&info, unchanged.data(), sizeof(info)) == 0);
    Require(poll(first) == 0 && result == ErrorSignedOut);
    Require(sceNpEntitlementAccessAbortRequest(first) == 0);
    Require(poll(first) == 0 && result == ErrorSignedOut);
    Require(sceNpEntitlementAccessDeleteRequest(first) == 0);
    result = 123;
    Require(poll(first) == ErrorRequestNotFound && result == 123);
    Require(sceNpEntitlementAccessAbortRequest(first) == ErrorRequestNotFound);
    Require(sceNpEntitlementAccessDeleteRequest(first) == ErrorRequestNotFound);
    Require(poll(second) == 0 && result == ErrorSignedOut);
    Require(sceNpEntitlementAccessDeleteRequest(second) == 0);
    std::array<int64_t, 16> ids{};
    for (auto& id : ids) Require(request(&param, &id) == 0 && id > second);
    for (auto id : ids) Require(sceNpEntitlementAccessDeleteRequest(id) == 0);
}
