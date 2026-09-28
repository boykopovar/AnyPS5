#include "prx/libSceAgcDriver/Graphics/include/ColorTargetTransfer.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include <vector>

namespace AgcDriver::Graphics {

void ReadColorTarget(const ColorTarget& target, std::span<std::byte> destination) {
    PerformanceTimer timing("ColorTarget.Read");
    const ColorTargetLayout layout(target.extent.width, target.extent.height, target.tileMode, target.elementBytes);
    Require(target.bytes == layout.Bytes() && destination.size() == layout.LinearBytes(), "color target transfer size mismatch");
    if (target.tileMode == ColorTileMode::Linear) {
        GuestMemory::Read(target.address, destination, layout.Alignment());
        return;
    }
    std::vector<std::byte> tiled(layout.Bytes());
    timing.Mark("allocate");
    GuestMemory::Read(target.address, tiled, layout.Alignment());
    timing.Mark("guest_read");
    layout.Detile(tiled, destination);
    timing.Mark("detile");
}

void WriteColorTarget(const ColorTarget& target, std::span<const std::byte> source) {
    PerformanceTimer timing("ColorTarget.Write");
    const ColorTargetLayout layout(target.extent.width, target.extent.height, target.tileMode, target.elementBytes);
    Require(target.bytes == layout.Bytes() && source.size() == layout.LinearBytes(), "color target transfer size mismatch");
    if (target.tileMode == ColorTileMode::Linear) {
        GuestMemory::Write(target.address, source, layout.Alignment());
        return;
    }
    std::vector<std::byte> tiled(layout.Bytes());
    timing.Mark("allocate");
    GuestMemory::Read(target.address, tiled, layout.Alignment());
    timing.Mark("guest_read");
    layout.Tile(source, tiled);
    timing.Mark("tile");
    GuestMemory::Write(target.address, tiled, layout.Alignment());
    timing.Mark("guest_write");
}

}
