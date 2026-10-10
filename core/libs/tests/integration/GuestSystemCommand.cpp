#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

extern "C" {
int APS5_VABI system_nid_postfix(const char*);
int* APS5_VABI __error_nid_postfix();
}

namespace {

using Testing::Case;
using Testing::RequireEqual;

constexpr int commandNotFound = 127;
constexpr const char* missingTool = "clrxdisasm --version > /dev/null 2>&1";

const Case nullCommand{"System_NullCommand_ReportsShellAvailable", [] {
    RequireEqual(system_nid_postfix(nullptr), 1, "shell availability query");
}};

const Case anyCommand{"System_Command_ExitsNormallyWithCommandNotFound", [] {
    const int status = system_nid_postfix(missingTool);
    RequireEqual(status & 0x7f, 0, "termination signal bits");
    RequireEqual((status >> 8) & 0xff, commandNotFound, "exit status");
}};

const Case everyCommand{"System_DifferentCommands_ReturnSameStatus", [] {
    const int status = system_nid_postfix(missingTool);
    RequireEqual(system_nid_postfix("exit 0"), status, "exit 0");
    RequireEqual(system_nid_postfix(""), status, "empty command");
}};

const Case keepsErrno{"System_Calls_PreserveErrno", [] {
    *__error_nid_postfix() = 13;
    system_nid_postfix(nullptr);
    system_nid_postfix(missingTool);
    system_nid_postfix("exit 0");
    system_nid_postfix("");
    RequireEqual(*__error_nid_postfix(), 13, "errno after system calls");
}};

} // namespace
