#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"
#include "prx/libkernel/System/include/InstanceLockHost.hpp"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string_view>

namespace {

std::unique_ptr<IInstanceLockBackend>& HeldBackend() {
    static auto* backend = new std::unique_ptr<IInstanceLockBackend>;
    return *backend;
}

bool HasTitleMetadata(const std::filesystem::path& root) {
    const auto metadata = root / "app0" / "sce_sys";
    return std::filesystem::exists(metadata / "param.json") || std::filesystem::exists(metadata / "param.sfo");
}

const bool InstanceLockChecked = [] {
    if (ParseInstanceLockMode(std::getenv("ANYPS5_INSTANCE_LOCK")) == InstanceLockMode::Off) return false;
    const auto root = std::filesystem::current_path();
    if (!HasTitleMetadata(root)) return false;
    const auto titleId = GetAppTitleId_nid_postfix();
    auto backend = MakeHostInstanceLockBackend();
    AcquireInstanceLock(*backend, root, std::string_view(titleId.value, strnlen(titleId.value, sizeof(titleId.value))));
    HeldBackend() = std::move(backend);
    return true;
}();

}

void ReleaseHostInstanceLock() {
    HeldBackend().reset();
}
