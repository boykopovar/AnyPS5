#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstddef>
#include <cstring>
#include <exception>
#include <string>
#include <thread>

struct GuestStack {
    void* sp;
    std::size_t size;
    int flags;
};

extern "C" {
int APS5_VABI sigaltstack_nid_postfix(const GuestStack*, GuestStack*);
int* APS5_VABI __error_nid_postfix();
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int ssOnstack = 1;
constexpr int ssDisable = 4;
constexpr int einval = 22;
constexpr int enomem = 12;
constexpr std::size_t areaSize = 4096;

struct Area {
    alignas(16) char bytes[areaSize];
};

void RunOnFreshThread(void (*body)()) {
    std::exception_ptr error;
    std::thread([body, &error] {
        try {
            body();
        } catch (...) {
            error = std::current_exception();
        }
    }).join();
    if (error) std::rethrow_exception(error);
}

GuestStack Query() {
    GuestStack current;
    std::memset(&current, 0xa5, sizeof(current));
    RequireEqual(sigaltstack_nid_postfix(nullptr, &current), 0, "query");
    return current;
}

void RequireStack(const GuestStack& stack, const void* sp, std::size_t size, int flags, const std::string& message) {
    Require(stack.sp == sp, message + " stack pointer");
    RequireEqual(stack.size, size, message + " size");
    RequireEqual(stack.flags, flags, message + " flags");
}

void RequireRejected(const GuestStack& stack, int error, const std::string& message) {
    GuestStack untouched;
    std::memset(&untouched, 0xa5, sizeof(untouched));
    GuestStack previous = untouched;
    *__error_nid_postfix() = 0;
    RequireEqual(sigaltstack_nid_postfix(&stack, &previous), -1, message + " result");
    RequireEqual(*__error_nid_postfix(), error, message + " errno");
    Require(std::memcmp(&previous, &untouched, sizeof(previous)) == 0, message + " leaves the old stack output untouched");
}

void Install(const GuestStack& stack, GuestStack& previous, const std::string& message) {
    RequireEqual(sigaltstack_nid_postfix(&stack, &previous), 0, message);
}

const Case freshThread{"Sigaltstack_FreshThread_ReportsDisabledEmptyStack", [] {
    RunOnFreshThread([] {
        RequireStack(Query(), nullptr, 0, ssDisable, "fresh thread");
        RequireEqual(sigaltstack_nid_postfix(nullptr, nullptr), 0, "query without output");
    });
}};

const Case invalidFlags{"Sigaltstack_InvalidFlags_FailsWithEinvalAndKeepsState", [] {
    RunOnFreshThread([] {
        Area area;
        RequireRejected({area.bytes, areaSize, ssOnstack}, einval, "SS_ONSTACK");
        RequireRejected({area.bytes, areaSize, ssDisable | 2}, einval, "SS_DISABLE with unknown flag");
        RequireStack(Query(), nullptr, 0, ssDisable, "state after rejections");
    });
}};

const Case belowMinimum{"Sigaltstack_StackBelowMinimumSize_FailsWithEnomemAndKeepsState", [] {
    RunOnFreshThread([] {
        Area area;
        RequireRejected({area.bytes, 2047, 0}, enomem, "2047 byte stack");
        RequireStack(Query(), nullptr, 0, ssDisable, "state after rejection");
    });
}};

const Case minimum{"Sigaltstack_MinimumSizeStack_InstallsAndReportsDisabledPrevious", [] {
    RunOnFreshThread([] {
        Area area;
        GuestStack previous{};
        Install({area.bytes, 2048, 0}, previous, "install 2048 byte stack");
        Require(previous.sp == nullptr, "previous stack pointer");
        RequireEqual(previous.flags, ssDisable, "previous flags");
    });
}};

const Case replace{"Sigaltstack_ReplaceInstalledStack_ReportsPreviousAndQueriesNew", [] {
    RunOnFreshThread([] {
        Area area;
        GuestStack previous{};
        Install({area.bytes, 2048, 0}, previous, "install minimum stack");
        Install({area.bytes, areaSize, 0}, previous, "install full stack");
        RequireStack(previous, area.bytes, 2048, 0, "previous");
        RequireStack(Query(), area.bytes, areaSize, 0, "current");
    });
}};

const Case otherThread{"Sigaltstack_InstalledOnOneThread_IsNotVisibleOnAnother", [] {
    RunOnFreshThread([] {
        Area area;
        GuestStack previous{};
        Install({area.bytes, areaSize, 0}, previous, "install stack");
        RunOnFreshThread([] { RequireStack(Query(), nullptr, 0, ssDisable, "other thread"); });
    });
}};

const Case disable{"Sigaltstack_Disable_KeepsStackAndSetsDisabledFlag", [] {
    RunOnFreshThread([] {
        Area area;
        GuestStack previous{};
        Install({area.bytes, areaSize, 0}, previous, "install stack");
        Install({nullptr, 0, ssDisable}, previous, "disable");
        RequireStack(previous, area.bytes, areaSize, 0, "previous");
        RequireStack(Query(), area.bytes, areaSize, ssDisable, "current");
    });
}};

const Case smallAfterDisable{"Sigaltstack_SmallStackAfterDisable_FailsWithEnomem", [] {
    RunOnFreshThread([] {
        Area area;
        GuestStack previous{};
        Install({area.bytes, areaSize, 0}, previous, "install stack");
        Install({nullptr, 0, ssDisable}, previous, "disable");
        RequireRejected({area.bytes, 16, 0}, enomem, "16 byte stack");
    });
}};

} // namespace
