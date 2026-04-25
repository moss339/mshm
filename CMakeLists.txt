cmake_minimum_required(VERSION 3.14)

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(MSHM_BUILD_TESTS "Build tests" ON)

project(mshm VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

enable_testing()

add_subdirectory(src)

if(MSHM_BUILD_TESTS)
    add_subdirectory(test)
endif()

# Export mshm target
export(TARGETS mshm NAMESPACE mshm:: FILE mshmTargets.cmake)

install(TARGETS mshm
    EXPORT mshmTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/shm
)

install(EXPORT mshmTargets
    NAMESPACE mshm::
    FILE mshmTargets.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mshm
)

install(DIRECTORY include/shm/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/shm)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/mshmConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/mshmConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mshm
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cmake/mshmConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mshm
)
