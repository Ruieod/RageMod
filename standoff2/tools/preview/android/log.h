#pragma once
#include <cstdio>
#define ANDROID_LOG_INFO 4
#define ANDROID_LOG_WARN 5
#define ANDROID_LOG_ERROR 6
inline int __android_log_print(int, const char* tag, const char* fmt, ...) { (void)tag; (void)fmt; return 0; }
