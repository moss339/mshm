#ifndef SHM_ADAPTER_H
#define SHM_ADAPTER_H

// 平台检测
#if defined(__linux__)
    #define SHM_PLATFORM_LINUX  1
    #define SHM_PLATFORM_QNX    0
    #define SHM_PLATFORM_NAME   "linux"
#elif defined(__QNX__)
    #define SHM_PLATFORM_LINUX  0
    #define SHM_PLATFORM_QNX    1
    #define SHM_PLATFORM_NAME   "qnx"
#else
    #error "Unsupported platform"
#endif

// 平台特定的包含文件
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

// 导出宏
#if defined(SHM_EXPORTS)
    #define SHM_API __attribute__((visibility("default")))
#else
    #define SHM_API
#endif

// 线程本地存储
#define SHM_THREAD_LOCAL __thread

#endif // SHM_ADAPTER_H
