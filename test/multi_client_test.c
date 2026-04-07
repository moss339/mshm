#include "shm/shm_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

void client_loop(const char* name, int id) {
    shm_handle_t handle = NULL;
    shm_error_t err;

    // 等待共享内存可用（最多等待 5 秒）
    for (int retry = 0; retry < 50; retry++) {
        err = shm_join(name, SHM_PERM_READ, &handle);
        if (err == SHM_OK) break;
        usleep(100000);  // 100ms
    }

    if (err != SHM_OK) {
        printf("[Consumer %d] Join failed after retries: %s\n", id, shm_strerror(err));
        return;
    }

    printf("[Consumer %d] Started\n", id);

    for (int count = 0; count < 5; count++) {
        err = shm_wait(handle, 3000);
        if (err == SHM_ERR_TIMEOUT) {
            printf("[Consumer %d] Timeout\n", id);
            continue;
        }
        if (err != SHM_OK) {
            printf("[Consumer %d] Error: %s\n", id, shm_strerror(err));
            break;
        }

        const int* data = (const int*)shm_get_data_ptr_const(handle);
        if (data) {
            printf("[Consumer %d] Got data: %d\n", id, *data);
        }
        shm_consume_notify(handle);
    }

    shm_close(&handle);
    printf("[Consumer %d] Done\n", id);
}

int main(int argc, char* argv[]) {
    int num_clients = (argc > 1) ? atoi(argv[1]) : 3;

    // Clean up any existing shm first
    shm_destroy("/test_multi");

    printf("[Main] Creating shared memory first...\n");

    shm_handle_t handle = NULL;
    shm_error_t err;

    err = shm_create("/test_multi",
                     sizeof(int),
                     SHM_PERM_READ | SHM_PERM_WRITE,
                     SHM_FLAG_CREATE | SHM_FLAG_EXCL,
                     &handle);
    if (err != SHM_OK) {
        printf("[Main] Create failed: %s\n", shm_strerror(err));
        return 1;
    }

    printf("[Main] Shared memory created, starting %d clients...\n", num_clients);

    // Start all clients (they will wait for shm to be available)
    for (int i = 0; i < num_clients; i++) {
        if (fork() == 0) {
            client_loop("/test_multi", i);
            exit(0);
        }
    }

    // Give clients time to join
    sleep(1);

    printf("[Main] Producer started\n");

    for (int i = 0; i < 5; i++) {
        int* data = (int*)shm_get_data_ptr(handle);
        *data = i * 100;

        shm_notify(handle);
        printf("[Main] Wrote: %d\n", *data);
        sleep(1);
    }

    // Wait for clients to finish, then cleanup
    for (int i = 0; i < num_clients; i++) {
        wait(NULL);
    }

    // Close handle first (unmaps memory, decrements ref count)
    shm_close(&handle);

    // Then destroy the shared memory object
    shm_destroy("/test_multi");

    printf("[Main] All done\n");
    return 0;
}
