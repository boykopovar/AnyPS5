#include "prx/libSceSaveData/include/Api.hpp"
#include <cstdlib>
#include <stdexcept>
#include <string_view>

namespace {

constexpr int SaveDataErrorNotInitialized = -2137063423;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename Function>
void RequireThrowsUnimplemented(Function function, std::string_view expectedName) {
    try {
        function();
    } catch (const std::runtime_error& error) {
        Require(std::string_view(error.what()).find(expectedName) != std::string_view::npos);
        Require(std::string_view(error.what()).find("not implemented") != std::string_view::npos);
        return;
    }
    std::abort();
}

void CheckDeleteVariants() {
    RequireThrowsUnimplemented([] { (void)sceSaveDataDeleteBackupData(); }, "sceSaveDataDeleteBackupData");
    RequireThrowsUnimplemented([] { (void)sceSaveDataDeleteTitle(); }, "sceSaveDataDeleteTitle");
    RequireThrowsUnimplemented([] { (void)sceSaveDataDeleteUser(); }, "sceSaveDataDeleteUser");
    RequireThrowsUnimplemented([] { (void)sceSaveDataDeleteAllUser(); }, "sceSaveDataDeleteAllUser");
    RequireThrowsUnimplemented([] { (void)sceSaveDataDeleteProspero(); }, "sceSaveDataDeleteProspero");
    RequireThrowsUnimplemented([] { (void)sceSaveDataManualDelete(); }, "sceSaveDataManualDelete");
}

} // namespace

int main() {
    Require(sceSaveDataTerminate() == SaveDataErrorNotInitialized);
    CheckDeleteVariants();

    Require(sceSaveDataInitialize3(nullptr) == 0);
    CheckDeleteVariants();

    Require(sceSaveDataTerminate() == 0);
    Require(sceSaveDataTerminate() == SaveDataErrorNotInitialized);
    CheckDeleteVariants();

    return 0;
}
