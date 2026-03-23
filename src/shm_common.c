#include "shm/shm_api.h"
#include "shm/shm_adapter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* shm_strerror(shm_error_t err) {
    static const char* errors[] = {
        [SHM_OK]                  = "Success",
        [SHM_ERR_INVALID_PARAM]   = "Invalid parameter",
        [SHM_ERR_NOT_FOUND]       = "Shared memory not found",
        [SHM_ERR_ALREADY_EXISTS]  = "Shared memory already exists",
        [SHM_ERR_PERMISSION]      = "Permission denied",
        [SHM_ERR_NO_MEMORY]       = "Out of memory",
        [SHM_ERR_SYSTEM]          = "System error",
        [SHM_ERR_TIMEOUT]         = "Operation timed out",
        [SHM_ERR_NOT_INITIALIZED] = "Not initialized",
        [SHM_ERR_NOT_CONNECTED]   = "Not connected",
        [SHM_ERR_STALE_HANDLE]    = "Stale handle",
        [SHM_ERR_VERSION_MISMATCH]= "Version mismatch",
        [SHM_ERR_NAME_INVALID]   = "Invalid name",
        [SHM_ERR_SIZE_INVALID]   = "Invalid size",
        [SHM_ERR_LOCK_TIMEOUT]    = "Lock timeout",
        [SHM_ERR_LOCK_DEAD]       = "Deadlock detected",
        [SHM_ERR_LOCK_RECOVERED]  = "Lock recovered from dead owner",
        [SHM_ERR_LOCK_NOT_OWNED]  = "Lock not owned by caller",
        [SHM_ERR_ALREADY_OPEN]    = "Already open",
        [SHM_ERR_NOT_OPEN]        = "Not open",
        [SHM_ERR_MAP_FAILED]      = "Map failed",
        [SHM_ERR_UNMAP_FAILED]    = "Unmap failed",
        [SHM_ERR_CLOSE_FAILED]    = "Close failed",
    };

    if (err >= 0 && err < (shm_error_t)(sizeof(errors) / sizeof(errors[0]))) {
        return errors[err];
    }
    return "Unknown error";
}

const char* shm_version(void) {
    return "1.0.0";
}
