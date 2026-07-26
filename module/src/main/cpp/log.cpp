// FlutterTap native module -- by Eduardo Lopes
#include "log.h"

#include <android/log.h>
#include <cstdarg>

#define LOG_TAG "FlutterTap"

void ft_log_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    __android_log_vprint(ANDROID_LOG_INFO, LOG_TAG, fmt, args);
    va_end(args);
}

void ft_log_warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    __android_log_vprint(ANDROID_LOG_WARN, LOG_TAG, fmt, args);
    va_end(args);
}

void ft_log_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    __android_log_vprint(ANDROID_LOG_ERROR, LOG_TAG, fmt, args);
    va_end(args);
}
