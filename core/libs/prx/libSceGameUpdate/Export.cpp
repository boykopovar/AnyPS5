#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceGameUpdateAbortRequest(int request_id) {
 (void)request_id;
 return 0;
}

int sceGameUpdateCheck(int request_id, const GameUpdateCheckParam* param, GameUpdateCheckResult* result) {
 (void)request_id;
 (void)param;
 (void)result;
 return 0;
}

int sceGameUpdateCreateRequest(void) {
 return 0;
}

int sceGameUpdateDeleteRequest(int request_id) {
 (void)request_id;
 return 0;
}

int sceGameUpdateGetAddcontLatestVersion(uint32_t service_label, const void* entitlement_label, GameUpdateAddcontVersionInfo* info) {
 (void)service_label;
 (void)entitlement_label;
 (void)info;
 return 0;
}

int sceGameUpdateInitialize(void) {
 return 0;
}

int sceGameUpdateTerminate(void) {
 return 0;
}

}
