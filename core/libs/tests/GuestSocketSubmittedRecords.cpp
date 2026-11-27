#include "prx/libkernel/Socket/include/SubmittedRecords.hpp"
#include <cstdlib>

static void Require(bool value) { if (!value) std::abort(); }

struct Operation {
    bool done = false;
    bool prepared = false;
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

    KernelSocketPeek::SubmittedRecords<Operation> retried;
    int attempts = 0;
    const auto prepare = [&attempts](Operation& operation) {
        if (++attempts == 1) return false;
        operation.prepared = true;
        return true;
    };
    Require(retried.Acquire(completed, prepare) == nullptr);
    auto* recovered = retried.Acquire(completed, prepare);
    Require(recovered && recovered->record.prepared && attempts == 2);
    retried.Release(recovered, false);
    Require(retried.Acquire(completed, prepare) == recovered && attempts == 3);
    retried.Release(recovered, false);
    return 0;
}
