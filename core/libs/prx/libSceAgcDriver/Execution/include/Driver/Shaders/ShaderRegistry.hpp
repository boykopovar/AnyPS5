#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_SHADERREGISTRY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_SHADERREGISTRY_HPP

#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace AgcDriver::DriverDetail {

struct HandleMemos {
    struct Entry {
        std::uint64_t key = 0;
        std::shared_ptr<const ShaderRecompiler::SourceHandle> handle;
        std::shared_ptr<const std::string> failure;
    };
    std::mutex mutex;
    std::array<Entry, 8> entries;
    std::size_t next = 0;
    std::atomic<std::uint32_t> poisoned{0};
};

template<typename Action>
struct UnwindAction {
    Action action;
    int exceptions = std::uncaught_exceptions();
    ~UnwindAction() {
        if (std::uncaught_exceptions() <= exceptions) return;
        try {
            action();
        } catch (...) {
        }
    }
};

struct ShaderSnapshot {
    std::uint64_t codeAddress;
    std::uint64_t headerAddress;
    std::uint8_t type;
    std::vector<std::uint32_t> code;
    std::vector<std::byte> header;
    std::unique_ptr<HandleMemos> handles = std::make_unique<HandleMemos>();
};

using ShaderRegistry = std::map<std::uint64_t, std::shared_ptr<const ShaderSnapshot>>;

bool FailureMemo();

std::shared_ptr<const ShaderRecompiler::SourceHandle> SourceHandleFor(const ShaderSnapshot& snapshot, std::size_t codeOffset, std::uint64_t deviceSerial, const ShaderRecompiler::RecompileRequest& request, bool bypass, const std::string** poisoned = nullptr);

}

#endif
