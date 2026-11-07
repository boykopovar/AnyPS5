#ifndef PS5LIFT_RUNTIME_HPP
#define PS5LIFT_RUNTIME_HPP

#include <ps5lift/State.h>

#include <cstdint>

namespace Ps5Lift {

const Ps5LiftProgram& Program();

Ps5LiftFunction LookupFunction(std::uint64_t address);

Ps5LiftState* BoundState();

void InstallNativeCalls();

void RequireNoPendingX87Exceptions(const Ps5LiftState& state);

}

#endif
