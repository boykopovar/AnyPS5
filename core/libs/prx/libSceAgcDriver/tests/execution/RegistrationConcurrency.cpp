#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderPreparation.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include <array>
#include <barrier>
#include <chrono>
#include <future>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::DriverDetail::ShaderPreparationTransaction;

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TAction>
void ExpectFailure(TAction action, const char* expected) {
    try { action(); }
    catch (const std::exception& error) {
        Require(std::string(error.what()).find(expected) != std::string::npos, error.what());
        return;
    }
    throw std::runtime_error("expected shader preparation failure");
}

class ComputeFixture {
public:
    struct Header {
        Shader shader{};
        std::array<ShaderRegister, 7> registers{};
        ShaderSpecialRegs specials{};
    } header;

    ComputeFixture() {
        const auto address = reinterpret_cast<std::uintptr_t>(code.data());
        header.shader.file_header = 0x34333231u;
        header.shader.version = 0x18;
        header.shader.header_size = sizeof(header);
        header.shader.shader_size = sizeof(code);
        header.shader.code = code.data();
        header.shader.sh_registers = header.registers.data();
        header.shader.num_sh_registers = header.registers.size();
        header.shader.specials = &header.specials;
        header.specials.dispatch_modifier = 0x8000;
        header.registers = {{{0x20c, static_cast<std::uint32_t>(address >> 8u)}, {0x20d, static_cast<std::uint32_t>(address >> 40u)}, {0x207, 1}, {0x208, 1}, {0x209, 1}, {0x212, 0}, {0x213, 0}}};
    }

    ComputeFixture(const ComputeFixture&) = delete;
    ComputeFixture& operator=(const ComputeFixture&) = delete;

private:
    alignas(256) std::array<std::uint32_t, 1> code{0xbf810000u};
};

void DeferredOperations() {
    for (unsigned operation = 0; operation < 4; ++operation) {
        AgcDriver::DriverDetail::ShaderSnapshot snapshot;
        std::promise<void> created;
        auto started = created.get_future();
        std::promise<void> completed;
        auto finished = completed.get_future();
        std::future<void> worker;
        bool deferred = false;
        bool blocked = false;
        {
            ShaderPreparationTransaction held;
            worker = std::async(std::launch::async, [&] {
                ShaderPreparationTransaction transaction(std::defer_lock);
                created.set_value();
                switch (operation) {
                case 0:
                    static_cast<void>(transaction.Read(snapshot));
                    break;
                case 1:
                    transaction.Edit(snapshot).rectangleRequested = true;
                    break;
                case 2:
                    transaction.Commit();
                    break;
                default: {
                    ShaderPreparationTransaction nested;
                    nested.Commit();
                    break;
                }
                }
                completed.set_value();
                if (operation != 2) transaction.Commit();
            });
            deferred = started.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
            blocked = finished.wait_for(std::chrono::milliseconds(100)) == std::future_status::timeout;
            held.Commit();
        }
        worker.get();
        Require(deferred, "deferred preparation acquired the writer before a shared-state operation");
        Require(blocked, "deferred preparation accessed shared state without acquiring the writer");
        Require(snapshot.prepared->rectangleRequested == (operation == 1), "deferred preparation lost its committed edit");
    }
}

void PreparationWhilePublicationBlocked() {
    ComputeFixture invalid;
    invalid.header.registers[2].value = 0;
    std::future<void> worker;
    bool prepared = false;
    {
        ShaderPreparationTransaction held;
        worker = std::async(std::launch::async, [&] {
            ExpectFailure([&] { AgcDriverRegisterShader_nid_postfix(&invalid.header.shader); }, "must be nonzero");
        });
        prepared = worker.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
        held.Commit();
    }
    worker.get();
    Require(prepared, "private shader preparation waited for an unrelated publication transaction");
}

void NestedFailure() {
    AgcDriver::DriverDetail::ShaderSnapshot pending;
    ComputeFixture invalid;
    invalid.header.registers[2].value = 0;
    {
        ShaderPreparationTransaction transaction;
        transaction.Edit(pending).registeredAbis.push_back({17});
        ExpectFailure([&] { AgcDriverRegisterShader_nid_postfix(&invalid.header.shader); }, "must be nonzero");
        ExpectFailure([&] { transaction.Commit(); }, "transaction was aborted");
    }
    Require(pending.prepared->registeredAbis.empty(), "caught registration failure published the enclosing preparation transaction");
}

void ConcurrentRegistrations() {
    std::array<ComputeFixture, 4> fixtures;
    constexpr std::size_t workerCount = 8;
    std::barrier start(workerCount);
    std::vector<std::future<void>> workers;
    for (std::size_t index = 0; index < fixtures.size(); ++index) {
        fixtures[index].header.registers[2].value = static_cast<std::uint32_t>(index + 1);
    }
    for (std::size_t worker = 0; worker < workerCount; ++worker) {
        const auto index = worker % fixtures.size();
        workers.push_back(std::async(std::launch::async, [&, index] {
            start.arrive_and_wait();
            const auto* shader = &fixtures[index].header.shader;
            AgcDriverRegisterShader_nid_postfix(shader);
            AgcDriverResolveShaderAbi_nid_postfix(shader, {}, {});
            AgcDriverRegisterShader_nid_postfix(shader);
        }));
    }
    for (auto& worker : workers) worker.get();
    std::vector<std::uint32_t> commands;
    for (const auto& fixture : fixtures) {
        for (const auto reg : fixture.header.registers) commands.insert(commands.end(), {0xc0017600u, reg.offset, reg.value});
        commands.insert(commands.end(), {0xc0031500u, 1, 1, 1, 0x8041});
    }
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    sceAgcDriverSubmitAcb(0x20, &packet);
    AgcDriverWaitIdle_nid_postfix();
}

}

int main() {
    try {
        DeferredOperations();
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        device.reset();
        PreparationWhilePublicationBlocked();
        NestedFailure();
        ConcurrentRegistrations();
        AgcDriverShutdown_nid_postfix();
        std::cout << "concurrent shader registration tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try {
            AgcDriverShutdown_nid_postfix();
        } catch (const std::exception& shutdownError) {
            std::cerr << shutdownError.what() << '\n';
        }
        return 1;
    }
}
