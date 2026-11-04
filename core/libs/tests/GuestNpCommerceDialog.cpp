#include "prx/libSceNpCommerce/include/Api.hpp"
#include "ProcessParametersFixture.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include "prx/libSceIme/include/ModalDialog.hpp"
#include "prx/libSceIme/include/PsnDialogNotice.hpp"
#include <array>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool value) { if (!value) std::abort(); }

alignas(16) std::array<std::byte, 64> process{};

constexpr int statusNone = 0;
constexpr int statusInitialized = 1;
constexpr int statusFinished = 3;
constexpr int errorAlreadyInitialized = static_cast<int>(0x80B80004u);
constexpr std::int32_t resultUserCanceled = 1;

void RequireCanceledResult(std::uint64_t userData, bool clearsReserved) {
    NpCommerceDialogResult result;
    std::memset(&result, 0x33, sizeof(result));
    Require(sceNpCommerceDialogGetResult(&result) == resultUserCanceled);
    Require(result.result == resultUserCanceled);
    Require(result.authorized == 0);
    Require(result.userData == userData);
    for (const auto value : result.padding) Require(value == 0x33);
    for (const auto value : result.reserved) Require(value == (clearsReserved ? 0 : 0x33));
}


bool PsnNoticeShown() {
    ModalDialogView view;
    if (!ModalDialogReadView_nid_postfix(view)) return false;
    return view.message == PsnDialogNoticeRequest(PsnDialogNoticeKind::PsnFeature).message;
}

void RequireNotices() {
    std::uint32_t sdkVersion = 0x03500000;
    std::memcpy(process.data() + 16, &sdkVersion, sizeof(sdkVersion));
    NpCommerceDialogParam param{};
    param.base.size = sizeof(param.base);
    param.base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&param)) - 1060003575u;
    param.size = sizeof(param);
    param.userId = USER_SERVICE_INITIAL_USER_ID;
    param.mode = 5;
    param.argument = 1;
    NpCommerceDialogParam2 param2{};
    param2.base.size = sizeof(param2.base);
    param2.base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&param2)) - 1060003575u;
    param2.size = sizeof(param2);
    param2.userId = USER_SERVICE_INITIAL_USER_ID;
    param2.mode = 5;
    param2.argument = 1;

    ModalDialogSetPresenter_nid_postfix(true);
    sceNpCommerceDialogTerminate();
    Require(sceNpCommerceDialogInitialize() == 0);

    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Silent);
    ModalDialogReset_nid_postfix(ModalDialogAnyOwner);
    Require(sceNpCommerceDialogOpen(&param) == 0);
    Require(!PsnNoticeShown() && !ModalDialogCapturesInput_nid_postfix());

    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Show);
    ModalDialogReset_nid_postfix(ModalDialogAnyOwner);
    Require(sceNpCommerceDialogOpen(nullptr) == static_cast<int>(0x80B8000Du));
    Require(!PsnNoticeShown());
    Require(sceNpCommerceDialogOpen(&param) == 0);
    Require(PsnNoticeShown());

    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Show);
    ModalDialogReset_nid_postfix(ModalDialogAnyOwner);
    Require(sceNpCommerceDialogOpen2(&param2) == 0);
    Require(PsnNoticeShown());

    ModalDialogReset_nid_postfix(ModalDialogAnyOwner);
    ModalDialogSetPresenter_nid_postfix(false);
    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Silent);
    Require(sceNpCommerceDialogTerminate() == 0);
}

}

int main() {
    Require(ProcessParametersFixture::Install(process.data(), process.size()));
    std::uint32_t sdkVersion = 0x03500000;
    std::memcpy(process.data() + 16, &sdkVersion, sizeof(sdkVersion));
    Require(sceUserServiceInitialize(nullptr) == 0);
    NpCommerceDialogParam2 param2{};
    param2.base.size = sizeof(param2.base);
    param2.base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&param2)) - 1060003575u;
    param2.size = sizeof(param2);
    param2.userId = USER_SERVICE_INITIAL_USER_ID;
    param2.userData = 0x123456789abcdef0;
    Require(sceNpCommerceDialogGetStatus() == statusNone);
    Require(sceNpCommerceDialogOpen(nullptr) == static_cast<int>(0x80B8000Du));
    Require(sceNpCommerceDialogOpen2(nullptr) == static_cast<int>(0x80B8000Du));
    Require(sceNpCommerceDialogOpen2(&param2) == static_cast<int>(0x80B80003u));
    param2.base.magic = 0;
    Require(sceNpCommerceDialogOpen2(&param2) == static_cast<int>(0x80B8000Au));
    param2.base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&param2)) - 1060003575u;
    param2.base.reserved[35] = 1;
    Require(sceNpCommerceDialogOpen2(&param2) == static_cast<int>(0x80B8000Au));
    param2.base.reserved[35] = 0;
    Require(sceNpCommerceDialogTerminate() == static_cast<int>(0x80B80003u));
    Require(sceNpCommerceDialogClose() == static_cast<int>(0x80B80003u));
    Require(sceNpCommerceDialogGetResult(nullptr) == static_cast<int>(0x80B8000Du));
    NpCommerceDialogResult result{};
    Require(sceNpCommerceDialogGetResult(&result) == static_cast<int>(0x80B80003u));

    Require(sceNpCommerceDialogInitializeInternal() == 0);
    Require(sceNpCommerceDialogGetStatus() == statusInitialized);
    Require(sceNpCommerceDialogInitializeInternal() == errorAlreadyInitialized);
    Require(sceNpCommerceDialogInitialize() == errorAlreadyInitialized);
    Require(sceNpCommerceDialogGetStatus() == statusInitialized);
    Require(sceNpCommerceDialogTerminate() == 0);
    Require(sceNpCommerceDialogGetStatus() == statusNone);
    Require(sceNpCommerceDialogInitialize() == 0);
    Require(sceNpCommerceDialogInitializeInternal() == errorAlreadyInitialized);
    Require(sceNpCommerceDialogGetStatus() == statusInitialized);
    Require(sceNpCommerceDialogTerminate() == 0);
    Require(sceNpCommerceDialogGetStatus() == statusNone);
    Require(sceNpCommerceDialogInitialize() == 0);
    Require(sceNpCommerceDialogClose() == static_cast<int>(0x80B8000Bu));
    Require(sceNpCommerceDialogGetResult(&result) == static_cast<int>(0x80B80005u));
    Require(sceNpCommerceDialogGetResult(nullptr) == static_cast<int>(0x80B8000Du));
    Require(sceNpCommerceDialogGetStatus() == statusInitialized);

    Require(sceNpCommerceDialogOpen2(&param2) == 0);
    Require(sceNpCommerceDialogUpdateStatus() == statusFinished);
    Require(sceNpCommerceDialogGetStatus() == statusFinished);
    RequireCanceledResult(param2.userData, true);

    Require(sceNpCommerceDialogClose() == static_cast<int>(0x80B8000Bu));
    Require(sceNpCommerceDialogGetStatus() == statusFinished);
    Require(sceNpCommerceDialogTerminate() == 0);
    Require(sceNpCommerceDialogGetStatus() == statusNone);

    NpCommerceDialogParam param{};
    param.base.size = sizeof(param.base);
    param.base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&param)) - 1060003575u;
    param.size = sizeof(param);
    param.userId = USER_SERVICE_INITIAL_USER_ID;
    Require(sceNpCommerceDialogInitialize() == 0);
    param.base.size = 0;
    Require(sceNpCommerceDialogOpen(&param) == static_cast<int>(0x80B8000Au));
    param.base.size = sizeof(param.base);
    param.size = 0;
    Require(sceNpCommerceDialogOpen(&param) == static_cast<int>(0x80B8000Au));
    param.size = sizeof(param);
    param.option = 8;
    Require(sceNpCommerceDialogOpen(&param) == static_cast<int>(0x80B8000Au));
    param.option = 0;
    param.reserved[31] = 1;
    Require(sceNpCommerceDialogOpen(&param) == static_cast<int>(0x80B8000Au));
    param.reserved[31] = 0;
    param.userId = -1;
    Require(sceNpCommerceDialogOpen(&param) == static_cast<int>(0x80B8000Au));
    param.userId = USER_SERVICE_INITIAL_USER_ID;
    const char* products[11]{};
    for (std::uint32_t mode = 0; mode <= 6; ++mode) {
        param.mode = mode;
        for (std::uint32_t count = 0; count <= 11; ++count) {
            param.productCount = count;
            for (bool hasProducts : {false, true}) {
                param.products = hasProducts ? products : nullptr;
                bool valid = false;
                if (mode == 0 || mode == 2) valid = count <= 1 && hasProducts == (count != 0);
                if (mode == 1) valid = count == 1 && hasProducts;
                if (mode == 3) valid = count >= 1 && count <= 10 && hasProducts;
                if (mode == 4) valid = count <= 10 && hasProducts == (count != 0);
                Require(sceNpCommerceDialogOpen(&param) == (valid ? 0 : static_cast<int>(0x80B8000Au)));
                param2.mode = mode;
                param2.productCount = count;
                param2.products = param.products;
                Require(sceNpCommerceDialogOpen2(&param2) == (valid ? 0 : static_cast<int>(0x80B8000Au)));
            }
        }
    }
    param2.mode = 1;
    param2.contentMode = 1;
    param2.productCount = 1;
    param2.products = products;
    param2.option = 99;
    Require(sceNpCommerceDialogOpen2(&param2) == 0);
    param2.option = 100;
    Require(sceNpCommerceDialogOpen2(&param2) == static_cast<int>(0x80B8000Au));
    param2.option = 0;
    param2.mode = 2;
    bool threw = false;
    try {
        sceNpCommerceDialogOpen2(&param2);

    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    param2.contentMode = 0;
    param2.mode = 0;
    param2.productCount = 0;
    param2.products = nullptr;
    param2.reserved[31] = 1;
    Require(sceNpCommerceDialogOpen2(&param2) == static_cast<int>(0x80B8000Au));
    param2.reserved[31] = 0;
    param2.size = 0;
    Require(sceNpCommerceDialogOpen2(&param2) == static_cast<int>(0x80B8000Au));
    param2.size = sizeof(param2);
    param2.userId = -1;
    Require(sceNpCommerceDialogOpen2(&param2) == static_cast<int>(0x80B8000Au));
    param2.userId = USER_SERVICE_INITIAL_USER_ID;
    param2.userData = 0x123456789abcdef0;
    for (std::uint32_t mode : {1000u, 1001u}) {
        param.mode = mode;
        param.productCount = 0;
        param.argument = 1;
        Require(sceNpCommerceDialogOpen(&param) == static_cast<int>(0x80B8000Au));
    }
    param.mode = 5;
    param.productCount = 0;
    param.products = nullptr;
    for (std::uint64_t argument = 0; argument <= 4; ++argument) {
        param.argument = argument;
        Require(sceNpCommerceDialogOpen(&param) == (argument == 1 ? 0 : static_cast<int>(0x80B8000Au)));
    }
    sdkVersion = 0x01700000;
    std::memcpy(process.data() + 16, &sdkVersion, sizeof(sdkVersion));
    param.argument = 3;
    param.userData = 0xfedcba9876543210;
    Require(sceNpCommerceDialogOpen(&param) == 0);
    RequireCanceledResult(param.userData, true);
    sdkVersion = 0x01600000;
    std::memcpy(process.data() + 16, &sdkVersion, sizeof(sdkVersion));
    param.size = 0;
    param.option = 100;
    param.userId = -1;
    param.reserved[31] = 1;
    param.argument = 7;
    Require(sceNpCommerceDialogOpen(&param) == 0);
    RequireCanceledResult(param.userData, false);
    Require(sceNpCommerceDialogOpen2(&param2) == 0);
    Require(sceNpCommerceDialogUpdateStatus() == statusFinished);
    RequireCanceledResult(param2.userData, false);
    Require(sceNpCommerceDialogTerminate() == 0);
    Require(sceNpCommerceDialogTerminate() == static_cast<int>(0x80B80003u));
    Require(sceNpCommerceDialogClose() == static_cast<int>(0x80B80003u));
    Require(sceNpCommerceDialogOpen(nullptr) == static_cast<int>(0x80B8000Du));
    Require(sceNpCommerceDialogOpen2(nullptr) == static_cast<int>(0x80B8000Du));
    Require(sceNpCommerceDialogOpen2(&param2) == static_cast<int>(0x80B80003u));
    Require(sceNpCommerceDialogGetResult(nullptr) == static_cast<int>(0x80B8000Du));
    Require(sceNpCommerceDialogGetResult(&result) == static_cast<int>(0x80B80003u));
    Require(sceNpCommerceDialogGetStatus() == statusNone);
    Require(sceNpCommerceDialogUpdateStatus() == statusNone);
    RequireNotices();
    Require(sceUserServiceTerminate() == 0);
    return 0;
}
