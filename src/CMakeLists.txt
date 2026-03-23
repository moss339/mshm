add_library(mshm STATIC
    shm_common.c
    shm_linux.c
)
target_link_libraries(mshm rt pthread)
