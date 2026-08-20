/**
 * @file logger.c
 * @brief Lineage2MBot C 核心轻量级日志记录器实现
 */

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

typedef enum {
    L2M_LOG_DEBUG = 0,
    L2M_LOG_INFO,
    L2M_LOG_WARN,
    L2M_LOG_ERROR
} L2MLogLevel;

static L2MLogLevel g_min_level = L2M_LOG_INFO;
static FILE* g_log_file = NULL;

void l2m_log_set_level(int level) {
    g_min_level = (L2MLogLevel)level;
}

void l2m_log_set_file(const char* filepath) {
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
    if (filepath && strlen(filepath) > 0) {
        g_log_file = fopen(filepath, "a");
    }
}

void l2m_log_write(int level, const char* tag, const char* fmt, ...) {
    if (level < (int)g_min_level) return;

    const char* level_strs[] = {"DEBUG", "INFO ", "WARN ", "ERROR"};
    const char* lvl_str = (level >= 0 && level <= 3) ? level_strs[level] : "INFO ";

    time_t now = time(NULL);
    struct tm t;
#ifdef _WIN32
    localtime_s(&t, &now);
#else
    localtime_r(&now, &t);
#endif

    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &t);

    char msg_buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    va_end(args);

    /* 控制台输出 */
    printf("%s - %s - [%s] %s\n", time_buf, lvl_str, tag ? tag : "Core", msg_buf);
    fflush(stdout);

    /* 文件输出 */
    if (g_log_file) {
        fprintf(g_log_file, "%s - %s - [%s] %s\n", time_buf, lvl_str, tag ? tag : "Core", msg_buf);
        fflush(g_log_file);
    }
}
