#include "shm/shm_api.h"
#include <stdio.h>
#include <poll.h>

typedef struct {
    float temperature;
    float humidity;
    uint64_t timestamp;
} SensorData;

int main(void) {
    shm_handle_t* handle = NULL;
    shm_error_t err;

    printf("[Client] Starting...\n");

    err = shm_join("/test_sensor", SHM_PERM_READ, &handle);
    if (err != SHM_OK) {
        printf("[Client] Join failed: %s\n", shm_strerror(err));
        return 1;
    }

    printf("[Client] Connected, waiting for data...\n");

    struct pollfd pfd = {
        .fd = shm_get_notify_fd(handle),
        .events = POLLIN,
    };

    for (int count = 0; count < 10; count++) {
        int ret = poll(&pfd, 1, 2000);

        if (ret < 0) {
            printf("[Client] Poll error\n");
            break;
        } else if (ret == 0) {
            printf("[Client] Timeout\n");
            continue;
        }

        if (pfd.revents & POLLIN) {
            shm_consume_notify(handle);

            shm_lock(handle, 500);
            const SensorData* data = (const SensorData*)shm_get_data_ptr_const(handle);
            printf("[Client] Got: temp=%.1f, humidity=%.1f\n",
                   data->temperature, data->humidity);
            shm_unlock(handle);
        }
    }

    shm_close(&handle);
    printf("[Client] Done\n");
    return 0;
}
