cmake_minimum_required(VERSION 3.14)

option(BUILD_SHARED_LIBS "Build shared libraries" ON)

set(HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/shm/shm_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/shm/shm_types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/shm/shm_errors.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/shm/shm_adapter.h
)

set(SOURCES
    shm_common.c
    shm_linux.c
)

if(BUILD_SHARED_LIBS)
    add_library(mshm SHARED ${SOURCES} ${HEADERS})
    set_target_properties(mshm PROPERTIES
        VERSION 1.0.0
        SOVERSION 1
        PUBLIC_HEADER "${HEADERS}"
        POSITION_INDEPENDENT_CODE ON
    )
else()
    add_library(mshm STATIC ${SOURCES} ${HEADERS})
    set_target_properties(mshm PROPERTIES
        VERSION 1.0.0
        SOVERSION 1
        PUBLIC_HEADER "${HEADERS}"
    )
endif()

target_include_directories(mshm PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(mshm PUBLIC
    rt
    pthread
)

install(TARGETS mshm
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)
