#include "shm/shm_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <poll.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>

static int validate_name(const char* name) {
    if (!name || name[0] != '/') return -1;
    if (strlen(name) >= SHM_NAME_MAX) return -1;
    return 0;
}

shm_error_t shm_create(const char* name, size_t data_size,
                       shm_permission_t perm, shm_flags_t flags,
                       shm_handle_t* out_handle) {
    (void)perm;
    if (validate_name(name) != 0) return SHM_ERR_NAME_INVALID;
    if (data_size == 0) return SHM_ERR_SIZE_INVALID;
    if (!out_handle) return SHM_ERR_INVALID_PARAM;

    size_t total_size = SHM_HEADER_SIZE + data_size;
    int oflag = O_RDWR | ((flags & SHM_FLAG_CREATE) ? O_CREAT : 0) |
                ((flags & SHM_FLAG_EXCL) ? O_EXCL : 0);

    int fd = shm_open(name, oflag, 0664);
    if (fd < 0) return (errno == EEXIST) ? SHM_ERR_ALREADY_EXISTS : SHM_ERR_SYSTEM;
    if (ftruncate(fd, total_size) < 0) { close(fd); shm_unlink(name); return SHM_ERR_SYSTEM; }

    void* addr = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) { close(fd); shm_unlink(name); return SHM_ERR_MAP_FAILED; }

    shm_header_t* header = (shm_header_t*)addr;
    header->magic = SHM_MAGIC; header->version = SHM_VERSION;
    header->data_size = data_size;
    // 使用原子操作初始化计数器
    __atomic_store_n(&header->ref_count, 1, __ATOMIC_SEQ_CST);
    __atomic_store_n(&header->connected_count, 1, __ATOMIC_SEQ_CST);
    header->interest_mask = 0xFFFFFFFF;
    header->pending_notify = 0;
    __atomic_store_n(&header->notify_counter, 0, __ATOMIC_SEQ_CST);
    __atomic_store_n(&header->client_count, 0, __ATOMIC_SEQ_CST);
    __atomic_store_n(&header->next_client_id, 1, __ATOMIC_SEQ_CST);

    // 初始化客户端槽位
    for (int i = 0; i < SHM_MAX_CLIENTS; i++) {
        header->clients[i].client_id = 0;
        header->clients[i].last_seen_seq = 0;
    }

    int notify_fd = eventfd(0, EFD_NONBLOCK);
    if (notify_fd < 0) { munmap(addr, total_size); close(fd); shm_unlink(name); return SHM_ERR_SYSTEM; }
    header->notify_fd = notify_fd;

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
    pthread_mutex_init(&header->lock.mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    header->lock.owner = getpid(); header->lock.state = SHM_LOCK_FREE;

    shm_handle_t handle = malloc(sizeof(struct shm_handle_impl));
    if (!handle) { close(notify_fd); munmap(addr, total_size); close(fd); shm_unlink(name); return SHM_ERR_NO_MEMORY; }

    strncpy(handle->name, name, SHM_NAME_MAX - 1); handle->name[SHM_NAME_MAX - 1] = '\0';
    handle->fd = fd; handle->addr = addr; handle->mapped_size = total_size;
    handle->is_server = 1; handle->local_notify_fd = notify_fd;
    handle->client_slot = -1;  // 服务端无槽位
    handle->client_id = 0;
    handle->last_seen_counter = 0;  // Server 初始计数
    handle->owner_thread = pthread_self();
    *out_handle = handle;
    return SHM_OK;
}

shm_error_t shm_join(const char* name, shm_permission_t perm, shm_handle_t* out_handle) {
    (void)perm;
    if (validate_name(name) != 0) return SHM_ERR_NAME_INVALID;
    if (!out_handle) return SHM_ERR_INVALID_PARAM;

    int fd = shm_open(name, O_RDWR, 0664);
    if (fd < 0) return (errno == ENOENT) ? SHM_ERR_NOT_FOUND : SHM_ERR_SYSTEM;

    struct stat st;
    if (fstat(fd, &st) < 0) { close(fd); return SHM_ERR_SYSTEM; }

    void* addr = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) { close(fd); return SHM_ERR_MAP_FAILED; }

    shm_header_t* header = (shm_header_t*)addr;
    if (header->magic != SHM_MAGIC) { munmap(addr, st.st_size); close(fd); return SHM_ERR_NOT_FOUND; }

    // 注册客户端到服务端
    uint32_t client_id = __atomic_fetch_add(&header->next_client_id, 1, __ATOMIC_SEQ_CST);
    int slot_found = -1;

    // 寻找空槽位（client_id 为 0 表示空槽）
    for (int i = 0; i < SHM_MAX_CLIENTS; i++) {
        uint32_t expected = 0;
        if (__atomic_compare_exchange_n(&header->clients[i].client_id, &expected, client_id,
                                         0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
            slot_found = i;
            break;
        }
    }

    if (slot_found < 0) {
        // 没有空槽位
        munmap(addr, st.st_size);
        close(fd);
        return SHM_ERR_NO_MEMORY;
    }

    __atomic_store_n(&header->clients[slot_found].last_seen_seq, 0, __ATOMIC_SEQ_CST);
    __atomic_add_fetch(&header->client_count, 1, __ATOMIC_SEQ_CST);

    __sync_add_and_fetch(&header->ref_count, 1);
    __sync_add_and_fetch(&header->connected_count, 1);

    shm_handle_t handle = malloc(sizeof(struct shm_handle_impl));
    if (!handle) {
        __atomic_store_n(&header->clients[slot_found].client_id, 0, __ATOMIC_SEQ_CST);
        __atomic_sub_fetch(&header->client_count, 1, __ATOMIC_SEQ_CST);
        __sync_sub_and_fetch(&header->ref_count, 1);
        __sync_sub_and_fetch(&header->connected_count, 1);
        munmap(addr, st.st_size);
        close(fd);
        return SHM_ERR_NO_MEMORY;
    }

    strncpy(handle->name, name, SHM_NAME_MAX - 1);
    handle->name[SHM_NAME_MAX - 1] = '\0';
    handle->fd = fd;
    handle->addr = addr;
    handle->mapped_size = st.st_size;
    handle->is_server = 0;
    // 客户端使用服务端的主 eventfd 进行 poll
    handle->local_notify_fd = header->notify_fd;
    handle->client_slot = slot_found;
    handle->client_id = client_id;
    handle->last_seen_counter = __atomic_load_n(&header->notify_counter, __ATOMIC_SEQ_CST);
    handle->owner_thread = pthread_self();
    *out_handle = handle;
    return SHM_OK;
}

shm_error_t shm_unmap(shm_handle_t handle) {
    if (!handle) return SHM_ERR_INVALID_PARAM;
    if (munmap(handle->addr, handle->mapped_size) < 0) return SHM_ERR_UNMAP_FAILED;
    handle->addr = NULL;
    return SHM_OK;
}

shm_error_t shm_close(shm_handle_t* handle) {
    if (!handle || !*handle) return SHM_ERR_INVALID_PARAM;
    shm_handle_t impl = *handle;
    shm_header_t* header = (shm_header_t*)impl->addr;

    if (header && header->magic == SHM_MAGIC) {
        __sync_sub_and_fetch(&header->ref_count, 1);
        __sync_sub_and_fetch(&header->connected_count, 1);

        // 客户端注销：释放槽位
        if (!impl->is_server && impl->client_slot >= 0 && impl->client_slot < SHM_MAX_CLIENTS) {
            __atomic_store_n(&header->clients[impl->client_slot].client_id, 0, __ATOMIC_SEQ_CST);
            __atomic_sub_fetch(&header->client_count, 1, __ATOMIC_SEQ_CST);
        }
    }
    // 注意：客户端不再关闭 local_notify_fd，因为它指向服务端的 eventfd
    // 只有服务端才关闭主 eventfd
    if (impl->is_server && impl->local_notify_fd >= 0) {
        close(impl->local_notify_fd);
    }
    close(impl->fd);
    if (impl->addr) munmap(impl->addr, impl->mapped_size);
    free(impl);
    *handle = NULL;
    return SHM_OK;
}

shm_error_t shm_destroy(const char* name) {
    if (validate_name(name) != 0) return SHM_ERR_NAME_INVALID;
    if (shm_unlink(name) < 0) return (errno == ENOENT) ? SHM_ERR_NOT_FOUND : SHM_ERR_SYSTEM;
    return SHM_OK;
}

void* shm_get_data_ptr(shm_handle_t handle) {
    if (!handle) return NULL;
    return (uint8_t*)handle->addr + SHM_HEADER_SIZE;
}

const void* shm_get_data_ptr_const(const shm_handle_t handle) {
    if (!handle) return NULL;
    return (const uint8_t*)handle->addr + SHM_HEADER_SIZE;
}

size_t shm_get_data_size(shm_handle_t handle) {
    if (!handle) return 0;
    return ((shm_header_t*)handle->addr)->data_size;
}

shm_error_t shm_notify(shm_handle_t handle) {
    if (!handle) return SHM_ERR_INVALID_PARAM;
    shm_header_t* header = (shm_header_t*)handle->addr;

    // 原子递增通知计数器
    __sync_add_and_fetch(&header->notify_counter, 1);

    // 写入主 eventfd，广播通知所有客户端
    // 客户端通过 poll 主 eventfd 并检查 notify_counter 来接收通知
    eventfd_write(header->notify_fd, 1);
    return SHM_OK;
}

shm_error_t shm_wait(shm_handle_t handle, int timeout_ms) {
    if (!handle) return SHM_ERR_INVALID_PARAM;
    shm_header_t* header = (shm_header_t*)handle->addr;

    // 首先检查是否有待处理的通知（通过计数器比较）
    uint64_t current_counter = __atomic_load_n(&header->notify_counter, __ATOMIC_SEQ_CST);
    if (current_counter > handle->last_seen_counter) {
        return SHM_OK;  // 有新通知
    }

    // 没有待处理通知，等待 eventfd
    struct pollfd pfd = { .fd = handle->local_notify_fd, .events = POLLIN };
    int ret = poll(&pfd, 1, timeout_ms);
    if (ret < 0) return SHM_ERR_SYSTEM;
    return (ret == 0) ? SHM_ERR_TIMEOUT : SHM_OK;
}

int shm_get_notify_fd(shm_handle_t handle) {
    return handle ? handle->local_notify_fd : -1;
}

shm_error_t shm_consume_notify(shm_handle_t handle) {
    if (!handle) return SHM_ERR_INVALID_PARAM;
    shm_header_t* header = (shm_header_t*)handle->addr;

    // 更新本地看到的通知计数器（不消费 eventfd）
    uint64_t current_counter = __atomic_load_n(&header->notify_counter, __ATOMIC_SEQ_CST);
    handle->last_seen_counter = current_counter;

    // 尝试消费 eventfd 以防止计数器无限增长
    // 使用非阻塞读取，如果失败（EAGAIN）说明计数器已经是 0，可以忽略
    eventfd_t val;
    eventfd_read(handle->local_notify_fd, &val);
    // 忽略读取结果，因为我们使用 notify_counter 作为真实的通知判断依据

    return SHM_OK;
}

shm_error_t shm_get_connection_count(shm_handle_t handle, uint32_t* count) {
    if (!handle || !count) return SHM_ERR_INVALID_PARAM;
    *count = ((shm_header_t*)handle->addr)->connected_count;
    return SHM_OK;
}

bool shm_exists(const char* name) {
    if (validate_name(name) != 0) return false;
    int fd = shm_open(name, O_RDONLY, 0664);
    if (fd >= 0) { close(fd); return true; }
    return false;
}

shm_error_t shm_lock(shm_handle_t handle, int timeout_ms) {
    if (!handle) return SHM_ERR_INVALID_PARAM;
    shm_header_t* header = (shm_header_t*)handle->addr;
    int ret;
    if (timeout_ms < 0) {
        ret = pthread_mutex_lock(&header->lock.mutex);
    } else {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
        ret = pthread_mutex_timedlock(&header->lock.mutex, &ts);
    }
    if (ret == EOWNERDEAD) { pthread_mutex_consistent(&header->lock.mutex); header->lock.owner = getpid(); header->lock.state = SHM_LOCK_BUSY; return SHM_ERR_LOCK_RECOVERED; }
    if (ret == ETIMEDOUT) return SHM_ERR_LOCK_TIMEOUT;
    if (ret != 0) return SHM_ERR_SYSTEM;
    header->lock.owner = getpid(); header->lock.state = SHM_LOCK_BUSY;
    return SHM_OK;
}

shm_error_t shm_lock_try(shm_handle_t handle) {
    if (!handle) return SHM_ERR_INVALID_PARAM;
    shm_header_t* header = (shm_header_t*)handle->addr;
    int ret = pthread_mutex_trylock(&header->lock.mutex);
    if (ret == EOWNERDEAD) { pthread_mutex_consistent(&header->lock.mutex); header->lock.owner = getpid(); header->lock.state = SHM_LOCK_BUSY; return SHM_ERR_LOCK_RECOVERED; }
    if (ret == EBUSY) return SHM_ERR_LOCK_TIMEOUT;
    if (ret != 0) return SHM_ERR_SYSTEM;
    header->lock.owner = getpid(); header->lock.state = SHM_LOCK_BUSY;
    return SHM_OK;
}

shm_error_t shm_unlock(shm_handle_t handle) {
    if (!handle) return SHM_ERR_INVALID_PARAM;
    shm_header_t* header = (shm_header_t*)handle->addr;
    header->lock.state = SHM_LOCK_FREE; header->lock.owner = 0;
    return (pthread_mutex_unlock(&header->lock.mutex) != 0) ? SHM_ERR_SYSTEM : SHM_OK;
}

bool shm_is_locked(shm_handle_t handle) {
    if (!handle) return false;
    return ((shm_header_t*)handle->addr)->lock.state == SHM_LOCK_BUSY;
}

// ========== Server/Client 别名实现 ==========

shm_error_t shm_server_create(const char* name, size_t data_size,
                              shm_permission_t perm, shm_flags_t flags,
                              shm_handle_t* out_handle) {
    return shm_create(name, data_size, perm, flags, out_handle);
}

shm_error_t shm_client_connect(const char* name, shm_permission_t perm,
                               shm_handle_t* out_handle) {
    return shm_join(name, perm, out_handle);
}

shm_error_t shm_disconnect(shm_handle_t* handle) {
    return shm_close(handle);
}

bool shm_is_server(shm_handle_t handle) {
    if (!handle) return false;
    return handle->is_server != 0;
}
