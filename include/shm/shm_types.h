#ifndef SHM_TYPES_H
#define SHM_TYPES_H

#include <stdint.h>
#include <pthread.h>

// ========== 常量定义 ==========
#define SHM_MAGIC           0x53484D5F    // 'SHM_'
#define SHM_VERSION         0x00000001    // 当前版本
#define SHM_NAME_MAX        64            // 最大名称长度
#define SHM_NAME_PREFIX     "/"           // POSIX共享内存路径前缀
#define SHM_HEADER_SIZE     256           // 控制块总大小
// reserved 大小由编译器自动计算以填满 256 字节

// ========== 权限与标志 ==========
typedef enum {
    SHM_PERM_NONE   = 0,
    SHM_PERM_READ   = 1 << 0,
    SHM_PERM_WRITE  = 1 << 1,
} shm_permission_t;

typedef enum {
    SHM_FLAG_NONE      = 0,
    SHM_FLAG_CREATE    = 1 << 0,
    SHM_FLAG_EXCL      = 1 << 1,
    SHM_FLAG_READONLY  = 1 << 2,
} shm_flags_t;

// ========== 锁状态 ==========
typedef enum {
    SHM_LOCK_FREE    = 0,
    SHM_LOCK_BUSY    = 1,
    SHM_LOCK_RECOVER = 2,
} shm_lock_state_t;

// ========== 事件类型 ==========
typedef enum {
    SHM_EVENT_NONE        = 0,
    SHM_EVENT_DATA_WRITE  = 1 << 0,
    SHM_EVENT_CONNECTED   = 1 << 1,
    SHM_EVENT_DISCONNECT  = 1 << 2,
    SHM_EVENT_DESTROY     = 1 << 3,
} shm_event_type_t;

// ========== 锁结构 (放在控制头中) ==========
typedef struct shm_lock {
    pthread_mutex_t  mutex;
    volatile pid_t   owner;
    volatile int32_t state;
} shm_lock_t;

// ========== 客户端通知管理 ==========
#define SHM_MAX_CLIENTS 8

typedef struct shm_client_entry {
    volatile uint32_t client_id;      // 客户端标识
    volatile uint32_t last_seen_seq;  // 上次确认序列号
} shm_client_entry_t;

// ========== 控制头 ==========
typedef struct shm_header {
    uint32_t          magic;
    uint32_t          version;
    uint32_t          data_size;
    volatile uint32_t ref_count;
    volatile uint32_t connected_count;
    int               notify_fd;          // 服务端主 eventfd，用于广播通知
    volatile uint32_t interest_mask;
    volatile uint64_t notify_counter;    // 通知计数器，用于广播语义
    volatile uint32_t pending_notify;
    volatile uint32_t client_count;      // 已注册客户端数
    volatile uint32_t next_client_id;    // 下一个客户端 ID
    shm_client_entry_t clients[SHM_MAX_CLIENTS];
    shm_lock_t        lock;
    uint8_t           reserved[SHM_HEADER_SIZE - sizeof(uint32_t) * 9 - sizeof(uint64_t) - sizeof(int) - sizeof(shm_lock_t) - sizeof(shm_client_entry_t) * SHM_MAX_CLIENTS];
} shm_header_t;

// ========== 句柄类型 (不透明指针) ==========
struct shm_handle_impl {
    char          name[SHM_NAME_MAX];
    int           fd;
    void*         addr;
    size_t        mapped_size;
    int           is_server;       // true: created the shm (server), false: joined as client
    int           local_notify_fd; // 本地 eventfd 副本（客户端 poll 用）
    int           client_slot;     // 客户端槽位索引（-1 表示服务端）
    uint32_t      client_id;       // 客户端唯一 ID
    uint64_t      last_seen_counter;  // 上次处理的通知计数器值
    pthread_t     owner_thread;
};
typedef struct shm_handle_impl* shm_handle_t;

// ========== 连接信息 ==========
typedef struct shm_connection {
    char     name[SHM_NAME_MAX];
    pid_t    pid;
    uint32_t flags;
    uint64_t connect_time;
} shm_connection_t;

// ========== 事件结构 ==========
typedef struct shm_event {
    shm_event_type_t  type;
    uint32_t          producer_pid;
    uint32_t          data_len;
    uint64_t          timestamp;
} shm_event_t;

#endif // SHM_TYPES_H
