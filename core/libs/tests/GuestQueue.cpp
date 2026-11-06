#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <atomic>
#include <barrier>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "Queue check failed at line %d\n", line);
        std::abort();
    }
}
#define Check(value) Require((value), __LINE__)

struct QueueNode {
    QueueNode* q_forw{nullptr};
    QueueNode* q_back{nullptr};
    int value{0};
};

struct LargePayloadNode {
    LargePayloadNode* q_forw{nullptr};
    LargePayloadNode* q_back{nullptr};
    std::uint64_t canary1{0x0123456789ABCDEFULL};
    char tag[32]{"AnyPS5_Payload_Integrity_Check"};
    std::uint64_t canary2{0xFEDCBA9876543210ULL};
};

static void CheckLinearInitial() {
    QueueNode canary;
    QueueNode e1{&canary, &canary, 42};
    insque_nid_postfix(&e1, nullptr);
    Check(e1.q_forw == nullptr);
    Check(e1.q_back == nullptr);
    Check(e1.value == 42);
}

static void CheckLinearSequential() {
    QueueNode e1{nullptr, nullptr, 1};
    QueueNode e2{nullptr, nullptr, 2};
    QueueNode e3{nullptr, nullptr, 3};

    insque_nid_postfix(&e1, nullptr);
    Check(e1.q_forw == nullptr && e1.q_back == nullptr);

    insque_nid_postfix(&e2, &e1);
    Check(e1.q_back == nullptr);
    Check(e1.q_forw == &e2);
    Check(e2.q_back == &e1);
    Check(e2.q_forw == nullptr);

    insque_nid_postfix(&e3, &e2);
    Check(e1.q_back == nullptr);
    Check(e1.q_forw == &e2);
    Check(e2.q_back == &e1);
    Check(e2.q_forw == &e3);
    Check(e3.q_back == &e2);
    Check(e3.q_forw == nullptr);

    QueueNode* curr = &e1;
    Check(curr->value == 1);
    curr = curr->q_forw;
    Check(curr == &e2 && curr->value == 2);
    curr = curr->q_forw;
    Check(curr == &e3 && curr->value == 3);
    curr = curr->q_forw;
    Check(curr == nullptr);

    curr = &e3;
    Check(curr->value == 3);
    curr = curr->q_back;
    Check(curr == &e2 && curr->value == 2);
    curr = curr->q_back;
    Check(curr == &e1 && curr->value == 1);
    curr = curr->q_back;
    Check(curr == nullptr);
}

static void CheckLinearMiddleInsertAndRemove() {
    QueueNode e1{nullptr, nullptr, 1};
    QueueNode e2{nullptr, nullptr, 2};
    QueueNode e3{nullptr, nullptr, 3};
    QueueNode e1_5{nullptr, nullptr, 15};

    insque_nid_postfix(&e1, nullptr);
    insque_nid_postfix(&e2, &e1);
    insque_nid_postfix(&e3, &e2);

    insque_nid_postfix(&e1_5, &e1);

    Check(e1.q_back == nullptr);
    Check(e1.q_forw == &e1_5);
    Check(e1_5.q_back == &e1);
    Check(e1_5.q_forw == &e2);
    Check(e2.q_back == &e1_5);
    Check(e2.q_forw == &e3);
    Check(e3.q_back == &e2);
    Check(e3.q_forw == nullptr);

    QueueNode* curr = &e1;
    Check(curr->value == 1);
    curr = curr->q_forw;
    Check(curr == &e1_5 && curr->value == 15);
    curr = curr->q_forw;
    Check(curr == &e2 && curr->value == 2);
    curr = curr->q_forw;
    Check(curr == &e3 && curr->value == 3);
    curr = curr->q_forw;
    Check(curr == nullptr);

    curr = &e3;
    Check(curr->value == 3);
    curr = curr->q_back;
    Check(curr == &e2 && curr->value == 2);
    curr = curr->q_back;
    Check(curr == &e1_5 && curr->value == 15);
    curr = curr->q_back;
    Check(curr == &e1 && curr->value == 1);
    curr = curr->q_back;
    Check(curr == nullptr);

    remque_nid_postfix(&e1_5);

    Check(e1.q_back == nullptr);
    Check(e1.q_forw == &e2);
    Check(e2.q_back == &e1);
    Check(e2.q_forw == &e3);
    Check(e3.q_back == &e2);
    Check(e3.q_forw == nullptr);

    curr = &e1;
    Check(curr->value == 1);
    curr = curr->q_forw;
    Check(curr == &e2 && curr->value == 2);
    curr = curr->q_forw;
    Check(curr == &e3 && curr->value == 3);
    curr = curr->q_forw;
    Check(curr == nullptr);

    curr = &e3;
    Check(curr->value == 3);
    curr = curr->q_back;
    Check(curr == &e2 && curr->value == 2);
    curr = curr->q_back;
    Check(curr == &e1 && curr->value == 1);
    curr = curr->q_back;
    Check(curr == nullptr);
}

static void CheckLinearHeadAndTailRemoval() {
    QueueNode e1{nullptr, nullptr, 1};
    QueueNode e2{nullptr, nullptr, 2};
    QueueNode e3{nullptr, nullptr, 3};

    insque_nid_postfix(&e1, nullptr);
    insque_nid_postfix(&e2, &e1);
    insque_nid_postfix(&e3, &e2);

    remque_nid_postfix(&e1);

    Check(e2.q_back == nullptr);
    Check(e2.q_forw == &e3);
    Check(e3.q_back == &e2);
    Check(e3.q_forw == nullptr);

    QueueNode* curr = &e2;
    Check(curr->value == 2);
    curr = curr->q_forw;
    Check(curr == &e3 && curr->value == 3);
    curr = curr->q_forw;
    Check(curr == nullptr);

    curr = &e3;
    Check(curr->value == 3);
    curr = curr->q_back;
    Check(curr == &e2 && curr->value == 2);
    curr = curr->q_back;
    Check(curr == nullptr);

    remque_nid_postfix(&e3);

    Check(e2.q_back == nullptr);
    Check(e2.q_forw == nullptr);
    Check(e2.value == 2);

    remque_nid_postfix(&e2);
}

static void CheckCircularQueue() {
    QueueNode c1{nullptr, nullptr, 100};
    QueueNode c2{nullptr, nullptr, 200};
    QueueNode c3{nullptr, nullptr, 300};
    QueueNode c4{nullptr, nullptr, 400};

    c1.q_forw = &c1;
    c1.q_back = &c1;

    insque_nid_postfix(&c2, &c1);
    Check(c1.q_forw == &c2);
    Check(c2.q_back == &c1);
    Check(c2.q_forw == &c1);
    Check(c1.q_back == &c2);

    insque_nid_postfix(&c3, &c2);
    Check(c1.q_forw == &c2);
    Check(c2.q_forw == &c3);
    Check(c3.q_forw == &c1);
    Check(c1.q_back == &c3);
    Check(c3.q_back == &c2);
    Check(c2.q_back == &c1);

    insque_nid_postfix(&c4, &c3);
    Check(c1.q_forw == &c2);
    Check(c2.q_forw == &c3);
    Check(c3.q_forw == &c4);
    Check(c4.q_forw == &c1);
    Check(c1.q_back == &c4);
    Check(c4.q_back == &c3);
    Check(c3.q_back == &c2);
    Check(c2.q_back == &c1);

    QueueNode* f = &c1;
    const int expectedFwd[] = {100, 200, 300, 400, 100, 200, 300, 400, 100, 200, 300, 400};
    for (int v : expectedFwd) {
        Check(f->value == v);
        f = f->q_forw;
    }
    Check(f == &c1);

    QueueNode* b = &c1;
    const int expectedBwd[] = {100, 400, 300, 200, 100, 400, 300, 200, 100, 400, 300, 200};
    for (int v : expectedBwd) {
        Check(b->value == v);
        b = b->q_back;
    }
    Check(b == &c1);

    remque_nid_postfix(&c2);
    Check(c1.q_forw == &c3);
    Check(c3.q_back == &c1);
    Check(c3.q_forw == &c4);
    Check(c4.q_back == &c3);
    Check(c4.q_forw == &c1);
    Check(c1.q_back == &c4);

    f = &c1;
    const int remainingFwd[] = {100, 300, 400, 100, 300, 400};
    for (int v : remainingFwd) {
        Check(f->value == v);
        f = f->q_forw;
    }
    Check(f == &c1);

    b = &c1;
    const int remainingBwd[] = {100, 400, 300, 100, 400, 300};
    for (int v : remainingBwd) {
        Check(b->value == v);
        b = b->q_back;
    }
    Check(b == &c1);

    remque_nid_postfix(&c4);
    Check(c1.q_forw == &c3);
    Check(c3.q_back == &c1);
    Check(c3.q_forw == &c1);
    Check(c1.q_back == &c3);

    remque_nid_postfix(&c3);
    Check(c1.q_forw == &c1);
    Check(c1.q_back == &c1);

    remque_nid_postfix(&c1);
    Check(c1.q_forw == &c1);
    Check(c1.q_back == &c1);
}

static void CheckNullPointers() {
    auto throws = [](auto call) {
        try { call(); } catch (const std::runtime_error&) { return true; }
        return false;
    };
    Check(throws([] { insque_nid_postfix(nullptr, nullptr); }));

    QueueNode dummy{nullptr, nullptr, 99};
    Check(throws([&] { insque_nid_postfix(nullptr, &dummy); }));
    Check(dummy.q_forw == nullptr);
    Check(dummy.q_back == nullptr);
    Check(dummy.value == 99);

    Check(throws([] { remque_nid_postfix(nullptr); }));
}

static void CheckLargeQueueSequences() {
    constexpr std::size_t kSize = 64;
    std::array<QueueNode, kSize> linearNodes;
    for (std::size_t i = 0; i < kSize; ++i) {
        linearNodes[i].q_forw = nullptr;
        linearNodes[i].q_back = nullptr;
        linearNodes[i].value = static_cast<int>(i);
    }

    insque_nid_postfix(&linearNodes[0], nullptr);
    for (std::size_t i = 1; i < kSize; ++i) {
        insque_nid_postfix(&linearNodes[i], &linearNodes[i - 1]);
    }

    QueueNode* cur = &linearNodes[0];
    for (std::size_t i = 0; i < kSize; ++i) {
        Check(cur != nullptr);
        Check(cur->value == static_cast<int>(i));
        cur = cur->q_forw;
    }
    Check(cur == nullptr);

    cur = &linearNodes[kSize - 1];
    for (std::size_t i = kSize; i > 0; --i) {
        Check(cur != nullptr);
        Check(cur->value == static_cast<int>(i - 1));
        cur = cur->q_back;
    }
    Check(cur == nullptr);

    for (std::size_t i = 2; i < kSize; i += 2) {
        remque_nid_postfix(&linearNodes[i]);
    }

    cur = &linearNodes[0];
    Check(cur->value == 0);
    cur = cur->q_forw;
    Check(cur->value == 1);
    cur = cur->q_forw;
    for (std::size_t i = 3; i < kSize; i += 2) {
        Check(cur != nullptr);
        Check(cur->value == static_cast<int>(i));
        cur = cur->q_forw;
    }
    Check(cur == nullptr);

    for (std::size_t i = 0; i < kSize; ++i) {
        if (i % 2 != 0 || i == 0) {
            remque_nid_postfix(&linearNodes[i]);
        }
    }

    std::array<QueueNode, kSize> circNodes;
    for (std::size_t i = 0; i < kSize; ++i) {
        circNodes[i].q_forw = nullptr;
        circNodes[i].q_back = nullptr;
        circNodes[i].value = static_cast<int>(i);
    }
    circNodes[0].q_forw = &circNodes[0];
    circNodes[0].q_back = &circNodes[0];

    for (std::size_t i = 1; i < kSize; ++i) {
        insque_nid_postfix(&circNodes[i], &circNodes[i - 1]);
    }

    cur = &circNodes[0];
    for (std::size_t i = 0; i < kSize; ++i) {
        Check(cur->value == static_cast<int>(i));
        cur = cur->q_forw;
    }
    Check(cur == &circNodes[0]);

    for (std::size_t i = 1; i < kSize; i += 2) {
        remque_nid_postfix(&circNodes[i]);
    }

    cur = &circNodes[0];
    for (std::size_t i = 0; i < kSize; i += 2) {
        Check(cur->value == static_cast<int>(i));
        cur = cur->q_forw;
    }
    Check(cur == &circNodes[0]);
}

static void CheckPayloadIntegrity() {
    LargePayloadNode p1{};
    LargePayloadNode p2{};
    insque_nid_postfix(&p1, nullptr);
    insque_nid_postfix(&p2, &p1);

    Check(p1.canary1 == 0x0123456789ABCDEFULL);
    Check(std::strcmp(p1.tag, "AnyPS5_Payload_Integrity_Check") == 0);
    Check(p1.canary2 == 0xFEDCBA9876543210ULL);

    Check(p2.canary1 == 0x0123456789ABCDEFULL);
    Check(std::strcmp(p2.tag, "AnyPS5_Payload_Integrity_Check") == 0);
    Check(p2.canary2 == 0xFEDCBA9876543210ULL);

    remque_nid_postfix(&p2);

    Check(p1.canary1 == 0x0123456789ABCDEFULL);
    Check(std::strcmp(p1.tag, "AnyPS5_Payload_Integrity_Check") == 0);
    Check(p1.canary2 == 0xFEDCBA9876543210ULL);

    remque_nid_postfix(&p1);

    Check(p1.canary1 == 0x0123456789ABCDEFULL);
    Check(std::strcmp(p1.tag, "AnyPS5_Payload_Integrity_Check") == 0);
    Check(p1.canary2 == 0xFEDCBA9876543210ULL);
}

static void CheckConcurrencyStress() {
    constexpr std::size_t kThreadCount = 8;
    constexpr int kIterations = 10000;
    std::barrier syncStart(static_cast<std::ptrdiff_t>(kThreadCount));
    std::atomic<bool> allPassed{true};
    std::array<std::thread, kThreadCount> workers;

    for (std::size_t t = 0; t < kThreadCount; ++t) {
        workers[t] = std::thread([&, t] {
            syncStart.arrive_and_wait();
            for (int iter = 0; iter < kIterations; ++iter) {
                QueueNode l0{nullptr, nullptr, static_cast<int>(t * 100 + 0)};
                QueueNode l1{nullptr, nullptr, static_cast<int>(t * 100 + 1)};
                QueueNode l2{nullptr, nullptr, static_cast<int>(t * 100 + 2)};
                QueueNode l3{nullptr, nullptr, static_cast<int>(t * 100 + 3)};
                QueueNode l4{nullptr, nullptr, static_cast<int>(t * 100 + 4)};

                insque_nid_postfix(&l0, nullptr);
                insque_nid_postfix(&l1, &l0);
                insque_nid_postfix(&l3, &l1);
                insque_nid_postfix(&l2, &l1);
                insque_nid_postfix(&l4, &l3);

                if (l0.q_back != nullptr || l0.q_forw != &l1) { allPassed = false; break; }
                if (l1.q_back != &l0 || l1.q_forw != &l2) { allPassed = false; break; }
                if (l2.q_back != &l1 || l2.q_forw != &l3) { allPassed = false; break; }
                if (l3.q_back != &l2 || l3.q_forw != &l4) { allPassed = false; break; }
                if (l4.q_back != &l3 || l4.q_forw != nullptr) { allPassed = false; break; }

                remque_nid_postfix(&l2);
                if (l1.q_forw != &l3 || l3.q_back != &l1) { allPassed = false; break; }

                remque_nid_postfix(&l0);
                if (l1.q_back != nullptr) { allPassed = false; break; }

                remque_nid_postfix(&l4);
                if (l3.q_forw != nullptr) { allPassed = false; break; }

                remque_nid_postfix(&l1);
                remque_nid_postfix(&l3);

                QueueNode c0{nullptr, nullptr, static_cast<int>(t * 100 + 10)};
                QueueNode c1{nullptr, nullptr, static_cast<int>(t * 100 + 11)};
                QueueNode c2{nullptr, nullptr, static_cast<int>(t * 100 + 12)};
                QueueNode c3{nullptr, nullptr, static_cast<int>(t * 100 + 13)};

                c0.q_forw = &c0;
                c0.q_back = &c0;
                insque_nid_postfix(&c1, &c0);
                insque_nid_postfix(&c2, &c1);
                insque_nid_postfix(&c3, &c2);

                if (c0.q_forw != &c1 || c1.q_forw != &c2 || c2.q_forw != &c3 || c3.q_forw != &c0) {
                    allPassed = false;
                    break;
                }
                if (c0.q_back != &c3 || c3.q_back != &c2 || c2.q_back != &c1 || c1.q_back != &c0) {
                    allPassed = false;
                    break;
                }

                remque_nid_postfix(&c1);
                if (c0.q_forw != &c2 || c2.q_back != &c0) { allPassed = false; break; }

                remque_nid_postfix(&c3);
                if (c0.q_back != &c2 || c2.q_forw != &c0) { allPassed = false; break; }

                remque_nid_postfix(&c2);
                if (c0.q_forw != &c0 || c0.q_back != &c0) { allPassed = false; break; }

                remque_nid_postfix(&c0);
            }
        });
    }

    for (auto& w : workers) {
        w.join();
    }
    Check(allPassed.load());
}

int main() {
    CheckLinearInitial();
    CheckLinearSequential();
    CheckLinearMiddleInsertAndRemove();
    CheckLinearHeadAndTailRemoval();
    CheckCircularQueue();
    CheckNullPointers();
    CheckLargeQueueSequences();
    CheckPayloadIntegrity();
    CheckConcurrencyStress();
    return 0;
}
