#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

static void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

static constexpr int GuestSigkill = 9;
static constexpr int GuestSigstop = 17;
static constexpr int GuestSigusr = 30;
static constexpr int GuestEinval = 22;

static std::uint32_t BlockedBit(int guest) {
    GuestSignalSet current{};
    Require(sigprocmask_nid_postfix(1, nullptr, &current) == 0);
    const int index = (guest - 1) >> 5;
    return (current.bits[index] >> ((guest - 1) & 31)) & 1u;
}

static void TestInvalidSignalsRejected() {
    const int invalid[] = {0, -1, 129};
    for (int sig : invalid) {
        *__error_nid_postfix() = 0;
        Require(sighold_nid_postfix(sig) == -1);
        Require(*__error_nid_postfix() == GuestEinval);

        *__error_nid_postfix() = 0;
        Require(sigrelse_nid_postfix(sig) == -1);
        Require(*__error_nid_postfix() == GuestEinval);

        *__error_nid_postfix() = 0;
        Require(sigignore_nid_postfix(sig) == -1);
        Require(*__error_nid_postfix() == GuestEinval);
    }
}

static void TestSigholdBlocksAndSigrelseUnblocks() {
    Require(BlockedBit(GuestSigusr) == 0);
    Require(sighold_nid_postfix(GuestSigusr) == 0);
    Require(BlockedBit(GuestSigusr) == 1);

    Require(sighold_nid_postfix(GuestSigusr) == 0);
    Require(BlockedBit(GuestSigusr) == 1);

    Require(sigrelse_nid_postfix(GuestSigusr) == 0);
    Require(BlockedBit(GuestSigusr) == 0);

    Require(sigrelse_nid_postfix(GuestSigusr) == 0);
    Require(BlockedBit(GuestSigusr) == 0);
}

static void TestSigholdLeavesOtherSignalsAlone() {
    Require(sighold_nid_postfix(GuestSigusr) == 0);
    Require(BlockedBit(GuestSigusr + 1) == 0);
    Require(sigrelse_nid_postfix(GuestSigusr) == 0);
    Require(BlockedBit(GuestSigusr) == 0);
}

static void TestUnblockableSignalsStayUnblocked() {
    Require(sighold_nid_postfix(GuestSigkill) == 0);
    Require(BlockedBit(GuestSigkill) == 0);
    Require(sighold_nid_postfix(GuestSigstop) == 0);
    Require(BlockedBit(GuestSigstop) == 0);
}

static void TestSigignoreSetsIgnoreDisposition() {
    GuestSigaction previous{};
    Require(sigignore_nid_postfix(GuestSigusr) == 0);
    Require(sigaction_nid_postfix(GuestSigusr, nullptr, &previous) == 0);
    Require(previous.handler == 1);

    GuestSigaction restore{};
    Require(sigaction_nid_postfix(GuestSigusr, &restore, nullptr) == 0);
    Require(sigaction_nid_postfix(GuestSigusr, nullptr, &previous) == 0);
    Require(previous.handler == 0);
}

static void TestSigignoreRejectsKillAndStop() {
    *__error_nid_postfix() = 0;
    Require(sigignore_nid_postfix(GuestSigkill) == -1);
    Require(*__error_nid_postfix() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(sigignore_nid_postfix(GuestSigstop) == -1);
    Require(*__error_nid_postfix() == GuestEinval);
}

static void TestBitPositionsAndOtherSignals() {
    Require(sighold_nid_postfix(128) == 0);
    GuestSignalSet mask{};
    Require(sigprocmask_nid_postfix(1, nullptr, &mask) == 0);
    Require((mask.bits[3] & (1u << 31)) != 0);
    Require(sigrelse_nid_postfix(128) == 0);

    Require(sighold_nid_postfix(33) == 0);
    Require(sigprocmask_nid_postfix(1, nullptr, &mask) == 0);
    Require((mask.bits[1] & 1u) != 0 && (mask.bits[0] & 1u) == 0);
    Require(sigrelse_nid_postfix(33) == 0);

    GuestSignalSet other{};
    other.bits[0] = 1u << 30;
    Require(sigprocmask_nid_postfix(1, &other, nullptr) == 0);
    Require(sighold_nid_postfix(GuestSigusr) == 0);
    Require(sigprocmask_nid_postfix(1, nullptr, &mask) == 0);
    Require((mask.bits[0] & (1u << 30)) != 0 && (mask.bits[0] & (1u << 29)) != 0);
    Require(sigrelse_nid_postfix(GuestSigusr) == 0);
    GuestSignalSet clear{};
    Require(sigprocmask_nid_postfix(3, &clear, nullptr) == 0);
}

int main() {
    TestInvalidSignalsRejected();
    TestSigholdBlocksAndSigrelseUnblocks();
    TestSigholdLeavesOtherSignalsAlone();
    TestUnblockableSignalsStayUnblocked();
    TestSigignoreSetsIgnoreDisposition();
    TestSigignoreRejectsKillAndStop();
    TestBitPositionsAndOtherSignals();
    return 0;
}
