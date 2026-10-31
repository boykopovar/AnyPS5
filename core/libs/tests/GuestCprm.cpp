#include "prx/libCprm/include/InventoryExports.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdlib>
#include <stdexcept>

namespace {

template <typename Function>
void RequireThrows(Function function) {
    try {
        function();
        std::abort();
    } catch (const std::runtime_error&) {
    }

}

} // namespace

int main() {
    RequireThrows(libCprmUnknown__plus_Eh9ZNYi6zo_nid_no_patch);
    RequireThrows(libCprmUnknown__plus_X9TC_plus_1sw8c_nid_no_patch);
    RequireThrows(libCprmUnknown_1M23A00Z5YY_nid_no_patch);
    RequireThrows(libCprmUnknown_9Blq0Y1cfC4_nid_no_patch);
    RequireThrows(libCprmUnknown_C_plus_UDpkvBfq0_nid_no_patch);
    RequireThrows(libCprmUnknown_Dxqbh2fid_minus_0_nid_no_patch);
    RequireThrows(libCprmUnknown_GYIfXUO2DQM_nid_no_patch);
    RequireThrows(libCprmUnknown_GzdjmaMs2ec_nid_no_patch);
    RequireThrows(libCprmUnknown_Ja2HmL89pQk_nid_no_patch);
    RequireThrows(libCprmUnknown_N1QA8ydpT5s_nid_no_patch);
    RequireThrows(libCprmUnknown_Qqt9wsrnORY_nid_no_patch);
    RequireThrows(libCprmUnknown_SebG2x8vugA_nid_no_patch);
    RequireThrows(libCprmUnknown_Sz5wfo9uoQo_nid_no_patch);
    RequireThrows(libCprmUnknown_UXwPU6S3K3M_nid_no_patch);
    RequireThrows(libCprmUnknown_Ut6wGp0l_minus_EU_nid_no_patch);
    RequireThrows(libCprmUnknown_Uv_minus_1Y7ctRPg_nid_no_patch);
    RequireThrows(libCprmUnknown_WWfjXT2d7SM_nid_no_patch);
    RequireThrows(libCprmUnknown_YhPUdyQoZ2w_nid_no_patch);
    RequireThrows(libCprmUnknown_YvBd1vant8Y_nid_no_patch);
    RequireThrows(libCprmUnknown_aoE6klVaQmo_nid_no_patch);
    RequireThrows(libCprmUnknown_fb1srL_plus__plus_42M_nid_no_patch);
    RequireThrows(libCprmUnknown_gnClz0AOgtw_nid_no_patch);
    RequireThrows(libCprmUnknown_h9jXWgBLon0_nid_no_patch);
    RequireThrows(libCprmUnknown_hs4OD9CTzsU_nid_no_patch);
    RequireThrows(libCprmUnknown_hvZH8f_plus_HBVg_nid_no_patch);
    RequireThrows(libCprmUnknown_i2aJI0V3U3A_nid_no_patch);
    RequireThrows(libCprmUnknown_iDCilRZjGRM_nid_no_patch);
    RequireThrows(libCprmUnknown_lCmvZpz_minus_8Sg_nid_no_patch);
    RequireThrows(libCprmUnknown_llRRCXKDB8g_nid_no_patch);
    RequireThrows(libCprmUnknown_mq_plus_QSu3eTDM_nid_no_patch);
    RequireThrows(libCprmUnknown_pa1PYEalrtU_nid_no_patch);
    RequireThrows(libCprmUnknown_tsJszAZ7k_plus_A_nid_no_patch);
    RequireThrows(libCprmUnknown_vR2Jy582NrI_nid_no_patch);
    RequireThrows(libCprmUnknown_x5bhPjRC_plus_WQ_nid_no_patch);
    RequireThrows(libCprmUnknown_y2mH4WVl028_nid_no_patch);
    return 0;
}
