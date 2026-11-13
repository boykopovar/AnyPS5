#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BINDLESSRESOLVERS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BINDLESSRESOLVERS_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureService.hpp"
#include <cstdint>
#include <functional>
#include <memory>

namespace ShaderRecompiler {
struct BindlessInterface;
}

namespace AgcDriver::Graphics {

class GuestBindlessMemory final : public BindlessMemory {
public:
    bool Read(std::uint64_t address, std::span<std::uint32_t> words) override;
    std::uint64_t Collect(std::uint64_t address, std::size_t bytes) override;
    bool Unchanged(std::uint64_t address, std::size_t bytes, std::uint64_t generation) override;
    std::uint64_t MappingSerial() override;
    bool Mapped(std::uint64_t address, std::size_t bytes) override;
};

class DriverBindlessResolver final : public BindlessResolver {
public:
    using InterfaceLookup = std::function<std::shared_ptr<const ShaderRecompiler::BindlessInterface>(std::uint64_t)>;
    explicit DriverBindlessResolver(const Context& context, InterfaceLookup lookup = {});
    BindlessPrepared Prepare(const DeviceImageKeys::Key& key) override;
    BindlessResolution Commit(const BindlessPrepared& prepared) override;
    bool Current(const BindlessResolution& resolution) override;
    bool RefreshGeneration(BindlessPrepared& prepared) override;

private:
    Context context;
    InterfaceLookup lookup;
};

}

#endif
