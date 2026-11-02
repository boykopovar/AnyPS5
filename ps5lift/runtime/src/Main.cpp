#include <ps5lift/State.h>

extern "C" const Ps5LiftProgram ps5lift_program;

int main() {
    ps5lift_use_program(&ps5lift_program);
    ps5lift_run_main();
    return 0;
}
