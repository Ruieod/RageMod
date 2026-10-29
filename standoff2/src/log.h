#pragma once
#include <android/log.h>

#define RM_TAG "RageMod"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, RM_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, RM_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, RM_TAG, __VA_ARGS__)
