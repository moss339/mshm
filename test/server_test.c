#include "shm/shm_api.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    float temperature;
    float humidity;
    uint64_t timestamp;
} SensorData;

int main(void) {
    shm_handle_t* handle = NULL;
    SensorData* data = NULL;
    shm_error_t err;

    printf("[Server] Starting...\n");

    err = shm_create("/test_sensor",
                     sizeof(SensorData),
                     SHM_PERM_READ | SHM_PERM_WRITE,
                     SHM_FLAG_CREATE | SHM_FLAG_EXCL,
                     &handle);
    if (err != SHM_OK) {
        printf("[Server] Create failed: %s\n", shm_strerror(err));
        return 1;
    }

    data = (SensorData*)shm_get_data_ptr(handle);

    for (int i = 0; i < 10; i++) {
        err = shm_lock(handle, 1000);
        if (err != SHM_OK) {
            printf("[Server] Lock failed: %s\n", shm_strerror(err));
            break;
        }

        data->temperature = 20.0f + (i % 10);
        data->humidity = 50.0f + (i % 20);
        data->timestamp = (uint64_t)time(NULL);

        printf("[Server] Wrote: temp=%.1f, humidity=%.1f\n",
               data->temperature, data->humidity);

        shm_unlock(handle);
        shm_notify(handle);

        usleep(100000);
    }

    printf("[Server] Finished, cleaning up...\n");

    shm_destroy("/test_sensor");
    shm_close(&handle);

    return 0;
}
