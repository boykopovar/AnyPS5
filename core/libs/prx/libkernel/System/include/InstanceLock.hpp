#ifndef CORE_LIBS_PRX_LIBKERNEL_SYSTEM_INCLUDE_INSTANCELOCK_HPP
#define CORE_LIBS_PRX_LIBKERNEL_SYSTEM_INCLUDE_INSTANCELOCK_HPP

#include <filesystem>
#include <string_view>

enum class InstanceLockMode {
    Off,
    On,
};

constexpr char InstanceLockVariable[] = "ANYPS5_INSTANCE_LOCK";
constexpr char InstanceLockFileName[] = ".anyps5-instance.lock";

class IInstanceLockBackend {
public:
    virtual ~IInstanceLockBackend() = default;
    virtual bool TryLock(const std::filesystem::path& file) = 0;
};

InstanceLockMode ParseInstanceLockMode(const char* value);
std::filesystem::path InstanceLockPath(const std::filesystem::path& root);
void AcquireInstanceLock(IInstanceLockBackend& backend, const std::filesystem::path& root, std::string_view titleId);

#endif
