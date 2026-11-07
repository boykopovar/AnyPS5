#include <ps5lift/State.h>

namespace {

const Ps5LiftProgram EmptyProgram = {};
const bool Used = (ps5lift_use_program(&EmptyProgram), true);

}
