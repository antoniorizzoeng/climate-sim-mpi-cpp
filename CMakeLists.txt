cmake_minimum_required(VERSION 3.16)
project(climate-sim-mpi-cpp VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(MPI REQUIRED)
find_package(yaml-cpp REQUIRED)

include(CTest)

option(ENABLE_COVERAGE "Enable coverage reporting" OFF)

if(ENABLE_COVERAGE)
    message(STATUS "Building with coverage flags")
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

option(ENABLE_KOKKOS "Build the optional Kokkos step backend" OFF)
option(KOKKOS_GPU_AWARE_MPI "Pass packed Kokkos device buffers directly to MPI" OFF)
if(ENABLE_KOKKOS)
    include(FetchContent)
    FetchContent_Declare(kokkos
        URL https://github.com/kokkos/kokkos/releases/download/4.7.04/kokkos-4.7.04.tar.gz
        URL_HASH SHA256=4213b248c39e112299fa94ee08817e51126fc02996ed6e2ab56aec4cdb80ee1f
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(kokkos)
endif()

add_subdirectory(src)

set(INSTALL_GTEST OFF CACHE BOOL "Disable installation of googletest" FORCE)
set(INSTALL_GMOCK OFF CACHE BOOL "Disable installation of googlemock" FORCE)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_SKIP_INSTALL_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

if (BUILD_TESTING)
  add_subdirectory(tests)
endif()

# -----------------------
# Install target + export
# -----------------------
install(TARGETS climate_sim
  EXPORT climate-sim-mpi-cppTargets
  RUNTIME DESTINATION bin
)

install(EXPORT climate-sim-mpi-cppTargets
  FILE climate-sim-mpi-cppTargets.cmake
  NAMESPACE ClimateSim::
  DESTINATION lib/cmake/climate-sim-mpi-cpp
)

# -----------------------
# Package config + version
# -----------------------
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/climate-sim-mpi-cppConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/climate-sim-mpi-cppConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/climate-sim-mpi-cppConfig.cmake"
  INSTALL_DESTINATION lib/cmake/climate-sim-mpi-cpp
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/climate-sim-mpi-cppConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/climate-sim-mpi-cppConfigVersion.cmake"
  DESTINATION lib/cmake/climate-sim-mpi-cpp
)

# Explicit benchmark target. The regular simulator always writes snapshots.
add_executable(step_loop_bench EXCLUDE_FROM_ALL benchmarks/step_loop.cpp)
target_link_libraries(step_loop_bench PRIVATE core)
