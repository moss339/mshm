#ifndef SHM_ERRORS_H
#define SHM_ERRORS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SHM_OK = 0,

    // 通用错误 (1-99)
    SHM_ERR_INVALID_PARAM    = 1,
    SHM_ERR_NOT_FOUND        = 2,
    SHM_ERR_ALREADY_EXISTS   = 3,
    SHM_ERR_PERMISSION       = 4,
    SHM_ERR_NO_MEMORY        = 5,
    SHM_ERR_SYSTEM           = 6,
    SHM_ERR_TIMEOUT          = 7,
    SHM_ERR_NOT_INITIALIZED  = 8,
    SHM_ERR_NOT_CONNECTED   = 9,
    SHM_ERR_STALE_HANDLE     = 10,
    SHM_ERR_VERSION_MISMATCH = 11,
    SHM_ERR_NAME_INVALID     = 12,
    SHM_ERR_SIZE_INVALID     = 13,

    // 锁相关错误 (100-199)
    SHM_ERR_LOCK_TIMEOUT     = 100,
    SHM_ERR_LOCK_DEAD        = 101,
    SHM_ERR_LOCK_RECOVERED   = 102,
    SHM_ERR_LOCK_NOT_OWNED   = 103,

    // 状态错误 (200-299)
    SHM_ERR_ALREADY_OPEN     = 200,
    SHM_ERR_NOT_OPEN         = 201,
    SHM_ERR_MAP_FAILED       = 202,
    SHM_ERR_UNMAP_FAILED     = 203,
    SHM_ERR_CLOSE_FAILED     = 204,
} shm_error_t;

const char* shm_strerror(shm_error_t err);

#ifdef __cplusplus
}
#endif

#endif // SHM_ERRORS_H
