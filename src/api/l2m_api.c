/**
 * @file l2m_api.c
 * @brief Lineage2MBot C 动态库统一导出 API 实现
 */

#if !defined(L2M_USE_STATIC) && !defined(L2M_BUILD_DLL)
#define L2M_BUILD_DLL
#endif
#include "../../include/l2m_api.h"
#include <string.h>
#include <stdlib.h>

static const char* G_VERSION = "Lineage2MBot-C-Engine v2.0.0";
static bool G_IS_INITIALIZED = false;

L2M_API const char* l2m_get_version(void) {
    return G_VERSION;
}

L2M_API bool l2m_init_engine(void) {
    G_IS_INITIALIZED = true;
    return true;
}

L2M_API void l2m_shutdown_engine(void) {
    G_IS_INITIALIZED = false;
}

L2M_API int32_t l2m_calculate_hp_raw(
    const uint8_t* rgb_data,
    int32_t width,
    int32_t height,
    int32_t stride,
    const L2MHpConfig* config,
    L2MHpResult* out_result
) {
    if (!rgb_data || width <= 0 || height <= 0 || !config || !out_result) {
        return -1;
    }

    L2MImageBuffer img;
    img.data = (uint8_t*)rgb_data;
    img.width = width;
    img.height = height;
    img.channels = 3;
    img.stride = stride > 0 ? stride : width * 3;
    img.format = L2M_FMT_RGB888;
    img.is_owner = false;

    if (l2m_calculate_hp(&img, config, out_result)) {
        return out_result->hp_percent;
    }
    return -1;
}

L2M_API bool l2m_detect_popup_raw(
    const uint8_t* rgb_data,
    int32_t width,
    int32_t height,
    int32_t stride,
    int32_t base_x,
    int32_t base_y,
    int32_t popup_type_enum,
    bool validate_bg,
    L2MPopupResult* out_result
) {
    if (!rgb_data || width <= 0 || height <= 0 || !out_result) {
        return false;
    }

    L2MImageBuffer img;
    img.data = (uint8_t*)rgb_data;
    img.width = width;
    img.height = height;
    img.channels = 3;
    img.stride = stride > 0 ? stride : width * 3;
    img.format = L2M_FMT_RGB888;
    img.is_owner = false;

    return l2m_detect_popup(&img, base_x, base_y, (L2MPopupType)popup_type_enum, validate_bg, out_result);
}

L2M_API bool l2m_check_popup_bg_raw(
    const uint8_t* rgb_data,
    int32_t width,
    int32_t height,
    int32_t stride,
    int32_t popup_type_enum,
    L2MPopupBgInfo* out_bg_info
) {
    if (!rgb_data || width <= 0 || height <= 0 || !out_bg_info) {
        return false;
    }

    L2MImageBuffer img;
    img.data = (uint8_t*)rgb_data;
    img.width = width;
    img.height = height;
    img.channels = 3;
    img.stride = stride > 0 ? stride : width * 3;
    img.format = L2M_FMT_RGB888;
    img.is_owner = false;

    return l2m_check_popup_background(&img, (L2MPopupType)popup_type_enum, out_bg_info);
}
