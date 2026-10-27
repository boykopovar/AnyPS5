#include "prx/libSceAgcDriver/Graphics/include/BarrierBatch.hpp"
#include <cstdio>

using namespace AgcDriver::Graphics;

namespace {

int failures = 0;

void Expect(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
}

constexpr VkPipelineStageFlags All = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
constexpr VkPipelineStageFlags Transfer = VK_PIPELINE_STAGE_TRANSFER_BIT;
constexpr VkPipelineStageFlags Compute = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
constexpr VkPipelineStageFlags Host = VK_PIPELINE_STAGE_HOST_BIT;
constexpr VkAccessFlags SW = VK_ACCESS_SHADER_WRITE_BIT;
constexpr VkAccessFlags SR = VK_ACCESS_SHADER_READ_BIT;
constexpr VkAccessFlags TW = VK_ACCESS_TRANSFER_WRITE_BIT;
constexpr VkAccessFlags TR = VK_ACCESS_TRANSFER_READ_BIT;
constexpr VkAccessFlags HW = VK_ACCESS_HOST_WRITE_BIT;
constexpr VkAccessFlags HR = VK_ACCESS_HOST_READ_BIT;
constexpr VkAccessFlags CAW = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
constexpr VkAccessFlags DSW = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
constexpr VkAccessFlags Full = SR | SW | TR | TW;

const BarrierMask PassEnd{All, All, SW | CAW | DSW | TW, Full};
const BarrierMask LabelLeading{All, Transfer, SW | TW | CAW, TW};
const BarrierMask StagingInLeading{All | Host, Transfer, SW | TW | HW, TR};
const BarrierMask StagingOutLeading{All, Transfer, SW, TR};
const BarrierMask WriteBackReady{All | Host, Compute, SR | SW | TR | TW | HW | CAW, SR | SW};

void testCoversStages() {
    Expect(Covers(PassEnd, LabelLeading), "a pass end covers the label run's leading barrier");
    Expect(!Covers(LabelLeading, PassEnd), "the leading barrier does not cover the pass end");
    Expect(Covers(PassEnd, PassEnd), "a barrier covers itself");
    Expect(Covers(BarrierMask{}, BarrierMask{}), "the empty barrier covers the empty one");
    Expect(Covers(PassEnd, StagingOutLeading), "ALL_COMMANDS holds a transfer destination and a shader write");
    Expect(!Covers(PassEnd, StagingInLeading), "ALL_COMMANDS does not hold the host stage or a host write");
    Expect(Covers(StagingInLeading, StagingOutLeading), "a wider source covers the narrower one");
    const BarrierMask computeOnly{Compute, Compute, SW, SR};
    Expect(!Covers(computeOnly, PassEnd), "a compute-only barrier does not cover ALL_COMMANDS");
    Expect(!Covers(computeOnly, StagingOutLeading), "a compute destination does not hold a transfer destination");
}

void testCoversAccess() {
    const BarrierMask noColor{All, All, SW | TW, Full};
    Expect(!Covers(noColor, LabelLeading), "a source without the colour attachment write does not cover a barrier that asks for it");
    const BarrierMask readOnlyDestination{All, All, SW | CAW | DSW | TW, SR | SW};
    Expect(!Covers(readOnlyDestination, LabelLeading), "a destination without the transfer write does not cover a barrier that asks for it");
    const BarrierMask same{All, Transfer, SW | TW | CAW, TW};
    Expect(Covers(same, LabelLeading) && Covers(LabelLeading, same), "equal barriers cover each other");
}

void testCoversImage() {
    BarrierMask transition = LabelLeading;
    transition.changesLayout = true;
    Expect(!Covers(PassEnd, transition), "a barrier that changes a layout is never covered by a memory barrier");
    BarrierMask transfer = LabelLeading;
    transfer.transfersQueue = true;
    Expect(!Covers(PassEnd, transfer), "a queue family ownership transfer is never covered");
    BarrierMask sameLayout = LabelLeading;
    Expect(Covers(PassEnd, sameLayout), "an image barrier that keeps the layout and the queue is covered like a memory barrier");
}

void testOrderedByTrailing() {
    constexpr VkAccessFlags dispatched = Full | HR;
    constexpr VkAccessFlags filled = Full | VK_ACCESS_INDIRECT_COMMAND_READ_BIT | HR;
    Expect(OrderedByTrailing(dispatched, StagingInLeading), "a dispatch's trailing barrier orders the staging copy-in");
    Expect(OrderedByTrailing(dispatched, StagingOutLeading), "a dispatch's trailing barrier orders the staging copy-out");
    Expect(OrderedByTrailing(filled, WriteBackReady), "a fill's trailing barrier orders the write-back's import-ready barrier");
    Expect(OrderedByTrailing(Full, LabelLeading), "the pass end's destination accesses order the label run");
    Expect(!OrderedByTrailing(0, StagingInLeading), "an intervening command clears the covered accesses, so the barrier is kept");
    Expect(!OrderedByTrailing(0, StagingOutLeading) && !OrderedByTrailing(0, WriteBackReady), "no trailing barrier, no elision");
    Expect(!OrderedByTrailing(SR | SW, StagingInLeading), "a shader-only trailing barrier does not order a transfer read");
    Expect(!OrderedByTrailing(SR | SW | TW, StagingInLeading), "a trailing barrier without the transfer read is kept");
    constexpr VkAccessFlags stagedIn = SR | VK_ACCESS_UNIFORM_READ_BIT | SW;
    Expect(!OrderedByTrailing(stagedIn, StagingInLeading), "the staging copy-in's own trailing barrier does not order the next copy-in");
    Expect(!OrderedByTrailing(Full, BarrierMask{All, Transfer, SW, TR | VK_ACCESS_INDIRECT_COMMAND_READ_BIT}), "a destination access the trailing barrier lacks is kept");
}

void testHostAndImage() {
    BarrierMask toHost = StagingOutLeading;
    toHost.dstStage |= Host;
    toHost.dstAccess |= HR;
    Expect(!OrderedByTrailing(Full | HR, toHost), "a barrier that makes writes visible to the host is kept");
    BarrierMask ready = WriteBackReady;
    Expect(OrderedByTrailing(Full | HR, ready), "an image barrier that keeps its layout is elided like a memory barrier");
    ready.changesLayout = true;
    Expect(!OrderedByTrailing(Full | HR, ready), "an image barrier that changes the layout is kept");
    ready.changesLayout = false;
    ready.transfersQueue = true;
    Expect(!OrderedByTrailing(Full | HR, ready), "a queue family transfer is kept");
}

}

int main() {
    testCoversStages();
    testCoversAccess();
    testCoversImage();
    testOrderedByTrailing();
    testHostAndImage();
    if (failures != 0) {
        std::fprintf(stderr, "%d barrier batch checks failed\n", failures);
        return 1;
    }
    std::printf("barrier batch tests passed\n");
    return 0;
}
