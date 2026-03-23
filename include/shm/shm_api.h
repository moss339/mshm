#ifndef SHM_API_H
#define SHM_API_H

#include "shm_adapter.h"
#include "shm_types.h"
#include "shm_errors.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ========== 生命周期管理 ==========
// Server: 创建共享内存服务
SHM_API shm_error_t shm_create(const char* name, size_t data_size,
                               shm_permission_t perm, shm_flags_t flags,
                               shm_handle_t* out_handle);
// Server: 创建共享内存服务 (shm_create 的别名，语义更明确)
SHM_API shm_error_t shm_server_create(const char* name, size_t data_size,
                                       shm_permission_t perm, shm_flags_t flags,
                                       shm_handle_t* out_handle);

// Client: 连接共享内存服务
SHM_API shm_error_t shm_join(const char* name, shm_permission_t perm,
                             shm_handle_t* out_handle);
// Client: 连接共享内存服务 (shm_join 的别名，语义更明确)
SHM_API shm_error_t shm_client_connect(const char* name, shm_permission_t perm,
                                       shm_handle_t* out_handle);

// 断开映射
SHM_API shm_error_t shm_unmap(shm_handle_t handle);

// 关闭连接 (Server/Client 都可以调用)
SHM_API shm_error_t shm_close(shm_handle_t* handle);
// 关闭连接 (shm_close 的别名)
SHM_API shm_error_t shm_disconnect(shm_handle_t* handle);

// 销毁共享内存 (仅 Server 有意义)
SHM_API shm_error_t shm_destroy(const char* name);

// ========== 数据访问 ==========
SHM_API void* shm_get_data_ptr(shm_handle_t handle);
SHM_API const void* shm_get_data_ptr_const(const shm_handle_t handle);
SHM_API size_t shm_get_data_size(shm_handle_t handle);

// ========== 通知机制 ==========
SHM_API shm_error_t shm_notify(shm_handle_t handle);
SHM_API shm_error_t shm_wait(shm_handle_t handle, int timeout_ms);
SHM_API int shm_get_notify_fd(shm_handle_t handle);
SHM_API shm_error_t shm_consume_notify(shm_handle_t handle);

// ========== 连接管理 ==========
SHM_API shm_error_t shm_get_connection_count(shm_handle_t handle, uint32_t* count);
SHM_API bool shm_exists(const char* name);

// ========== 角色查询 ==========
// 判断是否为 Server (true: Server, false: Client)
SHM_API bool shm_is_server(shm_handle_t handle);
// 注意: is_producer 已废弃，请使用 shm_is_server()

// ========== 进程间锁 ==========
SHM_API shm_error_t shm_lock(shm_handle_t handle, int timeout_ms);
SHM_API shm_error_t shm_lock_try(shm_handle_t handle);
SHM_API shm_error_t shm_unlock(shm_handle_t handle);
SHM_API bool shm_is_locked(shm_handle_t handle);

// ========== 工具函数 ==========
// shm_strerror 在 shm_errors.h 中声明
SHM_API const char* shm_version(void);

#ifdef __cplusplus
}
#endif

#endif // SHM_API_H
