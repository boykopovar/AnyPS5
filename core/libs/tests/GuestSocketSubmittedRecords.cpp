#include "prx/libkernel/Socket/include/SubmittedRecords.hpp"
#include <cstdlib>

static void Require(bool value) { if (!value) std::abort(); }

struct Operation {
    bool done = false;
};

int main() {
    KernelSocketPeek::SubmittedRecords<Operation> pool;
    const auto completed = [](Operation& operation) { return operation.done; };

    auto* pending = pool.Acquire(completed);
    pool.Release(pending, true);
    auto* second = pool.Acquire(completed);
    Require(second != pending);
    pool.Release(second, false);
    Require(pool.Acquire(completed) == second);
    auto* third = pool.Acquire(completed);
    Require(third != pending && third != second);
    pool.Release(second, false);
    pool.Release(third, false);
    Require(pool.Acquire(completed) == second);
    pool.Release(second, false);

    pending->record.done = true;
    Require(pool.Acquire(completed) == pending);
    pending->record.done = false;
    pool.Release(pending, false);
    Require(pool.Acquire(completed) == pending);
    pool.Release(pending, false);
    return 0;
}
