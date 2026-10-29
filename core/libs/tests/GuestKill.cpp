#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
using Handler = void (APS5_VABI *)(int);

volatile std::sig_atomic_t received = 0;
void APS5_VABI Callback(int value) { received = value; }
static void Require(bool value) { if (!value) std::abort(); }
static void Fails(int pid, int guest, int error) {
    *__error_nid_postfix() = 0;
    Require(kill_nid_postfix(pid, guest) == -1 && *__error_nid_postfix() == error);
}
static void KillpgFails(int pgrp, int guest, int error) {
    *__error_nid_postfix() = 0;
    Require(killpg_nid_postfix(pgrp, guest) == -1 && *__error_nid_postfix() == error);
}
int main() {
    const int self = getpid_nid_postfix();
    const auto invalid = reinterpret_cast<Handler>(static_cast<std::uintptr_t>(-1));
    const auto ignore = reinterpret_cast<Handler>(std::uintptr_t{1});
    for (int target : {self, 0, -self}) Require(kill_nid_postfix(target, 0) == 0);
    for (int target : {self, 0}) Require(killpg_nid_postfix(target, 0) == 0);
    for (int target : {self + 1, -1, -(self + 1)}) {
        Fails(target, 0, 3);
        Fails(target, 15, 3);
    }
    for (int target : {self + 1, self + 1000}) {
        KillpgFails(target, 0, 3);
        KillpgFails(target, 15, 3);
    }
    for (int target : {-1, -self}) {
        KillpgFails(target, 0, 22);
        KillpgFails(target, 15, 22);
    }
    for (int guest : {-1, 129}) {
        Fails(self, guest, 22);
        Fails(self + 1, guest, 22);
        KillpgFails(self, guest, 22);
        KillpgFails(self + 1, guest, 22);
    }
    Require(signal_nid_postfix(15, Callback) != invalid);
    for (int target : {self, 0, -self}) {
        received = 0;
        Require(kill_nid_postfix(target, 0) == 0 && received == 0);
        Require(kill_nid_postfix(target, 15) == 0 && received == 15);
    }
    for (int target : {self, 0}) {
        received = 0;
        Require(killpg_nid_postfix(target, 0) == 0 && received == 0);
        Require(killpg_nid_postfix(target, 15) == 0 && received == 15);
    }
    received = 0;
    Fails(self + 1, 15, 3);
    KillpgFails(self + 1, 15, 3);
    Require(received == 0);
    Require(signal_nid_postfix(15, ignore) == Callback);
    Require(kill_nid_postfix(self, 15) == 0 && received == 0);
    Require(killpg_nid_postfix(self, 15) == 0 && received == 0);
    Require(signal_nid_postfix(15, nullptr) == ignore);
    Fails(self, 100, 22);
    KillpgFails(self, 100, 22);
}
