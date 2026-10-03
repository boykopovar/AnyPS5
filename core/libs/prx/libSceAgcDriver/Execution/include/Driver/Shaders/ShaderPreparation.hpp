#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_SHADERS_SHADERPREPARATION_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_SHADERS_SHADERPREPARATION_HPP

#include "prx/libSceAgcDriver/Execution/include/Driver/DeviceAccess.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

struct Shader;

namespace AgcDriver::DriverDetail {

class ShaderPreparation {
public:
    explicit ShaderPreparation(const DevicePointer& device);
    ~ShaderPreparation();
    ShaderPreparation(const ShaderPreparation&) = delete;
    ShaderPreparation& operator=(const ShaderPreparation&) = delete;

    void Enqueue(const Shader& shader, std::shared_ptr<const ShaderSnapshot> snapshot);
    void Stop();

private:
    struct Pending {
        std::shared_ptr<const ShaderSnapshot> snapshot;
        Registers registers;
        std::uint32_t waveSize;
    };

    void run();
    void prepare(const Pending& pending) const;

    const DevicePointer& device;
    std::mutex mutex;
    std::condition_variable changed;
    std::deque<Pending> pending;
    std::thread worker;
    bool stopping = false;
};

}

#endif
