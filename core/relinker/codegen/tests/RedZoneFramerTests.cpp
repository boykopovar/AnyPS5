#include <codegen/x86/RedZoneFramer.hpp>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

constexpr std::uint64_t Base = 0x1000;

std::vector<std::uint8_t> padTo(std::vector<std::uint8_t> bytes, const std::size_t size) {
    bytes.resize(size, 0xcc);
    return bytes;
}

void append(std::vector<std::uint8_t>& bytes, const std::vector<std::uint8_t>& more) {
    bytes.insert(bytes.end(), more.begin(), more.end());
}

std::vector<std::uint8_t> callTo(const std::size_t from, const std::size_t target) {
    const auto displacement = static_cast<std::int32_t>(target) - static_cast<std::int32_t>(from + 5);
    return {0xe8, static_cast<std::uint8_t>(displacement), static_cast<std::uint8_t>(displacement >> 8), static_cast<std::uint8_t>(displacement >> 16), static_cast<std::uint8_t>(displacement >> 24)};
}

const std::vector<std::uint8_t> Prologue{0x55, 0x48, 0x89, 0xe5, 0x41, 0x57, 0x53};
const std::vector<std::uint8_t> Exit{0x5b, 0x41, 0x5f, 0x5d, 0xc3};
const std::vector<std::uint8_t> StoreBelowPushes{0x48, 0x89, 0x7d, 0xe8};
const std::vector<std::uint8_t> LoadBelowPushes{0x48, 0x8b, 0x45, 0xe8};
const std::vector<std::uint8_t> LoadAbovePushes{0x48, 0x8b, 0x45, 0x08};

std::vector<std::uint8_t> leaf(const std::vector<std::uint8_t>& body) {
    std::vector<std::uint8_t> bytes = Prologue;
    append(bytes, body);
    append(bytes, Exit);
    return bytes;
}

struct Image {
    std::vector<std::uint8_t> bytes;
    std::vector<Domain::ProgramHeader> segments;
    std::vector<Codegen::RedZoneFunction> functions;
};

Image image(const std::vector<std::vector<std::uint8_t>>& functions) {
    Image result;
    std::vector<std::size_t> starts;
    std::size_t at = 0x40;
    for (const auto& function : functions) {
        starts.push_back(at);
        at += (function.size() + 15) / 16 * 16 + 16;
    }
    std::vector<std::uint8_t> caller;
    for (const auto start : starts) append(caller, callTo(caller.size(), start));
    caller.push_back(0xc3);
    result.bytes = padTo(caller, 0x40);
    for (std::size_t index = 0; index < functions.size(); ++index) {
        result.bytes = padTo(result.bytes, starts[index]);
        append(result.bytes, functions[index]);
        result.functions.push_back({Base + starts[index], Base + starts[index] + functions[index].size()});
    }
    result.bytes = padTo(result.bytes, (result.bytes.size() + 15) / 16 * 16 + 16);
    result.segments.push_back({1, 5, 0, Base, Base, result.bytes.size(), result.bytes.size(), 16});
    return result;
}

void framesRedZoneLeaf() {
    std::vector<std::uint8_t> body = StoreBelowPushes;
    append(body, LoadBelowPushes);
    const auto input = image({leaf(body)});
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(input.bytes, input.segments, input.functions, {}, report, displacements);
    require(report.RedZoneFunctions == 1 && report.Framed == 1 && report.Exits == 1, "a red-zone leaf was not framed");
    require(sites.size() == 2, "a framed leaf needs one prologue and one exit site");
    require(displacements.empty() && report.FramedWithoutFramePointer == 0, "a leaf with a frame pointer had rsp operands moved");

    const auto& entry = sites[0];
    require(entry.Address == Base + 0x40 && entry.Length == Prologue.size(), "the prologue site does not cover the pushes");
    require(entry.OriginalBytes == Prologue, "the prologue site lost its bytes");
    std::vector<std::uint8_t> entryBody = Prologue;
    append(entryBody, {0x48, 0x8d, 0x64, 0x24, 0x80, 0xe9, 0, 0, 0, 0});
    require(entry.Body == entryBody && entry.ReturnBranchOffset == Prologue.size() + 5, "the prologue stub does not lower rsp after the pushes");

    const auto& exit = sites[1];
    require(exit.Address == Base + 0x40 + Prologue.size() + body.size() && exit.Length == Exit.size(), "the exit site does not cover the pops and ret");
    std::vector<std::uint8_t> exitBody{0x48, 0x8d, 0xa4, 0x24, 0x80, 0, 0, 0};
    append(exitBody, Exit);
    append(exitBody, {0xe9, 0, 0, 0, 0});
    require(exit.Body == exitBody && exit.ReturnBranchOffset == 8 + Exit.size(), "the exit stub does not raise rsp before the pops");
}

const std::vector<std::uint8_t> FramelessPrologue{0x53, 0x41, 0x56, 0x41, 0x57};
const std::vector<std::uint8_t> FramelessExit{0x41, 0x5f, 0x41, 0x5e, 0x5b, 0xc3};
const std::vector<std::uint8_t> StoreBelowRsp{0x48, 0x89, 0x7c, 0x24, 0xf0};
const std::vector<std::uint8_t> LoadBelowRsp{0x48, 0x8b, 0x44, 0x24, 0xf0};
const std::vector<std::uint8_t> LoadAboveRsp{0x48, 0x8b, 0x44, 0x24, 0x08};
const std::vector<std::uint8_t> LoadFarAboveRsp{0x48, 0x8b, 0x44, 0x24, 0x78};
const std::vector<std::uint8_t> LoadAtRsp{0x48, 0x8b, 0x04, 0x24};
const std::vector<std::uint8_t> StoreDeepBelowRsp{0x48, 0x89, 0x74, 0x24, 0xd8};
const std::vector<std::uint8_t> LoadDeepBelowRsp{0x48, 0x8b, 0x44, 0x24, 0xd8};
const std::vector<std::uint8_t> AddressBelowRsp{0x48, 0x8d, 0x44, 0x24, 0xe0};
const std::vector<std::uint8_t> AddressAboveRsp{0x48, 0x8d, 0x44, 0x24, 0x08};
const std::vector<std::uint8_t> StoreIndexedBelowRsp{0x48, 0x89, 0x7c, 0xcc, 0xf0};
const std::vector<std::uint8_t> StoreImmediateFarAboveRsp{0xc7, 0x44, 0x24, 0x7c, 0x78, 0x56, 0x34, 0x12};
const std::vector<std::uint8_t> LoadIndexedAtRsp{0x48, 0x8b, 0x04, 0xcc};
const std::vector<std::uint8_t> AddRdxToRax{0x48, 0x01, 0xd0};
const std::vector<std::uint8_t> VexStoreAtRsp{0xc5, 0xf8, 0x11, 0x04, 0x24};
const std::vector<std::uint8_t> EvexStoreBelowRsp{0x62, 0xf1, 0x7c, 0x48, 0x11, 0x44, 0x24, 0xff};
const std::vector<std::uint8_t> LoadRipRelative{0x48, 0x8b, 0x05, 0x00, 0x00, 0x00, 0x00};

std::vector<std::uint8_t> stubBody(std::vector<std::uint8_t> code) {
    append(code, {0xe9, 0, 0, 0, 0});
    return code;
}

std::vector<std::uint8_t> framelessLeaf(const std::vector<std::uint8_t>& prologue, const std::vector<std::uint8_t>& body, const std::vector<std::uint8_t>& exit) {
    std::vector<std::uint8_t> bytes = prologue;
    append(bytes, body);
    append(bytes, exit);
    return bytes;
}

void framesLeafWithoutFramePointer() {
    std::vector<std::uint8_t> body = StoreBelowRsp;
    append(body, LoadBelowRsp);
    const auto input = image({framelessLeaf(FramelessPrologue, body, FramelessExit)});
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(input.bytes, input.segments, input.functions, {}, report, displacements);
    require(report.RedZoneFunctions == 1 && report.Framed == 1 && report.FramedWithoutFramePointer == 1 && report.MovedOperands == 2, "a leaf that only pushes was not framed");
    require(sites.size() == 2, "a framed leaf needs one prologue and one exit site");
    const auto& entry = sites[0];
    require(entry.Address == Base + 0x40 && entry.Length == FramelessPrologue.size(), "the prologue site does not cover the pushes");
    std::vector<std::uint8_t> entryBody = FramelessPrologue;
    append(entryBody, {0x48, 0x8d, 0x64, 0x24, 0xf0, 0xe9, 0, 0, 0, 0});
    require(entry.Body == entryBody, "the prologue stub does not lower rsp by the leaf's depth below rsp after the pushes");
    const auto& exit = sites[1];
    require(exit.Address == Base + 0x40 + FramelessPrologue.size() + body.size() && exit.OriginalBytes == FramelessExit, "the exit site does not cover the pops and ret");
    std::vector<std::uint8_t> exitBody{0x48, 0x8d, 0xa4, 0x24, 0x10, 0, 0, 0};
    append(exitBody, FramelessExit);
    append(exitBody, {0xe9, 0, 0, 0, 0});
    require(exit.Body == exitBody, "the exit stub does not raise rsp by the leaf's depth below rsp before the pops");
    const auto storeDisplacement = 0x40 + FramelessPrologue.size() + 4;
    const auto loadDisplacement = storeDisplacement + StoreBelowRsp.size();
    require(displacements.size() == 2, "both rsp operands must move");
    require(displacements[0].Offset == storeDisplacement && displacements[0].Bytes == std::vector<std::uint8_t>{0x00}, "the store below rsp was not moved to the lowered rsp");
    require(displacements[1].Offset == loadDisplacement && displacements[1].Bytes == std::vector<std::uint8_t>{0x00}, "the load below rsp was not moved to the lowered rsp");
}

void shiftsByTheDeepestOperandBelowRsp() {
    std::vector<std::uint8_t> body = StoreBelowRsp;
    append(body, StoreDeepBelowRsp);
    append(body, LoadDeepBelowRsp);
    const auto input = image({framelessLeaf(FramelessPrologue, body, FramelessExit)});
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(input.bytes, input.segments, input.functions, {}, report, displacements);
    require(report.Framed == 1 && sites.size() == 2 && displacements.size() == 3, "a leaf reaching 0x28 bytes below rsp was not framed");
    std::vector<std::uint8_t> entryBody = FramelessPrologue;
    append(entryBody, {0x48, 0x8d, 0x64, 0x24, 0xd0, 0xe9, 0, 0, 0, 0});
    require(sites[0].Body == entryBody, "rsp is not lowered by the deepest displacement rounded up to 16");
    require(std::vector<std::uint8_t>(sites[1].Body.begin(), sites[1].Body.begin() + 8) == std::vector<std::uint8_t>{0x48, 0x8d, 0xa4, 0x24, 0x30, 0, 0, 0}, "rsp is not raised by the shift it was lowered by");
    require(displacements[0].Bytes == std::vector<std::uint8_t>{0x20} && displacements[1].Bytes == std::vector<std::uint8_t>{0x08} && displacements[2].Bytes == std::vector<std::uint8_t>{0x08}, "the operands below rsp were not moved by the shift");
}

void movesPositiveOperandsThatFitAfterTheShift() {
    std::vector<std::uint8_t> body = StoreBelowRsp;
    append(body, LoadAboveRsp);
    const auto input = image({framelessLeaf(FramelessPrologue, body, FramelessExit)});
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(input.bytes, input.segments, input.functions, {}, report, displacements);
    require(report.Framed == 1 && report.MovedOperands == 2 && sites.size() == 2, "a leaf with an 8-bit displacement above rsp that fits after the shift was not framed");
    const auto loadDisplacement = 0x40 + FramelessPrologue.size() + StoreBelowRsp.size() + 4;
    require(displacements.size() == 2 && displacements[1].Offset == loadDisplacement && displacements[1].Bytes == std::vector<std::uint8_t>{0x18}, "the load above rsp was not moved in place by the shift");
}

void shiftsByTheWholeRedZoneForAddressesBelowRsp() {
    std::vector<std::uint8_t> below = StoreBelowRsp;
    append(below, AddressBelowRsp);
    std::vector<std::uint8_t> above = StoreBelowRsp;
    append(above, AddressAboveRsp);
    const auto input = image({framelessLeaf(FramelessPrologue, below, FramelessExit), framelessLeaf(FramelessPrologue, above, FramelessExit)});
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(input.bytes, input.segments, input.functions, {}, report, displacements);
    require(report.Framed == 2 && sites.size() == 4 && displacements.size() == 4, "leaves that take the address of a stack slot were not framed");
    require(sites[0].Body[FramelessPrologue.size() + 4] == 0x80 && displacements[0].Bytes == std::vector<std::uint8_t>{0x70} && displacements[1].Bytes == std::vector<std::uint8_t>{0x60}, "a leaf that takes the address of a slot below rsp was not shifted by the whole red zone");
    require(sites[2].Body[FramelessPrologue.size() + 4] == 0xf0 && displacements[2].Bytes == std::vector<std::uint8_t>{0x00} && displacements[3].Bytes == std::vector<std::uint8_t>{0x18}, "a leaf that only takes the address of a slot above rsp was shifted by more than its depth");
}

void shiftsByTheWholeRedZoneForIndexedOperands() {
    std::vector<std::uint8_t> body = StoreIndexedBelowRsp;
    append(body, LoadBelowRsp);
    const auto input = image({framelessLeaf(FramelessPrologue, body, FramelessExit)});
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(input.bytes, input.segments, input.functions, {}, report, displacements);
    require(report.Framed == 1 && sites.size() == 2 && displacements.size() == 2, "a leaf with an indexed operand below rsp was not framed");
    require(sites[0].Body[FramelessPrologue.size() + 4] == 0x80 && displacements[0].Bytes == std::vector<std::uint8_t>{0x70} && displacements[1].Bytes == std::vector<std::uint8_t>{0x70}, "a leaf whose indexed operand can reach deeper than its displacement was not shifted by the whole red zone");
}

void stubsOperandsWhoseDisplacementOverflows() {
    std::vector<std::uint8_t> body = StoreBelowRsp;
    append(body, LoadFarAboveRsp);
    append(body, StoreImmediateFarAboveRsp);
    const auto input = image({framelessLeaf(FramelessPrologue, body, FramelessExit)});
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(input.bytes, input.segments, input.functions, {}, report, displacements);
    require(report.Framed == 1 && report.MovedOperands == 1 && report.OperandStubs == 2 && report.StubbedOperands == 2 && sites.size() == 4, "a leaf whose 8-bit displacements above rsp overflow after the shift was not framed with operand stubs");
    const auto load = Base + 0x40 + FramelessPrologue.size() + StoreBelowRsp.size();
    require(sites[2].Address == load && sites[2].Length == LoadFarAboveRsp.size() && sites[2].OriginalBytes == LoadFarAboveRsp, "the stub site does not cover the load that cannot move in place");
    require(sites[2].Body == stubBody({0x48, 0x8b, 0x84, 0x24, 0x88, 0x00, 0x00, 0x00}) && sites[2].ReturnBranchOffset == 8, "the load was not re-encoded with a 32-bit displacement moved by the shift");
    require(sites[3].Address == load + LoadFarAboveRsp.size() && sites[3].OriginalBytes == StoreImmediateFarAboveRsp, "the stub site does not cover the store of an immediate that cannot move in place");
    require(sites[3].Body == stubBody({0xc7, 0x84, 0x24, 0x8c, 0x00, 0x00, 0x00, 0x78, 0x56, 0x34, 0x12}), "the store lost its immediate when its displacement was widened");
    require(displacements.size() == 1 && displacements[0].Bytes == std::vector<std::uint8_t>{0x00}, "the store below rsp was not moved in place");
}

void stubsOperandsAtRspWithTheInstructionsAfterThem() {
    std::vector<std::uint8_t> atRsp = StoreBelowRsp;
    append(atRsp, LoadAtRsp);
    append(atRsp, LoadAboveRsp);
    std::vector<std::uint8_t> indexed = StoreBelowRsp;
    append(indexed, LoadIndexedAtRsp);
    append(indexed, AddRdxToRax);
    const auto input = image({framelessLeaf(FramelessPrologue, atRsp, FramelessExit), framelessLeaf(FramelessPrologue, indexed, FramelessExit)});
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(input.bytes, input.segments, input.functions, {}, report, displacements);
    require(report.Framed == 2 && report.MovedOperands == 2 && report.OperandStubs == 2 && report.StubbedOperands == 3 && sites.size() == 6, "leaves with operands at rsp were not framed with operand stubs");
    const auto first = 0x40 + FramelessPrologue.size() + StoreBelowRsp.size();
    require(sites[2].Address == Base + first && sites[2].Length == LoadAtRsp.size() + LoadAboveRsp.size(), "the stub site does not run on to the next instruction to make room for its jump");
    require(sites[2].Body == stubBody({0x48, 0x8b, 0x44, 0x24, 0x10, 0x48, 0x8b, 0x44, 0x24, 0x18}), "the load at rsp and the load the stub covers after it were not both moved by the shift");
    for (const auto& displacement : displacements) require(displacement.Offset < first || displacement.Offset >= first + sites[2].Length, "an rsp operand inside a stub site was also moved in place");
    require(sites[3].Body[FramelessPrologue.size() + 4] == 0x80, "a leaf with an indexed operand at rsp was not shifted by the whole red zone");
    require(sites[5].OriginalBytes.size() == LoadIndexedAtRsp.size() + AddRdxToRax.size() && sites[5].Body == stubBody({0x48, 0x8b, 0x84, 0xcc, 0x80, 0x00, 0x00, 0x00, 0x48, 0x01, 0xd0}), "the indexed load at rsp was not re-encoded with a 32-bit displacement of 0x80 ahead of the add it covers");
}

void stubsVexOperandsAtRsp() {
    std::vector<std::uint8_t> body = StoreBelowRsp;
    append(body, VexStoreAtRsp);
    const auto input = image({framelessLeaf(FramelessPrologue, body, FramelessExit)});
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(input.bytes, input.segments, input.functions, {}, report, displacements);
    require(report.Framed == 1 && report.OperandStubs == 1 && report.StubbedOperands == 1 && sites.size() == 3, "a leaf with a VEX store at rsp was not framed with an operand stub");
    require(sites[2].OriginalBytes == VexStoreAtRsp && sites[2].Body == stubBody({0xc5, 0xf8, 0x11, 0x44, 0x24, 0x10}), "the VEX store at rsp was not re-encoded with an 8-bit displacement moved by the shift");
}

void stubsOperandsAtRspWithTheInstructionsBeforeThem() {
    std::vector<std::uint8_t> body = StoreBelowRsp;
    append(body, LoadAtRsp);
    append(body, LoadRipRelative);
    const auto input = image({framelessLeaf(FramelessPrologue, body, FramelessExit)});
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(input.bytes, input.segments, input.functions, {}, report, displacements);
    require(report.Framed == 1 && report.MovedOperands == 0 && report.OperandStubs == 1 && report.StubbedOperands == 2 && sites.size() == 3 && displacements.empty(), "a leaf whose short operand at rsp sits before a rip-relative instruction was not framed with a stub over the instruction before it");
    const auto store = Base + 0x40 + FramelessPrologue.size();
    require(sites[2].Address == store && sites[2].Length == StoreBelowRsp.size() + LoadAtRsp.size(), "the stub site does not start at the store before the operand at rsp");
    require(sites[2].Body == stubBody({0x48, 0x89, 0x7c, 0x24, 0x00, 0x48, 0x8b, 0x44, 0x24, 0x10}), "the store before the operand and the operand at rsp were not both moved by the shift");
}

void skipsLeavesWithOperandsThatCannotMove() {
    std::vector<std::uint8_t> evex = StoreBelowRsp;
    append(evex, EvexStoreBelowRsp);
    std::vector<std::uint8_t> beforeExit = StoreBelowRsp;
    append(beforeExit, LoadAtRsp);
    std::vector<std::uint8_t> beforeTarget = StoreBelowRsp;
    append(beforeTarget, LoadAtRsp);
    append(beforeTarget, {0xff, 0xc0, 0x3b, 0x44, 0x24, 0xf0, 0x72, 0xf8});
    const auto input = image({framelessLeaf(FramelessPrologue, evex, FramelessExit), framelessLeaf(FramelessPrologue, beforeExit, FramelessExit), framelessLeaf(FramelessPrologue, beforeTarget, FramelessExit), framelessLeaf({0x41, 0x56, 0x41, 0x57}, StoreBelowRsp, {0x41, 0x5f, 0x41, 0x5e, 0xc3})});
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(input.bytes, input.segments, input.functions, {}, report, displacements);
    require(sites.empty() && displacements.empty() && report.RedZoneFunctions == 4, "a leaf whose rsp operands can neither move nor be stubbed, or whose prologue is too short, was framed");
    require(report.Skipped.at("rsp operand that cannot move with rsp") == 1, "an EVEX operand, whose 8-bit displacement is scaled, was not refused");
    require(report.Skipped.at("rsp operand that cannot move and has no room for a stub") == 2, "an operand at rsp followed by an exit or a branch target was not refused for want of room");
    require(report.Skipped.at("prologue shorter than a jump") == 1, "the leaf with a short prologue was not reported with its reason");
}

void skipsLeavesThatCopyRsp() {
    std::vector<std::uint8_t> withCmov = StoreBelowRsp;
    append(withCmov, {0x48, 0x0f, 0x45, 0xc4});
    std::vector<std::uint8_t> withShlx = StoreBelowRsp;
    append(withShlx, {0xc4, 0xe2, 0xf1, 0xf7, 0xc4});
    const auto input = image({framelessLeaf(FramelessPrologue, withCmov, FramelessExit), framelessLeaf(FramelessPrologue, withShlx, FramelessExit)});
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(input.bytes, input.segments, input.functions, {}, report, displacements);
    require(sites.empty() && displacements.empty() && report.Skipped.at("other use of rsp") == 2, "a leaf that copies rsp into another register was framed");
}

void tellsPopsWithoutRexFromXop() {
    std::vector<std::uint8_t> popBelowRsp = StoreBelowPushes;
    append(popBelowRsp, {0x52, 0x8f, 0x44, 0x24, 0xf0});
    std::vector<std::uint8_t> popRspWithFramePointer = StoreBelowPushes;
    append(popRspWithFramePointer, {0x52, 0x8f, 0xc4});
    std::vector<std::uint8_t> popRsp = StoreBelowRsp;
    append(popRsp, {0x8f, 0xc4});
    const auto pops = image({leaf(popBelowRsp), leaf(popRspWithFramePointer), framelessLeaf(FramelessPrologue, popRsp, FramelessExit)});
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(pops.bytes, pops.segments, pops.functions, {}, report, displacements);
    require(sites.empty() && displacements.empty() && report.RedZoneFunctions == 3, "a pop without REX to an rsp operand or into rsp, read as an XOP prefix, was framed");
    require(report.Skipped.at("other use of rsp") == 3, "a pop without REX to an rsp operand or into rsp was not reported as another use of rsp");

    std::vector<std::uint8_t> xopAtR12 = StoreBelowRsp;
    append(xopAtR12, {0x8f, 0xc8, 0x78, 0xc0, 0x04, 0x24, 0x01});
    std::vector<std::uint8_t> xopBelowRsp = StoreBelowRsp;
    append(xopBelowRsp, {0x8f, 0xe8, 0x78, 0xc0, 0x44, 0x24, 0xf0, 0x01});
    const auto xops = image({framelessLeaf(FramelessPrologue, xopAtR12, FramelessExit), framelessLeaf(FramelessPrologue, xopBelowRsp, FramelessExit)});
    Codegen::RedZoneFrameReport xopReport;
    std::vector<Codegen::RedZoneDisplacement> xopDisplacements;
    const auto xopSites = Codegen::FrameRedZoneFunctions(xops.bytes, xops.segments, xops.functions, {}, xopReport, xopDisplacements);
    require(xopReport.Framed == 2 && xopReport.MovedOperands == 3 && xopReport.OperandStubs == 0 && xopSites.size() == 4, "an XOP operand was not read with the base register extension of its XOP prefix");
}

const std::vector<std::uint8_t> InflatePrologue{0x55, 0x41, 0x57, 0x41, 0x56, 0x41, 0x55, 0x41, 0x54, 0x53, 0x48, 0x83, 0xec, 0x40};
const std::vector<std::uint8_t> InflatePops{0x5b, 0x41, 0x5c, 0x41, 0x5d, 0x41, 0x5e, 0x41, 0x5f, 0x5d, 0xc3};
const std::vector<std::uint8_t> InflateFree{0x48, 0x83, 0xc4, 0x40};

std::vector<std::uint8_t> inflateExit() {
    std::vector<std::uint8_t> bytes = InflateFree;
    append(bytes, InflatePops);
    return bytes;
}

std::vector<std::uint8_t> inflateBody() {
    return {
        0x48, 0x89, 0x7c, 0x24, 0xe0,
        0x48, 0x89, 0x7c, 0x24, 0x00,
        0x48, 0x89, 0x74, 0x24, 0x08,
        0x48, 0x89, 0x54, 0x24, 0x38,
        0xb9, 0x04, 0x00, 0x00, 0x00,
        0x31, 0xc0,
        0x48, 0x03, 0x44, 0x24, 0x08,
        0x48, 0x03, 0x44, 0x24, 0xe0,
        0x48, 0x03, 0x44, 0x24, 0x00,
        0xff, 0xc9,
        0x75, 0xed,
        0x48, 0x8b, 0x54, 0x24, 0x38,
    };
}

std::vector<std::uint8_t> inflateCompareBody(const std::vector<std::uint8_t>& prior, const std::uint8_t back) {
    std::vector<std::uint8_t> bytes{
        0x48, 0x89, 0x7c, 0x24, 0xe0,
        0x48, 0x89, 0x7c, 0x24, 0x00,
        0xb9, 0x04, 0x00, 0x00, 0x00,
        0x31, 0xc0,
        0x48, 0x03, 0x44, 0x24, 0xe0,
    };
    append(bytes, prior);
    append(bytes, {0x48, 0x39, 0x3c, 0x24, 0x75, 0x03, 0x48, 0x01, 0xf0, 0xff, 0xc9, 0x75, back});
    return bytes;
}

const std::vector<std::uint8_t> AddRdiToR8{0x49, 0x01, 0xf8};
const std::vector<std::uint8_t> CompareRdiAtRspShifted{0x48, 0x39, 0x7c, 0x24, 0x20};

void stubsShortOperandsAtRspBeforeABranchWithTheInstructionsBeforeThem() {
    const auto withAdd = inflateCompareBody(AddRdiToR8, 0xeb);
    std::vector<std::uint8_t> withLoad{
        0x48, 0x89, 0x7c, 0x24, 0xe0,
        0x48, 0x89, 0x7c, 0x24, 0x00,
        0xb9, 0x04, 0x00, 0x00, 0x00,
        0x31, 0xc0,
        0x48, 0x03, 0x44, 0x24, 0xe0,
        0x48, 0x39, 0x3c, 0x24, 0x75, 0x03, 0x48, 0x01, 0xf0, 0xff, 0xc9, 0x75, 0xee,
    };
    const auto input = image({framelessLeaf(InflatePrologue, withAdd, inflateExit()), framelessLeaf(InflatePrologue, withLoad, inflateExit())});
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(input.bytes, input.segments, input.functions, {}, report, displacements);
    require(report.Framed == 2 && report.OperandStubs == 2 && report.StubbedOperands == 3 && report.MovedOperands == 5 && sites.size() == 6 && report.Skipped.empty(), "inflate_fast's 4-byte cmp at rsp between an instruction and a branch was not framed with a stub over the instruction before it");
    const auto add = input.functions[0].Begin + InflatePrologue.size() + 22;
    require(sites[2].Address == add && sites[2].Length == AddRdiToR8.size() + 4, "the stub site does not start at the add before the cmp at rsp");
    std::vector<std::uint8_t> addBody = AddRdiToR8;
    append(addBody, CompareRdiAtRspShifted);
    require(sites[2].Body == stubBody(addBody), "the add was not copied ahead of the cmp re-encoded with a displacement of the shift");
    const auto load = input.functions[1].Begin + InflatePrologue.size() + 17;
    require(sites[5].Address == load && sites[5].Length == 9, "the stub site does not start at the load before the cmp at rsp");
    std::vector<std::uint8_t> loadBody{0x48, 0x03, 0x44, 0x24, 0x00};
    append(loadBody, CompareRdiAtRspShifted);
    require(sites[5].Body == stubBody(loadBody), "the load before the cmp, itself an rsp operand, was not moved by the shift in the stub");
    for (const auto& displacement : displacements) {
        const auto at = Base + displacement.Offset;
        require((at < sites[2].Address || at >= sites[2].Address + sites[2].Length) && (at < sites[5].Address || at >= sites[5].Address + sites[5].Length), "an rsp operand inside a stub site was also moved in place");
    }
}

void skipsShortOperandsAtRspWhoseWidenedStubCoversABranchTarget() {
    std::vector<std::uint8_t> body{
        0x48, 0x89, 0x7c, 0x24, 0xe0,
        0x48, 0x89, 0x7c, 0x24, 0x00,
        0xb9, 0x04, 0x00, 0x00, 0x00,
        0x31, 0xc0,
    };
    append(body, AddRdiToR8);
    append(body, {0x48, 0x39, 0x3c, 0x24, 0x75, 0x03, 0x48, 0x01, 0xf0, 0xff, 0xc9, 0x75, 0xf3});
    const auto input = image({framelessLeaf(InflatePrologue, body, inflateExit())});
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(input.bytes, input.segments, input.functions, {}, report, displacements);
    require(sites.empty() && displacements.empty() && report.RedZoneFunctions == 1 && report.Skipped.at("rsp operand that cannot move and has no room for a stub") == 1, "a cmp at rsp whose loop branches back to it was framed with a stub that starts at the add before it");
}

void framesLeavesThatAllocateBelowTheirPushes() {
    const auto body = inflateBody();
    const auto input = image({framelessLeaf(InflatePrologue, body, inflateExit())});
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(input.bytes, input.segments, input.functions, {}, report, displacements);
    require(report.RedZoneFunctions == 1 && report.Framed == 1 && report.FramedWithoutFramePointer == 1 && report.Exits == 1 && sites.size() == 2, "a leaf that allocates with sub after its pushes and spills below its allocation was not framed");
    const auto& entry = sites[0];
    require(entry.Address == Base + 0x40 && entry.OriginalBytes == InflatePrologue, "the prologue site does not cover the pushes and the sub");
    std::vector<std::uint8_t> entryBody = InflatePrologue;
    append(entryBody, {0x48, 0x8d, 0x64, 0x24, 0xe0, 0xe9, 0, 0, 0, 0});
    require(entry.Body == entryBody, "the prologue stub does not lower rsp by 0x20 after the sub");
    const auto& exit = sites[1];
    require(exit.Address == Base + 0x40 + InflatePrologue.size() + body.size() && exit.OriginalBytes == inflateExit(), "the exit site does not cover the add, the pops and the ret");
    std::vector<std::uint8_t> exitBody{0x48, 0x8d, 0xa4, 0x24, 0x20, 0, 0, 0};
    append(exitBody, inflateExit());
    append(exitBody, {0xe9, 0, 0, 0, 0});
    require(exit.Body == exitBody, "the exit stub does not raise rsp by 0x20 before the add");
    const std::vector<std::uint8_t> moved{0x00, 0x20, 0x28, 0x58, 0x28, 0x00, 0x20, 0x58};
    require(displacements.size() == moved.size() && report.MovedOperands == moved.size(), "not every rsp operand of the allocating leaf was moved");
    for (std::size_t index = 0; index < moved.size(); ++index)
        require(displacements[index].Bytes == std::vector<std::uint8_t>{moved[index]}, "an rsp operand of the allocating leaf was not moved by 0x20 from the post-sub rsp");
    require(displacements[0].Offset == 0x40 + InflatePrologue.size() + 4, "the spill below the allocation was not the first operand moved");
}

void framesLeavesThatAllocateWithLeaOrALargeSub() {
    const std::vector<std::uint8_t> leaPrologue{0x53, 0x41, 0x56, 0x48, 0x8d, 0x64, 0x24, 0xc0};
    const std::vector<std::uint8_t> leaExit{0x48, 0x8d, 0x64, 0x24, 0x40, 0x41, 0x5e, 0x5b, 0xc3};
    const std::vector<std::uint8_t> largePrologue{0x53, 0x48, 0x81, 0xec, 0x00, 0x01, 0x00, 0x00};
    const std::vector<std::uint8_t> largeExit{0x48, 0x81, 0xc4, 0x00, 0x01, 0x00, 0x00, 0x5b, 0xc3};
    std::vector<std::uint8_t> largeBody = StoreBelowRsp;
    append(largeBody, {0x48, 0x8b, 0x84, 0x24, 0x80, 0x00, 0x00, 0x00});
    const auto input = image({framelessLeaf(leaPrologue, StoreBelowRsp, leaExit), framelessLeaf(largePrologue, largeBody, largeExit)});
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(input.bytes, input.segments, input.functions, {}, report, displacements);
    require(report.Framed == 2 && sites.size() == 4 && displacements.size() == 3, "leaves that allocate with lea or a 32-bit sub were not framed");
    require(sites[0].OriginalBytes == leaPrologue && sites[1].OriginalBytes == leaExit, "the lea allocation and free are not part of the prologue and exit sites");
    require(sites[2].OriginalBytes == largePrologue && sites[3].OriginalBytes == largeExit, "the 32-bit sub and add are not part of the prologue and exit sites");
    require(displacements[2].Bytes == std::vector<std::uint8_t>{0x90, 0x00, 0x00, 0x00}, "the 32-bit displacement into the allocation was not moved by the shift");
}

void skipsAllocatingLeavesWithoutAMatchingFree() {
    const std::vector<std::uint8_t> otherFree{0x48, 0x83, 0xc4, 0x30};
    std::vector<std::uint8_t> wrongFree = otherFree;
    append(wrongFree, InflatePops);
    std::vector<std::uint8_t> moreRsp = StoreBelowRsp;
    append(moreRsp, {0x48, 0x83, 0xec, 0x08});
    const auto input = image({framelessLeaf(InflatePrologue, StoreBelowRsp, InflatePops), framelessLeaf(InflatePrologue, StoreBelowRsp, wrongFree), framelessLeaf(InflatePrologue, moreRsp, inflateExit())});
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(input.bytes, input.segments, input.functions, {}, report, displacements);
    require(sites.empty() && displacements.empty() && report.RedZoneFunctions == 3, "an allocating leaf without a matching free, or with more rsp arithmetic, was framed");
    require(report.Skipped.at("exit does not free the prologue's allocation") == 1, "an exit that does not free the allocation was not reported with its own reason");
    require(report.Skipped.at("other use of rsp") == 2, "an add of another size before the pops, or more rsp arithmetic in the body, was not reported as another use of rsp");
}

Image calledThenUncalled(const std::vector<std::uint8_t>& called, const std::vector<std::uint8_t>& uncalled) {
    Image result;
    result.bytes = callTo(0, 0x40);
    result.bytes.push_back(0xc3);
    result.bytes = padTo(result.bytes, 0x40);
    append(result.bytes, called);
    result.bytes.resize(0x60, 0x90);
    append(result.bytes, uncalled);
    result.bytes = padTo(result.bytes, 0x80);
    result.segments.push_back({1, 5, 0, Base, Base, result.bytes.size(), result.bytes.size(), 16});
    result.functions.push_back({Base + 0x40, Base + 0x40 + called.size()});
    result.functions.push_back({Base + 0x60, Base + 0x60 + uncalled.size()});
    return result;
}

void framesEachFunctionFromItsOwnFde() {
    std::vector<std::uint8_t> body = StoreBelowRsp;
    append(body, LoadBelowRsp);
    const auto input = calledThenUncalled(framelessLeaf(FramelessPrologue, body, FramelessExit), framelessLeaf(FramelessPrologue, StoreBelowRsp, FramelessExit));
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(input.bytes, input.segments, input.functions, {}, report, displacements);
    require(report.Functions == 2 && report.Framed == 2 && report.Exits == 2 && sites.size() == 4 && displacements.size() == 3, "two adjacent functions were not framed each by its own FDE");
    require(sites[0].Address < Base + 0x60 && sites[1].Address + sites[1].Length <= Base + 0x60 && displacements[0].Offset < 0x60 && displacements[1].Offset < 0x60, "the first function's frame runs past its FDE end");
    require(sites[2].Address == Base + 0x60 && sites[2].OriginalBytes == FramelessPrologue && displacements[2].Offset > 0x60, "the function reached without a call was not framed from its own FDE start");
}

void treatsATailCallPastTheFdeEndAsLeaving() {
    std::vector<std::uint8_t> called = FramelessPrologue;
    append(called, StoreBelowRsp);
    append(called, {0x85, 0xff, 0x75, 0x06});
    append(called, FramelessExit);
    append(called, {0x41, 0x5f, 0x41, 0x5e, 0x5b, 0xe9, 0x02, 0x00, 0x00, 0x00});
    const auto input = calledThenUncalled(called, framelessLeaf(FramelessPrologue, StoreBelowRsp, FramelessExit));
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(input.bytes, input.segments, input.functions, {}, report, displacements);
    require(report.Skipped.at("branch leaves the function") == 1, "the tail call past the FDE end was not reported as leaving the function");
    require(report.Framed == 1 && sites.size() == 2 && sites[0].Address == Base + 0x60, "the tail-called function was not framed from its own FDE start");
}

void framesALeafAfterAJumpWithoutInt3FromItsOwnFde() {
    std::vector<std::uint8_t> first = FramelessPrologue;
    append(first, StoreBelowRsp);
    append(first, {0x31, 0xc0, 0xeb, 0x06});
    append(first, FramelessExit);
    append(first, {0x48, 0x3b, 0x44, 0x24, 0xf0, 0x73, 0xf3, 0x48, 0xff, 0xc0, 0xeb, 0xf4});
    require(first.size() == 0x20, "the first leaf must end where the second starts");
    std::vector<std::uint8_t> second = StoreBelowRsp;
    append(second, LoadBelowRsp);
    const auto input = calledThenUncalled(first, framelessLeaf(FramelessPrologue, second, FramelessExit));
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(input.bytes, input.segments, input.functions, {}, report, displacements);
    require(report.Framed == 2 && report.Exits == 2 && sites.size() == 4 && displacements.size() == 4, "a leaf right after a jmp without int3 padding was not framed on its own");
    require(sites[1].Address + sites[1].Length <= Base + 0x60 && displacements[1].Offset < 0x60, "the leaf ending in a jmp took the next leaf into its body");
    require(sites[2].Address == Base + 0x60 && sites[2].OriginalBytes == FramelessPrologue && displacements[2].Offset > 0x60, "the leaf after the jmp was not framed from its own FDE start");
}

void keepsColdBlocksAfterARetInsideTheFde() {
    std::vector<std::uint8_t> leaf = FramelessPrologue;
    append(leaf, StoreBelowRsp);
    append(leaf, {0x85, 0xff, 0x74, 0x12});
    append(leaf, LoadBelowRsp);
    append(leaf, FramelessExit);
    append(leaf, {0x0f, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00});
    append(leaf, {0x48, 0x8b, 0x54, 0x24, 0xf0});
    append(leaf, FramelessExit);
    const auto input = image({leaf});
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(input.bytes, input.segments, input.functions, {}, report, displacements);
    require(report.Framed == 1 && report.Exits == 2 && sites.size() == 3, "the exit of a cold block after the ret inside the FDE was not framed");
    require(displacements.size() == 3 && displacements[2].Offset == 0x40 + 0x20 + 4 && displacements[2].Bytes == std::vector<std::uint8_t>{0x00}, "the rsp operand of a cold block after the ret inside the FDE was not moved");
}

void keepsLoopsAlignedAfterAJump() {
    std::vector<std::uint8_t> loop = FramelessPrologue;
    append(loop, StoreBelowRsp);
    append(loop, {0x31, 0xc0, 0xeb, 0x04, 0x66, 0x90, 0xff, 0xc0, 0x3b, 0x44, 0x24, 0xf0, 0x72, 0xf8});
    append(loop, FramelessExit);
    const auto input = image({loop});
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(input.bytes, input.segments, input.functions, {}, report, displacements);
    require(report.Framed == 1 && sites.size() == 2 && displacements.size() == 2, "a leaf whose loop header is aligned after a jump was not framed");
}

void leavesOtherFunctions() {
    std::vector<std::uint8_t> nonLeaf = Prologue;
    append(nonLeaf, StoreBelowPushes);
    append(nonLeaf, {0xff, 0xd0});
    append(nonLeaf, Exit);
    const auto input = image({leaf(LoadAbovePushes), nonLeaf});
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(input.bytes, input.segments, input.functions, {}, report, displacements);
    require(sites.empty() && report.RedZoneFunctions == 0, "a leaf without red-zone use or a function that calls was framed");
}

void skipsSitesTakenByOtherStubs() {
    const auto input = image({leaf(StoreBelowPushes)});
    Codegen::TrampolineSite other{};
    other.Address = Base + 0x40;
    other.Length = 5;
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(input.bytes, input.segments, input.functions, {other}, report, displacements);
    require(sites.empty() && report.Skipped.at("overlaps another stub") == 1, "a site over another stub was framed");
}

void framesAPointerOnlyFunctionByItsOwnBounds() {
    const auto first = leaf(StoreBelowPushes);
    std::vector<std::uint8_t> secondBody = StoreBelowPushes;
    append(secondBody, LoadBelowPushes);
    const auto second = leaf(secondBody);
    std::vector<std::uint8_t> bytes = callTo(0, 0x40);
    bytes.push_back(0xc3);
    bytes = padTo(bytes, 0x40);
    append(bytes, first);
    const auto secondStart = bytes.size();
    append(bytes, second);
    bytes = padTo(bytes, (bytes.size() + 15) / 16 * 16 + 16);
    const std::vector<Domain::ProgramHeader> segments{{1, 5, 0, Base, Base, bytes.size(), bytes.size(), 16}};
    const std::vector<Codegen::RedZoneFunction> functions{{Base + 0x40, Base + 0x40 + first.size()}, {Base + secondStart, Base + secondStart + second.size()}};
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(bytes, segments, functions, {}, report, displacements);
    require(report.Functions == 2 && report.Framed == 2 && report.Exits == 2, "a function reached only through a pointer was not framed by its own bounds");
    require(sites.size() == 4, "two framed leaves need two prologue and two exit sites");
    require(sites[2].Address == Base + secondStart && sites[2].OriginalBytes == Prologue, "the second function's prologue site is not at its own start");
    require(sites[1].Address + sites[1].Length == Base + 0x40 + first.size(), "the first function's exit runs past its end");
}

void leavesCodeWithoutAFunctionRange() {
    const auto input = image({leaf(StoreBelowPushes)});
    Codegen::RedZoneFrameReport report;
    std::vector<Codegen::RedZoneDisplacement> displacements;
    const auto sites = Codegen::FrameRedZoneFunctions(input.bytes, input.segments, {}, {}, report, displacements);
    require(sites.empty() && report.Functions == 0, "code without a function range was framed");
}

}

int main() {
    try {
        framesRedZoneLeaf();
        leavesOtherFunctions();
        skipsSitesTakenByOtherStubs();
        framesLeafWithoutFramePointer();
        shiftsByTheDeepestOperandBelowRsp();
        movesPositiveOperandsThatFitAfterTheShift();
        shiftsByTheWholeRedZoneForAddressesBelowRsp();
        shiftsByTheWholeRedZoneForIndexedOperands();
        framesLeavesThatAllocateBelowTheirPushes();
        framesLeavesThatAllocateWithLeaOrALargeSub();
        skipsAllocatingLeavesWithoutAMatchingFree();
        stubsOperandsWhoseDisplacementOverflows();
        stubsOperandsAtRspWithTheInstructionsAfterThem();
        stubsVexOperandsAtRsp();
        stubsOperandsAtRspWithTheInstructionsBeforeThem();
        stubsShortOperandsAtRspBeforeABranchWithTheInstructionsBeforeThem();
        skipsShortOperandsAtRspWhoseWidenedStubCoversABranchTarget();
        skipsLeavesWithOperandsThatCannotMove();
        skipsLeavesThatCopyRsp();
        framesEachFunctionFromItsOwnFde();
        treatsATailCallPastTheFdeEndAsLeaving();
        framesALeafAfterAJumpWithoutInt3FromItsOwnFde();
        keepsColdBlocksAfterARetInsideTheFde();
        keepsLoopsAlignedAfterAJump();
        framesAPointerOnlyFunctionByItsOwnBounds();
        leavesCodeWithoutAFunctionRange();
        tellsPopsWithoutRexFromXop();
        std::cout << "red-zone framer tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "red-zone framer tests failed: " << error.what() << '\n';
        return 1;
    }
}
