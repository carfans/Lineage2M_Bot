/**
 * @file l2m_api.h
 * @brief Lineage2MBot C 动态链接库统一导出 API
 */

#ifndef L2M_API_H
#define L2M_API_H

#include "l2m_types.h"
#include "l2m_vision.h"
#include "l2m_hp.h"
#include "l2m_popup.h"
#include "l2m_platform.h"

#ifdef _WIN32
  #if defined(L2M_BUILD_DLL)
    #define L2M_API __declspec(dllexport)
  #elif defined(L2M_USE_STATIC)
    #define L2M_API
  #else
    #define L2M_API __declspec(dllimport)
  #endif
#else
  #define L2M_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* 库初始化与版本信息 */
L2M_API const char* l2m_get_version(void);
L2M_API bool l2m_init_engine(void);
L2M_API void l2m_shutdown_engine(void);

/* 纯 C 高性能血条百分比计算接口 (供 Python ctypes 或原生 C 调用) */
L2M_API int32_t l2m_calculate_hp_raw(
    const uint8_t* rgb_data,
    int32_t width,
    int32_t height,
    int32_t stride,
    const L2MHpConfig* config,
    L2MHpResult* out_result
);

/* 纯 C 弹窗分析与按钮定位接口 (带背景色先验确认) */
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
);

/* 弹窗背景色独立校验接口 */
L2M_API bool l2m_check_popup_bg_raw(
    const uint8_t* rgb_data,
    int32_t width,
    int32_t height,
    int32_t stride,
    int32_t popup_type_enum,
    L2MPopupBgInfo* out_bg_info
);

#ifdef __cplusplus
}
#endif

#endif /* L2M_API_H */
