// FlutterTap native module -- by Eduardo Lopes
//
// printf-style logging to logcat under the "FlutterTap" tag. Since the module
// runs inside the host app's process, this is the only channel available for
// diagnosing a failed hook -- `adb logcat -s FlutterTap:V`.
//
// The format attribute makes the compiler type-check every call site, which
// matters here: a bad conversion would otherwise be a silent crash inside a
// hook running in someone else's app.
#pragma once

#define FT_PRINTF_LIKE __attribute__((format(printf, 1, 2)))

void ft_log_info(const char *fmt, ...) FT_PRINTF_LIKE;
void ft_log_warn(const char *fmt, ...) FT_PRINTF_LIKE;
void ft_log_error(const char *fmt, ...) FT_PRINTF_LIKE;
