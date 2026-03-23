#include "shm/shm_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>

static int g_test_pass = 1;
static int g_test_count = 0;
static int g_test_fail = 0;

#define TEST_ASSERT(cond, msg) do { \
    g_test_count++; \
    if (cond) { \
        printf("  [PASS] %s\n", msg); \
    } else { \
        printf("  [FAIL] %s\n", msg); \
        g_test_pass = 0; \
        g_test_fail++; \
    } \
} while(0)

#define TEST_ASSERT_EQ(actual, expected, msg) do { \
    g_test_count++; \
    if ((actual) == (expected)) { \
        printf("  [PASS] %s\n", msg); \
    } else { \
        printf("  [FAIL] %s (got %ld, expected %ld)\n", msg, (long)(actual), (long)(expected)); \
        g_test_pass = 0; \
        g_test_fail++; \
    } \
} while(0)

void test_version(void) {
    printf("[Test] shm_version\n");
    const char* ver = shm_version();
    TEST_ASSERT(ver != NULL && strlen(ver) > 0, "Version string is valid");
}

void test_strerror(void) {
    printf("[Test] shm_strerror\n");
    TEST_ASSERT(strcmp(shm_strerror(SHM_OK), "Success") == 0, "SHM_OK maps to 'Success'");
    TEST_ASSERT(strcmp(shm_strerror(SHM_ERR_INVALID_PARAM), "Invalid parameter") == 0, "SHM_ERR_INVALID_PARAM");
    TEST_ASSERT(strcmp(shm_strerror(SHM_ERR_NOT_FOUND), "Shared memory not found") == 0, "SHM_ERR_NOT_FOUND");
    TEST_ASSERT(strcmp(shm_strerror(SHM_ERR_NO_MEMORY), "Out of memory") == 0, "SHM_ERR_NO_MEMORY");
    TEST_ASSERT(strcmp(shm_strerror(SHM_ERR_TIMEOUT), "Operation timed out") == 0, "SHM_ERR_TIMEOUT");
    TEST_ASSERT(strcmp(shm_strerror(999), "Unknown error") == 0, "Unknown error code");
}

void test_exists(void) {
    printf("[Test] shm_exists\n");
    shm_destroy("/test_exists");  // cleanup

    TEST_ASSERT_EQ(shm_exists("/test_exists"), false, "Non-existent shm returns false");

    shm_handle_t* handle = NULL;
    shm_error_t err = shm_create("/test_exists", 1024, SHM_PERM_READ | SHM_PERM_WRITE,
                                 SHM_FLAG_CREATE | SHM_FLAG_EXCL, &handle);
    TEST_ASSERT(err == SHM_OK, "Created test shm");
    TEST_ASSERT_EQ(shm_exists("/test_exists"), true, "Existing shm returns true");

    shm_close(&handle);
    shm_destroy("/test_exists");
}

void test_create_join(void) {
    printf("[Test] shm_create and shm_join\n");
    shm_destroy("/test_cj");  // cleanup

    shm_handle_t* handle = NULL;
    shm_error_t err;

    // Test create
    err = shm_create("/test_cj", 1024, SHM_PERM_READ | SHM_PERM_WRITE,
                     SHM_FLAG_CREATE, &handle);
    TEST_ASSERT(err == SHM_OK, "shm_create succeeded");
    TEST_ASSERT(handle != NULL, "handle is not NULL after create");

    void* data_ptr = shm_get_data_ptr(handle);
    TEST_ASSERT(data_ptr != NULL, "shm_get_data_ptr returns valid pointer");

    size_t data_size = shm_get_data_size(handle);
    TEST_ASSERT_EQ(data_size, 1024, "shm_get_data_size returns correct size");

    // Test join from same process
    shm_handle_t* handle2 = NULL;
    err = shm_join("/test_cj", SHM_PERM_READ, &handle2);
    TEST_ASSERT(err == SHM_OK, "shm_join succeeded");

    uint32_t conn_count = 0;
    err = shm_get_connection_count(handle, &conn_count);
    TEST_ASSERT(err == SHM_OK, "shm_get_connection_count succeeded");
    TEST_ASSERT(conn_count >= 1, "Connection count >= 1");

    shm_close(&handle2);
    shm_close(&handle);
    shm_destroy("/test_cj");
}

void test_create_invalid_params(void) {
    printf("[Test] shm_create invalid params\n");
    shm_handle_t* handle = NULL;

    // Invalid name (no leading /)
    shm_error_t err = shm_create("test_no_slash", 1024, SHM_PERM_READ,
                                 SHM_FLAG_CREATE, &handle);
    TEST_ASSERT(err == SHM_ERR_NAME_INVALID, "Name without leading / fails");

    // Zero size
    err = shm_create("/test_zero_size", 0, SHM_PERM_READ, SHM_FLAG_CREATE, &handle);
    TEST_ASSERT(err == SHM_ERR_SIZE_INVALID, "Zero size fails");

    // NULL out_handle
    err = shm_create("/test_null_handle", 1024, SHM_PERM_READ, SHM_FLAG_CREATE, NULL);
    TEST_ASSERT(err == SHM_ERR_INVALID_PARAM, "NULL out_handle fails");

    // Already exists with EXCL should fail
    handle = NULL;
    err = shm_create("/test_exists2", 1024, SHM_PERM_READ, SHM_FLAG_CREATE | SHM_FLAG_EXCL, &handle);
    TEST_ASSERT(err == SHM_OK, "First create with EXCL succeeded");
    err = shm_create("/test_exists2", 1024, SHM_PERM_READ, SHM_FLAG_CREATE | SHM_FLAG_EXCL, &handle);
    TEST_ASSERT(err == SHM_ERR_ALREADY_EXISTS, "Duplicate create with EXCL fails");

    shm_close(&handle);
    shm_destroy("/test_exists2");

    // Without EXCL: opens existing or creates new
    err = shm_create("/test_exists2", 1024, SHM_PERM_READ, SHM_FLAG_CREATE, &handle);
    TEST_ASSERT(err == SHM_OK, "Create (no EXCL) succeeded");
    shm_close(&handle);
    err = shm_create("/test_exists2", 1024, SHM_PERM_READ, SHM_FLAG_CREATE, &handle);
    TEST_ASSERT(err == SHM_OK, "Re-open existing (no EXCL) also succeeds");
    shm_close(&handle);
    shm_destroy("/test_exists2");

    // With EXCL flag
    err = shm_create("/test_excl", 1024, SHM_PERM_READ, SHM_FLAG_CREATE | SHM_FLAG_EXCL, &handle);
    TEST_ASSERT(err == SHM_OK, "Create with EXCL succeeded");
    shm_close(&handle);
    shm_destroy("/test_excl");
}

void test_join_not_found(void) {
    printf("[Test] shm_join not found\n");
    shm_destroy("/test_notexist");

    shm_handle_t* handle = NULL;
    shm_error_t err = shm_join("/test_notexist", SHM_PERM_READ, &handle);
    TEST_ASSERT(err == SHM_ERR_NOT_FOUND, "Join non-existent shm fails");
}

void test_lock_unlock(void) {
    printf("[Test] shm_lock and shm_unlock\n");
    shm_destroy("/test_lock");

    shm_handle_t* handle = NULL;
    shm_error_t err = shm_create("/test_lock", 1024, SHM_PERM_READ | SHM_PERM_WRITE,
                                 SHM_FLAG_CREATE, &handle);
    TEST_ASSERT(err == SHM_OK, "Created test shm");

    // Test lock
    err = shm_lock(handle, 1000);
    TEST_ASSERT(err == SHM_OK || err == SHM_ERR_LOCK_RECOVERED, "shm_lock succeeded");

    // Test is_locked
    int locked = shm_is_locked(handle);
    TEST_ASSERT(locked == 1, "shm_is_locked returns true while locked");

    // Test unlock
    err = shm_unlock(handle);
    TEST_ASSERT(err == SHM_OK, "shm_unlock succeeded");

    // Test is_locked after unlock
    locked = shm_is_locked(handle);
    TEST_ASSERT(locked == 0, "shm_is_locked returns false after unlock");

    shm_close(&handle);
    shm_destroy("/test_lock");
}

void test_lock_try(void) {
    printf("[Test] shm_lock_try\n");
    shm_destroy("/test_lock_try");

    shm_handle_t* handle = NULL;
    shm_error_t err = shm_create("/test_lock_try", 1024, SHM_PERM_READ | SHM_PERM_WRITE,
                                 SHM_FLAG_CREATE, &handle);
    TEST_ASSERT(err == SHM_OK, "Created test shm");

    // First lock should succeed
    err = shm_lock_try(handle);
    TEST_ASSERT(err == SHM_OK || err == SHM_ERR_LOCK_RECOVERED, "shm_lock_try succeeded");

    // Second lock_try should fail (already locked by us)
    err = shm_lock_try(handle);
    TEST_ASSERT(err == SHM_ERR_LOCK_TIMEOUT, "shm_lock_try fails when already locked");

    shm_unlock(handle);
    shm_close(&handle);
    shm_destroy("/test_lock_try");
}

void test_lock_timeout(void) {
    printf("[Test] shm_lock timeout\n");
    shm_destroy("/test_lock_timeout");

    shm_handle_t* handle = NULL;
    shm_error_t err = shm_create("/test_lock_timeout", 1024, SHM_PERM_READ | SHM_PERM_WRITE,
                                 SHM_FLAG_CREATE, &handle);
    TEST_ASSERT(err == SHM_OK, "Created test shm");

    // Lock it first
    shm_lock(handle, -1);

    // Fork a child to try lock with timeout
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        shm_handle_t* handle2 = NULL;
        shm_error_t err2 = shm_join("/test_lock_timeout", SHM_PERM_READ, &handle2);
        if (err2 == SHM_OK) {
            err2 = shm_lock(handle2, 100);  // 100ms timeout
            if (err2 == SHM_ERR_LOCK_TIMEOUT) {
                _exit(0);  // Expected
            } else {
                _exit(1);  // Unexpected
            }
        }
        _exit(2);  // Join failed
    }

    int status;
    waitpid(pid, &status, 0);
    TEST_ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0,
                "Child process got lock timeout as expected");

    shm_unlock(handle);
    shm_close(&handle);
    shm_destroy("/test_lock_timeout");
}

void test_notify_wait(void) {
    printf("[Test] shm_notify and shm_wait\n");
    shm_destroy("/test_notify");

    shm_handle_t* handle = NULL;
    shm_error_t err = shm_create("/test_notify", 1024, SHM_PERM_READ | SHM_PERM_WRITE,
                                 SHM_FLAG_CREATE, &handle);
    TEST_ASSERT(err == SHM_OK, "Created test shm");

    // Test get_notify_fd
    int fd = shm_get_notify_fd(handle);
    TEST_ASSERT(fd >= 0, "shm_get_notify_fd returns valid fd");

    // Write data and notify
    int* data = (int*)shm_get_data_ptr(handle);
    *data = 42;
    err = shm_notify(handle);
    TEST_ASSERT(err == SHM_OK, "shm_notify succeeded");

    // Wait should succeed
    err = shm_wait(handle, 1000);
    TEST_ASSERT(err == SHM_OK, "shm_wait succeeded after notify");

    // Consume notification
    err = shm_consume_notify(handle);
    TEST_ASSERT(err == SHM_OK, "shm_consume_notify succeeded");

    // Second wait should timeout
    err = shm_wait(handle, 100);
    TEST_ASSERT(err == SHM_ERR_TIMEOUT, "shm_wait times out when no notification pending");

    shm_close(&handle);
    shm_destroy("/test_notify");
}

void test_close_and_unmap(void) {
    printf("[Test] shm_unmap and shm_close\n");
    shm_destroy("/test_close");

    shm_handle_t* handle = NULL;
    shm_error_t err = shm_create("/test_close", 1024, SHM_PERM_READ | SHM_PERM_WRITE,
                                 SHM_FLAG_CREATE, &handle);
    TEST_ASSERT(err == SHM_OK, "Created test shm");

    // Test unmap
    err = shm_unmap(handle);
    TEST_ASSERT(err == SHM_OK, "shm_unmap succeeded");

    // Note: after unmap, handle's addr is NULL, but handle itself is still valid
    // We need to close to free the handle

    // Re-create by joining
    shm_close(&handle);
    err = shm_join("/test_close", SHM_PERM_READ, &handle);
    TEST_ASSERT(err == SHM_OK, "Join after close succeeded");

    shm_close(&handle);
    shm_destroy("/test_close");
}

void test_destroy(void) {
    printf("[Test] shm_destroy\n");
    shm_destroy("/test_destroy");  // ensure clean

    // Destroy non-existent should fail
    shm_error_t err = shm_destroy("/test_destroy");
    TEST_ASSERT(err == SHM_ERR_NOT_FOUND, "Destroy non-existent fails");

    // Create, then destroy
    shm_handle_t* handle = NULL;
    err = shm_create("/test_destroy", 1024, SHM_PERM_READ | SHM_PERM_WRITE,
                     SHM_FLAG_CREATE, &handle);
    TEST_ASSERT(err == SHM_OK, "Created test shm");
    shm_close(&handle);

    err = shm_destroy("/test_destroy");
    TEST_ASSERT(err == SHM_OK, "Destroy succeeded");

    TEST_ASSERT(shm_exists("/test_destroy") == false, "shm no longer exists after destroy");
}

void test_readonly(void) {
    printf("[Test] SHM_FLAG_READONLY\n");
    shm_destroy("/test_readonly");

    shm_handle_t* handle = NULL;
    shm_error_t err = shm_create("/test_readonly", 1024, SHM_PERM_READ | SHM_PERM_WRITE,
                                 SHM_FLAG_CREATE, &handle);
    TEST_ASSERT(err == SHM_OK, "Created test shm");

    // Write some data
    int* data = (int*)shm_get_data_ptr(handle);
    *data = 123;
    TEST_ASSERT(*data == 123, "Write succeeded");

    shm_close(&handle);

    // Re-open as readonly
    err = shm_join("/test_readonly", SHM_PERM_READ, &handle);
    TEST_ASSERT(err == SHM_OK, "Join as readonly succeeded");

    // Reading should work
    const int* read_data = (const int*)shm_get_data_ptr_const(handle);
    TEST_ASSERT(*read_data == 123, "Read from readonly succeeded");

    shm_close(&handle);
    shm_destroy("/test_readonly");
}

void test_server_client(void) {
    printf("[Test] Server-Client pattern\n");
    shm_destroy("/test_pc");

    // Client first creates the shm so it's ready when server starts
    shm_handle_t* cons_handle = NULL;
    shm_error_t err = shm_create("/test_pc", sizeof(int), SHM_PERM_READ | SHM_PERM_WRITE,
                                 SHM_FLAG_CREATE | SHM_FLAG_EXCL, &cons_handle);
    TEST_ASSERT(err == SHM_OK, "Client created shm");

    // Client is the server in this test - write data
    int* data = (int*)shm_get_data_ptr(cons_handle);
    *data = 42;
    shm_notify(cons_handle);

    // Verify we can read our own data
    const int* read_data = (const int*)shm_get_data_ptr_const(cons_handle);
    TEST_ASSERT(*read_data == 42, "Server wrote and can read back value");

    shm_close(&cons_handle);
    shm_destroy("/test_pc");
}

void test_multi_client(void) {
    printf("[Test] Multiple clients\n");
    shm_destroy("/test_multi");

    // Create shm first
    shm_handle_t* prod_handle = NULL;
    shm_error_t err = shm_create("/test_multi", sizeof(int), SHM_PERM_READ | SHM_PERM_WRITE,
                                 SHM_FLAG_CREATE | SHM_FLAG_EXCL, &prod_handle);
    TEST_ASSERT(err == SHM_OK, "Server created shm");

    // Test that we can have multiple connections
    shm_handle_t* cons_handle1 = NULL;
    shm_handle_t* cons_handle2 = NULL;

    err = shm_join("/test_multi", SHM_PERM_READ, &cons_handle1);
    TEST_ASSERT(err == SHM_OK, "First client joined");

    err = shm_join("/test_multi", SHM_PERM_READ, &cons_handle2);
    TEST_ASSERT(err == SHM_OK, "Second client joined");

    // Write data and notify
    int* data = (int*)shm_get_data_ptr(prod_handle);
    *data = 456;
    shm_notify(prod_handle);

    // Verify both clients can read
    const int* read1 = (const int*)shm_get_data_ptr_const(cons_handle1);
    const int* read2 = (const int*)shm_get_data_ptr_const(cons_handle2);
    TEST_ASSERT(*read1 == 456, "First client read correct value");
    TEST_ASSERT(*read2 == 456, "Second client read correct value");

    shm_close(&cons_handle1);
    shm_close(&cons_handle2);
    shm_close(&prod_handle);
    shm_destroy("/test_multi");
}

int main(void) {
    printf("=== SHM Unit Tests ===\n\n");

    test_version();
    test_strerror();
    test_exists();
    test_create_join();
    test_create_invalid_params();
    test_join_not_found();
    test_lock_unlock();
    test_lock_try();
    test_lock_timeout();
    test_notify_wait();
    test_close_and_unmap();
    test_destroy();
    test_readonly();
    test_server_client();
    test_multi_client();

    printf("\n=== Results: %d/%d passed ===\n", g_test_count - g_test_fail, g_test_count);
    return g_test_pass ? 0 : 1;
}
